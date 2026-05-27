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

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

namespace QoreTokenizer {

//! Splits input text on regex pattern matches
/** The pattern is compiled with PCRE2 in Unicode mode (\c PCRE2_UTF | \c PCRE2_UCP)
    so that HuggingFace \c Split patterns work as written: Unicode property classes
    (\c \\p{L}, \c \\p{N}), inline group flags (\c (?i:...)), and lookarounds
    (\c (?!\\S)) are all supported, and matching is codepoint-correct over UTF-8.
    This is required for GPT-2/GPT-4/Llama/Qwen-style byte-level BPE tokenizers,
    whose pre-tokenizer is a \c Split{Regex} followed by a \c ByteLevel.
*/
class SplitPreTokenizer : public AbstractPreTokenizer {
public:
    //! Constructs from config hash
    /** @param config config with "pattern" and "behavior" keys
        @param xsink exception sink

        @throw PRETOKENIZER-CONFIG-ERROR if "pattern" is missing or the regex
        fails to compile under PCRE2
    */
    SplitPreTokenizer(const QoreHashNode* config, ExceptionSink* xsink);

    ~SplitPreTokenizer() override;

    SplitPreTokenizer(const SplitPreTokenizer&) = delete;
    SplitPreTokenizer& operator=(const SplitPreTokenizer&) = delete;

    std::vector<PreToken> pretokenize(const std::string& text) const override;

private:
    pcre2_code* re = nullptr;
    std::string behavior; // "Removed", "Isolated", "MergedWithNext", "MergedWithPrevious"
};

} // namespace QoreTokenizer

#endif
