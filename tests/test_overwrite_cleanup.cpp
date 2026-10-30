#include "output_safety_helpers.h"

using namespace team14;
using namespace safety_test;
namespace {
const std::string sentinel = "KEEP_EXISTING_BYTES_9F82";
struct PermissionsGuard {
    fs::path path;
    fs::perms original;
    explicit PermissionsGuard(fs::path p) : path(std::move(p)), original(fs::status(path).permissions()) {}
    ~PermissionsGuard() { std::error_code error; fs::permissions(path, original, error); }
};
}
int main() {
    try {
        Workspace workspace;
        const auto& root = workspace.root;
        const auto source = root / "source.bin";
        const auto archive = root / "good.huf";
        const auto destination = root / "existing.out";
        const std::string original(10, 'A');
        save(source, original);
        save(destination, sentinel);
        OperationStats stats{999, 888};
        auto result = compressFile(source, destination, stats);
        failed(result, stats);
        check(result.code == ErrorCode::OutputExists && load(destination) == sentinel, "compression preserves existing output bytes");
        check(load(source) == original, "source unchanged after refused overwrite");
        noTemporaryFiles(root);
        stats = {999, 888}; result = compressFile(source, source, stats);
        failed(result, stats); check(load(source) == original, "input cannot be overwritten through identical output path");

        result = compressFile(source, archive, stats);
        check(result.ok(), "create valid archive with real codec");
        const auto archiveBytes = load(archive);
        stats = {999, 888}; result = decompressFile(archive, destination, stats);
        failed(result, stats);
        check(result.code == ErrorCode::OutputExists && load(destination) == sentinel, "decompression preserves existing output bytes");
        stats = {999, 888}; result = decompressFile(archive, archive, stats);
        failed(result, stats); check(load(archive) == archiveBytes, "archive cannot overwrite itself");
        stats = {999, 888}; result = decompressFile(archive, source, stats);
        failed(result, stats); check(load(source) == original, "decompression preserves existing original file");

        // Directories and aliases count as occupied destinations too.
        const auto directory = root / "occupied-directory";
        fs::create_directory(directory); save(directory / "marker", sentinel);
        for (bool decompress : {false, true}) {
            stats = {999, 888};
            result = decompress ? decompressFile(archive, directory, stats) : compressFile(source, directory, stats);
            failed(result, stats); check(fs::is_directory(directory) && load(directory / "marker") == sentinel, "existing directory preserved");
        }
        std::error_code error;
        const auto hardAlias = root / "hard-alias";
        fs::create_hard_link(source, hardAlias, error);
        if (!error) {
            stats = {999, 888}; failed(compressFile(source, hardAlias, stats), stats);
            check(load(hardAlias) == original && load(source) == original, "hard-linked source alias preserved");
        } else std::cout << "SKIP hard-link alias: filesystem does not support fixture\n";

        const auto link = root / "output-link";
        error.clear(); fs::create_symlink(destination, link, error);
        if (!error) {
            for (bool decompress : {false, true}) {
                stats = {999, 888};
                result = decompress ? decompressFile(archive, link, stats) : compressFile(source, link, stats);
                failed(result, stats); check(fs::is_symlink(fs::symlink_status(link)) && load(destination) == sentinel, "output symlink and target preserved");
            }
            fs::remove(link);
            const auto absentTarget = root / "absent-target";
            fs::create_symlink(absentTarget, link);
            for (bool decompress : {false, true}) {
                stats = {999, 888};
                result = decompress ? decompressFile(archive, link, stats) : compressFile(source, link, stats);
                failed(result, stats);
                check(fs::is_symlink(fs::symlink_status(link)) && !fs::exists(absentTarget), "dangling destination link is not replaced or followed");
            }
            fs::remove(link);
        } else std::cout << "SKIP symlinks: platform/account does not allow fixture\n";
        noTemporaryFiles(root);

        // These errors happen after temporary output can already contain bytes.
        const auto badArchive = root / "bad.huf";
        const auto restored = root / "restored.bin";
        auto badCrc = archiveBytes; badCrc[24] ^= 1;
        auto badPadding = archiveBytes; badPadding.back() = static_cast<char>(static_cast<unsigned char>(badPadding.back()) | 1U);
        for (const auto& malformed : {badCrc, badPadding, archiveBytes.substr(0, archiveBytes.size()-1), archiveBytes + "trailing"}) {
            save(badArchive, malformed); stats = {999, 888}; result = decompressFile(badArchive, restored, stats);
            failed(result, stats); check(!fs::exists(restored), "failed restoration leaves no final partial file");
            check(load(badArchive) == malformed, "failed archive input preserved"); noTemporaryFiles(root);
            // The same destination remains usable after the failure.
            check(decompressFile(archive, restored, stats).ok() && load(restored) == original, "retry after failure publishes exact original bytes");
            fs::remove(restored); noTemporaryFiles(root);
        }
        // Parent permission denial: execute this suite unprivileged to exercise.
        const auto locked = root / "locked-parent";
        fs::create_directory(locked);
        {
            PermissionsGuard restore(locked);
            fs::permissions(locked, fs::perms::owner_read | fs::perms::owner_exec);
            bool bypass = false;
            { std::ofstream probe(locked / "probe", std::ios::binary); bypass = probe.is_open(); }
            if (!bypass) {
                for (bool decompress : {false, true}) {
                    const auto out = locked / (decompress ? "restored.bin" : "archive.huf");
                    stats = {999, 888};
                    result = decompress ? decompressFile(archive, out, stats) : compressFile(source, out, stats);
                    failed(result, stats); check(!fs::exists(out), "write access denial leaves no output");
                }
            } else { std::cout << "SKIP unwritable parent: account/OS bypasses permission bits\n"; fs::remove(locked / "probe"); }
        }
        noTemporaryFiles(root);
        check(load(source) == original && load(archive) == archiveBytes && load(destination) == sentinel, "all original and existing outputs remain unchanged");
        std::cout << "PASS: " << checks << " overwrite/cleanup checks\n";
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
