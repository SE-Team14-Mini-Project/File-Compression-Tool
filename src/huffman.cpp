
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

} // namespace team14
