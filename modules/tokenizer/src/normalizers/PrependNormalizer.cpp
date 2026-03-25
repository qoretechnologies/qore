/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    PrependNormalizer.cpp

    Prepend normalizer implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "normalizers/PrependNormalizer.h"
#include "utils/qore_helpers.h"

using namespace QoreTokenizer;

namespace QoreTokenizer {

PrependNormalizer::PrependNormalizer(const QoreHashNode* config, ExceptionSink* xsink) {
    if (!config) {
        xsink->raiseException("TOKENIZER-NORMALIZER-ERROR",
            "PrependNormalizer config is null");
        return;
    }

    std::string prepend_str = safeGetStringKey(config, "prepend");
    if (prepend_str.empty()) {
        xsink->raiseException("TOKENIZER-NORMALIZER-ERROR",
            "PrependNormalizer config missing 'prepend' field");
        return;
    }

    prepend = prepend_str;
}

std::string PrependNormalizer::normalize(const std::string& input) const {
    return prepend + input;
}

} // namespace QoreTokenizer
