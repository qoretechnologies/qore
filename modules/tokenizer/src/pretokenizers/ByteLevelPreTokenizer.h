/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ByteLevelPreTokenizer.h

    GPT-2 byte-level pre-tokenizer

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_BYTE_LEVEL_PRETOKENIZER_H
#define _QORE_TOKENIZER_BYTE_LEVEL_PRETOKENIZER_H

#include "pretokenizers/AbstractPreTokenizer.h"

namespace QoreTokenizer {

//! GPT-2 byte-level pre-tokenizer
/** Applies the GPT-2 splitting pattern (contractions, letters, digits, etc.)
    and then converts each token's bytes through the GPT-2 byte-to-unicode mapping.
*/
class ByteLevelPreTokenizer : public AbstractPreTokenizer {
public:
    //! Constructor with explicit parameters
    ByteLevelPreTokenizer(bool add_prefix_space = false, bool trim_offsets = true,
        bool use_regex = true);

    //! Constructor from config hash
    ByteLevelPreTokenizer(const QoreHashNode* config, ExceptionSink* xsink);

    std::vector<PreToken> pretokenize(const std::string& input) const override;

private:
    bool add_prefix_space;
    bool trim_offsets;
    bool use_regex;

    //! Applies the GPT-2 regex splitting pattern to the input
    /** The pattern matches:
        - Contractions: 's, 't, 're, 've, 'm, 'll, 'd (case insensitive)
        - optional_space + letters+
        - optional_space + digits+
        - optional_space + non-letter-non-digit-non-space+
        - spaces not followed by non-space (trailing spaces)
    */
    std::vector<std::string> gpt2Split(const std::string& input) const;

    //! Returns true if the byte is a UTF-8 leading byte for a letter character
    static bool isLetter(const std::string& s, size_t pos, size_t& charLen);

    //! Returns true if the byte is an ASCII digit
    static bool isDigit(unsigned char c);

    //! Returns true if the byte is a space character
    static bool isSpace(unsigned char c);

    //! Returns the byte length of a UTF-8 character
    static size_t utf8CharLen(unsigned char c);
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_BYTE_LEVEL_PRETOKENIZER_H
