@'
# Lossless Round-Trip Restoration - Issue #7

## Objective

Verify that the Huffman compression tool restores the original input bytes
without loss and rejects malformed archives without publishing invalid output.

## Test coverage

### Successful round-trip cases

The test suite in `test_round_trip.cpp` covers:

- **Empty file:** compression and decompression preserve an empty input.
- **One-byte file:** verifies restoration of a single-symbol input.
- **Text file:** verifies byte-for-byte restoration of text containing newlines.
- **Binary file:** covers all 256 byte values, including zero and high-valued
  bytes, along with repeated bytes.

Each successful case checks input creation, compression success, archive
creation, decompression success, restored-byte equality, SHA-256 hash
equality between the original and restored data, and restored byte count.

The SHA-256 implementation is also checked against the standard
empty-input test vector.


### Malformed archive cases

The suite also checks that decompression rejects:

- **Truncated archive:** the final archive byte is removed.
- **Invalid padding:** a padding bit in the final payload byte is corrupted.
- **Trailing data:** an extra byte is appended after a valid archive.

For each malformed case, the test checks that decompression fails and that
the destination output file is not published.

The padding test is conditional on the fixture having unused bits in its
final payload byte.

## Build and run

Run the following commands from the repository root in PowerShell with
a C++17-compatible compiler:

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude `
    tests\test_round_trip.cpp `
    src\controller.cpp `
    src\archive.cpp `
    src\file_io.cpp `
    src\huffman.cpp `
    src\validation.cpp `
    -o build\test_round_trip.exe

.\build\test_round_trip.exe
```

## Recorded verification

The recorded verification covers:
- SHA-256 known-answer tests for empty input and the standard `abc` test vector.
- Successful lossless round trips for empty, one-byte, text, and binary files.
- Original and restored byte equality, SHA-256 hash equality, and restored byte count.
- Rejection of truncated archives, invalid padding, and trailing data, with no output published for rejected archives.

## Scope and limitations

These tests do not establish exhaustive malformed-input coverage, CRC32 integrity verification, or performance against the SRS target.

See [ARCHIVE_FORMAT.md](../Docs/ARCHIVE_FORMAT.md) for the archive format and its structural rules.