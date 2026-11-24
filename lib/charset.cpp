/*
    charset.cpp

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

#include <qore/Qore.h>

#include <qore/intern/qore_encoding_private.h>
#include <qore/intern/qore_string_private.h>
#include "qore/intern/QoreForkHandlers.h"
#include "qore/intern/QoreLibIntern.h"
#include "qore/intern/QoreUtf8.h"
#include "qore/intern/QoreUtf16.h"
#include "qore/intern/QoreMiscMembersOps.h"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iconv.h>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <strings.h>

const QoreEncoding* QCS_DEFAULT, *QCS_USASCII, *QCS_UTF8,
    *QCS_UTF16, *QCS_UTF16BE, *QCS_UTF16LE,
    *QCS_ISO_8859_1, *QCS_ISO_8859_2, *QCS_ISO_8859_3, *QCS_ISO_8859_4,
    *QCS_ISO_8859_5, *QCS_ISO_8859_6, *QCS_ISO_8859_7, *QCS_ISO_8859_8,
    *QCS_ISO_8859_9, *QCS_ISO_8859_10, *QCS_ISO_8859_11, *QCS_ISO_8859_13,
    *QCS_ISO_8859_14, *QCS_ISO_8859_15, *QCS_ISO_8859_16,
    *QCS_KOI8_R, *QCS_KOI8_U, *QCS_KOI7, *QCS_WINDOWS_874,
    *QCS_WINDOWS_936, *QCS_WINDOWS_1250, *QCS_WINDOWS_1251,
    *QCS_WINDOWS_1252, *QCS_WINDOWS_1253, *QCS_WINDOWS_1254,
    *QCS_WINDOWS_1255, *QCS_WINDOWS_1256, *QCS_WINDOWS_1257,
    *QCS_WINDOWS_1258;


static size_t UTF8_getLength(const char* p, const char* end, bool& invalid);
static size_t UTF8_getByteLen(const char* p, const char* end, size_t l, bool& invalid);
static size_t UTF8_getCharPos(const char* p, const char* e, bool& invalid);
static unsigned UTF8_getUnicode(const char* p);

static size_t UTF16LE_getLength(const char* p, const char* end, bool& invalid);
static size_t UTF16LE_getByteLen(const char* p, const char* end, size_t l, bool& invalid);
static size_t UTF16LE_getCharPos(const char* p, const char* e, bool& invalid);
static unsigned UTF16LE_getUnicode(const char* p);

static size_t UTF16BE_getLength(const char* p, const char* end, bool& invalid);
static size_t UTF16BE_getByteLen(const char* p, const char* end, size_t l, bool& invalid);
static size_t UTF16BE_getCharPos(const char* p, const char* e, bool& invalid);
static unsigned UTF16BE_getUnicode(const char* p);

static unsigned UTF32BE_getUnicode(const char* p);

struct dbcs_bytes;
template <const dbcs_bytes* B>
static qore_offset_t DBCS_get_char_len(const char* p, size_t len);
extern const dbcs_bytes gbk_bytes;
template <mbcs_charlen_t CharLen>
static size_t mb_getLength(const char* p, const char* end, bool& invalid);
template <mbcs_charlen_t CharLen>
static size_t mb_getByteLen(const char* p, const char* end, size_t l, bool& invalid);
template <mbcs_charlen_t CharLen>
static size_t mb_getCharPos(const char* p, const char* end, bool& invalid);
static unsigned UTF32LE_getUnicode(const char* p);
static unsigned UCS2BE_getUnicode(const char* p);
static unsigned UCS2LE_getUnicode(const char* p);

encoding_map_t QoreEncodingManager::emap;
const_encoding_map_t QoreEncodingManager::amap;
QoreThreadLock QoreEncodingManager::mutex;
QoreEncodingManager QEM;

// the encoding maps can be used in a child process created with fork() (see QoreForkHandlers.h)
static QoreForkThreadLock qem_fork_handler(QoreEncodingManager::getLockForFork(), QFLR_LEAF, "encodings");

QoreEncoding::QoreEncoding(const char* code, const char* desc, unsigned char minwidth, unsigned char maxwidth,
        mbcs_length_t l, mbcs_end_t e, mbcs_pos_t p, mbcs_charlen_t c, mbcs_get_unicode_t gu, bool ascii_compat)
        : priv(new qore_encoding_private(code, desc, minwidth, maxwidth, l, e, p, c, gu, ascii_compat)) {
}

QoreEncoding::~QoreEncoding() {
    delete priv;
}

size_t QoreEncoding::getLength(const char* p, const char* end, bool& invalid) const {
    return priv->flength ? priv->flength(p, end, invalid) : strlen(p);
}

size_t QoreEncoding::getLength(const char* p, const char* end, ExceptionSink* xsink) const {
    if (!priv->flength)
        return strlen(p);

    bool invalid;
    size_t rc = priv->flength(p, end, invalid);
    if (invalid) {
        xsink->raiseException("INVALID-ENCODING", "invalid %s encoding encountered in string", priv->code.c_str());
        return 0;
    }
    return rc;
}

size_t QoreEncoding::getByteLen(const char* p, const char* end, size_t c, bool& invalid) const {
    return priv->fend ? priv->fend(p, end, c, invalid) : c;
}

size_t QoreEncoding::getByteLen(const char* p, const char* end, size_t c, ExceptionSink* xsink) const {
    if (!priv->fend) {
        // there is no character at or after the end
        size_t len = p < end ? static_cast<size_t>(end - p) : 0;
        if (c > len)
            c = len;
        return c;
    }

    bool invalid;
    size_t rc = priv->fend(p, end, c, invalid);
    if (invalid) {
        xsink->raiseException("INVALID-ENCODING", "invalid %s encoding encountered in string", priv->code.c_str());
        return 0;
    }
    return rc;
}

size_t QoreEncoding::getCharPos(const char* p, const char* end, bool& invalid) const {
    return priv->fpos ? priv->fpos(p, end, invalid) : end - p;
}

size_t QoreEncoding::getCharPos(const char* p, const char* end, ExceptionSink* xsink) const {
    if (!priv->fpos)
        return end - p;

    bool invalid;
    size_t rc = priv->fpos(p, end, invalid);
    if (invalid) {
        xsink->raiseException("INVALID-ENCODING", "invalid %s encoding encountered in string", priv->code.c_str());
        return 0;
    }
    return rc;
}

qore_offset_t QoreEncoding::getCharLen(const char* p, size_t valid_len) const {
    return priv->fcharlen ? priv->fcharlen(p, valid_len) : 1;
}

bool QoreEncoding::isMultiByte() const {
    return (bool)priv->flength;
}

const char* QoreEncoding::getCode() const {
    return priv->code.c_str();
}

const char* QoreEncoding::getDesc() const {
    return priv->desc.empty() ? "<no description available>" : priv->desc.c_str();
}

int QoreEncoding::getMaxCharWidth() const {
    return priv->maxwidth;
}

unsigned QoreEncoding::getMinCharWidth() const {
    return priv->getMinCharWidth();
}

bool QoreEncoding::isAsciiCompat() const {
    return priv->isAsciiCompat();
}

namespace {
//! the iconv descriptors that decode characters to UTF-8 for QoreEncoding::getUnicode() in the current thread
/** An encoding without a code point function (single-byte encodings other than ASCII, EBCDIC, and the multi-byte
    encodings such as EUC-JP, Shift_JIS, or GB18030) gets the code point of a character from iconv.  Opening a
    descriptor for each character would dominate the cost of a loop over the characters of a string, so a descriptor
    is opened once per encoding and thread and reused; iconv descriptors cannot be shared between threads, and a
    thread-local cache needs no lock, so other encodings and threads pay nothing for it.  The descriptors are closed
    when the thread exits.
*/
class ThreadUnicodeDecoders {
public:
    ThreadUnicodeDecoders() = default;

    ~ThreadUnicodeDecoders() {
        for (auto& i : decoders) {
            iconv_close(i.second);
        }
    }

    //! returns the descriptor for decoding the given encoding to UTF-8, or (iconv_t)-1 if it cannot be opened
    iconv_t get(const QoreEncoding* enc) {
        for (auto& i : decoders) {
            if (i.first == enc) {
                return i.second;
            }
        }
        iconv_t cd = iconv_open("UTF-8", qore_encoding_private::get(*enc)->getIconvSourceCode());
        if (cd != invalid()) {
            // encodings are never deleted while threads run, so the pointer identifies the encoding
            decoders.emplace_back(enc, cd);
        }
        return cd;
    }

    static iconv_t invalid() {
        return reinterpret_cast<iconv_t>(static_cast<intptr_t>(-1));
    }

private:
    // a thread uses few encodings, so a linear search is the fastest lookup
    std::vector<std::pair<const QoreEncoding*, iconv_t>> decoders;

    ThreadUnicodeDecoders(const ThreadUnicodeDecoders&) = delete;
    ThreadUnicodeDecoders& operator=(const ThreadUnicodeDecoders&) = delete;
};

thread_local ThreadUnicodeDecoders thread_unicode_decoders;

// needed for platforms where the input buffer is defined as "const char"
template<typename T>
size_t decoder_iconv(size_t (*iconv_f)(iconv_t, T, size_t*, char**, size_t*), iconv_t handle, char** inbuf,
        size_t* inavail, char** outbuf, size_t* outavail) {
    return (*iconv_f)(handle, const_cast<T>(inbuf), inavail, outbuf, outavail);
}

//! returns the code point of the character of the given byte length with iconv, or -1 if an exception was raised
int get_unicode_iconv(const QoreEncoding* enc, const char* p, unsigned clen, ExceptionSink* xsink) {
    iconv_t cd = thread_unicode_decoders.get(enc);
    if (cd == ThreadUnicodeDecoders::invalid()) {
        xsink->raiseException("ENCODING-CONVERSION-ERROR", "cannot convert from \"%s\" to \"UTF-8\"",
            enc->getCode());
        return -1;
    }
    // a new conversion state for each character, as the descriptor is reused
    decoder_iconv(::iconv, cd, nullptr, nullptr, nullptr, nullptr);
    // a character of up to 4 bytes gives at most 4 bytes of UTF-8, or 8 for a stateful encoding that writes a
    // sequence of characters (which are not decoded one at a time)
    char out[16];
    char* ib = const_cast<char*>(p);
    size_t il = clen;
    char* ob = out;
    size_t ol = sizeof(out);
    if (decoder_iconv(::iconv, cd, &ib, &il, &ob, &ol) == static_cast<size_t>(-1)
            || decoder_iconv(::iconv, cd, nullptr, nullptr, &ob, &ol) == static_cast<size_t>(-1)
            || ob == out) {
        xsink->raiseException("INVALID-ENCODING", "invalid %s encoding encountered in string", enc->getCode());
        return -1;
    }
    *ob = '\0';
    return UTF8_getUnicode(out);
}
}

int QoreEncoding::getUnicode(const char* p, const char* end, unsigned& clen, ExceptionSink* xsink) const {
    // get character length & check validity
    clen = (unsigned)getByteLen(p, end, 1, xsink);
    // an invalid character has no length; neither has the end of the string, where there is no character, and the
    // bytes after it are not part of the string
    if (!clen) {
        return *xsink ? -1 : 0;
    }

    if (!priv->get_unicode) {
        assert(this != QCS_UTF8);
        if (priv->ascii_compat && static_cast<unsigned char>(*p) < 128) {
            return *p;
        }

        // the character is converted with iconv; in an encoding that is not ASCII-compatible (ex: EBCDIC), an ASCII
        // byte is not the ASCII character
        return get_unicode_iconv(this, p, clen, xsink);
    }
    return priv->getUnicode(p);
}

const QoreEncoding* QoreEncodingManager::addUnlocked(const char* code, const char* desc, unsigned char minwidth,
        unsigned char maxwidth, mbcs_length_t l, mbcs_end_t e, mbcs_pos_t p, mbcs_charlen_t c,  mbcs_get_unicode_t gu,
        bool ascii_compat) {
    QoreEncoding* qcs = new QoreEncoding(code, desc, minwidth, maxwidth, l, e, p, c, gu, ascii_compat);
    emap[qcs->getCode()] = qcs;
    return qcs;
}

const QoreEncoding* QoreEncodingManager::add(const char* code, const char* desc, unsigned char maxwidth, mbcs_length_t l, mbcs_end_t e, mbcs_pos_t p, mbcs_charlen_t c) {
    QoreEncoding* qcs = new QoreEncoding(code, desc, 1, maxwidth, l, e, p, c);
    mutex.lock();
    emap[qcs->getCode()] = qcs;
    mutex.unlock();
    return qcs;
}

// have to handle HP-UX's non-standard names for character sets separately
#ifdef HPUX
#define ISO88591_STR "ISO8859-1"
#define ISO88592_STR "ISO8859-2"
#define ISO88593_STR "ISO8859-3"
#define ISO88594_STR "ISO8859-4"
#define ISO88595_STR "ISO8859-5"
#define ISO88596_STR "ISO8859-6"
#define ISO88597_STR "ISO8859-7"
#define ISO88598_STR "ISO8859-8"
#define ISO88599_STR "ISO8859-9"
#define ISO885910_STR "ISO8859-10"
#define ISO885911_STR "ISO8859-11"
#define ISO885913_STR "ISO8859-13"
#define ISO885914_STR "ISO8859-14"
#define ISO885915_STR "ISO8859-15"
#define ISO885916_STR "ISO8859-16"
#else
#define ISO88591_STR "ISO-8859-1"
#define ISO88592_STR "ISO-8859-2"
#define ISO88593_STR "ISO-8859-3"
#define ISO88594_STR "ISO-8859-4"
#define ISO88595_STR "ISO-8859-5"
#define ISO88596_STR "ISO-8859-6"
#define ISO88597_STR "ISO-8859-7"
#define ISO88598_STR "ISO-8859-8"
#define ISO88599_STR "ISO-8859-9"
#define ISO885910_STR "ISO-8859-10"
#define ISO885911_STR "ISO-8859-11"
#define ISO885913_STR "ISO-8859-13"
#define ISO885914_STR "ISO-8859-14"
#define ISO885915_STR "ISO-8859-15"
#define ISO885916_STR "ISO-8859-16"
#endif

