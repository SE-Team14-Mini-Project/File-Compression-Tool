# Code Coverage Report

**Project:** Team 14 - File Compression Tool
**Date:** 10 November 2026
**Coverage tool:** LLVM `llvm-cov`
**Test execution:** 19 passed, 0 failed

## 1. Objective

Measure production-code coverage using the final automated test suite.

## 2. Overall Coverage

| Metric | Covered | Total | Coverage |
|---|---:|---:|---:|
| Lines | 966 | 1054 | 91.65% |
| Regions | 698 | 747 | 93.44% |
| Functions | 36 | 36 | 100.00% |
| Branches | 457 | 528 | 86.55% |

## 3. Module-wise Coverage

| Module | Line Coverage | Region Coverage | Function Coverage | Branch Coverage |
|---|---:|---:|---:|---:|
| archive.cpp | 97.58% | 97.79% | 100.00% | 90.48% |
| controller.cpp | 81.88% | 83.43% | 100.00% | 72.73% |
| file_io.cpp | 98.26% | 96.91% | 100.00% | 88.75% |
| huffman.cpp | 97.79% | 97.39% | 100.00% | 95.00% |
| main.cpp | 97.74% | 95.00% | 100.00% | 90.48% |
| validation.cpp | 87.04% | 94.12% | 100.00% | 86.67% |

## 4. Interpretation

- Production modules only; test files and headers are excluded.
- Additional Huffman and file I/O fault-injection tests are included.
- Coverage does not establish exhaustive correctness.

## 5. Tooling Note

LLVM previously reported "1 functions have mismatched data"; this warning requires investigation.

## 6. Conclusion

Line coverage: 91.65%; branch coverage: 86.55%; function coverage: 100.00%.
