# Code Coverage Report

**Project:** Team 14 - File Compression Tool
**Date:** 2 November 2026
**Tool:** LLVM Source-Based Code Coverage
**Compiler:** Apple Clang / LLVM 21

## 1. Objective

To measure the extent to which the project's production source code is exercised by the automated test suite.

## 2. Testing Environment

- Build system: CMake
- Language standard: C++17
- Build directory: build-coverage
- Coverage instrumentation: -fprofile-instr-generate -fcoverage-mapping
- Test framework: CTest
- Test execution: 14 tests passed, 0 failed

## 3. Overall Coverage Results

| Metric | Covered | Total | Coverage |
|---|---:|---:|---:|
| Functions | 36 | 36 | 100.00% |
| Lines | 912 | 1054 | 86.53% |
| Regions | 669 | 747 | 89.56% |

## 4. Module-wise Results

| Source file | Function coverage | Line coverage | Region coverage |
|---|---:|---:|---:|
| archive.cpp | 100.00% | 97.58% | 97.79% |
| controller.cpp | 100.00% | 76.17% | 78.29% |
| file_io.cpp | 100.00% | 86.96% | 88.66% |
| huffman.cpp | 100.00% | 87.85% | 89.57% |
| main.cpp | 100.00% | 95.49% | 93.33% |
| validation.cpp | 100.00% | 85.19% | 92.44% |

## 5. Observations

- All 36 production functions were executed.
- Archive and main/CLI modules have comparatively high line coverage.
- Controller has the lowest line coverage at 76.17%.
- Additional tests could improve coverage of unexecuted controller paths.

## 6. Tooling Limitation

LLVM reported: "1 functions have mismatched data" during coverage reporting. The warning persisted after deleting old raw profiles and regenerating the coverage data. The displayed coverage figures are recorded as reported by LLVM; the mismatch remains a limitation requiring further investigation.

## 7. Conclusion

The automated test suite achieved 86.53% production line coverage and 100% function coverage. Further testing, particularly around controller paths, is recommended.
