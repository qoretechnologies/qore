/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    PunctuationPreTokenizer.cpp

    Punctuation-isolating pre-tokenizer

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "PunctuationPreTokenizer.h"

#include "BertPreTokenizer.h"
#include <utf8proc.h>

namespace QoreTokenizer {

std::vector<PreToken> PunctuationPreTokenizer::pretokenize(
        const std::string& text) const {
    std::vector<PreToken> result;
    size_t start = 0;
    for (size_t pos = 0; pos < text.size();) {
        utf8proc_int32_t cp;
        auto length = utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t*>(text.data() + pos),
            text.size() - pos, &cp);
        size_t end = pos + (length > 0 ? length : 1);
        if (BertPreTokenizer::isUnicodePunctuation(cp)) {
            if (start < pos) {
                result.push_back({text.substr(start, pos - start), start, pos});
            }
            result.push_back({text.substr(pos, end - pos), pos, end});
            start = end;
        }
        pos = end;
    }
    if (start < text.size()) {
        result.push_back({text.substr(start), start, text.size()});
    }
    return result;
}

} // namespace QoreTokenizer
