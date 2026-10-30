#include "interfaces.h"

#include <iomanip>
#include <iostream>
#include <sstream>
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

std::string hexadecimal(std::uint64_t value, int width) {
    std::ostringstream output;
    output << "0x" << std::hex << std::uppercase << std::setw(width)
           << std::setfill('0') << value;
    return output.str();
}

void printArchiveInfo(const team14::ArchiveMetadata& metadata) {
    std::uint64_t symbols = 0;
    for (const auto frequency : metadata.frequencies) {
        if (frequency != 0) ++symbols;
    }
    const std::uint64_t headerBytes = 30 + 9 * symbols;
    const std::uint64_t payloadBytes =
        metadata.encodedBits / 8 + (metadata.encodedBits % 8 != 0);
    std::cout << "Archive magic: HUF1\n"
              << "Archive version: " << static_cast<unsigned>(metadata.version) << '\n'
              << "Flags: " << hexadecimal(metadata.flags, 2) << '\n'
              << "Original filename: not stored in version 1\n"
              << "Original size: " << metadata.originalBytes << " bytes\n"
              << "Archive size: " << headerBytes + payloadBytes << " bytes\n"
              << "Header size: " << headerBytes << " bytes\n"
              << "Payload size: " << payloadBytes << " bytes\n"
              << "Encoded bits: " << metadata.encodedBits << '\n'
              << "Symbol count: " << symbols << '\n';
    if ((metadata.flags & team14::kFlagCrc32Present) != 0) {
        std::cout << "CRC32: present, stored value "
                  << hexadecimal(metadata.crc32, 8)
                  << " (not verified by info)\n";
    } else {
        std::cout << "CRC32: absent (checksum not available)\n";
    }
    std::cout << "Integrity: not verified (metadata inspection only)\n"
              << "Tree metadata (byte=frequency):";
    if (symbols == 0) std::cout << " none";
    for (unsigned symbol = 0; symbol < metadata.frequencies.size(); ++symbol) {
        if (metadata.frequencies[symbol] != 0) {
            std::cout << ' ' << hexadecimal(symbol, 2) << '='
                      << metadata.frequencies[symbol];
        }
    }
    std::cout << '\n';
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

        printArchiveInfo(metadata);
        return kExitSuccess;
    }

    std::cerr << "Error: unknown command '" << command << "'.\n";
    printGeneralUsage(std::cerr);
    return kExitUsage;
}
