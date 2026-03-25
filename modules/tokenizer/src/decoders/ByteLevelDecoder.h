/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ByteLevelDecoder.h

    Byte-level BPE decoder (GPT-2 style)

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_BYTE_LEVEL_DECODER_H
#define _QORE_TOKENIZER_BYTE_LEVEL_DECODER_H

#include "decoders/AbstractDecoder.h"

namespace QoreTokenizer {

//! Byte-level decoder: reverses GPT-2 unicode byte mapping
class ByteLevelDecoder : public AbstractDecoder {
public:
    //! Constructor
    /** @param config the Qore hash (currently unused)
        @param xsink exception sink
    */
    ByteLevelDecoder(const QoreHashNode* config, ExceptionSink* xsink);

    std::string decode(const std::vector<std::string>& tokens) const override;
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_BYTE_LEVEL_DECODER_H
