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

    int vocabSize() const override;
    std::string idToToken(int id) const override;
    int tokenToId(const std::string& token) const override;

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
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_BPE_MODEL_H
