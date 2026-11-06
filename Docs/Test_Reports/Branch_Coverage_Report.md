# Branch Coverage Report

**Project:** Team 14 - File Compression Tool
**Date:** 4 November 2026
**Coverage tool:** LLVM `llvm-cov`
**Test execution:** 17 passed, 0 failed

## 1. Objective

Measure how many production-code decision branches were exercised by the automated test suite.

## 2. Overall Branch Coverage

| Metric | Count |
|---|---:|
| Total branches | 528 |
| Covered branches | 436 |
| Uncovered branches | 92 |
| Branch coverage | 82.58% |

## 3. Module-wise Branch Coverage

| Module | Covered | Total | Branch Coverage |
|---|---:|---:|---:|
| archive.cpp | 114 | 126 | 90.48% |
| controller.cpp | 80 | 110 | 72.73% |
| file_io.cpp | 60 | 80 | 75.00% |
| huffman.cpp | 66 | 80 | 82.50% |
| main.cpp | 38 | 42 | 90.48% |
| validation.cpp | 78 | 90 | 86.67% |
| **Total** | **436** | **528** | **82.58%** |

## 4. Analysis

- Overall, 82.58% of production-code branches were exercised.
- Archive handling and the command-line entry point achieved 90.48%.
- Controller logic had the lowest branch coverage at 72.73%.
- File I/O achieved 75.00%.
- These modules are candidates for additional decision-path testing.

## 5. Tooling Note

LLVM emitted the warning `1 functions have mismatched data` during coverage reporting. This warning is documented as a tooling limitation.

## 6. Conclusion

The application achieved 82.58% production branch coverage, with 436 of 528 branches exercised. Additional tests should target uncovered controller and file I/O decision paths.
