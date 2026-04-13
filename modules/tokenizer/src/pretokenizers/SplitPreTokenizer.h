/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SplitPreTokenizer.h

    Regex-based split pre-tokenizer

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_SPLITPRETOKENIZER_H
#define _QORE_TOKENIZER_SPLITPRETOKENIZER_H

#include "AbstractPreTokenizer.h"

#include <string>
#include <regex>

namespace QoreTokenizer {

//! Splits input text on regex pattern matches
class SplitPreTokenizer : public AbstractPreTokenizer {
public:
    //! Constructs from config hash
    /** @param config config with "pattern" and "behavior" keys
    */
    SplitPreTokenizer(const QoreHashNode* config, ExceptionSink* xsink);

    std::vector<PreToken> pretokenize(const std::string& text) const override;

private:
    std::regex pattern;
    std::string behavior; // "Removed", "Isolated", "MergedWithNext", "MergedWithPrevious"
};

} // namespace QoreTokenizer

#endif
