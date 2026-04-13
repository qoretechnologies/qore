/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    DigitsPreTokenizer.h

    Digit-isolating pre-tokenizer

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_DIGITSPRETOKENIZER_H
#define _QORE_TOKENIZER_DIGITSPRETOKENIZER_H

#include "AbstractPreTokenizer.h"

namespace QoreTokenizer {

//! Isolates digits as separate pre-tokens
class DigitsPreTokenizer : public AbstractPreTokenizer {
public:
    DigitsPreTokenizer(bool individual_digits = true)
        : individual(individual_digits) {}

    std::vector<PreToken> pretokenize(const std::string& text) const override;

private:
    bool individual;
};

} // namespace QoreTokenizer

#endif
