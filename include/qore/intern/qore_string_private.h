/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_string_private.h

    QoreString private implementation

    Qore Programming Language

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

#ifndef QORE_QORE_STRING_PRIVATE_H
#define QORE_QORE_STRING_PRIVATE_H

#include <vector>

class QoreRegexBase;
class QoreRegexSubst;

#define MAX_INT_STRING_LEN     48
#define MAX_BIGINT_STRING_LEN  48
#define MAX_FLOAT_STRING_LEN   48
#define STR_CLASS_BLOCK        (0x10 * 4)
#define STR_CLASS_EXTRA        (0x10 * 3)

#define MIN_SPRINTF_BUFSIZE   64

#define QUS_PATH     0
#define QUS_QUERY    1
#define QUS_FRAGMENT 2

typedef std::vector<int> intvec_t;

struct qore_string_private {
public:
    size_t len = 0;
    size_t allocated = 0;
    char* buf = nullptr;
    const QoreEncoding* encoding = nullptr;

    DLLLOCAL qore_string_private() {
    }

    DLLLOCAL qore_string_private(const qore_string_private &p) {
        allocated = p.len + STR_CLASS_EXTRA;
        allocated = (allocated / 0x10 + 1) * 0x10; // use complete cache line
        buf = (char*)malloc(sizeof(char) * allocated);
        len = p.len;
        if (len)
            memcpy(buf, p.buf, len);
        buf[len] = '\0';
        encoding = p.getEncoding();
    }

    DLLLOCAL ~qore_string_private() {
        if (buf) {
            free(buf);
        }
    }

    DLLLOCAL void check_char(size_t i) {
        if (i >= allocated) {
            size_t d = i >> 2;
            allocated = i + (d < STR_CLASS_BLOCK ? STR_CLASS_BLOCK : d);
            allocated = (allocated / 0x10 + 1) * 0x10; // use complete cache line
            buf = (char*)realloc(buf, allocated * sizeof(char));
        }
    }

    DLLLOCAL size_t check_offset(qore_offset_t offset) {
        if (offset < 0) {
            offset = len + offset;
            return offset < 0 ? 0 : offset;
        }

        return ((size_t)offset > len) ? len : offset;
    }

    DLLLOCAL void check_offset(qore_offset_t offset, qore_offset_t num, size_t &n_offset, size_t &n_num) {
        n_offset = check_offset(offset);

        if (num < 0) {
            num = len + num - n_offset;
            if (num < 0)
                n_num = 0;
            else
                n_num = num;
            return;
        }
        n_num = num;
    }

    // NOTE: this is purely byte oriented - no character semantics here
    DLLLOCAL qore_offset_t find(char c, qore_offset_t pos = 0) {
        if (pos < 0) {
            pos = len + pos;
            if (pos < 0)
                pos = 0;
        } else if (pos > 0 && pos > (qore_offset_t)len)
            return -1;
        const char* p;
        if (!(p = strchr(buf + pos, c)))
            return -1;
        return (qore_offset_t)(p - buf);
    }

    // NOTE: this is purely byte oriented - no character semantics here
    DLLLOCAL qore_offset_t rfind(char c, qore_offset_t pos = -1) {
        if (pos < 0) {
            pos = len + pos;
            if (pos < 0) {
                return -1;
            }
        } else if (pos > 0 && pos > (qore_offset_t)len) {
            pos = len - 1;
        }

        const char* p = buf + pos;
        while (p >= buf) {
            if (*p == c) {
                return (qore_offset_t)(p - buf);
            }
            --p;
        }
        return -1;
    }

    // NOTE: this is purely byte oriented - no character semantics here
    DLLLOCAL qore_offset_t findAny(const char* str, qore_offset_t pos = 0) {
        if (pos < 0) {
            pos = len + pos;
            if (pos < 0) {
                pos = 0;
            }
        } else if (pos > 0 && pos > (qore_offset_t)len) {
            return -1;
        }
        const char* p;
        if (!(p = strstr(buf + pos, str))) {
            return -1;
        }
        return (qore_offset_t)(p - buf);
    }

