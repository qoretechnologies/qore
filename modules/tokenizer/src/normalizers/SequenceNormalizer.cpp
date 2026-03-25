/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SequenceNormalizer.cpp

    Sequence normalizer implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "normalizers/SequenceNormalizer.h"
#include "utils/qore_helpers.h"

using namespace QoreTokenizer;

namespace QoreTokenizer {

SequenceNormalizer::SequenceNormalizer(const QoreHashNode* config, ExceptionSink* xsink) {
    if (!config) {
        xsink->raiseException("TOKENIZER-NORMALIZER-ERROR",
            "SequenceNormalizer config is null");
        return;
    }

    // Read the "normalizers" list
    const QoreListNode* norms_list = safeGetListKey(config, "normalizers");
    if (!norms_list) {
        xsink->raiseException("TOKENIZER-NORMALIZER-ERROR",
            "SequenceNormalizer config missing 'normalizers' list");
        return;
    }

    size_t count = norms_list->size();
    normalizers.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        const QoreHashNode* norm_config = safeGetHash(norms_list->retrieveEntry(i));
        if (!norm_config) {
            xsink->raiseException("TOKENIZER-NORMALIZER-ERROR",
                "SequenceNormalizer: normalizer at index %zu is not a hash", i);
            return;
        }

        auto norm = AbstractNormalizer::fromConfig(norm_config, xsink);
        if (*xsink) {
            return;
        }
        if (norm) {
            normalizers.push_back(std::move(norm));
        }
    }
}

std::string SequenceNormalizer::normalize(const std::string& input) const {
    std::string result = input;
    for (const auto& norm : normalizers) {
        result = norm->normalize(result);
    }
    return result;
}

} // namespace QoreTokenizer
