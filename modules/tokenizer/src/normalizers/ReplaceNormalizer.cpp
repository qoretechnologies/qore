/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ReplaceNormalizer.cpp

    Replace normalizer implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "normalizers/ReplaceNormalizer.h"
#include "utils/qore_helpers.h"
#include "utils/TokenizerError.h"

using namespace QoreTokenizer;

namespace QoreTokenizer {

ReplaceNormalizer::ReplaceNormalizer(const QoreHashNode* config, ExceptionSink* xsink, const char* error)
        : error(error) {
    const QoreHashNode* patterns = config ? safeGetHashKey(config, "pattern") : nullptr;
    bool regex = patterns && patterns->existsKey("Regex");
    const char* key = patterns ? (regex ? "Regex" : "String") : "pattern";
    const QoreHashNode* source = patterns ? patterns : config;
    if (!source || source->getKeyValue(key).getType() != NT_STRING) {
        xsink->raiseException(error, "Replace requires a string or regex pattern");
        return;
    }
    pattern = configStringKey(source, key, xsink);
    if (*xsink) {
        return;
    }
    replacement = configStringKey(config, "content", xsink);
    if (*xsink || (!regex && !pattern.empty())) {
        return;
    }

    // An empty literal, like an empty regex, matches at character boundaries.
    int rc;
    PCRE2_SIZE offset;
    regex_pattern.reset(pcre2_compile(reinterpret_cast<PCRE2_SPTR>(pattern.data()), pattern.size(),
        PCRE2_UTF | PCRE2_UCP | PCRE2_NEVER_BACKSLASH_C, &rc, &offset, nullptr));
    if (!regex_pattern) {
        PCRE2_UCHAR msg[256];
        pcre2_get_error_message(rc, msg, sizeof(msg));
        xsink->raiseException(error, "invalid Replace pattern at byte offset %zu: %s", offset, msg);
    }
}

std::string ReplaceNormalizer::normalize(const std::string& input) const {
    if (input.empty()) {
        return input;
    }
    if (regex_pattern) {
        auto* md = pcre2_match_data_create_from_pattern(regex_pattern.get(), nullptr);
        if (!md) {
            throw TokenizerError(error, "could not allocate Replace match data");
        }
        ON_BLOCK_EXIT(pcre2_match_data_free, md);
        std::string out;
        size_t copied = 0;
        size_t offset = 0;
        bool first = true;
        while (offset <= input.size()) {
            int rc = pcre2_match(regex_pattern.get(), reinterpret_cast<PCRE2_SPTR>(input.data()), input.size(),
                offset, first ? 0 : PCRE2_NO_UTF_CHECK, md, nullptr);
            if (rc == PCRE2_ERROR_NOMATCH) {
                break;
            }
            if (rc < 0) {
                PCRE2_UCHAR msg[256];
                pcre2_get_error_message(rc, msg, sizeof(msg));
                throw TokenizerError(error, std::string("Replace match failed: ") + reinterpret_cast<char*>(msg));
            }
            const auto* match = pcre2_get_ovector_pointer(md);
            // Like Hugging Face, skip an empty match immediately after a preceding match.
            if (first || match[0] != copied || match[0] != match[1]) {
                out.append(input, copied, match[0] - copied);
                out.append(replacement);
                copied = match[1];
            }
            first = false;
            offset = match[1];
            if (match[0] == match[1]) {
                do {
                    ++offset;
                } while (offset < input.size() && (static_cast<unsigned char>(input[offset]) & 0xc0) == 0x80);
            }
        }
        out.append(input, copied, std::string::npos);
        return out;
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
