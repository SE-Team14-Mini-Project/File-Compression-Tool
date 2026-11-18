# Building and testing — Issue #19 / NFR-PORT-01

The root `CMakeLists.txt` builds the existing C++ modules and `huf` CLI without
changing their shared interfaces or archive format. No third-party C++ library
or downloaded dependency is required by this build.

## Prerequisites

- CMake **3.20 or newer**.
- A compiler and standard library with C++17 support, including
  `std::filesystem`. Recommended starting points: GCC 9+, Clang 10+ with a
  corresponding modern standard library, or Visual Studio 2019/2022 C++ tools.
  The installed standard library matters as well as the compiler version.
- A CMake-supported native build tool: Make, Ninja, or MSBuild.

Install these through your OS/package manager. On Windows, install the
**Desktop development with C++** workload and use a Visual Studio Developer
PowerShell/Command Prompt. On macOS, install the Xcode Command Line Tools and
CMake. On Linux, install a C++ compiler, CMake and Make or Ninja.

CMake requires C++17 with compiler extensions disabled; it does not silently
fall back to an older language standard. This configuration targets modern
standard libraries; older GCC/libstdc++ combinations that require a separate
`stdc++fs` library are not part of the documented baseline.

## Linux/macOS: configure, build and test

Run from the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/huf help
```

For a Release build, use a separate build directory:

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --parallel
ctest --test-dir build/release --output-on-failure
```

Choose another compiler only when configuring a **new** build directory:

```sh
cmake -S . -B build/clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug
cmake --build build/clang --parallel
ctest --test-dir build/clang --output-on-failure
```

If Ninja is installed, add `-G Ninja` at the initial configure step. Otherwise
CMake chooses the platform default generator. Do not switch compiler/generator
inside an already-configured build directory.

## Windows: Visual Studio multi-configuration build

From a Visual Studio Developer shell at the repository root:

```powershell
cmake -S . -B build
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
.\build\Debug\huf.exe help
```

For Release, replace `Debug` with `Release` in the build, test and executable
commands. Visual Studio generators select configurations with `--config` / `-C`,
not `CMAKE_BUILD_TYPE`. Ninja/Make generators instead use `CMAKE_BUILD_TYPE` and
normally place the executable directly in the chosen build directory.

## Targets and tests

- `team14_core`: static library containing archive, controller, file I/O,
  Huffman and validation implementations.
- `huf`: CLI executable linked to that library.
- Four independent test executables, registered as `team14.frequency`,
  `team14.file_io`, `team14.archive`, and `team14.validation`.
- `team14.cli_help`: a CLI startup/help smoke test.

`BUILD_TESTING` is ON by default via CTest. To build just the library and CLI:

```sh
cmake -S . -B build/no-tests -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/no-tests --parallel
```

For detailed test output or parallel execution:

```sh
ctest --test-dir build -V
ctest --test-dir build --parallel 4 --output-on-failure
```

Tests have separate working/temp directories within each build tree. Assertions
remain enabled in the test targets in Release configurations so the existing
assert-based frequency checks are not silently disabled. Permission and symlink
checks may explicitly skip cases when the account/OS cannot exercise them;
inspect verbose output rather than interpreting a skip as a passed denial test.

Warnings use `/W4 /permissive- /utf-8` with MSVC and
`-Wall -Wextra -Wpedantic` with GCC/Clang. Warnings are not globally promoted to
errors, so a compiler upgrade does not automatically make the build unusable.
The build does not add OS-specific APIs or change implementation behavior.

## CLI example

After building, use the executable path appropriate to your generator. For
example, from the repository root on Linux/macOS:

```sh
./build/huf compress input.txt example.huf
./build/huf info example.huf
./build/huf decompress example.huf restored.txt
```

Input files must exist, destination parent directories must exist, and output
filenames must not already exist. Compare the original and restored bytes with
an OS-appropriate comparison tool. Archives use the custom format defined in
[`ARCHIVE_FORMAT.md`](ARCHIVE_FORMAT.md), not standard ZIP/DEFLATE.

The current controller publishes completed files using standard C++ filesystem
hard links. Runtime operations therefore require a filesystem supporting hard
links; portable compilation alone does not guarantee identical filesystem
capabilities. Report such runtime limitations to the controller owner rather
than changing implementation code as part of this build issue.

## Validation scope

Build-01 / ST-12 verifies configuration, compilation/linking, documented commands
and test discovery/execution. Passing these tests is not certification of every
SRS requirement, all security cases, the 10 MB performance target, or every OS.
Platform verification results and exact toolchain versions belong in the PR's
test evidence. Windows/macOS commands are provided, but should not be called
verified until run on those systems.

The existing GitHub Actions workflow is intentionally unchanged by this issue;
it still checks the shared header only. The commands above run the full build
and registered tests locally and are suitable for a separately reviewed CI
extension by the workflow owners.
