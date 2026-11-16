/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
// The byte-position searches of the QoreString C++ API (issue #6271):
// - the bindex() overloads (const QoreString&, const char*, const std::string&) and find(const char*/std::string)
//   treat a negative position the same way: an offset from the end, and one before the start is the start
// - the brindex() overloads and rfind(const char*/std::string) find a match that lies within the bytes up to and
//   including the position, as rindex() does with characters
// - findAny() finds the first of any of the given bytes at or after the position, as rfindAny() finds the last
#include <qore/Qore.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static int checks = 0;

static void check(qore_offset_t got, qore_offset_t expected, const char* what, const char* needle, qore_offset_t pos) {
    ++checks;
    if (got != expected) {
        fprintf(stderr, "FAIL: %s(\"%s\", %lld): %lld expected %lld\n", what, needle, (long long)pos, (long long)got,
            (long long)expected);
        exit(1);
    }
}

// the reference: the first match at or after pos (a negative pos is an offset from the end; before the start is the
// start)
static qore_offset_t ref_bindex(const std::string& hay, const std::string& needle, qore_offset_t pos) {
    qore_offset_t n = hay.size();
    if (pos < 0) {
        pos += n;
        if (pos < 0) {
            pos = 0;
        }
    }
    if (pos >= n) {
        return -1;
    }
    size_t i = hay.find(needle, pos);
    return i == std::string::npos ? -1 : (qore_offset_t)i;
}

// the reference: the last match that lies within the bytes up to and including pos
static qore_offset_t ref_brindex(const std::string& hay, const std::string& needle, qore_offset_t pos) {
    qore_offset_t n = hay.size();
    if (pos < 0) {
        pos += n;
        if (pos < 0) {
            return -1;
        }
    }
    if (pos >= n) {
        pos = n - 1;
    }
    qore_offset_t start = pos - (qore_offset_t)needle.size() + 1;
    if (start < 0) {
        return -1;
    }
    size_t i = hay.rfind(needle, start);
    return i == std::string::npos ? -1 : (qore_offset_t)i;
}

static void check_bindex_brindex() {
    const std::string hay = "abcabdab";
    const char* needles[] = {"a", "ab", "b", "bd", "abd", "dab", "x", "abcabdab", "abcabdabx"};
    QoreString str(hay.c_str());
    for (const char* needle : needles) {
        std::string sn(needle);
        QoreString qn(needle);
        for (qore_offset_t pos = -12; pos <= 12; ++pos) {
            qore_offset_t exp = ref_bindex(hay, sn, pos);
            check(str.bindex(qn, pos), exp, "bindex(const QoreString&)", needle, pos);
            check(str.bindex(needle, pos), exp, "bindex(const char*)", needle, pos);
            check(str.bindex(sn, pos), exp, "bindex(const std::string&)", needle, pos);
            check(str.find(needle, pos), exp, "find(const char*)", needle, pos);
            check(str.find(sn, pos), exp, "find(const std::string&)", needle, pos);
            exp = ref_brindex(hay, sn, pos);
            check(str.brindex(qn, pos), exp, "brindex(const QoreString&)", needle, pos);
            check(str.brindex(needle, pos), exp, "brindex(const char*)", needle, pos);
            check(str.brindex(sn, pos), exp, "brindex(const std::string&)", needle, pos);
            check(str.rfind(needle, pos), exp, "rfind(const char*)", needle, pos);
            check(str.rfind(sn, pos), exp, "rfind(const std::string&)", needle, pos);
        }
    }
    // the examples of the issue
    QoreString abc("abc");
    check(abc.brindex("ab", 1), 0, "brindex", "ab", 1);
    check(abc.bindex(QoreString("c"), -10), 2, "bindex(const QoreString&)", "c", -10);
    check(abc.bindex(std::string("c"), -10), 2, "bindex(const std::string&)", "c", -10);
}

// the reference: the first byte at or after pos that is one of the bytes in chars
static qore_offset_t ref_find_any(const std::string& hay, const char* chars, qore_offset_t pos) {
    qore_offset_t n = hay.size();
    if (pos < 0) {
        pos += n;
        if (pos < 0) {
            pos = 0;
        }
    }
    for (qore_offset_t i = pos; i < n; ++i) {
        if (hay[i] && strchr(chars, hay[i])) {
            return i;
        }
    }
    return -1;
}

static qore_offset_t ref_rfind_any(const std::string& hay, const char* chars, qore_offset_t pos) {
    qore_offset_t n = hay.size();
    if (pos < 0) {
        pos += n;
        if (pos < 0) {
            return -1;
        }
    }
    if (pos >= n) {
        pos = n - 1;
    }
    for (qore_offset_t i = pos; i >= 0; --i) {
        if (hay[i] && strchr(chars, hay[i])) {
            return i;
        }
    }
    return -1;
}

static void check_find_any() {
    // with an embedded null byte: the search covers every byte of the string
    const std::string hay("path/to\0file.txt", 16);
    QoreString str(hay.c_str(), hay.size(), QCS_DEFAULT);
    const char* sets[] = {"/", "./", "t", "xt", "zq", "eo", "/."};
    for (const char* chars : sets) {
        for (qore_offset_t pos = -20; pos <= 20; ++pos) {
            check(str.findAny(chars, pos), ref_find_any(hay, chars, pos), "findAny", chars, pos);
            check(str.rfindAny(chars, pos), ref_rfind_any(hay, chars, pos), "rfindAny", chars, pos);
        }
    }
    // any of the characters, not the substring
    QoreString path("a/b.c");
    check(path.findAny("./"), 1, "findAny", "./", 0);
    check(path.findAny("./", 2), 3, "findAny", "./", 2);
    check(path.rfindAny("./"), 3, "rfindAny", "./", -1);
}

// runs the checks given by name as arguments ("bindex", "find-any"), or all of them
int main(int argc, char** argv) {
    qore_init(QL_MIT);
    bool all = argc < 2;
    auto selected = [&](const char* name) {
        for (int i = 1; i < argc; ++i) {
            if (!strcmp(argv[i], name)) {
                return true;
            }
        }
        return all;
    };
    if (selected("bindex")) {
        check_bindex_brindex();
    }
    if (selected("find-any")) {
        check_find_any();
    }
    qore_cleanup();
    printf("PASS: %d byte offset checks\n", checks);
    return 0;
}
