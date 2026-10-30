#include "interfaces.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>

namespace team14 {
namespace fs = std::filesystem;

namespace {

fs::path makeTemporaryDirectoryPath(const fs::path& parent) {
    static std::atomic<unsigned long long> sequence{0};

    const auto timestamp =
        std::chrono::steady_clock::now().time_since_epoch().count();

    return parent /
        (".team14-tmp-" + std::to_string(timestamp) + "-" +
         std::to_string(sequence++));
}

struct TemporaryDirectory {
    fs::path path;

    ~TemporaryDirectory() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
};

Result createTemporaryDirectory(const fs::path& parent,
                                fs::path& directory) {
    for (int attempt = 0; attempt < 32; ++attempt) {
        directory = makeTemporaryDirectoryPath(parent);

        std::error_code error;
        if (fs::create_directory(directory, error)) {
            return {};
        }

        if (error && error != std::errc::file_exists) {
            return {ErrorCode::IoError,
                    "Could not create a temporary directory."};
        }
    }

    return {ErrorCode::IoError,
            "Could not create a unique temporary directory."};
}

Result publishWithoutOverwrite(const fs::path& temporaryFile,
                               const fs::path& destination) {
    // A hard link publishes the already-verified file without replacing an
    // existing destination. Both paths must be on the same filesystem.
    std::error_code error;
    fs::create_hard_link(temporaryFile, destination, error);

    if (!error) {
        return {};
    }

    if (error == std::errc::file_exists) {
        return {ErrorCode::OutputExists,
                "Output already exists; it was not overwritten."};
    }

    return {ErrorCode::IoError,
            "Could not safely publish the completed output file."};
}
std::uint32_t calculateCrc32(std::istream& input) {
    std::uint32_t crc = 0xFFFFFFFFU;
    char ch;

    while (input.get(ch)) {
        crc ^= static_cast<unsigned char>(ch);

        for (int bit = 0; bit < 8; ++bit) {
            if ((crc & 1U) != 0) {
                crc = (crc >> 1) ^ 0xEDB88320U;
            } else {
                crc >>= 1;
            }
        }
    }

    if (input.bad()) {
        throw std::ios_base::failure("Failed to read input for CRC32.");
    }

    return crc ^ 0xFFFFFFFFU;
}

} // namespace

Result compressFile(const fs::path& inputPath,
                    const fs::path& archivePath,
                    OperationStats& stats) try {
    stats = OperationStats{};

    Result result = validateInputFile(inputPath);
    if (!result.ok()) {
        return result;
    }

    // Never overwrite an existing archive.
    result = validateOutputFile(archivePath, false);
    if (!result.ok()) {
        return result;
    }

    std::ifstream source(inputPath, std::ios::binary);
    if (!source.is_open()) {
        return {ErrorCode::IoError, "Could not open input file."};
    }

    FrequencyTable frequencies{};

    try {
        frequencies = buildFrequencyTable(source);
    } catch (const std::exception&) {
        return {ErrorCode::IoError, "Could not process input file while analyzing frequencies."};
    }

    if (source.bad()) {
        return {ErrorCode::IoError, "Could not read input file."};
    }

    source.clear();
    source.seekg(0, std::ios::beg);

    if (!source) {
        return {ErrorCode::IoError, "Could not rewind input file."};
    }

    std::uint32_t checksum = 0;

    try {
        checksum = calculateCrc32(source);
    } catch (const std::exception&) {
        return {ErrorCode::IoError, "Could not calculate input CRC32."};
}

    source.clear();
    source.seekg(0, std::ios::beg);

    if (!source) {
        return {ErrorCode::IoError, "Could not rewind input file after CRC32."};
}

    HuffmanTree tree;

    try {
        tree = buildTree(frequencies);
    } catch (const std::exception&) {
        return {ErrorCode::IoError, "Could not construct Huffman tree for compression."};
    }

    const CodeTable codes = generateCodes(tree);

    const fs::path parent =
        archivePath.has_parent_path()
            ? archivePath.parent_path()
            : fs::path(".");

    fs::path temporaryDirectory;
    result = createTemporaryDirectory(parent, temporaryDirectory);
    if (!result.ok()) {
        return result;
    }

    TemporaryDirectory cleanup{temporaryDirectory};

    const fs::path payloadPath = temporaryDirectory / "payload.bin";
    const fs::path archiveTempPath = temporaryDirectory / "archive.huf";

    EncodedStats encodedStats{};

    {
        std::ofstream payload(payloadPath, std::ios::binary);
        if (!payload.is_open()) {
            return {ErrorCode::IoError,
                    "Could not create temporary payload file."};
        }

        result = encode(source, codes, payload, encodedStats);
        payload.close();

        if (!result.ok()) {
            return result;
        }

        if (payload.fail()) {
            return {ErrorCode::IoError,
                    "Could not finish writing encoded payload."};
        }
    }

    ArchiveMetadata metadata{};
    metadata.version = kArchiveVersion;
    metadata.flags = kFlagCrc32Present;
    metadata.originalBytes = 0;
    metadata.encodedBits = encodedStats.meaningfulBits;
    metadata.crc32 = checksum;
    metadata.frequencies = frequencies;

    for (const auto frequency : frequencies) {
        if (frequency >
            std::numeric_limits<std::uint64_t>::max() -
                metadata.originalBytes) {
            return {ErrorCode::IoError, "Input size overflow."};
        }

        metadata.originalBytes += frequency;
    }

    {
        std::ifstream payload(payloadPath, std::ios::binary);
        std::ofstream archive(archiveTempPath, std::ios::binary);

        if (!payload.is_open() || !archive.is_open()) {
            return {ErrorCode::IoError,
                    "Could not open temporary archive files."};
        }

        result = writeArchive(archive, metadata, payload);
        archive.close();

        if (!result.ok()) {
            return result;
        }

        if (archive.fail()) {
            return {ErrorCode::IoError,
                    "Could not finish writing archive."};
        }
    }

    result = publishWithoutOverwrite(archiveTempPath, archivePath);
    if (!result.ok()) {
        return result;
    }

    stats.originalBytes = metadata.originalBytes;

    std::error_code sizeError;
    const auto archiveSize = fs::file_size(archivePath, sizeError);
    if (!sizeError) {
        stats.archiveBytes = archiveSize;
    }

    return {};
} catch (const std::exception&) {
    // Stack unwinding has already removed any scoped temporary output.
    // Never expose exception text that could contain user data.
    stats = OperationStats{};
    return {ErrorCode::IoError, "Compression failed before completion."};
}

Result decompressFile(const fs::path& archivePath,
                      const fs::path& outputPath,
                      OperationStats& stats) try {
    stats = OperationStats{};

    Result result = validateInputFile(archivePath);
    if (!result.ok()) {
        return result;
    }

    result = validateOutputPath(outputPath);
    if (!result.ok()) {
        return result;
    }

    result = validateOutputFile(outputPath, false);
    if (!result.ok()) {
        return result;
    }

    std::ifstream input(archivePath, std::ios::binary);
    if (!input.is_open()) {
        return {ErrorCode::IoError, "Could not open archive for reading."};
    }

    ArchiveMetadata metadata{};

    result = readArchiveHeader(input, metadata);
    if (!result.ok()) {
        return result;
    }

    result = validateArchiveMetadata(metadata, kDefaultMaxOutputBytes);
    if (!result.ok()) {
        return result;
    }

    HuffmanTree tree;

    try {
        tree = buildTree(metadata.frequencies);
    } catch (const std::exception&) {
        return {ErrorCode::InvalidArchive, "Could not construct Huffman tree from archive metadata."};
    }

    const fs::path parent =
        outputPath.has_parent_path()
            ? outputPath.parent_path()
            : fs::path(".");

    fs::path temporaryDirectory;
    result = createTemporaryDirectory(parent, temporaryDirectory);
    if (!result.ok()) {
        return result;
    }

    TemporaryDirectory cleanup{temporaryDirectory};

    const fs::path temporaryOutput = temporaryDirectory / "restored.bin";

    {
        std::ofstream restored(temporaryOutput, std::ios::binary);
        if (!restored.is_open()) {
            return {ErrorCode::IoError,
                    "Could not create temporary restored file."};
        }

        result = decode(input, tree, metadata.encodedBits,
                        metadata.originalBytes, restored);

        restored.close();

        if (!result.ok()) {
            return result;
        }

        if (restored.fail()) {
            return {ErrorCode::IoError,
                    "Could not finish writing restored data."};
        }
    }

    // The archive must end immediately after its declared payload.
    const int extra = input.peek();

    if (input.bad()) {
        return {ErrorCode::IoError,
                "Could not check the archive boundary."};
    }

    if (extra != std::char_traits<char>::eof()) {
        return {ErrorCode::InvalidArchive,
                "Archive contains trailing data."};
    }

    result = verifyDecodedFile(temporaryOutput, metadata);
    if (!result.ok()) {
        return result;
    }

    result = publishWithoutOverwrite(temporaryOutput, outputPath);
    if (!result.ok()) {
        return result;
    }

    stats.originalBytes = metadata.originalBytes;

    std::error_code sizeError;
    const auto archiveSize = fs::file_size(archivePath, sizeError);
    if (!sizeError) {
        stats.archiveBytes = archiveSize;
    }

    return {};
} catch (const std::exception&) {
    stats = OperationStats{};
    return {ErrorCode::IoError, "Decompression failed before completion."};
}

Result getArchiveInfo(const fs::path& archivePath,
                      ArchiveMetadata& metadata) {
    metadata = ArchiveMetadata{};

    Result result = validateInputFile(archivePath);
    if (!result.ok()) {
        return result;
    }

    std::ifstream input(archivePath, std::ios::binary);
    if (!input.is_open()) {
        return {ErrorCode::IoError,
                "Could not open archive for reading."};
    }

    result = readArchiveHeader(input, metadata);
    if (!result.ok()) {
        return result;
    }

    result = validateArchiveMetadata(metadata, kDefaultMaxOutputBytes);
    if (!result.ok()) {
        return result;
    }

    return {};
}

} // namespace team14
