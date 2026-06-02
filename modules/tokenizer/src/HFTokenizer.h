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
#include <shared_mutex>
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

    //! Advanced encoding with truncation and padding options
    /** @param text the input text
        @param options hash with: text_pair, max_length, truncation, padding,
        add_special_tokens
        @param xsink exception sink
        @return hash with input_ids, token_type_ids, attention_mask, tokens,
        offset_mapping, special_tokens_mask
    */
    QoreHashNode* encodeAdvanced(const QoreStringNode* text,
        const QoreHashNode* options, ExceptionSink* xsink);

    //! Batch encoding with truncation and padding
    QoreListNode* encodeBatch(const QoreListNode* texts,
        const QoreHashNode* options, ExceptionSink* xsink);

    //! Encodes one text as dense int64 buffers ready for model tensor creation
    QoreHashNode* encodeForModel(const QoreStringNode* text,
        const QoreHashNode* options, ExceptionSink* xsink);

    //! Encodes a batch of texts as dense int64 buffers ready for model tensor creation
    QoreHashNode* encodeBatchForModel(const QoreListNode* texts,
        const QoreHashNode* options, ExceptionSink* xsink);

    //! Encodes a batch of text pairs as dense int64 buffers ready for model tensor creation
    QoreHashNode* encodeBatchPairsForModel(const QoreListNode* texts,
        const QoreListNode* text_pairs, const QoreHashNode* options, ExceptionSink* xsink);

    //! Returns the pad token ID (or -1 if no pad token)
    int getPadTokenId() const { return pad_token_id; }

    //! Returns the EOS token ID (or -1 if not found)
    int getEosTokenId() const { return eos_token_id; }

    //! Returns the BOS token ID (or -1 if not found)
    int getBosTokenId() const { return bos_token_id; }

    //! Returns the CLS token ID (or -1 if not found)
    int getClsTokenId() const { return cls_token_id; }

    //! Returns the SEP token ID (or -1 if not found)
    int getSepTokenId() const { return sep_token_id; }

    //! Returns the UNK token ID (or -1 if not found)
    int getUnkTokenId() const { return unk_token_id; }

    //! Returns the model maximum sequence length (or -1 if not set)
    int getModelMaxLength() const { return model_max_length; }

    //! Decodes a batch of token ID sequences
    /** @param batch list of token ID lists
        @param skip_special_tokens whether to skip special tokens
        @param xsink exception sink
        @return list of decoded strings
    */
    QoreListNode* decodeBatch(const QoreListNode* batch, bool skip_special_tokens,
        ExceptionSink* xsink);

    //! Loads additional configuration from tokenizer_config.json
    /** Extracts special token overrides, model_max_length, and chat_template.
        @param config parsed tokenizer_config.json hash
        @param xsink exception sink
    */
    void loadConfig(const QoreHashNode* config, ExceptionSink* xsink);

    //! Applies a chat template to a list of messages
    /** @param messages list of {role, content} hashes
        @param xsink exception sink
        @return the formatted prompt string
    */
    QoreStringNode* applyChatTemplate(const QoreListNode* messages,
        ExceptionSink* xsink);

    //! Adds tokens to the vocabulary dynamically
    /** @param tokens list of token definitions (string or hash with content/special/single_word)
        @param xsink exception sink
        @return the number of tokens actually added (duplicates are skipped)

        @since tokenizer 1.1
    */
    int addTokens(const QoreListNode* tokens, ExceptionSink* xsink);

private:
    //! Reader-writer lock for thread-safe dynamic vocab extension
    mutable std::shared_mutex rw_mutex;
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

    //! Pad token ID (-1 if no pad token found)
    int pad_token_id = -1;
    //! EOS token ID (-1 if not found)
    int eos_token_id = -1;
    //! BOS token ID (-1 if not found)
    int bos_token_id = -1;
    //! CLS token ID (-1 if not found)
    int cls_token_id = -1;
    //! SEP token ID (-1 if not found)
    int sep_token_id = -1;
    //! UNK token ID (-1 if not found)
    int unk_token_id = -1;
    //! Model maximum sequence length (-1 if not set)
    int model_max_length = -1;
    //! Chat template string (Jinja2-like)
    std::string chat_template;

    //! Count of tokens added dynamically via addTokens() (not from config)
    int dynamic_added_count = 0;

    //! Internal encoding result with offsets and word IDs
    struct InternalEncoding {
        std::vector<int> ids;
        std::vector<std::string> tokens;
        std::vector<std::pair<size_t, size_t>> offsets;
        std::vector<int> word_ids;  //!< pre-token word index; -1 = special/added token
    };

    //! Internal model-input encoding with only fields required for dense tensor input
    struct ModelInputEncoding {
        std::vector<int> ids;
        std::vector<int> token_type_ids;
        std::vector<int> attention_mask;
        size_t sequence_length = 0;
    };

    //! Internal: encodeAdvanced without locking (caller must hold shared_lock)
    QoreHashNode* encodeAdvancedIntern(const QoreStringNode* text,
        const QoreHashNode* options, ExceptionSink* xsink);

    //! Internal: encode a single text or text pair for model-input buffers
    int encodeForModelIntern(const QoreStringNode* text, const QoreHashNode* options,
        const QoreStringNode* text_pair_override, ModelInputEncoding& out, ExceptionSink* xsink) const;

    //! Internal: build a dense model-input batch from texts and optional text pairs
    QoreHashNode* encodeBatchForModelIntern(const QoreListNode* texts, const QoreListNode* text_pairs,
        const QoreHashNode* options, ExceptionSink* xsink) const;

    //! Internal: encode pre-tokenized words (skip normalize + pre-tokenize)
    InternalEncoding encodePreTokenizedWords(
        const std::vector<std::string>& words) const;

    //! Internal: build a Qore encoding hash from an EncodingResult
    QoreHashNode* buildEncodingHash(const EncodingResult& post_result,
        const std::vector<int>& attention_mask, ExceptionSink* xsink) const;

    //! Internal: tokenize a single pre-token through the model
    std::vector<int> tokenizePreToken(const std::string& pre_token) const;

    //! Internal: encode a text segment through normalize + pre-tokenize + model
    InternalEncoding encodeSegment(const std::string& text) const;

    //! Internal: encode text with added token splitting
    InternalEncoding encodeText(const std::string& text) const;
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_HFTOKENIZER_H
