
#include "interfaces.h"

#include <algorithm>
#include <istream>
#include <limits>
#include <queue>
#include <stdexcept>
#include <vector>

using namespace std;

namespace team14 {

// Issue #2: Byte Frequency Analysis
FrequencyTable buildFrequencyTable(istream& input) {
    FrequencyTable frequencies{};

    char byte;

    while (input.get(byte)) {
        unsigned char symbol = byte;
        frequencies[symbol]++;
    }

    return frequencies;
}


// Comparator for the min-priority queue
struct CompareNodes {
    bool operator()(const HuffmanTree& a, const HuffmanTree& b) const {

        if (a->frequency != b->frequency) {
            return a->frequency > b->frequency;
        }

        return a->minSymbol > b->minSymbol;
    }
};


// Issue #3: Huffman Tree Construction
HuffmanTree buildTree(const FrequencyTable& frequencies) {

    priority_queue<HuffmanTree, vector<HuffmanTree>, CompareNodes> pq;

    // Insert all characters having non-zero frequencies
    for (int i = 0; i < 256; i++) {

        if (frequencies[i] == 0) {
            continue;
        }

        HuffmanTree node = make_shared<HuffmanNode>();

        node->frequency = frequencies[i];
        node->symbol = i;
        node->minSymbol = i;

        pq.push(node);
    }

    // Empty input
    if (pq.empty()) {
        return nullptr;
    }

    // Combine the two smallest nodes repeatedly
    while (pq.size() > 1) {

        HuffmanTree left = pq.top();
        pq.pop();

        HuffmanTree right = pq.top();
        pq.pop();

        if (left->frequency >
            numeric_limits<uint64_t>::max() - right->frequency) {
            throw overflow_error("Huffman frequency overflow");
        }

        HuffmanTree parent = make_shared<HuffmanNode>();

        parent->frequency = left->frequency + right->frequency;
        parent->symbol = -1;
        parent->minSymbol = min(left->minSymbol, right->minSymbol);

        parent->left = left;
        parent->right = right;

        pq.push(parent);
    }

    return pq.top();
}


void findCodes(HuffmanTree node, vector<bool> path, CodeTable& codes) {

    if (node == nullptr) {
        return;
    }

    // A leaf node represents a character.
    if (node->symbol != -1) {

        // Special case: only one unique character.
        if (path.empty()) {
            path.push_back(false);
        }

        codes[node->symbol] = path;
        return;
    }

    // Left child adds 0.
    path.push_back(false);
    findCodes(node->left, path, codes);

    // Change the last bit from 0 to 1 for the right child.
    path.back() = true;
    findCodes(node->right, path, codes);
}


CodeTable generateCodes(const HuffmanTree& tree) {

    CodeTable codes{};

    vector<bool> path;
    findCodes(tree, path, codes);

    return codes;
}


Result encode(istream& input, const CodeTable& codes,
              ostream& packedBits, EncodedStats& stats) {

    stats = EncodedStats{};

    unsigned char currentByte = 0;
    int bitCount = 0;
    char ch;

    while (input.get(ch)) {

        unsigned char symbol = ch;
        const vector<bool>& code = codes[symbol];

        if (code.empty()) {
            return {ErrorCode::InvalidArchive, "Missing Huffman code"};
        }

        for (bool bit : code) {

            // Shift existing bits left to make space.
            currentByte = currentByte << 1;

            // Insert the next Huffman bit.
            if (bit) {
                currentByte = currentByte | 1;
            }

            bitCount++;
            stats.meaningfulBits++;

            // Write whenever 8 bits are collected.
            if (bitCount == 8) {
                packedBits.put(static_cast<char>(currentByte));

                if (!packedBits) {
                    return {ErrorCode::IoError, "Failed to write encoded data"};
                }

                stats.packedBytes++;
                currentByte = 0;
                bitCount = 0;
            }
        }
    }

    // Check if reading failed for a reason other than EOF.
    if (input.bad()) {
        return {ErrorCode::IoError, "Failed to read input"};
    }

    // Write the last byte, padding unused bits with zeros.
    if (bitCount > 0) {

        currentByte = currentByte << (8 - bitCount);
        packedBits.put(static_cast<char>(currentByte));

        if (!packedBits) {
            return {ErrorCode::IoError, "Failed to write final byte"};
        }

        stats.packedBytes++;
    }

    return {ErrorCode::Ok, ""};
}


} // namespace team14
