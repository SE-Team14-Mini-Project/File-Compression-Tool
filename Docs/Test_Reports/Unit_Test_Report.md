# Unit Test Report

**Project:** Team 14 - File Compression Tool
**Date:** 10 November 2026
**Test Framework:** CTest
**Build System:** CMake

## 1. Objective

Verify individual modules and important failure-handling paths through automated unit testing.

## 2. Testing Environment

- Language: C++17
- Build directory: build
- Test command: `ctest --test-dir build --output-on-failure`

## 3. Execution Summary

| Result | Count |
|---|---:|
| Tests executed | 19 |
| Passed | 19 |
| Failed | 0 |
| Pass rate | 100% |

## 4. Test Execution Results

| No. | Test case | Result |
|---:|---|---|
| 1 | team14.output_publication_faults | PASS |
| 2 | team14.controller | PASS |
| 3 | team14.bit_io | PASS |
| 4 | team14.codec | PASS |
| 5 | team14.huffman_faults | PASS |
| 6 | team14.frequency | PASS |
| 7 | team14.file_io | PASS |
| 8 | team14.file_io_faults | PASS |
| 9 | team14.overwrite_cleanup | PASS |
| 10 | team14.no_overwrite_races | PASS |
| 11 | team14.overwrite_cli | PASS |
| 12 | team14.archive | PASS |
| 13 | team14.archive_info | PASS |
| 14 | team14.archive_info_cli | PASS |
| 15 | team14.validation | PASS |
| 16 | team14.security_logging | PASS |
| 17 | team14.round_trip | PASS |
| 18 | team14.output_limits | PASS |
| 19 | team14.cli_help | PASS |

## 5. Important Test Scenarios

- Frequency analysis and Huffman coding.
- 12 Huffman error-handling fault checks.
- 23 file I/O fault-injection checks.
- Bit-level input and output operations.
- Archive creation and archive information retrieval.
- Compression and decompression round trips.
- Input and output validation.
- Existing destination and overwrite handling.
- Temporary output cleanup and publication safety.
- Late destination collisions involving files, directories, and symlinks where supported.
- Injected codec and tree-construction failures.
- Output limits and security-related logging.

## 6. Result

All 19 registered CTest tests passed in the recorded execution. No test failures were reported.

## 7. Conclusion

The automated unit test suite completed successfully for the tested module-level functionality and selected error-handling scenarios.
