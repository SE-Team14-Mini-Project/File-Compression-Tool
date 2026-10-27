
#include "interfaces.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#define TEST_CHECK(condition)                                      \
    do {                                                           \
        if (!(condition)) {                                        \
            std::cerr << "Test failed: " #condition                \
                      << " at " << __FILE__ << ":" << __LINE__     \
                      << '\n';                                     \
            std::exit(EXIT_FAILURE);                               \
        }                                                          \
    } while (false)

int main() {
    namespace fs = std::filesystem;

    const fs::path archivePath = "security-test.huf";
    const fs::path outputPath = "security-test-output.txt";
    const std::string secretMarker =
        "PRIVATE_FILE_CONTENT_DO_NOT_LOG_7F91A2";

    {
        std::ofstream archive(archivePath, std::ios::binary);

        TEST_CHECK(archive.is_open());

        archive << secretMarker;
        archive.close();

        TEST_CHECK(!archive.fail());
    }

    team14::OperationStats stats{};

    const team14::Result result =
        team14::decompressFile(archivePath, outputPath, stats);

    TEST_CHECK(fs::exists(archivePath));
    TEST_CHECK(!result.ok());
    TEST_CHECK(!result.message.empty());
    TEST_CHECK(result.message.find(secretMarker) == std::string::npos);
    TEST_CHECK(!fs::exists(outputPath));

    fs::remove(archivePath);

    std::cout << "Security logging test passed.\n";
    return 0;
}
