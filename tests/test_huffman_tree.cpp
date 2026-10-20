#include "interfaces.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace team14;

int main() {
    // Test 1: Empty frequency table
    FrequencyTable empty{};
    HuffmanTree emptyTree = buildTree(empty);

    assert(emptyTree == nullptr);
    std::cout << "Test 1 passed: Empty input\n";

    // Test 2: Single symbol
    FrequencyTable single{};
    single['A'] = 5;

    HuffmanTree singleTree = buildTree(single);

    assert(singleTree != nullptr);
    assert(singleTree->symbol == 'A');
    assert(singleTree->frequency == 5);
    assert(singleTree->left == nullptr);
    assert(singleTree->right == nullptr);

    std::cout << "Test 2 passed: Single symbol\n";

    // Test 3: Two symbols
    FrequencyTable two{};
    two['A'] = 5;
    two['B'] = 2;

    HuffmanTree twoTree = buildTree(two);

    assert(twoTree != nullptr);
    assert(twoTree->symbol == -1);
    assert(twoTree->frequency == 7);
    assert(twoTree->left != nullptr);
    assert(twoTree->right != nullptr);

    std::cout << "Test 3 passed: Two symbols\n";

    // Test 4: Multiple symbols
    FrequencyTable multiple{};
    multiple['A'] = 5;
    multiple['B'] = 2;
    multiple['C'] = 1;
    multiple['D'] = 4;

    HuffmanTree tree = buildTree(multiple);

    assert(tree != nullptr);
    assert(tree->frequency == 12);

    std::cout << "Test 4 passed: Multiple symbols\n";

    // Test 5: Frequency overflow
    FrequencyTable overflow{};
    overflow['A'] = std::numeric_limits<std::uint64_t>::max();
    overflow['B'] = 1;

    bool threw = false;

    try {
        buildTree(overflow);
    } catch (const std::overflow_error&) {
        threw = true;
    }

    assert(threw);
    std::cout << "Test 5 passed: Frequency overflow\n";

    std::cout << "\nAll Huffman tree tests passed!\n";
    return 0;
}