QoreEncodingManager::QoreEncodingManager() {
    // add character sets and setup aliases

    QCS_USASCII = addUnlocked("US-ASCII", "7-bit ASCII character set");
    addAlias(QCS_USASCII, "ASCII");
    addAlias(QCS_USASCII, "USASCII");
    addAlias(QCS_USASCII, "US-ASCII");

    QCS_UTF8 = addUnlocked("UTF-8", "variable-width universal character set", 1, 4, UTF8_getLength, UTF8_getByteLen,
        UTF8_getCharPos, q_UTF8_get_char_len, UTF8_getUnicode);
    addAlias(QCS_UTF8, "UTF8");

    QCS_UTF16 = addUnlocked("UTF-16", "variable-width universal character set", 2, 4, UTF16BE_getLength,
        UTF16BE_getByteLen, UTF16BE_getCharPos, q_UTF16BE_get_char_len, UTF16BE_getUnicode, false);
    addAlias(QCS_UTF16, "UTF16");

    QCS_UTF16BE = addUnlocked("UTF-16BE", "variable-width universal character set, explicit big-endian encoding", 2,
        4, UTF16BE_getLength, UTF16BE_getByteLen, UTF16BE_getCharPos, q_UTF16BE_get_char_len, UTF16BE_getUnicode,
        false);
    addAlias(QCS_UTF16BE, "UTF16BE");

    QCS_UTF16LE = addUnlocked("UTF-16LE", "variable-width universal character set, explicit little-endian encoding",
        2, 4, UTF16LE_getLength, UTF16LE_getByteLen, UTF16LE_getCharPos, q_UTF16LE_get_char_len, UTF16LE_getUnicode,
        false);
    addAlias(QCS_UTF16LE, "UTF16LE");

    // every Unicode character can be represented in the built-in Unicode encodings, as in the Unicode forms created
    // on the fly (qore#6604)
    for (const QoreEncoding* enc : {QCS_UTF8, QCS_UTF16, QCS_UTF16BE, QCS_UTF16LE}) {
        qore_encoding_private::get(*const_cast<QoreEncoding*>(enc))->unicode_complete = true;
    }

    QCS_ISO_8859_1 = addUnlocked(ISO88591_STR, "latin-1, Western European character set");
    addAlias(QCS_ISO_8859_1, "ISO88591");
    addAlias(QCS_ISO_8859_1, "ISO-8859-1");
    addAlias(QCS_ISO_8859_1, "ISO8859-1");
    addAlias(QCS_ISO_8859_1, "ISO-88591");
    addAlias(QCS_ISO_8859_1, "ISO8859P1");
    addAlias(QCS_ISO_8859_1, "ISO81");
    addAlias(QCS_ISO_8859_1, "LATIN1");
    addAlias(QCS_ISO_8859_1, "LATIN-1");

    QCS_ISO_8859_2 = addUnlocked(ISO88592_STR, "latin-2, Central European character set");
    addAlias(QCS_ISO_8859_2, "ISO88592");
    addAlias(QCS_ISO_8859_2, "ISO-8859-2");
    addAlias(QCS_ISO_8859_2, "ISO8859-2");
    addAlias(QCS_ISO_8859_2, "ISO-88592");
    addAlias(QCS_ISO_8859_2, "ISO8859P2");
    addAlias(QCS_ISO_8859_2, "ISO82");
    addAlias(QCS_ISO_8859_2, "LATIN2");
    addAlias(QCS_ISO_8859_2, "LATIN-2");

    QCS_ISO_8859_3  = addUnlocked(ISO88593_STR, "latin-3, Southern European character set");
    addAlias(QCS_ISO_8859_3, "ISO88593");
    addAlias(QCS_ISO_8859_3, "ISO-8859-3");
    addAlias(QCS_ISO_8859_3, "ISO8859-3");
    addAlias(QCS_ISO_8859_3, "ISO-88593");
    addAlias(QCS_ISO_8859_3, "ISO8859P3");
    addAlias(QCS_ISO_8859_3, "ISO83");
    addAlias(QCS_ISO_8859_3, "LATIN3");
    addAlias(QCS_ISO_8859_3, "LATIN-3");

    QCS_ISO_8859_4  = addUnlocked(ISO88594_STR, "latin-4, Northern European character set");
    addAlias(QCS_ISO_8859_4, "ISO88594");
    addAlias(QCS_ISO_8859_4, "ISO-8859-4");
    addAlias(QCS_ISO_8859_4, "ISO8859-4");
    addAlias(QCS_ISO_8859_4, "ISO-88594");
    addAlias(QCS_ISO_8859_4, "ISO8859P4");
    addAlias(QCS_ISO_8859_4, "ISO84");
    addAlias(QCS_ISO_8859_4, "LATIN4");
    addAlias(QCS_ISO_8859_4, "LATIN-4");

    QCS_ISO_8859_5  = addUnlocked(ISO88595_STR, "Cyrillic character set");
    addAlias(QCS_ISO_8859_5, "ISO88595");
    addAlias(QCS_ISO_8859_5, "ISO-8859-5");
    addAlias(QCS_ISO_8859_5, "ISO8859-5");
    addAlias(QCS_ISO_8859_5, "ISO-88595");
    addAlias(QCS_ISO_8859_5, "ISO8859P5");
    addAlias(QCS_ISO_8859_5, "ISO85");

    QCS_ISO_8859_6  = addUnlocked(ISO88596_STR, "Arabic character set");
    addAlias(QCS_ISO_8859_6, "ISO88596");
    addAlias(QCS_ISO_8859_6, "ISO-8859-6");
    addAlias(QCS_ISO_8859_6, "ISO8859-6");
    addAlias(QCS_ISO_8859_6, "ISO-88596");
    addAlias(QCS_ISO_8859_6, "ISO8859P6");
    addAlias(QCS_ISO_8859_6, "ISO86");

    QCS_ISO_8859_7  = addUnlocked(ISO88597_STR, "Greek character set");
    addAlias(QCS_ISO_8859_7, "ISO88597");
    addAlias(QCS_ISO_8859_7, "ISO-8859-7");
    addAlias(QCS_ISO_8859_7, "ISO8859-7");
    addAlias(QCS_ISO_8859_7, "ISO-88597");
    addAlias(QCS_ISO_8859_7, "ISO8859P7");
    addAlias(QCS_ISO_8859_7, "ISO87");

    QCS_ISO_8859_8  = addUnlocked(ISO88598_STR, "Hebrew character set");
    addAlias(QCS_ISO_8859_8, "ISO88598");
    addAlias(QCS_ISO_8859_8, "ISO-8859-8");
    addAlias(QCS_ISO_8859_8, "ISO8859-8");
    addAlias(QCS_ISO_8859_8, "ISO-88598");
    addAlias(QCS_ISO_8859_8, "ISO8859P8");
    addAlias(QCS_ISO_8859_8, "ISO88");

    QCS_ISO_8859_9  = addUnlocked(ISO88599_STR, "latin-5, Turkish character set");
    addAlias(QCS_ISO_8859_9, "ISO88599");
    addAlias(QCS_ISO_8859_9, "ISO-8859-9");
    addAlias(QCS_ISO_8859_9, "ISO8859-9");
    addAlias(QCS_ISO_8859_9, "ISO-88599");
    addAlias(QCS_ISO_8859_9, "ISO8859P9");
    addAlias(QCS_ISO_8859_9, "ISO89");
    addAlias(QCS_ISO_8859_9, "LATIN5");
    addAlias(QCS_ISO_8859_9, "LATIN-5");

    QCS_ISO_8859_10 = addUnlocked(ISO885910_STR, "latin-6, Nordic character set");
    addAlias(QCS_ISO_8859_10, "ISO885910");
    addAlias(QCS_ISO_8859_10, "ISO-8859-10");
    addAlias(QCS_ISO_8859_10, "ISO8859-10");
    addAlias(QCS_ISO_8859_10, "ISO-885910");
    addAlias(QCS_ISO_8859_10, "ISO8859P10");
    addAlias(QCS_ISO_8859_10, "ISO810");
    addAlias(QCS_ISO_8859_10, "LATIN6");
    addAlias(QCS_ISO_8859_10, "LATIN-6");

    QCS_ISO_8859_11 = addUnlocked(ISO885911_STR, "Thai character set");
    addAlias(QCS_ISO_8859_11, "ISO885911");
    addAlias(QCS_ISO_8859_11, "ISO-8859-11");
    addAlias(QCS_ISO_8859_11, "ISO8859-11");
    addAlias(QCS_ISO_8859_11, "ISO-885911");
    addAlias(QCS_ISO_8859_11, "ISO8859P11");
    addAlias(QCS_ISO_8859_11, "ISO811");

    // there is no ISO-8859-12
    QCS_ISO_8859_13 = addUnlocked(ISO885913_STR, "latin-7, Baltic rim character set");
    addAlias(QCS_ISO_8859_13, "ISO885913");
    addAlias(QCS_ISO_8859_13, "ISO-8859-13");
    addAlias(QCS_ISO_8859_13, "ISO8859-13");
    addAlias(QCS_ISO_8859_13, "ISO-885913");
    addAlias(QCS_ISO_8859_13, "ISO8859P13");
    addAlias(QCS_ISO_8859_13, "ISO813");
    addAlias(QCS_ISO_8859_13, "LATIN7");
    addAlias(QCS_ISO_8859_13, "LATIN-7");

    QCS_ISO_8859_14 = addUnlocked(ISO885914_STR, "latin-8, Celtic character set");
    addAlias(QCS_ISO_8859_14, "ISO885914");
    addAlias(QCS_ISO_8859_14, "ISO-8859-14");
    addAlias(QCS_ISO_8859_14, "ISO8859-14");
    addAlias(QCS_ISO_8859_14, "ISO-885914");
    addAlias(QCS_ISO_8859_14, "ISO8859P14");
    addAlias(QCS_ISO_8859_14, "ISO814");
    addAlias(QCS_ISO_8859_14, "LATIN8");
    addAlias(QCS_ISO_8859_14, "LATIN-8");

    QCS_ISO_8859_15 = addUnlocked(ISO885915_STR, "latin-9, Western European with euro symbol");
    addAlias(QCS_ISO_8859_15, "ISO885915");
    addAlias(QCS_ISO_8859_15, "ISO-8859-15");
    addAlias(QCS_ISO_8859_15, "ISO8859-15");
    addAlias(QCS_ISO_8859_15, "ISO-885915");
    addAlias(QCS_ISO_8859_15, "ISO8859P15");
    addAlias(QCS_ISO_8859_15, "ISO815");
    addAlias(QCS_ISO_8859_15, "LATIN9");
    addAlias(QCS_ISO_8859_15, "LATIN-9");

    QCS_ISO_8859_16 = addUnlocked(ISO885916_STR, "latin-10, Southeast European character set");
    addAlias(QCS_ISO_8859_16, "ISO885916");
    addAlias(QCS_ISO_8859_16, "ISO-8859-16");
    addAlias(QCS_ISO_8859_16, "ISO8859-16");
    addAlias(QCS_ISO_8859_16, "ISO-885916");
    addAlias(QCS_ISO_8859_16, "ISO8859P16");
    addAlias(QCS_ISO_8859_16, "ISO816");
    addAlias(QCS_ISO_8859_16, "LATIN10");
    addAlias(QCS_ISO_8859_16, "LATIN-10");

    QCS_KOI8_R = addUnlocked("KOI8-R", "Russian: Kod Obmena Informatsiey, 8 bit");
    addAlias(QCS_KOI8_R, "KOI8R");

    QCS_KOI8_U = addUnlocked("KOI8-U", "Ukrainian: Kod Obmena Informatsiey, 8 bit");
    addAlias(QCS_KOI8_U, "KOI8U");

    QCS_KOI7 = addUnlocked("KOI7", "Russian: Kod Obmena Informatsiey, 7 bit characters");

    // Windows encodings
    QCS_WINDOWS_874 = addUnlocked("WINDOWS-874", "Windows 874: Latin/Thai, similar to ISO-8859-11");
    addAlias(QCS_WINDOWS_874, "WINDOWS874");
    addAlias(QCS_WINDOWS_874, "CP-874");
    addAlias(QCS_WINDOWS_874, "CP874");

    {
        // Windows 936 (GBK) has double-byte characters whose second byte can be an ASCII character, and the euro sign
        // 0x80
        QoreEncoding* qcs = new QoreEncoding("WINDOWS-936", "Windows 936: Simplified Chinese", 1, 2,
            mb_getLength<DBCS_get_char_len<&gbk_bytes>>, mb_getByteLen<DBCS_get_char_len<&gbk_bytes>>,
            mb_getCharPos<DBCS_get_char_len<&gbk_bytes>>, DBCS_get_char_len<&gbk_bytes>);
        qore_encoding_private::get(*qcs)->needs_boundary_check = true;
        qore_encoding_private::get(*qcs)->defined_multibyte = true;
        emap[qcs->getCode()] = qcs;
        QCS_WINDOWS_936 = qcs;
    }
    addAlias(QCS_WINDOWS_936, "WINDOWS936");
    addAlias(QCS_WINDOWS_936, "CP-936");
    addAlias(QCS_WINDOWS_936, "CP936");

    QCS_WINDOWS_1250 = addUnlocked("WINDOWS-1250", "Windows 1250: Central/Eastern European");
    addAlias(QCS_WINDOWS_1250, "WINDOWS1250");
    addAlias(QCS_WINDOWS_1250, "CP-1250");
    addAlias(QCS_WINDOWS_1250, "CP1250");

    QCS_WINDOWS_1251 = addUnlocked("WINDOWS-1251", "Windows 1251: Cyrillic: Russian, Ukrainian, Balarusian, "
        "Bulgarian, Serbian Cyrillic, Macedonian, and others");
    addAlias(QCS_WINDOWS_1251, "WINDOWS1251");
    addAlias(QCS_WINDOWS_1251, "CP-1251");
    addAlias(QCS_WINDOWS_1251, "CP1251");

    QCS_WINDOWS_1252 = addUnlocked("WINDOWS-1252", "Windows 1252: European: Spanish, French, German");
    addAlias(QCS_WINDOWS_1252, "WINDOWS1252");
    addAlias(QCS_WINDOWS_1252, "CP-1252");
    addAlias(QCS_WINDOWS_1252, "CP1252");

    QCS_WINDOWS_1253 = addUnlocked("WINDOWS-1253", "Windows 1253: Greek");
    addAlias(QCS_WINDOWS_1253, "WINDOWS1253");
    addAlias(QCS_WINDOWS_1253, "CP-1253");
    addAlias(QCS_WINDOWS_1253, "CP1253");

    QCS_WINDOWS_1254 = addUnlocked("WINDOWS-1254", "Windows 1254: Turkish");
    addAlias(QCS_WINDOWS_1254, "WINDOWS1254");
    addAlias(QCS_WINDOWS_1254, "CP-1254");
    addAlias(QCS_WINDOWS_1254, "CP1254");

    QCS_WINDOWS_1255 = addUnlocked("WINDOWS-1255", "Windows 1255: Hebrew");
    addAlias(QCS_WINDOWS_1255, "WINDOWS1255");
    addAlias(QCS_WINDOWS_1255, "CP-1255");
    addAlias(QCS_WINDOWS_1255, "CP1255");

    QCS_WINDOWS_1256 = addUnlocked("WINDOWS-1256", "Windows 1256: Arabic");
    addAlias(QCS_WINDOWS_1256, "WINDOWS1256");
    addAlias(QCS_WINDOWS_1256, "CP-1256");
    addAlias(QCS_WINDOWS_1256, "CP1256");

    QCS_WINDOWS_1257 = addUnlocked("WINDOWS-1257", "Windows 1257: Baltic languages");
    addAlias(QCS_WINDOWS_1257, "WINDOWS1257");
    addAlias(QCS_WINDOWS_1257, "CP-1257");
    addAlias(QCS_WINDOWS_1257, "CP1257");

    QCS_WINDOWS_1258 = addUnlocked("WINDOWS-1258", "Windows 1258: Vietnamese");
    addAlias(QCS_WINDOWS_1258, "WINDOWS1258");
    addAlias(QCS_WINDOWS_1258, "CP-1258");
    addAlias(QCS_WINDOWS_1258, "CP1258");

    // conversions between these and any ASCII text are known without iconv
    qore_encoding_private::get(*QCS_USASCII)->ascii_identical = 1;
    qore_encoding_private::get(*QCS_UTF8)->ascii_identical = 1;

    QCS_DEFAULT = QCS_UTF8;
};

