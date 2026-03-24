/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    PrependNormalizer.h

    Prepend normalizer: prepends a fixed string to the input

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_PREPEND_NORMALIZER_H
#define _QORE_TOKENIZER_PREPEND_NORMALIZER_H

#include "normalizers/AbstractNormalizer.h"

namespace QoreTokenizer {

//! Normalizer that prepends a fixed string to the input
class PrependNormalizer : public AbstractNormalizer {
public:
    //! Constructor: reads config from a parsed tokenizer.json "normalizer" hash
    /** @param config the Qore hash containing "prepend" string
        @param xsink exception sink
    */
    PrependNormalizer(const QoreHashNode* config, ExceptionSink* xsink);

    std::string normalize(const std::string& input) const override;

private:
    //! The string to prepend
    std::string prepend;
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_PREPEND_NORMALIZER_H
