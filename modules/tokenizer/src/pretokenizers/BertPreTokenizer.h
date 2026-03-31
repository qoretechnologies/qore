/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    BertPreTokenizer.h

    BERT pre-tokenizer: splits on whitespace and punctuation boundaries

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_BERT_PRETOKENIZER_H
#define _QORE_TOKENIZER_BERT_PRETOKENIZER_H

#include "pretokenizers/AbstractPreTokenizer.h"

namespace QoreTokenizer {

//! BERT pre-tokenizer: splits on whitespace and punctuation boundaries
/** Each punctuation character becomes its own token, whitespace is removed,
    and contiguous non-punctuation/non-whitespace characters form a token.
    Uses utf8proc to detect Unicode punctuation (general category starting with 'P').
*/
class BertPreTokenizer : public AbstractPreTokenizer {
public:
    BertPreTokenizer() = default;

    std::vector<PreToken> pretokenize(const std::string& input) const override;

private:
    //! Returns the byte length of a UTF-8 character starting at the given position
    static size_t utf8CharLen(const std::string& s, size_t pos);

    //! Decodes a UTF-8 character at the given position into a Unicode codepoint
    static int32_t decodeUtf8Codepoint(const std::string& s, size_t pos, size_t len);

    //! Returns true if the codepoint is Unicode punctuation (category starts with 'P')
    static bool isUnicodePunctuation(int32_t codepoint);

    //! Returns true if the codepoint is Unicode whitespace
    static bool isUnicodeWhitespace(int32_t codepoint);
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_BERT_PRETOKENIZER_H
