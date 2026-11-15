/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
// The offsets and lengths given to the QoreString C++ API stay within the string: an offset past the end of the
// string is clamped to its end, and a length past the end of the string to the rest of it.  Run under valgrind, so
// that an access outside the string's buffer is reported even where the result looks right.
#include <qore/Qore.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

static int checks = 0;

static void check_str(const QoreString& str, const char* expected, size_t expected_len, const char* what) {
    ++checks;
    if (str.size() != expected_len || memcmp(str.c_str(), expected, expected_len)) {
        fprintf(stderr, "FAIL: %s: got %zu bytes \"%s\", expected %zu bytes \"%s\"\n", what, str.size(), str.c_str(),
            expected_len, expected);
        exit(1);
    }
}

static void check_str(const QoreString& str, const char* expected, const char* what) {
    check_str(str, expected, strlen(expected), what);
}

// QoreString::replace(size_t, size_t, const char*): an offset at or past the end inserts at the end
static void replace_chars() {
    const size_t len = 6;
    for (size_t offset = len; offset <= len + 64; ++offset) {
        {
            QoreString str("abcdef");
            str.replace(offset, 0, "XY");
            check_str(str, "abcdefXY", "replace(char*) inserting at or past the end");
        }
        {
            QoreString str("abcdef");
            str.replace(offset, 3, "XY");
            check_str(str, "abcdefXY", "replace(char*) replacing at or past the end");
        }
        {
            QoreString str("abcdef");
            str.replace(offset, 3, (const char*)nullptr);
            check_str(str, "abcdef", "replace(char*) removing at or past the end");
        }
        {
            QoreString str("abcdef");
            str.replace(offset, 3, "");
            check_str(str, "abcdef", "replace(char*) with an empty string at or past the end");
        }
    }
    // offsets so large that offset + length wraps around
    {
        QoreString str("abcdef");
        str.replace((size_t)-1, 2, "XY");
        check_str(str, "abcdefXY", "replace(char*) at the largest offset");
        str.replace((size_t)-3, (size_t)-1, (const char*)nullptr);
        check_str(str, "abcdefXY", "replace(char*) removing at the largest offset");
    }
    // a length past the end is clamped to the rest of the string
    {
        QoreString str("abcdef");
        str.replace(4, 100, "XY");
        check_str(str, "abcdXY", "replace(char*) with a length past the end");
        str.replace(2, (size_t)-1, (const char*)nullptr);
        check_str(str, "ab", "replace(char*) removing with the largest length");
    }
}

// QoreString::replace(size_t, size_t, const QoreString*, ExceptionSink*): the same, also with a string that is
// converted to the encoding of the target first
static void replace_string() {
    ExceptionSink xsink;
    QoreString xy("XY");
    QoreString empty;
    QoreString xy_utf8("XY", QCS_UTF8);
    for (size_t offset = 6; offset <= 6 + 64; ++offset) {
        {
            QoreString str("abcdef");
            str.replace(offset, 0, &xy, &xsink);
            check_str(str, "abcdefXY", "replace(QoreString*) inserting at or past the end");
        }
        {
            QoreString str("abcdef");
            str.replace(offset, 2, &xy, &xsink);
            check_str(str, "abcdefXY", "replace(QoreString*) replacing at or past the end");
        }
        {
            QoreString str("abcdef");
            str.replace(offset, 2, &empty, &xsink);
            check_str(str, "abcdef", "replace(QoreString*) with an empty string at or past the end");
        }
        {
            QoreString str("abcdef");
            str.replace(offset, 2, (const QoreString*)nullptr, &xsink);
            check_str(str, "abcdef", "replace(QoreString*) removing at or past the end");
        }
        {
            // the replacement is converted to the encoding of the target
            QoreString str("abcdef", QCS_ISO_8859_1);
            str.replace(offset, 1, &xy_utf8, &xsink);
            check_str(str, "abcdefXY", "replace(QoreString*) converted at or past the end");
        }
    }
    assert(!xsink);
}

