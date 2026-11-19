/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Checks the UTF-8 validation that IconvHelper applies where the platform's iconv accepts malformed UTF-8 input,
 * against an independent decoder for every sequence of one to three bytes and every four-byte sequence with a
 * valid lead byte; run by utf8-conversion-validation.qtest
 */
#include <qore/Qore.h>
#include "qore/intern/IconvHelper.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

static void require(bool valid, const std::string& message) {
    if (!valid) {
        throw std::runtime_error(message);
    }
}

static std::string hex(const unsigned char* p, size_t len) {
    std::string rv;
    char buf[4];
    for (size_t i = 0; i < len; ++i) {
        std::snprintf(buf, sizeof(buf), "%02x", p[i]);
        rv += buf;
    }
    return rv;
}

//! the result of the reference decoder for one sequence
struct Expected {
    //! the length of the well-formed prefix
    size_t valid;
    //! the input after the prefix is a truncated, well-formed sequence
    bool incomplete;
};

//! decodes the first character with the definitions of RFC 3629 (code point ranges, shortest form, no surrogates)
static Expected reference(const unsigned char* p, size_t len) {
    if (p[0] < 0x80) {
        return {len == 1 ? 1u : 0u, false};
    }
    size_t n;
    unsigned cp;
    if ((p[0] & 0xe0) == 0xc0) {
        n = 2;
        cp = p[0] & 0x1f;
    } else if ((p[0] & 0xf0) == 0xe0) {
        n = 3;
        cp = p[0] & 0x0f;
    } else if ((p[0] & 0xf8) == 0xf0) {
        n = 4;
        cp = p[0] & 0x07;
    } else {
        return {0, false};
    }
    size_t have = len < n ? len : n;
    for (size_t i = 1; i < have; ++i) {
        if ((p[i] & 0xc0) != 0x80) {
            return {0, false};
        }
        cp = (cp << 6) | (p[i] & 0x3f);
    }
    // the bits so far decide shortest form and range when the sequence is truncated: complete it with the smallest
    // and the largest continuations, and require one of them to be a valid character
    unsigned lo = cp;
    unsigned hi = cp;
    for (size_t i = have; i < n; ++i) {
        lo = (lo << 6);
        hi = (hi << 6) | 0x3f;
    }
    static const unsigned min_cp[] = {0, 0, 0x80, 0x800, 0x10000};
    auto valid_cp = [n](unsigned c) {
        return c >= min_cp[n] && c <= 0x10ffff && !(c >= 0xd800 && c <= 0xdfff);
    };
    bool any = false;
    for (unsigned c = lo; c <= hi; ++c) {
        if (valid_cp(c)) {
            any = true;
            break;
        }
    }
    if (!any) {
        return {0, false};
    }
    if (len < n) {
        return {0, true};
    }
    // a complete sequence followed by more bytes: only the one character is checked here
    return {len == n ? n : 0, false};
}

//! the UTF-8 encoding's character functions use the same definition as the validator (qore#6604): the length of
//! the character, 0 for a malformed one, minus its length for a truncated one, and the string length with its
//! validity
static void check_encoding(const unsigned char* p, size_t len, const Expected& e) {
    const char* c = reinterpret_cast<const char*>(p);
    qore_offset_t cl = QCS_UTF8->getCharLen(c, len);
    bool invalid = false;
    size_t chars = QCS_UTF8->getLength(c, c + len, invalid);
    if (e.valid == 0 && !e.incomplete) {
        require(cl == 0, "getCharLen() of malformed " + hex(p, len) + ": " + std::to_string(cl));
        require(invalid, "getLength() of malformed " + hex(p, len) + " valid");
        return;
    }
    if (e.incomplete) {
        // minus the length of the whole character, which the lead byte gives
        qore_offset_t full = p[0] < 0xe0 ? 2 : (p[0] < 0xf0 ? 3 : 4);
        require(cl == -full, "getCharLen() of truncated " + hex(p, len) + ": " + std::to_string(cl));
        require(invalid, "getLength() of truncated " + hex(p, len) + " valid");
        return;
    }
    require(cl == static_cast<qore_offset_t>(len),
        "getCharLen() of well-formed " + hex(p, len) + ": " + std::to_string(cl));
    require(!invalid && chars == 1, "getLength() of well-formed " + hex(p, len) + ": " + std::to_string(chars));
}

