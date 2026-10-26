#include "interfaces.h"

#include <iostream>
#include <string>

namespace {

constexpr int kExitSuccess = 0;
constexpr int kExitFailure = 1;
constexpr int kExitUsage = 2;

void printGeneralUsage(std::ostream& output) {
    output << "Usage:\n"
              << "  huf compress <input> <archive>\n"
              << "  huf decompress <archive> <output>\n"
              << "  huf info <archive>\n"
              << "  huf help\n";
}

void printOperationError(const std::string& operation,
                         const team14::Result& result) {
    std::cerr << "Error: " << operation << " failed";

    if (!result.message.empty()) {
        std::cerr << ": " << result.message;
    }

    std::cerr << '\n';
}

} // namespace

int main(int argc, char* argv[]) {
    using namespace team14;

    if (argc < 2) {
        std::cerr << "Error: missing command.\n";
        printGeneralUsage(std::cerr);
        return kExitUsage;
    }

    const std::string command = argv[1];

    if (command == "help" || command == "--help" ||
        command == "-h") {
        printGeneralUsage(std::cout);
        return kExitSuccess;
    }

    if (command == "compress") {
        if (argc != 4) {
            std::cerr
                << "Error: compress requires an input file and an archive path.\n"
                << "Usage: huf compress <input> <archive>\n";
            return kExitUsage;
        }

        OperationStats stats;
        Result result = compressFile(argv[2], argv[3], stats);

        if (!result.ok()) {
            printOperationError("Compression", result);
            return kExitFailure;
        }

        std::cout << "Compression successful.\n"
                  << "Original size: " << stats.originalBytes << " bytes\n"
                  << "Archive size: " << stats.archiveBytes << " bytes\n";

        if (stats.archiveBytes == 0) {
            std::cout << "Compression ratio: N/A (archive size is zero)\n";
        } else {
            const double ratio =
                static_cast<double>(stats.originalBytes) /
                static_cast<double>(stats.archiveBytes);

            std::cout << "Compression ratio: "
                      << ratio << ":1\n";
        }
        return kExitSuccess;
    }

    if (command == "decompress") {
        if (argc != 4) {
            std::cerr
                << "Error: decompress requires an archive and an output path.\n"
                << "Usage: huf decompress <archive> <output>\n";
            return kExitUsage;
        }

        OperationStats stats;
        Result result = decompressFile(argv[2], argv[3], stats);

        if (!result.ok()) {
            printOperationError("Decompression", result);
            return kExitFailure;
        }

        std::cout << "Decompression successful.\n"
                  << "Restored size: " << stats.originalBytes << " bytes\n";
        return kExitSuccess;
    }

    if (command == "info") {
        if (argc != 3) {
            std::cerr
                << "Error: info requires exactly one archive path.\n"
                << "Usage: huf info <archive>\n";
            return kExitUsage;
        }

        ArchiveMetadata metadata;
        Result result = getArchiveInfo(argv[2], metadata);

        if (!result.ok()) {
            printOperationError("Reading archive information", result);
            return kExitFailure;
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
        return kExitSuccess;
    }

    std::cerr << "Error: unknown command '" << command << "'.\n";
    printGeneralUsage(std::cerr);
    return kExitUsage;
}
