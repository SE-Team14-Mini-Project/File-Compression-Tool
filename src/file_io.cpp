#include "interfaces.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <system_error>

namespace team14 {
namespace {
namespace fs = std::filesystem;

Result accessError(const std::error_code& error, const char* message) {
    return {error == std::errc::permission_denied ? ErrorCode::PermissionDenied
                                                : ErrorCode::IoError,
            message};
}

bool invalidPath(const fs::path& path) {
    return path.empty() || path.native().find(fs::path::value_type{}) !=
                               fs::path::string_type::npos;
}

// Use an exclusively created directory, not an existing destination or a
// guessed temporary filename. This checks actual access, including ACLs.
Result probeParent(const fs::path& parent) {
    static std::atomic<unsigned long long> sequence{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (int attempt = 0; attempt < 16; ++attempt) {
        const fs::path probe = parent / (".team14-io-check-" + std::to_string(stamp) +
                                        "-" + std::to_string(sequence++));
        std::error_code error;
        if (!fs::create_directory(probe, error)) {
            if (!error || error == std::errc::file_exists) {
                continue;
            }
            return accessError(error, "Cannot create output in the destination directory.");
        }
        // Cleanup also runs if constructing/opening the stream throws.
        struct Cleanup {
            fs::path path;
            ~Cleanup() { std::error_code ignored; fs::remove_all(path, ignored); }
        } cleanup{probe};
        bool writable = false;
        {
            std::ofstream output(probe / "check", std::ios::binary | std::ios::trunc);
            if (output.is_open()) {
                output.put('\0');
                output.close();
                writable = !output.fail();
            }
        }
        fs::remove_all(probe, error);
        if (error) {
            return accessError(error, "Could not clean up the output access check.");
        }
        if (!writable) {
            return {ErrorCode::IoError, "Cannot write a binary file in the destination directory."};
        }
        return {};
    }
    return {ErrorCode::IoError, "Could not create a unique output access check."};
}
} // namespace

Result validateInputFile(const fs::path& source) {
    if (invalidPath(source)) {
        return {ErrorCode::InvalidArguments, "Input path is empty or contains a null character."};
    }
    std::error_code error;
    const auto status = fs::status(source, error);
    if (error == std::errc::no_such_file_or_directory ||
        (!error && !fs::exists(status))) {
        return {ErrorCode::InputNotFound, "Input file does not exist."};
    }
    if (error) {
        return accessError(error, "Cannot access input file.");
    }
    if (!fs::is_regular_file(status)) {
        return {ErrorCode::InvalidArguments, "Input must be a regular file."};
    }
    std::ifstream input(source, std::ios::binary);
    if (!input.is_open()) {
        // Permissions are a useful diagnostic, not a replacement for opening.
        constexpr auto readable = fs::perms::owner_read | fs::perms::group_read |
                                  fs::perms::others_read;
        const bool noReadBits = status.permissions() != fs::perms::unknown &&
                               (status.permissions() & readable) == fs::perms::none;
        return {noReadBits ? ErrorCode::PermissionDenied : ErrorCode::IoError,
                "Could not open input file for binary reading."};
    }
    // Empty files are valid. A failed read for any reason other than EOF is not.
    input.peek();
    if (input.bad() || (input.fail() && !input.eof())) {
        return {ErrorCode::IoError, "Could not read input file."};
    }
    return {};
}

Result validateOutputFile(const fs::path& destination, bool overwrite) {
    if (invalidPath(destination)) {
        return {ErrorCode::InvalidArguments, "Output path is empty or contains a null character."};
    }
    std::error_code error;
    const auto status = fs::symlink_status(destination, error);
    if (error && error != std::errc::no_such_file_or_directory) {
        return accessError(error, "Cannot access output path.");
    }
    // A dangling link counts as an existing entry. Never follow output links.
    if (fs::exists(status)) {
        if (!overwrite) {
            return {ErrorCode::OutputExists, "Output already exists; overwrite was not permitted."};
        }
        if (fs::is_symlink(status) || !fs::is_regular_file(status)) {
            return {ErrorCode::UnsafeOutputPath, "Output must not be a symlink or a non-regular file."};
        }
        constexpr auto writable = fs::perms::owner_write | fs::perms::group_write |
                                  fs::perms::others_write;
        if (status.permissions() != fs::perms::unknown &&
            (status.permissions() & writable) == fs::perms::none) {
            return {ErrorCode::PermissionDenied, "Existing output has no write permission."};
        }
    }
    if (destination.filename().empty()) {
        return {ErrorCode::InvalidArguments, "Output must include a filename."};
    }
    const auto parent = destination.has_parent_path() ? destination.parent_path() : fs::path(".");
    error.clear();
    const auto parentStatus = fs::status(parent, error);
    if (error) {
        return accessError(error, "Cannot access output directory.");
    }
    if (!fs::is_directory(parentStatus)) {
        return {ErrorCode::IoError, "Output directory does not exist or is not a directory."};
    }
    // This is a preflight check, not a reservation or an atomic publish.
    // Controller must still handle open/write/close errors and prevent races.
    return probeParent(parent);
}
} // namespace team14
