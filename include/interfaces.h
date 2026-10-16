#ifndef TEAM14_INTERFACES_H
#define TEAM14_INTERFACES_H

#include <array>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

// Shared CONTRACT only: do not put implementation in this header.
// Archive byte layout and algorithm rules: ARCHIVE_FORMAT.md.
namespace team14 {

using FrequencyTable = std::array<std::uint64_t, 256>;
using CodeTable = std::array<std::vector<bool>, 256>;

constexpr std::uint8_t kArchiveVersion = 1;
constexpr std::uint8_t kFlagCrc32Present = 0x01;
constexpr std::uint64_t kDefaultMaxOutputBytes = 1024ULL * 1024 * 1024; // 1 GiB configurable safety default

enum class ErrorCode {
    Ok,
    InvalidArguments,
    InputNotFound,
    PermissionDenied,
    OutputExists,
    IoError,
    InvalidArchive,
    UnsupportedVersion,
    IntegrityFailure,
    UnsafeOutputPath,
    OutputLimitExceeded
};

struct Result {
    ErrorCode code = ErrorCode::Ok;
    std::string message;
    bool ok() const noexcept { return code == ErrorCode::Ok; }
};

struct OperationStats {
    std::uint64_t originalBytes = 0;
    std::uint64_t archiveBytes = 0;
};

// In-memory data representation, NOT written directly as a C++ struct.
struct ArchiveMetadata {
    std::uint8_t version = kArchiveVersion;
    std::uint8_t flags = 0;               // CRC present only when kFlagCrc32Present is set
    std::uint64_t originalBytes = 0;
    std::uint64_t encodedBits = 0;        // meaningful bits BEFORE zero padding
    std::uint32_t crc32 = 0;              // must be zero when flag is clear
    FrequencyTable frequencies{};
};

struct HuffmanNode {
    std::uint64_t frequency = 0;
    int symbol = -1;                      // 0..255 for leaf; -1 for internal node
    std::uint16_t minSymbol = 0;         // minimum original byte in this subtree
    std::shared_ptr<HuffmanNode> left;
    std::shared_ptr<HuffmanNode> right;
};
using HuffmanTree = std::shared_ptr<HuffmanNode>;

struct EncodedStats {
    std::uint64_t meaningfulBits = 0;
    std::uint64_t packedBytes = 0;
};

// DIVISHA: pure Huffman work + streaming bit packing/unpacking.
// encode writes zero padding to complete the last byte; reports meaningful bits.
// decode consumes exactly meaningfulBits, writes exactly originalBytes, and verifies
// final padding is zero. Caller verifies checksum and no trailing archive bytes.
FrequencyTable buildFrequencyTable(std::istream& input);
HuffmanTree buildTree(const FrequencyTable& frequencies);
CodeTable generateCodes(const HuffmanTree& tree);
Result encode(std::istream& input, const CodeTable& codes,
              std::ostream& packedBits, EncodedStats& stats);
Result decode(std::istream& packedBits, const HuffmanTree& tree,
              std::uint64_t meaningfulBits, std::uint64_t originalBytes,
              std::ostream& restored);

// DEBADITYA: file checks and archive serialization. When readArchiveHeader succeeds,
// the stream is positioned at the first payload byte.
Result writeArchive(std::ostream& archive, const ArchiveMetadata& metadata,
                    std::istream& packedPayload);
Result readArchiveHeader(std::istream& archive, ArchiveMetadata& metadata);
Result validateInputFile(const std::filesystem::path& source);
Result validateOutputFile(const std::filesystem::path& destination, bool overwrite);

// CHANDRAKANTH: validation, checksum, output safety and test support.
Result validateArchiveMetadata(const ArchiveMetadata& metadata,
                               std::uint64_t maxOutputBytes);
Result validateOutputPath(const std::filesystem::path& destination);
Result verifyDecodedFile(const std::filesystem::path& output,
                         const ArchiveMetadata& metadata);

// GREESHMA: CLI-facing orchestration; rollback partial output on any failure.
Result compressFile(const std::filesystem::path& input,
                    const std::filesystem::path& archive,
                    OperationStats& stats);
Result decompressFile(const std::filesystem::path& archive,
                      const std::filesystem::path& output,
                      OperationStats& stats);
Result getArchiveInfo(const std::filesystem::path& archive,
                      ArchiveMetadata& metadata);

} // namespace team14
#endif // TEAM14_INTERFACES_H
