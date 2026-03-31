/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    AbstractPostProcessor.cpp

    Abstract base class for post-processors - factory implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "postprocessors/AbstractPostProcessor.h"
#include "postprocessors/TemplateProcessor.h"
#include "utils/qore_helpers.h"

using namespace QoreTokenizer;

namespace QoreTokenizer {

std::unique_ptr<AbstractPostProcessor> AbstractPostProcessor::fromConfig(
        const QoreHashNode* config, ExceptionSink* xsink) {
    if (!config) {
        xsink->raiseException("TOKENIZER-POSTPROCESSOR-ERROR",
            "post_processor config is null");
        return nullptr;
    }

    std::string type = safeGetStringKey(config, "type");
    if (type.empty()) {
        xsink->raiseException("TOKENIZER-POSTPROCESSOR-ERROR",
            "post_processor config missing 'type' field");
        return nullptr;
    }

    if (type == "TemplateProcessing") {
        return std::make_unique<TemplateProcessor>(config, xsink);
    } else if (type == "BertProcessing") {
        return std::make_unique<BertProcessor>(config, xsink);
    } else if (type == "RobertaProcessing") {
        return std::make_unique<RobertaProcessor>(config, xsink);
    } else if (type == "ByteLevel") {
        return std::make_unique<ByteLevelPostProcessor>();
    } else if (type == "Sequence") {
        return std::make_unique<SequencePostProcessor>(config, xsink);
    } else {
        xsink->raiseException("TOKENIZER-POSTPROCESSOR-ERROR",
            "unknown post_processor type '%s'", type.c_str());
        return nullptr;
    }
}

EncodingResult AbstractPostProcessor::processWithOffsets(
        const std::vector<int>& ids_a,
        const std::vector<int>& ids_b,
        const std::vector<std::string>& tokens_a,
        const std::vector<std::string>& tokens_b,
        const std::vector<std::pair<size_t, size_t>>& offsets_a,
        const std::vector<std::pair<size_t, size_t>>& offsets_b,
        const std::vector<int>& word_ids_a,
        const std::vector<int>& word_ids_b,
        bool add_special) const {
    // Call the base process() to get ids, type_ids, tokens
    EncodingResult result = process(ids_a, ids_b, tokens_a, tokens_b, add_special);

    // Default fallback: use type_ids to distinguish sequence A (0) from B (1).
    // Tokens with type_id matching neither consumed A nor B position are
    // assumed to be special tokens.  This works reliably for simple
    // passthrough processors; template-based processors override this method
    // to use the template structure directly.
    size_t a_pos = 0;
    size_t b_pos = 0;
    result.offsets.reserve(result.ids.size());
    result.special_tokens_mask.reserve(result.ids.size());
    result.word_ids.reserve(result.ids.size());

    for (size_t i = 0; i < result.ids.size(); ++i) {
        int type_id = i < result.type_ids.size() ? result.type_ids[i] : 0;

        if (type_id == 0 && a_pos < ids_a.size()) {
            if (a_pos < offsets_a.size()) {
                result.offsets.push_back(offsets_a[a_pos]);
            } else {
                result.offsets.push_back({0, 0});
            }
            result.special_tokens_mask.push_back(0);
            result.word_ids.push_back(
                a_pos < word_ids_a.size() ? word_ids_a[a_pos] : -1);
            ++a_pos;
        } else if (type_id == 1 && b_pos < ids_b.size()) {
            if (b_pos < offsets_b.size()) {
                result.offsets.push_back(offsets_b[b_pos]);
            } else {
                result.offsets.push_back({0, 0});
            }
            result.special_tokens_mask.push_back(0);
            result.word_ids.push_back(
                b_pos < word_ids_b.size() ? word_ids_b[b_pos] : -1);
            ++b_pos;
        } else {
            // Special token
            result.offsets.push_back({0, 0});
            result.special_tokens_mask.push_back(1);
            result.word_ids.push_back(-1);
        }
    }

    return result;
}

} // namespace QoreTokenizer
