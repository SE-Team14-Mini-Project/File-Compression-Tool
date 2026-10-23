
#include "interfaces.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace team14;

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (condition) {
        std::cout << "[PASS] " << message << '\n';
    } else {
        std::cerr << "[FAIL] " << message << '\n';
        ++failures;
    }
}

bool writeBytes(const fs::path& path,
                const std::vector<unsigned char>& bytes) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        return false;
    }

    if (!bytes.empty()) {
        output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    }

    output.close();
    return !output.fail();
}

std::vector<unsigned char> readBytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);

    return std::vector<unsigned char>(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

void testRoundTrip(const fs::path& directory,
                   const std::string& name,
                   const std::vector<unsigned char>& original) {
    const fs::path input = directory / (name + ".input");
    const fs::path archive = directory / (name + ".huf");
    const fs::path restored = directory / (name + ".restored");

    check(writeBytes(input, original), name + ": create input");

    if (!fs::exists(input)) {
        return;
    }

    OperationStats compressionStats{};
    Result result = compressFile(input, archive, compressionStats);

    check(result.ok(), name + ": compression succeeds");

    if (!result.ok()) {
        std::cerr << "       " << result.message << '\n';
        return;
    }

    check(fs::exists(archive), name + ": archive is created");

    OperationStats decompressionStats{};
    result = decompressFile(archive, restored, decompressionStats);

    check(result.ok(), name + ": decompression succeeds");

    if (!result.ok()) {
        std::cerr << "       " << result.message << '\n';
        return;
    }

    const auto actual = readBytes(restored);

    check(actual == original, name + ": restored bytes match original");
    check(decompressionStats.originalBytes == original.size(),
          name + ": restored byte count is correct");
}


void testInvalidArchives(const fs::path& directory) {
    const fs::path input = directory / "negative.input";
    const fs::path archive = directory / "negative.huf";
    const fs::path restored = directory / "negative.restored";

    // Create a valid archive first.
    const std::vector<unsigned char> original = {
        'A', 'B', 'A', 'B', 'A', 'C', 'A', 'B'
    };

    check(writeBytes(input, original), "negative tests: create input");

    OperationStats compressionStats{};
    Result result = compressFile(input, archive, compressionStats);
    check(result.ok(), "negative tests: create valid archive");

    if (!result.ok()) {
        std::cerr << "       " << result.message << '\n';
        return;
    }

    const auto validBytes = readBytes(archive);

    // Case 1: Truncate the archive.
    if (!validBytes.empty()) {
        auto truncated = validBytes;
        truncated.pop_back();

        const fs::path badArchive = directory / "truncated.huf";
        const fs::path badOutput = directory / "truncated.restored";

        check(writeBytes(badArchive, truncated),
              "truncated archive: write malformed input");

        OperationStats stats{};
        result = decompressFile(badArchive, badOutput, stats);

        check(!result.ok(), "truncated archive: decompression rejected");
        check(!fs::exists(badOutput),
              "truncated archive: no output published");
    }

    // Case 2: Append unexpected trailing data.
    {
        auto trailing = validBytes;
        trailing.push_back('X');

        const fs::path badArchive = directory / "trailing.huf";
        const fs::path badOutput = directory / "trailing.restored";

        check(writeBytes(badArchive, trailing),
              "trailing data: write malformed input");

        OperationStats stats{};
        result = decompressFile(badArchive, badOutput, stats);

        check(!result.ok(), "trailing data: decompression rejected");
        check(!fs::exists(badOutput),
              "trailing data: no output published");
    }

    // Case 3: Corrupt the last payload byte's padding bits.
    // This case applies only when the archive has unused bits in its last byte.
    {
        ArchiveMetadata metadata{};
        std::ifstream archiveInput(archive, std::ios::binary);

        result = readArchiveHeader(archiveInput, metadata);

        if (result.ok() && metadata.encodedBits % 8 != 0) {
            const std::size_t payloadOffset =
                validBytes.size() -
                static_cast<std::size_t>((metadata.encodedBits + 7) / 8);

            auto invalidPadding = validBytes;
            const unsigned unusedBits =
                8U - static_cast<unsigned>(metadata.encodedBits % 8);

            invalidPadding.back() = static_cast<unsigned char>(
                static_cast<unsigned char>(invalidPadding.back()) |
                (1U << (unusedBits - 1)));

            const fs::path badArchive = directory / "padding.huf";
            const fs::path badOutput = directory / "padding.restored";

            check(payloadOffset < invalidPadding.size(),
                  "invalid padding: payload offset is valid");

            check(writeBytes(badArchive, invalidPadding),
                  "invalid padding: write malformed input");

            OperationStats stats{};
            result = decompressFile(badArchive, badOutput, stats);

            check(!result.ok(), "invalid padding: decompression rejected");
            check(!fs::exists(badOutput),
                  "invalid padding: no output published");
        } else {
            std::cout
                << "[INFO] Padding test skipped: fixture has no unused bits.\n";
        }
    }
}


} // namespace

int main() {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();

    const fs::path directory =
        fs::temp_directory_path() /
        ("team14-round-trip-" + std::to_string(stamp));

    std::error_code error;
    if (!fs::create_directory(directory, error)) {
        std::cerr << "[FAIL] Cannot create test directory: "
                  << error.message() << '\n';
        return 1;
    }

    testRoundTrip(directory, "empty", {});

    testRoundTrip(directory, "one-byte", {'A'});

    const std::string text =
        "Huffman compression preserves the original text.\n"
        "Round-trip restoration must be lossless.\n";

    testRoundTrip(
        directory,
        "text",
        std::vector<unsigned char>(text.begin(), text.end()));

    std::vector<unsigned char> binary;
    for (unsigned int value = 0; value < 256; ++value) {
        binary.push_back(static_cast<unsigned char>(value));
    }

    // Add repeated bytes and zero bytes to exercise varied frequencies.
    binary.insert(binary.end(), {0, 0, 255, 128, 128, 1, 1, 1});

    testRoundTrip(directory, "binary", binary);

    testInvalidArchives(directory);

    fs::remove_all(directory, error);
    if (error) {
        std::cerr << "[WARN] Could not clean up test directory: "
                  << error.message() << '\n';
    }

    if (failures != 0) {
        std::cerr << failures << " round-trip check(s) failed.\n";
        return 1;
    }

    std::cout << "All round-trip checks passed.\n";
    return 0;
}