QoreEncodingManager::~QoreEncodingManager() {
    encoding_map_t::iterator i;
    while ((i = emap.begin()) != emap.end()) {
        class QoreEncoding* qe = i->second;
        emap.erase(i);
        delete qe;
    }
}

void QoreEncodingManager::showEncodings() {
    for (encoding_map_t::const_iterator i = emap.begin(); i != emap.end(); i++)
        printf("%s: %s\n", i->first, i->second->getDesc());
}

void QoreEncodingManager::showAliases() {
    for (const_encoding_map_t::const_iterator i = amap.begin(); i != amap.end(); i++)
        if (strcmp(i->first, i->second->getCode()))
            printf("%s = %s: %s\n", i->first, i->second->getCode(), i->second->getDesc());
}

// Validates an encoding by attempting iconv_open from UTF-8
// Returns true if valid, false if invalid
static bool validateEncoding(const char* encoding) {
    iconv_t cd = iconv_open(encoding, "UTF-8");
    if (cd == (iconv_t)-1) {
        return false;
    }
    iconv_close(cd);
    return true;
}

/*  The default character encoding must be compatible with ASCII: program text, hash keys, and the output of the
    functions that format text are ASCII in it, and strings in the default encoding are parsed as ASCII text.  An
    encoding that is not (ex: UTF-16, UTF-32, EBCDIC, Shift_JIS) is refused:
    - given with --charset (\a def) or QORE_CHARSET, which select the encoding for Qore explicitly, the process exits
      with an error, as for an invalid name given with --charset: running with another encoding than the one
      requested would silently change the encoding of all output
    - given by the locale (LANG), which applies to every program and is not a choice made for Qore, an error is
      printed and UTF-8 is used, as for an encoding name in LANG that iconv does not know, so that Qore still runs in
      a locale with such an encoding (ex: ja_JP.SJIS)
*/
void qore_encoding_manager_ops::init(const char* def) {
    // ends the process for a default encoding that cannot be used; ARGV and QORE_ARGV are made before the library is
    // initialized, and exit() does not release them (qore#7000)
    auto refuse = []() {
        qore_release_argv();
        exit(1);
    };

    // returns the encoding for a valid name that is compatible with ASCII, otherwise prints an error
    auto get_default = [](const char* name, const char* source) -> const QoreEncoding* {
        const QoreEncoding* enc = QoreEncodingManager::findCreate(name);
        if (!enc->isAsciiCompat()) {
            fprintf(stderr, "qore: error: character encoding '%s' from %s is not compatible with ASCII and cannot be "
                "the default character encoding; use an ASCII-compatible encoding such as UTF-8\n", name, source);
            return nullptr;
        }
        return enc;
    };

    // now set default character set
    if (def) {
        // Validate encoding when explicitly provided on command line
        if (!validateEncoding(def)) {
            fprintf(stderr, "qore: error: invalid character encoding '%s'\n", def);
            fprintf(stderr, "Use 'qore --show-charsets' to list known encodings\n");
            refuse();
        }
        QCS_DEFAULT = get_default(def, "--charset");
        if (!QCS_DEFAULT) {
            refuse();
        }
        return;
    }
    // first see if QORE_CHARSET exists
    QoreEnvironmentValue estr_holder("QORE_CHARSET");
    const char* estr = estr_holder.c_str();
    if (estr) {
        if (validateEncoding(estr)) {
            QCS_DEFAULT = get_default(estr, "QORE_CHARSET");
            if (!QCS_DEFAULT) {
                refuse();
            }
        } else {
            fprintf(stderr, "qore: warning: invalid encoding '%s' in QORE_CHARSET, using UTF-8\n", estr);
            QCS_DEFAULT = QCS_UTF8;
        }
        return;
    }
    // try to get character set name from LANG variable; otherwise set QCS_DEFAULT to UTF-8
    QCS_DEFAULT = QCS_UTF8;
    QoreEnvironmentValue lang_holder("LANG");
    estr = lang_holder.c_str();
    const char* p;
    if (!estr || !(p = strrchr(estr, '.'))) {
        return;
    }
    // the encoding name ends at any modifier (ex: "@euro")
    std::string enc_name(p + 1);
    size_t at = enc_name.find('@');
    if (at != std::string::npos) {
        enc_name.erase(at);
    }
    if (!validateEncoding(enc_name.c_str())) {
        fprintf(stderr, "qore: warning: invalid encoding '%s' from LANG, using UTF-8\n", enc_name.c_str());
        return;
    }
    const QoreEncoding* enc = get_default(enc_name.c_str(), "LANG");
    if (!enc) {
        fprintf(stderr, "qore: using UTF-8 as the default character encoding\n");
        return;
    }
    QCS_DEFAULT = enc;
}

void QoreEncodingManager::addAlias(const QoreEncoding* qcs, const char* alias) {
    mutex.lock();
    amap[alias] = qcs;
    mutex.unlock();
}

// the private implementation of the registration of encodings, with access to the tables of QoreEncodingManager
class qore_encoding_manager_private {
public:
    //! finds or creates an encoding as findCreate() does for trusted code: any name that is not known is registered
    DLLLOCAL static const QoreEncoding* findCreateTrusted(const char* name);

    //! returns the encoding whose name or alias has the given key (its letters and digits in upper case), if any
    DLLLOCAL static const QoreEncoding* findByNameKey(const char* key);
};

// returns the canonical name of an encoding that Qore does not know, defined after the table of character set labels
static std::string qore_canonical_charset_name(const char* name);

const QoreEncoding* qore_encoding_manager_ops::findUnlocked(const char* name) {
    {
        encoding_map_t::const_iterator i = QoreEncodingManager::emap.find(name);
        if (i != QoreEncodingManager::emap.end())
            return i->second;
    }

    const_encoding_map_t::const_iterator i = QoreEncodingManager::amap.find(name);
    if (i != QoreEncodingManager::amap.end())
        return i->second;

    return 0;
}

// the name of the encoding that untrusted code gets for a name that it may not register (qore#6826)
static constexpr const char* QORE_UNREGISTERED_ENCODING = "X-QORE-UNREGISTERED";

// returns true if untrusted code may register an encoding of the given name that Qore does not know: a plain name of
// an encoding that iconv converts both ways; iconv also accepts names with any conversion suffix ("//...", or a
// trailing "/") and with leading whitespace, so that such names, and names with control characters or beyond a
// length that no encoding name has, would let it register names without bound (qore#6826)
static bool qore_untrusted_may_register_encoding(const char* name) {
    size_t len = strlen(name);
    if (!len || len > 64) {
        return false;
    }
    for (const char* p = name; *p; ++p) {
        unsigned char c = static_cast<unsigned char>(*p);
        if (c <= 0x20 || c >= 0x7f || c == '/' || c == '\\') {
            return false;
        }
    }
    iconv_t to = iconv_open(name, "UTF-8");
    if (to == reinterpret_cast<iconv_t>(-1)) {
        return false;
    }
    iconv_close(to);
    iconv_t from = iconv_open("UTF-8", name);
    if (from == reinterpret_cast<iconv_t>(-1)) {
        return false;
    }
    iconv_close(from);
    return true;
}

// returns the letters and digits of an encoding name in upper case: iconv ignores punctuation in names (glibc some
// of it, musl all of it), so that the names that it accepts for an encoding are unbounded but their keys are not
static std::string qore_encoding_name_key(const char* name) {
    std::string key;
    for (const char* p = name; *p; ++p) {
        char c = *p;
        if (c >= 'a' && c <= 'z') {
            key += static_cast<char>(c - 'a' + 'A');
        } else if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            key += c;
        }
    }
    return key;
}

const QoreEncoding* qore_encoding_manager_private::findByNameKey(const char* key) {
    AutoLocker al(QoreEncodingManager::mutex);
    for (const auto& i : QoreEncodingManager::emap) {
        if (qore_encoding_name_key(i.first) == key) {
            return i.second;
        }
    }
    for (const auto& i : QoreEncodingManager::amap) {
        if (qore_encoding_name_key(i.first) == key) {
            return i.second;
        }
    }
    return nullptr;
}

const QoreEncoding* QoreEncodingManager::findCreate(const char* name) {
    {
        AutoLocker al(mutex);
        if (const QoreEncoding* rv = qore_encoding_manager_ops::findUnlocked(name)) {
            return rv;
        }
    }
    // a name that is not known: code that is not trusted registers only a bounded set of names, as data that is not
    // trusted does (qore_find_untrusted_encoding()), and the encodings that iconv converts by their plain names
    if (qore_caller_is_untrusted()) {
        if (const QoreEncoding* rv = qore_find_untrusted_encoding(name)) {
            return rv;
        }
        // another spelling of a known name, which iconv accepts in unbounded spellings, gives its encoding; this
        // registers nothing
        std::string key = qore_encoding_name_key(name);
        if (!key.empty()) {
            if (const QoreEncoding* rv = qore_encoding_manager_private::findByNameKey(key.c_str())) {
                return rv;
            }
        }
        if (key.empty() || !qore_untrusted_may_register_encoding(name)) {
            return qore_encoding_manager_private::findCreateTrusted(QORE_UNREGISTERED_ENCODING);
        }
    }
    return qore_encoding_manager_private::findCreateTrusted(name);
}

const QoreEncoding* qore_encoding_manager_private::findCreateTrusted(const char* name) {
    AutoLocker al(QoreEncodingManager::mutex);
    const QoreEncoding* rv = qore_encoding_manager_ops::findUnlocked(name);
    if (rv) {
        return rv;
    }
    // an encoding that is not known is created with the properties of the encoding itself, with a canonical name that
    // does not depend on the spelling of its first use, as every later lookup, which ignores case, gives this encoding
    // (qore#6570)
    std::string code = qore_canonical_charset_name(name);
    std::unique_ptr<QoreEncoding> qcs(new QoreEncoding(code.c_str()));
    qore_encoding_private::get(*qcs)->probe();
    QoreEncodingManager::emap[qcs->getCode()] = qcs.get();
    return qcs.release();
}

const QoreEncoding* qore_encoding_manager_ops::find(const char* name) {
    AutoLocker al(QoreEncodingManager::mutex);
    return findUnlocked(name);
}

