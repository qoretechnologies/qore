/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    darts_trie.cpp

    Darts-clone DoubleArray trie implementation

    Based on the SentencePiece normalizer's use of Darts-clone:
    - github.com/google/sentencepiece/blob/master/src/normalizer.cc
    - github.com/google/sentencepiece/blob/master/third_party/darts_clone/darts.h

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "utils/darts_trie.h"

#include <cstring>
#include <algorithm>

namespace QoreTokenizer {

bool DartsTrie::parse(const uint8_t* data, size_t size) {
    if (size < 4) {
        return false;
    }

    // Read trie blob size (little-endian uint32, cast to avoid UB on shift)
    uint32_t trie_blob_size = (uint32_t)data[0] | ((uint32_t)data[1] << 8)
        | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);

    // Validate: must be at least 4 bytes and divisible by 4 (each unit is uint32)
    if (trie_blob_size < 4 || trie_blob_size % 4 != 0) {
        return false;
    }

    if (4 + (size_t)trie_blob_size > size) {
        return false;
    }

    // Reject unreasonably large tries (DoS protection: max 64MB)
    size_t num_units = trie_blob_size / 4;
    if (num_units > 16 * 1024 * 1024) {
        return false;
    }

    trie_units.resize(num_units);
    for (size_t i = 0; i < num_units; ++i) {
        size_t off = 4 + i * 4;
        trie_units[i] = (uint32_t)data[off] | ((uint32_t)data[off + 1] << 8)
            | ((uint32_t)data[off + 2] << 16) | ((uint32_t)data[off + 3] << 24);
    }

    // Remaining data is the normalized string pool
    size_t pool_start = 4 + trie_blob_size;
    if (pool_start < size) {
        string_pool.assign(reinterpret_cast<const char*>(data + pool_start),
            size - pool_start);
        // Validate: last byte should be \0
        if (!string_pool.empty() && string_pool.back() != '\0') {
            string_pool.push_back('\0');
        }
    }

    return true;
}

// Darts-clone DoubleArray traversal following the exact algorithm from darts.h:
//
// The DoubleArray encodes a trie where each node has:
// - offset(): base address for children; child for byte `c` is at offset() ^ c
// - has_leaf(): whether this node has a value (terminal)
// - value(): the stored value (only valid at leaf nodes, i.e., at offset() ^ 0)
//
// To traverse for byte `c` from a node:
//   next_pos = offset(unit[current_pos]) ^ c
//   if unit[next_pos] exists and label(unit[next_pos]) == c, transition succeeded
//
// To check for a value at a node:
//   if has_leaf(unit[current_pos]) is true, then unit[offset(unit[current_pos]) ^ 0]
//   contains the value via value()
//
// label check: (unit ^ c) & ((1 << 31) | 0xFF) == c
// commonPrefixSearch validates labels on each transition to ensure correctness.

std::vector<TrieMatch> DartsTrie::commonPrefixSearch(const uint8_t* input,
        size_t input_len) const {
    std::vector<TrieMatch> results;

    if (trie_units.empty() || input_len == 0) {
        return results;
    }

    // Start at root (position 0)
    uint32_t unit = trie_units[0];
    size_t node_pos = offset(unit);  // children base for root

    // Check if root itself has a value (empty string match)
    if (hasLeaf(unit)) {
        uint32_t leaf_pos = node_pos;  // offset ^ 0 = offset
        if (leaf_pos < trie_units.size()) {
            results.push_back({value(trie_units[leaf_pos]), 0});
        }
    }

    for (size_t i = 0; i < input_len; ++i) {
        // Traverse to child for input[i]
        size_t child_pos = node_pos ^ (size_t)input[i];
        if (child_pos >= trie_units.size()) {
            break;
        }

        unit = trie_units[child_pos];

        // Verify label matches (lower 8 bits XOR should give the byte)
        // In Darts-clone, label = unit & ((1 << 31) | 0xFF)
        // For a valid transition, label should match the input byte
        uint32_t label = unit & ((1u << 31) | 0xFF);
        if (label != (uint32_t)input[i]) {
            break;  // no valid transition
        }

        // Check if this node has a value (prefix match of length i+1)
        if (hasLeaf(unit)) {
            uint32_t leaf_offset = offset(unit);
            if (leaf_offset < trie_units.size()) {
                results.push_back({value(trie_units[leaf_offset]), i + 1});
            }
        }

        // Advance: children of this node start at offset(unit)
        node_pos = offset(unit);
    }

    // Sort by length descending (longest match first)
    std::sort(results.begin(), results.end(),
        [](const TrieMatch& a, const TrieMatch& b) {
            return a.second > b.second;
        });

    return results;
}

std::string DartsTrie::getNormalizedString(uint32_t off) const {
    if (off >= string_pool.size()) {
        return "";
    }
    // Read null-terminated string from pool
    size_t len = strnlen(string_pool.data() + off, string_pool.size() - off);
    return std::string(string_pool.data() + off, len);
}

} // namespace QoreTokenizer
