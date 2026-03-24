/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    darts_trie.cpp

    Darts-clone DoubleArray trie implementation

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

std::vector<TrieMatch> DartsTrie::commonPrefixSearch(const uint8_t* input,
        size_t input_len) const {
    std::vector<TrieMatch> results;

    if (trie_units.empty() || input_len == 0) {
        return results;
    }

    uint32_t node_pos = 0;
    uint32_t unit = trie_units[0];
    uint32_t node_offset = offset(unit);

    for (size_t i = 0; i < input_len; ++i) {
        uint8_t key = input[i];
        node_pos = node_offset ^ key;

        if (node_pos >= trie_units.size()) {
            break;
        }

        unit = trie_units[node_pos];

        // Check if this node has a leaf (= a match at this prefix length)
        if (hasLeaf(unit)) {
            uint32_t leaf_pos = node_offset ^ 0; // leaf is at label 0
            if (leaf_pos < trie_units.size()) {
                uint32_t leaf_unit = trie_units[leaf_pos];
                // For non-zero label traversals, the leaf check is different
            }
        }

        node_offset = offset(unit);

        // Check for a value at this node (via leaf at offset ^ 0)
        if (node_offset < trie_units.size()) {
            uint32_t leaf_check = node_offset; // position 0 under this node
            if (leaf_check < trie_units.size()) {
                uint32_t lc_unit = trie_units[leaf_check];
                if (hasLeaf(lc_unit) || (lc_unit & (1u << 31))) {
                    // This is a terminal node
                    uint32_t val = value(lc_unit);
                    results.push_back({val, i + 1});
                }
            }
        }
    }

    // Sort by length descending (longest match first)
    std::sort(results.begin(), results.end(),
        [](const TrieMatch& a, const TrieMatch& b) {
            return a.second > b.second;
        });

    return results;
}

std::string DartsTrie::getNormalizedString(uint32_t offset) const {
    if (offset >= string_pool.size()) {
        return "";
    }
    // Read null-terminated string from pool
    size_t len = strnlen(string_pool.data() + offset, string_pool.size() - offset);
    return std::string(string_pool.data() + offset, len);
}

} // namespace QoreTokenizer
