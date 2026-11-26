/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    AbstractDecoder.cpp

    Abstract base class for token decoders - factory implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "decoders/AbstractDecoder.h"
#include "decoders/WordPieceDecoder.h"
#include "decoders/ByteLevelDecoder.h"
#include "decoders/MetaspaceDecoder.h"
#include "decoders/ByteFallbackDecoder.h"
#include "decoders/SequenceDecoder.h"
#include "utils/qore_helpers.h"

using namespace QoreTokenizer;

namespace QoreTokenizer {

std::string AbstractDecoder::decode(std::vector<std::string> tokens) const {
    auto parts = decodeChain(std::move(tokens));
    if (parts.size() == 1) {
        return std::move(parts.front());
    }
    std::string text;
    for (const auto& part : parts) {
        text += part;
    }
    return text;
}

std::unique_ptr<AbstractDecoder> AbstractDecoder::fromConfig(
        const QoreHashNode* config, ExceptionSink* xsink) {
    if (!config) {
        xsink->raiseException("TOKENIZER-DECODER-ERROR", "decoder config is null");
        return nullptr;
    }

    std::string type = configStringKey(config, "type", xsink);
    if (*xsink) {
        return nullptr;
    }
    if (type.empty()) {
        xsink->raiseException("TOKENIZER-DECODER-ERROR", "decoder config missing 'type' field");
        return nullptr;
    }

    if (type == "WordPiece") {
        return std::make_unique<WordPieceDecoder>(config, xsink);
    } else if (type == "ByteLevel") {
        return std::make_unique<ByteLevelDecoder>(config, xsink);
    } else if (type == "Metaspace") {
        return std::make_unique<MetaspaceDecoder>(config, xsink);
    } else if (type == "BPEDecoder") {
        if (config->existsKey("suffix")) {
            return std::make_unique<BPESuffixDecoder>(configStringKey(config, "suffix", xsink));
        }
        // Older Qore configs used BPEDecoder as an alias for an optional Replace followed by byte fallback.
        std::vector<std::unique_ptr<AbstractDecoder>> steps;
        if (config->existsKey("pattern")) {
            steps.push_back(std::make_unique<ReplaceDecoder>(config, xsink));
            if (*xsink) {
                return nullptr;
            }
        }
        steps.push_back(std::make_unique<ByteFallbackDecoder>(config, xsink));
        if (*xsink) {
            return nullptr;
        }
        steps.push_back(std::make_unique<FuseDecoder>());
        return std::make_unique<SequenceDecoder>(std::move(steps));
    } else if (type == "ByteFallback") {
        return std::make_unique<ByteFallbackDecoder>(config, xsink);
    } else if (type == "Fuse") {
        return std::make_unique<FuseDecoder>();
    } else if (type == "Strip") {
        return std::make_unique<StripDecoder>(config, xsink);
    } else if (type == "Replace") {
        return std::make_unique<ReplaceDecoder>(config, xsink);
    } else if (type == "Sequence") {
        return std::make_unique<SequenceDecoder>(config, xsink);
    } else {
        xsink->raiseException("TOKENIZER-DECODER-ERROR",
            "unknown decoder type '%s'", type.c_str());
        return nullptr;
    }
}

} // namespace QoreTokenizer