// QoreString::concat(const QoreString&, qore_offset_t pos, qore_offset_t len, ExceptionSink*): the characters taken
// from the source are within it, and the length is a number of characters from the position, in a single-byte
// encoding as in a multi-byte one
static void concat_range() {
    ExceptionSink xsink;
    const QoreEncoding* encs[] = {QCS_ISO_8859_1, QCS_UTF8};
    for (const QoreEncoding* enc : encs) {
        QoreString src("abcdef", enc);
        // positions and lengths that reach before the start are only checked in a single-byte encoding here: in a
        // multi-byte encoding, they are character positions resolved as in index() (see issue #6246)
        struct {
            qore_offset_t pos;
            qore_offset_t len;
            const char* expected;
            bool single_byte_only;
        } cases[] = {
            {0, 1, "a", false},
            {1, 1, "b", false},
            {4, 1, "e", false},
            {4, 2, "ef", false},
            {4, 100, "ef", false},
            {5, 100, "f", false},
            {6, 1, "", false},
            {100, 1, "", false},
            {0, -1, "abcde", false},
            {2, -1, "cde", false},
            {2, -4, "", false},
            {2, -100, "", true},
            {-2, 1, "e", false},
            {-2, 100, "ef", false},
            {-100, 2, "ab", true},
            {-100, -5, "a", true},
        };
        for (auto& c : cases) {
            if (c.single_byte_only && enc->isMultiByte()) {
                continue;
            }
            QoreString str(enc);
            assert(!str.concat(src, c.pos, c.len, &xsink));
            char what[128];
            snprintf(what, sizeof(what), "%s concat(pos: %lld, len: %lld)", enc->getCode(), (long long)c.pos,
                (long long)c.len);
            check_str(str, c.expected, what);
        }
    }
    // in a multi-byte encoding, a length past the end is the rest of the string after the position, in characters
    {
        // "aé€😀b" in UTF-8
        QoreString src("a\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80" "b", QCS_UTF8);
        struct {
            qore_offset_t pos;
            qore_offset_t len;
            const char* expected;
        } mb_cases[] = {
            {3, 100, "\xf0\x9f\x98\x80" "b"},
            {3, 2, "\xf0\x9f\x98\x80" "b"},
            {4, 0x7fffffffffffffffll, "b"},
            {1, 3, "\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80"},
            {-2, 100, "\xf0\x9f\x98\x80" "b"},
            {5, 1, ""},
        };
        for (auto& c : mb_cases) {
            QoreString str(QCS_UTF8);
            assert(!str.concat(src, c.pos, c.len, &xsink));
            check_str(str, c.expected, "UTF-8 concat(pos, len) with multi-byte characters");
        }
    }

    // concat(str, pos) from the end of the string or after it, and from an empty string, concatenates nothing, in a
    // single-byte encoding as in a multi-byte one
    for (const QoreEncoding* enc : encs) {
        QoreString src("abcdef", enc);
        QoreString empty(enc);
        for (qore_offset_t pos : {(qore_offset_t)6, (qore_offset_t)7, (qore_offset_t)100}) {
            QoreString str(enc);
            assert(!str.concat(src, pos, &xsink));
            check_str(str, "", "concat(pos) at or after the end");
        }
        for (qore_offset_t pos : {(qore_offset_t)-100, (qore_offset_t)-1, (qore_offset_t)0, (qore_offset_t)1}) {
            QoreString str(enc);
            assert(!str.concat(empty, pos, &xsink));
            check_str(str, "", "concat(pos) of an empty string");
            assert(!str.concat(empty, pos, 1, &xsink));
            check_str(str, "", "concat(pos, len) of an empty string");
        }
        {
            QoreString str(enc);
            assert(!str.concat(src, 2, &xsink));
            check_str(str, "cdef", "concat(pos)");
        }
    }
    assert(!xsink);
}

// QoreString::rfind(char, qore_offset_t): a position at or past the end searches from the last byte
static void rfind_char() {
    QoreString str("abca");
    for (qore_offset_t pos = 3; pos < 70; ++pos) {
        ++checks;
        assert(str.rfind('a', pos) == 3);
        ++checks;
        assert(str.rfind('\0', pos) == -1);
        ++checks;
        assert(str.rfindAny("a", pos) == 3);
        ++checks;
        assert(str.rfindAny("x", pos) == -1);
    }
    QoreString empty;
    ++checks;
    assert(empty.rfind('a', 0) == -1);
    ++checks;
    assert(empty.rfind('\0', 0) == -1);
    ++checks;
    assert(empty.rfindAny("a", 0) == -1);
}

// runs the checks given by name as arguments, or all of them
int main(int argc, char** argv) {
    qore_init(QL_MIT);
    struct {
        const char* name;
        void (*run)();
    } sections[] = {
        {"replace-chars", replace_chars},
        {"replace-string", replace_string},
        {"concat-range", concat_range},
        {"rfind-char", rfind_char},
    };
    for (auto& section : sections) {
        bool run = argc < 2;
        for (int i = 1; i < argc && !run; ++i) {
            run = !strcmp(argv[i], section.name);
        }
        if (run) {
            section.run();
        }
    }
    qore_cleanup();
    printf("PASS: %d string offset checks\n", checks);
    return 0;
}
