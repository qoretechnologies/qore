/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    WordPieceDecoder.cpp

    WordPiece token decoder implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "decoders/WordPieceDecoder.h"
#include "utils/qore_helpers.h"

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
        // Remove spaces before punctuation characters
        std::string cleaned;
        cleaned.reserve(result.size());
        for (size_t i = 0; i < result.size(); ++i) {
            if (result[i] == ' ' && i + 1 < result.size() && std::ispunct((unsigned char)result[i + 1])) {
                // Skip the space before punctuation
                continue;
            }
            cleaned += result[i];
        }
        return cleaned;
    }

    return result;
}

} // namespace QoreTokenizer
