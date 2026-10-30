#include "interfaces.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;
using namespace team14;
namespace {
unsigned checks = 0;
void check(bool value, const char* label) {
    ++checks;
    if (!value) throw std::runtime_error(label);
}
std::string load(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}
void save(const fs::path& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    out.close(); check(!out.fail(), "fixture saved");
}
bool reset(const ArchiveMetadata& m) {
    return m.version == kArchiveVersion && m.flags == 0 && m.originalBytes == 0 &&
           m.encodedBits == 0 && m.crc32 == 0 && m.frequencies == FrequencyTable{};
}
void set64(std::string& data, unsigned offset, std::uint64_t value) {
    for (unsigned n = 0; n < 8; ++n) data[offset+n] = static_cast<char>(value >> (8*n));
}
}
int main() {
    // CTest supplies an isolated working directory for this test.
    const fs::path path = "archive-info-fixture.huf";
    struct Cleanup { fs::path path; ~Cleanup(){std::error_code ec;fs::remove(path,ec);} } cleanup{path};
    try {
        ArchiveMetadata expected;
        expected.originalBytes = 10; expected.encodedBits = 15;
        expected.frequencies['A'] = 5; expected.frequencies['B'] = 3; expected.frequencies['C'] = 2;
        std::istringstream payload(std::string("\x07\xf4", 2), std::ios::binary);
        std::ostringstream output(std::ios::binary);
        check(writeArchive(output, expected, payload).ok(), "build golden fixture");
        const auto golden = output.str();
        save(path, golden);
        const auto before = load(path);
        ArchiveMetadata actual;
        check(getArchiveInfo(path, actual).ok(), "info succeeds");
        check(actual.originalBytes == 10 && actual.encodedBits == 15 && actual.frequencies == expected.frequencies, "metadata exact");
        check(load(path) == before, "info does not modify archive");
        for (auto bytes : {golden.substr(0, 5), golden.substr(0, golden.size()-1), golden + "x"}) {
            save(path, bytes); actual = expected;
            check(!getArchiveInfo(path, actual).ok(), "truncated or trailing archive rejected");
            check(reset(actual), "failed inspection leaves no stale metadata");
            check(load(path) == bytes, "failed inspection does not modify archive");
        }
        auto bad = golden; bad[0] = 'X'; save(path, bad);
        check(getArchiveInfo(path, actual).code == ErrorCode::InvalidArchive, "bad magic rejected");
        bad = golden; bad[4] = 2; save(path, bad);
        check(getArchiveInfo(path, actual).code == ErrorCode::UnsupportedVersion, "unsupported version rejected");
        auto crc = golden; crc[5] = 1; crc[24] = 0x12; save(path, crc);
        check(getArchiveInfo(path, actual).ok() && actual.crc32 == 0x12, "stored CRC inspected without claiming verification");
        // Changing meaningful payload bits is deliberately not decoded by info.
        crc.back() ^= 0x40; save(path, crc);
        check(getArchiveInfo(path, actual).ok(), "info does not decode payload");
        save(path, std::string("HUF1\1\0", 6) + std::string(24, '\0'));
        check(getArchiveInfo(path, actual).ok() && actual.originalBytes == 0, "empty archive metadata");
        // Sparse, single-symbol archive: info must not allocate/decode the
        // declared >1 GiB original size or apply a decoding-only safety limit.
        std::string large(39, '\0'); large.replace(0, 4, "HUF1"); large[4]=1; large[28]=1; large[30]='A';
        const auto declared = kDefaultMaxOutputBytes + 1;
        set64(large,8,declared);set64(large,16,declared);set64(large,31,declared);
        save(path,large);
        fs::resize_file(path,39+declared/8+(declared%8!=0));
        check(getArchiveInfo(path,actual).ok() && actual.originalBytes == declared, "large metadata inspected without decoding");
        fs::remove(path); actual = expected;
        check(getArchiveInfo(path,actual).code==ErrorCode::InputNotFound && reset(actual), "missing input and reset metadata");
        check(!getArchiveInfo(fs::current_path(),actual).ok(), "directory rejected");
        std::cout << "PASS: " << checks << " archive-info checks\n";
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
