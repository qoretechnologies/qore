/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    BPEModel.h

    Byte-Pair Encoding tokenizer model

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_BPE_MODEL_H
#define _QORE_TOKENIZER_BPE_MODEL_H

#include "models/AbstractTokenizerModel.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <utility>

namespace QoreTokenizer {

//! Byte-Pair Encoding tokenizer model
class BPEModel : public AbstractTokenizerModel {
public:
    //! Constructor: reads config from a parsed tokenizer.json "model" hash
    /** @param config the Qore hash containing vocab, merges, unk_token, etc.
        @param xsink exception sink
    */
    BPEModel(const QoreHashNode* config, ExceptionSink* xsink);

    //! Tokenizes a pre-token using the BPE algorithm
    std::vector<int> tokenize(const std::string& pre_token) const override;

    std::vector<TokenWithOffset> tokenizeWithOffsets(
        const std::string& pre_token) const override;
    int vocabSize() const override;
    std::string idToToken(int id) const override;
    int tokenToId(const std::string& token) const override;
    std::unordered_map<std::string, int> getVocab() const override;

private:
    //! token string -> token ID
    std::unordered_map<std::string, int> vocab;

    //! token ID -> token string
    std::vector<std::string> id_to_token;

    //! merge pairs in priority order
    std::vector<std::pair<std::string, std::string>> merges;

    //! merge pair -> rank (lower = higher priority)
    std::unordered_map<std::string, int> merge_rank;

    //! unknown token
    std::string unk_token;

    //! prefix added to continuing subword tokens (e.g., empty for GPT-2 BPE)
    std::string continuing_subword_prefix;

    //! suffix added to end-of-word tokens
    std::string end_of_word_suffix;

    //! whether to fall back to byte tokens <0xHH> when a token is not in vocab
    bool byte_fallback = false;

    //! whether to fuse consecutive unknown tokens into one
    bool fuse_unk = false;

    //! Gets the merge rank for a pair, or -1 if not found
    int getMergeRank(const std::string& a, const std::string& b) const;

    //! Splits a UTF-8 string into individual UTF-8 characters
    static std::vector<std::string> splitUtf8Chars(const std::string& input);

    //! Encodes a byte value as a <0xHH> token
    static std::string byteToken(uint8_t byte);

    //! Node in the BPE doubly-linked list
    struct BPENode {
        std::string symbol;
        int prev = -1;
        int next = -1;
        bool active = true;
        size_t orig_start = 0;
        size_t orig_end = 0;
    };

    //! Merge candidate for the priority queue
    struct MergeCandidate {
        int rank;
        int left_idx;
        int right_idx;
        bool operator>(const MergeCandidate& o) const { return rank > o.rank; }
    };

    //! Internal: run BPE merge with priority queue, return final symbols with offsets
    std::vector<TokenWithOffset> bpeMerge(const std::string& pre_token) const;

    //! Internal: look up symbols -> token IDs with byte_fallback/fuse_unk
    std::vector<int> symbolsToIds(const std::vector<std::string>& symbols) const;
    std::vector<TokenWithOffset> symbolsToIdsWithOffsets(
        const std::vector<BPENode>& nodes, int head) const;
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_BPE_MODEL_H
