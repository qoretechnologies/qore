/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    WordPieceDecoder.h

    WordPiece token decoder

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_WORDPIECE_DECODER_H
#define _QORE_TOKENIZER_WORDPIECE_DECODER_H

#include "decoders/AbstractDecoder.h"

#include <string>

namespace QoreTokenizer {

//! WordPiece decoder: joins tokens, stripping the continuation prefix
class WordPieceDecoder : public AbstractDecoder {
public:
    //! Constructor: reads config
    /** @param config the Qore hash with prefix and cleanup settings
        @param xsink exception sink
    */
    WordPieceDecoder(const QoreHashNode* config, ExceptionSink* xsink);

    std::string decode(const std::vector<std::string>& tokens) const override;

private:
    //! the continuation prefix to strip (default "##")
    std::string prefix;

    //! whether to remove spaces before punctuation
    bool cleanup;
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_WORDPIECE_DECODER_H
