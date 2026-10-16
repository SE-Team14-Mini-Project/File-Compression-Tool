# Team 14 — Huffman archive and interface contract

**Version:** 1.0 (proposed team baseline, 15 Oct 2026)  
**Scope:** C++17 single-file, lossless, local CLI compression. The `.huf` file is **not a standard ZIP archive**.  
**Document priority:** The SRS states *what* must be supported; the SAD defines modules; this document fixes interoperable **how** decisions. Any change must be agreed by all owners and updated in both code and tests.

## 1. Binary archive layout (version 1)

Multi-byte integers are unsigned **little-endian**, serialized field-by-field (never dump native C++ structs). No filename or output path is stored in version 1; the user supplies the destination on the CLI.

| Byte offset | Bytes | Field | Rule |
|---|---:|---|---|
| 0 | 4 | magic | ASCII `HUF1` |
| 4 | 1 | version | `1` |
| 5 | 1 | flags | bit 0 = CRC32 present; bits 1–7 must be zero |
| 6 | 2 | reserved | zero; reject nonzero |
| 8 | 8 | originalBytes | exact number of uncompressed bytes |
| 16 | 8 | encodedBits | number of meaningful Huffman bits, excluding padding |
| 24 | 4 | crc32 | CRC of **uncompressed bytes** if flag set; otherwise zero |
| 28 | 2 | symbolCount | integer 0..256 |
| 30 | 9 × symbolCount | frequency pairs | one byte `symbol`, then uint64 `frequency`; symbols strictly ascending; nonzero frequency |
| 30 + 9 × symbolCount | ceil(encodedBits / 8) | compressed payload | MSB-first packed Huffman bits, trailing unused bits zero |

Header size is **30 + 9 × symbolCount** bytes; e.g. 3 symbols = 57-byte header. For files containing all 256 bytes, the sparse table is larger than the earlier 2,048-byte fixed table; simplicity and sparse-file savings justify this tradeoff. An archive ends precisely after the encoded payload: reject extra bytes and truncated content.

## 2. Deterministic Huffman algorithm

1. Operate on **byte values 0..255**, not language characters.
2. Ignore symbols whose frequency is zero when building the min-priority queue.
3. Rank each node by the pair **(frequency, minSymbol)**, smallest first. For a leaf `minSymbol = symbol`; for an internal node `minSymbol = min(left.minSymbol, right.minSymbol)`.
4. Remove the two smallest nodes as `first` and `second`. `first` becomes **left** (`0`); `second` becomes **right** (`1`). Merge with frequency sum and minSymbol minimum. Enforce overflow safety.
5. Distinct live subtrees have disjoint byte sets, so their `minSymbol` values cannot tie. A third tie-breaker or creation-order field is **not required**.
6. Encode the original bytes using root-to-leaf binary paths. Pack meaningful bits **most-significant bit first** (first bit in bit 7).
7. Empty input: symbolCount = 0, originalBytes = 0, encodedBits = 0, no payload, crc32 = 0 unless CRC enabled (CRC32 of empty data is also zero).
8. Single-symbol input: code is **`0` per byte**, so encodedBits equals originalBytes; the decoder checks all meaningful bits are `0` and writes the declared count.
9. Decoding must emit **exactly `originalBytes` bytes**, consume **exactly `encodedBits`** meaningful bits, reject invalid traversals/truncation and nonzero padding, and verify no trailing payload bytes. Do not merely stop early and ignore malformed data.
10. The sum of stored frequencies must equal originalBytes; reject overflow, duplicate/out-of-order symbols, zero stored frequencies, or impossible counts.

## 3. Integrity, safety, and the two sprints

