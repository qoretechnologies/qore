/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ReplaceNormalizer.cpp

    Replace normalizer implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "normalizers/ReplaceNormalizer.h"
#include "utils/qore_helpers.h"

using namespace QoreTokenizer;

namespace QoreTokenizer {

ReplaceNormalizer::ReplaceNormalizer(const QoreHashNode* config, ExceptionSink* xsink) {
    if (!config) {
        xsink->raiseException("TOKENIZER-NORMALIZER-ERROR",
            "ReplaceNormalizer config is null");
        return;
    }

    // Read the "pattern" key: expected format is {"String": "..."} or {"Regex": "..."}
    // For now, only String patterns are supported
    const QoreHashNode* pattern_hash = safeGetHashKey(config, "pattern");
    if (pattern_hash) {
        // Try to get "String" key from the pattern hash
        std::string str_val = safeGetStringKey(pattern_hash, "String");
        if (!str_val.empty()) {
            pattern = str_val;
        } else {
            // Check if it's a Regex type (not yet supported)
            std::string regex_val = safeGetStringKey(pattern_hash, "Regex");
            if (!regex_val.empty()) {
                xsink->raiseException("TOKENIZER-NORMALIZER-ERROR",
                    "ReplaceNormalizer: Regex patterns are not yet supported");
                return;
            } else {
                xsink->raiseException("TOKENIZER-NORMALIZER-ERROR",
                    "ReplaceNormalizer: pattern hash has no 'String' or 'Regex' key");
                return;
            }
        }
    } else {
        // Maybe it's a plain string
        const QoreStringNode* str_node = safeGetString(config->getKeyValue("pattern"));
        if (str_node) {
            pattern = str_node->c_str();
        } else {
            xsink->raiseException("TOKENIZER-NORMALIZER-ERROR",
                "ReplaceNormalizer: missing or invalid 'pattern' field");
            return;
        }
    }

    // Read the "content" (replacement) string
    std::string content_str = safeGetStringKey(config, "content");
    if (!content_str.empty()) {
        replacement = content_str;
    }
    // If no content specified, replacement defaults to empty string
}

std::string ReplaceNormalizer::normalize(const std::string& input) const {
    if (pattern.empty()) {
        return input;
    }

    std::string result;
    result.reserve(input.size());

    size_t pos = 0;
    size_t prev = 0;

    while ((pos = input.find(pattern, prev)) != std::string::npos) {
        result.append(input, prev, pos - prev);
        result.append(replacement);
        prev = pos + pattern.size();
    }

    result.append(input, prev, input.size() - prev);
    return result;
}

} // namespace QoreTokenizer
