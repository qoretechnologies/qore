/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    AbstractDecoder.h

    Abstract base class for token decoders

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_ABSTRACT_DECODER_H
#define _QORE_TOKENIZER_ABSTRACT_DECODER_H

#include <string>
#include <vector>
#include <memory>

#include "qore/Qore.h"

namespace QoreTokenizer {

//! Abstract base class for token decoders
/** Decoders convert a list of token strings back to a readable string by
    reversing the encoding applied during tokenization.
*/
class AbstractDecoder {
public:
    virtual ~AbstractDecoder() = default;

    //! Decodes a list of token strings into a single output string
    /** @param tokens the list of token strings to decode
        @return the decoded string
    */
    virtual std::string decode(const std::vector<std::string>& tokens) const = 0;

    //! Factory: creates a decoder from a parsed tokenizer.json "decoder" config hash
    /** Dispatches on config.type:
        - "WordPiece" -> WordPieceDecoder
        - "ByteLevel" -> ByteLevelDecoder
        - "Metaspace" -> MetaspaceDecoder
        - "BPEDecoder" -> same as ByteFallbackDecoder + Fuse (legacy alias)
        - "ByteFallback" -> ByteFallbackDecoder
        - "Fuse" -> FuseDecoder (handled by SequenceDecoder)
        - "Strip" -> StripDecoder (handled by SequenceDecoder)
        - "Replace" -> ReplaceDecoder (handled by SequenceDecoder)
        - "Sequence" -> SequenceDecoder

        @param config the Qore hash from the tokenizer.json "decoder" key
        @param xsink exception sink
        @return the decoder, or nullptr on error
    */
    static std::unique_ptr<AbstractDecoder> fromConfig(const QoreHashNode* config,
        ExceptionSink* xsink);
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_ABSTRACT_DECODER_H
