# System Validation Report

**Project:** Team 14 - File Compression Tool
**Date:** 4 November 2026
**Validation method:** Automated CTest execution and production coverage analysis

## 1. Objective

Validate the integrated file compression application against its tested functional, interface, file-handling, and failure-handling behaviours.

## 2. Validation Environment

- Language: C++17
- Build system: CMake
- Test framework: CTest
- Normal build directory: `build`
- Coverage build directory: `build-coverage`

## 3. System Test Execution

The complete registered CTest suite was executed using:

`ctest --test-dir build --output-on-failure`

**Result:** 17 tests passed, 0 failed.

## 4. Validation Areas

| Validation area | Test evidence | Result |
|---|---|---|
| Compression and decompression | Round-trip tests | PASS |
| Huffman coding components | Frequency and codec tests | PASS |
| Archive handling | Archive and archive-info tests | PASS |
| Command-line interface | CLI help and overwrite CLI tests | PASS |
| File operations | File I/O tests | PASS |
| Input validation | Validation tests | PASS |
| Output publication safety | Publication fault tests | PASS |
| Destination collision handling | No-overwrite race tests | PASS |
| Overwrite cleanup | Overwrite cleanup tests | PASS |
| Output-size restrictions | Output limits tests | PASS |
| Security-related logging | Security logging tests | PASS |

## 5. Failure-handling Validation

The publication fault tests include injected failures during tree construction, encoding, and decoding. They check that failed operations do not publish incomplete output and that temporary files are cleaned up.

The no-overwrite race tests exercise late destination collisions involving files, directories, and symlinks where supported.

## 6. Coverage Evidence

The production coverage run reported:

- Line coverage: 88.71%
- Branch coverage: 82.58%
- Function coverage: 100.00%
- Region coverage: 91.16%

LLVM also emitted a `1 functions have mismatched data` warning. This remains an open tooling limitation.

## 7. Limitations

- Results represent the registered automated tests, not exhaustive proof of correctness.
- Platform-dependent symlink scenarios may be skipped where unsupported.
- Performance acceptance targets have not been independently established by this report.
- Uncovered production branches remain, particularly in controller and file I/O.

## 8. Conclusion

The integrated application passed all 17 registered automated tests. The tested compression, decompression, archive, command-line, validation, and output-safety scenarios completed successfully. Further testing is recommended for uncovered branches and performance requirements specified in the project test plan.
