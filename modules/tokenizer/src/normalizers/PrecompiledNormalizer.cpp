/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    PrecompiledNormalizer.cpp

    Precompiled normalizer implementation using Darts trie

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "normalizers/PrecompiledNormalizer.h"
#include "utils/qore_helpers.h"

using namespace QoreTokenizer;

namespace QoreTokenizer {

size_t PrecompiledNormalizer::utf8CharLen(uint8_t lead_byte) {
    if (lead_byte < 0x80) {
        return 1;
    } else if ((lead_byte & 0xE0) == 0xC0) {
        return 2;
    } else if ((lead_byte & 0xF0) == 0xE0) {
        return 3;
    } else if ((lead_byte & 0xF8) == 0xF0) {
        return 4;
    }
    return 1; // invalid byte, treat as single byte
}

PrecompiledNormalizer::PrecompiledNormalizer(const QoreHashNode* config, ExceptionSink* xsink) {
    if (!config) {
        xsink->raiseException("TOKENIZER-NORMALIZER-ERROR",
            "PrecompiledNormalizer config is null");
        return;
    }

    // Read the "precompiled_charsmap" field (base64-encoded string)
    const QoreStringNode* charsmap_node = safeGetString(
        config->getKeyValue("precompiled_charsmap"));
    if (!charsmap_node) {
        xsink->raiseException("TOKENIZER-NORMALIZER-ERROR",
            "PrecompiledNormalizer config missing 'precompiled_charsmap' field");
        return;
    }

    // Decode base64 using Qore's built-in parseBase64()
    SimpleRefHolder<BinaryNode> bin(parseBase64(charsmap_node->c_str(),
        static_cast<int>(charsmap_node->size()), xsink));
    if (*xsink) {
        return;
    }
    if (!bin || !bin->size()) {
        xsink->raiseException("TOKENIZER-NORMALIZER-ERROR",
            "PrecompiledNormalizer: failed to decode base64 charsmap data");
        return;
    }

    // Parse the binary data into the Darts trie
    if (!trie.parse(static_cast<const uint8_t*>(bin->getPtr()), bin->size())) {
        xsink->raiseException("TOKENIZER-NORMALIZER-ERROR",
            "PrecompiledNormalizer: failed to parse precompiled charsmap trie data");
        return;
    }
}

std::string PrecompiledNormalizer::normalize(const std::string& input) const {
    if (!trie.isValid() || input.empty()) {
        return input;
    }

    std::string result;
    result.reserve(input.size());

    const uint8_t* data = reinterpret_cast<const uint8_t*>(input.data());
    size_t len = input.size();
    size_t pos = 0;

    while (pos < len) {
        // Find the longest prefix match at the current position
        std::vector<TrieMatch> matches = trie.commonPrefixSearch(data + pos, len - pos);

        if (!matches.empty()) {
            // Use the longest match (first in the list, since results are sorted by length desc)
            const TrieMatch& best = matches[0];
            std::string replacement = trie.getNormalizedString(best.first);
            result += replacement;
            pos += best.second;
        } else {
            // No match: pass through one UTF-8 character unchanged
            size_t char_len = utf8CharLen(data[pos]);
            if (pos + char_len > len) {
                char_len = len - pos; // safety: don't read past end
            }
            result.append(input, pos, char_len);
            pos += char_len;
        }
    }

    return result;
}

} // namespace QoreTokenizer
