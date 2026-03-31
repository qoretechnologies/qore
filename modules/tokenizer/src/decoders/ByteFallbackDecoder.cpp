/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ByteFallbackDecoder.cpp

    Byte fallback token decoder implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "decoders/ByteFallbackDecoder.h"

#include <cstdlib>

namespace QoreTokenizer {

ByteFallbackDecoder::ByteFallbackDecoder(const QoreHashNode* /* config */,
        ExceptionSink* /* xsink */) {
    // No configuration needed
}

bool ByteFallbackDecoder::parseByteToken(const std::string& token, uint8_t& byte_out) {
    // Pattern: <0xHH> where H is a hex digit
    if (token.size() != 6) {
        return false;
    }
    if (token[0] != '<' || token[1] != '0' || token[2] != 'x' || token[5] != '>') {
        return false;
    }

    char hex[3] = { token[3], token[4], 0 };
    // Validate hex digits
    for (int i = 0; i < 2; ++i) {
        char c = hex[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }

    byte_out = (uint8_t)strtoul(hex, nullptr, 16);
    return true;
}

std::string ByteFallbackDecoder::decode(const std::vector<std::string>& tokens) const {
    std::string result;
    std::string byte_buffer;

    auto flush_bytes = [&]() {
        if (!byte_buffer.empty()) {
            result += byte_buffer;
            byte_buffer.clear();
        }
    };

    for (const auto& tok : tokens) {
        uint8_t byte_val;
        if (parseByteToken(tok, byte_val)) {
            // Accumulate byte tokens
            byte_buffer += (char)byte_val;
        } else {
            // Flush any accumulated bytes as UTF-8
            flush_bytes();
            // Pass through non-byte tokens
            result += tok;
        }
    }

    // Flush any remaining bytes
    flush_bytes();

    return result;
}

} // namespace QoreTokenizer