// the character set labels that a name from untrusted data may select besides the names and aliases of the encodings
// that Qore knows (see qore_find_untrusted_encoding()): standard character set names (IANA) that Qore has no built-in
// encoding for, and common labels of them and of built-in encodings (WHATWG Encoding labels and IANA aliases), each
// with the name of the encoding that it selects, which is registered on its first use if it is not built in; the table
// is fixed, so that such data can register only these encodings
static const struct {
    const char* label;
    const char* name;
} qore_untrusted_charset_labels[] = {
    // Japanese
    {"Shift_JIS", "Shift_JIS"}, {"SJIS", "Shift_JIS"}, {"x-sjis", "Shift_JIS"}, {"MS_Kanji", "Shift_JIS"},
    {"csShiftJIS", "Shift_JIS"}, {"Windows-31J", "Windows-31J"}, {"CP932", "CP932"}, {"EUC-JP", "EUC-JP"},
    {"x-euc-jp", "EUC-JP"}, {"csEUCPkdFmtJapanese", "EUC-JP"}, {"ISO-2022-JP", "ISO-2022-JP"},
    {"csISO2022JP", "ISO-2022-JP"}, {"ISO-2022-JP-2", "ISO-2022-JP-2"},
    // Korean
    {"EUC-KR", "EUC-KR"}, {"csEUCKR", "EUC-KR"}, {"ks_c_5601-1987", "CP949"}, {"ks_c_5601-1989", "CP949"},
    {"KSC_5601", "CP949"}, {"korean", "CP949"}, {"windows-949", "CP949"}, {"CP949", "CP949"}, {"UHC", "CP949"},
    // Chinese
    {"GB2312", "GB2312"}, {"csGB2312", "GB2312"}, {"EUC-CN", "GB2312"}, {"GBK", "GBK"}, {"x-gbk", "GBK"},
    {"GB18030", "GB18030"}, {"Big5", "Big5"}, {"csBig5", "Big5"}, {"x-x-big5", "Big5"}, {"CN-Big5", "Big5"},
    {"Big5-HKSCS", "Big5-HKSCS"}, {"CP950", "CP950"}, {"EUC-TW", "EUC-TW"},
    // Unicode
    {"UTF-32", "UTF-32"}, {"UTF-32BE", "UTF-32BE"}, {"UTF-32LE", "UTF-32LE"}, {"UTF-7", "UTF-7"},
    {"unicode-1-1-utf-8", "UTF-8"}, {"unicode11utf8", "UTF-8"}, {"x-unicode20utf8", "UTF-8"},
    // labels of built-in encodings
    {"ANSI_X3.4-1968", "US-ASCII"}, {"iso-ir-6", "US-ASCII"}, {"csASCII", "US-ASCII"}, {"us", "US-ASCII"},
    {"iso-ir-100", "ISO-8859-1"}, {"csISOLatin1", "ISO-8859-1"}, {"l1", "ISO-8859-1"}, {"iso_8859-1", "ISO-8859-1"},
    {"cp819", "ISO-8859-1"}, {"ibm819", "ISO-8859-1"},
    {"iso-ir-101", "ISO-8859-2"}, {"csISOLatin2", "ISO-8859-2"}, {"l2", "ISO-8859-2"}, {"iso_8859-2", "ISO-8859-2"},
    {"iso_8859-3", "ISO-8859-3"}, {"iso_8859-4", "ISO-8859-4"}, {"iso_8859-5", "ISO-8859-5"},
    {"iso_8859-6", "ISO-8859-6"}, {"iso_8859-7", "ISO-8859-7"}, {"iso_8859-8", "ISO-8859-8"},
    {"iso_8859-9", "ISO-8859-9"}, {"iso_8859-10", "ISO-8859-10"}, {"iso_8859-13", "ISO-8859-13"},
    {"iso_8859-14", "ISO-8859-14"}, {"iso_8859-15", "ISO-8859-15"}, {"iso_8859-16", "ISO-8859-16"},
    {"ISO-8859-8-I", "ISO-8859-8"}, {"csISO88598I", "ISO-8859-8"}, {"logical", "ISO-8859-8"},
    {"x-cp1250", "WINDOWS-1250"}, {"x-cp1251", "WINDOWS-1251"}, {"x-cp1252", "WINDOWS-1252"},
    {"x-cp1253", "WINDOWS-1253"}, {"x-cp1254", "WINDOWS-1254"}, {"x-cp1255", "WINDOWS-1255"},
    {"x-cp1256", "WINDOWS-1256"}, {"x-cp1257", "WINDOWS-1257"}, {"x-cp1258", "WINDOWS-1258"},
    {"dos-874", "WINDOWS-874"}, {"csKOI8R", "KOI8-R"}, {"koi", "KOI8-R"}, {"koi8", "KOI8-R"},
    // others
    {"TIS-620", "TIS-620"}, {"VISCII", "VISCII"}, {"macintosh", "macintosh"}, {"mac", "macintosh"},
    {"x-mac-roman", "macintosh"}, {"csMacintosh", "macintosh"}, {"IBM437", "IBM437"}, {"CP437", "IBM437"},
    {"IBM850", "IBM850"}, {"CP850", "IBM850"}, {"IBM852", "IBM852"}, {"CP852", "IBM852"}, {"IBM855", "IBM855"},
    {"IBM857", "IBM857"}, {"IBM862", "IBM862"}, {"IBM866", "IBM866"}, {"CP866", "IBM866"}, {"KOI8-RU", "KOI8-RU"},
};

// the canonical name of an encoding that Qore does not know: the standard spelling of its name in the table of
// character set labels (ex: "Shift_JIS" for "SHIFT_JIS"), or the name in upper case, as the names of the encodings that
// Qore knows are
static std::string qore_canonical_charset_name(const char* name) {
    for (const auto& label : qore_untrusted_charset_labels) {
        if (!strcasecmp(name, label.name)) {
            return label.name;
        }
    }
    // a name with a backslash (as every name with a NUL character has in its escaped form; see
    // findCreate(const QoreString*)) is kept as it is
    std::string code(name);
    if (code.find('\\') != std::string::npos) {
        return code;
    }
    for (char& c : code) {
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }
    return code;
}

const QoreEncoding* qore_find_untrusted_encoding(const char* name, size_t len) {
    // a name with a NUL character would be looked up cut at the NUL character
    if (!name || !len || memchr(name, '\0', len)) {
        return nullptr;
    }
    // the name is looked up as a NUL-terminated string
    assert(!name[len]);
    // a label selects the encoding of its name, whatever encodings the program has registered (qore#6570: a program
    // that used "x-sjis" as a name registered an encoding of that name, which a later lookup of the label gave)
    for (const auto& label : qore_untrusted_charset_labels) {
        if (!strcasecmp(name, label.label)) {
            const QoreEncoding* enc = qore_encoding_manager_ops::find(label.name);
            return enc ? enc : qore_encoding_manager_private::findCreateTrusted(label.name);
        }
    }
    return qore_encoding_manager_ops::find(name);
}

const QoreEncoding* qore_find_untrusted_encoding(const char* name) {
    return qore_find_untrusted_encoding(name, name ? strlen(name) : 0);
}

const QoreEncoding* QoreEncodingManager::findCreate(const QoreString* str) {
    // a name with a NUL character would be looked up cut at the NUL character and select the encoding named before
    // it; it is the name of an encoding that Qore does not know instead: the whole name, with every control
    // character escaped as \xNN and every backslash doubled, so that converting to or from it raises
    // ENCODING-CONVERSION-ERROR as for any other unknown name; like any other unknown name, it is registered once
    if (str->size() && memchr(str->c_str(), '\0', str->size())) {
        QoreString name;
        const char* p = str->c_str();
        for (size_t i = 0, e = str->size(); i < e; ++i) {
            unsigned char c = static_cast<unsigned char>(p[i]);
            if (c < 0x20 || c == 0x7f) {
                name.sprintf("\\x%02x", c);
            } else if (c == '\\') {
                name.concat("\\\\");
            } else {
                name.concat(static_cast<char>(c));
            }
        }
        return findCreate(name.c_str());
    }
    return findCreate(str->c_str());
}

// the UTF-8 encoding's character function: the one definition of a valid UTF-8 character (qore#6604)
qore_offset_t q_UTF8_get_char_len(const char* p, size_t len) {
    return q_utf8_char_len(p, len);
}

static size_t UTF8_getLength(const char* p, const char* end, bool& invalid) {
    size_t partial_count;
    size_t count = q_utf8_count_chars(p, end, &partial_count);
    invalid = count == Q_UTF8_INVALID_COUNT;
    return invalid ? partial_count : count;
}

static size_t UTF8_getByteLen(const char* p, const char* end, size_t l, bool& invalid) {
    size_t b = 0;
    while ((p < end) && l) {
        qore_offset_t bl = q_utf8_char_len(p, end - p);
        if (bl <= 0) {
            invalid = true;
            return b;
        }
        b += bl;
        p += bl;
        --l;
    }
    invalid = false;
    return b;
}

static size_t UTF8_getCharPos(const char* p, const char* end, bool& invalid) {
    size_t i = 0;
    while (p < end) {
        qore_offset_t l = q_utf8_char_len(p, end - p);
        if (l <= 0) {
            invalid = true;
            return i;
        }
        p += l;
        ++i;
    }

    invalid = false;
    return i;
}

static unsigned UTF8_getUnicode(const char* p) {
    if ((*p & 0xc0) == 0xc0) {
        // check for a 3-byte sequence
        if ((*p) & 0x20) {
            // check for a 4-byte sequence
            if ((*p) & 0x10) {
                // 4-byte char
                return (((unsigned)(p[0] & 0x07)) << 18)
                        | (((unsigned)(p[1] & 0x3f)) << 12)
                        | ((((unsigned)p[2] & 0x3f)) << 6)
                        | (((unsigned)p[3] & 0x3f));
            }
            // 3-byte char
            return ((p[0] & 0x0f) << 12)
                    | ((p[1] & 0x3f) << 6)
                    | (p[2] & 0x3f);
        }
        // 2-byte char
        return ((p[0] & 0x1f) << 6)
                | (p[1] & 0x3f);
    }
    return p[0];
}

// the UTF-16LE encoding's character function: the one definition of a valid UTF-16 character (qore#6604)
qore_offset_t q_UTF16LE_get_char_len(const char* p, size_t len) {
    return q_utf16_char_len<false>(p, len);
}

static unsigned UTF16LE_getUnicode(const char* p) {
    unsigned code_unit = (((unsigned char)p[1]) << 8) + ((unsigned char)p[0]);
    if (code_unit >= 0xd800 && code_unit <= 0xdbff) {
        unsigned code_unit_2 = (((unsigned char)p[3]) << 8) + ((unsigned char)p[2]);
        if (code_unit_2 >= 0xdc00 && code_unit_2 <= 0xdfff) {
            return (code_unit << 10) + code_unit_2 - 0x35fdc00;
        }
    }
    return code_unit;
}

static size_t UTF16LE_getLength(const char* p, const char* end, bool& invalid) {
    size_t i = 0;
    while (p < end) {
        qore_offset_t l = q_utf16_char_len<false>(p, end - p);
        //printd(5, "UTF16_getLength() p: %p end: %p len: %p l: %zd\n", p, end, end - p, l);
        if (l <= 0) {
            invalid = true;
            return i;
        }
        p += l;
        ++i;
    }

    invalid = false;
    return i;
}

static size_t UTF16LE_getByteLen(const char* p, const char* end, size_t l, bool& invalid) {
    size_t b = 0;
    while ((p < end) && l) {
        qore_offset_t bl = q_utf16_char_len<false>(p, end - p);
        if (bl <= 0) {
            invalid = true;
            return b;
        }
        b += bl;
        p += bl;
        --l;
    }
    invalid = false;
    return b;
}

static size_t UTF16LE_getCharPos(const char* p, const char* end, bool& invalid) {
    size_t i = 0;
    while (p < end) {
        qore_offset_t l = q_utf16_char_len<false>(p, end - p);
        if (l <= 0) {
            invalid = true;
            return i;
        }
        p += l;
        ++i;
    }

    invalid = false;
    return i;
}

// the character function of UTF-16BE and of the generic UTF-16 encoding, which is big-endian: the one definition of
// a valid UTF-16 character (qore#6604)
qore_offset_t q_UTF16BE_get_char_len(const char* p, size_t len) {
    return q_utf16_char_len<true>(p, len);
}

static unsigned UTF16BE_getUnicode(const char* p) {
    unsigned code_unit = (((unsigned char)p[0]) << 8) + ((unsigned char)p[1]);
    if (code_unit >= 0xd800 && code_unit <= 0xdbff) {
        unsigned code_unit_2 = (((unsigned char)p[2]) << 8) + ((unsigned char)p[3]);
        if (code_unit_2 >= 0xdc00 && code_unit_2 <= 0xdfff) {
            return (code_unit << 10) + code_unit_2 - 0x35fdc00;
        }
    }
    return code_unit;
}

static size_t UTF16BE_getLength(const char* p, const char* end, bool& invalid) {
    size_t i = 0;
    while (p < end) {
        qore_offset_t l = q_utf16_char_len<true>(p, end - p);
        //printd(5, "UTF16_getLength() p: %p end: %p len: %p l: %zd\n", p, end, end - p, l);
        if (l <= 0) {
            invalid = true;
            return i;
        }
        p += l;
        ++i;
    }

    invalid = false;
    return i;
}

static size_t UTF16BE_getByteLen(const char* p, const char* end, size_t l, bool& invalid) {
    size_t b = 0;
    while ((p < end) && l) {
        qore_offset_t bl = q_utf16_char_len<true>(p, end - p);
        if (bl <= 0) {
            invalid = true;
            return b;
        }
        b += bl;
        p += bl;
        --l;
    }
    invalid = false;
    return b;
}

static size_t UTF16BE_getCharPos(const char* p, const char* end, bool& invalid) {
    size_t i = 0;
    while (p < end) {
        qore_offset_t l = q_utf16_char_len<true>(p, end - p);
        if (l <= 0) {
            invalid = true;
            return i;
        }
        p += l;
        ++i;
    }

    invalid = false;
    return i;
}

size_t q_get_byte_len(const QoreEncoding* enc, const char* p, const char* end, size_t c, ExceptionSink* xsink) {
    return enc->getByteLen(p, end, c, xsink);
}

qore_offset_t q_get_char_len(const QoreEncoding* enc, const char* p, size_t valid_len, ExceptionSink* xsink) {
    qore_offset_t rc = enc->getCharLen(p, valid_len);
    if (rc <= 0) {
        xsink->raiseException("INVALID-ENCODING", "invalid %s encoding encountered in string", enc->getCode());
        return -1;
    }
    return rc;
}

