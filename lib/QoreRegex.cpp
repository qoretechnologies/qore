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

#include <memory>

QoreRegex::QoreRegex() : QoreRegexBase(new QoreString) {
}

QoreRegex::QoreRegex(const QoreString& s, int64 opts, ExceptionSink* xsink) : QoreRegexBase(PCRE2_UTF | (int)opts),
        global(opts & QRE_GLOBAL ? true : false) {
    if (check_re_options(options)) {
        xsink->raiseException("REGEX-OPTION-ERROR", QLLD " contains invalid option bits", opts);
        options = 0;
    }

    parseRT(&s, xsink);
}

QoreRegex::QoreRegex(const char* s, int64 opts, ExceptionSink* xsink) : QoreRegexBase(PCRE2_UTF | (int)opts),
        global(opts & QRE_GLOBAL ? true : false) {
    if (check_re_options(options)) {
        xsink->raiseException("REGEX-OPTION-ERROR", QLLD " contains invalid option bits", opts);
        options = 0;
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

    p = pcre2_compile(reinterpret_cast<PCRE2_SPTR8>(pattern), PCRE2_ZERO_TERMINATED, options, &errorcode, &eo,
        nullptr);
    if (!p) {
        PCRE2_UCHAR buffer[qore_pcre2_errorbuf_size];
        pcre2_get_error_message(errorcode, buffer, sizeof(buffer));
        //printd(5, "QoreRegex::parse() error parsing '%s': %s", pattern, (char* )err);
        xsink->raiseException("REGEX-COMPILATION-ERROR", "Regular expression compilation failed at %lu ('%s'): %s",
            eo, pattern, buffer);
    }
}

void QoreRegex::parse(q_get_loc_t get_loc) {
    ExceptionSink xsink;
    parseRT(str, &xsink);
    delete str;
    str = nullptr;
    if (xsink.isEvent()) {
        // override the exception location with the real parse location in case of an error
        xsink.overrideLocation(*get_loc());
        qore_program_private::addParseException(getProgram(), xsink);
    }
}

bool QoreRegex::exec(const QoreString* target, ExceptionSink* xsink) const {
    TempEncodingHelper t(target, QCS_UTF8, xsink);
    if (!t)
        return false;

    return exec(t->c_str(), t->strlen());
}

bool QoreRegex::exec(const char* str, size_t len) const {
    // the PCRE docs say that if we don't send an ovector here the library may have to malloc
    // memory, so, even though we don't need the results, we include the vector to avoid
    // extraneous malloc()s

    pcre2_match_data* md = pcre2_match_data_create_from_pattern(p, nullptr);
    ON_BLOCK_EXIT(pcre2_match_data_free, md);

    int rc = pcre2_match(p, reinterpret_cast<PCRE2_SPTR8>(str), len, 0, 0, md, nullptr);
    // rc == 0 means the ovector was not large enough, which should not happen when using
    // pcre2_match_data_create_from_pattern()
    assert(rc);
    //printd(5, "QoreRegex::exec(%s) this: %p pre_exec() rc=%d\n", str, this, rc);
    return rc >= 0;
}

// return type: *list<*string>
QoreListNode* QoreRegex::extractSubstrings(const QoreString* target, ExceptionSink* xsink) const {
    TempEncodingHelper t(target, QCS_UTF8, xsink);
    if (!t) {
        return nullptr;
    }

    ReferenceHolder<QoreListNode> l(xsink);

    PCRE2_SIZE offset = 0;

    pcre2_match_data* md = pcre2_match_data_create_from_pattern(p, nullptr);
    ON_BLOCK_EXIT(pcre2_match_data_free, md);

    while (true) {
        if (offset >= t->size()) {
            break;
        }

        int rc = pcre2_match(p, reinterpret_cast<PCRE2_SPTR8>(t->c_str()), t->size(), offset, 0, md, nullptr);
        //printd(5, "QoreRegex::extractSubstrings('%s') =~ /xxx/ = %d (global: %d)\n", t->c_str() + offset, rc, global);
        // rc == 0 means the ovector was not large enough, which should not happen when using
        // pcre2_match_data_create_from_pattern()
        assert(rc);

        if (rc < 1) {
#ifdef DEBUG
            if (rc != PCRE2_ERROR_NOMATCH && rc != PCRE2_ERROR_UTF8_ERR21) {
                printd(0, "QoreRegex::extractSubstrings() Unknown error returned from pcre2_match(//, '%s') -> %d\n",
                    t->c_str(), rc);
                assert(false);
            }
#endif
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
                QoreStringNode* tstr = new QoreStringNode;
                tstr->concat(t->c_str() + ovector[pos], ovector[pos + 1] - ovector[pos]);
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
    int split_count = 0;

    pcre2_match_data* md = pcre2_match_data_create_from_pattern(p, nullptr);
    ON_BLOCK_EXIT(pcre2_match_data_free, md);

    while (true) {
        if (offset >= t->size()) {
            break;
        }

        // if limit is reached, add the remainder and stop
        if (limit > 0 && split_count >= limit - 1) {
            QoreStringNode* tstr = new QoreStringNode(t->c_str() + offset, t->getEncoding());
            l->push(tstr, xsink);
            break;
        }

        int rc = pcre2_match(p, reinterpret_cast<PCRE2_SPTR8>(t->c_str()), t->size(), offset, 0, md, nullptr);
        printd(5, "QoreRegex::extractWithPattern('%s') = %d\n", t->c_str() + offset, rc);

        assert(rc);

        if (rc < 1) {
#ifdef DEBUG
            if (rc != PCRE2_ERROR_NOMATCH && rc != PCRE2_ERROR_UTF8_ERR21) {
                printd(0, "QoreRegex::extractWithPattern() Unknown error returned from pcre2_match(//, '%s') -> %d\n",
                    t->c_str(), rc);
                assert(false);
            }
#endif
            // add rest of string to list
            QoreStringNode* tstr = new QoreStringNode(t->c_str() + offset, t->getEncoding());
            l->push(tstr, xsink);
            break;
        }

        assert(rc == 1);
        PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(md);
        int pos = (rc - 1) * 2;
        int new_offset = ovector[pos + (include_pattern ? 1 : 0)];
        printd(5, "QoreRegex::extractWithPattern() offset: %d new_offset: %d size: %d ovector[%d..]: %d %d\n", offset,
            new_offset, t->strlen(), pos, ovector[pos], ovector[pos + 1]);
        SimpleRefHolder<QoreStringNode> tstr(new QoreStringNode(t->c_str() + offset, new_offset - offset, t->getEncoding()));
        printd(5, "substring %d: %d - %d (len %d) tstr: '%s' (%d)\n", rc, ovector[pos], ovector[pos + 1],
            ovector[pos + 1] - ovector[pos], tstr->c_str(), (int)tstr->size());
        l->push(tstr.release(), xsink);
        offset = ovector[pos + 1];
        ++split_count;
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

    while (true) {
        if (offset >= t->size()) {
            break;
        }

        int rc = pcre2_match(p, reinterpret_cast<PCRE2_SPTR8>(t->c_str()), t->size(), offset, 0, md, nullptr);
        if (rc < 1) {
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
    }

    return count;
}

// returns *hash<string, *string> for non-global, *list<hash<string, *string>> for global
QoreValue QoreRegex::extractNamedGroups(const QoreString* target, ExceptionSink* xsink) const {
    TempEncodingHelper t(target, QCS_UTF8, xsink);
    if (!t) {
        return QoreValue();
    }

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

    while (true) {
        if (offset >= t->size()) {
            break;
        }

        int rc = pcre2_match(p, reinterpret_cast<PCRE2_SPTR8>(t->c_str()), t->size(), offset, 0, md, nullptr);
        if (rc < 1) {
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
                h->setKeyValue(name, new QoreStringNode(t->c_str() + ovector[2 * n],
                    ovector[2 * n + 1] - ovector[2 * n]), xsink);
            } else {
                h->setKeyValue(name, QoreValue(), xsink);
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
            if (offset > t->size()) {
                break;
            }
        } else {
            offset = ovector[1];
        }
    }

    return result_list ? result_list.release() : QoreValue();
}

// return type: *list<hash<RegexMatchInfo>>
QoreListNode* QoreRegex::extractDetailed(const QoreString* target, ExceptionSink* xsink) const {
    TempEncodingHelper t(target, QCS_UTF8, xsink);
    if (!t) {
        return nullptr;
    }

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

    while (true) {
        if (offset >= t->size()) {
            break;
        }

        int rc = pcre2_match(p, reinterpret_cast<PCRE2_SPTR8>(t->c_str()), t->size(), offset, 0, md, nullptr);
        if (rc < 1) {
            break;
        }

        PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(md);

        // build RegexMatchInfo hash
        ReferenceHolder<QoreHashNode> h(new QoreHashNode(hashdeclRegexMatchInfo, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }

        // full match text
        h->setKeyValue("match", new QoreStringNode(t->c_str() + ovector[0], ovector[1] - ovector[0]), xsink);
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
                    groups->push(new QoreStringNode(t->c_str() + ovector[pos],
                        ovector[pos + 1] - ovector[pos]), xsink);
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
                    named->setKeyValue(name, new QoreStringNode(t->c_str() + ovector[2 * n],
                        ovector[2 * n + 1] - ovector[2 * n]), xsink);
                } else {
                    named->setKeyValue(name, QoreValue(), xsink);
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
            if (offset > t->size()) {
                break;
            }
        } else {
            offset = ovector[1];
        }

        if (!global) {
            break;
        }
    }

    return result_list.release();
}

QoreString* QoreRegex::getString() {
    QoreString* rs = str;
    str = nullptr;
    return rs;
}
