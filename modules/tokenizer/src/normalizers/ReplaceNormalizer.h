/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ReplaceNormalizer.h

    Replace normalizer: finds and replaces all occurrences of a pattern

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_REPLACE_NORMALIZER_H
#define _QORE_TOKENIZER_REPLACE_NORMALIZER_H

#include "normalizers/AbstractNormalizer.h"

namespace QoreTokenizer {

//! Normalizer that replaces all occurrences of a pattern string
class ReplaceNormalizer : public AbstractNormalizer {
public:
    //! Constructor: reads config from a parsed tokenizer.json "normalizer" hash
    /** @param config the Qore hash containing "pattern" and "content" (replacement)
        @param xsink exception sink
    */
    ReplaceNormalizer(const QoreHashNode* config, ExceptionSink* xsink);

    std::string normalize(const std::string& input) const override;

private:
    //! The string pattern to find
    std::string pattern;

    //! The replacement string
    std::string replacement;
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_REPLACE_NORMALIZER_H
