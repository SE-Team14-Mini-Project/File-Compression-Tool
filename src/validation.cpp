
#include "interfaces.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>

namespace team14 {
namespace fs = std::filesystem;

Result validateArchiveMetadata(const ArchiveMetadata& metadata,
                               std::uint64_t maxOutputBytes) {
    if (metadata.version != kArchiveVersion) {
        return {ErrorCode::UnsupportedVersion,
                "Unsupported archive version."};
    }

    if ((metadata.flags & ~kFlagCrc32Present) != 0) {
        return {ErrorCode::InvalidArchive,
                "Archive contains unsupported flags."};
    }

    if ((metadata.flags & kFlagCrc32Present) == 0 &&
        metadata.crc32 != 0) {
        return {ErrorCode::InvalidArchive,
                "CRC value must be zero when CRC is disabled."};
    }

    if (metadata.originalBytes > maxOutputBytes) {
        return {ErrorCode::OutputLimitExceeded,
                "Archive exceeds the maximum allowed output size."};
    }

    std::uint64_t total = 0;
    std::size_t symbolCount = 0;

    for (const auto frequency : metadata.frequencies) {
        if (frequency == 0) {
            continue;
        }

        ++symbolCount;

        if (frequency >
            std::numeric_limits<std::uint64_t>::max() - total) {
            return {ErrorCode::InvalidArchive,
                    "Frequency total overflows."};
        }

        total += frequency;
        if (total > maxOutputBytes) {
            return {ErrorCode::OutputLimitExceeded,
                "Archive frequency total exceeds the maximum allowed output size."};
        }
    }

    if (total != metadata.originalBytes) {
        return {ErrorCode::InvalidArchive,
                "Frequencies do not match the declared original size."};
    }

    if (symbolCount == 0) {
        if (metadata.originalBytes != 0 || metadata.encodedBits != 0) {
            return {ErrorCode::InvalidArchive,
                    "Invalid empty archive metadata."};
        }

        return {};
    }

    if (metadata.originalBytes == 0 || metadata.encodedBits == 0) {
        return {ErrorCode::InvalidArchive,
                "Non-empty archive has invalid size metadata."};
    }

    // Validate the exact encoded bit count using the Huffman tree
    // reconstructed from the archive's frequency table.
    const HuffmanTree tree = buildTree(metadata.frequencies);
    const CodeTable codes = generateCodes(tree);

    std::uint64_t expectedBits = 0;

    for (std::size_t symbol = 0;
         symbol < metadata.frequencies.size();
         ++symbol) {

        const std::uint64_t frequency = metadata.frequencies[symbol];

        if (frequency == 0) {
            continue;
        }

        const std::uint64_t codeLength =
            static_cast<std::uint64_t>(codes[symbol].size());

        if (codeLength == 0) {
            return {ErrorCode::InvalidArchive,
                    "Huffman code is missing for a stored symbol."};
        }

        // Check multiplication overflow.
        if (frequency >
            std::numeric_limits<std::uint64_t>::max() / codeLength) {
            return {ErrorCode::InvalidArchive,
                    "Expected encoded bit count overflows."};
        }

        const std::uint64_t symbolBits = frequency * codeLength;

        // Check addition overflow.
        if (symbolBits >
            std::numeric_limits<std::uint64_t>::max() - expectedBits) {
            return {ErrorCode::InvalidArchive,
                    "Expected encoded bit count overflows."};
        }

        expectedBits += symbolBits;
    }

    if (expectedBits != metadata.encodedBits) {
        return {ErrorCode::InvalidArchive,
                "Encoded bit count does not match Huffman frequencies."};
    }

    return {};
}

Result validateOutputPath(const fs::path& destination) {
    // Reject empty paths and paths without a filename.
    if (destination.empty() ||
        destination.filename().empty() ||
        destination == fs::path(".")) {
        return {ErrorCode::UnsafeOutputPath,
                "Output path must include a filename."};
    }

    // Reject parent-directory components.
    for (const auto& component : destination) {
        if (component == fs::path("..")) {
            return {ErrorCode::UnsafeOutputPath,
                    "Output path must not contain parent-directory components."};
        }
    }

    // The output parent must already exist and be a directory.
    std::error_code error;

    const fs::path parent = destination.has_parent_path()
                                ? destination.parent_path()
                                : fs::path(".");

    if (!fs::exists(parent, error) || error) {
        return {ErrorCode::UnsafeOutputPath,
                "Output parent directory does not exist."};
    }

    if (!fs::is_directory(parent, error) || error) {
        return {ErrorCode::UnsafeOutputPath,
                "Output parent path is not a directory."};
    }

    // Reject an existing directory as the output destination.
    error.clear();
    const auto status = fs::symlink_status(destination, error);

    if (!error && fs::is_directory(status)) {
        return {ErrorCode::UnsafeOutputPath,
                "Output path points to a directory."};
    }

    return {};
}

Result verifyDecodedFile(const fs::path& output,
                         const ArchiveMetadata& metadata) {
    std::ifstream input(output, std::ios::binary);

    if (!input.is_open()) {
        return {ErrorCode::IoError,
                "Could not open restored file for verification."};
    }

    std::uint64_t size = 0;
    std::uint32_t crc = 0xFFFFFFFFU;
    char ch;

    while (input.get(ch)) {
        if (size == std::numeric_limits<std::uint64_t>::max()) {
            return {ErrorCode::IntegrityFailure,
                    "Restored file size overflow."};
        }

        ++size;

        if ((metadata.flags & kFlagCrc32Present) != 0) {
            crc ^= static_cast<unsigned char>(ch);

            for (int bit = 0; bit < 8; ++bit) {
                if ((crc & 1U) != 0) {
                    crc = (crc >> 1) ^ 0xEDB88320U;
                } else {
                    crc >>= 1;
                }
            }
        }
    }

    if (input.bad()) {
        return {ErrorCode::IoError,
                "Failed while reading restored file."};
    }

    if (size != metadata.originalBytes) {
        return {ErrorCode::IntegrityFailure,
                "Restored file size does not match archive metadata."};
    }

    if ((metadata.flags & kFlagCrc32Present) != 0) {
        crc ^= 0xFFFFFFFFU;

        if (crc != metadata.crc32) {
            return {ErrorCode::IntegrityFailure,
                    "Restored file checksum does not match."};
        }
    }

    return {};
}

} // namespace team14
