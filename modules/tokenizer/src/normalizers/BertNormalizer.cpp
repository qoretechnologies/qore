/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    BertNormalizer.cpp

    BERT text normalizer implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "normalizers/BertNormalizer.h"

#include <utf8proc.h>

namespace QoreTokenizer {

BertNormalizer::BertNormalizer(const QoreHashNode* config, ExceptionSink* xsink) {
    if (!config) {
        xsink->raiseException("TOKENIZER-NORMALIZER-ERROR",
            "BertNormalizer config is null");
        return;
    }

    // Read clean_text (default: true)
    QoreValue ct_val = config->getKeyValue("clean_text");
    if (ct_val.getType() == NT_BOOLEAN) {
        clean_text = ct_val.getAsBool();
    }

    // Read handle_chinese_chars (default: true)
    QoreValue hcc_val = config->getKeyValue("handle_chinese_chars");
    if (hcc_val.getType() == NT_BOOLEAN) {
        handle_chinese_chars = hcc_val.getAsBool();
    }

    // Read lowercase (default: true)
    QoreValue lc_val = config->getKeyValue("lowercase");
    if (lc_val.getType() == NT_BOOLEAN) {
        lowercase = lc_val.getAsBool();
    }

    // Read strip_accents (nullable: follows lowercase if not specified)
    QoreValue sa_val = config->getKeyValue("strip_accents");
    if (sa_val.getType() == NT_BOOLEAN) {
        strip_accents = sa_val.getAsBool();
        null_strip_accents = false;
    } else {
        // null or missing: strip_accents follows lowercase
        null_strip_accents = true;
        strip_accents = lowercase;
    }
}

size_t BertNormalizer::utf8CharLen(uint8_t lead_byte) {
    if (lead_byte < 0x80) {
        return 1;
    } else if ((lead_byte & 0xE0) == 0xC0) {
        return 2;
    } else if ((lead_byte & 0xF0) == 0xE0) {
        return 3;
    } else if ((lead_byte & 0xF8) == 0xF0) {
        return 4;
    }
    return 1; // invalid byte, treat as single byte
}

int32_t BertNormalizer::decodeUtf8(const uint8_t* data, size_t len) {
    if (len == 0) {
        return -1;
    }
    utf8proc_int32_t cp;
    utf8proc_ssize_t ret = utf8proc_iterate(data, static_cast<utf8proc_ssize_t>(len), &cp);
    if (ret < 0) {
        return -1;
    }
    return cp;
}

void BertNormalizer::encodeUtf8(int32_t cp, std::string& result) {
    utf8proc_uint8_t buf[4];
    utf8proc_ssize_t len = utf8proc_encode_char(cp, buf);
    if (len > 0) {
        result.append(reinterpret_cast<const char*>(buf), static_cast<size_t>(len));
    }
}

bool BertNormalizer::isControlChar(int32_t cp) {
    // Control characters are < 0x20 except \t (0x09), \n (0x0A), \r (0x0D)
    // Also include U+FFFD and the Unicode "Cc" category characters above 0x7F
    if (cp == 0x09 || cp == 0x0A || cp == 0x0D) {
        return false;
    }
    if (cp >= 0 && cp < 0x20) {
        return true;
    }
    // Also treat DEL (0x7F) as control
    if (cp == 0x7F) {
        return true;
    }
    // Check Unicode Cc (control) category for characters > 0x7F
    if (cp > 0x7F) {
        utf8proc_category_t cat = utf8proc_category(cp);
        return cat == UTF8PROC_CATEGORY_CC;
    }
    return false;
}

bool BertNormalizer::isCjkCharacter(int32_t cp) {
    // CJK Unified Ideographs: U+4E00-U+9FFF
    if (cp >= 0x4E00 && cp <= 0x9FFF) {
        return true;
    }
    // CJK Unified Ideographs Extension A: U+3400-U+4DBF
    if (cp >= 0x3400 && cp <= 0x4DBF) {
        return true;
    }
    // CJK Unified Ideographs Extension B: U+20000-U+2A6DF
    if (cp >= 0x20000 && cp <= 0x2A6DF) {
        return true;
    }
    // CJK Unified Ideographs Extension C: U+2A700-U+2B73F
    if (cp >= 0x2A700 && cp <= 0x2B73F) {
        return true;
    }
    // CJK Unified Ideographs Extension D: U+2B740-U+2B81F
    if (cp >= 0x2B740 && cp <= 0x2B81F) {
        return true;
    }
    // CJK Unified Ideographs Extension E: U+2B820-U+2CEAF
    if (cp >= 0x2B820 && cp <= 0x2CEAF) {
        return true;
    }
    // CJK Unified Ideographs Extension F: U+2CEB0-U+2EBEF
    if (cp >= 0x2CEB0 && cp <= 0x2EBEF) {
        return true;
    }
    // CJK Compatibility Ideographs: U+F900-U+FAFF
    if (cp >= 0xF900 && cp <= 0xFAFF) {
        return true;
    }
    // CJK Compatibility Ideographs Supplement: U+2F800-U+2FA1F
    if (cp >= 0x2F800 && cp <= 0x2FA1F) {
        return true;
    }
    return false;
}

std::string BertNormalizer::cleanText(const std::string& input) {
    std::string result;
    result.reserve(input.size());

    size_t i = 0;
    while (i < input.size()) {
        size_t char_len = utf8CharLen(static_cast<uint8_t>(input[i]));
        if (i + char_len > input.size()) {
            break;
        }

        int32_t cp = decodeUtf8(reinterpret_cast<const uint8_t*>(input.data() + i), char_len);
        if (cp < 0) {
            // Skip invalid bytes
            ++i;
            continue;
        }

        if (cp == 0) {
            // Skip null character
            ++i;
            continue;
        }

        if (isControlChar(cp)) {
            // Remove control characters
            i += char_len;
            continue;
        }

        // Replace \t, \n, \r with space
        if (cp == 0x09 || cp == 0x0A || cp == 0x0D) {
            result += ' ';
        } else {
            result.append(input, i, char_len);
        }
        i += char_len;
    }

    return result;
}

std::string BertNormalizer::tokenizeChineseChars(const std::string& input) {
    std::string result;
    result.reserve(input.size() * 2);

    size_t i = 0;
    while (i < input.size()) {
        size_t char_len = utf8CharLen(static_cast<uint8_t>(input[i]));
        if (i + char_len > input.size()) {
            break;
        }

        int32_t cp = decodeUtf8(reinterpret_cast<const uint8_t*>(input.data() + i), char_len);
        if (cp < 0) {
            result += input[i];
            ++i;
            continue;
        }

        if (isCjkCharacter(cp)) {
            result += ' ';
            result.append(input, i, char_len);
            result += ' ';
        } else {
            result.append(input, i, char_len);
        }
        i += char_len;
    }

    return result;
}

std::string BertNormalizer::lowercaseText(const std::string& input) {
    utf8proc_uint8_t* result = nullptr;
    utf8proc_ssize_t len = utf8proc_map(
        reinterpret_cast<const utf8proc_uint8_t*>(input.c_str()),
        static_cast<utf8proc_ssize_t>(input.size()),
        &result,
        static_cast<utf8proc_option_t>(UTF8PROC_CASEFOLD | UTF8PROC_STABLE));

    if (len < 0 || !result) {
        return input;
    }

    std::string output(reinterpret_cast<const char*>(result), static_cast<size_t>(len));
    free(result);
    return output;
}

std::string BertNormalizer::stripAccentMarks(const std::string& input) {
    // First apply NFD decomposition
    utf8proc_uint8_t* nfd_result = nullptr;
    utf8proc_ssize_t nfd_len = utf8proc_map(
        reinterpret_cast<const utf8proc_uint8_t*>(input.c_str()),
        static_cast<utf8proc_ssize_t>(input.size()),
        &nfd_result,
        static_cast<utf8proc_option_t>(UTF8PROC_DECOMPOSE | UTF8PROC_STABLE));

    if (nfd_len < 0 || !nfd_result) {
        return input;
    }

    std::string nfd_str(reinterpret_cast<const char*>(nfd_result), static_cast<size_t>(nfd_len));
    free(nfd_result);

    // Now iterate codepoints and remove combining marks (category Mn)
    std::string result;
    result.reserve(nfd_str.size());

    size_t i = 0;
    while (i < nfd_str.size()) {
        size_t char_len = utf8CharLen(static_cast<uint8_t>(nfd_str[i]));
        if (i + char_len > nfd_str.size()) {
            break;
        }

        int32_t cp = decodeUtf8(reinterpret_cast<const uint8_t*>(nfd_str.data() + i), char_len);
        if (cp >= 0) {
            utf8proc_category_t cat = utf8proc_category(cp);
            if (cat != UTF8PROC_CATEGORY_MN) {
                result.append(nfd_str, i, char_len);
            }
        }
        i += char_len;
    }

    return result;
}

std::string BertNormalizer::normalize(const std::string& input) const {
    std::string result = input;

    if (clean_text) {
        result = cleanText(result);
    }

    if (handle_chinese_chars) {
        result = tokenizeChineseChars(result);
    }

    // Determine effective strip_accents: if null, follows lowercase
    bool effective_strip_accents = null_strip_accents ? lowercase : strip_accents;

    if (effective_strip_accents) {
        result = stripAccentMarks(result);
    }

    if (lowercase) {
        result = lowercaseText(result);
    }

    return result;
}

} // namespace QoreTokenizer
