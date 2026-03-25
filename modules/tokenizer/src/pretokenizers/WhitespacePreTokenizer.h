/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    WhitespacePreTokenizer.h

    Whitespace pre-tokenizer (with optional punctuation splitting)

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_WHITESPACE_PRETOKENIZER_H
#define _QORE_TOKENIZER_WHITESPACE_PRETOKENIZER_H

#include "pretokenizers/AbstractPreTokenizer.h"

namespace QoreTokenizer {

//! Whitespace pre-tokenizer
/** In split-only mode (WhitespaceSplit), splits on whitespace only.
    In full mode (Whitespace), splits on both whitespace and punctuation
    boundaries, keeping punctuation characters as separate tokens.
*/
class WhitespacePreTokenizer : public AbstractPreTokenizer {
public:
    //! Constructor
    /** @param split_only if true, only split on whitespace (WhitespaceSplit mode);
        if false, also split on punctuation boundaries (Whitespace mode)
    */
    explicit WhitespacePreTokenizer(bool split_only = false);

    std::vector<PreToken> pretokenize(const std::string& input) const override;

private:
    bool split_only;

    //! Returns true if the codepoint is Unicode punctuation
    static bool isUnicodePunctuation(int32_t codepoint);

    //! Returns true if the codepoint is whitespace
    static bool isUnicodeWhitespace(int32_t codepoint);

    //! Returns the byte length of a UTF-8 character
    static size_t utf8CharLen(const std::string& s, size_t pos);

    //! Decodes a UTF-8 character into a codepoint
    static int32_t decodeUtf8Codepoint(const std::string& s, size_t pos, size_t len);
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_WHITESPACE_PRETOKENIZER_H
