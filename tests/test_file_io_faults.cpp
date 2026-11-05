// Deterministic filesystem fault injection for the file I/O preflight checks.
// This executable compiles the real implementation with a filesystem adapter;
// the production library and executable are unaffected.
#include "interfaces.h"
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <system_error>

namespace io_fault_fs {
using namespace std::filesystem;
enum class Fault { none, collision_empty, collision_exists, collision_always,
                   create_denied, remove_denied, block_check, status_denied,
                   status_io, parent_is_file };
Fault fault = Fault::none;
unsigned creates = 0;
unsigned removes = 0;

bool create_directory(const path& p, std::error_code& ec) {
    ++creates;
    if ((fault == Fault::collision_empty && creates == 1) ||
        fault == Fault::collision_always) {
        ec.clear();
        return false;
    }
    if (fault == Fault::collision_exists && creates == 1) {
        ec = std::make_error_code(std::errc::file_exists);
        return false;
    }
    if (fault == Fault::create_denied) {
        ec = std::make_error_code(std::errc::permission_denied);
        return false;
    }
    const bool created = std::filesystem::create_directory(p, ec);
    if (created && fault == Fault::block_check) {
        // Opening a directory as an output file fails, even with elevated privileges.
        std::filesystem::create_directory(p / "check");
    }
    return created;
}
std::uintmax_t remove_all(const path& p, std::error_code& ec) {
    ++removes;
    if (fault == Fault::remove_denied && removes == 1) {
        ec = std::make_error_code(std::errc::permission_denied);
        return static_cast<std::uintmax_t>(-1);
    }
    return std::filesystem::remove_all(p, ec);
}
file_status status(const path& p, std::error_code& ec) {
    if (fault == Fault::status_denied || fault == Fault::status_io) {
        ec = std::make_error_code(fault == Fault::status_denied
                                  ? std::errc::permission_denied : std::errc::io_error);
        return file_status(file_type::none);
    }
    return std::filesystem::status(p, ec);
}
file_status symlink_status(const path& p, std::error_code& ec) {
    if (fault == Fault::parent_is_file) {
        // Simulate a destination that did not exist at the initial check;
        // status() then observes its parent as a regular file.
        ec = std::make_error_code(std::errc::no_such_file_or_directory);
        return file_status(file_type::not_found);
    }
    return std::filesystem::symlink_status(p, ec);
}
}
#define TEAM14_FILE_IO_FILESYSTEM io_fault_fs
#include "../src/file_io.cpp"
#undef TEAM14_FILE_IO_FILESYSTEM

namespace fs = std::filesystem;
using team14::ErrorCode;
namespace {
unsigned checks = 0;
void expect(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
void reset(io_fault_fs::Fault f) {
    io_fault_fs::fault = f;
    io_fault_fs::creates = io_fault_fs::removes = 0;
}
struct Fixture {
    fs::path root;
    Fixture() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (unsigned n = 0; n < 100; ++n) {
            root = fs::temp_directory_path() / ("team14-io-fault-" + std::to_string(stamp) + "-" + std::to_string(n));
            std::error_code ec;
            if (fs::create_directory(root, ec)) return;
        }
        throw std::runtime_error("Could not create test directory");
    }
    ~Fixture() { std::error_code ec; fs::remove_all(root, ec); }
};
void expect_no_probe(const fs::path& root) {
    for (const auto& entry : fs::directory_iterator(root)) {
        expect(entry.path().filename().string().find(".team14-io-check-") != 0,
               "temporary probe directory leaked");
    }
}
}
int main() {
    try {
        Fixture f;
        const auto output = f.root / "output.huf";
        const auto input = f.root / "input";
        { std::ofstream out(input); out << "data"; }
        using io_fault_fs::Fault;
        for (auto fault : {Fault::collision_empty, Fault::collision_exists}) {
            reset(fault);
            expect(team14::validateOutputFile(output, false).ok(), "retry after collision");
            expect(io_fault_fs::creates == 2, "retry attempts counted");
            expect_no_probe(f.root);
        }
        reset(Fault::collision_always);
        auto result = team14::validateOutputFile(output, false);
        expect(result.code == ErrorCode::IoError && result.message == "Could not create a unique output access check.", "retry exhaustion");
        expect(io_fault_fs::creates == 16, "retry limit");
        expect_no_probe(f.root);

        reset(Fault::create_denied);
        expect(team14::validateOutputFile(output, false).code == ErrorCode::PermissionDenied, "probe creation access failure");
        expect_no_probe(f.root);

        reset(Fault::remove_denied);
        result = team14::validateOutputFile(output, false);
        expect(result.code == ErrorCode::PermissionDenied && result.message == "Could not clean up the output access check.", "probe cleanup failure");
        expect(io_fault_fs::removes == 2, "cleanup guard retries removal");
        expect_no_probe(f.root);

        reset(Fault::block_check);
        result = team14::validateOutputFile(output, false);
        expect(result.code == ErrorCode::IoError && result.message == "Cannot write a binary file in the destination directory.", "probe write failure");
        expect_no_probe(f.root);

        reset(Fault::status_denied);
        expect(team14::validateInputFile(input).code == ErrorCode::PermissionDenied, "input status permission error");
        reset(Fault::status_io);
        expect(team14::validateInputFile(input).code == ErrorCode::IoError, "input status I/O error");
        reset(Fault::status_denied);
        result = team14::validateOutputFile(output, false);
        expect(result.code == ErrorCode::PermissionDenied && result.message == "Cannot access output directory.", "output parent status failure");

        reset(Fault::none);
        result = team14::validateOutputFile(f.root / "missing" / "", false);
        expect(result.code == ErrorCode::InvalidArguments && result.message == "Output must include a filename.", "output requires a filename");
        reset(Fault::parent_is_file);
        result = team14::validateOutputFile(input / "output", false);
        expect(result.code == ErrorCode::IoError && result.message == "Output directory does not exist or is not a directory.", "parent is not a directory");
        expect_no_probe(f.root);
        expect(!fs::exists(output), "preflight never creates output");
        std::cout << "PASS: " << checks << " file I/O fault checks\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