// fixed-width Unicode encodings created on the fly (UTF-32*, UCS-4*, UCS-2*)

static unsigned UTF32BE_getUnicode(const char* p) {
    const unsigned char* u = reinterpret_cast<const unsigned char*>(p);
    return (static_cast<unsigned>(u[0]) << 24) | (static_cast<unsigned>(u[1]) << 16)
        | (static_cast<unsigned>(u[2]) << 8) | static_cast<unsigned>(u[3]);
}

static unsigned UTF32LE_getUnicode(const char* p) {
    const unsigned char* u = reinterpret_cast<const unsigned char*>(p);
    return (static_cast<unsigned>(u[3]) << 24) | (static_cast<unsigned>(u[2]) << 16)
        | (static_cast<unsigned>(u[1]) << 8) | static_cast<unsigned>(u[0]);
}

static unsigned UCS2BE_getUnicode(const char* p) {
    const unsigned char* u = reinterpret_cast<const unsigned char*>(p);
    return (static_cast<unsigned>(u[0]) << 8) | static_cast<unsigned>(u[1]);
}

static unsigned UCS2LE_getUnicode(const char* p) {
    const unsigned char* u = reinterpret_cast<const unsigned char*>(p);
    return (static_cast<unsigned>(u[1]) << 8) | static_cast<unsigned>(u[0]);
}

static bool is_surrogate(unsigned code) {
    return code >= 0xd800 && code <= 0xdfff;
}

// a UTF-32 character is a Unicode scalar value in four bytes
template <mbcs_get_unicode_t GetUnicode>
static qore_offset_t UTF32_get_char_len(const char* p, size_t len) {
    assert(len);
    if (len < 4) {
        return -4;
    }
    unsigned code = GetUnicode(p);
    return (code > 0x10ffff || is_surrogate(code)) ? 0 : 4;
}

// a UCS-2 character is a code point of the Basic Multilingual Plane other than a surrogate in two bytes
template <mbcs_get_unicode_t GetUnicode>
static qore_offset_t UCS2_get_char_len(const char* p, size_t len) {
    assert(len);
    if (len < 2) {
        return -2;
    }
    return is_surrogate(GetUnicode(p)) ? 0 : 2;
}

template <mbcs_charlen_t CharLen>
static size_t mb_getLength(const char* p, const char* end, bool& invalid) {
    size_t i = 0;
    while (p < end) {
        qore_offset_t l = CharLen(p, end - p);
        if (l <= 0) {
            invalid = true;
            return i;
        }
        p += l;
        ++i;
    }
    invalid = false;
    return i;
}

template <mbcs_charlen_t CharLen>
static size_t mb_getByteLen(const char* p, const char* end, size_t l, bool& invalid) {
    size_t b = 0;
    while ((p < end) && l) {
        qore_offset_t bl = CharLen(p, end - p);
        if (bl <= 0) {
            invalid = true;
            return b;
        }
        b += bl;
        p += bl;
        --l;
    }
    invalid = false;
    return b;
}

template <mbcs_charlen_t CharLen>
static size_t mb_getCharPos(const char* p, const char* end, bool& invalid) {
    // the character functions are the same
    return mb_getLength<CharLen>(p, end, invalid);
}

// variable-width multi-byte encodings that are compatible with ASCII (or Shift_JIS, which is not); the length of a
// character is given by its first byte (in GB18030, by its first two bytes)

// EUC encodings (EUC-JP, EUC-KR, EUC-CN / GB2312): ASCII, and code set 1 in two bytes 0xa1 - 0xfe; a byte that is
// not a character of the encoding is invalid (issue #6665):
// - SS2 and SS3 (the code sets 2 and 3 of EUC-JP): 0x8e and one byte 0xa1 - 0xdf (JIS X 0201 kana), and 0x8f and two
//   bytes 0xa1 - 0xfe (JIS X 0212); an EUC encoding without them has no character starting with 0x8e or 0x8f
// - C1: the C1 control characters 0x80 - 0x9f (other than SS2 and SS3 when they are used) are characters of the EUC
//   encodings that the standards of EUC-JP (JIS X 0208:1997 Annex 1) and EUC-KR (KS X 2901) define with them; the
//   others (EUC-CN, and the EUC encodings that Qore finds by probing) have no character 0x80 - 0xa0
template <bool C1, bool SS>
static qore_offset_t EUC_get_char_len(const char* p, size_t len) {
    assert(len);
    const unsigned char* u = reinterpret_cast<const unsigned char*>(p);
    if (u[0] < 0x80) {
        return 1;
    }
    if (u[0] >= 0xa1 && u[0] <= 0xfe) {
        if (len < 2) {
            return -2;
        }
        return (u[1] >= 0xa1 && u[1] <= 0xfe) ? 2 : 0;
    }
    if (SS && u[0] == 0x8e) {
        if (len < 2) {
            return -2;
        }
        return (u[1] >= 0xa1 && u[1] <= 0xdf) ? 2 : 0;
    }
    if (SS && u[0] == 0x8f) {
        // the following bytes that are there are checked first: a sequence is incomplete only if they can be part of
        // a character
        if (len >= 2 && (u[1] < 0xa1 || u[1] > 0xfe)) {
            return 0;
        }
        if (len < 3) {
            return -3;
        }
        return (u[2] >= 0xa1 && u[2] <= 0xfe) ? 3 : 0;
    }
    return (C1 && u[0] <= 0x9f) ? 1 : 0;
}

// the bytes of a double-byte encoding (issue #6665): for each byte, lead is 1 for a character of its own, 0 for a byte
// that is not a character or the first byte of one, and 2 + n for the first byte of a character whose second byte
// has bit n set in its entry of trail; the character functions of the double-byte encodings below are these tables
struct dbcs_bytes {
    unsigned char lead[256];
    unsigned char trail[256];
};

// a range of bytes
struct byte_range {
    unsigned first;
    unsigned last;
};

// returns the tables of a double-byte encoding whose first bytes in lead_ranges have second bytes in trail_ranges,
// and whose first bytes in lead_ranges2 (if any) have second bytes in trail_ranges2; 0x00 - 0x7f, and 0x80 if
// euro, are characters of their own
template <size_t L, size_t T, size_t L2 = 0, size_t T2 = 0>
constexpr dbcs_bytes make_dbcs_bytes(bool euro, const byte_range (&lead_ranges)[L], const byte_range (&trail_ranges)[T],
        const byte_range* lead_ranges2 = nullptr, const byte_range* trail_ranges2 = nullptr) {
    dbcs_bytes b{};
    for (unsigned i = 0; i < 0x80; ++i) {
        b.lead[i] = 1;
    }
    if (euro) {
        b.lead[0x80] = 1;
    }
    for (const byte_range& r : lead_ranges) {
        for (unsigned i = r.first; i <= r.last; ++i) {
            b.lead[i] = 2;
        }
    }
    for (const byte_range& r : trail_ranges) {
        for (unsigned i = r.first; i <= r.last; ++i) {
            b.trail[i] |= 1;
        }
    }
    for (size_t j = 0; j < L2; ++j) {
        for (unsigned i = lead_ranges2[j].first; i <= lead_ranges2[j].last; ++i) {
            b.lead[i] = 3;
        }
    }
    for (size_t j = 0; j < T2; ++j) {
        for (unsigned i = trail_ranges2[j].first; i <= trail_ranges2[j].last; ++i) {
            b.trail[i] |= 2;
        }
    }
    return b;
}

constexpr byte_range dbcs_leads[] = {{0x81, 0xfe}};
// the second bytes of GBK / CP936 and of the double-byte encodings that Qore finds by probing
constexpr byte_range gbk_trails[] = {{0x40, 0x7e}, {0x80, 0xfe}};
// the second bytes of Big5 and its forms
constexpr byte_range big5_trails[] = {{0x40, 0x7e}, {0xa1, 0xfe}};
constexpr byte_range big5_leads[] = {{0xa1, 0xf9}};
constexpr byte_range big5hkscs_leads[] = {{0x87, 0xfe}};
// the second bytes of UHC (CP949)
constexpr byte_range uhc_trails[] = {{0x41, 0x5a}, {0x61, 0x7a}, {0x81, 0xfe}};
// Johab (KS X 1001 Annex 3, CP1361): Hangul, and the other characters
constexpr byte_range johab_hangul_leads[] = {{0x84, 0xd3}};
constexpr byte_range johab_hangul_trails[] = {{0x41, 0x7e}, {0x81, 0xfe}};
constexpr byte_range johab_other_leads[] = {{0xd8, 0xde}, {0xe0, 0xf9}};
constexpr byte_range johab_other_trails[] = {{0x31, 0x7e}, {0x91, 0xfe}};

// a double-byte encoding that Qore finds by probing: 0xff is not a character, and nor is 0x80
constexpr dbcs_bytes dbcs_probed_bytes = make_dbcs_bytes(false, dbcs_leads, gbk_trails);
// GBK / CP936, and a probed double-byte encoding with the euro sign 0x80 as a character of its own
constexpr dbcs_bytes gbk_bytes = make_dbcs_bytes(true, dbcs_leads, gbk_trails);
constexpr dbcs_bytes big5_bytes = make_dbcs_bytes(false, big5_leads, big5_trails);
constexpr dbcs_bytes cp950_bytes = make_dbcs_bytes(false, dbcs_leads, big5_trails);
constexpr dbcs_bytes big5hkscs_bytes = make_dbcs_bytes(false, big5hkscs_leads, big5_trails);
constexpr dbcs_bytes uhc_bytes = make_dbcs_bytes(false, dbcs_leads, uhc_trails);
constexpr dbcs_bytes johab_bytes = make_dbcs_bytes<1, 2, 2, 2>(false, johab_hangul_leads, johab_hangul_trails,
    johab_other_leads, johab_other_trails);

// the character function of a double-byte encoding with the given tables
template <const dbcs_bytes* B>
static qore_offset_t DBCS_get_char_len(const char* p, size_t len) {
    assert(len);
    const unsigned char* u = reinterpret_cast<const unsigned char*>(p);
    unsigned c = B->lead[u[0]];
    if (c < 2) {
        return c;
    }
    if (len < 2) {
        return -2;
    }
    return (B->trail[u[1]] & (1u << (c - 2))) ? 2 : 0;
}

// ISO 6937 (ISO 6937-2, T.61, ANSI X3.110 / NAPLPS): a non-spacing diacritical mark (column C, 0xc1 - 0xcf) and the
// graphic character that follows it (0x20 - 0x7e: a basic letter, or the space, which gives the spacing form of the
// mark) are one character, and every other byte is a character, also the C1 control characters (0x80 - 0x9f), which
// the double-byte family that probing finds for these encodings refused (qore#6900); a byte or pair that the encoding
// does not assign is refused by its conversion, as an undefined byte of a single-byte encoding
static qore_offset_t ISO6937_get_char_len(const char* p, size_t len) {
    assert(len);
    const unsigned char* u = reinterpret_cast<const unsigned char*>(p);
    if (u[0] < 0xc1 || u[0] > 0xcf) {
        return 1;
    }
    if (len < 2) {
        return -2;
    }
    return (u[1] >= 0x20 && u[1] <= 0x7e) ? 2 : 0;
}

// GB18030: as GBK, and four bytes when the second byte is a digit: 0x81 - 0xfe, 0x30 - 0x39, 0x81 - 0xfe, 0x30 - 0x39
static qore_offset_t GB18030_get_char_len(const char* p, size_t len) {
    assert(len);
    const unsigned char* u = reinterpret_cast<const unsigned char*>(p);
    if (u[0] < 0x80) {
        return 1;
    }
    if (u[0] == 0x80 || u[0] == 0xff) {
        return 0;
    }
    if (len < 2) {
        return -2;
    }
    if (u[1] >= 0x30 && u[1] <= 0x39) {
        // the following bytes that are there are checked first: a sequence is incomplete only if they can be part of
        // a character
        if (len >= 3 && (u[2] < 0x81 || u[2] == 0xff)) {
            return 0;
        }
        if (len < 4) {
            return -4;
        }
        return (u[3] < 0x30 || u[3] > 0x39) ? 0 : 4;
    }
    return (u[1] < 0x40 || u[1] == 0x7f || u[1] == 0xff) ? 0 : 2;
}

// JIS X 0201 Roman/control bytes and half-width kana use one byte. JIS X 0208 Appendix 1
// uses the lead ranges 81-9f and e0-ef. Windows-31J and other extended forms go through fc.
// The bytes 0x80, 0xa0, and those after the last lead byte are not characters of any form (they are undefined in
// Microsoft's code page 932 as well), so they are invalid (issue #6665).
template <bool Extended>
static qore_offset_t SJIS_get_char_len(const char* p, size_t len) {
    assert(len);
    const unsigned char* u = reinterpret_cast<const unsigned char*>(p);
    if (u[0] <= 0x7f || (u[0] >= 0xa1 && u[0] <= 0xdf)) {
        return 1;
    }
    constexpr unsigned last_lead = Extended ? 0xfc : 0xef;
    if (!((u[0] >= 0x81 && u[0] <= 0x9f) || (u[0] >= 0xe0 && u[0] <= last_lead))) {
        return 0;
    }
    if (len < 2) {
        return -2;
    }
    return (u[1] < 0x40 || u[1] == 0x7f || u[1] > 0xfc) ? 0 : 2;
}

// the character functions of an encoding
struct form_functions {
    mbcs_charlen_t charlen;
    mbcs_get_unicode_t get_unicode;
    mbcs_length_t length;
    mbcs_end_t end;
    mbcs_pos_t pos;
    unsigned char minwidth;
    unsigned char maxwidth;

    // returns the functions for an encoding with the given character length and code point functions
    template <mbcs_charlen_t CharLen, mbcs_get_unicode_t GetUnicode>
    static form_functions get(unsigned char minwidth, unsigned char maxwidth) {
        return form_functions{CharLen, GetUnicode, mb_getLength<CharLen>, mb_getByteLen<CharLen>,
            mb_getCharPos<CharLen>, minwidth, maxwidth};
    }
};

