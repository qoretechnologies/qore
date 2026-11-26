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
            // Hugging Face passes the whole token through if any character is unmapped.
            return input;
        }
        i += len;
    }

    return result;
}

// returns the decoded bytes as a UTF-8 string, each invalid byte sequence replaced by U+FFFD
/* Decoding a sequence of tokens that ends or starts in the middle of a character (ex: the first byte-level token of
   an emoji, or a ByteFallback <0xF0> token alone) gives bytes that are not valid UTF-8.  As in Hugging Face
   tokenizers, which decode with Rust's String::from_utf8_lossy(), each maximal subpart of an invalid sequence (the
   longest start of a valid sequence, or a single byte that cannot start one) is replaced by one U+FFFD (the
   "substitution of maximal subparts" of the Unicode standard, chapter 3.9); the valid text is kept as it is
   (qore#6604)
*/
std::string decodedUtf8(const std::string& bytes) {
    static const char replacement[] = "\xef\xbf\xbd";
    const unsigned char* p = reinterpret_cast<const unsigned char*>(bytes.data());
    size_t len = bytes.size();
    std::string out;
    out.reserve(len);
    size_t i = 0;
    while (i < len) {
        unsigned char b = p[i];
        if (b < 0x80) {
            out += static_cast<char>(b);
            ++i;
            continue;
        }
        // the length of the sequence and the range of its second byte (RFC 3629, Unicode Table 3-7)
        size_t n;
        unsigned char lo = 0x80, hi = 0xbf;
        if (b >= 0xc2 && b <= 0xdf) {
            n = 2;
        } else if (b >= 0xe0 && b <= 0xef) {
            n = 3;
            if (b == 0xe0) {
                lo = 0xa0;
            } else if (b == 0xed) {
                hi = 0x9f;
            }
        } else if (b >= 0xf0 && b <= 0xf4) {
            n = 4;
            if (b == 0xf0) {
                lo = 0x90;
            } else if (b == 0xf4) {
                hi = 0x8f;
            }
        } else {
            // a continuation byte, an overlong lead byte (C0, C1) or a byte above F4
            out.append(replacement, 3);
            ++i;
            continue;
        }
        // the number of bytes of a valid sequence that are there
        size_t k = 1;
        while (k < n && i + k < len) {
            unsigned char c = p[i + k];
            if (k == 1 ? (c < lo || c > hi) : (c < 0x80 || c > 0xbf)) {
                break;
            }
            ++k;
        }
        if (k == n) {
            out.append(bytes, i, n);
        } else {
            out.append(replacement, 3);
        }
        i += k;
    }
    return out;
}

} // namespace QoreTokenizer
