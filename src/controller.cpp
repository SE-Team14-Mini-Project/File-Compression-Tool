
#include "interfaces.h"

#include <atomic>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace team14 {
namespace {

namespace fs = std::filesystem;
std::atomic<unsigned long long> tempSequence{0};

class TemporaryDirectory {
public:
    explicit TemporaryDirectory(fs::path path)
        : path_(std::move(path)) {}

    ~TemporaryDirectory() {
        std::error_code ignored;
        fs::remove_all(path_, ignored);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    const fs::path& path() const noexcept {
        return path_;
    }

private:
    fs::path path_;
};

Result createTemporaryDirectory(const fs::path& parent,
                                const std::string& prefix,
                                TemporaryDirectory*& directory,
                                std::unique_ptr<TemporaryDirectory>& owner) {
    for (int attempt = 0; attempt < 32; ++attempt) {
        const auto stamp =
            std::chrono::steady_clock::now().time_since_epoch().count();

        const fs::path candidate =
            parent / (prefix + std::to_string(stamp) + "-" +
                      std::to_string(tempSequence.fetch_add(1)));

        std::error_code error;
        if (fs::create_directory(candidate, error)) {
            owner = std::make_unique<TemporaryDirectory>(candidate);
            directory = owner.get();
            return {};
        }

        if (error != std::errc::file_exists) {
            return {ErrorCode::IoError,
                    "Could not create a temporary directory: " +
                        error.message()};
        }
    }

    return {ErrorCode::IoError,
            "Could not allocate a unique temporary directory."};
}

Result countOriginalBytes(const FrequencyTable& frequencies,
                          std::uint64_t& total) {
    total = 0;

    for (const std::uint64_t frequency : frequencies) {
        if (frequency > UINT64_MAX - total) {
            return {ErrorCode::IoError, "Input size counter overflow."};
        }
        total += frequency;
    }

    return {};
}

} // namespace

Result compressFile(const fs::path& input,
                    const fs::path& archive,
                    OperationStats& stats) {
    stats = OperationStats{};

    Result result = validateInputFile(input);
    if (!result.ok()) {
        return result;
    }

    result = validateOutputFile(archive, false);
    if (!result.ok()) {
        return result;
    }

    std::ifstream source(input, std::ios::binary);
    if (!source.is_open()) {
        return {ErrorCode::IoError, "Could not open input file."};
    }

    FrequencyTable frequencies{};
    try {
        frequencies = buildFrequencyTable(source);
    } catch (const std::exception& error) {
        return {ErrorCode::IoError, error.what()};
    }

    if (source.bad()) {
        return {ErrorCode::IoError, "Could not read the input file."};
    }

    std::uint64_t originalBytes = 0;
    result = countOriginalBytes(frequencies, originalBytes);
    if (!result.ok()) {
        return result;
    }

    source.clear();
    source.seekg(0, std::ios::beg);
    if (!source) {
        return {ErrorCode::IoError, "Could not rewind input file."};
    }

    HuffmanTree tree;
    CodeTable codes{};

    try {
        tree = buildTree(frequencies);
        codes = generateCodes(tree);
    } catch (const std::exception& error) {
        return {ErrorCode::IoError, error.what()};
    }

    const fs::path outputParent =
        archive.has_parent_path() ? archive.parent_path() : fs::path(".");

    TemporaryDirectory* payloadDirectory = nullptr;
    std::unique_ptr<TemporaryDirectory> payloadOwner;

    result = createTemporaryDirectory(
        fs::temp_directory_path(), "team14-payload-",
        payloadDirectory, payloadOwner);
    if (!result.ok()) {
        return result;
    }

    const fs::path payloadPath = payloadDirectory->path() / "payload.bin";

    EncodedStats encodedStats{};

    {
        std::ofstream payload(payloadPath, std::ios::binary);
        if (!payload.is_open()) {
            return {ErrorCode::IoError,
                    "Could not create temporary encoded payload."};
        }

        result = encode(source, codes, payload, encodedStats);
        if (!result.ok()) {
            return result;
        }

        payload.flush();
        if (!payload) {
            return {ErrorCode::IoError,
                    "Could not flush temporary encoded payload."};
        }

        payload.close();
        if (payload.fail()) {
            return {ErrorCode::IoError,
                    "Could not close temporary encoded payload."};
        }
    }

    if (encodedStats.meaningfulBits != 0 &&
        encodedStats.meaningfulBits / 8 +
                (encodedStats.meaningfulBits % 8 != 0) !=
            encodedStats.packedBytes) {
        return {ErrorCode::IoError,
                "Encoded payload byte count is inconsistent."};
    }

    ArchiveMetadata metadata{};
    metadata.version = kArchiveVersion;
    metadata.flags = 0;
    metadata.originalBytes = originalBytes;
    metadata.encodedBits = encodedStats.meaningfulBits;
    metadata.crc32 = 0;
    metadata.frequencies = frequencies;

    std::ifstream payload(payloadPath, std::ios::binary);
    if (!payload.is_open()) {
        return {ErrorCode::IoError,
                "Could not reopen temporary encoded payload."};
    }

    TemporaryDirectory* archiveDirectory = nullptr;
    std::unique_ptr<TemporaryDirectory> archiveOwner;

    result = createTemporaryDirectory(
        outputParent, ".team14-archive-",
        archiveDirectory, archiveOwner);
    if (!result.ok()) {
        return result;
    }

    const fs::path temporaryArchive =
        archiveDirectory->path() / "archive.huf";

    {
        std::ofstream output(temporaryArchive, std::ios::binary);
        if (!output.is_open()) {
            return {ErrorCode::IoError,
                    "Could not create temporary archive."};
        }

        result = writeArchive(output, metadata, payload);
        if (!result.ok()) {
            return result;
        }

        output.flush();
        if (!output) {
            return {ErrorCode::IoError,
                    "Could not flush temporary archive."};
        }

        output.close();
        if (output.fail()) {
            return {ErrorCode::IoError,
                    "Could not close temporary archive."};
        }
    }

    std::error_code error;
    const std::uint64_t archiveBytes =
        fs::file_size(temporaryArchive, error);

    if (error) {
        return {ErrorCode::IoError,
                "Could not determine temporary archive size."};
    }

    // Publish only after the complete archive has been written.
    // Recheck the destination immediately before publication.
    result = validateOutputFile(archive, false);
    if (!result.ok()) {
        return result;
    }

    fs::rename(temporaryArchive, archive, error);
    if (error) {
        return {ErrorCode::IoError,
                "Could not publish completed archive: " +
                    error.message()};
    }

    stats.originalBytes = originalBytes;
    stats.archiveBytes = archiveBytes;
    return {};
}


Result decompressFile(const fs::path& archive,
                      const fs::path& output,
                      OperationStats& stats) {
    stats = OperationStats{};

    Result result = validateInputFile(archive);
    if (!result.ok()) {
        return result;
    }

    result = validateOutputFile(output, false);
    if (!result.ok()) {
        return result;
    }

    std::error_code error;
    const std::uint64_t archiveBytes = fs::file_size(archive, error);
    if (error) {
        return {ErrorCode::IoError,
                "Could not determine archive size."};
    }

    std::ifstream input(archive, std::ios::binary);
    if (!input.is_open()) {
        return {ErrorCode::IoError,
                "Could not open archive for decompression."};
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

    if (metadata.originalBytes > kDefaultMaxOutputBytes) {
        return {ErrorCode::OutputLimitExceeded,
                "Archive exceeds the maximum permitted output size."};
    }

    // Do not accept a checksum-protected archive without verifying its CRC.
    if ((metadata.flags & kFlagCrc32Present) != 0) {
        return {ErrorCode::InvalidArchive,
                "CRC32-protected archives are not supported by this build."};
    }

    HuffmanTree tree;
    try {
        tree = buildTree(metadata.frequencies);
    } catch (const std::exception& exception) {
        return {ErrorCode::InvalidArchive,
                std::string("Could not rebuild Huffman tree: ") +
                    exception.what()};
    }

    const fs::path outputParent =
        output.has_parent_path() ? output.parent_path() : fs::path(".");

    TemporaryDirectory* outputDirectory = nullptr;
    std::unique_ptr<TemporaryDirectory> outputOwner;

    result = createTemporaryDirectory(
        outputParent, ".team14-restore-",
        outputDirectory, outputOwner);

    if (!result.ok()) {
        return result;
    }

    const fs::path temporaryOutput =
        outputDirectory->path() / "restored.bin";

    {
        std::ofstream restored(temporaryOutput, std::ios::binary);
        if (!restored.is_open()) {
            return {ErrorCode::IoError,
                    "Could not create temporary restored file."};
        }

        result = decode(input, tree, metadata.encodedBits,
                        metadata.originalBytes, restored);
        if (!result.ok()) {
            return result;
        }

        // The archive must contain exactly the declared packed payload.
        const int trailingByte = input.peek();

        if (input.bad() || (input.fail() && !input.eof())) {
            return {ErrorCode::IoError,
                    "Could not verify the end of the archive."};
        }

        if (trailingByte != std::char_traits<char>::eof()) {
            return {ErrorCode::InvalidArchive,
                    "Archive contains trailing bytes after the payload."};
        }

        restored.flush();
        if (!restored) {
            return {ErrorCode::IoError,
                    "Could not flush restored file."};
        }

        restored.close();
        if (restored.fail()) {
            return {ErrorCode::IoError,
                    "Could not close restored file."};
        }
    }

    // Recheck the destination immediately before publication.
    result = validateOutputFile(output, false);
    if (!result.ok()) {
        return result;
    }

    fs::rename(temporaryOutput, output, error);
    if (error) {
        return {ErrorCode::IoError,
                "Could not publish restored file: " + error.message()};
    }

    stats.originalBytes = metadata.originalBytes;
    stats.archiveBytes = archiveBytes;
    return {};
}


Result getArchiveInfo(const fs::path& archive,
                      ArchiveMetadata& metadata) {
    metadata = ArchiveMetadata{};

    Result result = validateInputFile(archive);
    if (!result.ok()) {
        return result;
    }

    std::ifstream input(archive, std::ios::binary);
    if (!input.is_open()) {
        return {ErrorCode::IoError, "Could not open archive for reading."};
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
