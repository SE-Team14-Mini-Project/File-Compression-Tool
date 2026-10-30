// Test-only codec creates a destination after preflight and before publication.
// Real controller, file I/O, serializer, checksum and publisher are exercised.
#include "output_safety_helpers.h"
#include <sstream>

using namespace team14;
using namespace safety_test;
namespace {
enum class Race { File, Directory, Symlink, WriteFailure };
Race mode = Race::File;
fs::path destination, sentinelTarget;
bool hookSucceeded = false;
const std::string sentinel = "RACER_OWNS_THIS_CONTENT_683D";
void inject(std::ostream& output) {
    hookSucceeded = false;
    if (mode == Race::WriteFailure) {
        output.setstate(std::ios::badbit);
    } else {
        if (fs::exists(fs::symlink_status(destination))) return;
        if (mode == Race::File) save(destination, sentinel);
        if (mode == Race::Directory) { fs::create_directory(destination); save(destination / "marker", sentinel); }
        if (mode == Race::Symlink) fs::create_symlink(sentinelTarget, destination);
    }
    hookSucceeded = true;
}
std::string archiveFixture() {
    ArchiveMetadata m; m.originalBytes = m.encodedBits = 10; m.frequencies['A'] = 10;
    m.flags = kFlagCrc32Present; m.crc32 = 0x478ED0CFU; // CRC32/IEEE of ten A bytes
    std::istringstream payload(std::string(2, '\0'), std::ios::binary);
    std::ostringstream archive(std::ios::binary);
    check(writeArchive(archive, m, payload).ok(), "build valid race fixture");
    return archive.str();
}
}
namespace team14 {
FrequencyTable buildFrequencyTable(std::istream& input) {
    FrequencyTable frequencies{}; char c;
    while (input.get(c)) ++frequencies[static_cast<unsigned char>(c)];
    return frequencies;
}
HuffmanTree buildTree(const FrequencyTable& frequencies) {
    auto node = std::make_shared<HuffmanNode>(); node->symbol = 'A'; node->minSymbol = 'A'; node->frequency = frequencies['A'];
    return node;
}
CodeTable generateCodes(const HuffmanTree&) {
    // The real metadata validator also checks reconstructed code lengths.
    CodeTable codes{};
    codes['A'] = {false};
    return codes;
}
Result encode(std::istream& input, const CodeTable&, std::ostream& output, EncodedStats& stats) {
    char c; std::uint64_t count = 0;
    while (input.get(c)) ++count;
    stats.meaningfulBits = count; stats.packedBytes = count / 8 + (count % 8 != 0);
    for (std::uint64_t n = 0; n < stats.packedBytes; ++n) output.put('\0');
    inject(output); return {};
}
Result decode(std::istream& input, const HuffmanTree&, std::uint64_t bits,
              std::uint64_t original, std::ostream& output) {
    for (std::uint64_t n = 0; n < bits / 8 + (bits % 8 != 0); ++n) {
        if (input.get() == std::char_traits<char>::eof()) return {ErrorCode::InvalidArchive, "Truncated test fixture."};
    }
    for (std::uint64_t n = 0; n < original; ++n) output.put('A');
    inject(output); return {};
}
}
int main() {
    try {
        Workspace workspace; const auto& root = workspace.root;
        const auto source = root / "source.bin", archive = root / "input.huf";
        save(source, std::string(10, 'A')); const auto bytes = archiveFixture(); save(archive, bytes);
        sentinelTarget = root / "sentinel-target"; save(sentinelTarget, sentinel);
        bool symlinks = true;
        std::error_code error; fs::create_symlink(sentinelTarget, root / "probe-link", error);
        if (error) { symlinks = false; std::cout << "SKIP symlink race: platform/account cannot create symlinks\n"; }
        else fs::remove(root / "probe-link");
        for (bool decompress : {false, true}) {
            for (auto race : {Race::File, Race::Directory, Race::Symlink, Race::WriteFailure}) {
                if (race == Race::Symlink && !symlinks) continue;
                mode = race; hookSucceeded = false;
                destination = root / "racing-output"; OperationStats stats{999, 888};
                const auto result = decompress ? decompressFile(archive, destination, stats) : compressFile(source, destination, stats);
                check(hookSucceeded, "injected race/write failure occurs after preflight");
                failed(result, stats);
                if (race == Race::File) {
                    check(result.code == ErrorCode::OutputExists, "late regular-file collision reported");
                    check(load(destination) == sentinel, "late file is never overwritten");
                } else if (race == Race::Directory) {
                    check(fs::is_directory(destination) && load(destination / "marker") == sentinel, "late directory and marker are preserved");
                } else if (race == Race::Symlink) {
                    check(fs::is_symlink(fs::symlink_status(destination)), "late symlink is not replaced");
                    check(load(sentinelTarget) == sentinel, "late symlink target preserved");
                } else {
                    check(result.code == ErrorCode::IoError && !fs::exists(destination), "late stream write failure leaves no final output");
                }
                check(load(source) == std::string(10, 'A') && load(archive) == bytes, "race preserves all inputs");
                noTemporaryFiles(root);
                fs::remove_all(destination); // Only the fixture-created racer entry.
            }
        }
        check(load(sentinelTarget) == sentinel, "sentinel unchanged after all races");
        std::cout << "PASS: " << checks << " deterministic race/write-failure checks\n";
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
