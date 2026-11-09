#include "interfaces.h"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>

using namespace team14;

namespace {

int checks = 0;

void check(bool condition, const char* message) {
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

// Simulate write failures without changing file permissions.
class FailingOutputBuffer : public std::streambuf {
protected:
    int_type overflow(int_type) override {
        return traits_type::eof();
    }
};

HuffmanTree leaf(int symbol) {
    auto node = std::make_shared<HuffmanNode>();
    node->symbol = symbol;
    return node;
}

HuffmanTree internal(HuffmanTree left, HuffmanTree right) {
    auto node = std::make_shared<HuffmanNode>();
    node->symbol = -1;
    node->left = left;
    node->right = right;
    return node;
}

void testEncodeErrors() {
    CodeTable codes{};
    EncodedStats stats{};

    // Test 1: Missing Huffman code.
    {
        std::istringstream input("A");
        std::ostringstream output;
        auto result = encode(input, codes, output, stats);
        check(result.code == ErrorCode::InvalidArchive,
              "missing Huffman code rejected");
    }

    codes['A'] = {false};
    FailingOutputBuffer buffer;

    // Test 2: Failure while writing a complete encoded byte.
    {
        std::istringstream input("AAAAAAAA");
        std::ostream output(&buffer);
        auto result = encode(input, codes, output, stats);
        check(result.code == ErrorCode::IoError,
              "complete encoded byte write failure");
    }

    // Test 3: Failure while writing the final padded byte.
    {
        std::istringstream input("A");
        std::ostream output(&buffer);
        auto result = encode(input, codes, output, stats);
        check(result.code == ErrorCode::IoError,
              "final encoded byte write failure");
    }
}

void testDecodeErrors() {
    // Test 4: Valid empty archive.
    {
        std::istringstream bits("");
        std::ostringstream restored;
        check(decode(bits, nullptr, 0, 0, restored).ok(),
              "valid empty archive accepted");
    }

    // Test 5: Empty archive with nonzero encoded bits.
    {
        std::istringstream bits("");
        std::ostringstream restored;
        check(decode(bits, nullptr, 1, 0, restored).code ==
                  ErrorCode::InvalidArchive,
              "invalid empty archive rejected");
    }

    // Test 6: Empty archive containing a Huffman tree.
    {
        std::istringstream bits("");
        std::ostringstream restored;
        check(decode(bits, leaf('A'), 0, 0, restored).code ==
                  ErrorCode::InvalidArchive,
              "unexpected tree rejected");
    }

    // Test 7: Nonempty archive without a Huffman tree.
    {
        std::istringstream bits("");
        std::ostringstream restored;
        check(decode(bits, nullptr, 1, 1, restored).code ==
                  ErrorCode::InvalidArchive,
              "missing tree rejected");
    }

    // Test 8: Invalid bit for a single-symbol tree.
    {
        std::istringstream bits(std::string(1, char(0x80)),
                                std::ios::binary);
        std::ostringstream restored;
        check(decode(bits, leaf('A'), 1, 1, restored).code ==
                  ErrorCode::InvalidArchive,
              "invalid single-symbol bit rejected");
    }

    // Test 9: Invalid branch in a malformed Huffman tree.
    {
        auto tree = internal(leaf('A'), nullptr);
        std::istringstream bits(std::string(1, char(0x80)),
                                std::ios::binary);
        std::ostringstream restored;
        check(decode(bits, tree, 1, 1, restored).code ==
                  ErrorCode::InvalidArchive,
              "missing decoding branch rejected");
    }

    // Test 10: Bitstream ends halfway through a Huffman code.
    {
        auto tree = internal(
            internal(leaf('A'), leaf('B')),
            leaf('C')
        );

        std::istringstream bits(std::string(1, char(0x00)),
                                std::ios::binary);
        std::ostringstream restored;

        check(decode(bits, tree, 1, 1, restored).code ==
                  ErrorCode::InvalidArchive,
              "incomplete Huffman code rejected");
    }

    // Test 11: Failure while writing decoded output.
    {
        FailingOutputBuffer buffer;
        std::ostream restored(&buffer);

        std::istringstream bits(std::string(1, char(0x00)),
                                std::ios::binary);

        check(decode(bits, leaf('A'), 1, 1, restored).code ==
                  ErrorCode::IoError,
              "decoded output write failure reported");
    }

    // Test 12: Truncated encoded input.
    {
        std::istringstream bits("");
        std::ostringstream restored;

        check(decode(bits, leaf('A'), 1, 1, restored).code ==
                  ErrorCode::InvalidArchive,
              "truncated packed data rejected");
    }
}

} // namespace

int main() {
    testEncodeErrors();
    testDecodeErrors();

    std::cout << "PASS: " << checks
              << " Huffman fault checks\n";

    return EXIT_SUCCESS;
}
