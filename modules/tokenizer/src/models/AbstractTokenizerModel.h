/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    AbstractTokenizerModel.h

    Abstract base class for tokenizer models (BPE, WordPiece, Unigram)

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_ABSTRACT_TOKENIZER_MODEL_H
#define _QORE_TOKENIZER_ABSTRACT_TOKENIZER_MODEL_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

#include "qore/Qore.h"

namespace QoreTokenizer {

//! Abstract base class for tokenizer models
class AbstractTokenizerModel {
public:
    virtual ~AbstractTokenizerModel() = default;

    //! Tokenizes a pre-token (word/subword) into token IDs
    /** @param pre_token the pre-tokenized string to tokenize
        @return vector of token IDs
    */
    virtual std::vector<int> tokenize(const std::string& pre_token) const = 0;

    //! Returns the vocabulary size
    virtual int vocabSize() const = 0;

    //! Returns the token string for a given ID
    /** @param id the token ID
        @return the token string, or empty string if id is out of range
    */
    virtual std::string idToToken(int id) const = 0;

    //! Returns the token ID for a given token string
    /** @param token the token string
        @return the token ID, or -1 if not found
    */
    virtual int tokenToId(const std::string& token) const = 0;

    //! Returns the full vocabulary as a token-to-id mapping
    virtual std::unordered_map<std::string, int> getVocab() const = 0;

    //! Factory: creates a model from a parsed tokenizer.json "model" config hash
    /** Dispatches on config.type:
        - "BPE" -> BPEModel
        - "WordPiece" -> WordPieceModel
        - "Unigram" -> UnigramModel

        @param config the Qore hash from the tokenizer.json "model" key
        @param xsink exception sink
        @return the model, or nullptr on error
    */
    static std::unique_ptr<AbstractTokenizerModel> fromConfig(const QoreHashNode* config,
        ExceptionSink* xsink);
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_ABSTRACT_TOKENIZER_MODEL_H
