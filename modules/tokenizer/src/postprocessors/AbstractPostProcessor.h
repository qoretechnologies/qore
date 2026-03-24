/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    AbstractPostProcessor.h

    Abstract base class for post-processors

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_ABSTRACT_POST_PROCESSOR_H
#define _QORE_TOKENIZER_ABSTRACT_POST_PROCESSOR_H

#include <string>
#include <vector>
#include <memory>

#include "qore/Qore.h"

namespace QoreTokenizer {

//! Result of post-processing: token IDs, type IDs, and token strings
struct EncodingResult {
    //! token IDs
    std::vector<int> ids;

    //! type IDs (0 for sequence A, 1 for sequence B, etc.)
    std::vector<int> type_ids;

    //! token strings
    std::vector<std::string> tokens;

    //! byte-level offsets (start, end) relative to original input
    std::vector<std::pair<size_t, size_t>> offsets;

    //! special tokens mask (1 = special, 0 = content)
    std::vector<int> special_tokens_mask;
};

//! Abstract base class for post-processors
/** Post-processors add special tokens (e.g., [CLS], [SEP], <s>, </s>) to
    the encoded output and assign type IDs for sequence pair classification.
*/
class AbstractPostProcessor {
public:
    virtual ~AbstractPostProcessor() = default;

    //! Processes encoded sequences, adding special tokens
    /** @param ids_a token IDs for sequence A
        @param ids_b token IDs for sequence B (empty if single sequence)
        @param tokens_a token strings for sequence A
        @param tokens_b token strings for sequence B (empty if single sequence)
        @param add_special whether to add special tokens
        @return the post-processed encoding result
    */
    virtual EncodingResult process(
        const std::vector<int>& ids_a,
        const std::vector<int>& ids_b,
        const std::vector<std::string>& tokens_a,
        const std::vector<std::string>& tokens_b,
        bool add_special) const = 0;

    //! Extended process with offset and special token mask propagation
    /** Default implementation calls process() then populates offsets and mask.
        Special tokens get offset (0,0) and mask 1; sequence tokens get their
        offsets from offsets_a/offsets_b and mask 0.
    */
    virtual EncodingResult processWithOffsets(
        const std::vector<int>& ids_a,
        const std::vector<int>& ids_b,
        const std::vector<std::string>& tokens_a,
        const std::vector<std::string>& tokens_b,
        const std::vector<std::pair<size_t, size_t>>& offsets_a,
        const std::vector<std::pair<size_t, size_t>>& offsets_b,
        bool add_special) const;

    //! Returns the number of special tokens added for single/pair encoding
    virtual int numAddedTokens(bool has_pair) const { return 0; }

    //! Factory: creates a post-processor from a parsed tokenizer.json "post_processor" config hash
    /** Dispatches on config.type:
        - "TemplateProcessing" -> TemplateProcessor
        - "BertProcessing" -> BertProcessor
        - "RobertaProcessing" -> RobertaProcessor
        - "ByteLevel" -> ByteLevelPostProcessor
        - "Sequence" -> SequencePostProcessor

        @param config the Qore hash from the tokenizer.json "post_processor" key
        @param xsink exception sink
        @return the post-processor, or nullptr on error
    */
    static std::unique_ptr<AbstractPostProcessor> fromConfig(const QoreHashNode* config,
        ExceptionSink* xsink);
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_ABSTRACT_POST_PROCESSOR_H
