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

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

namespace QoreTokenizer {

//! Normalizer that replaces all occurrences of a pattern string
class ReplaceNormalizer : public AbstractNormalizer {
public:
    //! Constructor: reads config from a parsed tokenizer.json "normalizer" hash
    /** @param config the Qore hash containing "pattern" and "content" (replacement)
        @param xsink exception sink
    */
    ReplaceNormalizer(const QoreHashNode* config, ExceptionSink* xsink,
        const char* error = "TOKENIZER-NORMALIZER-ERROR");

    std::string normalize(const std::string& input) const override;

private:
    //! The string pattern to find
    std::string pattern;

    //! Compiled regular expression when the HuggingFace pattern is {"Regex": "..."}
    std::unique_ptr<pcre2_code, decltype(&pcre2_code_free)> regex_pattern{nullptr, pcre2_code_free};

    //! The replacement string
    std::string replacement;

    //! Exception code of the component that uses the replacement
    const char* error;
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_REPLACE_NORMALIZER_H
