# File Compression Tool — Team 14

**PES University | Software Engineering Mini-Project | C++17**

A planned command-line, **lossless file compressor and decompressor** built around Huffman coding. This is a **custom ZIP-style educational utility**, not a standard `.zip` / DEFLATE implementation. The archive extension is `.huf`, and version 1 of our format is defined in [`docs/ARCHIVE_FORMAT.md`](docs/ARCHIVE_FORMAT.md).

> **Project status:** Design and backlog prepared. `include/interfaces.h` contains declarations only. The compressor/decompressor, tests, and CLI will be implemented during the planned sprints. This repository should not yet be described as a working compression tool.

## What we are building

- Compress one local file using Huffman coding and save a `.huf` archive.
- Decompress a valid archive and restore the **exact original bytes**.
- Inspect archive metadata and report compression statistics.
- Reject invalid archives and handle file errors safely.
- Test correctness, security, usability and performance according to the SRS and Test Plan.

**Out of scope for v1:** compatibility with standard ZIP programs, graphical interface, networking/cloud services, and multi-file archives.

## Team and responsibilities

| Member | Role | Main responsibility |
|---|---|---|
| **Divisha Gupta** | Algorithm / Development Lead | Byte-frequency analysis, deterministic Huffman tree, code generation, bit packing, decoding |
| **Debaditya Chakrabarti** | Archive & File I/O Lead | Reading/writing files, `.huf` headers, sparse frequency pairs and payload |
| **Greeshma N** | Architecture / Integration Lead | CLI, orchestration, integration and user-facing errors |
| **Chandrakanth V G** | Testing & Security Lead | Validation, integrity/security checks and tests |

Responsibilities describe component ownership; everybody reviews integration changes and contributes to testing.

## Shared development contract — read before coding

**The two files below must be reviewed together:**

1. [`include/interfaces.h`](include/interfaces.h) — shared **C++ declarations**: types, function signatures, result/error codes, statistics and module hand-offs. It does **not** contain implementations.
2. [`docs/ARCHIVE_FORMAT.md`](docs/ARCHIVE_FORMAT.md) — precise `.huf` **binary file format**: byte offsets, endianness, sparse frequencies, tree tie-breaking, payload bits, padding, CRC32 flags, edge cases, and validation rules.

### Why both files matter

All team members include **the same tracked header** in whichever `.cpp` files use its declarations. Do **not** maintain separate personalized copies. Code can compile against the header only when its implementations match the signatures, and all generated archives can be decoded only when the writer and reader agree on the exact format.

**We use `std::` names explicitly inside the shared header** (for example `std::filesystem::path`). Avoid `using namespace std;` in headers. In `.cpp` files, local aliases are optional.

### Our agreed Huffman conventions

| Topic | Convention |
|---|---|
| Input symbols | Individual bytes `0`–`255` |
| Frequency storage | Sparse list of **only present** symbols: `(1-byte symbol, 8-byte frequency)` |
| Tie-breaking | Sort nodes by `(frequency, smallest byte value in subtree)` |
| Tree edges | First extracted node goes left (`0`), second goes right (`1`) |
| Bit packing | Most-significant bit first inside each output byte |
| Final padding | Add zero bits up to a full byte; record **exact meaningful-bit count** |
| Integer encoding | Little-endian, serialized **field by field** |
| Empty input | No symbols and no payload |
| One unique symbol | Assign code `0` |
| CRC32 | Flag-controlled; may be absent in the Sprint 1 prototype, with verification added in Sprint 2 |

The decompressor reconstructs the same tree from the stored frequencies. The two sides **must** apply the same tie-breaking rule; otherwise equal-frequency nodes can produce different codes. `originalBytes` and `encodedBits` are both checked, rather than trusting padding to mark the end of data.

## Proposed architecture

```text
User / CLI
     |
     v
Compression Controller <----> Validation / error handling
     |
     +----> File I/O
     |
     +----> Huffman Codec (frequency -> tree -> codes -> bits)
     |
     +----> Archive Manager (header + sparse frequencies + packed payload)
```

