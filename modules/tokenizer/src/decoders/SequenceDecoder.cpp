/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SequenceDecoder.cpp

    Sequence decoder and inline decoders (Fuse, Strip, Replace) implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "decoders/SequenceDecoder.h"
#include "utils/qore_helpers.h"

#include <cassert>
#include <algorithm>

using namespace QoreTokenizer;

namespace QoreTokenizer {

// --- FuseDecoder ---

std::string FuseDecoder::decode(const std::vector<std::string>& tokens) const {
    std::string result;
    for (const auto& tok : tokens) {
        result += tok;
    }
    return result;
}

// --- StripDecoder ---

StripDecoder::StripDecoder(const QoreHashNode* config, ExceptionSink* xsink)
        : start(0), stop(0) {
    assert(config);

    std::string content_str = safeGetStringKey(config, "content");
    if (!content_str.empty()) {
        content = content_str;
    }

    QoreValue start_val = config->getKeyValue("start");
    if (start_val.getType() == NT_INT) {
        start = (int)start_val.getAsBigInt();
    }

    QoreValue stop_val = config->getKeyValue("stop");
    if (stop_val.getType() == NT_INT) {
        stop = (int)stop_val.getAsBigInt();
    }
}

std::string StripDecoder::decode(const std::vector<std::string>& tokens) const {
    std::vector<std::string> result;
    result.reserve(tokens.size());

    for (const auto& tok : tokens) {
        std::string s = tok;

        // Strip 'start' occurrences of 'content' from the beginning
        if (!content.empty() && start > 0) {
            for (int i = 0; i < start; ++i) {
                if (s.size() >= content.size() && s.compare(0, content.size(), content) == 0) {
                    s = s.substr(content.size());
                } else {
                    break;
                }
            }
        }

        // Strip 'stop' occurrences of 'content' from the end
        if (!content.empty() && stop > 0) {
            for (int i = 0; i < stop; ++i) {
                if (s.size() >= content.size()
                        && s.compare(s.size() - content.size(), content.size(), content) == 0) {
                    s = s.substr(0, s.size() - content.size());
                } else {
                    break;
                }
            }
        }

        result.push_back(s);
    }

    // Join the stripped tokens with no separator (caller provides context)
    std::string output;
    for (const auto& s : result) {
        output += s;
    }
    return output;
}

// --- ReplaceDecoder ---

ReplaceDecoder::ReplaceDecoder(const QoreHashNode* config, ExceptionSink* xsink) {
    assert(config);

    std::string pattern_str = safeGetStringKey(config, "pattern");
    if (!pattern_str.empty()) {
        pattern = pattern_str;
    }

    std::string content_val = safeGetStringKey(config, "content");
    if (!content_val.empty()) {
        content = content_val;
    }
}

std::string ReplaceDecoder::decode(const std::vector<std::string>& tokens) const {
    std::vector<std::string> result;
    result.reserve(tokens.size());

    for (const auto& tok : tokens) {
        if (pattern.empty()) {
            result.push_back(tok);
            continue;
        }

        std::string s;
        size_t pos = 0;
        while (pos < tok.size()) {
            size_t found = tok.find(pattern, pos);
            if (found == std::string::npos) {
                s += tok.substr(pos);
                break;
            }
            s += tok.substr(pos, found - pos);
            s += content;
            pos = found + pattern.size();
        }
        result.push_back(s);
    }

    // Join tokens
    std::string output;
    for (const auto& s : result) {
        output += s;
    }
    return output;
}

// --- SequenceDecoder ---

SequenceDecoder::SequenceDecoder(const QoreHashNode* config, ExceptionSink* xsink) {
    assert(config);

    const QoreListNode* decoders_list = safeGetListKey(config, "decoders");
    if (!decoders_list) {
        xsink->raiseException("TOKENIZER-DECODER-ERROR",
            "Sequence decoder config missing 'decoders' list");
        return;
    }

    for (size_t i = 0; i < decoders_list->size(); ++i) {
        const QoreHashNode* decoder_config =
            safeGetHash(decoders_list->retrieveEntry(i));
        if (!decoder_config) {
            xsink->raiseException("TOKENIZER-DECODER-ERROR",
                "Sequence decoder: entry %zu is not a hash", i);
            return;
        }

        auto decoder = AbstractDecoder::fromConfig(decoder_config, xsink);
        if (*xsink) {
            return;
        }
        if (decoder) {
            decoders.push_back(std::move(decoder));
        }
    }
}

std::string SequenceDecoder::decode(const std::vector<std::string>& tokens) const {
    if (decoders.empty()) {
        // No decoders: just join tokens
        std::string result;
        for (const auto& tok : tokens) {
            result += tok;
        }
        return result;
    }

    // First decoder gets the full token list
    std::string result = decoders[0]->decode(tokens);

    // Subsequent decoders get a single-element list with the result
    for (size_t i = 1; i < decoders.size(); ++i) {
        std::vector<std::string> single = {result};
        result = decoders[i]->decode(single);
    }

    return result;
}

} // namespace QoreTokenizer
