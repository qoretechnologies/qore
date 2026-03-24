/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    UnigramModel.h

    Unigram (SentencePiece) tokenizer model

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_UNIGRAM_MODEL_H
#define _QORE_TOKENIZER_UNIGRAM_MODEL_H

#include "models/AbstractTokenizerModel.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace QoreTokenizer {

//! Unigram (SentencePiece) tokenizer model
/** Uses the Viterbi algorithm to find the most likely segmentation.
    The vocabulary consists of (token, log_probability) pairs, and the
    best segmentation maximizes the sum of log probabilities.
*/
class UnigramModel : public AbstractTokenizerModel {
public:
    //! Constructor: reads config from a parsed tokenizer.json "model" hash
    /** @param config the Qore hash containing vocab (array of [token, score] pairs),
               unk_id, byte_fallback
        @param xsink exception sink
    */
    UnigramModel(const QoreHashNode* config, ExceptionSink* xsink);

    //! Tokenizes a pre-token using the Viterbi algorithm
    std::vector<int> tokenize(const std::string& pre_token) const override;

    int vocabSize() const override;
    std::string idToToken(int id) const override;
    int tokenToId(const std::string& token) const override;
    std::unordered_map<std::string, int> getVocab() const override;

private:
    //! vocabulary: index = ID, value = (token string, log probability)
    std::vector<std::pair<std::string, double>> vocab;

    //! token string -> ID lookup
    std::unordered_map<std::string, int> token_to_id;

    //! set of all vocab token strings for fast prefix matching
    std::unordered_set<std::string> token_set;

    //! maximum token length in bytes (for bounding substring searches)
    size_t max_token_len = 0;

    //! ID of the unknown token in the vocab array
    int unk_id = 0;

    //! whether to fall back to byte tokens for unknown characters
    bool byte_fallback = false;

    //! Encodes a byte value as a <0xHH> token
    static std::string byteToken(uint8_t byte);
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_UNIGRAM_MODEL_H
