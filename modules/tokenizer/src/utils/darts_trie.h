/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    darts_trie.h

    Darts-clone DoubleArray trie for SentencePiece precompiled normalizer

    This implements the commonPrefixSearch operation on the DoubleArray trie
    format used by SentencePiece for its precompiled character normalization
    maps. The binary format is: [trie_size:u32le][trie_data:u32le[]][strings:\0-terminated]

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_DARTS_TRIE_H
#define _QORE_TOKENIZER_DARTS_TRIE_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <utility>

namespace QoreTokenizer {

//! Result of a prefix search: (value, length) where value is the offset
//! into the normalized string pool, and length is the number of input bytes matched
using TrieMatch = std::pair<uint32_t, size_t>;

//! Darts-clone DoubleArray trie for commonPrefixSearch
class DartsTrie {
public:
    //! Parses the precompiled charsmap from base64-decoded binary data
    /** @param data pointer to the raw binary data
        @param size size of the binary data in bytes
        @return true on success, false if the data is malformed
    */
    bool parse(const uint8_t* data, size_t size);

    //! Finds all prefixes of the input that match in the trie
    /** @param input pointer to the input bytes
        @param input_len length of the input
        @return list of matches (value, matched_length), longest first
    */
    std::vector<TrieMatch> commonPrefixSearch(const uint8_t* input, size_t input_len) const;

    //! Gets the normalized string at the given offset in the string pool
    /** @param offset byte offset into the string pool
        @return the null-terminated string at that offset, or empty if invalid
    */
    std::string getNormalizedString(uint32_t offset) const;

    //! Returns true if the trie has been successfully parsed
    bool isValid() const { return !trie_units.empty(); }

private:
    std::vector<uint32_t> trie_units;
    std::string string_pool;

    // Darts DoubleArray unit accessors
    static bool hasLeaf(uint32_t unit) { return (unit >> 8) & 1; }
    static uint32_t value(uint32_t unit) { return unit & ((1u << 31) - 1); }
    static uint32_t offset(uint32_t unit) {
        return (unit >> 10) << ((unit & (1u << 9)) >> 6);
    }
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_DARTS_TRIE_H
