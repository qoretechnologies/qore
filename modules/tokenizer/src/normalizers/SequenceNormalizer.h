/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SequenceNormalizer.h

    Sequence normalizer: applies a series of normalizers in order

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_SEQUENCE_NORMALIZER_H
#define _QORE_TOKENIZER_SEQUENCE_NORMALIZER_H

#include "normalizers/AbstractNormalizer.h"

#include <vector>

namespace QoreTokenizer {

//! Applies a sequence of normalizers in order
class SequenceNormalizer : public AbstractNormalizer {
public:
    //! Constructor: reads config from a parsed tokenizer.json "normalizer" hash
    /** @param config the Qore hash containing a "normalizers" list
        @param xsink exception sink
    */
    SequenceNormalizer(const QoreHashNode* config, ExceptionSink* xsink);

    std::string normalize(const std::string& input) const override;

private:
    //! The sequence of normalizers to apply
    std::vector<std::unique_ptr<AbstractNormalizer>> normalizers;
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_SEQUENCE_NORMALIZER_H
