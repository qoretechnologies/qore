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

    std::string repl_str = configStringKey(config, "replacement", xsink);
    if (*xsink) {
        return;
    }
    if (!repl_str.empty()) {
        replacement = repl_str;
    }

    // Check if prepend_scheme was explicitly provided in the config
    QoreValue scheme_val = config->getKeyValue("prepend_scheme");
    if (!scheme_val.isNullOrNothing() && scheme_val.getType() == NT_STRING) {
        std::string scheme_str = configString(scheme_val, "configuration value 'prepend_scheme'", xsink);
        if (*xsink) {
            return;
        }
        if (!scheme_str.empty()) {
            prepend_scheme = std::move(scheme_str);
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

std::vector<std::string> MetaspaceDecoder::decodeChain(std::vector<std::string> tokens) const {
    for (size_t i = 0; i < tokens.size(); ++i) {
        auto& token = tokens[i];
        const bool skip = i == 0 && prepend_scheme != "never";
        size_t pos = 0;
        while ((pos = token.find(replacement, pos)) != std::string::npos) {
            token.replace(pos, replacement.size(), skip ? "" : " ");
            pos += !skip;
        }
    }
    return tokens;
}

} // namespace QoreTokenizer
