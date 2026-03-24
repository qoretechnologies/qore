/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    UnicodeNormalizer.cpp

    Unicode normalization implementation using utf8proc

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "normalizers/UnicodeNormalizer.h"

#include <utf8proc.h>

namespace QoreTokenizer {

UnicodeNormalizer::UnicodeNormalizer(Form form, bool strip_accents_after)
    : form(form), strip_accents_after(strip_accents_after) {
}

UnicodeNormalizer::Form UnicodeNormalizer::parseForm(const std::string& name) {
    if (name == "NFC") {
        return Form::NFC;
    } else if (name == "NFD") {
        return Form::NFD;
    } else if (name == "NFKC") {
        return Form::NFKC;
    } else if (name == "NFKD") {
        return Form::NFKD;
    }
    return Form::NFC;
}

std::string UnicodeNormalizer::applyNormalization(const std::string& input) const {
    utf8proc_option_t options = UTF8PROC_STABLE;

    switch (form) {
        case Form::NFC:
            options = static_cast<utf8proc_option_t>(UTF8PROC_COMPOSE | UTF8PROC_STABLE);
            break;
        case Form::NFD:
            options = static_cast<utf8proc_option_t>(UTF8PROC_DECOMPOSE | UTF8PROC_STABLE);
            break;
        case Form::NFKC:
            options = static_cast<utf8proc_option_t>(
                UTF8PROC_COMPOSE | UTF8PROC_COMPAT | UTF8PROC_STABLE);
            break;
        case Form::NFKD:
            options = static_cast<utf8proc_option_t>(
                UTF8PROC_DECOMPOSE | UTF8PROC_COMPAT | UTF8PROC_STABLE);
            break;
    }

    utf8proc_uint8_t* result = nullptr;
    utf8proc_ssize_t len = utf8proc_map(
        reinterpret_cast<const utf8proc_uint8_t*>(input.c_str()),
        static_cast<utf8proc_ssize_t>(input.size()),
        &result,
        options);

    if (len < 0 || !result) {
        return input;
    }

    std::string output(reinterpret_cast<const char*>(result), static_cast<size_t>(len));
    free(result);
    return output;
}

std::string UnicodeNormalizer::stripCombiningMarks(const std::string& input) {
    std::string result;
    result.reserve(input.size());

    size_t i = 0;
    while (i < input.size()) {
        // Determine UTF-8 character length
        uint8_t lead = static_cast<uint8_t>(input[i]);
        size_t char_len;
        if (lead < 0x80) {
            char_len = 1;
        } else if ((lead & 0xE0) == 0xC0) {
            char_len = 2;
        } else if ((lead & 0xF0) == 0xE0) {
            char_len = 3;
        } else if ((lead & 0xF8) == 0xF0) {
            char_len = 4;
        } else {
            char_len = 1; // invalid byte
        }

        if (i + char_len > input.size()) {
            break;
        }

        // Decode codepoint using utf8proc
        utf8proc_int32_t cp;
        utf8proc_ssize_t ret = utf8proc_iterate(
            reinterpret_cast<const utf8proc_uint8_t*>(input.data() + i),
            static_cast<utf8proc_ssize_t>(char_len), &cp);

        if (ret > 0 && cp >= 0) {
            utf8proc_category_t cat = utf8proc_category(cp);
            if (cat != UTF8PROC_CATEGORY_MN) {
                result.append(input, i, char_len);
            }
        } else {
            // Pass through invalid sequences
            result.append(input, i, char_len);
        }

        i += char_len;
    }

    return result;
}

std::string UnicodeNormalizer::normalize(const std::string& input) const {
    std::string result = applyNormalization(input);

    if (strip_accents_after) {
        // For StripAccents: NFD was already applied, now strip combining marks
        result = stripCombiningMarks(result);
    }

    return result;
}

} // namespace QoreTokenizer
