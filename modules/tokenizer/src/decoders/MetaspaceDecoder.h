/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    MetaspaceDecoder.h

    Metaspace token decoder (SentencePiece style)

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_METASPACE_DECODER_H
#define _QORE_TOKENIZER_METASPACE_DECODER_H

#include "decoders/AbstractDecoder.h"

#include <string>

namespace QoreTokenizer {

//! Metaspace decoder: replaces metaspace characters back to spaces
class MetaspaceDecoder : public AbstractDecoder {
public:
    //! Constructor: reads config
    /** @param config the Qore hash with replacement and prepend_scheme settings
        @param xsink exception sink
    */
    MetaspaceDecoder(const QoreHashNode* config, ExceptionSink* xsink);

    std::string decode(const std::vector<std::string>& tokens) const override;

private:
    //! the metaspace replacement character (default U+2581 "▁")
    std::string replacement;

    //! prepend scheme: "always", "first", "never"
    std::string prepend_scheme;
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_METASPACE_DECODER_H
