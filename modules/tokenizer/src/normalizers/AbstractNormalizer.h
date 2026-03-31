/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    AbstractNormalizer.h

    Abstract base class for text normalizers

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_ABSTRACT_NORMALIZER_H
#define _QORE_TOKENIZER_ABSTRACT_NORMALIZER_H

#include <string>
#include <memory>

#include "qore/Qore.h"

namespace QoreTokenizer {

//! Abstract base class for text normalizers
class AbstractNormalizer {
public:
    virtual ~AbstractNormalizer() = default;

    //! Normalizes the input string
    virtual std::string normalize(const std::string& input) const = 0;

    //! Factory: creates a normalizer from a parsed tokenizer.json "normalizer" config hash
    /** @param config the Qore hash from the tokenizer.json "normalizer" key
        @param xsink exception sink
        @return the normalizer, or nullptr on error
    */
    static std::unique_ptr<AbstractNormalizer> fromConfig(const QoreHashNode* config,
        ExceptionSink* xsink);
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_ABSTRACT_NORMALIZER_H
