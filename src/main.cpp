#include "interfaces.h"

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    using namespace team14;

    if (argc < 2) {
        std::cerr << "Usage:\n"
                  << "  huf compress <input> <archive>\n"
                  << "  huf decompress <archive> <output>\n"
                  << "  huf info <archive>\n";
        return 1;
    }

    const std::string command = argv[1];

    if (command == "compress") {
        if (argc != 4) {
            std::cerr << "Usage: huf compress <input> <archive>\n";
            return 1;
        }

        OperationStats stats;
        Result result = compressFile(argv[2], argv[3], stats);

        if (!result.ok()) {
            std::cerr << "Compression failed: " << result.message << '\n';
            return 1;
        }

        std::cout << "Compression successful.\n"
                  << "Original size: " << stats.originalBytes << " bytes\n"
                  << "Archive size: " << stats.archiveBytes << " bytes\n";
        return 0;
    }

    if (command == "decompress") {
        if (argc != 4) {
            std::cerr << "Usage: huf decompress <archive> <output>\n";
            return 1;
        }

        OperationStats stats;
        Result result = decompressFile(argv[2], argv[3], stats);

        if (!result.ok()) {
            std::cerr << "Decompression failed: " << result.message << '\n';
            return 1;
        }

        std::cout << "Decompression successful.\n"
                  << "Restored size: " << stats.originalBytes << " bytes\n";
        return 0;
    }

    if (command == "info") {
        if (argc != 3) {
            std::cerr << "Usage: huf info <archive>\n";
            return 1;
        }

        ArchiveMetadata metadata;
        Result result = getArchiveInfo(argv[2], metadata);

        if (!result.ok()) {
            std::cerr << "Could not read archive info: "
                      << result.message << '\n';
            return 1;
        }

        std::cout << "Archive version: "
                  << static_cast<int>(metadata.version) << '\n'
                  << "Original size: " << metadata.originalBytes << " bytes\n"
                  << "Encoded bits: " << metadata.encodedBits << '\n'
                  << "CRC32: "
                  << ((metadata.flags & kFlagCrc32Present)
                          ? "present"
                          : "absent")
                  << '\n';
        return 0;
    }

    std::cerr << "Unknown command: " << command << '\n'
              << "Available commands: compress, decompress, info\n";
    return 1;
}


