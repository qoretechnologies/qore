/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    PrecompiledNormalizer.h

    Precompiled normalizer: SentencePiece-style character normalization via Darts trie

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_TOKENIZER_PRECOMPILED_NORMALIZER_H
#define _QORE_TOKENIZER_PRECOMPILED_NORMALIZER_H

#include "normalizers/AbstractNormalizer.h"
#include "utils/darts_trie.h"

namespace QoreTokenizer {

//! Precompiled normalizer using a SentencePiece Darts trie character map
/** Uses a base64-encoded precompiled charsmap from tokenizer.json, which contains
    a Darts-clone DoubleArray trie and a string pool for character replacements.
    For each position in the input, finds the longest prefix match in the trie
    and emits the corresponding replacement from the string pool. If no match
    is found, passes through one UTF-8 character unchanged.
*/
class PrecompiledNormalizer : public AbstractNormalizer {
public:
    //! Constructor: reads config from a parsed tokenizer.json "normalizer" hash
    /** @param config the Qore hash containing "precompiled_charsmap" (base64 string)
        @param xsink exception sink
    */
    PrecompiledNormalizer(const QoreHashNode* config, ExceptionSink* xsink);

    std::string normalize(const std::string& input) const override;

private:
    //! The parsed Darts trie for character normalization
    DartsTrie trie;

    //! Returns the byte length of a UTF-8 character from the lead byte
    static size_t utf8CharLen(uint8_t lead_byte);
};

} // namespace QoreTokenizer

#endif // _QORE_TOKENIZER_PRECOMPILED_NORMALIZER_H