static void check(const unsigned char* p, size_t len) {
    Expected e = reference(p, len);
    check_encoding(p, len, e);
    bool incomplete = true;
    size_t valid = IconvHelper::validUtf8Prefix(reinterpret_cast<const char*>(p), len, incomplete);
    // a sequence longer than one character: the validator also checks the rest, the reference only the first
    if (e.valid == 0 && !e.incomplete) {
        require(valid == 0 && !incomplete, "malformed " + hex(p, len) + " accepted");
        return;
    }
    if (e.incomplete) {
        require(valid == 0 && incomplete, "truncated " + hex(p, len) + " not reported as incomplete");
        return;
    }
    require(valid == len && !incomplete, "well-formed " + hex(p, len) + " rejected");
}

static void check_all() {
    unsigned char b[4];
    // every sequence of one and two bytes
    for (unsigned i = 0; i < 256; ++i) {
        b[0] = static_cast<unsigned char>(i);
        check(b, 1);
        for (unsigned j = 0; j < 256; ++j) {
            b[1] = static_cast<unsigned char>(j);
            if (i >= 0xc2 && i <= 0xdf) {
                check(b, 2);
            } else if (i >= 0xe0 && i <= 0xf4) {
                // three- and four-byte lead bytes: the truncated sequence
                check(b, 2);
            }
        }
    }
    // every three-byte sequence with a three- or four-byte lead byte
    for (unsigned i = 0xe0; i <= 0xf4; ++i) {
        b[0] = static_cast<unsigned char>(i);
        for (unsigned j = 0; j < 256; ++j) {
            b[1] = static_cast<unsigned char>(j);
            for (unsigned k = 0; k < 256; ++k) {
                b[2] = static_cast<unsigned char>(k);
                check(b, 3);
            }
        }
    }
    // four-byte sequences: every first continuation with the boundary values of the others
    static const unsigned char tails[] = {0x00, 0x7f, 0x80, 0xbf, 0xc0, 0xff};
    for (unsigned i = 0xf0; i <= 0xf4; ++i) {
        b[0] = static_cast<unsigned char>(i);
        for (unsigned j = 0; j < 256; ++j) {
            b[1] = static_cast<unsigned char>(j);
            for (unsigned char k : tails) {
                b[2] = k;
                for (unsigned char l : tails) {
                    b[3] = l;
                    check(b, 4);
                }
            }
        }
    }
    // the prefix stops at the first malformed or truncated character after valid ones
    const char* mixed = "ab\xc3\xa9" "c\xc2 d";
    bool incomplete = true;
    require(IconvHelper::validUtf8Prefix(mixed, strlen(mixed), incomplete) == 5 && !incomplete,
        "prefix before a malformed sequence");
    const char* truncated = "ab\xe2\x82";
    require(IconvHelper::validUtf8Prefix(truncated, strlen(truncated), incomplete) == 2 && incomplete,
        "prefix before a truncated sequence");
    require(IconvHelper::validUtf8Prefix("", 0, incomplete) == 0 && !incomplete, "empty input");
}

