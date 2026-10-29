#include "interfaces.h"
#include <fstream>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
using namespace team14;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
    std::cout << "PASS: " << message << '\n';
}

ArchiveMetadata singleSymbolMetadata(std::uint64_t size) {
    ArchiveMetadata meta{};
    meta.originalBytes = size;
    meta.encodedBits = size;
    meta.frequencies['A'] = size;
    return meta;
}

int main() {
    try {
        const std::uint64_t limit = kDefaultMaxOutputBytes;

        // 1. Exactly at the maximum allowed size.
        {
            const auto meta = singleSymbolMetadata(limit);
            expect(validateArchiveMetadata(meta, limit).ok(),
                   "Archive exactly at 1 GiB limit accepted");
        }

        // 2. One byte above the maximum.
        {
            const auto meta = singleSymbolMetadata(limit + 1);
            expect(validateArchiveMetadata(meta, limit).code ==
                       ErrorCode::OutputLimitExceeded,
                   "Archive exceeding limit by one byte rejected");
        }

        // 3. Zero-byte archive.
        {
            ArchiveMetadata meta{};
            expect(validateArchiveMetadata(meta, limit).ok(),
                   "Empty archive accepted");
        }

        // 4. Small valid archive.
        {
            const auto meta = singleSymbolMetadata(10);
            expect(validateArchiveMetadata(meta, limit).ok(),
                   "Small archive accepted");
        }

        // 5. Manipulated frequency information.
        {
            auto meta = singleSymbolMetadata(10);
            meta.frequencies['A'] = 11;
            expect(validateArchiveMetadata(meta, limit).code ==
                       ErrorCode::InvalidArchive,
                   "Inconsistent frequency metadata rejected");
        }

        // Reject frequency totals exceeding the configured limit,
        // even when the declared original size is within the limit.
        {
            auto meta = singleSymbolMetadata(10);
            meta.frequencies['A'] = 11;

            const Result result = validateArchiveMetadata(meta, 10);

            expect(result.code == ErrorCode::OutputLimitExceeded,
                "Frequency total exceeding configured limit rejected");
        }

        // 6. Decoder must not write more than declared size.
        {
            FrequencyTable frequencies{};
            frequencies['A'] = 3;
            const HuffmanTree tree = buildTree(frequencies);

            // Three zero bits represent three A symbols.
            std::string payload(1, '\0');
            std::istringstream packed(payload, std::ios::binary);
            std::ostringstream restored(std::ios::binary);

            const Result result = decode(packed, tree, 3, 2, restored);

            expect(!result.ok(),
                   "Decoder rejects excess decoded output");
            expect(restored.str().size() == 2,
                   "Decoder stops before writing third byte");
        }

        // 7. Decoder must reject incomplete output.
        {
            FrequencyTable frequencies{};
            frequencies['A'] = 2;
            const HuffmanTree tree = buildTree(frequencies);

            std::string payload(1, '\0');
            std::istringstream packed(payload, std::ios::binary);
            std::ostringstream restored(std::ios::binary);

            const Result result = decode(packed, tree, 1, 2, restored);

            expect(!result.ok(),
                   "Decoder rejects output smaller than declared size");
        }
        // 8. End-to-end oversized archive rejection.
{
    const fs::path testDir =
        fs::temp_directory_path() / "team14_output_limit_test";

    fs::create_directories(testDir);

    const fs::path archivePath = testDir / "oversized.huf";
    const fs::path outputPath = testDir / "restored.bin";

    fs::remove(archivePath);
    fs::remove(outputPath);

    const std::uint64_t oversized = kDefaultMaxOutputBytes + 1;

    {
        std::ofstream archive(archivePath, std::ios::binary);
        expect(archive.is_open(), "Test archive opened");

        archive.write("HUF1", 4);
        archive.put(static_cast<char>(kArchiveVersion));
        archive.put(0);
        archive.put(0);
        archive.put(0);

        const auto writeLE = [&](std::uint64_t value, unsigned bytes) {
            for (unsigned i = 0; i < bytes; ++i) {
                archive.put(
                    static_cast<char>((value >> (8 * i)) & 0xFFU));
            }
        };

        writeLE(oversized, 8);
        writeLE(oversized, 8);
        writeLE(0, 4);
        writeLE(1, 2);
        writeLE('A', 1);
        writeLE(oversized, 8);

        expect(archive.good(), "Oversized archive header created");
    }

    OperationStats stats{};
    const Result result =
        decompressFile(archivePath, outputPath, stats);

    expect(result.code == ErrorCode::OutputLimitExceeded,
           "Oversized archive rejected before decompression");

    expect(!fs::exists(outputPath),
           "Rejected archive did not create output file");

    fs::remove(archivePath);
    fs::remove(outputPath);
    fs::remove(testDir);
}

        std::cout << "\nAll output-size limit tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
