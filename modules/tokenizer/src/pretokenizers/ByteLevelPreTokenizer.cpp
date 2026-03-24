/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ByteLevelPreTokenizer.cpp

    GPT-2 byte-level pre-tokenizer implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "pretokenizers/ByteLevelPreTokenizer.h"
#include "utils/byte_level_mapping.h"

#include <utf8proc.h>

#include <cctype>

namespace QoreTokenizer {

ByteLevelPreTokenizer::ByteLevelPreTokenizer(bool add_prefix_space, bool trim_offsets,
        bool use_regex)
    : add_prefix_space(add_prefix_space), trim_offsets(trim_offsets), use_regex(use_regex) {
}

ByteLevelPreTokenizer::ByteLevelPreTokenizer(const QoreHashNode* config, ExceptionSink* xsink)
    : add_prefix_space(false), trim_offsets(true), use_regex(true) {
    if (!config) {
        return;
    }

    QoreValue v = config->getKeyValue("add_prefix_space");
    if (!v.isNullOrNothing()) {
        add_prefix_space = v.getAsBool();
    }

    // NOTE: "trim_offsets" is parsed from tokenizer configs but not yet
    // implemented — offset trimming has no effect.  The member is retained
    // so the value is available for future implementation.

    v = config->getKeyValue("use_regex");
    if (!v.isNullOrNothing()) {
        use_regex = v.getAsBool();
    }
}

size_t ByteLevelPreTokenizer::utf8CharLen(unsigned char c) {
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
    return 1;
}

bool ByteLevelPreTokenizer::isDigit(const std::string& s, size_t pos, size_t& charLen) {
    if (pos >= s.size()) {
        charLen = 0;
        return false;
    }

    unsigned char c = s[pos];
    charLen = utf8CharLen(c);

    if (pos + charLen > s.size()) {
        charLen = 1;
        return false;
    }

    // Decode the codepoint
    int32_t cp;
    if (charLen == 1) {
        cp = c;
    } else if (charLen == 2) {
        cp = ((c & 0x1F) << 6) | (s[pos + 1] & 0x3F);
    } else if (charLen == 3) {
        cp = ((c & 0x0F) << 12) | ((s[pos + 1] & 0x3F) << 6) | (s[pos + 2] & 0x3F);
    } else {
        cp = ((c & 0x07) << 18) | ((s[pos + 1] & 0x3F) << 12)
            | ((s[pos + 2] & 0x3F) << 6) | (s[pos + 3] & 0x3F);
    }

    // Check Unicode Nd (decimal digit number) category
    return utf8proc_category(cp) == UTF8PROC_CATEGORY_ND;
}

bool ByteLevelPreTokenizer::isSpace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

bool ByteLevelPreTokenizer::isLetter(const std::string& s, size_t pos, size_t& charLen) {
    if (pos >= s.size()) {
        charLen = 0;
        return false;
    }

    unsigned char c = s[pos];
    charLen = utf8CharLen(c);

    if (pos + charLen > s.size()) {
        charLen = 1;
        return false;
    }

    // Decode the codepoint
    int32_t cp;
    if (charLen == 1) {
        cp = c;
    } else if (charLen == 2) {
        cp = ((c & 0x1F) << 6) | (s[pos + 1] & 0x3F);
    } else if (charLen == 3) {
        cp = ((c & 0x0F) << 12) | ((s[pos + 1] & 0x3F) << 6) | (s[pos + 2] & 0x3F);
    } else {
        cp = ((c & 0x07) << 18) | ((s[pos + 1] & 0x3F) << 12)
            | ((s[pos + 2] & 0x3F) << 6) | (s[pos + 3] & 0x3F);
    }

    // Use utf8proc to check if it's a letter
    utf8proc_category_t cat = utf8proc_category(cp);
    switch (cat) {
        case UTF8PROC_CATEGORY_LU:  // Uppercase letter
        case UTF8PROC_CATEGORY_LL:  // Lowercase letter
        case UTF8PROC_CATEGORY_LT:  // Titlecase letter
        case UTF8PROC_CATEGORY_LM:  // Modifier letter
        case UTF8PROC_CATEGORY_LO:  // Other letter
            return true;
        default:
            return false;
    }
}

//! Helper: case-insensitive prefix match
static bool matchCaseInsensitive(const std::string& s, size_t pos, const char* target,
        size_t targetLen) {
    if (pos + targetLen > s.size()) {
        return false;
    }
    for (size_t i = 0; i < targetLen; ++i) {
        char c = s[pos + i];
        // Convert to lowercase for comparison
        if (c >= 'A' && c <= 'Z') {
            c = c + ('a' - 'A');
        }
        if (c != target[i]) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> ByteLevelPreTokenizer::gpt2Split(const std::string& input) const {
    std::vector<std::string> tokens;
    if (input.empty()) {
        return tokens;
    }

    size_t i = 0;
    while (i < input.size()) {
        // Try to match contractions: 's, 't, 're, 've, 'm, 'll, 'd
        // preceded by optional space
        bool matched = false;

        // Check for optional leading space before contraction
        size_t start = i;
        size_t cpos = i;
        if (cpos < input.size() && input[cpos] == ' ') {
            cpos++;
        }

        // Try contractions (case insensitive)
        if (cpos < input.size() && input[cpos] == '\'') {
            // Check 3-char contractions: 're, 've, 'll
            if (matchCaseInsensitive(input, cpos, "'re", 3)) {
                tokens.push_back(input.substr(start, (cpos - start) + 3));
                i = cpos + 3;
                matched = true;
            } else if (matchCaseInsensitive(input, cpos, "'ve", 3)) {
                tokens.push_back(input.substr(start, (cpos - start) + 3));
                i = cpos + 3;
                matched = true;
            } else if (matchCaseInsensitive(input, cpos, "'ll", 3)) {
                tokens.push_back(input.substr(start, (cpos - start) + 3));
                i = cpos + 3;
                matched = true;
            // Check 2-char contractions: 's, 't, 'm, 'd
            } else if (matchCaseInsensitive(input, cpos, "'s", 2)) {
                tokens.push_back(input.substr(start, (cpos - start) + 2));
                i = cpos + 2;
                matched = true;
            } else if (matchCaseInsensitive(input, cpos, "'t", 2)) {
                tokens.push_back(input.substr(start, (cpos - start) + 2));
                i = cpos + 2;
                matched = true;
            } else if (matchCaseInsensitive(input, cpos, "'m", 2)) {
                tokens.push_back(input.substr(start, (cpos - start) + 2));
                i = cpos + 2;
                matched = true;
            } else if (matchCaseInsensitive(input, cpos, "'d", 2)) {
                tokens.push_back(input.substr(start, (cpos - start) + 2));
                i = cpos + 2;
                matched = true;
            }
        }

        if (matched) {
            continue;
        }

        // Try: optional_space + letters+
        {
            size_t tstart = i;
            size_t tpos = i;
            if (tpos < input.size() && input[tpos] == ' ') {
                tpos++;
            }
            size_t charLen;
            if (tpos < input.size() && isLetter(input, tpos, charLen)) {
                tpos += charLen;
                while (tpos < input.size() && isLetter(input, tpos, charLen)) {
                    tpos += charLen;
                }
                tokens.push_back(input.substr(tstart, tpos - tstart));
                i = tpos;
                continue;
            }
        }

        // Try: optional_space + digits+
        {
            size_t tstart = i;
            size_t tpos = i;
            if (tpos < input.size() && input[tpos] == ' ') {
                tpos++;
            }
            size_t dLen;
            if (tpos < input.size() && isDigit(input, tpos, dLen)) {
                tpos += dLen;
                while (tpos < input.size() && isDigit(input, tpos, dLen)) {
                    tpos += dLen;
                }
                tokens.push_back(input.substr(tstart, tpos - tstart));
                i = tpos;
                continue;
            }
        }

        // Try: optional_space + non-letter-non-digit-non-space+
        {
            size_t tstart = i;
            size_t tpos = i;
            if (tpos < input.size() && input[tpos] == ' ') {
                tpos++;
            }
            size_t charLen, dLen2;
            if (tpos < input.size() && !isSpace(input[tpos])
                    && !isDigit(input, tpos, dLen2)
                    && !isLetter(input, tpos, charLen)) {
                // Consume the first non-letter-non-digit-non-space character
                tpos += utf8CharLen(input[tpos]);
                while (tpos < input.size() && !isSpace(input[tpos])
                        && !isDigit(input, tpos, dLen2)
                        && !isLetter(input, tpos, charLen)) {
                    tpos += utf8CharLen(input[tpos]);
                }
                tokens.push_back(input.substr(tstart, tpos - tstart));
                i = tpos;
                continue;
            }
        }

        // Try: spaces not followed by non-space (i.e., spaces at end or spaces before space)
        // and trailing spaces
        if (isSpace(input[i])) {
            size_t tstart = i;
            while (i < input.size() && isSpace(input[i])) {
                // Check if the next character is a non-space
                if (i + 1 < input.size() && !isSpace(input[i + 1])) {
                    // This space is followed by a non-space; stop here
                    // (it will be consumed as an optional leading space in the next iteration)
                    break;
                }
                i++;
            }
            if (i > tstart) {
                tokens.push_back(input.substr(tstart, i - tstart));
            }
            continue;
        }

        // Fallback: consume one UTF-8 character
        {
            size_t clen = utf8CharLen(input[i]);
            if (i + clen > input.size()) {
                clen = input.size() - i;
            }
            tokens.push_back(input.substr(i, clen));
            i += clen;
        }
    }

    return tokens;
}

std::vector<PreToken> ByteLevelPreTokenizer::pretokenize(const std::string& input) const {
    std::string text = input;

    // If add_prefix_space, prepend a space
    if (add_prefix_space && !text.empty() && text[0] != ' ') {
        text = " " + text;
    }

    std::vector<PreToken> result;

    if (!use_regex) {
        // No regex splitting: convert the whole input through byte-to-unicode
        std::string converted = bytesToUnicode(text);
        if (!converted.empty()) {
            result.push_back({converted, 0, input.size()});
        }
        return result;
    }

    // Apply GPT-2 splitting
    std::vector<std::string> pieces = gpt2Split(text);

    // Track position in the original input for offset calculation
    size_t offset_adjustment = (add_prefix_space && !input.empty() && input[0] != ' ') ? 1 : 0;
    size_t pos = 0;

    for (const auto& piece : pieces) {
        // Convert through byte-to-unicode mapping
        std::string converted = bytesToUnicode(piece);

        // Calculate offsets relative to the original input
        size_t start;
        size_t end;
        if (pos >= offset_adjustment) {
            start = pos - offset_adjustment;
        } else {
            start = 0;
        }
        size_t piece_end = pos + piece.size();
        if (piece_end >= offset_adjustment) {
            end = piece_end - offset_adjustment;
        } else {
            end = 0;
        }

        // Clamp to input bounds
        if (start > input.size()) {
            start = input.size();
        }
        if (end > input.size()) {
            end = input.size();
        }

        if (!converted.empty()) {
            result.push_back({converted, start, end});
        }

        pos += piece.size();
    }

    return result;
}

} // namespace QoreTokenizer
