/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ByteFallbackDecoder.h

    Byte fallback token decoder

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_BYTE_FALLBACK_DECODER_H
#define _QORE_TOKENIZER_BYTE_FALLBACK_DECODER_H

#include "decoders/AbstractDecoder.h"

namespace QoreTokenizer {

//! Byte fallback decoder: converts <0xHH> tokens back to bytes
/** Accumulates consecutive tokens matching the <0xHH> pattern, parses
    the hex values, and decodes the accumulated bytes as UTF-8.
    Non-matching tokens pass through unchanged.
*/
class ByteFallbackDecoder : public AbstractDecoder {
public:
    //! Constructor
    /** @param config the Qore hash (currently unused)
        @param xsink exception sink
    */
    ByteFallbackDecoder(const QoreHashNode* config, ExceptionSink* xsink);

    std::string decode(const std::vector<std::string>& tokens) const override;

private:
    //! Checks if a token matches the <0xHH> byte token pattern
    /** @param token the token string to check
        @param byte_out if matching, receives the parsed byte value
        @return true if the token matches
    */
    static bool parseByteToken(const std::string& token, uint8_t& byte_out);
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_BYTE_FALLBACK_DECODER_H