**CRC32 is explicitly optional per archive**, controlled by the flags byte. That keeps the version-1 format stable across both sprints:
- **Sprint 1 MVP:** writer sets `flags = 0` and `crc32 = 0` unless CRC32 is already implemented and checked end-to-end. The CLI must not claim checksum integrity verification for these archives. Always validate basic header/size/bitstream correctness and produce byte-identical round trips.
- **Sprint 2:** support and preferably emit `flags = 1`; CRC32/ISO-HDLC (also called CRC-32/IEEE), reflected polynomial `0xEDB88320`, initial value `0xFFFFFFFF`, final xor `0xFFFFFFFF`, computed over original uncompressed bytes. During decompression recompute over restored bytes and compare **before accepting output as trusted**. Archives with `flags = 0` remain readable, with an explicit 'checksum absent' status.
- CRC32 detects common accidental corruption. It **does not** provide encryption, authenticity or protection against a deliberate attacker changing the checksum too.
- Validate declared output sizes against a configured limit **before allocation/writing** and enforce the same limit **during decoding**. Suggested default 1 GiB, adjustable by the controller for testing and product use; this default is a design choice, not a formal SRS number.
- Never overwrite a target silently. Write to a temporary file in the destination directory, verify success and integrity, then safely publish to the chosen output path; delete incomplete temporary output on failure. Do not claim full fail-closed/security compliance until Sprint 2 tests pass.
- Never trust archive-provided paths. Version 1 stores **no filename**, so there is no archive-controlled extraction path. If added in the future, reject absolute and `..` traversal paths.
- Avoid logging full input/output contents. Return nonzero exit codes for errors.

## 4. Module hand-offs

| Owner | Module | Inputs/outputs |
|---|---|---|
| Divisha | Huffman codec | `FrequencyTable`, `HuffmanTree`, `CodeTable`; `encode` produces packed bits + exact `EncodedStats`; `decode` consumes packed bits and produces uncompressed bytes |
| Debaditya | File I/O + Archive Manager | Writes/reads the version-1 header, sparse frequency pairs, and `ceil(encodedBits/8)` payload bytes; returns stream positioned at first payload byte |
| Greeshma | Controller / CLI | Coordinates counting, tree building, encoding, metadata, archive write, read/validate/decode, output publication, and errors |
| Chandrakanth | Validation/tests | Validates metadata/bounds/paths/checksum; tests both sprint stages, including corruption and round-trip recovery |

**Streaming convention:** Because Huffman frequencies require a first pass and metadata precedes payload, the controller may rewind the source stream (for regular files), and write compressed bits to a temporary binary spool before calling `writeArchive`. `encode` returns `EncodedStats.meaningfulBits`; this becomes `metadata.encodedBits`. No codec implementation should write archive headers directly.

## 5. Minimum cross-team test vectors

- `AAAAABBBCC`: A=5, B=3, C=2. Merge C(2)+B(3), then A(5)+node(5) ordered by minSymbol. Codes: A=`0`, C=`10`, B=`11`; meaningful stream `000001111111010` (15 bits); packed bytes `00000111 11110100` (hex `07 F4`). Header: `symbolCount=3`, `originalBytes=10`, `encodedBits=15`; version 1 with flags 0 gives 57-byte header and 2-byte payload = **59-byte archive**.
- Empty input: output archive with 30-byte header, 0 payload bytes.
- Ten `A` bytes: single-symbol tree, ten `0` bits -> hex `00 00` (six padding zeros); 39-byte header and 2-byte payload = **41 bytes**.
- Corruption tests: wrong magic/version, reserved or unknown flags, bad count or duplicate symbol, frequency-sum mismatch, truncated bitstream, excess trailing bytes, and flipped nonzero padding bits.
- Round trip: compare exact original/restored bytes, not just file length. Benchmark the 10 MB test from the SRS in the appropriate sprint.

## 6. Approval and change control

Before individual module implementation, all four contributors should agree on this **exact** layout, bit order, node comparator, signatures, and error/partial-output behavior. Any subsequent format change must be documented and tested; don't silently change a header field or function signature in one module. Keep backlog issue statuses `Todo` until actual work starts.
