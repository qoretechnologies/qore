/* -*-indent-tabs-mode: nil -*- */
/*
    QoreRegex.cpp

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#include <qore/Qore.h>
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreValueExceptionOps.h"

#include <memory>

QoreStringNode* qore_regex_subject_result(QoreStringNode* utf8, const QoreEncoding* encoding, ExceptionSink* xsink) {
    if (encoding == QCS_UTF8) {
        return utf8;
    }
    SimpleRefHolder<QoreStringNode> value(utf8);
    return value->convertEncoding(encoding, xsink);
}

QoreStringNode* qore_regex_capture(const char* bytes, size_t len, const QoreEncoding* encoding, ExceptionSink* xsink) {
    return qore_regex_subject_result(new QoreStringNode(bytes, len, QCS_UTF8), encoding, xsink);
}

void qore_regex_set_named_capture(QoreHashNode& groups, const char* utf8_name, QoreValue value, ExceptionSink* xsink) {
    if (QCS_DEFAULT == QCS_UTF8) {
        groups.setKeyValue(utf8_name, value, xsink);
    } else {
        groups.setKeyValue(QoreString(utf8_name, QCS_UTF8), value, xsink);
    }
}

QoreRegex::QoreRegex() : QoreRegexBase(new QoreString(QCS_UTF8)) {
}

QoreRegex::QoreRegex(const QoreString& s, int64 opts, ExceptionSink* xsink) : QoreRegexBase(PCRE2_UTF | (int)opts),
        global(opts & QRE_GLOBAL ? true : false) {
    if (check_re_options(options)) {
        xsink->raiseException("REGEX-OPTION-ERROR", QLLD " contains invalid option bits", opts);
        options = 0;
    }
    pattern_cache = s.c_str();  // preserve pattern for AOT serialization
    parseRT(&s, xsink);
}

QoreRegex::QoreRegex(const char* s, int64 opts, ExceptionSink* xsink) : QoreRegexBase(PCRE2_UTF | (int)opts),
        global(opts & QRE_GLOBAL ? true : false) {
    if (check_re_options(options)) {
        xsink->raiseException("REGEX-OPTION-ERROR", QLLD " contains invalid option bits", opts);
        options = 0;
    }
    if (s) {
        pattern_cache = s;  // preserve pattern for AOT serialization
    }
    parseRT(s, xsink);
}

QoreRegex::~QoreRegex() {
}

void QoreRegex::concat(char c) {
   str->concat(c);
}

void QoreRegex::parseRT(const QoreString* pattern, ExceptionSink* xsink) {
    // convert to UTF-8 if necessary
    TempEncodingHelper t(pattern, QCS_UTF8, xsink);
    if (*xsink) {
        return;
    }

    parseRT(t->c_str(), xsink);
}

void QoreRegex::parseRT(const char* pattern, ExceptionSink* xsink) {
    int errorcode;
    PCRE2_SIZE eo;

    //printd(5, "QoreRegex::parseRT(%s) this=%p\n", t->c_str(), this);

    // \C matches one byte, which can be part of a character, and so gives a string that is not valid UTF-8: it is
    // refused when the pattern is compiled (qore#6554)
    p = pcre2_compile(reinterpret_cast<PCRE2_SPTR8>(pattern), PCRE2_ZERO_TERMINATED,
        options | PCRE2_NEVER_BACKSLASH_C, &errorcode, &eo, nullptr);
    if (!p) {
        PCRE2_UCHAR buffer[qore_pcre2_errorbuf_size];
        pcre2_get_error_message(errorcode, buffer, sizeof(buffer));
        //printd(5, "QoreRegex::parse() error parsing '%s': %s", pattern, (char* )err);
        xsink->raiseException("REGEX-COMPILATION-ERROR", "Regular expression compilation failed at %lu ('%s'): %s",
            eo, pattern, buffer);
        return;
    }
    jitCompile();
}

void QoreRegex::parse(q_get_loc_t get_loc) {
    ExceptionSink xsink;
    savePattern();
    parseRT(str, &xsink);
    delete str;
    str = nullptr;
    if (xsink.isEvent()) {
        // override the exception location with the real parse location in case of an error
        qore_exception_sink_ops(xsink).overrideLocation(*get_loc());
        qore_program_private::addParseException(getProgram(), xsink);
    }
}

bool QoreRegex::exec(const QoreString* target, ExceptionSink* xsink) const {
    TempEncodingHelper t(target, QCS_UTF8, xsink);
    if (!t)
        return false;

    return exec(t->c_str(), t->strlen(), xsink);
}

bool QoreRegex::exec(const char* str, size_t len) const {
    // internal callers matching short identifiers (ex: namespace and class name filters) have no
    // exception sink; a resource-limit failure can only mean "no match" for them
    return exec(str, len, nullptr);
}

bool QoreRegex::exec(const char* str, size_t len, ExceptionSink* xsink) const {
    // the PCRE docs say that if we don't send an ovector here the library may have to malloc
    // memory, so, even though we don't need the results, we include the vector to avoid
    // extraneous malloc()s

    pcre2_match_data* md = pcre2_match_data_create_from_pattern(p, nullptr);
    ON_BLOCK_EXIT(pcre2_match_data_free, md);

    int rc = qore_pcre2_match(p, reinterpret_cast<PCRE2_SPTR8>(str), len, 0, 0, md);
    // rc == 0 means the ovector was not large enough, which should not happen when using
    // pcre2_match_data_create_from_pattern()
    assert(rc);
    //printd(5, "QoreRegex::exec(%s) this: %p pre_exec() rc=%d\n", str, this, rc);
    if (rc < 1) {
        qore_pcre2_check_match_error(rc, xsink, str, len);
        return false;
    }
    return true;
}

// return type: *list<*string>
QoreListNode* QoreRegex::extractSubstrings(const QoreString* target, ExceptionSink* xsink) const {
    TempEncodingHelper t(target, QCS_UTF8, xsink);
    if (!t) {
        return nullptr;
    }
    const char* subject = t->c_str();
    const size_t subject_len = t->size();
    const QoreEncoding* subject_encoding = t.is_temp() ? target->getEncoding() : QCS_UTF8;

    ReferenceHolder<QoreListNode> l(xsink);

    PCRE2_SIZE offset = 0;

    pcre2_match_data* md = pcre2_match_data_create_from_pattern(p, nullptr);
    ON_BLOCK_EXIT(pcre2_match_data_free, md);

    // See extractWithPattern() below for the rationale; first call validates,
    // subsequent calls skip the O(N) UTF-8 rescan.
    uint32_t match_options = 0;

    while (true) {
        // a global match over a large subject can iterate many times; stay cancellable
        if (qore_check_cancel(xsink, "regular expression match")) {
            return nullptr;
        }

        if (offset >= subject_len) {
            break;
        }

        int rc = qore_pcre2_match(p, reinterpret_cast<PCRE2_SPTR8>(subject), subject_len, offset,
            match_options, md);
        //printd(5, "QoreRegex::extractSubstrings('%s') =~ /xxx/ = %d (global: %d)\n", subject + offset, rc, global);
        // rc == 0 means the ovector was not large enough, which should not happen when using
        // pcre2_match_data_create_from_pattern()
        assert(rc);

        if (rc < 1) {
            if (qore_pcre2_check_match_error(rc, xsink, subject, subject_len)) {
                assert(*xsink);
                return nullptr;
            }
            break;
        }

        PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(md);

        // issue #2083: pcre2 can return a match with a zero length, in which case we must ignore it
        // otherwise there will be an infinite loop
        if (rc > 1 && (rc != 2 || ovector[2] != ovector[3])) {
            int x = 0;
            while (++x < rc) {
                int pos = x * 2;
                if (ovector[pos] == -1) {
                    if (!l) {
                        l = new QoreListNode(stringOrNothingTypeInfo);
                    }
                    l->push(QoreValue(), xsink);
                    continue;
                }
                QoreStringNode* tstr = qore_regex_capture(subject + ovector[pos], ovector[pos + 1] - ovector[pos],
                    subject_encoding, xsink);
                if (!tstr) {
                    return nullptr;
                }
                if (!l) {
                    l = new QoreListNode(stringOrNothingTypeInfo);
                }
                //printd(5, "substring %d: %d - %d (len %d) tstr: '%s' (%d)\n", x, ovector[pos], ovector[pos + 1],
                //    ovector[pos + 1] - ovector[pos], tstr->c_str(), (int)tstr->size());
                l->push(tstr, xsink);
            }

            offset = ovector[(x - 1) * 2 + 1];
            //printd(5, "QoreRegex::extractSubstrings() offset: %d size: %d ovector[%d]: %d\n", offset, t->strlen(),
            //    (x - 1) * 2 + 1, ovector[(x - 1) * 2 + 1]);
        } else {
            break;
        }

        if (!global) {
            break;
        }
        // Subject is verified UTF-8 after the first successful match; skip rescan.
        match_options = PCRE2_NO_UTF_CHECK;
    }

    return l.release();
}

// return type: list<string>
QoreListNode* QoreRegex::extractWithPattern(const QoreString& target, bool include_pattern,
        ExceptionSink* xsink, int limit) const {
    TempEncodingHelper t(target, QCS_UTF8, xsink);
    if (!t) {
        assert(*xsink);
        return nullptr;
    }

    ReferenceHolder<QoreListNode> l(new QoreListNode(stringTypeInfo), xsink);
    PCRE2_SIZE offset = 0;
    auto make_piece = [&](size_t end) {
        if (!offset && end == t->size()) {
            return new QoreStringNode(target);
        }
        return qore_regex_capture(t->c_str() + offset, end - offset, target.getEncoding(), xsink);
    };
    PCRE2_SIZE search_offset = 0;
    int split_count = 0;

    pcre2_match_data* md = pcre2_match_data_create_from_pattern(p, nullptr);
    ON_BLOCK_EXIT(pcre2_match_data_free, md);

    // PCRE2 re-validates the entire subject as UTF-8 from byte 0 on every
    // pcre2_match() call.  In a split loop that's O(N) per call, O(N^2)
    // overall.  We pay the validation once on the first call, then pass
    // PCRE2_NO_UTF_CHECK so subsequent calls skip the rescan.  Anything
    // less than UTF-8 valid would have surfaced as a UTF-8 error code on
    // the first call (handled below) — once we see a successful match the
    // subject is known good.
    uint32_t match_options = 0;

    while (true) {
        // a split over a large subject can iterate many times; stay cancellable
        if (qore_check_cancel(xsink, "regular expression split")) {
            return nullptr;
        }

        if (search_offset >= t->size()) {
            break;
        }

        // if limit is reached, add the remainder and stop
        if (limit > 0 && split_count >= limit - 1) {
            break;
        }

        int rc = qore_pcre2_match(p, reinterpret_cast<PCRE2_SPTR8>(t->c_str()), t->size(), search_offset,
            match_options, md);
        printd(5, "QoreRegex::extractWithPattern('%s') = %d\n", t->c_str() + offset, rc);

        assert(rc);

        if (rc < 1) {
            if (qore_pcre2_check_match_error(rc, xsink, t->c_str(), t->size())) {
                assert(*xsink);
                return nullptr;
            }
            break;
        }

        PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(md);
        PCRE2_SIZE piece_end = ovector[include_pattern ? 1 : 0];
        auto* piece = make_piece(piece_end);
        if (!piece) {
            return nullptr;
        }
        l->push(piece, xsink);
        offset = ovector[1];
        search_offset = ovector[0] == offset ? qore_regex_after_empty_match(t->c_str(), t->size(), offset) : offset;
        ++split_count;
        // First successful match — subject is verified valid UTF-8; skip
        // re-validation on subsequent calls.
        match_options = PCRE2_NO_UTF_CHECK;
    }

    if (offset < t->size()) {
        auto* tail = make_piece(t->size());
        if (!tail) {
            return nullptr;
        }
        l->push(tail, xsink);
    }
    return l.release();
}

int64 QoreRegex::countMatches(const QoreString* target, ExceptionSink* xsink) const {
    TempEncodingHelper t(target, QCS_UTF8, xsink);
    if (!t) {
        return 0;
    }

    int64 count = 0;
    PCRE2_SIZE offset = 0;

    pcre2_match_data* md = pcre2_match_data_create_from_pattern(p, nullptr);
    ON_BLOCK_EXIT(pcre2_match_data_free, md);

    // See extractWithPattern() for the rationale; first call validates,
    // subsequent calls pass PCRE2_NO_UTF_CHECK to avoid O(N^2) UTF-8 rescans.
    uint32_t match_options = 0;

    while (true) {
        // a global match over a large subject can iterate many times; stay cancellable
        if (qore_check_cancel(xsink, "regular expression match count")) {
            return 0;
        }

        if (offset >= t->size()) {
            break;
        }

        int rc = qore_pcre2_match(p, reinterpret_cast<PCRE2_SPTR8>(t->c_str()), t->size(), offset,
            match_options, md);
        if (rc < 1) {
            if (qore_pcre2_check_match_error(rc, xsink, t->c_str(), t->size())) {
                assert(*xsink);
                return 0;
            }
            break;
        }

        ++count;

        PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(md);

        // advance past this match; handle zero-length matches
        if (ovector[0] == ovector[1]) {
            ++offset;
            if (offset > t->size()) {
                break;
            }
        } else {
            offset = ovector[1];
        }
        match_options = PCRE2_NO_UTF_CHECK;
    }

    return count;
}

// returns *hash<string, *string> for non-global, *list<hash<string, *string>> for global
QoreValue QoreRegex::extractNamedGroups(const QoreString* target, ExceptionSink* xsink) const {
    TempEncodingHelper t(target, QCS_UTF8, xsink);
    if (!t) {
        return QoreValue();
    }
    const char* subject = t->c_str();
    const size_t subject_len = t->size();
    const QoreEncoding* subject_encoding = t.is_temp() ? target->getEncoding() : QCS_UTF8;

    // get named capture group info from the compiled pattern
    uint32_t namecount = 0;
    pcre2_pattern_info(p, PCRE2_INFO_NAMECOUNT, &namecount);
    if (!namecount) {
        return QoreValue();  // no named groups in pattern
    }

    uint32_t name_entry_size = 0;
    PCRE2_SPTR nametable = nullptr;
    pcre2_pattern_info(p, PCRE2_INFO_NAMEENTRYSIZE, &name_entry_size);
    pcre2_pattern_info(p, PCRE2_INFO_NAMETABLE, &nametable);

    pcre2_match_data* md = pcre2_match_data_create_from_pattern(p, nullptr);
    ON_BLOCK_EXIT(pcre2_match_data_free, md);

    ReferenceHolder<QoreListNode> result_list(xsink);  // for global mode
    PCRE2_SIZE offset = 0;
    // See extractWithPattern() for the rationale.
    uint32_t match_options = 0;

    while (true) {
        // a global match over a large subject can iterate many times; stay cancellable
        if (qore_check_cancel(xsink, "regular expression named group match")) {
            return QoreValue();
        }

        if (offset >= subject_len) {
            break;
        }

        int rc = qore_pcre2_match(p, reinterpret_cast<PCRE2_SPTR8>(subject), subject_len, offset,
            match_options, md);
        if (rc < 1) {
            if (qore_pcre2_check_match_error(rc, xsink, subject, subject_len)) {
                assert(*xsink);
                return QoreValue();
            }
            break;
        }

        PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(md);

        // build hash from nametable
        ReferenceHolder<QoreHashNode> h(new QoreHashNode(stringOrNothingTypeInfo), xsink);
        PCRE2_SPTR tabptr = nametable;
        for (uint32_t i = 0; i < namecount; ++i) {
            int n = (tabptr[0] << 8) | tabptr[1];
            const char* name = reinterpret_cast<const char*>(tabptr + 2);
            if (n < rc && ovector[2 * n] != (PCRE2_SIZE)-1) {
                qore_regex_set_named_capture(**h, name, qore_regex_capture(subject + ovector[2 * n],
                    ovector[2 * n + 1] - ovector[2 * n], subject_encoding, xsink), xsink);
            } else {
                qore_regex_set_named_capture(**h, name, QoreValue(), xsink);
            }
            if (*xsink) {
                return QoreValue();
            }
            tabptr += name_entry_size;
        }

        if (!global) {
            return h.release();
        }

        if (!result_list) {
            result_list = new QoreListNode(autoHashTypeInfo);
        }
        result_list->push(h.release(), xsink);
        if (*xsink) {
            return QoreValue();
        }

        // advance past this match; handle zero-length matches
        if (ovector[0] == ovector[1]) {
            ++offset;
            if (offset > subject_len) {
                break;
            }
        } else {
            offset = ovector[1];
        }
        match_options = PCRE2_NO_UTF_CHECK;
    }

    return result_list ? result_list.release() : QoreValue();
}

// return type: *list<hash<RegexMatchInfo>>
QoreListNode* QoreRegex::extractDetailed(const QoreString* target, ExceptionSink* xsink) const {
    TempEncodingHelper t(target, QCS_UTF8, xsink);
    if (!t) {
        return nullptr;
    }
    const char* subject = t->c_str();
    const size_t subject_len = t->size();
    const QoreEncoding* subject_encoding = t.is_temp() ? target->getEncoding() : QCS_UTF8;

    // get named capture group info
    uint32_t namecount = 0;
    uint32_t name_entry_size = 0;
    PCRE2_SPTR nametable = nullptr;
    pcre2_pattern_info(p, PCRE2_INFO_NAMECOUNT, &namecount);
    if (namecount) {
        pcre2_pattern_info(p, PCRE2_INFO_NAMEENTRYSIZE, &name_entry_size);
        pcre2_pattern_info(p, PCRE2_INFO_NAMETABLE, &nametable);
    }

    pcre2_match_data* md = pcre2_match_data_create_from_pattern(p, nullptr);
    ON_BLOCK_EXIT(pcre2_match_data_free, md);

    ReferenceHolder<QoreListNode> result_list(xsink);
    PCRE2_SIZE offset = 0;
    // See extractWithPattern() for the rationale.
    uint32_t match_options = 0;

    while (true) {
        // a global match over a large subject can iterate many times; stay cancellable
        if (qore_check_cancel(xsink, "regular expression detailed match")) {
            return nullptr;
        }

        if (offset >= subject_len) {
            break;
        }

        int rc = qore_pcre2_match(p, reinterpret_cast<PCRE2_SPTR8>(subject), subject_len, offset,
            match_options, md);
        if (rc < 1) {
            if (qore_pcre2_check_match_error(rc, xsink, subject, subject_len)) {
                assert(*xsink);
                return nullptr;
            }
            break;
        }

        PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(md);

        // build RegexMatchInfo hash
        ReferenceHolder<QoreHashNode> h(new QoreHashNode(hashdeclRegexMatchInfo, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }

        // full match text
        h->setKeyValue("match", qore_regex_capture(subject + ovector[0], ovector[1] - ovector[0],
            subject_encoding, xsink), xsink);
        // offset and length
        h->setKeyValue("offset", static_cast<int64>(ovector[0]), xsink);
        h->setKeyValue("length", static_cast<int64>(ovector[1] - ovector[0]), xsink);
        if (*xsink) {
            return nullptr;
        }

        // captured groups
        if (rc > 1) {
            ReferenceHolder<QoreListNode> groups(new QoreListNode(stringOrNothingTypeInfo), xsink);
            for (int x = 1; x < rc; ++x) {
                int pos = x * 2;
                if (ovector[pos] == (PCRE2_SIZE)-1) {
                    groups->push(QoreValue(), xsink);
                } else {
                    groups->push(qore_regex_capture(subject + ovector[pos],
                        ovector[pos + 1] - ovector[pos], subject_encoding, xsink), xsink);
                }
            }
            h->setKeyValue("groups", groups.release(), xsink);
        }

        // named groups
        if (namecount) {
            ReferenceHolder<QoreHashNode> named(new QoreHashNode(stringOrNothingTypeInfo), xsink);
            PCRE2_SPTR tabptr = nametable;
            for (uint32_t i = 0; i < namecount; ++i) {
                int n = (tabptr[0] << 8) | tabptr[1];
                const char* name = reinterpret_cast<const char*>(tabptr + 2);
                if (n < rc && ovector[2 * n] != (PCRE2_SIZE)-1) {
                    qore_regex_set_named_capture(**named, name, qore_regex_capture(subject + ovector[2 * n],
                        ovector[2 * n + 1] - ovector[2 * n], subject_encoding, xsink), xsink);
                } else {
                    qore_regex_set_named_capture(**named, name, QoreValue(), xsink);
                }
                if (*xsink) {
                    return nullptr;
                }
                tabptr += name_entry_size;
            }
            h->setKeyValue("named_groups", named.release(), xsink);
        }

        if (*xsink) {
            return nullptr;
        }

        if (!result_list) {
            result_list = new QoreListNode(hashdeclRegexMatchInfo->getTypeInfo());
        }
        result_list->push(h.release(), xsink);
        if (*xsink) {
            return nullptr;
        }

        // advance past this match; handle zero-length matches
        if (ovector[0] == ovector[1]) {
            ++offset;
            if (offset > subject_len) {
                break;
            }
        } else {
            offset = ovector[1];
        }

        if (!global) {
            break;
        }
        match_options = PCRE2_NO_UTF_CHECK;
    }

    return result_list.release();
}

QoreString* QoreRegex::getString() {
    QoreString* rs = str;
    str = nullptr;
    return rs;
}
