# Code Coverage Report

**Project:** Team 14 - File Compression Tool
**Date:** 4 November 2026
**Coverage tool:** LLVM `llvm-cov`
**Test execution:** 17 passed, 0 failed

## 1. Objective

Measure code coverage of the production components exercised by the automated test suite.

## 2. Coverage Summary

| Metric | Covered | Total | Coverage |
|---|---:|---:|---:|
| Lines | 935 | 1054 | 88.71% |
| Regions | 681 | 747 | 91.16% |
| Functions | 36 | 36 | 100.00% |
| Branches | 436 | 528 | 82.58% |

## 3. Module-wise Coverage

| Module | Line Coverage | Region Coverage | Function Coverage | Branch Coverage |
|---|---:|---:|---:|---:|
| archive.cpp | 97.58% | 97.79% | 100.00% | 90.48% |
| controller.cpp | 81.88% | 83.43% | 100.00% | 72.73% |
| file_io.cpp | 86.96% | 88.66% | 100.00% | 75.00% |
| huffman.cpp | 87.85% | 89.57% | 100.00% | 82.50% |
| main.cpp | 97.74% | 95.00% | 100.00% | 90.48% |
| validation.cpp | 87.04% | 94.12% | 100.00% | 86.67% |
| **Total** | **88.71%** | **91.16%** | **100.00%** | **82.58%** |

## 4. Coverage Interpretation

- All 36 production functions were covered.
- 88.71% of production lines were executed.
- 82.58% of production branches were exercised.
- Controller and file I/O contain comparatively more uncovered branches.
- The results identify areas where additional tests could improve coverage.

## 5. Tooling Note

LLVM emitted the warning `1 functions have mismatched data` during coverage reporting. The warning is recorded as a tooling limitation. The coverage figures should be interpreted with this limitation in mind.

## 6. Conclusion

The production code achieved 88.71% line coverage, 82.58% branch coverage, 100.00% function coverage, and 91.16% region coverage during the 17-test execution. Further testing should focus on uncovered controller and file I/O paths.
