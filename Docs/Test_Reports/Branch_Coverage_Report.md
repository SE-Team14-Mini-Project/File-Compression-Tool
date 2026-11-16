# Branch Coverage Report

**Project:** Team 14 - File Compression Tool
**Date:** 10 November 2026
**Coverage tool:** LLVM `llvm-cov`
**Test execution:** 19 passed, 0 failed

## 1. Objective

Measure production-code decision branch coverage.

## 2. Overall Branch Coverage

- Covered branches: 457
- Total branches: 528
- Uncovered branches: 71
- Branch coverage: 86.55%

## 3. Module-wise Branch Coverage

| Module | Covered | Total | Coverage |
|---|---:|---:|---:|
| archive.cpp | 114 | 126 | 90.48% |
| controller.cpp | 80 | 110 | 72.73% |
| file_io.cpp | 71 | 80 | 88.75% |
| huffman.cpp | 76 | 80 | 95.00% |
| main.cpp | 38 | 42 | 90.48% |
| validation.cpp | 78 | 90 | 86.67% |
| **Total** | **457** | **528** | **86.55%** |

## 4. Analysis

The 19-test suite includes new Huffman and file I/O fault tests.
Uncovered branches remain; the results are not exhaustive proof of correctness.

## 5. Tooling Note

LLVM previously reported "1 functions have mismatched data"; this warning requires investigation.

## 6. Conclusion

457 of 528 production branches were covered (86.55%).