    // NOTE: this is purely byte oriented - no character semantics here
    DLLLOCAL qore_offset_t rfindAny(const char* str, qore_offset_t pos = -1) {
        if (pos < 0) {
            pos = len + pos;
            if (pos < 0) {
                return -1;
            }
        } else if (pos > 0 && pos > (qore_offset_t)len) {
            pos = len - 1;
        }

        const char* p = buf + pos;
        while (p >= buf) {
            for (const char* t = str; *t; ++t) {
                if (*p == *t) {
                    return (qore_offset_t)(p - buf);
                }
            }
            --p;
        }
        return -1;
    }

    DLLLOCAL static qore_offset_t index_simple(const char* haystack, size_t hlen, const char* needle, size_t nlen,
        qore_offset_t pos = 0) {
        const char* start = haystack + pos;
        void* ptr = q_memmem(start, hlen - pos, needle, nlen);
        if (!ptr) {
            return -1;
        }
        return reinterpret_cast<const char*>(ptr) - start + pos;
    }

    DLLLOCAL qore_offset_t index(const QoreString &orig_needle, qore_offset_t pos, ExceptionSink *xsink) const {
        assert(xsink);
        TempEncodingHelper needle(orig_needle, getEncoding(), xsink);
        if (!needle)
            return -1;

        // do simple index
        if (!getEncoding()->isMultiByte()) {
            if (pos < 0) {
                pos = len + pos;
                if (pos < 0) {
                    pos = 0;
                }
            } else if (pos >= (qore_offset_t)len) {
                return -1;
            }

            return index_simple(buf, len, needle->c_str(), needle->size(), pos);
        }

        // do multibyte index()
        if (findByteOffset(pos, xsink))
            return -1;
        if (pos < 0)
            pos = 0;
        else if (pos >= (qore_offset_t)len)
            return -1;

        qore_offset_t ind = index_simple(buf + pos, len - pos, needle->c_str(), needle->size());
        if (ind != -1) {
            ind = getEncoding()->getCharPos(buf, buf + pos + ind, xsink);
            if (*xsink)
                return -1;
        }

        return ind;
    }

    DLLLOCAL qore_offset_t bindex(const QoreString& needle, qore_offset_t pos) const {
        if (needle.strlen() + pos > len)
            return -1;

        return bindex(needle.c_str(), pos, needle.size());
    }

    DLLLOCAL qore_offset_t bindex(const std::string& needle, qore_offset_t pos) const {
        if (needle.size() + pos > len)
            return -1;

        return bindex(needle.c_str(), pos, needle.size());
    }

    DLLLOCAL qore_offset_t bindex(const char* needle, qore_offset_t pos, size_t nsize = 0) const {
        if (pos < 0) {
            pos = len + pos;
            if (pos < 0) {
                pos = 0;
            }
        } else if (pos >= (qore_offset_t)len) {
            return -1;
        }

        if (!nsize) {
            nsize = strlen(needle);
        }
        return index_simple(buf, len, needle, nsize, pos);
    }

    // finds the last occurrence of needle in haystack at or before position pos
    // pos must be a non-negative valid byte offset in haystack
    DLLLOCAL static qore_offset_t rindex_simple(const char* haystack, size_t hlen, const char* needle,
            size_t nlen, qore_offset_t pos = -1) {
        if (pos < 0) {
            pos = hlen + pos;
            if (pos < 0) {
                return -1;
            }
        } else if (pos >= (qore_offset_t)hlen) {
            pos = hlen - 1;
        }

        assert(pos < (qore_offset_t)hlen);
        void* ptr = q_memrmem(haystack, pos + 1, needle, nlen);
        if (!ptr) {
            return -1;
        }
        return static_cast<qore_offset_t>(reinterpret_cast<const char*>(ptr) -
            reinterpret_cast<const char*>(haystack));
    }

    // start is a byte offset that has to point to the start of a valid character
    DLLLOCAL int findByteOffset(qore_offset_t& pos, ExceptionSink* xsink, size_t start = 0) const {
        assert(xsink);
        assert(getEncoding()->isMultiByte());
        if (!pos)
            return 0;
        // get positive character offset if negative
        if (pos < 0) {
            // get the length of the string in characters
            size_t clen = getEncoding()->getLength(buf + start, buf + len, xsink);
            if (*xsink)
                return -1;
            pos = clen + pos;
        }
        // now get the byte position from this character offset
        pos = getEncoding()->getByteLen(buf + start, buf + len, pos, xsink);
        return *xsink ? -1 : 0;
    }

