// Standalone: g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude
// src/archive.cpp tests/test_archive.cpp -o /tmp/team14-test-archive
#include "interfaces.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>

using namespace team14;
namespace {
unsigned checks = 0;
void expect(bool ok, const char* name) {
    ++checks;
    if (!ok) throw std::runtime_error(name);
}
std::string unhex(const std::string& hex) {
    std::string out;
    for (std::size_t n = 0; n < hex.size(); n += 2) out += static_cast<char>(std::stoul(hex.substr(n, 2), nullptr, 16));
    return out;
}
ArchiveMetadata sample() {
    ArchiveMetadata m;
    m.originalBytes = 10; m.encodedBits = 15;
    m.frequencies['A'] = 5; m.frequencies['B'] = 3; m.frequencies['C'] = 2;
    return m;
}
std::string write(const ArchiveMetadata& m, const std::string& payload) {
    std::istringstream input(payload, std::ios::binary);
    std::ostringstream output(std::ios::binary);
    expect(writeArchive(output, m, input).ok(), "write valid archive");
    return output.str();
}
Result parse(const std::string& data, ArchiveMetadata& m) {
    std::istringstream input(data, std::ios::binary);
    return readArchiveHeader(input, m);
}
void same(const ArchiveMetadata& a, const ArchiveMetadata& b) {
    expect(a.version == b.version && a.flags == b.flags && a.originalBytes == b.originalBytes &&
           a.encodedBits == b.encodedBits && a.crc32 == b.crc32 && a.frequencies == b.frequencies,
           "all metadata fields round trip");
}
void rejectedWrite(const ArchiveMetadata& m, const std::string& payload, ErrorCode code) {
    std::istringstream input(payload, std::ios::binary);
    std::ostringstream output(std::ios::binary);
    expect(writeArchive(output, m, input).code == code, "reject invalid write");
}
struct BrokenRead : std::streambuf {
    int_type underflow() override { throw std::ios_base::failure("simulated read failure"); }
};
struct LimitedWrite : std::streambuf {
    std::size_t remaining;
    explicit LimitedWrite(std::size_t limit) : remaining(limit) {}
    std::streamsize xsputn(const char*, std::streamsize n) override {
        auto accepted = std::min<std::size_t>(remaining, static_cast<std::size_t>(n));
        remaining -= accepted;
        return static_cast<std::streamsize>(accepted);
    }
    int_type overflow(int_type) override { return traits_type::eof(); }
};
struct FlushFailure : std::stringbuf { int sync() override { return -1; } };
struct NoSeek : std::stringbuf {
    explicit NoSeek(const std::string& s) : std::stringbuf(s, std::ios::in) {}
    pos_type seekoff(off_type, std::ios::seekdir, std::ios::openmode) override { return pos_type(off_type(-1)); }
    pos_type seekpos(pos_type, std::ios::openmode) override { return pos_type(off_type(-1)); }
};
}
int main() {
    try {
        const auto payload = unhex("07f4");
        const auto golden = unhex("48554631010000000a000000000000000f00000000000000000000000300"
                                  "41050000000000000042030000000000000043020000000000000007f4");
        expect(golden.size() == 59, "golden length");
        expect(write(sample(), payload) == golden, "exact documented 59-byte archive");
        ArchiveMetadata m;
        std::istringstream input(golden, std::ios::binary);
        expect(readArchiveHeader(input, m).ok(), "read golden header");
        same(m, sample());
        expect(input.tellg() == 57, "stream at first payload byte");
        expect(input.get() == 7 && input.get() == 244, "payload untouched");
        NoSeek nonSeekable(golden);
        std::istream noSeek(&nonSeekable);
        expect(readArchiveHeader(noSeek, m).ok(), "non-seekable header input");
        expect(noSeek.get() == 7, "non-seekable payload untouched");
        expect(write(ArchiveMetadata{}, "") == unhex("485546310100000000000000000000000000000000000000000000000000"), "empty 30-byte archive");
        ArchiveMetadata single;
        single.originalBytes = single.encodedBits = 10; single.frequencies['A'] = 10;
        const auto singleData = write(single, std::string(2, '\0'));
        expect(singleData.size() == 41, "single-symbol 41-byte archive");
        expect(parse(singleData, m).ok(), "read single symbol"); same(m, single);
        ArchiveMetadata all;
        all.originalBytes = 256; all.encodedBits = 2048; all.frequencies.fill(1);
        std::string allPayload;
        for (unsigned n = 0; n < 256; ++n) allPayload += static_cast<char>(n);
        const auto allData = write(all, allPayload);
        expect(allData.size() == 30 + 9 * 256 + 256, "all 256 symbols count and size");
        expect(static_cast<unsigned char>(allData[28]) == 0 && static_cast<unsigned char>(allData[29]) == 1, "count 256 little endian");
        expect(parse(allData, m).ok(), "parse all symbols including 0 and 255"); same(m, all);
        auto crc = sample(); crc.flags = kFlagCrc32Present; crc.crc32 = 0x12345678;
        const auto crcData = write(crc, payload);
        expect(crcData.substr(24, 4) == unhex("78563412"), "CRC little endian");
        expect(parse(crcData, m).ok(), "CRC is preserved not recomputed"); same(m, crc);
        // Header-only parser must not attempt payload/trailer validation.
        expect(parse(golden.substr(0, 57), m).ok(), "header-only read");
        expect(parse(golden + "extra", m).ok(), "payload boundary owned by decoder/controller");
        for (std::size_t n = 0; n < 57; ++n) {
            m = sample();
            expect(parse(golden.substr(0, n), m).code == ErrorCode::InvalidArchive, "every header truncation rejected");
            same(m, ArchiveMetadata{});
        }
        auto bad = golden; bad[0] = 'X';
        expect(parse(bad, m).code == ErrorCode::InvalidArchive, "wrong magic");
        bad = golden; bad[4] = 2;
        expect(parse(bad, m).code == ErrorCode::UnsupportedVersion, "unsupported version");
        for (unsigned offset : {5U, 6U, 7U, 24U, 28U, 29U, 31U, 39U, 48U}) {
            bad = golden;
            switch (offset) {
                case 5: bad[5] = 2; break;
                case 6: case 7: bad[offset] = 1; break;
                case 24: bad[24] = 1; break;
                case 28: bad[28] = 4; break;
                case 29: bad[29] = 2; break;
                case 31: bad[31] = 0; break;
                case 39: bad[39] = 'A'; break;
                case 48: bad[48] = 'A'; break;
            }
            expect(parse(bad, m).code == ErrorCode::InvalidArchive, "malformed header rejected");
        }
        bad = golden; bad[8] = 11;
        expect(parse(bad, m).code == ErrorCode::InvalidArchive, "frequency sum mismatch");
        bad = golden; for (unsigned n = 31; n < 39; ++n) bad[n] = static_cast<char>(255);
        expect(parse(bad, m).code == ErrorCode::InvalidArchive, "frequency overflow");
        auto invalid = sample(); invalid.version = 2; rejectedWrite(invalid, payload, ErrorCode::UnsupportedVersion);
        invalid = sample(); invalid.flags = 128; rejectedWrite(invalid, payload, ErrorCode::InvalidArchive);
        invalid = sample(); invalid.crc32 = 1; rejectedWrite(invalid, payload, ErrorCode::InvalidArchive);
        invalid = sample(); invalid.originalBytes = 9; rejectedWrite(invalid, payload, ErrorCode::InvalidArchive);
        invalid = sample(); invalid.frequencies['A'] = std::numeric_limits<std::uint64_t>::max(); rejectedWrite(invalid, payload, ErrorCode::InvalidArchive);
        invalid = single; invalid.encodedBits = 11; rejectedWrite(invalid, "\0\0", ErrorCode::InvalidArchive);
        invalid = sample(); invalid.encodedBits = 0; rejectedWrite(invalid, "", ErrorCode::InvalidArchive);
        invalid = ArchiveMetadata{}; invalid.encodedBits = 1; rejectedWrite(invalid, "", ErrorCode::InvalidArchive);
        rejectedWrite(sample(), payload.substr(0, 1), ErrorCode::InvalidArchive);
        rejectedWrite(sample(), payload + "x", ErrorCode::InvalidArchive);
        rejectedWrite(sample(), unhex("07f5"), ErrorCode::InvalidArchive);
        rejectedWrite(ArchiveMetadata{}, "x", ErrorCode::InvalidArchive);
        for (unsigned remainder = 1; remainder <= 8; ++remainder) {
            auto edge = single; edge.originalBytes = edge.encodedBits = edge.frequencies['A'] = remainder;
            expect(write(edge, std::string(1, '\0')).size() == 40, "all final-byte alignments");
            if (remainder < 8) rejectedWrite(edge, std::string(1, '\1'), ErrorCode::InvalidArchive);
        }
        for (std::size_t limit : {0U, 5U, 29U, 57U, 58U}) {
            LimitedWrite buffer(limit); std::ostream out(&buffer); std::istringstream in(payload);
            expect(writeArchive(out, sample(), in).code == ErrorCode::IoError, "short output write");
        }
        { FlushFailure buffer; std::ostream out(&buffer); std::istringstream in(payload);
          expect(writeArchive(out, sample(), in).code == ErrorCode::IoError, "flush failure"); }
        { BrokenRead buffer; std::istream in(&buffer);
          expect(readArchiveHeader(in, m).code == ErrorCode::IoError, "input hardware failure"); }
        { BrokenRead buffer; std::istream in(&buffer); std::ostringstream out;
          expect(writeArchive(out, sample(), in).code == ErrorCode::IoError, "payload hardware failure"); }
        { LimitedWrite buffer(1); std::ostream out(&buffer); out.exceptions(std::ios::badbit | std::ios::failbit);
          std::istringstream in(payload);
          expect(writeArchive(out, sample(), in).code == ErrorCode::IoError, "output exception converted"); }
        { std::istringstream in("HUF"); in.exceptions(std::ios::badbit | std::ios::failbit);
          expect(readArchiveHeader(in, m).code == ErrorCode::InvalidArchive, "header exception converted"); }
        { std::istringstream in(payload.substr(0, 1)); in.exceptions(std::ios::badbit | std::ios::failbit); std::ostringstream out;
          expect(writeArchive(out, sample(), in).code == ErrorCode::InvalidArchive, "payload truncation exception converted"); }
        { std::istringstream in(payload); in.exceptions(std::ios::badbit | std::ios::failbit | std::ios::eofbit); std::ostringstream out;
          expect(writeArchive(out, sample(), in).ok(), "expected payload EOF with exceptions"); }
        // 64-bit values beyond 32 bits, checked without allocating huge payloads.
        bad = singleData;
        const auto large = unhex("0807060504030201");
        bad.replace(8, 8, large); bad.replace(16, 8, large); bad.replace(31, 8, large);
        expect(parse(bad, m).ok(), "64-bit fields parse");
        expect(m.originalBytes == 0x0102030405060708ULL && m.encodedBits == m.originalBytes && m.frequencies['A'] == m.originalBytes, "64-bit little endian");
        // Multi-buffer spool and exact byte-preserving transport, not a codec test.
        auto largeMeta = all;
        largeMeta.frequencies.fill(1024); largeMeta.originalBytes = 256 * 1024;
        largeMeta.encodedBits = largeMeta.originalBytes * 8;
        std::string largePayload;
        for (unsigned n = 0; n < 1024; ++n) largePayload += allPayload;
        const auto largeArchive = write(largeMeta, largePayload);
        expect(largeArchive.substr(30 + 9 * 256) == largePayload, "buffered binary payload preserved");
        // Independent two-symbol valid streams, deterministic randomized fixtures.
        std::mt19937 rng(14);
        for (unsigned n = 0; n < 300; ++n) {
            ArchiveMetadata random;
            const unsigned low = rng() % 128, high = 128 + rng() % 128;
            random.frequencies[low] = 1 + rng() % 500;
            random.frequencies[high] = 1 + rng() % 500;
            random.originalBytes = random.frequencies[low] + random.frequencies[high];
            random.encodedBits = random.originalBytes;
            std::string packed(static_cast<std::size_t>((random.encodedBits + 7) / 8), '\0');
            // Lower frequency symbol is the left/zero child; ties use byte order.
            const auto zeroCount = std::min(random.frequencies[low], random.frequencies[high]);
            for (auto bit = zeroCount; bit < random.encodedBits; ++bit) {
                const auto index = static_cast<std::size_t>(bit / 8);
                packed[index] = static_cast<char>(static_cast<unsigned char>(packed[index]) | (1U << (7 - bit % 8)));
            }
            const auto serialized = write(random, packed);
            expect(parse(serialized, m).ok(), "random header parse"); same(m, random);
            expect(serialized.substr(48) == packed, "random payload preserved");
        }
        std::cout << "PASS: " << checks << " archive checks\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
