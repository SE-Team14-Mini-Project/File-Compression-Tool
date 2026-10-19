#include "interfaces.h"

#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
namespace team14 {
Result compressFile(const std::filesystem::path& input,
                    const std::filesystem::path& archive,
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

    source.clear();
    source.seekg(0, std::ios::beg);
    if (!source) {
        return {ErrorCode::IoError, "Could not rewind input file."};
    }

    HuffmanTree tree = buildTree(frequencies);
    CodeTable codes = generateCodes(tree);

    // The payload must be produced before the archive header is written.
    // A temporary payload file will be added in the next implementation step.
    (void)codes;
    return {ErrorCode::IoError,
            "Compression orchestration is incomplete: temporary payload handling is not implemented yet."};
}
Result getArchiveInfo(const std::filesystem::path& archive,
                      ArchiveMetadata& metadata) {
    // Reset metadata so a failed operation does not leave stale values.
    metadata = ArchiveMetadata{};

    // Check that the archive exists and is a valid input file.
    Result result = validateInputFile(archive);
    if (!result.ok()) {
        return result;
    }

    // Open the archive in binary mode.
    std::ifstream input(archive, std::ios::binary);
    if (!input.is_open()) {
        return {ErrorCode::IoError, "Could not open archive for reading."};
    }

    // Read and parse the archive header.
    result = readArchiveHeader(input, metadata);
    if (!result.ok()) {
        return result;
    }

    // Validate the metadata and enforce the configured output-size limit.
    result = validateArchiveMetadata(metadata, kDefaultMaxOutputBytes);
    if (!result.ok()) {
        return result;
    }

    return {};
}

} // namespace team14
