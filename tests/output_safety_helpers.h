#ifndef TEAM14_OUTPUT_SAFETY_TEST_HELPERS_H
#define TEAM14_OUTPUT_SAFETY_TEST_HELPERS_H
// Test support only; never included by the production library or CLI.
#include "interfaces.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

namespace safety_test {
namespace fs = std::filesystem;
inline unsigned checks = 0;
inline void check(bool ok, const char* label) {
    ++checks;
    if (!ok) throw std::runtime_error(label);
}
inline void save(const fs::path& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    check(out.is_open(), "create test fixture");
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    out.close();
    check(!out.fail(), "finish test fixture");
}
inline std::string load(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    check(in.is_open(), "read test fixture");
    return {std::istreambuf_iterator<char>(in), {}};
}
struct Workspace {
    fs::path root;
    bool owned = false;
    Workspace() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (unsigned n = 0; n < 32; ++n) {
            root = fs::current_path() / ("overwrite-fixtures-" + std::to_string(stamp) + "-" + std::to_string(n));
            std::error_code error;
            if (fs::create_directory(root, error)) { owned = true; return; }
            if (error && error != std::errc::file_exists) throw std::runtime_error("Cannot create test workspace");
        }
        throw std::runtime_error("Cannot reserve unique test workspace");
    }
    ~Workspace() { if (owned) { std::error_code error; fs::remove_all(root, error); } }
};
inline void noTemporaryFiles(const fs::path& parent) {
    for (const auto& entry : fs::recursive_directory_iterator(parent)) {
        const auto name = entry.path().filename().string();
        check(name.find(".team14-tmp-") != 0 && name.find(".team14-io-check-") != 0,
              "no staging directory or access probe remains");
    }
}
inline void failed(const team14::Result& result, const team14::OperationStats& stats) {
    check(!result.ok(), "operation fails without claiming success");
    check(stats.originalBytes == 0 && stats.archiveBytes == 0, "failed statistics reset");
}
} // namespace safety_test
#endif
