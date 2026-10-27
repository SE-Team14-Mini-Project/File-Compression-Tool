
#include "interfaces.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace team14;

namespace {

unsigned checks = 0;

void expect(bool ok, const char* name) {
    ++checks;

    if (!ok) {
        throw std::runtime_error(name);
    }
}

// Documented AAAAABBBCC vector from ARCHIVE_FORMAT.md section 5.
std::string validArchive() {
    ArchiveMetadata metadata;

    metadata.originalBytes = 10;
    metadata.encodedBits = 15;
    metadata.frequencies['A'] = 5;
    metadata.frequencies['B'] = 3;
    metadata.frequencies['C'] = 2;

    std::istringstream payload(
        std::string("\x07\xf4", 2),
        std::ios::binary);

    std::ostringstream output(std::ios::binary);

    expect(
        writeArchive(output, metadata, payload).ok(),
        "build valid archive");

    return output.str();
}

Result parse(const std::string& bytes, ArchiveMetadata& metadata) {
    std::istringstream input(bytes, std::ios::binary);
    return readArchiveHeader(input, metadata);
}

// A rejected archive must not leave half-parsed metadata behind.
bool metadataIsReset(const ArchiveMetadata& metadata) {
    return metadata.version == kArchiveVersion &&
           metadata.flags == 0 &&
           metadata.originalBytes == 0 &&
           metadata.encodedBits == 0 &&
           metadata.crc32 == 0 &&
           std::all_of(
               metadata.frequencies.begin(),
               metadata.frequencies.end(),
               [](std::uint64_t frequency) {
                   return frequency == 0;
               });
}

} // namespace

