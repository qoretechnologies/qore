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
        : replacement(DEFAULT_METASPACE), prepend_scheme("always") {
    assert(config);

    std::string repl_str = safeGetStringKey(config, "replacement");
    if (!repl_str.empty()) {
        replacement = repl_str;
    }

    std::string scheme_str = safeGetStringKey(config, "prepend_scheme");
    if (!scheme_str.empty()) {
        prepend_scheme = scheme_str;
    }

    // Also check legacy "add_prefix_space" boolean
    if (prepend_scheme.empty()) {
        QoreValue add_prefix = config->getKeyValue("add_prefix_space");
        if (add_prefix.getAsBool()) {
            prepend_scheme = "always";
        } else {
            prepend_scheme = "never";
        }
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
