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
static unsigned UTF32LE_getUnicode(const char* p);
static unsigned UCS2BE_getUnicode(const char* p);
static unsigned UCS2LE_getUnicode(const char* p);

encoding_map_t QoreEncodingManager::emap;
const_encoding_map_t QoreEncodingManager::amap;
QoreThreadLock QoreEncodingManager::mutex;
QoreEncodingManager QEM;

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
        size_t len = (end - p);
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

int QoreEncoding::getUnicode(const char* p, const char* end, unsigned& clen, ExceptionSink* xsink) const {
    // get character length & check validity
    clen = (unsigned)getByteLen(p, end, 1, xsink);
    if (*xsink)
        return -1;

    if (!priv->get_unicode) {
        assert(this != QCS_UTF8);
        if (priv->ascii_compat && static_cast<unsigned char>(*p) < 128) {
            return *p;
        }

        // the character is converted with iconv; in an encoding that is not ASCII-compatible (ex: EBCDIC), an ASCII
        // byte is not the ASCII character
        QoreString tmp(QCS_UTF8);
        if (qore_string_private::convert_encoding_intern(p, clen, this, tmp, QCS_UTF8, xsink)) {
            return -1;
        }
        if (tmp.empty()) {
            xsink->raiseException("INVALID-ENCODING", "invalid %s encoding encountered in string", priv->code.c_str());
            return -1;
        }

        return UTF8_getUnicode(tmp.c_str());
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

    QCS_WINDOWS_936 = addUnlocked("WINDOWS-936", "Windows 936: Simplified Chinese");
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

void QoreEncodingManager::init(const char* def) {
    // now set default character set
    if (def) {
        // Validate encoding when explicitly provided on command line
        if (!validateEncoding(def)) {
            fprintf(stderr, "qore: error: invalid character encoding '%s'\n", def);
            fprintf(stderr, "Use 'qore --show-charsets' to list known encodings\n");
            exit(1);
        }
        QCS_DEFAULT = findCreate(def);
    } else {
        // first see if QORE_CHARSET exists
        char* estr = getenv("QORE_CHARSET");
        if (estr) {
            if (validateEncoding(estr)) {
                QCS_DEFAULT = findCreate(estr);
            } else {
                fprintf(stderr, "qore: warning: invalid encoding '%s' in QORE_CHARSET, using UTF-8\n", estr);
                QCS_DEFAULT = QCS_UTF8;
            }
        } else { // try to get character set name from LANG variable
            estr = getenv("LANG");
            char* p;
            if (estr && ((p = strrchr(estr, '.')))) {
                char* o = strchr(p + 1, '@');
                const char* enc_name;
                if (!o) {
                    enc_name = p + 1;
                } else {
                    *o = '\0';
                    enc_name = p + 1;
                }
                if (validateEncoding(enc_name)) {
                    QCS_DEFAULT = findCreate(enc_name);
                } else {
                    fprintf(stderr, "qore: warning: invalid encoding '%s' from LANG, using UTF-8\n", enc_name);
                    QCS_DEFAULT = QCS_UTF8;
                }
                if (o) {
                    *o = '@';
                }
            } else // otherwise set QCS_DEFAULT to UTF-8
                QCS_DEFAULT = QCS_UTF8;
        }
    }
}

void QoreEncodingManager::addAlias(const QoreEncoding* qcs, const char* alias) {
    mutex.lock();
    amap[alias] = qcs;
    mutex.unlock();
}

const QoreEncoding* QoreEncodingManager::findUnlocked(const char* name) {
    {
        encoding_map_t::const_iterator i = emap.find(name);
        if (i != emap.end())
            return i->second;
    }

    const_encoding_map_t::const_iterator i = amap.find(name);
    if (i != amap.end())
        return i->second;

    return 0;
}

const QoreEncoding* QoreEncodingManager::findCreate(const char* name) {
    AutoLocker al(mutex);
    const QoreEncoding* rv = findUnlocked(name);
    if (rv) {
        return rv;
    }
    // an encoding that is not known is created with the properties of the encoding itself
    std::unique_ptr<QoreEncoding> qcs(new QoreEncoding(name));
    qore_encoding_private::get(*qcs)->probe();
    emap[qcs->getCode()] = qcs.get();
    return qcs.release();
}

const QoreEncoding* QoreEncodingManager::find(const char* name) {
    AutoLocker al(mutex);
    return findUnlocked(name);
}

const QoreEncoding* QoreEncodingManager::findCreate(const QoreString* str) {
   return findCreate(str->getBuffer());
}

qore_offset_t q_UTF8_get_char_len(const char* p, size_t len) {
    // see if a multi-byte char is starting
    if ((*p & 0xc0) == 0xc0) {
        //printd(5, "MULTIBYTE *p = %hhx\n", *p);
        // check for a 3-byte sequence
        if ((*p) & 0x20) {
            // check for a 4-byte sequence
            if ((*p) & 0x10) {
                if (len >= 4) {
                    if ((*(p+1)) & 0x80 && (*(p+2)) & 0x80 && (*(p+3)) & 0x80)
                        return 4;
                    // encoding error - invalid UTF-8 character
                    return 0;
                }
                return -4;
            } else { // should be 3-byte sequence
                if (len >= 3) {
                    if ((*(p+1)) & 0x80 && (*(p+2)) & 0x80)
                        return 3;
                    // encoding error - invalid UTF-8 character
                    return 0;
                }
                return -3;
            }
        } else { // should be a 2-byte sequence - check next char for high bit
            if (len >= 2) {
                if ((*(p+1)) & 0x80)
                   return 2;
                // encoding error - invalid UTF-8 character
                return 0;
            }
            return -2;
        }
    }
    return 1;
}

static size_t UTF8_getLength(const char* p, const char* end, bool& invalid) {
    constexpr size_t ascii_high_bit_mask = (~static_cast<size_t>(0) / 0xff) * 0x80;
    size_t i = 0;
    while (p < end) {
        unsigned char c = static_cast<unsigned char>(*p);
        if (c < 0x80) {
            ++p;
            ++i;
            while (static_cast<size_t>(end - p) >= sizeof(size_t)) {
                size_t word;
                memcpy(&word, p, sizeof(word));
                if (word & ascii_high_bit_mask) {
                    break;
                }
                p += sizeof(size_t);
                i += sizeof(size_t);
            }
            continue;
        }

        qore_offset_t l = q_UTF8_get_char_len(p, end - p);
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

static size_t UTF8_getByteLen(const char* p, const char* end, size_t l, bool& invalid) {
    size_t b = 0;
    while ((p < end) && l) {
        qore_offset_t bl = q_UTF8_get_char_len(p, end - p);
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
        qore_offset_t l = q_UTF8_get_char_len(p, end - p);
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

qore_offset_t q_UTF16LE_get_char_len(const char* p, size_t len) {
    assert(len);
    if (len == 1)
        return -2;

    unsigned char c = (unsigned char)p[1];
    size_t l = c >= 0xd8 && c < 0xdc ? 4 : 2;
    return len >= l ? l : -l;
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
        qore_offset_t l = q_UTF16LE_get_char_len(p, end - p);
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
        qore_offset_t bl = q_UTF16LE_get_char_len(p, end - p);
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
        qore_offset_t l = q_UTF16LE_get_char_len(p, end - p);
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

qore_offset_t q_UTF16BE_get_char_len(const char* p, size_t len) {
    assert(len);

    unsigned char c = (unsigned char)*p;
    size_t l = c >= 0xd8 && c < 0xdc ? 4 : 2;
    return len >= l ? l : -l;
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
        qore_offset_t l = q_UTF16BE_get_char_len(p, end - p);
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
        qore_offset_t bl = q_UTF16BE_get_char_len(p, end - p);
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
        qore_offset_t l = q_UTF16BE_get_char_len(p, end - p);
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
    DLLLOCAL ProbeConverter(const char* to, const char* from) : cd(iconv_open(to, from)) {
    }

    DLLLOCAL ~ProbeConverter() {
        if (valid()) {
            iconv_close(cd);
        }
    }

    DLLLOCAL bool valid() const {
        // iconv_open() returns (iconv_t)-1 for an error
        return cd != reinterpret_cast<iconv_t>(static_cast<intptr_t>(-1));
    }

    // converts a short text with a new conversion state, including the bytes that return the output to the initial
    // shift state; returns false if the text cannot be converted or a character is converted non-reversibly
    DLLLOCAL bool convert(const std::string& in, std::string& out) {
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
    DLLLOCAL static size_t iconv_adapter(size_t (*iconv_f)(iconv_t, T, size_t*, char**, size_t*), iconv_t handle,
            char** inbuf, size_t* inavail, char** outbuf, size_t* outavail) {
        return (*iconv_f)(handle, const_cast<T>(inbuf), inavail, outbuf, outavail);
    }

    ProbeConverter(const ProbeConverter&) = delete;
    ProbeConverter& operator=(const ProbeConverter&) = delete;
};

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

/*  An encoding that Qore does not know is created from its name, and its properties are determined here by
    converting sample text to it with iconv, so that each function that depends on them (hash keys, the conversion of
    strings to numbers, string lengths and offsets, line splitting, ...) handles text in the encoding correctly.

    - ASCII compatibility: each ASCII character (TAB, LF, CR, and every printable character) is converted on its
      own; the encoding is ASCII-compatible if every one that the encoding can represent is the same single byte, and
      every letter, digit, and whitespace character can be represented.  A punctuation character that has no
      representation is accepted, so that encodings such as Shift_JIS, where iconv may map 0x5c to the yen sign,
      keep being handled as ASCII-compatible.  An ASCII-compatible encoding is handled as single-byte text, as before,
      also when it has multi-byte characters (ex: EUC-JP or GB18030): Qore has no character functions for them.
    - Character width: each character of a sample is converted alone and twice; the difference is the size of the
      character, and the rest of the single conversion is a constant prefix, which iconv writes for an encoding with
      a byte order mark (ex: "UTF-32").  If every character is the same as in one of the Unicode encoding forms
      UTF-32BE, UTF-32LE, UTF-16BE, or UTF-16LE, the encoding gets the character functions of that form: four bytes
      per character for UTF-32, two to four for UTF-16, and two for a UTF-16 form that cannot represent characters
      outside the Basic Multilingual Plane (UCS-2).  If iconv writes a byte order mark, text is converted to the
      encoding in the byte order of the form explicitly (ex: "UTF-32BE"), as for QCS_UTF16, so that strings in the
      encoding have no byte order mark; conversions from it are made with its own name, which takes the byte order
      from a byte order mark in the input.
    - Any other encoding that is not ASCII-compatible (ex: EBCDIC code pages, UTF-7, ISO-2022 encodings) is handled
      as single-byte text that is not ASCII-compatible: every function that parses or splits text converts it to
      UTF-8 first, and character offsets are byte offsets, as before.  Variable-width and stateful encodings cannot
      be decoded one character at a time without functions specific to them.

    If iconv does not know the encoding, the properties are not changed: no text can be converted to or from the
    encoding, so its strings are handled as single-byte, ASCII-compatible text, as before, rather than raising an
    error when the encoding is named (ex: in a string tagged with an encoding only for information).
*/
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

void qore_encoding_private::probe() {
    ProbeConverter to(code.c_str(), "UTF-8");
    ProbeConverter from("UTF-8", code.c_str());
    if (!to.valid() || !from.valid()) {
        return;
    }

    // converts UTF-8 text to the encoding; returns false if it cannot be represented in the encoding, including when
    // iconv substitutes a character without reporting it, as Apple's libiconv does
    auto convert = [&to, &from](const std::string& utf8, std::string& out) -> bool {
        std::string back;
        return to.convert(utf8, out) && from.convert(out, back) && back == utf8;
    };

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
    if (compat) {
        return;
    }
    ascii_compat = false;

    // get the byte sequence of each sample character and any constant prefix (a byte order mark)
    // the code point and the bytes of each sample character that can be represented
    std::vector<std::pair<unsigned, std::string>> units;
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
        if (pc.code > 0xffff) {
            non_bmp = true;
        }
    }
    if (!bmp_complete) {
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
        if (!prefix.empty()) {
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
        return;
    }
}
