#include "interfaces.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main() {
    namespace fs = std::filesystem;

    const fs::path archivePath = "security-test.huf";
    const fs::path outputPath = "security-test-output.txt";

    const std::string secretMarker =
        "PRIVATE_FILE_CONTENT_DO_NOT_LOG_7F91A2";

    {
        std::ofstream archive(archivePath, std::ios::binary);
        assert(archive.is_open());
        archive << secretMarker;
    }

    team14::OperationStats stats{};
    const team14::Result result =
        team14::decompressFile(archivePath, outputPath, stats);

    assert(!result.ok());
    assert(!result.message.empty());
    assert(result.message.find(secretMarker) == std::string::npos);
    assert(!fs::exists(outputPath));

    fs::remove(archivePath);

    std::cout << "Security logging test passed.\n";
    return 0;
}