    DLLLOCAL qore_offset_t rindex(const QoreString &orig_needle, qore_offset_t pos, ExceptionSink *xsink) const {
        assert(xsink);
        TempEncodingHelper needle(orig_needle, getEncoding(), xsink);
        if (!needle)
            return -1;

        if (!getEncoding()->isMultiByte()) {
            if (pos < 0) {
                pos = len + pos;
                if (pos < 0)
                return -1;
            }

            return rindex_simple(buf, len, needle->c_str(), needle->size(), pos);
        }

        // do multi-byte rindex
        if (findByteOffset(pos, xsink))
            return -1;
        if (pos < 0)
            return -1;

        // get byte rindex position
        qore_offset_t ind = rindex_simple(buf, len, needle->c_str(), needle->size(), pos);

        // calculate character position from byte position
        if (ind && ind != -1) {
            ind = getEncoding()->getCharPos(buf, buf + ind, xsink);
            if (*xsink)
                return 0;
        }

        return ind;
    }

    DLLLOCAL qore_offset_t brindex(const QoreString& needle, qore_offset_t pos) const {
        return brindex(needle.getBuffer(), needle.strlen(), pos);
    }

    DLLLOCAL qore_offset_t brindex(const std::string& needle, qore_offset_t pos) const {
        return brindex(needle.c_str(), needle.size(), pos);
    }

    DLLLOCAL qore_offset_t brindex(const char* needle, size_t needle_len, qore_offset_t pos) const {
        if (pos < 0) {
            pos = len + pos;
        }

        if (pos >= (qore_offset_t)len) {
            pos = len - 1;
        }

        if (pos < 0) {
            if (pos == -1 && !len && !needle_len) {
                return 0;
            }
            return -1;
        }

        if (needle_len + (len - pos) > len) {
            return -1;
        }

        return rindex_simple(buf, len, needle, needle_len, pos);
    }

    DLLLOCAL bool startsWith(const char* str, size_t ssize) const {
        return !strncmp(str, buf, ssize);
    }

    DLLLOCAL bool endsWith(const char* str, size_t ssize) const {
        if (ssize > len) {
            return false;
        }
        return !strncmp(str, buf + len - ssize, ssize);
    }

    DLLLOCAL bool isDataPrintableAscii() const {
        for (size_t i = 0; i < len; ++i) {
            if (buf[i] < 32 || buf[i] > 126)
                return false;
        }
        return true;
    }

    DLLLOCAL bool isDataAscii() const {
        for (size_t i = 0; i < len; ++i) {
            if ((unsigned char)(buf[i]) > 127)
                return false;
        }
        return true;
    }

    DLLLOCAL void concat_intern(const char* p, size_t plen) {
        assert(p);
        assert(plen);
        check_char(len + plen);
        memcpy(buf + len, p, plen);
        len += plen;
        buf[len] = '\0';
    }

    DLLLOCAL void concat_simple(const qore_string_private& str, qore_offset_t pos) {
        if (pos < 0) {
            pos = str.len + pos;
            if (pos < 0)
                pos = 0;
        }
        else if (pos >= (qore_offset_t)str.len)
            return;

        concat_intern(str.buf + pos, str.len - pos);
    }

    DLLLOCAL int concat(const qore_string_private& str, qore_offset_t pos, ExceptionSink* xsink) {
        assert(str.getEncoding() == getEncoding());

        if (!getEncoding()->isMultiByte()) {
            concat_simple(str, pos);
            return 0;
        }

        // find byte positions from character positions
        if (pos) {
            if (str.findByteOffset(pos, xsink))
                return -1;
            if (pos < 0)
                pos = 0;
            else if (pos > (qore_offset_t)str.len)
                return 0;
        }

        concat_intern(str.buf + pos, str.len - pos);
        return 0;
    }

    DLLLOCAL void concat_simple(const qore_string_private& str, qore_offset_t pos, qore_offset_t plen) {
        if (pos < 0) {
            pos = str.len + pos;
            if (pos < 0)
                pos = 0;
        } else if (pos >= (qore_offset_t)str.len)
            return;

        if (plen < 0) {
            plen = str.len + plen;
            if (plen <= 0)
                return;
        } else if (plen > (qore_offset_t)str.len)
            plen = str.len;

        concat_intern(str.buf + pos, plen);
    }

