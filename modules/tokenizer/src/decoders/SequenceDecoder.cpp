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

std::vector<std::string> FuseDecoder::decodeChain(std::vector<std::string> tokens) const {
    std::string result;
    for (const auto& tok : tokens) {
        result += tok;
    }
    return {std::move(result)};
}

// --- StripDecoder ---

StripDecoder::StripDecoder(const QoreHashNode* config, ExceptionSink* xsink)
        : start(0), stop(0) {
    assert(config);

    std::string content_str = configStringKey(config, "content", xsink);
    if (*xsink) {
        return;
    }
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

std::vector<std::string> StripDecoder::decodeChain(std::vector<std::string> tokens) const {
    for (auto& s : tokens) {
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
    }
    return tokens;
}

ReplaceDecoder::ReplaceDecoder(const QoreHashNode* config, ExceptionSink* xsink)
        : replace(config, xsink, "TOKENIZER-DECODER-ERROR") {
}

std::vector<std::string> ReplaceDecoder::decodeChain(std::vector<std::string> tokens) const {
    for (auto& token : tokens) {
        token = replace.normalize(token);
    }
    return tokens;
}

std::vector<std::string> BPESuffixDecoder::decodeChain(std::vector<std::string> tokens) const {
    for (size_t i = 0; i < tokens.size(); ++i) {
        auto& token = tokens[i];
        bool space = i + 1 < tokens.size();
        if (suffix.empty()) {
            // The empty suffix matches before and after each Unicode character.
            if (space) {
                std::string separated;
                for (unsigned char c : token) {
                    if ((c & 0xc0) != 0x80) {
                        separated += ' ';
                    }
                    separated += static_cast<char>(c);
                }
                token = separated + ' ';
            }
            continue;
        }
        size_t pos = 0;
        while ((pos = token.find(suffix, pos)) != std::string::npos) {
            token.replace(pos, suffix.size(), space ? " " : "");
            pos += space;
        }
    }
    return tokens;
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

SequenceDecoder::SequenceDecoder(std::vector<std::unique_ptr<AbstractDecoder>> decoders)
        : decoders(std::move(decoders)) {
}

std::vector<std::string> SequenceDecoder::decodeChain(std::vector<std::string> tokens) const {
    for (const auto& decoder : decoders) {
        tokens = decoder->decodeChain(std::move(tokens));
    }
    return tokens;
}

} // namespace QoreTokenizer
