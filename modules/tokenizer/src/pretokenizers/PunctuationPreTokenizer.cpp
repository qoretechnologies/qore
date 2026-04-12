/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    PunctuationPreTokenizer.cpp

    Punctuation-isolating pre-tokenizer

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "PunctuationPreTokenizer.h"

#include <cctype>

namespace QoreTokenizer {

static bool isPunctuation(char c) {
    // ASCII punctuation characters
    return (c >= '!' && c <= '/') || (c >= ':' && c <= '@')
        || (c >= '[' && c <= '`') || (c >= '{' && c <= '~');
}

std::vector<PreToken> PunctuationPreTokenizer::pretokenize(
        const std::string& text) const {
    std::vector<PreToken> result;
    if (text.empty()) return result;

    std::string current;
    size_t current_start = 0;

    for (size_t i = 0; i < text.size(); ++i) {
        if (isPunctuation(text[i])) {
            // Flush current non-punctuation segment
            if (!current.empty()) {
                result.push_back({current, current_start});
                current.clear();
            }
            // Add punctuation as its own pre-token
            result.push_back({std::string(1, text[i]), i});
            current_start = i + 1;
        } else {
            if (current.empty()) {
                current_start = i;
            }
            current += text[i];
        }
    }

    // Flush remaining
    if (!current.empty()) {
        result.push_back({current, current_start});
    }

    return result;
}

} // namespace QoreTokenizer