int main() {
    try {
        const std::string good = validArchive();
        ArchiveMetadata metadata;

        // --- Baseline: valid archive. ---
        expect(parse(good, metadata).ok(),
               "valid archive accepted");

        expect(
            validateArchiveMetadata(
                metadata, kDefaultMaxOutputBytes).ok(),
            "valid metadata accepted");

        // --- Magic bytes: every byte position and near-miss values. ---
        for (std::size_t position = 0; position < 4; ++position) {
            auto bad = good;
            bad[position] =
                static_cast<char>(bad[position] ^ 0x20);

            metadata = ArchiveMetadata{};

            expect(
                parse(bad, metadata).code == ErrorCode::InvalidArchive,
                "magic byte changed");

            expect(
                metadataIsReset(metadata),
                "metadata empty after bad magic");
        }

        for (const char* magic : {
                 "HUF2", "huf1", "ZIP1", "PK\x03\x04"}) {
            auto bad = good;
            bad.replace(0, 4, magic, 4);

            expect(
                parse(bad, metadata).code == ErrorCode::InvalidArchive,
                "wrong magic string");
        }

        // Files that are not valid archives.
        expect(
            parse("hello, this is just a text file", metadata).code ==
                ErrorCode::InvalidArchive,
            "plain text file rejected");

        expect(
            parse(std::string(64, '\0'), metadata).code ==
                ErrorCode::InvalidArchive,
            "all-zero file rejected");

        expect(
            parse("", metadata).code == ErrorCode::InvalidArchive,
            "empty file rejected");

        expect(
            parse("HUF", metadata).code == ErrorCode::InvalidArchive,
            "partial magic rejected");

        // --- Format version: byte offset 4. ---
        for (unsigned version : {0U, 2U, 3U, 127U, 128U, 255U}) {
            auto bad = good;
            bad[4] = static_cast<char>(version);

            metadata = ArchiveMetadata{};

            expect(
                parse(bad, metadata).code ==
                    ErrorCode::UnsupportedVersion,
                "unsupported version rejected by parser");

            expect(
                metadataIsReset(metadata),
                "metadata empty after bad version");
        }

        // Invalid magic takes precedence over an invalid version.
        {
            auto bad = good;
            bad[0] = 'X';
            bad[4] = 9;

            expect(
                parse(bad, metadata).code == ErrorCode::InvalidArchive,
                "bad magic takes precedence over bad version");
        }

        // The parser must reject an unsupported version before reading
        // the remainder of the archive header.
        {
            std::string prefix = "HUF1";
            prefix.push_back(static_cast<char>(2));

            expect(
                parse(prefix, metadata).code ==
                    ErrorCode::UnsupportedVersion,
                "version rejected before the rest of the header");
        }

        // --- Metadata validator: supported and unsupported versions. ---
        for (unsigned version : {0U, 2U, 255U}) {
            ArchiveMetadata meta;
            meta.version = static_cast<std::uint8_t>(version);

            const Result result =
                validateArchiveMetadata(meta, kDefaultMaxOutputBytes);

            expect(
                result.code == ErrorCode::UnsupportedVersion,
                "validator rejects unsupported version");

            expect(
                !result.ok() && !result.message.empty(),
                "rejection contains an actionable message");
        }

        {
            ArchiveMetadata meta;

            expect(
                validateArchiveMetadata(
                    meta, kDefaultMaxOutputBytes).ok(),
                "supported version accepted");
        }

        // --- Malformed metadata and security bounds. ---
        {
            ArchiveMetadata meta;
            meta.flags = 0x80;

            expect(
                validateArchiveMetadata(
                    meta, kDefaultMaxOutputBytes).code ==
                    ErrorCode::InvalidArchive,
                "unknown flag bits rejected");
        }

        {
            ArchiveMetadata meta;
            meta.crc32 = 1;

            expect(
                validateArchiveMetadata(
                    meta, kDefaultMaxOutputBytes).code ==
                    ErrorCode::InvalidArchive,
                "CRC rejected when checksum flag is disabled");
        }

        {
            ArchiveMetadata meta;
            meta.originalBytes = 10;
            meta.encodedBits = 10;
            meta.frequencies['A'] = 10;

            expect(
                validateArchiveMetadata(meta, 9).code ==
                    ErrorCode::OutputLimitExceeded,
                "output exceeding configured limit rejected");
        }

        {
            ArchiveMetadata meta;
            meta.originalBytes = 11;
            meta.encodedBits = 15;
            meta.frequencies['A'] = 5;
            meta.frequencies['B'] = 3;
            meta.frequencies['C'] = 2;

            expect(
                validateArchiveMetadata(
                    meta, kDefaultMaxOutputBytes).code ==
                    ErrorCode::InvalidArchive,
                "frequency total mismatch rejected");
        }

        {
            ArchiveMetadata meta;
            meta.frequencies[0] =
                std::numeric_limits<std::uint64_t>::max();
            meta.frequencies[1] = 1;
            meta.originalBytes =
                std::numeric_limits<std::uint64_t>::max();

            expect(
                validateArchiveMetadata(
                    meta,
                    std::numeric_limits<std::uint64_t>::max()).code ==
                    ErrorCode::InvalidArchive,
                "frequency sum overflow rejected");
        }

        {
            ArchiveMetadata meta;
            meta.encodedBits = 1;

            expect(
                validateArchiveMetadata(
                    meta, kDefaultMaxOutputBytes).code ==
                    ErrorCode::InvalidArchive,
                "empty archive with nonzero bit count rejected");
        }

        {
            ArchiveMetadata meta;
            meta.originalBytes = 1;
            meta.frequencies['A'] = 1;

            expect(
                validateArchiveMetadata(
                    meta, kDefaultMaxOutputBytes).code ==
                    ErrorCode::InvalidArchive,
                "non-empty archive with zero encoded bits rejected");
        }

        {
            ArchiveMetadata meta;
            meta.originalBytes = 2;
            meta.encodedBits = 3;
            meta.frequencies['A'] = 2;

            expect(
                validateArchiveMetadata(
                    meta, kDefaultMaxOutputBytes).code ==
                    ErrorCode::InvalidArchive,
                "single-symbol archive with incorrect bit count rejected");
        }

        // --- Decoded output integrity tests. ---
        const auto testDir =
            std::filesystem::temp_directory_path() /
            "team14_validation_tests";

        std::filesystem::create_directories(testDir);

        const auto outputPath = testDir / "decoded_test.bin";

        // Correct restored file size, with checksum disabled.
        {
            {
                std::ofstream output(outputPath, std::ios::binary);
                output << "hello";
            }

            ArchiveMetadata meta;
            meta.originalBytes = 5;

            expect(
                verifyDecodedFile(outputPath, meta).ok(),
                "correct decoded file size accepted");

            meta.originalBytes = 4;

            expect(
                verifyDecodedFile(outputPath, meta).code ==
                    ErrorCode::IntegrityFailure,
                "incorrect decoded file size rejected");
        }

        // CRC32 of "hello" is 0x3610A686.
        {
            {
                std::ofstream output(outputPath, std::ios::binary);
                output << "hello";
            }

            ArchiveMetadata meta;
            meta.originalBytes = 5;
            meta.flags = kFlagCrc32Present;
            meta.crc32 = 0x3610A686U;

            expect(
                verifyDecodedFile(outputPath, meta).ok(),
                "correct decoded file checksum accepted");

            meta.crc32 ^= 1U;

            expect(
                verifyDecodedFile(outputPath, meta).code ==
                    ErrorCode::IntegrityFailure,
                "incorrect decoded file checksum rejected");
        }

        // A missing output file must fail safely.
        {
            const auto missingPath = testDir / "missing.bin";

            std::error_code error;
            std::filesystem::remove(missingPath, error);

            ArchiveMetadata meta;

            expect(
                verifyDecodedFile(missingPath, meta).code ==
                    ErrorCode::IoError,
                "missing decoded file rejected");
        }

        // --- Valid output paths. ---
        {
            const auto relativeDir = testDir / "relative";
            std::filesystem::create_directories(relativeDir);

            const auto relativePath = relativeDir / "output.txt";

            expect(
                validateOutputPath(relativePath).ok(),
                "valid output path with an existing parent accepted");

            const auto absolutePath =
                std::filesystem::absolute(
                    testDir / "absolute-output.txt");

            expect(
                validateOutputPath(absolutePath).ok(),
                "valid absolute output path accepted");
        }

        // --- Output path safety tests. ---
        {
            expect(
                validateOutputPath({}).code ==
                    ErrorCode::UnsafeOutputPath,
                "empty output path rejected");

            expect(
                validateOutputPath(".").code ==
                    ErrorCode::UnsafeOutputPath,
                "current directory rejected as output");

            expect(
                validateOutputPath("../output.txt").code ==
                    ErrorCode::UnsafeOutputPath,
                "parent-directory output path rejected");

            expect(
                validateOutputPath("folder/../output.txt").code ==
                    ErrorCode::UnsafeOutputPath,
                "embedded parent-directory component rejected");

            expect(
                validateOutputPath("folder/../../output.txt").code ==
                    ErrorCode::UnsafeOutputPath,
                "multiple parent-directory components rejected");

            expect(
                validateOutputPath("/outside.txt").code ==
                    ErrorCode::UnsafeOutputPath,
                "rooted output path without a root name rejected");
        }

        // Clean up test artifacts.
        {
            std::error_code error;
            std::filesystem::remove_all(testDir, error);
        }

        std::cout << "PASS: " << checks << " validation checks\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
