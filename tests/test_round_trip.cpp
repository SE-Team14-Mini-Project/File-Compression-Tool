
#include "interfaces.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace team14;

namespace {

int failures = 0;


std::string sha256(const std::vector<unsigned char>& input) {
    static constexpr std::array<std::uint32_t, 64> K = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
        0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
        0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
        0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
        0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    std::array<std::uint32_t, 8> H = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    const auto rotateRight = [](std::uint32_t x, unsigned n) {
        return (x >> n) | (x << (32 - n));
    };

    std::vector<unsigned char> data = input;
    const std::uint64_t bitLength =
        static_cast<std::uint64_t>(data.size()) * 8;

    data.push_back(0x80);
    while (data.size() % 64 != 56) {
        data.push_back(0);
    }

    for (int shift = 56; shift >= 0; shift -= 8) {
        data.push_back(
            static_cast<unsigned char>(bitLength >> shift));
    }

    for (std::size_t offset = 0; offset < data.size(); offset += 64) {
        std::array<std::uint32_t, 64> W{};

        for (std::size_t i = 0; i < 16; ++i) {
            const std::size_t p = offset + i * 4;
            W[i] =
                (static_cast<std::uint32_t>(data[p]) << 24) |
                (static_cast<std::uint32_t>(data[p + 1]) << 16) |
                (static_cast<std::uint32_t>(data[p + 2]) << 8) |
                static_cast<std::uint32_t>(data[p + 3]);
        }

        for (std::size_t i = 16; i < 64; ++i) {
            const std::uint32_t s0 =
                rotateRight(W[i - 15], 7) ^
                rotateRight(W[i - 15], 18) ^
                (W[i - 15] >> 3);

            const std::uint32_t s1 =
                rotateRight(W[i - 2], 17) ^
                rotateRight(W[i - 2], 19) ^
                (W[i - 2] >> 10);

            W[i] = W[i - 16] + s0 + W[i - 7] + s1;
        }

        auto a = H[0];
        auto b = H[1];
        auto c = H[2];
        auto d = H[3];
        auto e = H[4];
        auto f = H[5];
        auto g = H[6];
        auto h = H[7];

        for (std::size_t i = 0; i < 64; ++i) {
            const std::uint32_t S1 =
                rotateRight(e, 6) ^
                rotateRight(e, 11) ^
                rotateRight(e, 25);

            const std::uint32_t choice =
                (e & f) ^ (~e & g);

            const std::uint32_t temp1 =
                h + S1 + choice + K[i] + W[i];

            const std::uint32_t S0 =
                rotateRight(a, 2) ^
                rotateRight(a, 13) ^
                rotateRight(a, 22);

            const std::uint32_t majority =
                (a & b) ^ (a & c) ^ (b & c);

            const std::uint32_t temp2 = S0 + majority;

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        H[0] += a;
        H[1] += b;
        H[2] += c;
        H[3] += d;
        H[4] += e;
        H[5] += f;
        H[6] += g;
        H[7] += h;
    }

    std::ostringstream output;
    output << std::hex << std::setfill('0');

    for (const auto value : H) {
        output << std::setw(8) << value;
    }

    return output.str();
}


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

    check(actual == original,
      name + ": restored bytes match original");

    check(sha256(original) == sha256(actual),
      name + ": original and restored SHA-256 hashes match");

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

    check(
    sha256({'a', 'b', 'c'}) ==
        "ba7816bf8f01cfea414140de5dae2223"
        "b00361a396177a9cb410ff61f20015ad",
    "SHA-256 matches the standard abc test vector");
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
