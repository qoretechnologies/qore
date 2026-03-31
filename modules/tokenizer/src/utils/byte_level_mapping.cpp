/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    byte_level_mapping.cpp

    GPT-2 byte-level BPE byte <-> Unicode mapping implementation

    The GPT-2 tokenizer maps each byte value (0-255) to a visible Unicode
    character to avoid control characters in the vocabulary. Printable ASCII
    bytes map to themselves; non-printable bytes map to Unicode codepoints
    starting at U+0100.

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "utils/byte_level_mapping.h"

#include <cstring>

namespace QoreTokenizer {

// Build the byte-to-unicode mapping following GPT-2's bytes_to_unicode()
static std::unordered_map<uint8_t, std::string> buildByteToUnicode() {
    std::unordered_map<uint8_t, std::string> mapping;

    // Printable ASCII ranges that map to themselves:
    // '!' (33) - '~' (126), '¡' (161) - '¬' (172), '®' (174) - 'ÿ' (255)
    int n = 0;
    for (int b = 0; b < 256; ++b) {
        bool direct = false;
        if (b >= 33 && b <= 126) {
            direct = true;
        } else if (b >= 161 && b <= 172) {
            direct = true;
        } else if (b >= 174 && b <= 255) {
            direct = true;
        }

        char utf8[4];
        if (direct) {
            // Map to same codepoint
            if (b < 128) {
                utf8[0] = (char)b;
                utf8[1] = 0;
            } else {
                // Encode as UTF-8 (b is in 128-255 range = 2-byte UTF-8)
                utf8[0] = (char)(0xC0 | (b >> 6));
                utf8[1] = (char)(0x80 | (b & 0x3F));
                utf8[2] = 0;
            }
        } else {
            // Map to U+0100 + n (all in 2-byte UTF-8 range U+0100 - U+017F)
            int cp = 256 + n;
            utf8[0] = (char)(0xC0 | (cp >> 6));
            utf8[1] = (char)(0x80 | (cp & 0x3F));
            utf8[2] = 0;
            ++n;
        }
        mapping[b] = std::string(utf8);
    }

    return mapping;
}

static std::unordered_map<std::string, uint8_t> buildUnicodeToByte() {
    auto b2u = buildByteToUnicode();
    std::unordered_map<std::string, uint8_t> mapping;
    for (auto& p : b2u) {
        mapping[p.second] = p.first;
    }
    return mapping;
}

const std::unordered_map<uint8_t, std::string>& getByteToUnicode() {
    static auto mapping = buildByteToUnicode();
    return mapping;
}

const std::unordered_map<std::string, uint8_t>& getUnicodeToByte() {
    static auto mapping = buildUnicodeToByte();
    return mapping;
}

std::string bytesToUnicode(const std::string& input) {
    auto& b2u = getByteToUnicode();
    std::string result;
    result.reserve(input.size() * 2);
    for (uint8_t b : input) {
        auto it = b2u.find(b);
        if (it != b2u.end()) {
            result += it->second;
        }
    }
    return result;
}

std::string unicodeToBytes(const std::string& input) {
    auto& u2b = getUnicodeToByte();
    std::string result;
    result.reserve(input.size());

    size_t i = 0;
    while (i < input.size()) {
        // Determine UTF-8 character length
        uint8_t c = input[i];
        int len = 1;
        if ((c & 0xE0) == 0xC0) {
            len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            len = 3;
        } else if ((c & 0xF8) == 0xF0) {
            len = 4;
        }

        if (i + len > input.size()) {
            break;
        }

        std::string ch = input.substr(i, len);
        auto it = u2b.find(ch);
        if (it != u2b.end()) {
            result += (char)it->second;
        } else {
            // Pass through unmapped characters as-is
            result += ch;
        }
        i += len;
    }

    return result;
}

} // namespace QoreTokenizer
