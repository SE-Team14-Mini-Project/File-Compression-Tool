
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



// Decode MSB-first Huffman payload bits into the restored stream.
Result decode(istream& packedBits, const HuffmanTree& tree,
              uint64_t meaningfulBits, uint64_t originalBytes,
              ostream& restored) {
    if (originalBytes == 0) {
        if (meaningfulBits != 0 || tree != nullptr) {
            return {ErrorCode::InvalidArchive,
                    "Invalid empty-file Huffman metadata."};
        }
        return {};
    }

    if (!tree || meaningfulBits == 0) {
        return {ErrorCode::InvalidArchive,
                "Missing Huffman tree or encoded bits."};
    }

    uint64_t produced = 0;
    unsigned char lastByte = 0;

    if (tree->symbol != -1) {
        if (meaningfulBits != originalBytes) {
            return {ErrorCode::InvalidArchive,
                    "Invalid bit count for a single-symbol archive."};
        }

        for (uint64_t bitIndex = 0; bitIndex < meaningfulBits; ++bitIndex) {
            if (bitIndex % 8 == 0) {
                char rawByte = 0;
                if (!packedBits.get(rawByte)) {
                    return {ErrorCode::InvalidArchive,
                            "Encoded payload is truncated."};
                }
                lastByte = static_cast<unsigned char>(rawByte);
            }

            const unsigned shift =
                7U - static_cast<unsigned>(bitIndex % 8);
            if (((lastByte >> shift) & 1U) != 0) {
                return {ErrorCode::InvalidArchive,
                        "Invalid bit in a single-symbol payload."};
            }

            restored.put(static_cast<char>(tree->symbol));
            if (!restored) {
                return {ErrorCode::IoError,
                        "Failed to write decoded output."};
            }
        }

        const unsigned remainder =
            static_cast<unsigned>(meaningfulBits % 8);
        if (remainder != 0) {
            const unsigned unusedBits = 8U - remainder;
            const unsigned char mask =
                static_cast<unsigned char>((1U << unusedBits) - 1U);
            if ((lastByte & mask) != 0) {
                return {ErrorCode::InvalidArchive,
                        "Non-zero padding bits in encoded payload."};
            }
        }
        return {};
    }

    HuffmanTree current = tree;

    for (uint64_t bitIndex = 0; bitIndex < meaningfulBits; ++bitIndex) {
        if (bitIndex % 8 == 0) {
            char rawByte = 0;
            if (!packedBits.get(rawByte)) {
                return {ErrorCode::InvalidArchive,
                        "Encoded payload is truncated."};
            }
            lastByte = static_cast<unsigned char>(rawByte);
        }

        const unsigned shift = 7U - static_cast<unsigned>(bitIndex % 8);
        const bool bit = ((lastByte >> shift) & 1U) != 0;

        if (tree->symbol != -1) {
            // A single-symbol tree uses code 0 for each occurrence.
            if (bit) {
                return {ErrorCode::InvalidArchive,
                        "Invalid bit in a single-symbol payload."};
            }

            if (produced >= originalBytes) {
                return {ErrorCode::InvalidArchive,
                        "Decoded output exceeds the declared size."};
            }

            restored.put(static_cast<char>(tree->symbol));
            if (!restored) {
                return {ErrorCode::IoError,
                        "Failed to write decoded output."};
            }
            ++produced;
            continue;
        }

        // Follow the next bit through the Huffman tree.
        current = bit ? current->right : current->left;
        if (!current) {
            return {ErrorCode::InvalidArchive,
                    "Encoded bits do not match the Huffman tree."};
        }

        if (current->symbol != -1) {
            if (produced >= originalBytes) {
                return {ErrorCode::InvalidArchive,
                        "Decoded output exceeds the declared size."};
            }

            restored.put(static_cast<char>(current->symbol));
            if (!restored) {
                return {ErrorCode::IoError,
                        "Failed to write decoded output."};
            }

            ++produced;
            current = tree;
        }
    }

    if (current != tree) {
        return {ErrorCode::InvalidArchive,
                "Encoded bits end in the middle of a Huffman code."};
    }

    if (produced != originalBytes) {
        return {ErrorCode::InvalidArchive,
                "Decoded size does not match the archive header."};
    }

    // Unused low-order bits in the final byte must be zero.
    const unsigned remainder = static_cast<unsigned>(meaningfulBits % 8);
    if (remainder != 0) {
        const unsigned unusedBits = 8U - remainder;
        const unsigned char mask =
            static_cast<unsigned char>((1U << unusedBits) - 1U);
        if ((lastByte & mask) != 0) {
            return {ErrorCode::InvalidArchive,
                    "Non-zero padding bits in encoded payload."};
        }
    }

    return {};
}


} // namespace team14
