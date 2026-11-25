/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    DigitsPreTokenizer.cpp

    Digit-isolating pre-tokenizer

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "DigitsPreTokenizer.h"

#include <utf8proc.h>

namespace QoreTokenizer {

std::vector<PreToken> DigitsPreTokenizer::pretokenize(
        const std::string& text) const {
    std::vector<PreToken> result;
    size_t start = 0;
    bool was_numeric = false;
    for (size_t pos = 0; pos < text.size();) {
        utf8proc_int32_t cp;
        auto length = utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t*>(text.data() + pos),
            text.size() - pos, &cp);
        size_t end = pos + (length > 0 ? length : 1);
        auto category = utf8proc_category(cp);
        bool numeric = category >= UTF8PROC_CATEGORY_ND && category <= UTF8PROC_CATEGORY_NO;
        if (start < pos && (numeric != was_numeric || (numeric && individual))) {
            result.push_back({text.substr(start, pos - start), start, pos});
            start = pos;
        }
        was_numeric = numeric;
        pos = end;
    }
    if (start < text.size()) {
        result.push_back({text.substr(start), start, text.size()});
    }
    return result;
}

} // namespace QoreTokenizer