//! a character after any number of ASCII bytes, and text with short ASCII runs between other characters: the validator
//! and the string length give the result of the reference (qore#6604)
static void check_ascii_runs() {
    struct Seq {
        const char* bytes;
        // the result after the ASCII prefix: 1 = valid, 0 = malformed, -1 = truncated at the end of the input
        int kind;
    };
    static const Seq seqs[] = {
        {"\xc3\xa9", 1}, {"\xe2\x82\xac", 1}, {"\xf0\x9f\x98\x80", 1}, {"\x80", 0}, {"\xc0\xaf", 0},
        {"\xed\xa0\x80", 0}, {"\xf4\x90\x80\x80", 0}, {"\xff", 0}, {"\xc3", -1}, {"\xe2\x82", -1},
    };
    for (size_t k = 0; k <= 24; ++k) {
        for (const Seq& q : seqs) {
            for (size_t after : {static_cast<size_t>(0), static_cast<size_t>(3), static_cast<size_t>(11)}) {
                // a truncated sequence is truncated only at the end of the input
                if (q.kind < 0 && after) {
                    continue;
                }
                std::string in = std::string(k, 'a') + q.bytes + std::string(after, 'b');
                bool incomplete = q.kind >= 0;
                size_t valid = IconvHelper::validUtf8Prefix(in.data(), in.size(), incomplete);
                std::string label = std::to_string(k) + " ASCII bytes + " +
                    hex(reinterpret_cast<const unsigned char*>(q.bytes), strlen(q.bytes)) + " + " +
                    std::to_string(after);
                if (q.kind == 1) {
                    require(valid == in.size() && !incomplete, "valid " + label);
                } else {
                    require(valid == k && incomplete == (q.kind < 0), "prefix of " + label);
                }
                bool invalid = false;
                size_t chars = QCS_UTF8->getLength(in.data(), in.data() + in.size(), invalid);
                size_t expected_chars = q.kind == 1 ? k + 1 + after : k;
                require(invalid == (q.kind != 1) && chars == expected_chars, "getLength() of " + label);
            }
        }
    }
    // short ASCII runs between other characters, then an invalid byte after them
    std::string text;
    for (int i = 0; i < 64; ++i) {
        text += "caf\xc3\xa9 na\xc3\xafve r\xc3\xa9sum\xc3\xa9 ";
    }
    bool incomplete = true;
    require(IconvHelper::validUtf8Prefix(text.data(), text.size(), incomplete) == text.size() && !incomplete,
        "short ASCII runs");
    std::string bad = text + "x\x80";
    require(IconvHelper::validUtf8Prefix(bad.data(), bad.size(), incomplete) == text.size() + 1 && !incomplete,
        "an invalid byte after short ASCII runs");
}

//! clears a cancellation request of the current thread
struct CancelReset {
    ~CancelReset() {
        qore_clear_thread_cancel();
    }
};

//! the validation is a cancellation point every CancelCheckBytes bytes when given an exception sink
static void check_cancellation() {
    for (size_t size : {IconvHelper::CancelCheckBytes - 1, IconvHelper::CancelCheckBytes,
            IconvHelper::CancelCheckBytes * 4 + 1}) {
        std::string bytes(size, 'a');
        ExceptionSink xsink;
        bool incomplete = true;
        bool cancelled = false;
        // without a cancellation request, the whole input is valid
        require(IconvHelper::validUtf8Prefix(bytes.data(), size, incomplete, &xsink, &cancelled) == size
            && !incomplete && !cancelled && !xsink, "validation without cancellation");
        CancelReset reset;
        require(!qore_cancel_thread(q_gettid(), "UTF-8 validation"), "cannot request cancellation");
        IconvHelper::validUtf8Prefix(bytes.data(), size, incomplete, &xsink, &cancelled);
        // input shorter than one check interval has no cancellation point
        bool expected = size > IconvHelper::CancelCheckBytes;
        require(cancelled == expected && static_cast<bool>(xsink) == expected,
            "cancellation at " + std::to_string(size) + " bytes");
        xsink.clear();
        // without an exception sink, the validation is not a cancellation point
        cancelled = false;
        require(IconvHelper::validUtf8Prefix(bytes.data(), size, incomplete, nullptr, &cancelled) == size
            && !cancelled, "validation without an exception sink");
    }
}

//! the conversion with validation stops at malformed UTF-8 as a strict iconv does, whatever the platform's iconv
static void check_conversion() {
    struct Case {
        const char* input;
        size_t len;
        // the expected errno, or 0 for a successful conversion
        int err;
        // the byte offset where the conversion stops
        size_t offset;
    };
    static const Case cases[] = {
        {"a\xf4\x90\x80\x80 b", 7, EILSEQ, 1},
        {"a\xc2 b", 4, EILSEQ, 1},
        {"ab\x80", 3, EILSEQ, 2},
        {"a\xed\xa0\x80 b", 6, EILSEQ, 1},
        {"a\xe2\x82", 3, EINVAL, 1},
        {"a\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80" "b", 11, 0, 11},
    };
    for (const Case& c : cases) {
        ExceptionSink xsink;
        IconvHelper conv(QCS_UTF16LE, QCS_UTF8, &xsink, true);
        require(conv.isValid() && !xsink, "cannot open the conversion");
        char in[32];
        std::memcpy(in, c.input, c.len);
        char out[128];
        char* ib = in;
        size_t il = c.len;
        char* ob = out;
        size_t ol = sizeof(out);
        size_t rc = conv.iconv(&ib, &il, &ob, &ol, &xsink);
        std::string label = hex(reinterpret_cast<const unsigned char*>(c.input), c.len);
        if (c.err) {
            require(rc == static_cast<size_t>(-1) && errno == c.err, "conversion of " + label + " not rejected");
        } else {
            require(rc != static_cast<size_t>(-1), "conversion of " + label + " failed");
        }
        require(static_cast<size_t>(ib - in) == c.offset && il == c.len - c.offset,
            "conversion of " + label + " stopped at the wrong offset");
        // the valid prefix was converted: two bytes per BMP character in UTF-16LE
        require(ob > out || !c.offset, "the valid prefix of " + label + " was not converted");
    }
}

