# Archive information — Issue #12 (FR-INFO-01 / TC-05)

`huf info <archive>` reports HUF1/version/flags, original size, total archive,
header and packed payload sizes, meaningful encoded bits, symbol count and
ascending byte/frequency pairs. The pairs are the stored tree reconstruction
metadata, not a native serialized Huffman tree.

Version 1 stores **no original filename or output path**. The command states
this explicitly rather than inventing metadata or changing the shared format.
This is the already-identified deviation from the older SRS/issue wording and
requires team documentation approval, not a unilateral format change.

The command opens the source read-only, validates header structure and metadata,
and checks that file length exactly matches header + ceil(encodedBits/8).
Metadata is returned only after all checks succeed. No decoding or output file
creation occurs, and the decompression-only output-size limit is not applied to
this read-only inspection. It can inspect metadata for large declared outputs
without allocating or decoding that data.

A displayed CRC is only its **stored value**. Info explicitly says it does not
verify it; matching lengths and valid metadata do not prove payload integrity.
Use decompression for full bitstream and CRC validation. Checksum-absent legacy
archives remain inspectable. Malformed/truncated/trailing-byte archives fail
with a nonzero CLI status and no success metadata.

Tests: `team14.archive_info` covers valid/empty/legacy/CRC metadata, read-only
behavior, invalid headers/lengths, reset-on-failure and sparse large metadata.
`team14.archive_info_cli` checks the exact published vector fields, unchanged
archive SHA-256, no new directory entries and failure output. Run them through
CTest; neither test changes interfaces.h or ARCHIVE_FORMAT.md.