    DLLLOCAL int concat(const qore_string_private& str, qore_offset_t pos, qore_offset_t plen, ExceptionSink* xsink) {
        assert(str.getEncoding() == getEncoding());
        assert(plen);

        if (!getEncoding()->isMultiByte()) {
            concat_simple(str, pos);
            return 0;
        }

        // find byte positions from character positions
        if (pos) {
            if (str.findByteOffset(pos, xsink))
                return -1;
            if (pos < 0)
                pos = 0;
            else if (pos > (qore_offset_t)str.len)
                return 0;
        }

        // find the byte position from the starting byte
        if (str.findByteOffset(plen, xsink, pos))
            return -1;
        if (plen <= 0)
            return 0;
        if (plen > (qore_offset_t)str.len)
            plen = str.len;

        concat_intern(str.buf + pos, plen);
        return 0;
    }

    DLLLOCAL qore_offset_t getByteOffset(size_t i, ExceptionSink* xsink) const {
        assert(xsink);
        size_t rc;
        if (i) {
            rc = getEncoding()->getByteLen(buf, buf + len, i, xsink);
            if (*xsink)
                return -1;
        } else
            rc = 0;
        return rc > len ? -1 : (qore_offset_t)rc;
    }

    DLLLOCAL size_t removeBytes(size_t num) {
        if (num >= len) {
            clear();
            return len;
        }
        memmove(buf, buf + num, len - num);
        len -= num;
        return num;
    }

    DLLLOCAL void clear() {
        if (allocated) {
            len = 0;
            buf[0] = '\0';
        }
    }

    DLLLOCAL void concat(char c) {
        if (allocated) {
            buf[len] = c;
            check_char(++len);
            buf[len] = '\0';
            return;
        }
        // allocate new string buffer
        allocated = STR_CLASS_BLOCK;
        len = 1;
        buf = (char*)malloc(sizeof(char) * allocated);
        buf[0] = c;
        buf[1] = '\0';
    }

    DLLLOCAL void concat(const qore_string_private* str) {
        assert(!str || (str->encoding == encoding) || !str->encoding);

        // if it's not a null string
        if (str && str->len) {
            // if priv->buffer needs to be resized
            check_char(str->len + len + STR_CLASS_EXTRA);
            // concatenate new string
            memcpy(buf + len, str->buf, str->len);
            len += str->len;
            buf[len] = '\0';
        }
    }

    // concatenate QoreString without encoding conversion
    DLLLOCAL void concat(const QoreString* str) {
        if (str) {
            concat(str->priv);
        }
    }

    DLLLOCAL void concat(const char *str) {
        // if it's not a null string
        if (str) {
            size_t i = 0;
            // iterate through new string
            while (str[i]) {
                // if priv->buffer needs to be resized
                check_char(len);
                // concatenate one character at a time
                buf[len++] = str[i++];
            }
            // see if priv->buffer needs to be resized for '\0'
            check_char(len);
            // terminate string
            buf[len] = '\0';
        }
    }

    DLLLOCAL int concat(const QoreString* str, ExceptionSink* xsink);

    // return 0 for success
    DLLLOCAL int vsprintf(const char *fmt, va_list args) {
        size_t fmtlen = ::strlen(fmt);
        // ensure minimum space is free
        if ((allocated - len - fmtlen) < MIN_SPRINTF_BUFSIZE) {
            allocated += fmtlen + MIN_SPRINTF_BUFSIZE;
            allocated = (allocated / 0x10 + 1) * 0x10; // use complete cache line
            // resize buffer
            buf = (char*)realloc(buf, allocated * sizeof(char));
        }
        // set free buffer size
        qore_offset_t free = allocated - len;

        // copy formatted string to buffer
        int i = ::vsnprintf(buf + len, free, fmt, args);

#ifdef HPUX
        // vsnprintf failed but didn't tell us how big the buffer should be
        if (i < 0) {
            //printf("DEBUG: vsnprintf() failed: i=%d allocated=" QSD " len=" QSD " buf=%p fmtlen=" QSD
            //    " (new=i+%d = %d)\n", i, allocated, len, buf, fmtlen, STR_CLASS_EXTRA, i + STR_CLASS_EXTRA);
            // resize buffer
            allocated += STR_CLASS_EXTRA;
            allocated = (allocated / 0x10 + 1) * 0x10; // use complete cache line
            buf = (char*)realloc(buf, sizeof(char) * allocated);
            *(buf + len) = '\0';
            return -1;
        }
#else
        if (i >= free) {
            //printf("DEBUG: vsnprintf() failed: i=%d allocated=" QSD " len=" QSD " buf=%p fmtlen=" QSD
            //    " (new=i+%d = %d)\n", i, allocated, len, buf, fmtlen, STR_CLASS_EXTRA, i + STR_CLASS_EXTRA);
            // resize buffer
            allocated = len + i + STR_CLASS_EXTRA;
            allocated = (allocated / 0x10 + 1) * 0x10; // use complete cache line
            buf = (char*)realloc(buf, sizeof(char) * allocated);
            *(buf + len) = '\0';
            return -1;
        }
#endif

        len += i;
        return 0;
    }

