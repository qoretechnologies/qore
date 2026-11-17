/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Checks the character offsets of UTF-8 strings, which use the cached length of their ASCII prefix (issue #6138),
 * against the same bytes in a reference encoding, "UTF-8-REF", whose functions are the UTF-8 functions of the library:
 * a string in another encoding does not use the cache, so its results are the results of the code without it.  Every
 * mutator of the C++ API is applied after the cache is filled, in strings that own their buffer and in views, and the
 * string is then checked with every character offset, negative and out of range offsets included.  The last check reads
 * one string from several threads at once.
 */
#include <qore/Qore.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>
#include <latch>
#include <string>
#include <thread>
#include <vector>

static const QoreEncoding* qcs_ref = nullptr;

static size_t ref_length(const char* p, const char* end, bool& invalid) {
    return QCS_UTF8->getLength(p, end, invalid);
}

static size_t ref_byte_len(const char* p, const char* end, size_t c, bool& invalid) {
    return QCS_UTF8->getByteLen(p, end, c, invalid);
}

static size_t ref_char_pos(const char* p, const char* end, bool& invalid) {
    return QCS_UTF8->getCharPos(p, end, invalid);
}

static qore_offset_t ref_char_len(const char* p, size_t valid_len) {
    return QCS_UTF8->getCharLen(p, valid_len);
}

static unsigned failures = 0;
static unsigned checks = 0;

//! the result of a call: an exception, no value, a string or a number
struct Outcome {
    bool raised = false;
    std::string err;
    std::string desc;
    bool null = false;
    std::string bytes;
    int64 num = 0;

    bool operator==(const Outcome& o) const {
        return raised == o.raised && err == o.err && desc == o.desc && null == o.null && bytes == o.bytes
            && num == o.num;
    }

    std::string show() const {
        if (raised) {
            return "exception " + err + ": " + desc;
        }
        if (null) {
            return "NOTHING";
        }
        return "'" + bytes + "' (" + std::to_string(num) + ")";
    }
};

//! takes the exception of the sink; the name of the reference encoding is replaced so that descriptions compare
static void take_exception(Outcome& o, ExceptionSink& xsink) {
    if (!xsink) {
        return;
    }
    o.raised = true;
    QoreValue err = xsink.getExceptionErr();
    QoreValue desc = xsink.getExceptionDesc();
    o.err = err.getType() == NT_STRING ? err.get<const QoreStringNode>()->c_str() : "?";
    o.desc = desc.getType() == NT_STRING ? desc.get<const QoreStringNode>()->c_str() : "?";
    size_t i = o.desc.find("UTF-8-REF");
    if (i != std::string::npos) {
        o.desc.replace(i, 9, "UTF-8");
    }
    xsink.clear();
}

static Outcome node_result(QoreStringNode* rv, ExceptionSink& xsink) {
    Outcome o;
    SimpleRefHolder<QoreStringNode> holder(rv);
    take_exception(o, xsink);
    if (!o.raised) {
        if (!rv) {
            o.null = true;
        } else {
            o.bytes.assign(rv->c_str(), rv->size());
            o.num = static_cast<int64>(rv->length());
        }
    }
    return o;
}

static Outcome string_result(QoreString* rv, ExceptionSink& xsink) {
    Outcome o;
    TempString holder(rv);
    take_exception(o, xsink);
    if (!o.raised) {
        if (!rv) {
            o.null = true;
        } else {
            o.bytes.assign(rv->c_str(), rv->size());
        }
    }
    return o;
}

//! the bytes that a concatenation left in its target, or its exception
static Outcome target_result(int rc, const QoreString& target, ExceptionSink& xsink) {
    Outcome o;
    take_exception(o, xsink);
    if (!o.raised) {
        o.bytes.assign(target.c_str(), target.size());
        o.num = rc;
    }
    return o;
}

static Outcome number_result(int64 n, ExceptionSink& xsink) {
    Outcome o;
    take_exception(o, xsink);
    if (!o.raised) {
        o.num = n;
    }
    return o;
}

static void expect(const Outcome& actual, const Outcome& expected, const std::string& what, const char* label) {
    ++checks;
    if (!(actual == expected)) {
        ++failures;
        if (failures < 50) {
            fprintf(stderr, "FAIL: %s: %s: got %s, expected %s\n", label, what.c_str(), actual.show().c_str(),
                expected.show().c_str());
        }
    }
}

//! the order of the character offsets of a check
enum class Order {
    Forward,
    Backward,
    Strided,
};

//! compares the character offsets of a UTF-8 string with the same bytes in the reference encoding
/** @param s the string; a QoreStringNode, so that substr() can return views
*/
static void compare(const QoreStringNode& s, Order order, const char* label, bool sparse = false) {
    ExceptionSink xsink;
    // the bytes (a part in the middle of its parent is a copy, issue #6239)
    QoreStringNodeHolder ref(new QoreStringNode(s.c_str(), s.size(), qcs_ref));
    const qore_offset_t n = static_cast<qore_offset_t>(ref->length());
    // whether the text is valid UTF-8 (the reference walk finds no invalid byte)
    bool valid;
    {
        const char* b = ref->c_str();
        bool invalid = false;
        ref_length(b, b + ref->size(), invalid);
        valid = !invalid;
    }

    {
        Outcome a, e;
        a.num = static_cast<int64>(s.length());
        e.num = static_cast<int64>(ref->length());
        expect(a, e, "length()", label);
    }

    // every offset of a short string; the offsets at the ends and around the middle of a long one, and every 37th
    std::vector<qore_offset_t> offsets;
    for (qore_offset_t i = -n - 3; i < n + 3; ++i) {
        qore_offset_t from_start = i < 0 ? i + n : i;
        if (sparse ? (from_start < 8 || from_start > n - 8 || !(i % 97))
                : (n <= 600 || from_start < 24 || from_start > n - 24
                    || (from_start > n / 2 - 10 && from_start < n / 2 + 10) || !(i % 61))) {
            offsets.push_back(i);
        }
    }
    if (order == Order::Backward) {
        std::reverse(offsets.begin(), offsets.end());
    } else if (order == Order::Strided) {
        std::vector<qore_offset_t> strided;
        for (size_t start = 0; start < 7; ++start) {
            for (size_t i = start; i < offsets.size(); i += 7) {
                strided.push_back(offsets[(i * 3 + 1) % offsets.size()]);
            }
        }
        offsets.swap(strided);
    }
    // substr() without a length copies the rest of the string, so it is checked at fewer offsets of a long string
    const qore_offset_t step = sparse ? 1000000 : (n > 600 ? 13 : 1);

    for (qore_offset_t i : offsets) {
        std::string at = std::to_string(i);
        // str[i]
        expect(node_result(s.substr(i, 1, &xsink), xsink), node_result(ref->substr(i, 1, &xsink), xsink),
            "node substr(" + at + ", 1)", label);
        expect(string_result(s.QoreString::substr(i, 1, &xsink), xsink),
            string_result(ref->QoreString::substr(i, 1, &xsink), xsink), "substr(" + at + ", 1)", label);
        expect(node_result(s.substr(i, 2, &xsink), xsink), node_result(ref->substr(i, 2, &xsink), xsink),
            "node substr(" + at + ", 2)", label);
        if (!(i % step)) {
            expect(node_result(s.substr(i, &xsink), xsink), node_result(ref->substr(i, &xsink), xsink),
                "node substr(" + at + ")", label);
            expect(node_result(s.substr(i, -1, &xsink), xsink), node_result(ref->substr(i, -1, &xsink), xsink),
                "node substr(" + at + ", -1)", label);
            expect(string_result(s.QoreString::substr(i, &xsink), xsink),
                string_result(ref->QoreString::substr(i, &xsink), xsink), "substr(" + at + ")", label);
            expect(string_result(s.QoreString::substr(i, -2, &xsink), xsink),
                string_result(ref->QoreString::substr(i, -2, &xsink), xsink), "substr(" + at + ", -2)", label);
        }
        if (i >= 0) {
            expect(number_result(s.getByteOffset(i, &xsink), xsink), number_result(ref->getByteOffset(i, &xsink),
                xsink), "getByteOffset(" + at + ")", label);
        }
        // the code points of the characters in the string: getUnicodePointFromUTF8() decodes the bytes as UTF-8
        // whatever the encoding, so the reference string's result, which walks the bytes without the cache, is the
        // expected one; getUnicodePoint() gives the same code point for valid text (for invalid text,
        // getUnicodePointFromUTF8() fails for the whole string, getUnicodePoint() only from the invalid character)
        if (i >= -n && i < n) {
            const unsigned expected = ref->getUnicodePointFromUTF8(i);
            expect(number_result(s.getUnicodePointFromUTF8(i), xsink), number_result(expected, xsink),
                "getUnicodePointFromUTF8(" + at + ")", label);
            if (valid) {
                expect(number_result(s.getUnicodePoint(i, &xsink), xsink), number_result(expected, xsink),
                    "getUnicodePoint(" + at + ")", label);
            }
        }
        // the concatenations of a part of the string, which find their character offsets in it
        // only concatenations of at least one character: a concatenation of no bytes fails an assertion of
        // concat_intern() in the base code as well
        if (!(i % step) && i >= -n && i < n) {
            if (i > 0) {
                QoreString t("<", QCS_UTF8);
                QoreString tr("<", qcs_ref);
                t.concat(&s, static_cast<size_t>(i), &xsink);
                Outcome a = target_result(0, t, xsink);
                tr.concat(*ref, static_cast<size_t>(i), &xsink);
                Outcome e = target_result(0, tr, xsink);
                expect(a, e, "concat(str, " + at + " chars)", label);
            }
            QoreString t("<", QCS_UTF8);
            QoreString tr("<", qcs_ref);
            int rc = t.concat(s, i, &xsink);
            Outcome a = target_result(rc, t, xsink);
            rc = tr.concat(**ref, i, &xsink);
            expect(a, target_result(rc, tr, xsink), "concat(str, " + at + ")", label);
            QoreString t2("<", QCS_UTF8);
            QoreString tr2("<", qcs_ref);
            rc = t2.concat(s, i, 3, &xsink);
            a = target_result(rc, t2, xsink);
            rc = tr2.concat(**ref, i, 3, &xsink);
            expect(a, target_result(rc, tr2, xsink), "concat(str, " + at + ", 3)", label);
        }
        if (!(i % (step * 5))) {
            for (const char* needle : {"a", "\xc3\xa9", "Z", "\xe2\x82\xac"}) {
                QoreString nu(needle, QCS_UTF8);
                QoreString nr(needle, qcs_ref);
                expect(number_result(s.index(nu, i, &xsink), xsink), number_result(ref->index(nr, i, &xsink),
                    xsink), "index('" + std::string(needle) + "', " + at + ")", label);
                expect(number_result(s.rindex(nu, i, &xsink), xsink), number_result(ref->rindex(nr, i, &xsink),
                    xsink), "rindex('" + std::string(needle) + "', " + at + ")", label);
            }
        }
    }
}

//! the contents of the strings checked
static const std::vector<std::pair<const char*, std::string>>& contents() {
    static const std::vector<std::pair<const char*, std::string>> rv = [] {
        std::string ascii;
        for (unsigned i = 0; i < 80; ++i) {
            ascii += static_cast<char>('a' + i % 26);
        }
        std::vector<std::pair<const char*, std::string>> v = {
            {"ascii", ascii + "Z\n"},
            {"empty", ""},
            {"2-byte at start", "\xc3\xa9" + ascii},
            {"3-byte in middle", ascii.substr(0, 40) + "\xe2\x82\xac" + ascii.substr(40)},
            {"4-byte at end", ascii + "\xf0\x9f\x98\x80"},
            {"mixed", "a\xc3\xa9" "b\xe2\x82\xac" "c\xf0\x9f\x98\x80" + ascii + "\xc3\xa9"},
            // invalid UTF-8: the exceptions and the offsets before the invalid byte must not change
            {"truncated 2-byte at end", ascii + "\xc3"},
            {"invalid 3-byte in middle", ascii.substr(0, 40) + "\xe9" "cd" + ascii.substr(40)},
            {"continuation byte in middle", ascii.substr(0, 40) + "\x80" + ascii.substr(40)},
            {"truncated 4-byte at end", ascii + "\xf0\x9f\x98"},
            {"invalid at start", "\xc3" "a" + ascii},
        };
        return v;
    }();
    return rv;
}

//! a mutation of the C++ API
struct Mutation {
    const char* name;
    std::function<void(QoreString&, ExceptionSink*)> apply;
};

static const char* const E_ACUTE = "\xc3\xa9";

static const std::vector<Mutation> mutations = {
    {"none", [](auto&, auto*) {}},
    {"replace-char", [](auto& s, auto*) { s.replaceChar(0, '\xc3'); }},
    {"replace-char-middle", [](auto& s, auto*) { s.replaceChar(s.size() / 2, '\xe9'); }},
    {"terminate-grow", [](auto& s, auto*) { s.terminate(s.size() / 2); s.concat("\xe2\x82\xac" "abc"); }},
    {"raw-write-terminate", [](auto& s, auto*) {
        // bytes written to the buffer and then made the content of the string by terminate()
        size_t n = s.size();
        s.reserve(n + 8);
        char* b = const_cast<char*>(s.getBuffer());
        if (n > 2) {
            b[1] = '\xc3';
            b[2] = '\xa9';
        }
        b[n] = '\xc3';
        b[n + 1] = '\xa9';
        s.terminate(n + 2);
    }},
    {"replace", [](auto& s, auto*) { s.replace(0, 2, E_ACUTE); }},
    // replace() takes a byte offset that must not be after the end of the string
    {"replace-shrink", [](auto& s, auto*) { if (s.size() >= 1) s.replace(1, 5, "\xe2"); }},
    {"replace-remove", [](auto& s, auto*) { s.replace(0, 2, ""); s.concat(E_ACUTE); }},
    {"replace-string", [](auto& s, auto* x) {
        QoreString r("\xe2\x82\xac", QCS_UTF8);
        if (s.size() >= 3) s.replace(3, 1, &r, x);
    }},
    {"replace-all", [](auto& s, auto*) { s.replaceAll("b", E_ACUTE); }},
    {"splice-tail", [](auto& s, auto* x) { s.splice(3, x); s.concat(E_ACUTE); }},
    {"splice-range", [](auto& s, auto* x) { s.splice(1, 2, x); }},
    {"splice-negative", [](auto& s, auto* x) { s.splice(-5, 2, x); }},
    {"splice-replace", [](auto& s, auto* x) { QoreString r("\xc3\xa9\xc3\xa9", QCS_UTF8); s.splice(1, 1, r, x); }},
    {"splice-replace-shrink", [](auto& s, auto* x) { QoreString r("\xc3\xa9", QCS_UTF8); s.splice(0, 9, r, x); }},
    {"extract-tail", [](auto& s, auto* x) { TempString r(s.extract(4, x)); s.concat(E_ACUTE); }},
    {"extract-range", [](auto& s, auto* x) { TempString r(s.extract(2, 3, x)); }},
    {"extract-replace", [](auto& s, auto* x) {
        QoreStringNodeHolder r(new QoreStringNode("\xe2\x82\xac", QCS_UTF8));
        TempString e(s.extract(0, 2, QoreValue(*r), x));
    }},
    {"sprintf", [](auto& s, auto*) { s.sprintf("-%s:%d", E_ACUTE, 17); }},
    {"regex", [](auto& s, auto* x) { QoreString a("a"), b(E_ACUTE, QCS_UTF8); s.regexSubstInPlace(a, b, 0, x); }},
    {"regex-global", [](auto& s, auto* x) {
        QoreString a("[ace]"), b(E_ACUTE, QCS_UTF8);
        s.regexSubstInPlace(a, b, QS_RE_GLOBAL, x);
    }},
    {"append-char", [](auto& s, auto*) { s.concat('\xc3'); s.concat('\xa9'); }},
    {"append-text", [](auto& s, auto*) { s.concat("xyz\xe2\x82\xac"); }},
    {"append-bytes", [](auto& s, auto*) { s.concat("\xe2\x82\xac" "xy", size_t(5)); }},
    {"append-string", [](auto& s, auto* x) { QoreString r("\xf0\x9f\x98\x80", QCS_UTF8); s.concat(&r, x); }},
    {"append-unicode", [](auto& s, auto* x) { s.concatUnicode(0xe9, x); }},
    {"append-padding", [](auto& s, auto*) { s.addch('x', 17); }},
    {"prepend", [](auto& s, auto*) { s.prepend(E_ACUTE); }},
    {"insert", [](auto& s, auto*) { s.insert(E_ACUTE, 1); }},
    {"insert-end", [](auto& s, auto*) { s.insert(E_ACUTE, s.size()); }},
    {"insert-char", [](auto& s, auto*) { s.insertch('\xc3', 1, 1); }},
    {"trim-leading", [](auto& s, auto*) { s.prepend("  "); s.trim_leading(' '); s.prepend(E_ACUTE); }},
    {"trim-leading-chars", [](auto& s, auto*) { s.trim_leading("ab"); }},
    {"trim-trailing", [](auto& s, auto*) { s.trim_trailing(" Z\n"); s.concat(E_ACUTE); }},
    {"trim-unicode", [](auto& s, auto* x) {
        s.prepend(" \xc2\xa0 ");
        s.concat(" \n");
        QoreString chars(" \n\xc2\xa0", QCS_UTF8);
        s.trim(x, &chars);
    }},
    {"trim-single-leading", [](auto& s, auto*) { s.prepend(" "); s.trim_single_leading(' '); }},
    {"trim-single-trailing", [](auto& s, auto*) { s.trim_single_trailing('\n'); s.concat(E_ACUTE); }},
    {"chomp", [](auto& s, auto*) { s.concat("\r\n"); s.chomp(); s.concat(E_ACUTE); }},
    {"uppercase", [](auto& s, auto*) { s.prepend(E_ACUTE); s.toupr(); }},
    {"lowercase", [](auto& s, auto*) { s.prepend("\xc3\x89"); s.tolwr(); }},
    {"remove-bytes", [](auto& s, auto*) { s.removeBytes(1); }},
    {"clear", [](auto& s, auto*) { s.clear(); s.concat("\xc3\xa9" "abc"); }},
    {"reset", [](auto& s, auto*) { s.reset(); s.concat("\xc3\xa9" "abc"); }},
    {"set", [](auto& s, auto*) { s.set("\xc3\xa9" "new"); }},
    {"set-bytes", [](auto& s, auto*) { s.set("\xc3\xa9" "new", size_t(5)); }},
    {"set-string", [](auto& s, auto*) { QoreString r("\xc3\xa9" "new", QCS_UTF8); s.set(r); }},
    {"set-std-string", [](auto& s, auto*) { s.set(std::string("\xc3\xa9" "new"), QCS_UTF8); }},
    {"set-buffer", [](auto& s, auto*) { s.set(strdup("\xc3\xa9" "new"), 5, 6, QCS_UTF8); }},
    {"assign", [](auto& s, auto*) { s = "\xc3\xa9" "new"; }},
    {"take", [](auto& s, auto*) { s.take(strdup("\xc3\xa9" "new")); }},
    {"take-size", [](auto& s, auto*) { s.take(strdup("\xc3\xa9" "new"), 5); }},
    {"take-and-terminate", [](auto& s, auto*) { s.takeAndTerminate(strdup("\xc3\xa9" "new"), 5); }},
    {"give-buffer", [](auto& s, auto*) { free(s.giveBuffer()); s.setEncoding(QCS_UTF8); s.concat("\xc3\xa9"); }},
    {"reserve", [](auto& s, auto*) { s.reserve(s.size() + 16384); s.concat(E_ACUTE); }},
    {"encoding-change", [](auto& s, auto*) {
        // the bytes change while the string is in another encoding
        s.setEncoding(QCS_ISO_8859_1);
        s.replaceChar(0, '\xe9');
        s.setEncoding(QCS_UTF8);
    }},
    {"stateful-trim", [](auto& s, auto* x) {
        // the text is trimmed in a stateful encoding, which swaps the text of the string with the result
        s.prepend("  ");
        s.concat("  ");
        // fills the cache with the spaces
        s.length();
        s.setEncoding(QoreEncodingManager::findCreate("ISO-2022-JP"));
        s.trim(x);
        // text that is not valid in ISO-2022-JP is not changed
        x->clear();
        s.setEncoding(QCS_UTF8);
    }},
};

//! fills the cache: the whole string, half of it or nothing
enum class Prime {
    None,
    Half,
    Full,
};

static void prime(const QoreStringNode& s, Prime p) {
    ExceptionSink xsink;
    if (p == Prime::Full) {
        s.length();
        SimpleRefHolder<QoreStringNode> c(s.substr(-1, 1, &xsink));
    } else if (p == Prime::Half) {
        SimpleRefHolder<QoreStringNode> c(s.substr(static_cast<qore_offset_t>(s.size() / 2), 1, &xsink));
    }
    xsink.clear();
}

static const char* prime_name(Prime p) {
    return p == Prime::None ? "none" : (p == Prime::Half ? "half" : "full");
}

//! checks a mutation of a string that owns its buffer
static void check_owned(const Mutation& m, const char* content_name, const std::string& content, Prime p) {
    std::string label = std::string(m.name) + ", owned, " + content_name + ", prime " + prime_name(p);
    ExceptionSink xsink;
    QoreStringNodeHolder s(new QoreStringNode(content.data(), content.size(), QCS_UTF8));
    prime(**s, p);
    m.apply(**s, &xsink);
    xsink.clear();
    compare(**s, Order::Forward, label.c_str());
    // the cache was filled by the check: a second mutation changes the string again
    m.apply(**s, &xsink);
    xsink.clear();
    compare(**s, Order::Strided, (label + ", twice").c_str());
    s->concat("!");
    compare(**s, Order::Backward, (label + ", appended").c_str());
}

//! checks a mutation of a view at the end of its parent, of a part in its middle (a copy, issue #6239), or of a view
//! of either
static void check_view(const Mutation& m, const char* content_name, const std::string& content, bool middle,
        bool nested, bool prime_parent) {
    std::string label = std::string(m.name) + ", " + (middle ? "middle" : "end") + " view" + (nested ? " of view" : "")
        + ", " + content_name + (prime_parent ? ", parent primed" : "");
    ExceptionSink xsink;
    // a view needs at least 4096 bytes
    std::string body = content;
    while (body.size() < 4200) {
        body += "abcdefghij";
    }
    std::string parent_text = "prefix" + body + (middle ? "suffix" : "");
    QoreStringNodeHolder parent(new QoreStringNode(parent_text.data(), parent_text.size(), QCS_UTF8));
    if (prime_parent) {
        prime(**parent, Prime::Full);
    }
    qore_offset_t body_chars;
    {
        QoreStringNodeHolder tmp(new QoreStringNode(body.data(), body.size(), qcs_ref));
        body_chars = static_cast<qore_offset_t>(tmp->length());
    }
    QoreStringNodeHolder view(middle ? parent->substr(6, body_chars, &xsink) : parent->substr(6, &xsink));
    if (xsink || !view) {
        // invalid text before the view: the reference gives the same exception, which is checked elsewhere
        xsink.clear();
        return;
    }
    if (nested) {
        QoreStringNodeHolder inner(view->substr(0, &xsink));
        if (xsink || !inner) {
            xsink.clear();
            return;
        }
        view = inner.release();
    }
    if (middle && !nested) {
        // a part in the middle of its parent is a copy, which owns its storage (issue #6239)
        Outcome a, e;
        a.num = view->capacity() >= view->size() ? 1 : 0;
        e.num = 1;
        expect(a, e, "middle part capacity()", label.c_str());
    } else {
        // a view has no storage of its own, whatever its cache holds; a substring of fewer than 4096 bytes is a copy
        Outcome a, e;
        a.num = view->size() >= 4096 ? static_cast<int64>(view->capacity()) : 0;
        expect(a, e, "view capacity()", label.c_str());
    }
    if (!middle) {
        // a view at the end of its parent is read without being materialized
        compare(**view, Order::Strided, (label + ", unchanged").c_str());
    }
    m.apply(**view, &xsink);
    xsink.clear();
    compare(**view, Order::Forward, label.c_str());
    view->concat("!");
    compare(**view, Order::Backward, (label + ", appended").c_str());
    // the parent does not change
    Outcome a, e;
    a.bytes.assign(parent->c_str(), parent->size());
    e.bytes = parent_text;
    expect(a, e, "parent bytes", label.c_str());
    compare(**parent, Order::Strided, (label + ", parent").c_str(), true);
}

//! reads strings from several threads at once, each thread in another order
/** Each round starts the threads on a new string, whose cache they fill at the same time
*/
static void check_threads() {
    constexpr unsigned thread_count = 8;
    constexpr unsigned rounds = 10;
    constexpr size_t ascii_chars = 10000;
    const size_t chars = ascii_chars + 4;
    std::string text;
    for (size_t i = 0; i < ascii_chars; ++i) {
        text += static_cast<char>('a' + i % 26);
    }
    text += "\xc3\xa9" "end";
    for (unsigned round = 0; round < rounds; ++round) {
        QoreStringNodeHolder s(new QoreStringNode(text.data(), text.size(), QCS_UTF8));
        std::vector<unsigned> errors(thread_count, 0);
        std::latch start(thread_count);
        std::vector<std::thread> threads;
        for (unsigned t = 0; t < thread_count; ++t) {
            threads.emplace_back([&, t]() {
                ExceptionSink xsink;
                start.arrive_and_wait();
                for (size_t k = 0; k < chars; ++k) {
                    // every thread reads all characters, starting in another place and stepping by another stride
                    size_t i = (k * (2 * t + 1) + t * 2503 + round * 997) % chars;
                    SimpleRefHolder<QoreStringNode> c(s->substr(static_cast<qore_offset_t>(i), 1, &xsink));
                    bool ok;
                    if (!c || xsink) {
                        ok = false;
                    } else if (i < ascii_chars) {
                        ok = c->size() == 1 && c->c_str()[0] == static_cast<char>('a' + i % 26);
                    } else if (i == ascii_chars) {
                        ok = c->size() == 2 && !memcmp(c->c_str(), "\xc3\xa9", 2);
                    } else {
                        ok = c->size() == 1 && c->c_str()[0] == "end"[i - ascii_chars - 1];
                    }
                    if (!ok) {
                        ++errors[t];
                    }
                    xsink.clear();
                    if (!(k % 1000) && s->length() != chars) {
                        ++errors[t];
                    }
                }
            });
        }
        for (auto& th : threads) {
            th.join();
        }
        for (unsigned t = 0; t < thread_count; ++t) {
            ++checks;
            if (errors[t]) {
                ++failures;
                fprintf(stderr, "FAIL: round %u: thread %u read %u wrong characters\n", round, t, errors[t]);
            }
        }
    }
}

int main(int argc, char** argv) {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    qcs_ref = QoreEncodingManager::add("UTF-8-REF", "UTF-8 without the ASCII prefix cache", 4, ref_length,
        ref_byte_len, ref_char_pos, ref_char_len);
    for (const auto& m : mutations) {
        if (argc > 1 && strcmp(argv[1], m.name)) {
            continue;
        }
        for (const auto& c : contents()) {
            for (Prime p : {Prime::None, Prime::Half, Prime::Full}) {
                check_owned(m, c.first, c.second, p);
            }
            // a view has more than 4096 bytes: the views are checked with fewer contents
            if (strcmp(c.first, "ascii") && strcmp(c.first, "2-byte at start") && strcmp(c.first, "3-byte in middle")
                    && strcmp(c.first, "invalid 3-byte in middle")) {
                continue;
            }
            // views at the end of their parent and in its middle, views of views, and parents with and without their
            // cache filled
            check_view(m, c.first, c.second, false, false, true);
            check_view(m, c.first, c.second, false, true, false);
            check_view(m, c.first, c.second, true, false, false);
            check_view(m, c.first, c.second, true, true, true);
        }
    }
    if (argc < 2 || !strcmp(argv[1], "threads")) {
        check_threads();
    }
    qore_cleanup();
    if (!failures && checks) {
        printf("PASS: %u ASCII prefix cache checks\n", checks);
        return 0;
    }
    fprintf(stderr, "FAIL: %u of %u checks failed\n", failures, checks);
    return 1;
}
