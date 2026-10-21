// Standalone module tests: g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude
// src/file_io.cpp tests/test_file_io.cpp -o /tmp/team14-test-file-io
#include "interfaces.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace fs = std::filesystem;
using namespace team14;
namespace {
unsigned checks = 0;
void expect(bool condition, const char* name) {
    ++checks;
    if (!condition) throw std::runtime_error(name);
}
std::string bytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}
struct Fixture {
    fs::path root;
    Fixture() {
        auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (unsigned n = 0; n < 100; ++n) {
            root = fs::temp_directory_path() / ("team14-file-io-test-" + std::to_string(stamp) + "-" + std::to_string(n));
            if (fs::create_directory(root)) return;
        }
        throw std::runtime_error("Cannot create test directory");
    }
    ~Fixture() {
        std::error_code ec;
        fs::permissions(root, fs::perms::owner_all, ec);
        fs::permissions(root / "binary.dat", fs::perms::owner_all, ec);
        fs::remove_all(root, ec);
    }
};
}
int main() {
    try {
        Fixture f;
        const auto file = f.root / "binary.dat";
        std::string binary;
        for (unsigned n = 0; n < 256; ++n) binary += static_cast<char>(n);
        { std::ofstream out(file, std::ios::binary); out.write(binary.data(), binary.size()); }
        expect(validateInputFile(file).ok(), "readable binary input");
        expect(bytes(file) == binary, "all byte values preserved");
        const auto empty = f.root / "empty";
        { std::ofstream out(empty, std::ios::binary); }
        expect(validateInputFile(empty).ok(), "empty input");
        expect(validateInputFile(f.root / "missing").code == ErrorCode::InputNotFound, "missing input");
        expect(validateInputFile({}).code == ErrorCode::InvalidArguments, "empty input path");
        expect(validateInputFile(fs::path(std::string("a\0b", 3))).code == ErrorCode::InvalidArguments, "null input path");
        expect(validateInputFile(f.root).code == ErrorCode::InvalidArguments, "directory input");
        const auto output = f.root / "output archive.huf";
        expect(validateOutputFile(output, false).ok(), "new output accepted");
        expect(!fs::exists(output), "preflight does not create destination");
        expect(validateOutputFile(file, false).code == ErrorCode::OutputExists, "no silent overwrite");
        expect(validateOutputFile(file, true).ok(), "explicit overwrite preflight");
        expect(bytes(file) == binary, "overwrite check never truncates");
        expect(validateOutputFile({}, false).code == ErrorCode::InvalidArguments, "empty output path");
        expect(validateOutputFile(fs::path(std::string("a\0b", 3)), false).code == ErrorCode::InvalidArguments, "null output path");
        expect(!validateOutputFile(f.root / "missing-dir" / "out", false).ok(), "missing parent");
        expect(!validateOutputFile(file / "out", false).ok(), "file as parent");
        expect(!validateOutputFile(f.root, true).ok(), "directory output");
        std::error_code ec;
        fs::create_symlink(file, f.root / "link", ec);
        if (!ec) {
            expect(validateInputFile(f.root / "link").ok(), "input regular-file symlink");
            expect(validateOutputFile(f.root / "link", false).code == ErrorCode::OutputExists, "output link exists");
            expect(validateOutputFile(f.root / "link", true).code == ErrorCode::UnsafeOutputPath, "output link rejected");
            fs::create_symlink(f.root / "absent", f.root / "dangling");
            expect(validateOutputFile(f.root / "dangling", false).code == ErrorCode::OutputExists, "dangling link exists");
            expect(validateOutputFile(f.root / "dangling", true).code == ErrorCode::UnsafeOutputPath, "dangling output link rejected");
            expect(validateInputFile(f.root / "dangling").code == ErrorCode::InputNotFound, "dangling input missing");
        } else std::cout << "SKIP symlinks: unavailable on this platform\n";
        fs::permissions(file, fs::perms::none);
        std::ifstream denied(file, std::ios::binary);
        if (!denied.is_open()) {
            expect(validateInputFile(file).code == ErrorCode::PermissionDenied, "unreadable input");
        } else std::cout << "SKIP unreadable input: elevated permissions\n";
        expect(validateOutputFile(file, true).code == ErrorCode::PermissionDenied, "read-only output");
        denied.close();
        fs::permissions(file, fs::perms::owner_read | fs::perms::owner_write);
        fs::permissions(f.root, fs::perms::owner_read | fs::perms::owner_exec);
        // An elevated process may still create files despite permission bits.
        const auto permissionResult = validateOutputFile(output, false);
        if (!permissionResult.ok()) {
            expect(permissionResult.code == ErrorCode::PermissionDenied, "unwritable parent");
        } else std::cout << "SKIP unwritable parent: elevated permissions\n";
        fs::permissions(f.root, fs::perms::owner_all);
        for (const auto& entry : fs::directory_iterator(f.root)) {
            expect(entry.path().filename().string().find(".team14-io-check-") != 0, "no leaked probe");
        }
        expect(bytes(file) == binary, "input unchanged after all checks");
        std::cout << "PASS: " << checks << " file I/O checks\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
