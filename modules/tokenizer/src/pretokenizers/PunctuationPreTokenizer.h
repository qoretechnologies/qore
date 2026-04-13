/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    PunctuationPreTokenizer.h

    Punctuation-isolating pre-tokenizer

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_PUNCTUATIONPRETOKENIZER_H
#define _QORE_TOKENIZER_PUNCTUATIONPRETOKENIZER_H

#include "AbstractPreTokenizer.h"

namespace QoreTokenizer {

//! Isolates punctuation characters as separate pre-tokens
class PunctuationPreTokenizer : public AbstractPreTokenizer {
public:
    PunctuationPreTokenizer() = default;

    std::vector<PreToken> pretokenize(const std::string& text) const override;
};

} // namespace QoreTokenizer

#endif
