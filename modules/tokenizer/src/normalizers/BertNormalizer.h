/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    BertNormalizer.h

    BERT text normalizer: clean text, handle Chinese chars, strip accents, lowercase

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_BERT_NORMALIZER_H
#define _QORE_TOKENIZER_BERT_NORMALIZER_H

#include "normalizers/AbstractNormalizer.h"

namespace QoreTokenizer {

//! BERT text normalizer
/** Implements the HuggingFace tokenizers BertNormalizer with support for:
    - Cleaning control characters and replacing whitespace with spaces
    - Adding spaces around CJK (Chinese/Japanese/Korean) characters
    - Lowercasing via utf8proc
    - Stripping accents (combining marks) via NFD + utf8proc category filtering
*/
class BertNormalizer : public AbstractNormalizer {
public:
    //! Constructor: reads config from a parsed tokenizer.json "normalizer" hash
    /** @param config the Qore hash containing clean_text, handle_chinese_chars,
        lowercase, strip_accents
        @param xsink exception sink
    */
    BertNormalizer(const QoreHashNode* config, ExceptionSink* xsink);

    std::string normalize(const std::string& input) const override;

private:
    //! Whether to remove control characters and normalize whitespace
    bool clean_text = true;

    //! Whether to add spaces around CJK characters
    bool handle_chinese_chars = true;

    //! Whether to lowercase the text
    bool lowercase = true;

    //! Whether to strip accent marks; if null_strip_accents is true, follows lowercase
    bool strip_accents = true;

    //! True if the config did not specify strip_accents (null), meaning it follows lowercase
    bool null_strip_accents = false;

    //! Returns true if the codepoint is a CJK Unified Ideograph or related range
    static bool isCjkCharacter(int32_t cp);

    //! Returns true if the codepoint is a control character (< 0x20 except \t, \n, \r)
    static bool isControlChar(int32_t cp);

    //! Removes control characters and normalizes whitespace
    static std::string cleanText(const std::string& input);

    //! Adds spaces around CJK characters
    static std::string tokenizeChineseChars(const std::string& input);

    //! Lowercases the input using utf8proc
    static std::string lowercaseText(const std::string& input);

    //! Strips combining marks (accent characters) by doing NFD then removing Mn category
    static std::string stripAccentMarks(const std::string& input);

    //! Returns the byte length of a UTF-8 character from the lead byte
    static size_t utf8CharLen(uint8_t lead_byte);

    //! Decodes a UTF-8 character into a codepoint
    static int32_t decodeUtf8(const uint8_t* data, size_t len);

    //! Encodes a codepoint as UTF-8 and appends to result
    static void encodeUtf8(int32_t cp, std::string& result);
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_BERT_NORMALIZER_H
