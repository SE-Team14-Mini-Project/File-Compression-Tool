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
    // Build a Huffman tree from representative text.
    const std::string original = "AAAABBBCCDA";
    std::istringstream frequencyInput(original);
    const FrequencyTable frequencies = buildFrequencyTable(frequencyInput);
    const HuffmanTree tree = buildTree(frequencies);
    const CodeTable codes = generateCodes(tree);

    check(tree != nullptr, "Huffman tree is created");

    // Encode the input into packed bytes.
    std::istringstream input(original);
    std::ostringstream packed(std::ios::binary);
    EncodedStats stats{};

    const Result encodeResult = encode(input, codes, packed, stats);
    check(encodeResult.ok(), "encoding succeeds");
    check(stats.meaningfulBits > 0, "encoded bit count is non-zero");
    check(stats.packedBytes == packed.str().size(),
          "packed byte count matches output");

    // Decode exactly the meaningful bits.
    std::istringstream packedInput(packed.str(), std::ios::binary);
    std::ostringstream restored(std::ios::binary);

    const Result decodeResult = decode(
        packedInput, tree, stats.meaningfulBits,
        original.size(), restored);

    check(decodeResult.ok(), "decoding succeeds");
    check(restored.str() == original, "decoded text matches original");

    // Exercise an empty input.
    std::istringstream emptyInput("");
    const FrequencyTable emptyFrequencies =
        buildFrequencyTable(emptyInput);
    const HuffmanTree emptyTree = buildTree(emptyFrequencies);
    check(emptyTree == nullptr, "empty input produces no Huffman tree");

    std::cout << "All codec tests passed.\n";
    return EXIT_SUCCESS;
}
