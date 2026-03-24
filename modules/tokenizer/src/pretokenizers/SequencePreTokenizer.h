/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SequencePreTokenizer.h

    Sequence pre-tokenizer: chains multiple pre-tokenizers in sequence

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_SEQUENCE_PRETOKENIZER_H
#define _QORE_TOKENIZER_SEQUENCE_PRETOKENIZER_H

#include "pretokenizers/AbstractPreTokenizer.h"

#include <memory>
#include <vector>

namespace QoreTokenizer {

//! Sequence pre-tokenizer: applies multiple pre-tokenizers in order
/** The first pre-tokenizer is applied to the input, then each subsequent
    pre-tokenizer is applied to each piece from the previous step.
*/
class SequencePreTokenizer : public AbstractPreTokenizer {
public:
    //! Constructor from a list of pre-tokenizers
    explicit SequencePreTokenizer(std::vector<std::unique_ptr<AbstractPreTokenizer>>&& pretokenizers);

    //! Constructor from config hash
    SequencePreTokenizer(const QoreHashNode* config, ExceptionSink* xsink);

    std::vector<PreToken> pretokenize(const std::string& input) const override;

private:
    std::vector<std::unique_ptr<AbstractPreTokenizer>> pretokenizers;
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_SEQUENCE_PRETOKENIZER_H
