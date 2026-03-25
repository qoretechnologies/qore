/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    MetaspaceDecoder.cpp

    Metaspace token decoder implementation

    Replaces the metaspace character (U+2581 "▁") back to spaces, and
    optionally strips the leading space based on the prepend_scheme.

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "decoders/MetaspaceDecoder.h"
#include "utils/qore_helpers.h"

#include <cassert>

using namespace QoreTokenizer;

namespace QoreTokenizer {

// U+2581 LOWER ONE EIGHTH BLOCK encoded as UTF-8: E2 96 81
static const std::string DEFAULT_METASPACE = "\xE2\x96\x81";

MetaspaceDecoder::MetaspaceDecoder(const QoreHashNode* config, ExceptionSink* xsink)
        : replacement(DEFAULT_METASPACE) {
    assert(config);

    std::string repl_str = safeGetStringKey(config, "replacement");
    if (!repl_str.empty()) {
        replacement = repl_str;
    }

    // Check if prepend_scheme was explicitly provided in the config
    QoreValue scheme_val = config->getKeyValue("prepend_scheme");
    if (!scheme_val.isNullOrNothing() && scheme_val.getType() == NT_STRING) {
        const QoreStringNode* scheme_str = scheme_val.get<const QoreStringNode>();
        if (scheme_str && scheme_str->size() > 0) {
            prepend_scheme = scheme_str->c_str();
        } else {
            // Explicit but empty: fall back to add_prefix_space
            QoreValue add_prefix = config->getKeyValue("add_prefix_space");
            prepend_scheme = add_prefix.getAsBool() ? "always" : "never";
        }
    } else {
        // prepend_scheme not present in config: use legacy add_prefix_space
        // boolean to determine behavior
        QoreValue add_prefix = config->getKeyValue("add_prefix_space");
        prepend_scheme = add_prefix.getAsBool() ? "always" : "never";
    }
}

std::string MetaspaceDecoder::decode(const std::vector<std::string>& tokens) const {
    // Join all tokens
    std::string joined;
    for (const auto& tok : tokens) {
        joined += tok;
    }

    // Replace all occurrences of the metaspace character with spaces
    if (!replacement.empty()) {
        std::string result;
        result.reserve(joined.size());
        size_t pos = 0;
        while (pos < joined.size()) {
            size_t found = joined.find(replacement, pos);
            if (found == std::string::npos) {
                result += joined.substr(pos);
                break;
            }
            result += joined.substr(pos, found - pos);
            result += ' ';
            pos = found + replacement.size();
        }
        joined = result;
    }

    // Strip leading space if prepend_scheme was "always" or "first"
    if ((prepend_scheme == "always" || prepend_scheme == "first")
            && !joined.empty() && joined[0] == ' ') {
        joined = joined.substr(1);
    }

    return joined;
}

} // namespace QoreTokenizer
