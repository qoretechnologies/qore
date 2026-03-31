/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    MetaspacePreTokenizer.h

    Metaspace pre-tokenizer (used by SentencePiece-based tokenizers)

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_METASPACE_PRETOKENIZER_H
#define _QORE_TOKENIZER_METASPACE_PRETOKENIZER_H

#include "pretokenizers/AbstractPreTokenizer.h"

namespace QoreTokenizer {

//! Metaspace pre-tokenizer: replaces spaces with a replacement character
/** Commonly used by SentencePiece-based tokenizers. Replaces spaces with
    the replacement character (default U+2581 "lower one eighth block"),
    optionally prepends the replacement to tokens, and optionally splits
    on replacement character boundaries.
*/
class MetaspacePreTokenizer : public AbstractPreTokenizer {
public:
    //! Prepend scheme values
    enum class PrependScheme {
        Always,     //!< Prepend replacement to every piece
        First,      //!< Prepend replacement only to the first piece
        Never       //!< Never prepend replacement
    };

    //! Constructor with explicit parameters
    MetaspacePreTokenizer(const std::string& replacement = "\xE2\x96\x81",
        PrependScheme prepend_scheme = PrependScheme::Always,
        bool do_split = true);

    //! Constructor from config hash
    MetaspacePreTokenizer(const QoreHashNode* config, ExceptionSink* xsink);

    std::vector<PreToken> pretokenize(const std::string& input) const override;

private:
    std::string replacement;        //!< Replacement character (default U+2581)
    PrependScheme prepend_scheme;   //!< When to prepend the replacement character
    bool do_split;                  //!< Whether to split on replacement boundaries

    //! Parses a prepend_scheme string into the enum
    static PrependScheme parsePrependScheme(const std::string& scheme);
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_METASPACE_PRETOKENIZER_H
