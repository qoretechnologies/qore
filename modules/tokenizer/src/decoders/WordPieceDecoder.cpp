/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    WordPieceDecoder.cpp

    WordPiece token decoder implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "decoders/WordPieceDecoder.h"
#include "utils/qore_helpers.h"

#include <utf8proc.h>

#include <cassert>
#include <cctype>

using namespace QoreTokenizer;

namespace QoreTokenizer {

WordPieceDecoder::WordPieceDecoder(const QoreHashNode* config, ExceptionSink* xsink)
        : prefix("##"), cleanup(true) {
    assert(config);

    std::string prefix_str = safeGetStringKey(config, "prefix");
    if (!prefix_str.empty()) {
        prefix = prefix_str;
    }

    QoreValue cleanup_val = config->getKeyValue("cleanup");
    if (cleanup_val.getType() == NT_BOOLEAN || cleanup_val.getType() == NT_INT) {
        cleanup = cleanup_val.getAsBool();
    }
}

std::string WordPieceDecoder::decode(const std::vector<std::string>& tokens) const {
    std::string result;

    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& tok = tokens[i];

        // Check if token starts with the continuation prefix
        if (!prefix.empty() && tok.size() > prefix.size()
                && tok.compare(0, prefix.size(), prefix) == 0) {
            // Strip the prefix and append without space
            result += tok.substr(prefix.size());
        } else {
            // First token or non-continuation: add space separator (except for first token)
            if (i > 0) {
                result += ' ';
            }
            result += tok;
        }
    }

    if (cleanup) {
        // Remove spaces before punctuation characters (Unicode-aware)
        std::string cleaned;
        cleaned.reserve(result.size());
        for (size_t i = 0; i < result.size(); ++i) {
            if (result[i] == ' ' && i + 1 < result.size()) {
                // Decode the UTF-8 character after the space
                unsigned char c = result[i + 1];
                bool is_punct = false;
                if (c < 0x80) {
                    is_punct = std::ispunct(c);
                } else {
                    // Decode multi-byte UTF-8 codepoint
                    utf8proc_int32_t cp;
                    utf8proc_ssize_t len = utf8proc_iterate(
                        reinterpret_cast<const utf8proc_uint8_t*>(result.data() + i + 1),
                        result.size() - i - 1, &cp);
                    if (len > 0 && cp >= 0) {
                        utf8proc_category_t cat = utf8proc_category(cp);
                        is_punct = (cat == UTF8PROC_CATEGORY_PC
                            || cat == UTF8PROC_CATEGORY_PD
                            || cat == UTF8PROC_CATEGORY_PE
                            || cat == UTF8PROC_CATEGORY_PF
                            || cat == UTF8PROC_CATEGORY_PI
                            || cat == UTF8PROC_CATEGORY_PO
                            || cat == UTF8PROC_CATEGORY_PS);
                    }
                }
                if (is_punct) {
                    // Skip the space before punctuation
                    continue;
                }
            }
            cleaned += result[i];
        }
        return cleaned;
    }

    return result;
}

} // namespace QoreTokenizer
