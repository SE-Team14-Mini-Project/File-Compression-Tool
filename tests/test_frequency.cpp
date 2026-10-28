
#include "interfaces.h"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#define TEST_CHECK(condition)                                      \
    do {                                                            \
        if (!(condition)) {                                         \
            std::cerr << "Test failed: " #condition                 \
                      << " at " << __FILE__ << ":" << __LINE__      \
                      << '\n';                                      \
            std::exit(EXIT_FAILURE);                                \
        }                                                           \
    } while (false)

int main() {
    // Test 1: Normal text
    std::istringstream input("AAAAABBBCC");
    auto freq = team14::buildFrequencyTable(input);

    TEST_CHECK(freq['A'] == 5);
    TEST_CHECK(freq['B'] == 3);
    TEST_CHECK(freq['C'] == 2);
    TEST_CHECK(freq['D'] == 0);

    std::cout << "Test 1 passed: Normal text\n";

    // Test 2: Empty input
    std::istringstream emptyInput("");
    auto emptyFreq = team14::buildFrequencyTable(emptyInput);

    for (auto count : emptyFreq) {
        TEST_CHECK(count == 0);
    }

    std::cout << "Test 2 passed: Empty input\n";

    // Test 3: Spaces and newlines
    std::istringstream whitespaceInput("A A\n");
    auto whitespaceFreq = team14::buildFrequencyTable(whitespaceInput);

    TEST_CHECK(whitespaceFreq['A'] == 2);
    TEST_CHECK(whitespaceFreq[' '] == 1);
    TEST_CHECK(whitespaceFreq['\n'] == 1);

    std::cout << "Test 3 passed: Spaces and newlines\n";

    // Test 4: Binary bytes (0 and 255)
    std::string binaryData;
    binaryData.push_back(static_cast<char>(0));
    binaryData.push_back(
        static_cast<char>(static_cast<unsigned char>(255)));
    binaryData.push_back(
        static_cast<char>(static_cast<unsigned char>(255)));

    std::istringstream binaryInput(
        binaryData, std::ios::in | std::ios::binary);
    auto binaryFreq = team14::buildFrequencyTable(binaryInput);

    TEST_CHECK(binaryFreq[0] == 1);
    TEST_CHECK(binaryFreq[255] == 2);

    std::cout << "Test 4 passed: Binary bytes\n";
    std::cout << "\nAll frequency tests passed!\n";

    return 0;
}
