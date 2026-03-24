/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    HFTokenizer.h

    Main HuggingFace tokenizer class

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_HFTOKENIZER_H
#define _QORE_TOKENIZER_HFTOKENIZER_H

#include "qore/Qore.h"

#include "normalizers/AbstractNormalizer.h"
#include "pretokenizers/AbstractPreTokenizer.h"
#include "models/AbstractTokenizerModel.h"
#include "postprocessors/AbstractPostProcessor.h"
#include "decoders/AbstractDecoder.h"

#include <memory>
#include <string>
#include <vector>
#include <unordered_set>

namespace QoreTokenizer {

//! Main HuggingFace-compatible tokenizer
/** Implements the full tokenization pipeline:
    normalize → pre-tokenize → model tokenize → post-process

    Thread-safe: concurrent encode() calls on a shared instance are safe.
*/
class QoreHFTokenizer : public AbstractPrivateData {
public:
    //! Constructs from a parsed tokenizer.json config hash
    QoreHFTokenizer(const QoreHashNode* config, ExceptionSink* xsink);

    //! Encodes text to token IDs
    /** @param text the input text
        @param text_pair optional second text for sentence-pair tasks
        @param add_special_tokens whether to add special tokens (default true)
        @param xsink exception sink
        @return hash with input_ids, token_type_ids, attention_mask, tokens
    */
    QoreHashNode* encode(const QoreStringNode* text, const QoreStringNode* text_pair,
        bool add_special_tokens, ExceptionSink* xsink);

    //! Decodes token IDs back to text
    /** @param ids list of token IDs
        @param skip_special_tokens whether to skip special tokens
        @param xsink exception sink
        @return the decoded text
    */
    QoreStringNode* decode(const QoreListNode* ids, bool skip_special_tokens,
        ExceptionSink* xsink);

    //! Returns the vocabulary size
    int getVocabSize() const;

    //! Converts a token ID to its string representation
    QoreStringNode* idToToken(int id, ExceptionSink* xsink) const;

    //! Converts a token string to its ID
    int tokenToId(const QoreStringNode* token) const;

    //! Returns the full vocabulary as a token→id hash (includes added tokens)
    QoreHashNode* getVocab(ExceptionSink* xsink) const;

private:
    std::unique_ptr<AbstractNormalizer> normalizer;
    std::unique_ptr<AbstractPreTokenizer> pre_tokenizer;
    std::unique_ptr<AbstractTokenizerModel> model;
    std::unique_ptr<AbstractPostProcessor> post_processor;
    std::unique_ptr<AbstractDecoder> decoder;

    //! Special token IDs (for skip_special_tokens in decode)
    std::unordered_set<int> special_token_ids;

    //! Added tokens that are matched before the model
    struct AddedToken {
        std::string content;
        int id;
        bool special;
        bool single_word;
    };
    std::vector<AddedToken> added_tokens;

    //! Internal: tokenize a single pre-token through the model
    std::vector<int> tokenizePreToken(const std::string& pre_token) const;

    //! Internal: encode a text segment through normalize + pre-tokenize + model
    std::vector<int> encodeSegment(const std::string& text) const;

    //! Internal: encode text with added token splitting
    std::vector<int> encodeText(const std::string& text) const;
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_HFTOKENIZER_H