    DLLLOCAL int sprintf(const char *fmt, ...) {
        va_list args;
        while (true) {
            va_start(args, fmt);
            int rc = vsprintf(fmt, args);
            va_end(args);
            if (!rc)
                break;
        }
        return 0;
    }

    DLLLOCAL int vsnprintf(size_t size, const char* fmt, va_list args) {
        // ensure minimum space is free
        if ((allocated - len) < (unsigned)size) {
            allocated += (size + STR_CLASS_EXTRA);
            // resize buffer
            buf = (char*)realloc(buf, allocated * sizeof(char));
        }
        // copy formatted string to buffer
        int i = ::vsnprintf(buf + len, size, fmt, args);
        len += i;
        return i;
    }

    DLLLOCAL int snprintf(size_t size, const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        int i = vsnprintf(size, fmt, args);
        va_end(args);
        return i;
    }

    DLLLOCAL void concatUTF8FromUnicode(unsigned code);

    // writes a new qore_string_private with the characters reversed from this string
    // assumes the encoding is the same and the target length is 0
    DLLLOCAL void concat_reverse(qore_string_private& targ) const {
        assert(targ.getEncoding() == getEncoding());
        assert(!targ.len);

        targ.check_char(len);
        if (getEncoding()->isMultiByte()) {
            char* p = buf;
            char* targ_end = targ.buf + len;
            char* end = buf + len;
            while (p < end) {
                bool invalid;
                int bl = getEncoding()->getByteLen(p, end, 1, invalid);
                if (invalid) {
                    bl = 1;
                }
                targ_end -= bl;
                if (targ_end < targ.buf) {
                    break;
                }
                strncpy(targ_end, p, bl);
                p += bl;
            }
        } else {
            for (size_t i = 0; i < len; ++i) {
                targ.buf[i] = buf[len - i - 1];
            }
        }

        targ.buf[len] = '\0';
        targ.len = len;
    }

    DLLLOCAL int concatUnicode(unsigned code, ExceptionSink *xsink) {
        assert(xsink);
        if (getEncoding() == QCS_UTF8) {
            concatUTF8FromUnicode(code);
            return 0;
        }

        QoreString tmp(QCS_UTF8);
        tmp.concatUTF8FromUnicode(code);
        TempString ns(tmp.convertEncoding(getEncoding(), xsink));
        if (*xsink)
            return -1;
        concat(ns->priv);
        return 0;
    }

    DLLLOCAL void setRegexBaseOpts(QoreRegexBase& re, int opts);

    DLLLOCAL void setRegexOpts(QoreRegexSubst& re, int opts);

    DLLLOCAL void splice_simple(size_t offset, size_t length, QoreString* extract = nullptr);
    DLLLOCAL void splice_simple(size_t offset, size_t length, const char* str, size_t str_len,
            QoreString* extract = nullptr);
    DLLLOCAL void splice_complex(qore_offset_t offset, ExceptionSink* xsink, QoreString* extract = nullptr);
    DLLLOCAL void splice_complex(qore_offset_t offset, qore_offset_t length, ExceptionSink* xsink,
            QoreString* extract = nullptr);
    DLLLOCAL void splice_complex(qore_offset_t offset, qore_offset_t length, const QoreString* str,
            ExceptionSink* xsink, QoreString* extract = nullptr);
    DLLLOCAL int substr_simple(QoreString* str, qore_offset_t offset) const;
    DLLLOCAL int substr_simple(QoreString* str, qore_offset_t offset, qore_offset_t length) const;
    DLLLOCAL int substr_complex(QoreString* str, qore_offset_t offset, ExceptionSink* xsink) const;
    DLLLOCAL int substr_complex(QoreString* str, qore_offset_t offset, qore_offset_t length,
            ExceptionSink* xsink) const;

