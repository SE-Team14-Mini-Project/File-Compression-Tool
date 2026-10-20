
#include "interfaces.h"
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

int main() {

    // Test 1: Normal text
    std::istringstream input("AAAAABBBCC");
    auto freq = team14::buildFrequencyTable(input);

    assert(freq['A'] == 5);
    assert(freq['B'] == 3);
    assert(freq['C'] == 2);
    assert(freq['D'] == 0);

    std::cout << "Test 1 passed: Normal text\n";


    // Test 2: Empty input
    std::istringstream emptyInput("");
    auto emptyFreq = team14::buildFrequencyTable(emptyInput);

    for (auto count : emptyFreq) {
        assert(count == 0);
    }

    std::cout << "Test 2 passed: Empty input\n";


    // Test 3: Spaces and newlines
    std::istringstream whitespaceInput("A A\n");
    auto whitespaceFreq = team14::buildFrequencyTable(whitespaceInput);

    assert(whitespaceFreq['A'] == 2);
    assert(whitespaceFreq[' '] == 1);
    assert(whitespaceFreq['\n'] == 1);

    std::cout << "Test 3 passed: Spaces and newlines\n";


    // Test 4: Binary bytes (0 and 255)
    std::string binaryData;

    binaryData.push_back(char(0));
    binaryData.push_back(char(255));
    binaryData.push_back(char(255));

    std::istringstream binaryInput(binaryData);
    auto binaryFreq = team14::buildFrequencyTable(binaryInput);

    assert(binaryFreq[0] == 1);
    assert(binaryFreq[255] == 2);

    std::cout << "Test 4 passed: Binary bytes\n";


    std::cout << "\nAll frequency tests passed!\n";

    return 0;
}
