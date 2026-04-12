/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SplitPreTokenizer.cpp

    Regex-based split pre-tokenizer

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "SplitPreTokenizer.h"

namespace QoreTokenizer {

static std::string extractPattern(const QoreHashNode* config, ExceptionSink* xsink) {
    QoreValue pv = config->getKeyValue("pattern");
    if (pv.isNullOrNothing()) {
        xsink->raiseException("PRETOKENIZER-CONFIG-ERROR",
            "Split pre-tokenizer requires 'pattern' key");
        return "";
    }
    const QoreHashNode* ph = pv.get<const QoreHashNode>();
    if (ph) {
        // {"Regex": "pattern_string"} or {"String": "literal"}
        QoreValue rv = ph->getKeyValue("Regex");
        if (!rv.isNullOrNothing()) {
            const QoreStringNode* s = rv.get<const QoreStringNode>();
            return s ? s->c_str() : "";
        }
        rv = ph->getKeyValue("String");
        if (!rv.isNullOrNothing()) {
            const QoreStringNode* s = rv.get<const QoreStringNode>();
            // Escape for literal match
            std::string lit = s ? s->c_str() : "";
            return std::regex_replace(lit, std::regex(R"([-[\]{}()*+?.,\\^$|#\s])"),
                R"(\$&)");
        }
    }
    // Plain string
    const QoreStringNode* ps = pv.get<const QoreStringNode>();
    return ps ? ps->c_str() : "";
}

SplitPreTokenizer::SplitPreTokenizer(const QoreHashNode* config, ExceptionSink* xsink) {
    std::string pat = extractPattern(config, xsink);
    if (*xsink) return;
    try {
        pattern = std::regex(pat);
    } catch (const std::regex_error& e) {
        xsink->raiseException("PRETOKENIZER-CONFIG-ERROR",
            "invalid Split regex pattern: %s", e.what());
        return;
    }

    QoreValue bv = config->getKeyValue("behavior");
    const QoreStringNode* bs = bv.isNullOrNothing() ? nullptr : bv.get<const QoreStringNode>();
    behavior = bs ? bs->c_str() : "Removed";
}

std::vector<PreToken> SplitPreTokenizer::pretokenize(const std::string& text) const {
    std::vector<PreToken> result;
    if (text.empty()) return result;

    std::sregex_iterator it(text.begin(), text.end(), pattern);
    std::sregex_iterator end;

    size_t last_end = 0;

    while (it != end) {
        size_t match_start = it->position();
        size_t match_len = it->length();
        std::string matched = it->str();

        // Add text before match
        if (match_start > last_end) {
            std::string before = text.substr(last_end, match_start - last_end);
            if (!before.empty()) {
                result.push_back({before, last_end});
            }
        }

        // Handle the match based on behavior
        if (behavior == "Isolated") {
            result.push_back({matched, match_start});
        } else if (behavior == "MergedWithNext") {
            // Will merge with the next segment in the next iteration
            // For now, just add it
            result.push_back({matched, match_start});
        } else if (behavior == "MergedWithPrevious") {
            if (!result.empty()) {
                result.back().text += matched;
            } else {
                result.push_back({matched, match_start});
            }
        }
        // "Removed" — skip the match

        last_end = match_start + match_len;
        ++it;
    }

    // Add remaining text
    if (last_end < text.size()) {
        std::string remaining = text.substr(last_end);
        if (!remaining.empty()) {
            result.push_back({remaining, last_end});
        }
    }

    return result;
}

} // namespace QoreTokenizer