    DLLLOCAL int trimLeading(ExceptionSink* xsink, const intvec_t& vec);
    DLLLOCAL int trimLeading(ExceptionSink* xsink, const qore_string_private* chars);
    DLLLOCAL int trimTrailing(ExceptionSink* xsink, const intvec_t& vec);
    DLLLOCAL int trimTrailing(ExceptionSink* xsink, const qore_string_private* chars);

    DLLLOCAL void terminate(size_t size);

    DLLLOCAL int concatUnicode(unsigned code);

    DLLLOCAL int concatDecodeUriIntern(ExceptionSink* xsink, const qore_string_private& str,
            bool detect_query = false);

    DLLLOCAL int concatEncodeUriRequest(ExceptionSink* xsink, const qore_string_private& str);

    DLLLOCAL unsigned int getUnicodePointFromBytePos(size_t offset, unsigned& len, ExceptionSink* xsink) const;

    DLLLOCAL int concatEncode(ExceptionSink* xsink, const QoreString& str, unsigned code = CE_XHTML);
    DLLLOCAL int concatDecode(ExceptionSink* xsink, const QoreString& str, unsigned code = CD_ALL);

    DLLLOCAL int getUnicodeCharArray(intvec_t& vec, ExceptionSink* xsink) const {
        size_t j = 0;
        while (j < len) {
            unsigned clen;
            int c = getUnicodePointFromBytePos((qore_offset_t)j, clen, xsink);
            if (*xsink)
                return -1;
            vec.push_back(c);
            j += clen;
        }
        return 0;
    }

    DLLLOCAL int allocate(unsigned requested_size) {
        if ((unsigned)allocated >= requested_size)
            return 0;
        requested_size = (requested_size / 0x10 + 1) * 0x10; // fill complete cache line
        char* aux = (char*)realloc(buf, requested_size * sizeof(char));
        if (!aux) {
            assert(false);
            // FIXME: std::bad_alloc() should be thrown here;
            return -1;
        }
        buf = aux;
        allocated = requested_size;
        return 0;
    }

    DLLLOCAL const QoreEncoding* getEncoding() const {
        return encoding ? encoding : QCS_USASCII;
    }

    DLLLOCAL size_t getCharWidth(ExceptionSink* xsink) const;

    DLLLOCAL void concatBase64(const char* bbuf, size_t size, size_t maxlinelen, bool url_encode = false);

    DLLLOCAL static bool inVector(int c, const intvec_t& vec) {
        for (unsigned j = 0; j < vec.size(); ++j) {
            if ((int)vec[j] == c)
                return true;
        }
        return false;
    }

    DLLLOCAL static qore_string_private* get(QoreString& str) {
        return str.priv;
    }

    DLLLOCAL static qore_string_private* get(QoreString* str) {
        return str ? str->priv : nullptr;
    }

    DLLLOCAL static void adopt(QoreString& str, qore_string_private* p) {
        delete str.priv;
        str.priv = p;
    }

    DLLLOCAL static int getHex(const char*& p) {
        if (*p == '%' && isxdigit(*(p + 1)) && isxdigit(*(p + 2))) {
            char x[3] = { *(p + 1), *(p + 2), '\0' };
            p += 3;
            return strtol(x, 0, 16);
        }
        return -1;
    }

    DLLLOCAL static int convert_encoding_intern(const char* src, size_t src_len, const QoreEncoding* from,
            QoreString& targ, const QoreEncoding* nccs, ExceptionSink* xsink);
};

template <typename T>
DLLLOCAL T* binary_to_string(BinaryNode* bin, const QoreEncoding* qe) {
    SimpleRefHolder<BinaryNode> b(bin);
    if (!b) {
        return nullptr;
    }

    if (b->empty()) {
        return new QoreStringNode;
    }

    qore_string_private *p = new qore_string_private;
    p->len = b->size() - 1;
    p->buf = (char*)b->giveBuffer();
    p->encoding = qe;

    // free binary object
    b = 0;

    // check for null termination
    if (p->buf[p->len]) {
        ++p->len;
        p->buf = (char*)realloc(p->buf, p->len + 1);
        p->buf[p->len] = '\0';
    }

    p->allocated = p->len + 1;
    T* rv = new T();
    qore_string_private::adopt(*rv, p);
    return rv;
}

#endif