namespace {
// the character encoding forms of Unicode that an encoding created on the fly can use
enum class UnicodeForm {
    UTF32BE,
    UTF32LE,
    UTF16BE,
    UTF16LE,
};

// returns the bytes of the given code point in the given encoding form
std::string encode_unicode(UnicodeForm form, unsigned code) {
    std::string rv;
    auto add16 = [&rv, form](unsigned unit) {
        if (form == UnicodeForm::UTF16BE) {
            rv += static_cast<char>((unit >> 8) & 0xff);
            rv += static_cast<char>(unit & 0xff);
        } else {
            rv += static_cast<char>(unit & 0xff);
            rv += static_cast<char>((unit >> 8) & 0xff);
        }
    };
    switch (form) {
        case UnicodeForm::UTF32BE:
            for (int shift = 24; shift >= 0; shift -= 8) {
                rv += static_cast<char>((code >> shift) & 0xff);
            }
            break;
        case UnicodeForm::UTF32LE:
            for (int shift = 0; shift <= 24; shift += 8) {
                rv += static_cast<char>((code >> shift) & 0xff);
            }
            break;
        case UnicodeForm::UTF16BE:
        case UnicodeForm::UTF16LE:
            if (code > 0xffff) {
                code -= 0x10000;
                add16(0xd800 + (code >> 10));
                add16(0xdc00 + (code & 0x3ff));
            } else {
                add16(code);
            }
            break;
    }
    return rv;
}

// an iconv conversion descriptor for probing the properties of an encoding; closed when the object is destroyed
class ProbeConverter {
public:
    ProbeConverter(const char* to, const char* from) : cd(iconv_open(to, from)) {
    }

    ~ProbeConverter() {
        if (valid()) {
            iconv_close(cd);
        }
    }

    bool valid() const {
        // iconv_open() returns (iconv_t)-1 for an error
        return cd != reinterpret_cast<iconv_t>(static_cast<intptr_t>(-1));
    }

    // converts a short text with a new conversion state, including the bytes that return the output to the initial
    // shift state; returns false if the text cannot be converted or a character is converted non-reversibly
    bool convert(const std::string& in, std::string& out) {
        assert(valid());
        out.clear();
        if (iconv_adapter(::iconv, cd, nullptr, nullptr, nullptr, nullptr) == static_cast<size_t>(-1)) {
            return false;
        }
        // the probe texts have at most two characters
        char buf[64];
        char* ib = const_cast<char*>(in.data());
        size_t il = in.size();
        char* ob = buf;
        size_t ol = sizeof(buf);
        size_t rc = iconv_adapter(::iconv, cd, &ib, &il, &ob, &ol);
        if (rc == static_cast<size_t>(-1) || rc > 0 || il) {
            return false;
        }
        if (iconv_adapter(::iconv, cd, nullptr, nullptr, &ob, &ol) == static_cast<size_t>(-1)) {
            return false;
        }
        out.assign(buf, ob - buf);
        return true;
    }

private:
    iconv_t cd;

    // needed for platforms where the input buffer is defined as "const char"
    template<typename T>
    static size_t iconv_adapter(size_t (*iconv_f)(iconv_t, T, size_t*, char**, size_t*), iconv_t handle,
            char** inbuf, size_t* inavail, char** outbuf, size_t* outavail) {
        return (*iconv_f)(handle, const_cast<T>(inbuf), inavail, outbuf, outavail);
    }

    ProbeConverter(const ProbeConverter&) = delete;
    ProbeConverter& operator=(const ProbeConverter&) = delete;
};

// returns true if iconv converts text in an encoding with the characters of the given Unicode form from the given
// name to UTF-8 although the text has a code unit sequence that is not a character of the form: a value above
// U+10FFFF or a surrogate in UTF-32 (glibc's "UCS-4" converts values up to 0x7fffffff), an unpaired surrogate in
// UTF-16, or a surrogate in UCS-2; also if the conversion cannot be opened, so that the text is validated then
static bool iconv_accepts_invalid_form(const char* source, UnicodeForm form, bool utf32, bool non_bmp) {
    static const std::vector<unsigned> utf32_samples[] = {{0x110000}, {0xd800}, {0xdc00}};
    static const std::vector<unsigned> utf16_samples[] = {{0xdc00, 0x41}, {0xd800, 0x41}};
    static const std::vector<unsigned> ucs2_samples[] = {{0xd800}, {0xdc00}};
    const std::vector<unsigned>* samples = utf32 ? utf32_samples : (non_bmp ? utf16_samples : ucs2_samples);
    size_t num = utf32 ? 3 : 2;
    for (size_t i = 0; i < num; ++i) {
        // the byte order mark gives the byte order to an encoding that takes it from the text
        std::string in = encode_unicode(form, 0xfeff) + encode_unicode(form, 0x61);
        for (unsigned unit : samples[i]) {
            in += encode_unicode(form, unit);
        }
        in += encode_unicode(form, 0x62);
        ProbeConverter dec("UTF-8", source);
        std::string out;
        if (!dec.valid() || dec.convert(in, out)) {
            return true;
        }
    }
    return false;
}

// a character converted to the encoding being probed
struct probe_char {
    // the Unicode code point
    unsigned code;
    // the character in UTF-8
    const char* utf8;
};

// characters from different scripts and planes: ASCII, Latin-1, Greek, Cyrillic, the euro sign, CJK, and an emoji
// outside the Basic Multilingual Plane
const probe_char probe_chars[] = {
    {0x41, "A"},
    {0x30, "0"},
    {0x0a, "\n"},
    {0xe9, "\xc3\xa9"},
    {0x3a9, "\xce\xa9"},
    {0x416, "\xd0\x96"},
    {0x20ac, "\xe2\x82\xac"},
    {0x4e2d, "\xe4\xb8\xad"},
    {0x1f600, "\xf0\x9f\x98\x80"},
};
}

