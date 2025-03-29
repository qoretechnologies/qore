/*
    QoreRegexSubst.cpp

    regular expression substitution node definition

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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
#include "qore/intern/QoreRegexSubst.h"
#include "qore/intern/qore_program_private.h"

#include <cctype>
#include <cstdlib>
#include <strings.h>

// constructor used when parsing
QoreRegexSubst::QoreRegexSubst() : QoreRegexBase(new QoreString), newstr(new QoreString) {
    //printd(5, "QoreRegexSubst::QoreRegexSubst() this=%p\n", this);
}

// constructor when used at run-time
QoreRegexSubst::QoreRegexSubst(const QoreString* pstr, int opts, ExceptionSink *xsink)
        : QoreRegexBase(PCRE2_UTF | (int)opts) {
    if (check_re_options(opts)) {
        xsink->raiseException("REGEX-OPTION-ERROR", "%d contains invalid option bits", opts);
    } else {
        options |= opts;
    }

    parseRT(pstr, xsink);
}

QoreRegexSubst::QoreRegexSubst(const char* pstr, int opts, ExceptionSink *xsink)
        : QoreRegexBase(PCRE2_UTF | (int)opts) {
    if (check_re_options(opts)) {
        xsink->raiseException("REGEX-OPTION-ERROR", "%d contains invalid option bits", opts);
    } else {
        options |= opts;
    }

    parseRT(pstr, xsink);
}

QoreRegexSubst::~QoreRegexSubst() {
    //printd(5, "QoreRegexSubst::~QoreRegexSubst() this=%p\n", this);
    delete newstr;
}

void QoreRegexSubst::concatSource(char c) {
    str->concat(c);
}

void QoreRegexSubst::concatTarget(char c) {
    newstr->concat(c);
}

// returns 0 for OK, -1 if parse error raised
int QoreRegexSubst::parseRT(const QoreString* pstr, ExceptionSink* xsink) {
    // convert to UTF-8 if necessary
    TempEncodingHelper t(pstr, QCS_UTF8, xsink);
    if (*xsink) {
        return -1;
    }
    return parseRT(t->c_str(), xsink);
}

// returns 0 for OK, -1 if parse error raised
int QoreRegexSubst::parseRT(const char* pstr, ExceptionSink* xsink) {
    int errorcode;
    PCRE2_SIZE eo;

    //printd(5, "QoreRegexSubst::parseRT(%s) this: %p\n", t->c_str(), this);

    p = pcre2_compile(reinterpret_cast<PCRE2_SPTR8>(pstr), PCRE2_ZERO_TERMINATED, options, &errorcode, &eo, nullptr);
    if (!p) {
        PCRE2_UCHAR buffer[qore_pcre2_errorbuf_size];
        pcre2_get_error_message(errorcode, buffer, sizeof(buffer));
        //printd(5, "QoreRegex::parse() error parsing '%s': %s", pattern, (char* )err);
        xsink->raiseException("REGEX-COMPILATION-ERROR", "Regular expression compilation failed at %lu ('%s'): %s",
            eo, pstr, buffer);
        return -1;
    }
    return 0;
}

int QoreRegexSubst::parse() {
    //printd(5, "QoreRegexSubst() this=%p: str='%s', divider=%d\n", this, str->c_str(), divider);
    ExceptionSink xsink;
    parseRT(str, &xsink);
    if (xsink.isEvent()) {
        qore_program_private::addParseException(getProgram(), xsink);
        return -1;
    }

    //printd(5, "QoreRegexSubst::parse() this=%p: pstr=%s, newstr=%s, global=%s\n", this, pstr->c_str(), newstr->c_str(), global ? "true" : "false");

    delete str;
    str = nullptr;
    return 0;
}

// static function
int QoreRegexSubst::concat(ExceptionSink& xsink, QoreString* cstr, PCRE2_SIZE* ovector, int olen, const char* ptr,
        const char* target, int rc) {
    while (*ptr) {
        if (*ptr == '\\') {
            ++ptr;
            if (isoctaldigit(*ptr) && isoctaldigit(*(ptr + 1)) && isoctaldigit(*(ptr + 2))) {
                int val = (*ptr - 48) * 64 + (*(ptr + 1) - 48) * 8 + (*(ptr + 2) - 48);
                if (val > 255) {
                    xsink.raiseException("REGEX-OCTAL-ERROR", "octal constant \\%c%c%c is too large "
                        "(decimal %d; must be < 256)", *ptr, *(ptr + 1), *(ptr + 2), val);
                    return -1;
                }
                cstr->concat((const char)val);
                ptr += 3;
            } else if (*(ptr) == '\\' || *(ptr) == '$') {
                cstr->concat(*(ptr++));
            } else if (*ptr == 'a') {
                // \a = BEL (bell)
                cstr->concat((const char)7);
                ++ptr;
            } else if (*ptr == 'b') {
                // \b = BS (backspace)
                cstr->concat((const char)8);
                ++ptr;
            } else if (*ptr == 'e') {
                // \e = ESC (escape)
                cstr->concat((const char)27);
                ++ptr;
            } else if (*ptr == 'f') {
                // \f = FF (form feed)
                cstr->concat((const char)12);
                ++ptr;
            } else if (*ptr == 'n') {
                // \n = NL (newline)
                cstr->concat((const char)10);
                ++ptr;
            } else if (*ptr == 'r') {
                // \r = CR (carriage return)
                cstr->concat((const char)13);
                ++ptr;
            } else if (*ptr == 't') {
                // \t = HT (horizontal tab)
                cstr->concat((const char)9);
                ++ptr;
            } else if (*ptr == 'v') {
                // \v = VT (vertical tab)
                cstr->concat((const char)11);
                ++ptr;
            } else {
                cstr->concat('\\');
            }
        } else if (*ptr == '$' && isdigit(ptr[1])) {
            QoreString n;
            ++ptr;
            do {
                n.concat(*(ptr++));
            } while (isdigit(*ptr));
            int num = atoi(n.c_str());
            int pos = num * 2;
            //printd(5, "QoreRegexSubst::concat() pos: %d olen: %d ovector[%d]: %d ovector[%d]: %d rc: %d\n", pos,
            //  olen, pos, ovector[pos], pos + 1, ovector[pos + 1], rc);
            if (pos > 0 && pos < olen && num < rc && ovector[pos] != -1) {
                cstr->concat(target + ovector[pos], ovector[pos + 1] - ovector[pos]);
            }
        } else {
            cstr->concat(*(ptr++));
        }
    }
    //printd(5, "QoreRegexSubst::concat() target: '%s' cstr: '%s'\n", target, cstr->c_str());
    return 0;
}

#define SUBST_OVECSIZE 30
#define SUBST_LASTELEM 20
// called directly for run-time evaluation
QoreStringNode* QoreRegexSubst::exec(const QoreString* target, const QoreString* nstr, ExceptionSink* xsink) const {
    TempEncodingHelper t(target, QCS_UTF8, xsink);
    if (*xsink) {
        return nullptr;
    }

    SimpleRefHolder<QoreStringNode> tstr(new QoreStringNode);

    const char* ptr = t->c_str();
    // detect infinite recursion (empty pattern matches)
    int last_match = -1;

    pcre2_match_data* md = pcre2_match_data_create_from_pattern(p, nullptr);
    ON_BLOCK_EXIT(pcre2_match_data_free, md);

    //printd(5, "QoreRegexSubst::exec(%s) this=%p: global=%s\n", ptr, this, global ? "true" : "false");
    while (true) {
        PCRE2_SIZE offset = ptr - t->c_str();
        if ((unsigned)offset >= t->size()) {
            break;
        }
        int rc = pcre2_match(p, reinterpret_cast<PCRE2_SPTR8>(t->c_str()), t->size(), offset, 0, md, nullptr);
        //int rc = pcre_exec(p, 0, t->c_str(), t->strlen(), offset, 0, ovector, SUBST_OVECSIZE);

        //printd(5, "QoreRegexSubst::exec() prec_exec() rc: %d ovector[0]: %d\n", rc, ovector[0]);
        // FIXME: rc = 0 means that not enough space was available in ovector!
        if (rc < 1) {
#ifdef DEBUG
            if (rc != PCRE2_ERROR_NOMATCH) {
                printd(0, "QoreRegexSubst::exec() Unknown error returned from pcre2_match(//, '%s') -> %d\n",
                    t->c_str(), rc);
                assert(false);
            }
#endif
            break;
        }

        PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(md);

        // detect infinite recursion
        if (ovector[0] == last_match) {
            xsink->raiseException("REGEX-SUBST-ERROR", "Infinite recursion detected in regex substitution string; "
                "this normally happens with an empty pattern with use with the global option (RE_GLOBAL)");
            return nullptr;
        } else {
            last_match = ovector[0];
        }

        if (ovector[0] > offset) {
            tstr->concat(ptr, ovector[0] - offset);
        }

        if (concat(*xsink, *tstr, ovector, SUBST_LASTELEM, nstr ? nstr->c_str() : "", t->c_str(), rc)) {
            assert(*xsink);
            return nullptr;
        }

        //printd(5, "QoreRegexSubst::exec() '%s' =~ s/?/%s/%s offset=%d, 0=%d, 1=%d ('%s')\n", t->c_str(), nstr->c_str(), global ? "g" : "", offset, ovector[0], ovector[1], tstr->c_str());

        ptr = t->c_str() + ovector[1];

        if (!global) {
            break;
        }
    }

    //printd(5, "QoreRegexSubst::exec() *ptr=%d ('%s') tstr='%s'\n", *ptr, ptr, tstr->c_str());
    if (*ptr) {
        tstr->concat(ptr);
    }

    //printd(5, "QoreRegexSubst::exec() this=%p: returning '%s'\n", this, tstr->c_str());
    return tstr.release();
}

// called for run-time evaluation of parse-time-created objects
QoreStringNode* QoreRegexSubst::exec(const QoreString* target, ExceptionSink* xsink) const {
    return exec(target, newstr, xsink);
}

void QoreRegexSubst::setGlobal() {
    global = true;
}

QoreString* QoreRegexSubst::getPattern() const {
    return str;
}
