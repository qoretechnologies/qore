/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    BertPreTokenizer.cpp

    BERT pre-tokenizer implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "pretokenizers/BertPreTokenizer.h"

#include <utf8proc.h>

namespace QoreTokenizer {

size_t BertPreTokenizer::utf8CharLen(const std::string& s, size_t pos) {
    if (pos >= s.size()) {
        return 0;
    }
    unsigned char c = s[pos];
    if (c < 0x80) {
        return 1;
    }
    if ((c & 0xE0) == 0xC0) {
        return 2;
    }
    if ((c & 0xF0) == 0xE0) {
        return 3;
    }
    if ((c & 0xF8) == 0xF0) {
        return 4;
    }
    // Invalid UTF-8 leading byte; treat as single byte
    return 1;
}

int32_t BertPreTokenizer::decodeUtf8Codepoint(const std::string& s, size_t pos, size_t len) {
    if (len == 0 || pos + len > s.size()) {
        return -1;
    }
    unsigned char c0 = s[pos];
    if (len == 1) {
        return c0;
    }
    if (len == 2) {
        return ((c0 & 0x1F) << 6) | (s[pos + 1] & 0x3F);
    }
    if (len == 3) {
        return ((c0 & 0x0F) << 12) | ((s[pos + 1] & 0x3F) << 6) | (s[pos + 2] & 0x3F);
    }
    if (len == 4) {
        return ((c0 & 0x07) << 18) | ((s[pos + 1] & 0x3F) << 12)
            | ((s[pos + 2] & 0x3F) << 6) | (s[pos + 3] & 0x3F);
    }
    return -1;
}

bool BertPreTokenizer::isUnicodePunctuation(int32_t codepoint) {
    if (codepoint < 0) {
        return false;
    }
    // Use utf8proc to get the Unicode general category
    utf8proc_category_t cat = utf8proc_category(codepoint);
    // Punctuation categories: Pc, Pd, Pe, Pf, Pi, Po, Ps
    switch (cat) {
        case UTF8PROC_CATEGORY_PC:  // Connector punctuation
        case UTF8PROC_CATEGORY_PD:  // Dash punctuation
        case UTF8PROC_CATEGORY_PE:  // Close punctuation
        case UTF8PROC_CATEGORY_PF:  // Final punctuation
        case UTF8PROC_CATEGORY_PI:  // Initial punctuation
        case UTF8PROC_CATEGORY_PO:  // Other punctuation
        case UTF8PROC_CATEGORY_PS:  // Open punctuation
            return true;
        default:
            break;
    }
    // Also treat ASCII symbols that are commonly considered punctuation
    // but may not be in the 'P' Unicode category (e.g., $, +, <, =, >, ^, `, |, ~)
    if (codepoint >= 33 && codepoint <= 47) {
        return true;  // ! " # $ % & ' ( ) * + , - . /
    }
    if (codepoint >= 58 && codepoint <= 64) {
        return true;  // : ; < = > ? @
    }
    if (codepoint >= 91 && codepoint <= 96) {
        return true;  // [ \ ] ^ _ `
    }
    if (codepoint >= 123 && codepoint <= 126) {
        return true;  // { | } ~
    }
    return false;
}

bool BertPreTokenizer::isUnicodeWhitespace(int32_t codepoint) {
    if (codepoint < 0) {
        return false;
    }
    // Standard ASCII whitespace
    if (codepoint == ' ' || codepoint == '\t' || codepoint == '\n'
            || codepoint == '\r' || codepoint == '\f' || codepoint == '\v') {
        return true;
    }
    // Use utf8proc: category Zs (space separator), Zl (line separator), Zp (paragraph separator)
    utf8proc_category_t cat = utf8proc_category(codepoint);
    return cat == UTF8PROC_CATEGORY_ZS || cat == UTF8PROC_CATEGORY_ZL
        || cat == UTF8PROC_CATEGORY_ZP;
}

std::vector<PreToken> BertPreTokenizer::pretokenize(const std::string& input) const {
    std::vector<PreToken> result;
    if (input.empty()) {
        return result;
    }

    size_t i = 0;
    while (i < input.size()) {
        size_t char_len = utf8CharLen(input, i);
        if (char_len == 0) {
            break;
        }
        int32_t cp = decodeUtf8Codepoint(input, i, char_len);

        // Skip whitespace
        if (isUnicodeWhitespace(cp)) {
            i += char_len;
            continue;
        }

        // Punctuation: each punctuation character is its own token
        if (isUnicodePunctuation(cp)) {
            result.push_back({input.substr(i, char_len), i, i + char_len});
            i += char_len;
            continue;
        }

        // Non-punctuation, non-whitespace: accumulate contiguous characters
        size_t start = i;
        i += char_len;
        while (i < input.size()) {
            char_len = utf8CharLen(input, i);
            if (char_len == 0) {
                break;
            }
            cp = decodeUtf8Codepoint(input, i, char_len);
            if (isUnicodeWhitespace(cp) || isUnicodePunctuation(cp)) {
                break;
            }
            i += char_len;
        }
        result.push_back({input.substr(start, i - start), start, i});
    }

    return result;
}

} // namespace QoreTokenizer