namespace {
// the variable-width multi-byte encoding families that an encoding created on the fly can be decoded as
struct multibyte_family {
    // the name of the family, for debugging
    const char* name;
    mbcs_charlen_t charlen;
    mbcs_length_t length;
    mbcs_end_t end;
    mbcs_pos_t pos;
    unsigned char maxwidth;
    // true if the encoding is not compatible with ASCII (Shift_JIS)
    bool not_ascii_compat;
};

#define QORE_MULTIBYTE_FAMILY(name, cl, maxwidth, not_ascii_compat) \
    {name, cl, mb_getLength<cl>, mb_getByteLen<cl>, mb_getCharPos<cl>, maxwidth, not_ascii_compat}

// in the order they are checked: an encoding is decoded as the first family that gives the length of every sample
// character; the families with fewer multi-byte first bytes come first
const multibyte_family multibyte_families[] = {
    QORE_MULTIBYTE_FAMILY("EUC", (EUC_get_char_len<false, false>), 2, false),
    // with the code sets 2 and 3 of EUC-JP
    QORE_MULTIBYTE_FAMILY("EUC-SS", (EUC_get_char_len<false, true>), 3, false),
    QORE_MULTIBYTE_FAMILY("DBCS", DBCS_get_char_len<&dbcs_probed_bytes>, 2, false),
    // with the euro sign 0x80 (CP936)
    QORE_MULTIBYTE_FAMILY("DBCS-Euro", DBCS_get_char_len<&gbk_bytes>, 2, false),
    QORE_MULTIBYTE_FAMILY("GB18030", GB18030_get_char_len, 4, false),
    // Shift_JIS is not ASCII-compatible: its bytes 0x5c and 0x7e are the yen sign and overline of JIS X 0201, and
    // iconv implementations and variants (ex: CP932) map them differently, so it is handled the same way on every
    // platform; that the second bytes of its double-byte characters can be ASCII characters is handled by the
    // character boundary checks of all of these families
    QORE_MULTIBYTE_FAMILY("SJIS", SJIS_get_char_len<true>, 2, true),
};

// the encodings whose definitions Qore knows get the characters of their definitions, once probing has found that
// they are multi-byte encodings of the same structure (issue #6665)
const struct {
    const char* name;
    multibyte_family family;
} named_multibyte_families[] = {
    {"EUC-JP", QORE_MULTIBYTE_FAMILY("EUC-JP", (EUC_get_char_len<true, true>), 3, false)},
    {"EUC-KR", QORE_MULTIBYTE_FAMILY("EUC-KR", (EUC_get_char_len<true, false>), 2, false)},
    {"GBK", QORE_MULTIBYTE_FAMILY("GBK", DBCS_get_char_len<&gbk_bytes>, 2, false)},
    {"CP936", QORE_MULTIBYTE_FAMILY("GBK", DBCS_get_char_len<&gbk_bytes>, 2, false)},
    {"Big5", QORE_MULTIBYTE_FAMILY("Big5", DBCS_get_char_len<&big5_bytes>, 2, false)},
    {"CP950", QORE_MULTIBYTE_FAMILY("CP950", DBCS_get_char_len<&cp950_bytes>, 2, false)},
    {"Big5-HKSCS", QORE_MULTIBYTE_FAMILY("Big5-HKSCS", DBCS_get_char_len<&big5hkscs_bytes>, 2, false)},
    {"BIG5HKSCS", QORE_MULTIBYTE_FAMILY("Big5-HKSCS", DBCS_get_char_len<&big5hkscs_bytes>, 2, false)},
    {"CP949", QORE_MULTIBYTE_FAMILY("UHC", DBCS_get_char_len<&uhc_bytes>, 2, false)},
    {"UHC", QORE_MULTIBYTE_FAMILY("UHC", DBCS_get_char_len<&uhc_bytes>, 2, false)},
    {"JOHAB", QORE_MULTIBYTE_FAMILY("JOHAB", DBCS_get_char_len<&johab_bytes>, 2, false)},
    {"CP1361", QORE_MULTIBYTE_FAMILY("JOHAB", DBCS_get_char_len<&johab_bytes>, 2, false)},
    // other names of these encodings that iconv implementations know (glibc's eucJP-ms is EUC-JP with the user-defined
    // area, which has the same byte structure) (qore#6796)
    {"eucJP-ms", QORE_MULTIBYTE_FAMILY("EUC-JP", (EUC_get_char_len<true, true>), 3, false)},
    {"eucJP-open", QORE_MULTIBYTE_FAMILY("EUC-JP", (EUC_get_char_len<true, true>), 3, false)},
    {"csEUCPkdFmtJapanese", QORE_MULTIBYTE_FAMILY("EUC-JP", (EUC_get_char_len<true, true>), 3, false)},
    {"csEUCKR", QORE_MULTIBYTE_FAMILY("EUC-KR", (EUC_get_char_len<true, false>), 2, false)},
    {"MS936", QORE_MULTIBYTE_FAMILY("GBK", DBCS_get_char_len<&gbk_bytes>, 2, false)},
    {"Windows-936", QORE_MULTIBYTE_FAMILY("GBK", DBCS_get_char_len<&gbk_bytes>, 2, false)},
    {"csBig5", QORE_MULTIBYTE_FAMILY("Big5", DBCS_get_char_len<&big5_bytes>, 2, false)},
    {"BIG-FIVE", QORE_MULTIBYTE_FAMILY("Big5", DBCS_get_char_len<&big5_bytes>, 2, false)},
    // glibc: eucJP-win (eucJP-ms), UJIS and OSF00030010 (EUC-JP), OSF0004000A (EUC-KR), MSCP1361 (JOHAB)
    {"eucJP-win", QORE_MULTIBYTE_FAMILY("EUC-JP", (EUC_get_char_len<true, true>), 3, false)},
    {"UJIS", QORE_MULTIBYTE_FAMILY("EUC-JP", (EUC_get_char_len<true, true>), 3, false)},
    {"OSF00030010", QORE_MULTIBYTE_FAMILY("EUC-JP", (EUC_get_char_len<true, true>), 3, false)},
    {"OSF0004000A", QORE_MULTIBYTE_FAMILY("EUC-KR", (EUC_get_char_len<true, false>), 2, false)},
    {"MSCP1361", QORE_MULTIBYTE_FAMILY("JOHAB", DBCS_get_char_len<&johab_bytes>, 2, false)},
    // EUC-CN (GB2312): code set 1 only, no C1 character (see EUC_get_char_len())
    {"GB2312", QORE_MULTIBYTE_FAMILY("EUC-CN", (EUC_get_char_len<false, false>), 2, false)},
    {"EUC-CN", QORE_MULTIBYTE_FAMILY("EUC-CN", (EUC_get_char_len<false, false>), 2, false)},
    {"CN-GB", QORE_MULTIBYTE_FAMILY("EUC-CN", (EUC_get_char_len<false, false>), 2, false)},
    {"csGB2312", QORE_MULTIBYTE_FAMILY("EUC-CN", (EUC_get_char_len<false, false>), 2, false)},
    {"GB18030", QORE_MULTIBYTE_FAMILY("GB18030", GB18030_get_char_len, 4, false)},
    // glibc: GB13000 (GBK), CN-BIG5 (Big5), MSCP949 and OSF100203B5 (UHC)
    {"GB13000", QORE_MULTIBYTE_FAMILY("GBK", DBCS_get_char_len<&gbk_bytes>, 2, false)},
    {"CN-BIG5", QORE_MULTIBYTE_FAMILY("Big5", DBCS_get_char_len<&big5_bytes>, 2, false)},
    {"MSCP949", QORE_MULTIBYTE_FAMILY("UHC", DBCS_get_char_len<&uhc_bytes>, 2, false)},
    {"OSF100203B5", QORE_MULTIBYTE_FAMILY("UHC", DBCS_get_char_len<&uhc_bytes>, 2, false)},
    // the encodings of ISO 6937 and its forms, whose non-spacing diacritical marks make two-byte characters with the
    // following character, so that probing found a double-byte family for them (qore#6900); glibc's names
    {"ISO6937", QORE_MULTIBYTE_FAMILY("ISO6937", ISO6937_get_char_len, 2, false)},
    {"ISO_6937:1992", QORE_MULTIBYTE_FAMILY("ISO6937", ISO6937_get_char_len, 2, false)},
    {"ISO-IR-156", QORE_MULTIBYTE_FAMILY("ISO6937", ISO6937_get_char_len, 2, false)},
    {"ISO_6937-2", QORE_MULTIBYTE_FAMILY("ISO6937", ISO6937_get_char_len, 2, false)},
    {"ISO_6937-2:1983", QORE_MULTIBYTE_FAMILY("ISO6937", ISO6937_get_char_len, 2, false)},
    {"ISO-IR-90", QORE_MULTIBYTE_FAMILY("ISO6937", ISO6937_get_char_len, 2, false)},
    {"CSISO90", QORE_MULTIBYTE_FAMILY("ISO6937", ISO6937_get_char_len, 2, false)},
    {"T.61", QORE_MULTIBYTE_FAMILY("ISO6937", ISO6937_get_char_len, 2, false)},
    {"T.61-8BIT", QORE_MULTIBYTE_FAMILY("ISO6937", ISO6937_get_char_len, 2, false)},
    {"ISO-IR-103", QORE_MULTIBYTE_FAMILY("ISO6937", ISO6937_get_char_len, 2, false)},
    {"CSISO103T618BIT", QORE_MULTIBYTE_FAMILY("ISO6937", ISO6937_get_char_len, 2, false)},
    {"NAPLPS", QORE_MULTIBYTE_FAMILY("ISO6937", ISO6937_get_char_len, 2, false)},
    {"CSISO99NAPLPS", QORE_MULTIBYTE_FAMILY("ISO6937", ISO6937_get_char_len, 2, false)},
    {"ANSI_X3.110", QORE_MULTIBYTE_FAMILY("ISO6937", ISO6937_get_char_len, 2, false)},
    {"ANSI_X3.110-1983", QORE_MULTIBYTE_FAMILY("ISO6937", ISO6937_get_char_len, 2, false)},
    {"CSA_T500", QORE_MULTIBYTE_FAMILY("ISO6937", ISO6937_get_char_len, 2, false)},
    {"CSA_T500-1983", QORE_MULTIBYTE_FAMILY("ISO6937", ISO6937_get_char_len, 2, false)},
    {"ISO-IR-99", QORE_MULTIBYTE_FAMILY("ISO6937", ISO6937_get_char_len, 2, false)},
};
#undef QORE_MULTIBYTE_FAMILY

// returns the letters and digits of the name in upper case (qore_encoding_name_key()), without any conversion flags
// after "//": iconv implementations ignore punctuation in names (ex: "EUCJP" is "EUC-JP" for glibc; musl ignores all
// of it, so "Shift.JIS" is Shift_JIS there), and every spelling that iconv accepts must find the definition of its
// family (qore#6796)
static std::string normalize_multibyte_family_name(const std::string& name) {
    return qore_encoding_name_key(name.substr(0, name.find("//")).c_str());
}

// returns the family of the encoding of the given name, if Qore knows its definition; any spelling of the name that
// iconv accepts is matched (qore#6796: "EUCJP" and "EUCKR" got the generic EUC family, which refuses the C1 bytes
// of EUC-JP and EUC-KR that iconv decodes)
const multibyte_family* get_named_multibyte_family(const std::string& code) {
    std::string name = normalize_multibyte_family_name(code);
    for (const auto& i : named_multibyte_families) {
        if (name == normalize_multibyte_family_name(i.name)) {
            return &i.family;
        }
    }
    return nullptr;
}

// sample characters for the multi-byte encodings: Japanese (kanji, hiragana, katakana, half-width katakana, JIS X 0212
// in EUC-JP), Chinese (GB2312, GBK, Big5, and a character outside the Basic Multilingual Plane, four bytes in
// GB18030), Korean (KS X 1001 and UHC), and Latin characters
const probe_char multibyte_probe_chars[] = {
    {0x65e5, "\xe6\x97\xa5"},     // 日
    {0x306e, "\xe3\x81\xae"},     // の
    {0x30dd, "\xe3\x83\x9d"},     // ポ (Shift_JIS 0x83 0x7c)
    {0x30bd, "\xe3\x82\xbd"},     // ソ (Shift_JIS 0x83 0x5c)
    {0x8868, "\xe8\xa1\xa8"},     // 表 (Shift_JIS 0x95 0x5c)
    {0xff71, "\xef\xbd\xb1"},     // half-width katakana A
    {0x4e02, "\xe4\xb8\x82"},     // 丂 (GBK 0x81 0x40)
    {0x4e2d, "\xe4\xb8\xad"},     // 中
    {0x8a31, "\xe8\xa8\xb1"},     // 許 (Big5 0xb3 0x5c)
    {0x4e00, "\xe4\xb8\x80"},     // 一 (Big5 0xa4 0x40)
    {0xd55c, "\xed\x95\x9c"},     // 한
    {0xac02, "\xea\xb0\x82"},     // 갂 (UHC 0x81 0x41)
    {0x1f600, "\xf0\x9f\x98\x80"}, // emoji
    {0xe9, "\xc3\xa9"},            // é (EUC-JP 0x8f 0xab 0xb1)
    {0x20ac, "\xe2\x82\xac"},     // euro sign
};

// returns the multi-byte family that gives the length of every sample character in the encoding, or nullptr if
// there is none or the encoding has no multi-byte characters
template <typename F>
const multibyte_family* get_multibyte_family(F& convert) {
    std::vector<std::string> units;
    bool multi = false;
    for (const probe_char& pc : multibyte_probe_chars) {
        std::string one, two;
        std::string in(pc.utf8);
        if (!convert(in, one) || !convert(in + in, two)) {
            // the character cannot be represented in the encoding
            continue;
        }
        // a stateful encoding (ex: ISO-2022-JP) does not have one byte sequence per character
        if (one.empty() || two != one + one) {
            return nullptr;
        }
        if (one.size() > 1) {
            multi = true;
        }
        units.push_back(one);
    }
    if (!multi) {
        return nullptr;
    }
    for (const multibyte_family& f : multibyte_families) {
        bool match = true;
        for (const std::string& u : units) {
            std::string two = u + u;
            if (f.charlen(u.data(), u.size()) != static_cast<qore_offset_t>(u.size())
                    || f.charlen(two.data(), two.size()) != static_cast<qore_offset_t>(u.size())) {
                match = false;
                break;
            }
        }
        if (match) {
            return &f;
        }
    }
    return nullptr;
}

// returns true if the encoding is stateful: if a sample character that it can represent, converted twice, is not
// its conversion on its own repeated after a constant prefix (a byte order mark, or the designation that ISO-2022-KR
// writes at the start of its output); in a stateful encoding, the bytes that shift to the character set of a
// character (ISO-2022-*, the SO and SI bytes of the mixed EBCDIC code pages) or the bits of a base64 run (UTF-7) are
// shared by the characters of a run, so a character has no byte sequence of its own
template <typename F>
bool is_stateful_encoding(F& convert) {
    auto check = [&convert](const probe_char* chars, size_t num) -> bool {
        for (size_t i = 0; i < num; ++i) {
            std::string one, two;
            std::string in(chars[i].utf8);
            if (!convert(in, one) || !convert(in + in, two)) {
                // the character cannot be represented in the encoding
                continue;
            }
            if (two.size() <= one.size() || (two.size() - one.size()) > one.size()) {
                return true;
            }
            size_t width = two.size() - one.size();
            std::string unit = one.substr(one.size() - width);
            if (two != one.substr(0, one.size() - width) + unit + unit) {
                return true;
            }
        }
        return false;
    };
    return check(probe_chars, sizeof(probe_chars) / sizeof(probe_chars[0]))
        || check(multibyte_probe_chars, sizeof(multibyte_probe_chars) / sizeof(multibyte_probe_chars[0]));
}

// returns true if a sample character that the encoding can represent has more than one byte, after any constant
// prefix that iconv writes at the start of its output
template <typename F>
bool has_multibyte_units(F& convert) {
    auto check = [&convert](const probe_char* chars, size_t num) -> bool {
        for (size_t i = 0; i < num; ++i) {
            std::string one, two;
            std::string in(chars[i].utf8);
            if (!convert(in, one) || !convert(in + in, two) || two.size() <= one.size()) {
                continue;
            }
            if (two.size() - one.size() > 1) {
                return true;
            }
        }
        return false;
    };
    return check(probe_chars, sizeof(probe_chars) / sizeof(probe_chars[0]))
        || check(multibyte_probe_chars, sizeof(multibyte_probe_chars) / sizeof(multibyte_probe_chars[0]));
}
}

const QoreEncoding* qore_encoding_private::getBomEncoding(const QoreEncoding* enc, const char* p, size_t len,
        size_t& bom_len) const {
    assert(get(*enc) == this);
    if (!bom.empty() && len >= bom.size() && !memcmp(p, bom.data(), bom.size())) {
        bom_len = bom.size();
        return iconv_target_code.empty() ? enc : QoreEncodingManager::findCreate(iconv_target_code.c_str());
    }
    if (!swapped_bom.empty() && len >= swapped_bom.size() && !memcmp(p, swapped_bom.data(), swapped_bom.size())) {
        bom_len = swapped_bom.size();
        return QoreEncodingManager::findCreate(swapped_code.c_str());
    }
    return nullptr;
}

void qore_encoding_private::setCharFunctions(const form_functions& f) {
    fcharlen = f.charlen;
    get_unicode = f.get_unicode;
    flength = f.length;
    fend = f.end;
    fpos = f.pos;
    minwidth = f.minwidth;
    maxwidth = f.maxwidth;
}

/*  An encoding that Qore does not know is created from its name, and its properties are determined here by
    converting sample text to it with iconv, so that each function that depends on them (hash keys, the conversion of
    strings to numbers, string lengths and offsets, searches, line splitting, ...) handles text in the encoding
    correctly.

    - UTF-7: if iconv decodes the "-" that ends a base64 run as a character when it is the last byte of its input
      (Apple's libiconv), conversions from the encoding give such a "-" to iconv on its own; see
      IconvHelper::iconvUtf7().
    - Stateful encodings (ex: UTF-7, ISO-2022-JP, ISO-2022-KR, IBM930): if a sample character converted twice is not
      the conversion of the character on its own repeated (after a constant prefix), the bytes of a character
      depend on the characters before it (see is_stateful_encoding()).  Such an encoding is not ASCII-compatible,
      and character operations on its strings are made on their text in UTF-8 (see
      qore_string_private::isStateful()); no other property is probed.
    - ASCII compatibility: each ASCII character (TAB, LF, CR, and every printable character) is converted on its
      own; the encoding is ASCII-compatible if every one that the encoding can represent is the same single byte, and
      every letter, digit, and whitespace character can be represented.  A punctuation character that has no
      representation is accepted here, as in Shift_JIS, where iconv may map 0x5c to the yen sign; whether Shift_JIS
      is ASCII-compatible is decided by its multi-byte family below, the same way on every platform.
    - Multi-byte encodings: an ASCII-compatible encoding, or one that is not ASCII-compatible and has no Unicode
      encoding form, is checked with Japanese, Chinese, and Korean sample characters; if it has multi-byte characters
      and one of the families in multibyte_families gives the length of every sample character (EUC, double-byte
      encodings such as GBK, Big5, or CP949, GB18030, Shift_JIS), it gets the character functions of the family, and
      searches only match at character boundaries, as the second byte of a character can be an ASCII character; an
      encoding whose definition Qore knows (by the names of named_multibyte_families in any spelling: EUC-JP, EUC-KR,
      EUC-CN, GBK, GB18030, Big5 and its forms, UHC, Johab, and ISO 6937 and its forms (T.61, NAPLPS), whose
      non-spacing diacritical marks make two-byte characters with the letters after them) then gets the bytes of its
      definition, and the input of
      its conversions is checked with them (see defined_multibyte).
      Such an encoding stays ASCII-compatible, except for the Shift_JIS family (see multibyte_families).  Code points
      are decoded with iconv (QoreEncoding::getUnicode()).
    - Character width of encodings that are not ASCII-compatible: each character of a sample is converted alone and
      twice; the difference is the size of the character, and the rest of the single conversion is a constant
      prefix, which iconv writes for an encoding with a byte order mark (ex: "UTF-32").  If every character is the
      same as in one of the Unicode encoding forms UTF-32BE, UTF-32LE, UTF-16BE, or UTF-16LE, the encoding gets the
      character functions of that form: four bytes per character for UTF-32, two to four for UTF-16, and two for a
      UTF-16 form that cannot represent characters outside the Basic Multilingual Plane (UCS-2).  If iconv writes a
      byte order mark, text is converted to the encoding in the byte order of the form explicitly (ex: "UTF-32BE"),
      as for QCS_UTF16, so that strings in the encoding have no byte order mark; conversions from it are made with
      its own name, which takes the byte order from a byte order mark in the input.
    - Any other encoding (ex: single-byte EBCDIC code pages) is handled as single-byte text, ASCII-compatible or
      not as found above; text in an encoding that is not ASCII-compatible is converted to UTF-8 by every function
      that parses or splits it.

    A name with iconv conversion flags after "//" (ex: "UTF-16LE//") that iconv does not know is probed and converted
    without the flags if iconv knows that name (musl rejects every name with flags, and GNU libiconv, as on macOS with
    MacPorts, a name whose flags are empty), so that the encoding has the same
    properties on every platform; see qore_encoding_private::iconvCodeWithoutFlags().

    If iconv does not know the encoding, the properties are not changed: no text can be converted to or from the
    encoding, so its strings are handled as single-byte, ASCII-compatible text, as before, rather than raising an
    error when the encoding is named (ex: in a string tagged with an encoding only for information).
*/
void qore_encoding_private::probe() {
    // a name with conversion flags that iconv does not accept is given to iconv without them (musl; GNU libiconv for
    // empty flags)
    iconv_code = iconvCodeWithoutFlags(code, [](const std::string& name) -> bool {
        ProbeConverter to(name.c_str(), "UTF-8");
        ProbeConverter from("UTF-8", name.c_str());
        return to.valid() && from.valid();
    });
    // Select the named Shift_JIS forms directly. A probe of the platform converter can accept malformed
    // pairs or identify the extended Windows repertoire as Shift_JIS.  The name is matched as iconv implementations
    // match it, also with conversion flags (ex: "SJIS//IGNORE") (qore#6796); a spelling that the platform's iconv
    // does not know (ex: "S-JIS" on glibc) gets these character functions too, as they are those of the definition,
    // but its text cannot be converted
    const std::string name = normalize_multibyte_family_name(code);
    const bool windows_sjis = name == "CP932" || name == "WINDOWS31J" || name == "MS932" || name == "SJISWIN"
        || name == "SJISOPEN" || name == "CSWINDOWS31J";
    if (windows_sjis || name == "SHIFTJIS" || name == "SJIS" || name == "MSKANJI" || name == "CSSHIFTJIS") {
        setCharFunctions(windows_sjis ? form_functions::get<SJIS_get_char_len<true>, nullptr>(1, 2)
            : form_functions::get<SJIS_get_char_len<false>, nullptr>(1, 2));
        needs_boundary_check = true;
        defined_multibyte = true;
        ascii_compat = false;
        return;
    }
    ProbeConverter to(getIconvCode(), "UTF-8");
    bool no_encoder = !to.valid() && errno == EINVAL;
    ProbeConverter from("UTF-8", getIconvCode());
    if (!to.valid() || !from.valid()) {
        // Resource errors do not make an encoding an opaque tag.
        opaque_single_byte = no_encoder && !from.valid() && errno == EINVAL;
        return;
    }

    // UTF-7 (found by its encoding of "é"): Apple's libiconv decodes the "-" that ends a base64 run as a "-" character
    // when it is the last byte of the input of a conversion; conversions from the encoding then give it on its own
    {
        std::string enc, dec;
        if (to.convert("\xc3\xa9", enc) && enc == "+AOk-" && from.convert(enc, dec) && dec == "\xc3\xa9-") {
            utf7_final_dash_literal = true;
        }
    }

    // converts UTF-8 text to the encoding; returns false if it cannot be represented in the encoding, including when
    // iconv substitutes a character without reporting it, as Apple's libiconv does
    const bool dash_literal = utf7_final_dash_literal;
    probe_convert_t convert = [&to, &from, dash_literal](const std::string& utf8, std::string& out) -> bool {
        std::string back;
        if (!to.convert(utf8, out) || !from.convert(out, back)) {
            return false;
        }
        if (back == utf8) {
            return true;
        }
        // Apple's libiconv decodes the "-" that ends a UTF-7 base64 run at the end of its input as a character; the
        // conversions from the encoding give it on its own (see IconvHelper::iconvUtf7()), so the text is the same
        return dash_literal && !out.empty() && out.back() == '-' && back.size() == utf8.size() + 1
            && back.back() == '-' && !back.compare(0, utf8.size(), utf8);
    };

    // a stateful encoding is not ASCII-compatible, even if each ASCII character on its own is the same byte (as in
    // ISO-2022-JP), as an ASCII byte in a shifted run is part of another character; it keeps the single-byte
    // character functions, as its character operations are made on its text in UTF-8
    if (is_stateful_encoding(convert)) {
        setStateful(convert);
        return;
    }

    probeProperties(convert);

    // an encoding left without character functions whose characters have more than one byte has no functions to
    // find its characters (ex: ISO-2022-JP with musl, which shifts back to ASCII after every character, so that a
    // character converted twice is its conversion on its own repeated): it is handled as a stateful encoding
    if (!flength && has_multibyte_units(convert)) {
        setStateful(convert);
    }
}

