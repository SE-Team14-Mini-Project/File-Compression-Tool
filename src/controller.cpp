#include "interfaces.h"

#include <filesystem>
#include <fstream>

namespace team14 {

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
