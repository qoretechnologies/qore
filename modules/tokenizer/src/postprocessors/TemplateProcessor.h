/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    TemplateProcessor.h

    Template-based post-processor and BertProcessing/RobertaProcessing

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_TEMPLATE_PROCESSOR_H
#define _QORE_TOKENIZER_TEMPLATE_PROCESSOR_H

#include "postprocessors/AbstractPostProcessor.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace QoreTokenizer {

//! A piece in a template: either a Sequence reference or a SpecialToken
struct TemplatePiece {
    enum Type {
        SEQUENCE,       //!< Reference to input sequence A or B
        SPECIAL_TOKEN   //!< Reference to a special token
    };

    Type type;

    //! For SEQUENCE: "A" or "B"; for SPECIAL_TOKEN: the token key (e.g., "[CLS]")
    std::string id;

    //! type_id to assign to tokens from this piece
    int type_id = 0;
};

//! Special token info: its IDs and token strings
struct SpecialTokenInfo {
    std::vector<int> ids;
    std::vector<std::string> tokens;
};

//! Template-based post-processor
/** Supports arbitrary templates for single and pair sequences with
    configurable special tokens.
*/
class TemplateProcessor : public AbstractPostProcessor {
public:
    //! Constructor: reads config
    /** @param config the Qore hash with single, pair, and special_tokens
        @param xsink exception sink
    */
    TemplateProcessor(const QoreHashNode* config, ExceptionSink* xsink);

    EncodingResult process(
        const std::vector<int>& ids_a,
        const std::vector<int>& ids_b,
        const std::vector<std::string>& tokens_a,
        const std::vector<std::string>& tokens_b,
        bool add_special) const override;

    EncodingResult processWithOffsets(
        const std::vector<int>& ids_a,
        const std::vector<int>& ids_b,
        const std::vector<std::string>& tokens_a,
        const std::vector<std::string>& tokens_b,
        const std::vector<std::pair<size_t, size_t>>& offsets_a,
        const std::vector<std::pair<size_t, size_t>>& offsets_b,
        bool add_special) const override;

    int numAddedTokens(bool has_pair) const override;

protected:
    //! Constructor for subclasses that set up templates directly
    TemplateProcessor() = default;

    //! template for single sequence
    std::vector<TemplatePiece> single_template;

    //! template for pair of sequences
    std::vector<TemplatePiece> pair_template;

    //! special token definitions: key -> {ids, tokens}
    std::unordered_map<std::string, SpecialTokenInfo> special_tokens;

private:
    //! Parses a template from the config list
    static std::vector<TemplatePiece> parseTemplate(const QoreListNode* tmpl_list,
        ExceptionSink* xsink);
};

//! BERT post-processor: [CLS] A [SEP] / [CLS] A [SEP] B [SEP]
class BertProcessor : public TemplateProcessor {
public:
    //! Constructor: reads config for [CLS] and [SEP] tokens
    /** @param config the Qore hash with sep and cls token definitions
        @param xsink exception sink
    */
    BertProcessor(const QoreHashNode* config, ExceptionSink* xsink);
};

//! RoBERTa post-processor: <s> A </s> / <s> A </s></s> B </s>
class RobertaProcessor : public TemplateProcessor {
public:
    //! Constructor: reads config for <s> and </s> tokens
    /** @param config the Qore hash with sep and cls token definitions
        @param xsink exception sink
    */
    RobertaProcessor(const QoreHashNode* config, ExceptionSink* xsink);
};

//! Byte-level post-processor: no-op passthrough
class ByteLevelPostProcessor : public AbstractPostProcessor {
public:
    EncodingResult process(
        const std::vector<int>& ids_a,
        const std::vector<int>& ids_b,
        const std::vector<std::string>& tokens_a,
        const std::vector<std::string>& tokens_b,
        bool add_special) const override;
};

//! Sequence post-processor: applies multiple post-processors in order
class SequencePostProcessor : public AbstractPostProcessor {
public:
    //! Constructor: reads config
    /** @param config the Qore hash with processors list
        @param xsink exception sink
    */
    SequencePostProcessor(const QoreHashNode* config, ExceptionSink* xsink);

    EncodingResult process(
        const std::vector<int>& ids_a,
        const std::vector<int>& ids_b,
        const std::vector<std::string>& tokens_a,
        const std::vector<std::string>& tokens_b,
        bool add_special) const override;

private:
    std::vector<std::unique_ptr<AbstractPostProcessor>> processors;
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_TEMPLATE_PROCESSOR_H
