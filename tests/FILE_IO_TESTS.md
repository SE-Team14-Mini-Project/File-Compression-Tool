# File I/O module — Issue #1

Owner: Debaditya Chakrabarti. Implements only the existing shared declarations
`validateInputFile` and `validateOutputFile` in `src/file_io.cpp`.

## Run independently (C++17)

From the repository root:

```sh
g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Iinclude \
    src/file_io.cpp tests/test_file_io.cpp -o /tmp/team14-test-file-io
/tmp/team14-test-file-io
```

The basic module test requires no codec, validation implementation, or external
test framework. Its checks remain active with `NDEBUG` defined.

## File I/O fault-injection tests

The separate `team14.file_io_faults` CTest target exercises rare preflight
failures deterministically, without changing the filesystem implementation used
by the production library or executable. It covers:

- Probe name collisions (with and without `file_exists`), successful retries,
  and exhaustion of the 16-attempt limit.
- Probe creation failure, cleanup failure and the cleanup guard's retry, and a
  failed binary write probe. It checks that no temporary probe remains.
- Input status permission and I/O errors, output parent status failure, a
  missing output filename, and a parent observed as a non-directory after
  the destination check (a simulated filesystem race).

Run the full CTest suite, or just the two file I/O targets, from the project
root (substitute the project's build directory if different):

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
ctest --test-dir build -R '^team14\.file_io(_faults)?$' --output-on-failure
```

The fault fixture can also be run independently with a C++17 compiler:

```sh
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude \
    tests/test_file_io_faults.cpp -o /tmp/team14-test-file-io-faults
/tmp/team14-test-file-io-faults
```

The fixture compiles a separate instrumentable copy of `src/file_io.cpp` using
a test-only adapter. When producing LLVM coverage, include this executable's
mapping and profile alongside the other instrumented targets, and investigate
any mapping/profile mismatch warnings. Recalculate coverage after the team's
changes are merged; the existing report is a historical baseline.

## Behavior and coverage

- Input must be an existing regular file that can actually be opened/read in
  binary mode. Empty files and all 256 byte values are supported. Input symlinks
  resolving to regular files are allowed.
- Reject empty/NUL-containing paths, missing input, directories and access errors.
- Existing output entries (including dangling links) return `OutputExists` by
  default. Explicit overwrite permits a regular writable file only; it never
  truncates or changes the existing file during validation.
- Output symlinks/non-regular files are rejected when overwrite is requested.
- The parent must already exist. An exclusively created temporary directory
  containing a one-byte binary probe checks real parent write access; the probe
  is removed. The chosen destination is never created by this function.
- Tests check binary preservation, spaces in filenames, absent/non-directory
  parents, permissions, symlinks and cleanup.
- Permission and symlink tests explicitly report skips on platforms/accounts
  that cannot exercise them. Run under an unprivileged user to test denials.

## Integration boundaries

These are **preflight checks, not reservations**. File-system state may change
between checking and use. Existing-file permission bits are advisory; ACLs,
read-only mounts, disk-full conditions and races can still affect later writes.
The controller must use binary streams and check open/read/write/flush/close,
write to a temporary output, validate, then publish safely without silently
replacing an existing destination. It must remove partial output on failure.

`validateOutputPath`, output-size/CRC checks and controller publication are owned
by other modules and are intentionally not implemented or modified here. Thus
this module alone does not complete Sprint-2 Issues #14/#21 or full CLI TC-01.
