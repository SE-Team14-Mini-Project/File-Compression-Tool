# Branch Coverage Report

**Project:** Team 14 - File Compression Tool
**Date:** 2 November 2026
**Tool:** LLVM Source-Based Code Coverage
**Compiler:** Apple Clang / LLVM 21

## 1. Objective

To measure how effectively the automated tests exercise alternative decision outcomes and conditional branches in the production source code.

## 2. Overall Branch Coverage

| Branches covered | Total branches | Branch coverage |
|---:|---:|---:|
| 424 | 528 | 80.30% |

A total of 104 branches were not covered during the measured test execution.

## 3. Module-wise Branch Coverage

| Source file | Covered branches | Total branches | Coverage |
|---|---:|---:|---:|
| archive.cpp | 114 | 126 | 90.48% |
| controller.cpp | 73 | 110 | 66.36% |
| file_io.cpp | 60 | 80 | 75.00% |
| huffman.cpp | 66 | 80 | 82.50% |
| main.cpp | 37 | 42 | 88.10% |
| validation.cpp | 74 | 90 | 82.22% |
| **Total** | **424** | **528** | **80.30%** |

## 4. Analysis

- Main/CLI has 88.10% branch coverage.
- Archive has 90.48% branch coverage, the highest among the production modules.
- Controller has 66.36% branch coverage, the lowest among the production modules.
- File I/O has 75.00% branch coverage.
- Additional tests should target unexecuted decision outcomes, especially in controller and file I/O.

## 5. Tooling Limitation

LLVM reported: "1 functions have mismatched data" during coverage reporting. This warning persisted after regenerating raw profiles. The reported branch figures are retained without modification; the mismatch requires further investigation.

## 6. Conclusion

The project achieved 80.30% production branch coverage. Further tests targeting uncovered conditional paths, especially in controller.cpp, are recommended.
