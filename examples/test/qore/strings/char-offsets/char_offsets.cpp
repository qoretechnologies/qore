/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
// Character offsets in the QoreString C++ API mean the same in every encoding (issue #6246):
// - QoreString::getUnicodePoint() and QoreString::getUnicodePointFromBytePos() at or after the end of a string give
//   0 (and a character length of 0) without reading the bytes after the string
// - QoreString::concat() with a character position and length takes the same characters in multi-byte encodings as
//   in single-byte ones, also with a position or a negative length that reaches before the start
// Run it under valgrind to check that no byte after a string is read:
//   valgrind --leak-check=full qore-char-offsets-test
#include <qore/Qore.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static int checks = 0;

static void fail(const char* fmt, const char* a, long long b, long long c, const char* d) {
    fprintf(stderr, "FAIL: ");
    fprintf(stderr, fmt, a, b, c, d);
    fprintf(stderr, "\n");
    exit(1);
}

// the characters of the source text in UTF-8: one-, two-, four- and three-byte characters
static const std::vector<std::string> Chars = {"a", "\xc3\xa9", "\xf0\x9f\x98\x80", "d", "\xe2\x82\xac", "z"};

// the encodings checked; every one can hold every character of Chars
static const char* Encodings[] = {"UTF-8", "UTF-16LE", "UTF-16BE", "UTF-32LE", "UTF-32BE"};

static const QoreEncoding* get_enc(const char* name) {
    const QoreEncoding* enc = QEM.findCreate(name);
    assert(enc);
    return enc;
}

// returns the string of the characters [start, end) of Chars in the given encoding
static QoreString* chars(const QoreEncoding* enc, size_t start, size_t end) {
    QoreString utf8(QCS_UTF8);
    for (size_t i = start; i < end && i < Chars.size(); ++i) {
        utf8.concat(Chars[i].c_str());
    }
    ExceptionSink xsink;
    QoreString* rv = utf8.convertEncoding(enc, &xsink);
    assert(!xsink && rv);
    return rv;
}

// the character range of concat(str, pos, len) on a string of n characters, as in a single-byte encoding
static void ref_range(qore_offset_t n, qore_offset_t pos, qore_offset_t len, bool has_len, size_t& start,
        size_t& end) {
    if (pos < 0) {
        pos += n;
        if (pos < 0) {
            pos = 0;
        }
    }
    if (pos >= n) {
        start = end = 0;
        return;
    }
    qore_offset_t rest = n - pos;
    if (!has_len) {
        len = rest;
    } else if (len < 0) {
        len += rest;
        if (len <= 0) {
            start = end = 0;
            return;
        }
    } else if (len > rest) {
        len = rest;
    }
    start = pos;
    end = pos + len;
}

static void check_concat() {
    const qore_offset_t n = Chars.size();
    for (const char* name : Encodings) {
        const QoreEncoding* enc = get_enc(name);
        SimpleRefHolder<QoreStringNode> src(new QoreStringNode(enc));
        {
            TempString all(chars(enc, 0, n));
            ExceptionSink xsink;
            src->concat(*all, &xsink);
            assert(!xsink);
        }
        for (qore_offset_t pos = -n - 3; pos <= n + 3; ++pos) {
            // concat(str, pos); a position at or after the end, which concatenates nothing, is checked with the
            // offset bounds of the C++ API (issue #6238)
            if (pos < n) {
                size_t start, end;
                ref_range(n, pos, 0, false, start, end);
                TempString expected(chars(enc, start, end));
                QoreString str(enc);
                ExceptionSink xsink;
                assert(!str.concat(**src, pos, &xsink));
                assert(!xsink);
                ++checks;
                if (str.size() != expected->size() || memcmp(str.c_str(), expected->c_str(), str.size())) {
                    fail("%s concat(pos: %lld) (%lld): wrong characters%s", name, (long long)pos, 0, "");
                }
            }
            // concat(str, pos, len); a length of 0 concatenates nothing
            for (qore_offset_t len = -n - 3; len <= n + 3; ++len) {
                if (!len) {
                    continue;
                }
                size_t start, end;
                ref_range(n, pos, len, true, start, end);
                TempString expected(chars(enc, start, end));
                QoreString str(enc);
                ExceptionSink xsink;
                assert(!str.concat(**src, pos, len, &xsink));
                assert(!xsink);
                ++checks;
                if (str.size() != expected->size() || memcmp(str.c_str(), expected->c_str(), str.size())) {
                    fail("%s concat(pos: %lld, len: %lld): wrong characters%s", name, (long long)pos,
                        (long long)len, "");
                }
            }
        }
    }
}

static void check_unicode_past_end() {
    const char* names[] = {"UTF-8", "ISO-8859-1", "UTF-16LE", "UTF-16BE", "UTF-32LE", "UTF-32BE", "UCS-2LE",
        "UCS-2BE"};
    for (const char* name : names) {
        const QoreEncoding* enc = get_enc(name);
        // "a" and "é" fit in every encoding checked
        TempString str(chars(enc, 0, 2));
        TempString empty(chars(enc, 0, 0));
        ExceptionSink xsink;
        for (size_t i = 0; i < 4; ++i) {
            ++checks;
            unsigned cp = str->getUnicodePoint(2 + i, &xsink);
            assert(!xsink);
            if (cp) {
                fail("%s getUnicodePoint(%lld): %lld expected 0%s", name, 2 + i, cp, "");
            }
            ++checks;
            cp = empty->getUnicodePoint(i, &xsink);
            assert(!xsink);
            if (cp) {
                fail("%s getUnicodePoint(%lld) of an empty string: %lld expected 0%s", name, i, cp, "");
            }
            // byte offsets at and after the end
            unsigned len = 99;
            ++checks;
            cp = str->getUnicodePointFromBytePos(str->size() + i, len, &xsink);
            assert(!xsink);
            if (cp || len) {
                fail("%s getUnicodePointFromBytePos(%lld): %lld expected 0%s", name, str->size() + i, cp, "");
            }
        }
        // the last character is still found
        ++checks;
        unsigned cp = str->getUnicodePoint(1, &xsink);
        assert(!xsink);
        if (cp != 0xe9) {
            fail("%s getUnicodePoint(%lld): %lld expected 233%s", name, 1, cp, "");
        }
    }
}

int main() {
    qore_init(QL_MIT);
    check_concat();
    check_unicode_past_end();
    qore_cleanup();
    printf("PASS: %d character offset checks\n", checks);
    return 0;
}
