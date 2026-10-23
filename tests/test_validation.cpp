// FR-DEC-01 (TC-08 / ST-08): archives with an unsupported magic or format
// version must be rejected before any restoration is trusted.
// Standalone: g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Iinclude
//   src/archive.cpp src/validation.cpp tests/test_validation.cpp -o /tmp/team14-test-validation
#include "interfaces.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <limits>
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


        // --- Malformed metadata and security bounds (NFR-SEC-01). ---
        {
            ArchiveMetadata meta;
            meta.flags = 0x80;
            expect(
                validateArchiveMetadata(meta, kDefaultMaxOutputBytes).code ==
                    ErrorCode::InvalidArchive,
                "unknown flag bits rejected");
        }
        {
            ArchiveMetadata meta;
            meta.crc32 = 1; // CRC flag is disabled by default.
            expect(
                validateArchiveMetadata(meta, kDefaultMaxOutputBytes).code ==
                    ErrorCode::InvalidArchive,
                "CRC rejected when checksum flag is disabled");
        }
        {
            ArchiveMetadata meta;
            meta.originalBytes = 10;
            meta.encodedBits = 10;
            meta.frequencies['A'] = 10;
            expect(
                validateArchiveMetadata(meta, 9).code ==
                    ErrorCode::OutputLimitExceeded,
                "output exceeding configured limit rejected");
        }
        {
            ArchiveMetadata meta;
            meta.originalBytes = 11;
            meta.encodedBits = 15;
            meta.frequencies['A'] = 5;
            meta.frequencies['B'] = 3;
            meta.frequencies['C'] = 2;
            expect(
                validateArchiveMetadata(meta, kDefaultMaxOutputBytes).code ==
                    ErrorCode::InvalidArchive,
                "frequency total mismatch rejected");
        }
        {
            ArchiveMetadata meta;
            meta.frequencies[0] =
                std::numeric_limits<std::uint64_t>::max();
            meta.frequencies[1] = 1;
            meta.originalBytes =
                std::numeric_limits<std::uint64_t>::max();
            expect(
                validateArchiveMetadata(
                    meta, std::numeric_limits<std::uint64_t>::max()).code ==
                    ErrorCode::InvalidArchive,
                "frequency sum overflow rejected");
        }
        {
            ArchiveMetadata meta;
            meta.encodedBits = 1; // Empty archive cannot contain encoded bits.
            expect(
                validateArchiveMetadata(meta, kDefaultMaxOutputBytes).code ==
                    ErrorCode::InvalidArchive,
                "empty archive with nonzero bit count rejected");
        }
        {
            ArchiveMetadata meta;
            meta.originalBytes = 1;
            meta.frequencies['A'] = 1;
            // encodedBits remains zero.
            expect(
                validateArchiveMetadata(meta, kDefaultMaxOutputBytes).code ==
                    ErrorCode::InvalidArchive,
                "non-empty archive with zero encoded bits rejected");
        }
        {
            ArchiveMetadata meta;
            meta.originalBytes = 2;
            meta.encodedBits = 3;
            meta.frequencies['A'] = 2;
            expect(
                validateArchiveMetadata(meta, kDefaultMaxOutputBytes).code ==
                    ErrorCode::InvalidArchive,
                "single-symbol archive with incorrect bit count rejected");
        }


        std::cout << "PASS: " << checks << " validation checks\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
