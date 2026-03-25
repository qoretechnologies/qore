/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SequenceDecoder.h

    Sequence decoder and inline decoders (Fuse, Strip, Replace)

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_SEQUENCE_DECODER_H
#define _QORE_TOKENIZER_SEQUENCE_DECODER_H

#include "decoders/AbstractDecoder.h"

#include <string>
#include <vector>
#include <memory>

namespace QoreTokenizer {

//! Fuse decoder: joins all tokens with no separator
class FuseDecoder : public AbstractDecoder {
public:
    std::string decode(const std::vector<std::string>& tokens) const override;
};

//! Strip decoder: strips characters from the start and/or end of each token
class StripDecoder : public AbstractDecoder {
public:
    //! Constructor: reads config
    /** @param config the Qore hash with content, start, stop settings
        @param xsink exception sink
    */
    StripDecoder(const QoreHashNode* config, ExceptionSink* xsink);

    std::string decode(const std::vector<std::string>& tokens) const override;

private:
    //! character to strip
    std::string content;

    //! number of characters to strip from the start
    int start;

    //! number of characters to strip from the end
    int stop;
};

//! Replace decoder: performs a string replacement on each token
class ReplaceDecoder : public AbstractDecoder {
public:
    //! Constructor: reads config
    /** @param config the Qore hash with pattern and content settings
        @param xsink exception sink
    */
    ReplaceDecoder(const QoreHashNode* config, ExceptionSink* xsink);

    std::string decode(const std::vector<std::string>& tokens) const override;

private:
    //! string pattern to search for
    std::string pattern;

    //! replacement string
    std::string content;
};

//! Sequence decoder: applies multiple decoders in sequence
/** The first decoder receives the token list. Subsequent decoders receive
    a single-element list containing the result string from the previous decoder.
*/
class SequenceDecoder : public AbstractDecoder {
public:
    //! Constructor: reads config and creates child decoders
    /** @param config the Qore hash with decoders list
        @param xsink exception sink
    */
    SequenceDecoder(const QoreHashNode* config, ExceptionSink* xsink);

    //! Constructor: takes ownership of pre-built child decoders
    /** @param decoders the child decoders to apply in sequence
    */
    explicit SequenceDecoder(std::vector<std::unique_ptr<AbstractDecoder>> decoders);

    std::string decode(const std::vector<std::string>& tokens) const override;

private:
    //! child decoders applied in order
    std::vector<std::unique_ptr<AbstractDecoder>> decoders;
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_SEQUENCE_DECODER_H
