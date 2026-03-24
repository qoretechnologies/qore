/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SequencePreTokenizer.cpp

    Sequence pre-tokenizer implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "pretokenizers/SequencePreTokenizer.h"
#include "utils/qore_helpers.h"

using namespace QoreTokenizer;

namespace QoreTokenizer {

SequencePreTokenizer::SequencePreTokenizer(
        std::vector<std::unique_ptr<AbstractPreTokenizer>>&& pretokenizers)
    : pretokenizers(std::move(pretokenizers)) {
}

SequencePreTokenizer::SequencePreTokenizer(const QoreHashNode* config, ExceptionSink* xsink) {
    if (!config) {
        xsink->raiseException("PRETOKENIZER-CONFIG-ERROR",
            "Sequence pre-tokenizer config is null");
        return;
    }

    // Read the "pretokenizers" key (note: NOT "pre_tokenizers")
    QoreValue list_val = config->getKeyValue("pretokenizers");
    if (list_val.isNullOrNothing()) {
        xsink->raiseException("PRETOKENIZER-CONFIG-ERROR",
            "Sequence pre-tokenizer config missing 'pretokenizers' key");
        return;
    }

    const QoreListNode* list = safeGetList(list_val);
    if (!list) {
        xsink->raiseException("PRETOKENIZER-CONFIG-ERROR",
            "Sequence pre-tokenizer 'pretokenizers' is not a list");
        return;
    }

    for (size_t i = 0; i < list->size(); ++i) {
        QoreValue elem = list->retrieveEntry(i);
        const QoreHashNode* elem_hash = safeGetHash(elem);
        if (!elem_hash) {
            xsink->raiseException("PRETOKENIZER-CONFIG-ERROR",
                "Sequence pre-tokenizer element %zu is not a hash", i);
            return;
        }

        auto pt = AbstractPreTokenizer::fromConfig(elem_hash, xsink);
        if (*xsink) {
            return;
        }
        if (!pt) {
            xsink->raiseException("PRETOKENIZER-CONFIG-ERROR",
                "failed to create Sequence pre-tokenizer element %zu", i);
            return;
        }
        pretokenizers.push_back(std::move(pt));
    }
}

std::vector<PreToken> SequencePreTokenizer::pretokenize(const std::string& input) const {
    if (pretokenizers.empty()) {
        if (input.empty()) {
            return {};
        }
        return {{input, 0, input.size()}};
    }

    // Apply the first pre-tokenizer to the input
    std::vector<PreToken> current = pretokenizers[0]->pretokenize(input);

    // Apply each subsequent pre-tokenizer to each piece from the previous step
    for (size_t i = 1; i < pretokenizers.size(); ++i) {
        std::vector<PreToken> next;

        for (const auto& token : current) {
            std::vector<PreToken> sub_tokens = pretokenizers[i]->pretokenize(token.text);

            // Adjust offsets: sub_tokens have offsets relative to token.text,
            // but we need offsets relative to the original input
            for (auto& sub : sub_tokens) {
                sub.start += token.start;
                sub.end = std::min(sub.end + token.start, token.end);
                next.push_back(std::move(sub));
            }
        }

        current = std::move(next);
    }

    return current;
}

} // namespace QoreTokenizer
