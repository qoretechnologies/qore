/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    WordPieceModel.h

    WordPiece tokenizer model (used by BERT and similar models)

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_WORDPIECE_MODEL_H
#define _QORE_TOKENIZER_WORDPIECE_MODEL_H

#include "models/AbstractTokenizerModel.h"

#include <string>
#include <vector>
#include <unordered_map>

namespace QoreTokenizer {

//! WordPiece tokenizer model
/** Uses greedy longest-prefix matching. For each word, it finds the longest
    prefix in the vocabulary, then continues with the remainder prefixed by
    the continuing_subword_prefix (default "##").
*/
class WordPieceModel : public AbstractTokenizerModel {
public:
    //! Constructor: reads config from a parsed tokenizer.json "model" hash
    /** @param config the Qore hash containing vocab, unk_token, etc.
        @param xsink exception sink
    */
    WordPieceModel(const QoreHashNode* config, ExceptionSink* xsink);

    //! Tokenizes a pre-token using greedy longest-prefix matching
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

    //! unknown token
    std::string unk_token;

    //! prefix for continuing subword tokens (default "##")
    std::string continuing_subword_prefix;

    //! maximum number of characters per input word
    int max_input_chars_per_word;
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_WORDPIECE_MODEL_H