**Compression:** Read the source file → count frequencies → build tree and codes → pack encoded bits → write metadata and payload into a `.huf` archive.

**Decompression:** Read and validate the header → load sparse frequencies → rebuild the same tree → decode the meaningful bits → validate output and publish the restored file.

The codec should **not** write archive headers; archive storage belongs to the Archive Manager. The controller coordinates the components.

## Planned repository layout

```text
SE-Mini-project-14/
├── README.md
├── include/
│   └── interfaces.h             # One shared contract for all components
├── docs/
│   └── ARCHIVE_FORMAT.md        # Detailed version-1 file format
├── src/                         # To be added during implementation
│   ├── huffman.cpp              # Divisha
│   ├── archive.cpp              # Debaditya
│   ├── file_io.cpp              # Debaditya
│   ├── controller.cpp           # Greeshma
│   ├── validation.cpp           # Chandrakanth
│   └── main.cpp                 # CLI entry point
├── tests/                       # To be added during implementation
└── CMakeLists.txt               # To be added during implementation
```

The `src/`, `tests/` and `CMakeLists.txt` entries above are a **proposed structure**, not a claim that they already exist. Agree on changes to filenames/layout before implementing imports and build configuration.

## Planned CLI (not implemented yet)

```sh
# Command forms in the SRS; executable name will be finalized with the build
compress input.txt output.huf
decompress output.huf restored.txt
info output.huf
```

The CLI will report errors and nonzero exit codes on failure. **Build and run commands will be documented when the executable and build system exist.**

## Sprint plan

| Iteration | Scheduled dates | Goal |
|---|---|---|
| **Sprint 1** | Oct 17–21, 2026 | Working single-file compression/decompression MVP, basic archive and CLI, lossless round-trip verification |
| **Sprint 2** | Oct 22–26, 2026 | Remaining functionality, robust validations, CRC32 verification, security hardening, performance benchmarks and testing |



## How to work

1. **Read** the shared header and archive specification before implementing your module.
2. **Agree on the contract** in a team review; any proposal to modify it should be reviewed by affected module owners.
3. Implement your module in a separate `.cpp` file using the declarations in `include/interfaces.h`.
4. Add tests for your module, including **empty files, repeated bytes, tied frequencies, and binary data** as applicable.
5. Create a feature branch and pull request (PR); request a review before merging to the default branch.
6. Greeshma integrates the modules, runs the CLI end-to-end, and coordinates fixes with owners.
7. Update the corresponding GitHub issue status only when work actually starts or finishes.

Compile source files with `-Iinclude` so `#include "interfaces.h"` resolves. The exact CMake/build command will be supplied after the build system is added.

## Acceptance and testing focus

- **Round trip:** output after decompression is byte-for-byte identical to the input.
- **Format:** empty file, one-symbol file, ties, invalid version, malformed header, wrong symbol count, truncation, invalid padding or trailing bytes are handled correctly.
- **Safety:** no silent overwrite, invalid/partial output is not accepted as successful, output-size limits are enforced.
- **Integrity:** when the CRC32 flag is set, restored bytes must pass CRC32/IEEE verification. Files without CRC32 must not be claimed checksum-verified.
- **Performance:** the SRS specifies a **10 MB input within 5 seconds per operation on the reference machine**; measure and report results rather than assuming the target is met.

These are **planned acceptance expectations**, not completed test results.

## Project documents and links

- [Repository](https://github.com/SE-Team14-Mini-Project/File-Compression-Tool)
- [GitHub Project / sprint backlog](https://github.com/orgs/SE-Team14-Mini-Project/projects/1)
- Part-1 deliverables: **SRS**, **Software Test Plan**, and **Software Architecture and Design (SAD)** — consult the versions committed to this repository.
- [`docs/ARCHIVE_FORMAT.md`](docs/ARCHIVE_FORMAT.md) for exact on-disk details and the worked `AAAAABBBCC` test vector.

**Change control:** once the team approves the interface/archive baseline, do not change a signature, field offset, encoding rule, or error convention silently. Update both contract documents and relevant tests in the same reviewed PR.
