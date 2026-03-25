/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    MetaspacePreTokenizer.cpp

    Metaspace pre-tokenizer implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "pretokenizers/MetaspacePreTokenizer.h"
#include "utils/qore_helpers.h"

#include <cstring>

using namespace QoreTokenizer;

namespace QoreTokenizer {

// U+2581 "LOWER ONE EIGHTH BLOCK" encoded as UTF-8: E2 96 81
static const char* DEFAULT_REPLACEMENT = "\xE2\x96\x81";

MetaspacePreTokenizer::MetaspacePreTokenizer(const std::string& replacement,
        PrependScheme prepend_scheme, bool do_split)
    : replacement(replacement), prepend_scheme(prepend_scheme), do_split(do_split) {
}

MetaspacePreTokenizer::MetaspacePreTokenizer(const QoreHashNode* config, ExceptionSink* xsink)
    : replacement(DEFAULT_REPLACEMENT), prepend_scheme(PrependScheme::Always), do_split(true) {
    if (!config) {
        return;
    }

    // Read "replacement" key
    QoreValue v = config->getKeyValue("replacement");
    if (!v.isNullOrNothing()) {
        const QoreStringNode* str = safeGetString(v);
        if (str) {
            replacement = str->getBuffer();
        }
    }

    // Read "str_rep" as alternative key name (some tokenizer.json files use this)
    v = config->getKeyValue("str_rep");
    if (!v.isNullOrNothing()) {
        const QoreStringNode* str = safeGetString(v);
        if (str) {
            replacement = str->getBuffer();
        }
    }

    // Read "prepend_scheme" key
    v = config->getKeyValue("prepend_scheme");
    if (!v.isNullOrNothing()) {
        const QoreStringNode* str = safeGetString(v);
        if (str) {
            prepend_scheme = parsePrependScheme(str->getBuffer());
        }
    }

    // Read "add_prefix_space" key: if true and prepend_scheme not explicitly set,
    // default to "always"; if false, default to "never"
    // Only apply this if prepend_scheme was not explicitly set
    bool has_prepend_scheme = false;
    {
        QoreValue ps_val = config->getKeyValue("prepend_scheme");
        if (!ps_val.isNullOrNothing()) {
            has_prepend_scheme = true;
        }
    }

    if (!has_prepend_scheme) {
        v = config->getKeyValue("add_prefix_space");
        if (!v.isNullOrNothing()) {
            if (v.getAsBool()) {
                prepend_scheme = PrependScheme::Always;
            } else {
                prepend_scheme = PrependScheme::Never;
            }
        }
    }

    // Read "split" key
    v = config->getKeyValue("split");
    if (!v.isNullOrNothing()) {
        do_split = v.getAsBool();
    }
}

MetaspacePreTokenizer::PrependScheme MetaspacePreTokenizer::parsePrependScheme(
        const std::string& scheme) {
    if (scheme == "always") {
        return PrependScheme::Always;
    }
    if (scheme == "first") {
        return PrependScheme::First;
    }
    if (scheme == "never") {
        return PrependScheme::Never;
    }
    // Default to "always" for unrecognized values
    return PrependScheme::Always;
}

std::vector<PreToken> MetaspacePreTokenizer::pretokenize(const std::string& input) const {
    std::vector<PreToken> result;
    if (input.empty()) {
        return result;
    }

    // Step 1: Replace spaces with the replacement character
    std::string replaced;
    replaced.reserve(input.size() * 2);

    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == ' ') {
            replaced += replacement;
        } else {
            replaced += input[i];
        }
    }

    // Step 2: If not splitting, return as a single piece with optional prepend
    if (!do_split) {
        std::string text;
        if (prepend_scheme == PrependScheme::Always || prepend_scheme == PrependScheme::First) {
            text = replacement + replaced;
        } else {
            text = replaced;
        }
        if (!text.empty()) {
            result.push_back({text, 0, input.size()});
        }
        return result;
    }

    // Step 3: Split on replacement character boundaries
    // We split such that each piece starts with the replacement character
    // (except possibly the first piece if it doesn't start with one)
    std::vector<std::string> pieces;
    std::vector<size_t> piece_starts_in_original;

    size_t rep_len = replacement.size();
    size_t pos = 0;
    size_t orig_pos = 0;

    while (pos < replaced.size()) {
        // Check if we're at a replacement character
        bool at_replacement = false;
        if (pos + rep_len <= replaced.size()) {
            at_replacement = (replaced.compare(pos, rep_len, replacement) == 0);
        }

        if (at_replacement && !pieces.empty()) {
            // Start a new piece at this replacement character
            size_t piece_start = pos;
            size_t piece_orig_start = orig_pos;
            pos += rep_len;
            orig_pos++;  // The replacement corresponds to one space in the original

            // Continue until the next replacement character
            while (pos < replaced.size()) {
                bool next_rep = false;
                if (pos + rep_len <= replaced.size()) {
                    next_rep = (replaced.compare(pos, rep_len, replacement) == 0);
                }
                if (next_rep) {
                    break;
                }
                // Advance one byte in the replaced string and one byte in the original
                pos++;
                orig_pos++;
            }

            pieces.push_back(replaced.substr(piece_start, pos - piece_start));
            piece_starts_in_original.push_back(piece_orig_start);
        } else {
            // First piece or not at replacement: accumulate
            size_t piece_start = pos;
            size_t piece_orig_start = orig_pos;

            if (at_replacement) {
                // First piece starting with replacement
                pos += rep_len;
                orig_pos++;  // corresponds to a space
            }

            // Continue until the next replacement character
            while (pos < replaced.size()) {
                bool next_rep = false;
                if (pos + rep_len <= replaced.size()) {
                    next_rep = (replaced.compare(pos, rep_len, replacement) == 0);
                }
                if (next_rep) {
                    break;
                }
                pos++;
                orig_pos++;
            }

            pieces.push_back(replaced.substr(piece_start, pos - piece_start));
            piece_starts_in_original.push_back(piece_orig_start);
        }
    }

    // Step 4: Apply prepend scheme
    for (size_t i = 0; i < pieces.size(); ++i) {
        std::string text = pieces[i];

        // Check if piece already starts with the replacement
        bool starts_with_rep = false;
        if (text.size() >= rep_len) {
            starts_with_rep = (text.compare(0, rep_len, replacement) == 0);
        }

        if (prepend_scheme == PrependScheme::Always && !starts_with_rep) {
            text = replacement + text;
        } else if (prepend_scheme == PrependScheme::First && i == 0 && !starts_with_rep) {
            text = replacement + text;
        }

        // Calculate original offsets
        size_t orig_start = piece_starts_in_original[i];
        size_t orig_end;
        if (i + 1 < piece_starts_in_original.size()) {
            orig_end = piece_starts_in_original[i + 1];
        } else {
            orig_end = input.size();
        }

        if (!text.empty()) {
            result.push_back({text, orig_start, orig_end});
        }
    }

    // Handle empty input that still needs prepend
    if (result.empty() && prepend_scheme != PrependScheme::Never) {
        result.push_back({replacement, 0, 0});
    }

    return result;
}

} // namespace QoreTokenizer
