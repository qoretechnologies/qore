/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SplitPreTokenizer.cpp

    Regex-based split pre-tokenizer

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "SplitPreTokenizer.h"
#include "utils/qore_helpers.h"

#include <cctype>

namespace QoreTokenizer {

//! Escapes an ASCII literal so it matches verbatim as a PCRE2 pattern
/** Escapes every ASCII non-word character (so regex metacharacters become
    literals); multibyte UTF-8 bytes (>= 0x80) and word characters are left
    untouched.  Used for the \c {"String": "..."} literal pattern form.
*/
static std::string escapeLiteral(const std::string& lit) {
    std::string out;
    out.reserve(lit.size() * 2);
    for (unsigned char c : lit) {
        if (c < 0x80 && !(std::isalnum(c) || c == '_')) {
            out.push_back('\\');
        }
        out.push_back(static_cast<char>(c));
    }
    return out;
}

//! Returns the byte length of the UTF-8 character starting at \a pos (>= 1)
static size_t utf8CharLen(const std::string& s, size_t pos) {
    if (pos >= s.size()) {
        return 1;
    }
    unsigned char c = static_cast<unsigned char>(s[pos]);
    if (c < 0x80) {
        return 1;
    }
    if ((c & 0xE0) == 0xC0) {
        return 2;
    }
    if ((c & 0xF0) == 0xE0) {
        return 3;
    }
    if ((c & 0xF8) == 0xF0) {
        return 4;
    }
    return 1; // invalid lead byte: advance one byte to make forward progress
}

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
            return safeGetStdString(rv);
        }
        rv = ph->getKeyValue("String");
        if (!rv.isNullOrNothing()) {
            return escapeLiteral(safeGetStdString(rv));
        }
    }
    // Plain string
    return safeGetStdString(pv);
}

SplitPreTokenizer::SplitPreTokenizer(const QoreHashNode* config, ExceptionSink* xsink) {
    std::string pat = extractPattern(config, xsink);
    if (*xsink) {
        return;
    }

    // PCRE2_UTF: codepoint-correct matching over UTF-8 input.
    // PCRE2_UCP: \w, \d, \s, \p{...} use full Unicode properties.
    // These together make HuggingFace Split patterns (\p{L}, \p{N}, (?i:...),
    // lookarounds) work as written — std::regex (ECMAScript) supports none of them.
    int errornumber = 0;
    PCRE2_SIZE erroroffset = 0;
    re = pcre2_compile(reinterpret_cast<PCRE2_SPTR>(pat.c_str()), PCRE2_ZERO_TERMINATED,
        PCRE2_UTF | PCRE2_UCP, &errornumber, &erroroffset, nullptr);
    if (!re) {
        PCRE2_UCHAR buf[256];
        pcre2_get_error_message(errornumber, buf, sizeof(buf));
        xsink->raiseException("PRETOKENIZER-CONFIG-ERROR",
            "invalid Split regex pattern at offset %d: %s",
            static_cast<int>(erroroffset), reinterpret_cast<const char*>(buf));
        return;
    }

    // Attach JIT code if the platform and pattern support it; failures are ignored because PCRE2
    // falls back to the interpreter when no JIT code is present.  This is not just a speed-up:
    // PCRE2's interpreter disables its required-code-unit start-of-match optimization once the
    // remaining subject reaches 5,000,000 code units (REQ_CU_MAX * 1000 in pcre2_match.c), after
    // which an unanchored pattern is retried at every offset -- quadratic in the text length.  The
    // JIT has no such cutoff.  pcre2_jit_compile() modifies the pcre2_code block, so it must run
    // here, before the pretokenizer is visible to other threads.
    pcre2_jit_compile(re, PCRE2_JIT_COMPLETE);

    QoreValue bv = config->getKeyValue("behavior");
    std::string bs = bv.isNullOrNothing() ? "" : safeGetStdString(bv);
    behavior = bs.empty() ? "Removed" : std::move(bs);
}

SplitPreTokenizer::~SplitPreTokenizer() {
    if (re) {
        pcre2_code_free(re);
    }
}

std::vector<PreToken> SplitPreTokenizer::pretokenize(const std::string& text) const {
    std::vector<PreToken> result;
    if (text.empty() || !re) {
        return result;
    }

    pcre2_match_data* md = pcre2_match_data_create_from_pattern(re, nullptr);
    if (!md) {
        return result;
    }
    ON_BLOCK_EXIT(pcre2_match_data_free, md);

    PCRE2_SPTR subject = reinterpret_cast<PCRE2_SPTR>(text.c_str());
    PCRE2_SIZE subject_len = text.size();
    PCRE2_SIZE offset = 0;
    size_t last_end = 0;

    // For the "MergedWithNext" behavior a matched delimiter is held back and
    // prepended to the following emitted segment; pending_prefix is empty in all
    // other behaviors (a Split has a single behavior for every match).
    std::string pending_prefix;
    size_t pending_start = 0;
    auto emitSegment = [&](const std::string& seg, size_t start, size_t end) {
        if (!pending_prefix.empty()) {
            result.push_back({pending_prefix + seg, pending_start, end});
            pending_prefix.clear();
        } else {
            result.push_back({seg, start, end});
        }
    };

    while (offset <= subject_len) {
        int rc = pcre2_match(re, subject, subject_len, offset, 0, md, nullptr);
        if (rc < 0) {
            // PCRE2_ERROR_NOMATCH (no further matches) or a hard error (e.g. bad
            // UTF-8): stop; any remaining text is emitted as the trailing segment.
            break;
        }
        PCRE2_SIZE* ov = pcre2_get_ovector_pointer(md);
        size_t match_start = ov[0];
        size_t match_end = ov[1];
        size_t match_len = match_end - match_start;

        // Emit any unmatched text before this match as its own segment.
        if (match_start > last_end) {
            emitSegment(text.substr(last_end, match_start - last_end), last_end, match_start);
        }

        // Handle the matched (delimiter) text according to the configured behavior.
        if (behavior == "Isolated") {
            emitSegment(text.substr(match_start, match_len), match_start, match_end);
        } else if (behavior == "MergedWithNext") {
            // Hold the delimiter back; it prepends to the next emitted segment.
            if (pending_prefix.empty()) {
                pending_start = match_start;
            }
            pending_prefix += text.substr(match_start, match_len);
        } else if (behavior == "MergedWithPrevious") {
            if (!result.empty()) {
                result.back().text += text.substr(match_start, match_len);
                result.back().end = match_end;
            } else {
                emitSegment(text.substr(match_start, match_len), match_start, match_end);
            }
        }
        // "Removed": drop the matched (delimiter) text.

        last_end = match_end;

        // Advance the search position. Guard against zero-length matches (which
        // would otherwise loop forever) by stepping one full UTF-8 character so
        // the next search starts on a codepoint boundary (required by PCRE2_UTF).
        if (match_end > offset) {
            offset = match_end;
        } else {
            offset = match_end + utf8CharLen(text, match_end);
        }
    }

    // Emit any trailing unmatched text (flushing a pending MergedWithNext prefix).
    if (last_end < text.size()) {
        emitSegment(text.substr(last_end), last_end, text.size());
    } else if (!pending_prefix.empty()) {
        // A delimiter matched at the very end with nothing after it.
        result.push_back({pending_prefix, pending_start, text.size()});
    }

    return result;
}

} // namespace QoreTokenizer
