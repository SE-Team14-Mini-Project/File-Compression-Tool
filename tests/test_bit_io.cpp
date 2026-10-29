#include "interfaces.h"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

using namespace team14;

namespace {
void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main() {
    const std::string original = "Bit I/O test: 101010 ABC xyz!";
    std::istringstream frequencyInput(original);
    const FrequencyTable frequencies = buildFrequencyTable(frequencyInput);
    const HuffmanTree tree = buildTree(frequencies);
    const CodeTable codes = generateCodes(tree);

    check(tree != nullptr, "Huffman tree is created");

    std::istringstream input(original);
    std::ostringstream packed(std::ios::out | std::ios::binary);
    EncodedStats stats{};

    const Result encoded = encode(input, codes, packed, stats);
    check(encoded.ok(), "Bit encoding succeeds");
    check(stats.packedBytes == packed.str().size(),
          "Packed byte count is correct");

    std::istringstream packedInput(
        packed.str(), std::ios::in | std::ios::binary);
    std::ostringstream restored(std::ios::out | std::ios::binary);

    const Result decoded = decode(
        packedInput, tree, stats.meaningfulBits,
        original.size(), restored);

    check(decoded.ok(), "Bit decoding succeeds");
    check(restored.str() == original, "Decoded bytes match original");

    std::cout << "All bit I/O tests passed.\n";
    return EXIT_SUCCESS;
}
