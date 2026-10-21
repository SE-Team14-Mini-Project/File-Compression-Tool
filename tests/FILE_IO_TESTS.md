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

No codec, validation implementation, CMake change, or external test framework
is required. The checks remain active with `NDEBUG` defined.

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
