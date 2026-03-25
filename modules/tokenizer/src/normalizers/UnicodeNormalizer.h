/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    UnicodeNormalizer.h

    Unicode normalization (NFC, NFD, NFKC, NFKD) with optional accent stripping

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_UNICODE_NORMALIZER_H
#define _QORE_TOKENIZER_UNICODE_NORMALIZER_H

#include "normalizers/AbstractNormalizer.h"

namespace QoreTokenizer {

//! Unicode normalization normalizer using utf8proc
class UnicodeNormalizer : public AbstractNormalizer {
public:
    //! Unicode normalization forms
    enum class Form {
        NFC,
        NFD,
        NFKC,
        NFKD,
    };

    //! Constructor for a specific normalization form
    /** @param form the Unicode normalization form to apply
        @param strip_accents_after if true, strip combining marks (Mn category) after normalization
    */
    UnicodeNormalizer(Form form, bool strip_accents_after = false);

    std::string normalize(const std::string& input) const override;

    //! Parses a normalization form string ("NFC", "NFD", "NFKC", "NFKD")
    /** @param name the form name
        @return the form enum value, defaults to NFC if unrecognized
    */
    static Form parseForm(const std::string& name);

private:
    //! The normalization form to apply
    Form form;

    //! Whether to strip combining marks after normalization
    bool strip_accents_after;

    //! Applies the utf8proc normalization
    std::string applyNormalization(const std::string& input) const;

    //! Strips combining marks (Unicode general category Mn) from the input
    static std::string stripCombiningMarks(const std::string& input);
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_UNICODE_NORMALIZER_H
