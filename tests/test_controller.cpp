
#include "interfaces.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <system_error>

using namespace team14;
namespace fs = std::filesystem;

namespace {

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

} // namespace

int main() {
    const fs::path inputPath = "controller_input.txt";
    const fs::path archivePath = "controller_test.huf";
    const fs::path restoredPath = "controller_restored.txt";

    // Remove artifacts from previous test runs.
    std::error_code cleanupError;

    fs::remove(inputPath, cleanupError);
    check(!cleanupError, "Old input file can be removed");

    cleanupError.clear();
    fs::remove(archivePath, cleanupError);
    check(!cleanupError, "Old archive file can be removed");

    cleanupError.clear();
    fs::remove(restoredPath, cleanupError);
    check(!cleanupError, "Old restored file can be removed");

    const std::string original =
        "Controller module test: Huffman compression and decompression.";

    // Create the input file.
    {
        std::ofstream input(inputPath, std::ios::binary);
        check(input.is_open(), "Input file can be created");

        input.write(
            original.data(),
            static_cast<std::streamsize>(original.size()));

        check(input.good(), "Input content is written");
    }

    // Test compression.
    OperationStats compressionStats{};
    const Result compressed =
        compressFile(inputPath, archivePath, compressionStats);

    if (!compressed.ok()) {
        std::cerr << "[CONTROLLER ERROR] "
                  << compressed.message << '\n';
        std::cerr << "[ERROR CODE] "
                  << static_cast<int>(compressed.code) << '\n';
    }

    check(compressed.ok(), "Controller compression succeeds");
    check(fs::exists(archivePath), "Archive file is created");

    // Test decompression.
    OperationStats decompressionStats{};
    const Result decompressed =
        decompressFile(archivePath, restoredPath, decompressionStats);

    if (!decompressed.ok()) {
        std::cerr << "[DECOMPRESSION ERROR] "
                  << decompressed.message << '\n';
        std::cerr << "[ERROR CODE] "
                  << static_cast<int>(decompressed.code) << '\n';
    }

    check(decompressed.ok(), "Controller decompression succeeds");
    check(fs::exists(restoredPath), "Restored file is created");

    // Verify that the restored content matches the original.
    std::ifstream restored(restoredPath, std::ios::binary);
    check(restored.is_open(), "Restored file can be opened");

    const std::string actual(
        (std::istreambuf_iterator<char>(restored)),
        std::istreambuf_iterator<char>());

    check(actual == original, "Restored content matches original");

    // Verify that the controller reports correct byte counts.
    check(
        compressionStats.originalBytes == original.size(),
        "Compression reports the correct original size");

    check(
        decompressionStats.originalBytes == original.size(),
        "Decompression reports the correct restored size");

    std::cout << "All controller tests passed.\n";
    return EXIT_SUCCESS;
}
