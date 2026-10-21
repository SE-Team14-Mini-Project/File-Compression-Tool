// FR-DEC-01 (TC-08 / ST-08): archives with an unsupported magic or format
// version must be rejected before any restoration is trusted.
// Standalone: g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Iinclude
//   src/archive.cpp src/validation.cpp tests/test_validation.cpp -o /tmp/team14-test-validation
#include "interfaces.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <algorithm>

using namespace team14;
namespace {
unsigned checks = 0;
void expect(bool ok, const char* name) {
    ++checks;
    if (!ok) throw std::runtime_error(name);
}

// Documented AAAAABBBCC vector from ARCHIVE_FORMAT.md section 5 (59 bytes).
std::string validArchive() {
    ArchiveMetadata m;
    m.originalBytes = 10;
    m.encodedBits = 15;
    m.frequencies['A'] = 5;
    m.frequencies['B'] = 3;
    m.frequencies['C'] = 2;
    std::istringstream payload(std::string("\x07\xf4", 2), std::ios::binary);
    std::ostringstream out(std::ios::binary);
    expect(writeArchive(out, m, payload).ok(), "build valid archive");
    return out.str();
}

Result parse(const std::string& bytes, ArchiveMetadata& m) {
    std::istringstream in(bytes, std::ios::binary);
    return readArchiveHeader(in, m);
}

// A rejected archive must not leave half-parsed metadata behind for the caller.
bool metadataIsReset(const ArchiveMetadata& m) {
    return m.version == kArchiveVersion &&
           m.flags == 0 &&
           m.originalBytes == 0 &&
           m.encodedBits == 0 &&
           m.crc32 == 0 &&
           std::all_of(m.frequencies.begin(), m.frequencies.end(),
                       [](std::uint64_t frequency) {
                           return frequency == 0;
                       });
}
} // namespace

int main() {
    try {
        const std::string good = validArchive();
        ArchiveMetadata m;

        // Baseline: a valid archive is accepted by the parser and the validator.
        expect(parse(good, m).ok(), "valid archive accepted");
        expect(validateArchiveMetadata(m, kDefaultMaxOutputBytes).ok(), "valid metadata accepted");

        // --- Magic bytes: every byte position, plus near-miss values. ---
        for (std::size_t pos = 0; pos < 4; ++pos) {
            auto bad = good;
            bad[pos] = static_cast<char>(bad[pos] ^ 0x20);   // flips letter case
            m = ArchiveMetadata{};
            expect(parse(bad, m).code == ErrorCode::InvalidArchive, "magic byte changed");
            expect(metadataIsReset(m), "metadata empty after bad magic");
        }
        for (const char* magic : {"HUF2", "huf1", "ZIP1", "PK\x03\x04"}) {
            auto bad = good;
            bad.replace(0, 4, magic, 4);
            expect(parse(bad, m).code == ErrorCode::InvalidArchive, "wrong magic string");
        }
        // A file that is not an archive at all (common user mistake).
        expect(parse("hello, this is just a text file", m).code == ErrorCode::InvalidArchive, "plain text file");
        expect(parse(std::string(64, '\0'), m).code == ErrorCode::InvalidArchive, "all-zero file");
        // Too short to even contain the magic.
        expect(parse("", m).code == ErrorCode::InvalidArchive, "empty file");
        expect(parse("HUF", m).code == ErrorCode::InvalidArchive, "partial magic");

        // --- Format version (byte offset 4). ---
        for (unsigned version : {0U, 2U, 3U, 127U, 128U, 255U}) {
            auto bad = good;
            bad[4] = static_cast<char>(version);
            m = ArchiveMetadata{};
            expect(parse(bad, m).code == ErrorCode::UnsupportedVersion, "unsupported version rejected by parser");
            expect(metadataIsReset(m), "metadata empty after bad version");
        }
        // Magic is checked first: if both are wrong it is reported as bad magic.
        {
            auto bad = good;
            bad[0] = 'X';
            bad[4] = 9;
            expect(parse(bad, m).code == ErrorCode::InvalidArchive, "bad magic wins over bad version");
        }
        // Magic and version only: the parser must stop at the version, so later
        // header fields are not trusted at all.
        {
            std::string onlyPrefix = "HUF1";
            onlyPrefix.push_back(static_cast<char>(2));
            expect(parse(onlyPrefix, m).code == ErrorCode::UnsupportedVersion, "version rejected before rest of header is read");
        }

        // --- validateArchiveMetadata: second line of defence for the controller. ---
        for (unsigned version : {0U, 2U, 255U}) {
            ArchiveMetadata meta;
            meta.version = static_cast<std::uint8_t>(version);
            const Result r = validateArchiveMetadata(meta, kDefaultMaxOutputBytes);
            expect(r.code == ErrorCode::UnsupportedVersion, "validator rejects unsupported version");
            expect(!r.ok() && !r.message.empty(), "rejection carries an actionable message");
        }
        {
            ArchiveMetadata meta;                       // defaults to kArchiveVersion
            expect(validateArchiveMetadata(meta, kDefaultMaxOutputBytes).ok(), "supported version accepted");
        }

        std::cout << "PASS: " << checks << " validation checks\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}