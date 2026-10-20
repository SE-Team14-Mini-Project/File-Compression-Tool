# Archive serialization module — Issue #5

Owner: Debaditya Chakrabarti. Implements the shared `writeArchive` and
`readArchiveHeader` declarations in `src/archive.cpp` without codec/validator
implementation dependencies.

## Run independently (C++17)

```sh
g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Iinclude \
    src/archive.cpp tests/test_archive.cpp -o /tmp/team14-test-archive
/tmp/team14-test-archive
```

No shared header, archive-format, CMake, CI or other implementation changes are
required. Tests use explicit checks (not `assert`), so `NDEBUG` does not disable
them. Optional: add `-g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer`
when the compiler's matching sanitizer runtimes are installed.

## Contract and scope

The repository's `Docs/ARCHIVE_FORMAT.md` is authoritative for on-disk bytes:
`HUF1`, version 1, flags, zero reserved bytes, little-endian 64-bit sizes,
32-bit CRC field, 16-bit symbol count and ascending sparse byte/frequency pairs.
A native C++ struct is never dumped into the archive.

- Header reads are bounded to at most 256 pairs, work on non-seekable streams,
  and stop exactly before payload. Output metadata is reset on failure and
  populated only after a complete valid structural header.
- Reject wrong magic/version, unknown flags, nonzero reserved bytes, bad symbol
  counts/order/duplicates/zero frequencies, frequency overflow/sum mismatch,
  inconsistent empty/single-symbol metadata and truncated headers.
- Writes transport exactly `encodedBits / 8 + (encodedBits % 8 != 0)` bytes
  using a fixed 64 KiB buffer. Reject short/extra spool data and nonzero padding;
  check write/flush errors and convert standard stream exceptions into `Result`.
- The spool must be positioned at its first payload byte. Writing does not
  rewind it. A failed write can leave partial bytes in the supplied output;
  the controller owns close checks, rollback, temporary output and publication.
- Basic structural checks are not a substitute for `validateArchiveMetadata`,
  deterministic-Huffman/bitstream validation, decompression output limits,
  decoded CRC checking, or final archive EOF checks after decoding.
- CRC fields are transported, not computed or authenticated. Sprint 1 callers
  should set flags/CRC to zero unless end-to-end verification is implemented.
- `readArchiveHeader` deliberately accepts a header without payload or with
  extra bytes after payload: its contract is header parsing, not full archive
  verification. Decoder/controller must reject truncation and trailing bytes.

## Test evidence

The standalone suite covers the exact published `AAAAABBBCC` vector (59 bytes,
57-byte header, `07 F4` payload), empty (30 bytes), ten `A` bytes (41 bytes), all
256 symbols, 32/64-bit endianness, CRC transport, all 57 golden-header truncation
points, invalid headers, all final-byte alignments, short/extra/padded payloads,
non-seekable input, failing/exception-enabled streams, flush failure, a 256 KiB
binary spool, and 300 seeded two-symbol serialization round trips.
These are module tests, not full compression/decompression or CRC-integrity tests.

## Requirements clarification for review

Issue #5 and the older test-plan TC-05 mention a stored filename. Version 1 of
the agreed format explicitly stores **no filename or output path**. This module
follows the agreed format; no new field was added. Similarly CRC is optional by
flag, and the `.huf` file is not a standard ZIP archive. The team should reconcile
older requirement/test wording in a separately reviewed documentation change.
