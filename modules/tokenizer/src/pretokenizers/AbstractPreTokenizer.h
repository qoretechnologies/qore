/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    AbstractPreTokenizer.h

    Abstract base class for pre-tokenizers

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_ABSTRACT_PRETOKENIZER_H
#define _QORE_TOKENIZER_ABSTRACT_PRETOKENIZER_H

#include <string>
#include <vector>
#include <memory>

#include "qore/Qore.h"

namespace QoreTokenizer {

//! A pre-token: text fragment with byte offsets into the original input
struct PreToken {
    std::string text;
    size_t start;
    size_t end;
};

//! Abstract base class for pre-tokenizers
class AbstractPreTokenizer {
public:
    virtual ~AbstractPreTokenizer() = default;

    //! Pre-tokenizes the input string into a list of pre-tokens
    virtual std::vector<PreToken> pretokenize(const std::string& input) const = 0;

    //! Factory: creates a pre-tokenizer from a parsed tokenizer.json "pre_tokenizer" config hash
    /** @param config the Qore hash from the tokenizer.json "pre_tokenizer" key
        @param xsink exception sink
        @return the pre-tokenizer, or nullptr on error
    */
    static std::unique_ptr<AbstractPreTokenizer> fromConfig(const QoreHashNode* config,
        ExceptionSink* xsink);
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_ABSTRACT_PRETOKENIZER_H
