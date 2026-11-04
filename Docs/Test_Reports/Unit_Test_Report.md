# Unit Test Report

**Project:** Team 14 - File Compression Tool
**Date:** 4 November 2026
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
| Tests executed | 17 |
| Passed | 17 |
| Failed | 0 |
| Pass rate | 100% |

## 4. Test Execution Results

| No. | Test case | Result |
|---:|---|---|
| 1 | Output publication faults | PASS |
| 2 | Controller | PASS |
| 3 | Bit I/O | PASS |
| 4 | Codec | PASS |
| 5 | Frequency analysis | PASS |
| 6 | File I/O | PASS |
| 7 | Overwrite cleanup | PASS |
| 8 | No-overwrite races | PASS |
| 9 | Overwrite CLI | PASS |
| 10 | Archive | PASS |
| 11 | Archive information | PASS |
| 12 | Archive information CLI | PASS |
| 13 | Validation | PASS |
| 14 | Security logging | PASS |
| 15 | Round trip | PASS |
| 16 | Output limits | PASS |
| 17 | CLI help | PASS |

## 5. Important Test Scenarios

- Frequency analysis and Huffman coding.
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

All 17 registered CTest tests passed in the recorded execution. No test failures were reported.

## 7. Conclusion

The automated unit test suite completed successfully for the tested module-level functionality and selected error-handling scenarios.
