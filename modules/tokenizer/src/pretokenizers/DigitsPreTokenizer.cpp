/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    DigitsPreTokenizer.cpp

    Digit-isolating pre-tokenizer

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "DigitsPreTokenizer.h"

namespace QoreTokenizer {

std::vector<PreToken> DigitsPreTokenizer::pretokenize(
        const std::string& text) const {
    std::vector<PreToken> result;
    if (text.empty()) return result;

    std::string current;
    size_t current_start = 0;
    bool in_digits = false;

    for (size_t i = 0; i < text.size(); ++i) {
        bool is_digit = (text[i] >= '0' && text[i] <= '9');

        if (is_digit != in_digits && !current.empty()) {
            result.push_back({current, current_start});
            current.clear();
        }

        if (is_digit && individual) {
            // Each digit is a separate pre-token
            if (!current.empty() && !in_digits) {
                result.push_back({current, current_start});
                current.clear();
            }
            result.push_back({std::string(1, text[i]), i});
            in_digits = true;
            current_start = i + 1;
            continue;
        }

        if (current.empty()) {
            current_start = i;
        }
        current += text[i];
        in_digits = is_digit;
    }

    if (!current.empty()) {
        result.push_back({current, current_start});
    }

    return result;
}

} // namespace QoreTokenizer