// the public check of the text of a parser or another input (q_get_invalid_text_offset(), q_check_text_encoding())
static void check_public_api() {
    struct Case {
        const QoreEncoding* enc;
        const char* bytes;
        size_t len;
        qore_offset_t offset;
    };
    const QoreEncoding* sjis = QEM.findCreate("Shift_JIS");
    const Case cases[] = {
        {QCS_UTF8, "", 0, -1},
        {QCS_UTF8, "abc", 3, -1},
        {QCS_UTF8, "a\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80", 10, -1},
        {QCS_UTF8, "ab\xe9", 3, 2},
        {QCS_UTF8, "a\xed\xa0\x80" "b", 5, 1},          // a surrogate
        {QCS_UTF8, "abc\xc0\xaf" "d", 6, 3},             // an overlong form
        {QCS_UTF8, "\xf4\x90\x80\x80", 4, 0},          // above U+10FFFF
        {QCS_UTF8, "ab\xe2\x82", 4, 2},               // cut off by the end of the text
        {QCS_UTF8, "a\0b\xff", 4, 3},                 // a NUL byte is a character
        {QCS_ISO_8859_1, "\xe9\xff\x80", 3, -1},       // a single-byte encoding has no invalid bytes
        {QCS_USASCII, "ab\x80", 3, 2},
        {sjis, "a\x83\x5c", 3, -1},
        {sjis, "a\x83", 2, 1},
        {QCS_UTF16LE, "a\0\0\xd8", 4, 2},             // a lone high surrogate
    };
    for (const Case& c : cases) {
        std::string label = std::string(c.enc->getCode()) + " " + hex(reinterpret_cast<const unsigned char*>(c.bytes),
            c.len);
        require(q_get_invalid_text_offset(c.enc, c.bytes, c.len) == c.offset, "q_get_invalid_text_offset() of "
            + label);
        ExceptionSink xsink;
        int rc = q_check_text_encoding(c.enc, c.bytes, c.len, "test()", "test text", &xsink);
        if (c.offset < 0) {
            require(!rc && !xsink, "q_check_text_encoding() of valid " + label + " raised an exception");
            continue;
        }
        require(rc == -1 && xsink, "q_check_text_encoding() of " + label + " raised no exception");
        QoreValue err = xsink.getExceptionErr();
        QoreValue desc = xsink.getExceptionDesc();
        require(err.getType() == NT_STRING && !strcmp(err.get<const QoreStringNode>()->c_str(), "INVALID-ENCODING"),
            "q_check_text_encoding() of " + label + " raised another exception");
        std::string expected = std::string("test(): the test text is not valid ") + c.enc->getCode()
            + ": invalid byte sequence at byte offset " + std::to_string(c.offset);
        require(desc.getType() == NT_STRING && expected == desc.get<const QoreStringNode>()->c_str(),
            "q_check_text_encoding() of " + label + ": description: "
                + (desc.getType() == NT_STRING ? desc.get<const QoreStringNode>()->c_str() : "(none)"));
        xsink.clear();
    }
}

int main() {
    qore_init(QL_MIT, "UTF-8", false, QLO_DISABLE_SIGNAL_HANDLING);
    int status = 0;
    try {
        check_all();
        check_ascii_runs();
        check_cancellation();
        check_conversion();
        check_public_api();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        status = 1;
    }
    qore_cleanup();
    if (!status) {
        std::printf("PASS: UTF-8 validation\n");
    }
    return status;
}
