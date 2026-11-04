# Team 14 — Updated Test Case Document

**Project:** Huffman File Compression Tool
**Version:** 1.0
**Updated:** 4 November 2026
**Sprint:** Part 3 — Testing and Validation

## 1. Objective

Document the test cases for the current implementation, incorporating
Sprint 2 changes and the version-1 archive format contract.

The original test case IDs are retained for traceability. Test status
reflects available test evidence and the latest CTest execution.

## 2. Test environment

- Language: C++17
- Build system: CMake
- Test framework: CTest
- Coverage tooling: LLVM llvm-cov and llvm-profdata
- Platform used: macOS
- Latest recorded execution: 17 tests passed, 0 failed.

## 3. Updated test cases

| ID | Test case | Requirement / area | Test procedure and expected result | Evidence | Status |
|---|---|---|---|---|---|
| TC-01 | Input and output path validation | FR-CMP-01, FR-OUT-01 | Test readable input, missing files, directories, invalid paths and existing destinations. Valid inputs should be accepted; invalid paths should be rejected. | test_file_io.cpp, test_controller.cpp, test_round_trip.cpp | PASS |
| TC-02 | Frequency analysis | FR-CMP-02 | Test repeated and distinct byte values. Frequency counts should represent the input bytes correctly. | test_frequency.cpp | PASS |
| TC-03 | Huffman tree and code generation | FR-CMP-03 | Test Huffman construction and encoding behaviour, including single-symbol and multiple-symbol inputs. | test_codec.cpp | PASS |
| TC-04 | Bitstream encoding and decoding | FR-CMP-04 | Test bit packing, reading and decoding, including partial final bytes and padding behaviour. | test_bit_io.cpp, test_codec.cpp, test_round_trip.cpp | PASS |
| TC-05 | Archive metadata and information | FR-CMP-05, FR-INFO-01 | Run huf info on valid archives. Verify HUF1 version, flags, sizes, header and payload lengths, encoded bits, symbol count and frequency pairs. The command must not invent a filename because v1 stores none. | test_archive_info.cpp, test_archive_info_cli.cmake | PASS |
| TC-06 | Compression statistics | FR-CMP-06 | Verify original size, archive size and compression statistics for representative inputs. Check that empty input does not cause divide-by-zero. | Controller and archive information tests; no separate complete statistics acceptance evidence recorded. | PARTIAL |
| TC-07 | Lossless round trip | FR-CMP-07, FR-DEC-02 | Compress and decompress empty, one-byte, text and binary files. Compare restored bytes and SHA-256 hashes with the original. | test_round_trip.cpp | PASS |
| TC-08 | Malformed and tampered archive rejection | FR-DEC-01, FR-DEC-03, FR-ERR-01, security | Test invalid headers, truncated archives, invalid padding and trailing bytes. Decompression must fail and must not publish invalid output. CRC field transport alone is not CRC integrity verification. | test_archive.cpp, test_round_trip.cpp, test_validation.cpp | PARTIAL |
| TC-09 | Output path and overwrite safety | FR-DEC-04, FR-OUT-01, safe output handling | Test existing destinations, overwrite rules, invalid output paths, cleanup and publication failures. Version 1 contains no archive filename or extraction path, so archive-based path traversal is not applicable. | test_file_io.cpp, test_overwrite_cleanup.cpp, test_no_overwrite_races.cpp, test_overwrite_cli.cmake, test_output_publication_faults.cpp | PASS |
| TC-10 | 10 MB performance | NFR-PERF-01 | Compress and decompress the 10 MB reference file. Record timings and verify restored bytes. The planned target is at most 5 seconds per operation. | No measured acceptance result recorded. | NOT VERIFIED |

## 4. Additional Sprint 2 regression and validation tests

| Test area | Test source / CTest coverage | Expected behaviour |
|---|---|---|
| Archive serialization | test_archive.cpp | Correct version-1 header and payload serialization; reject malformed metadata and invalid stream data. |
| Archive information CLI | test_archive_info_cli.cmake | Correct metadata output; malformed archives fail without success output. |
| Controller integration | test_controller.cpp | Correct coordination of compression, decompression and error handling. |
| File I/O | test_file_io.cpp | Validate input/output paths and filesystem conditions. |
| Overwrite cleanup | test_overwrite_cleanup.cpp | Failed operations clean up incomplete output. |
| No-overwrite races | test_no_overwrite_races.cpp | Prevent unsafe replacement of an existing destination. |
| Output publication faults | test_output_publication_faults.cpp | Handle publication failures and exceptions without leaking internal exception details. |
| Output limits | test_output_limits.cpp | Enforce configured output-size restrictions. |
| CLI overwrite | test_overwrite_cli.cmake | Verify command-line overwrite behaviour. |
| Security logging | test_security_logging.cpp | Ensure diagnostics do not expose private file contents or internal exception details. |
| CLI behaviour | test_cli.cmake | Verify command-line interface behaviour and error handling. |
| Validation | test_validation.cpp | Reject invalid archive metadata and inputs. |

## 5. Execution summary

Latest recorded CTest execution:

- Registered tests: 17
- Passed: 17
- Failed: 0
- Result: 100% of registered CTest tests passed.

This result applies to the registered automated test suite. It does not
mean every original SRS acceptance criterion has been independently
verified.

## 6. Coverage and outstanding verification

The latest production-only LLVM coverage results are:

- Line coverage: 88.71%
- Region coverage: 91.16%
- Function coverage: 100.00%
- Branch coverage: 82.58%

Outstanding or limited verification:

1. TC-06: Complete compression-statistics acceptance evidence is not
   independently recorded.
2. TC-08: CRC field transport and malformed archive rejection do not
   establish end-to-end CRC32 integrity verification. Confirm CRC-enabled
   decompression behaviour separately.
3. TC-10: The 10 MB performance target has not been verified with recorded
   timings.
4. Platform-dependent permission and symlink tests may be skipped when the
   operating system or account cannot exercise those conditions.
5. LLVM reported one function with mismatched coverage data. This warning
   is recorded in the coverage reports.

## 7. Archive format clarifications

The version-1 HUF archive:

- Is a custom format, not a standard ZIP archive.
- Stores no original filename or output path.
- Uses a sparse frequency table to reconstruct Huffman metadata.
- Supports an optional CRC32 field controlled by the flags byte.
- Requires exact payload length and rejects malformed or trailing data.

Accordingly, TC-05 no longer expects a stored filename, and TC-09 tests
CLI destination-path safety rather than archive-controlled extraction
paths.

## 8. Related documents

- Docs/ARCHIVE_FORMAT.md
- Docs/ARCHIVE_INFO.md
- Docs/Test_Plan_File_Compression_Tool_Team14.pdf
- Docs/Test_Reports/Unit_Test_Report.md
- Docs/Test_Reports/System_Validation_Report.md
- Docs/Test_Reports/Code_Coverage_Report.md
- Docs/Test_Reports/Branch_Coverage_Report.md