bool qore_encoding_private::probeAsciiIdentical() const {
    bool identical = false;
    if (ascii_compat && !stateful) {
        ProbeConverter to(getIconvCode(), "UTF-8");
        ProbeConverter from("UTF-8", getIconvCode());
        if (to.valid() && from.valid()) {
            identical = true;
            std::string in, out, back;
            // every character that a string without a null byte can have
            for (int c = 1; c < 0x80 && identical; ++c) {
                in.assign(1, static_cast<char>(c));
                identical = to.convert(in, out) && out == in && from.convert(out, back) && back == in;
            }
        }
    }
    // two threads can get here for an encoding, with the same result
    ascii_identical.store(identical ? 1 : 0, std::memory_order_relaxed);
    return identical;
}

void qore_encoding_private::setStateful(const probe_convert_t& convert) {
    stateful = true;
    ascii_compat = false;
    // the constant prefix of the conversion of an ASCII character, which is in the initial shift state
    std::string one, two;
    if (convert("A", one) && convert("AA", two) && two.size() > one.size()
            && (two.size() - one.size()) <= one.size()) {
        size_t width = two.size() - one.size();
        std::string unit = one.substr(one.size() - width);
        std::string prefix = one.substr(0, one.size() - width);
        if (two == prefix + unit + unit) {
            stateful_prefix = prefix;
        }
    }
}

void qore_encoding_private::probeProperties(const probe_convert_t& convert) {
    std::string out;
    bool compat = true;
    for (int c = 1; c < 0x80 && compat; ++c) {
        bool space = (c == '\t' || c == '\n' || c == '\r' || c == ' ');
        if (!space && (c < 0x20 || c == 0x7f)) {
            continue;
        }
        std::string in(1, static_cast<char>(c));
        if (!convert(in, out)) {
            bool alnum = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
            if (alnum || space) {
                compat = false;
            }
            continue;
        }
        if (out != in) {
            compat = false;
        }
    }
    // gives a variable-width multi-byte encoding the character functions of its family; returns true if found
    auto set_multibyte = [this, &convert]() -> bool {
        const multibyte_family* f = get_multibyte_family(convert);
        if (!f) {
            return false;
        }
        // an encoding whose definition Qore knows gets the characters of its definition
        if (const multibyte_family* named = get_named_multibyte_family(code)) {
            f = named;
            defined_multibyte = true;
        }
        fcharlen = f->charlen;
        flength = f->length;
        fend = f->end;
        fpos = f->pos;
        minwidth = 1;
        maxwidth = f->maxwidth;
        needs_boundary_check = true;
        if (f->not_ascii_compat) {
            ascii_compat = false;
        }
        return true;
    };

    if (compat) {
        set_multibyte();
        return;
    }
    ascii_compat = false;

    // get the byte sequence of each sample character and any constant prefix (a byte order mark)
    // the code point and the bytes of each sample character that can be represented
    std::vector<std::pair<unsigned, std::string>> units;
    // the UTF-8 text of each sample character in units
    std::vector<std::string> units_utf8;
    std::string prefix;
    bool bmp_complete = true;
    bool non_bmp = false;
    for (const probe_char& pc : probe_chars) {
        std::string one, two;
        std::string in(pc.utf8);
        if (!convert(in, one) || !convert(in + in, two)) {
            if (pc.code > 0xffff) {
                continue;
            }
            bmp_complete = false;
            break;
        }
        if (two.size() <= one.size() || (two.size() - one.size()) > one.size()) {
            // not one byte sequence per character
            return;
        }
        size_t width = two.size() - one.size();
        std::string p = one.substr(0, one.size() - width);
        std::string unit = one.substr(one.size() - width);
        if (two != p + unit + unit || (!units.empty() && p != prefix)) {
            return;
        }
        prefix = p;
        units.emplace_back(pc.code, unit);
        units_utf8.emplace_back(pc.utf8);
        if (pc.code > 0xffff) {
            non_bmp = true;
        }
    }
    if (!bmp_complete) {
        set_multibyte();
        return;
    }

    static const UnicodeForm forms[] = {
        UnicodeForm::UTF32BE,
        UnicodeForm::UTF32LE,
        UnicodeForm::UTF16BE,
        UnicodeForm::UTF16LE,
    };
    for (UnicodeForm form : forms) {
        bool match = true;
        for (const auto& u : units) {
            if (u.second != encode_unicode(form, u.first)) {
                match = false;
                break;
            }
        }
        if (!match || (!prefix.empty() && prefix != encode_unicode(form, 0xfeff))) {
            continue;
        }

        bool utf32 = (form == UnicodeForm::UTF32BE || form == UnicodeForm::UTF32LE);
        bool be = (form == UnicodeForm::UTF32BE || form == UnicodeForm::UTF16BE);
        UnicodeForm be_form = utf32 ? UnicodeForm::UTF32BE : UnicodeForm::UTF16BE;
        UnicodeForm le_form = utf32 ? UnicodeForm::UTF32LE : UnicodeForm::UTF16LE;

        // a generic Unicode encoding (ex: "UTF-32", "UTF-16"): iconv takes the byte order of text from a byte order
        // mark in either byte order; without one, iconv implementations disagree (glibc writes and reads its native
        // byte order, while GNU libiconv and musl use big-endian), so text in the encoding without a byte order mark
        // is big-endian, as in the Unicode standard, on every platform: it is converted in both directions with the
        // explicit big-endian name, and a byte order mark in either byte order is found by Qore itself (see
        // getBomEncoding()).  An encoding whose iconv conversion writes a little-endian form without a byte order
        // mark (ex: "UCS-2" with glibc) keeps the byte order it is written in.
        // each sample character is decoded on its own after the byte order mark, as ProbeConverter converts short
        // texts only, and with a new conversion each time: glibc keeps the byte order of the first byte order mark
        // that a conversion reads, even after it is reset
        const std::string enc_code = getIconvCode();
        auto decodes_with_bom = [&enc_code, &units, &units_utf8](UnicodeForm f) -> bool {
            for (size_t i = 0; i < units.size(); ++i) {
                ProbeConverter dec("UTF-8", enc_code.c_str());
                std::string back;
                if (!dec.valid() || !dec.convert(encode_unicode(f, 0xfeff) + encode_unicode(f, units[i].first), back)
                        || back != units_utf8[i]) {
                    return false;
                }
            }
            return true;
        };
        bool generic = decodes_with_bom(be_form) && decodes_with_bom(le_form);
        if (generic && (be || !prefix.empty())) {
            const char* be_code = utf32 ? "UTF-32BE" : (non_bmp ? "UTF-16BE" : "UCS-2BE");
            ProbeConverter tc(be_code, "UTF-8");
            if (!tc.valid()) {
                return;
            }
            for (const probe_char& pc : probe_chars) {
                if (pc.code > 0xffff && !non_bmp) {
                    continue;
                }
                std::string tout;
                if (!tc.convert(pc.utf8, tout) || tout != encode_unicode(be_form, pc.code)) {
                    return;
                }
            }
            be = true;
            form = be_form;
            iconv_target_code = be_code;
            iconv_source_code = be_code;
            swapped_bom = encode_unicode(le_form, 0xfeff);
            swapped_code = utf32 ? "UTF-32LE" : (non_bmp ? "UTF-16LE" : "UCS-2LE");
        } else if (!prefix.empty()) {
            // iconv writes a byte order mark; text is converted to the encoding in its byte order explicitly
            const char* target = utf32
                ? (be ? "UTF-32BE" : "UTF-32LE")
                : (non_bmp ? (be ? "UTF-16BE" : "UTF-16LE") : (be ? "UCS-2BE" : "UCS-2LE"));
            ProbeConverter tc(target, "UTF-8");
            if (!tc.valid()) {
                return;
            }
            for (const probe_char& pc : probe_chars) {
                if (pc.code > 0xffff && !non_bmp) {
                    continue;
                }
                std::string tout;
                if (!tc.convert(pc.utf8, tout) || tout != encode_unicode(form, pc.code)) {
                    return;
                }
            }
            iconv_target_code = target;
            UnicodeForm swapped = utf32
                ? (be ? UnicodeForm::UTF32LE : UnicodeForm::UTF32BE)
                : (be ? UnicodeForm::UTF16LE : UnicodeForm::UTF16BE);
            swapped_bom = encode_unicode(swapped, 0xfeff);
            swapped_code = utf32
                ? (be ? "UTF-32LE" : "UTF-32BE")
                : (non_bmp ? (be ? "UTF-16LE" : "UTF-16BE") : (be ? "UCS-2LE" : "UCS-2BE"));
        }
        bom = encode_unicode(form, 0xfeff);

        if (utf32) {
            setCharFunctions(be ? form_functions::get<UTF32_get_char_len<UTF32BE_getUnicode>, UTF32BE_getUnicode>(4, 4)
                : form_functions::get<UTF32_get_char_len<UTF32LE_getUnicode>, UTF32LE_getUnicode>(4, 4));
            unicode_complete = non_bmp;
        } else if (non_bmp) {
            // the UTF-16 functions of the built-in UTF-16 encodings
            setCharFunctions(be
                ? form_functions{q_UTF16BE_get_char_len, UTF16BE_getUnicode, UTF16BE_getLength, UTF16BE_getByteLen,
                    UTF16BE_getCharPos, 2, 4}
                : form_functions{q_UTF16LE_get_char_len, UTF16LE_getUnicode, UTF16LE_getLength, UTF16LE_getByteLen,
                    UTF16LE_getCharPos, 2, 4});
            unicode_complete = true;
        } else {
            setCharFunctions(be ? form_functions::get<UCS2_get_char_len<UCS2BE_getUnicode>, UCS2BE_getUnicode>(2, 2)
                : form_functions::get<UCS2_get_char_len<UCS2LE_getUnicode>, UCS2LE_getUnicode>(2, 2));
        }
        // text that the character functions reject must not be converted: where iconv accepts it, text is
        // validated with the character functions before it is converted (see IconvHelper)
        iconv_accepts_invalid = iconv_accepts_invalid_form(getIconvSourceCode(), form, utf32, non_bmp);
        return;
    }
    set_multibyte();
}
