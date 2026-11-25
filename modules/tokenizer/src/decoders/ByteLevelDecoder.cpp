/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ByteLevelDecoder.cpp

    Byte-level BPE decoder implementation

    Joins all tokens and converts from GPT-2 unicode representation back
    to raw bytes using the unicodeToBytes() mapping.

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "decoders/ByteLevelDecoder.h"
#include "utils/byte_level_mapping.h"

namespace QoreTokenizer {

ByteLevelDecoder::ByteLevelDecoder(const QoreHashNode* /* config */, ExceptionSink* /* xsink */) {
    // No configuration needed
}

std::vector<std::string> ByteLevelDecoder::decodeChain(std::vector<std::string> tokens) const {
    std::string bytes;
    for (const auto& token : tokens) {
        bytes += unicodeToBytes(token);
    }
    return {decodedUtf8(bytes)};
}

} // namespace QoreTokenizer
