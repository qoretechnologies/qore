/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreEncoding.h

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

#ifndef _QORE_ENCODING_H

#define _QORE_ENCODING_H

/** @file QoreEncoding.h
    provides definitions related to character encoding support in Qore including the QoreEncoding class
    and QCS_DEFAULT, the default encoding for the Qore library
 */

#include <qore/common.h>
#include <qore/QoreThreadLock.h>

#include <cstring>
#include <map>
#include <string>
#include <strings.h>

//! for multi-byte character set encodings: gives the length of the string in characters
typedef size_t (*mbcs_length_t)(const char* str, const char* end, bool &invalid);

//! for multi-byte character set encodings: gives the number of bytes for the number of chars
typedef size_t (*mbcs_end_t)(const char* str, const char* end, size_t num_chars, bool &invalid);

//! for multi-byte character set encodings: gives the character position of the ptr
typedef size_t (*mbcs_pos_t)(const char* str, const char* ptr, bool &invalid);

//! for multi-byte encodings: gives the number of total bytes for the character given one or more characters
/** @param str a pointer to the character data to check
    @param valid_len the number of valid bytes at the start of the character pointer
    @return 0=invalid, positive = number of characters needed, negative numbers = number of additional bytes needed to perform the check
 */
typedef qore_offset_t (*mbcs_charlen_t)(const char* str, size_t valid_len);

//! returns the unicode code point for the given character, assumes there is enough data for the character and that the character is valid (must be checked before calling)
typedef unsigned (*mbcs_get_unicode_t)(const char* p);

// private implementation of the QoreEncoding class
struct qore_encoding_private;

//! defines string encoding functions in Qore
/** for performance reasons this is not a class hierarchy with virtual methods;
    this ugly implementation with function pointers is much faster.
    Only encodings where a single character can be more than 1 byte needs to
    have functions implemented.

    @note only encodings that are backwards compatible with ASCII are supported by Qore; currently the only multi-byte encoding completely supported by qore is UTF-8 (UTF-16* encodings are not properly supported yet)

    @note the default encoding is represented by QCS_DEFAULT; unless another encoding is explicitly given, all strings will be tagged with QCS_DEFAULT

    @see QCS_DEFAULT
*/
class QoreEncoding {
    friend struct qore_encoding_private;

protected:
    qore_encoding_private* priv;

public:
    DLLLOCAL QoreEncoding(const char* code, const char* desc = nullptr, unsigned char minwidth = 1,
            unsigned char maxwidth = 1, mbcs_length_t l = nullptr, mbcs_end_t e = nullptr, mbcs_pos_t p = nullptr,
            mbcs_charlen_t c = nullptr, mbcs_get_unicode_t gu = nullptr, bool ascii_compat = true);

    DLLLOCAL ~QoreEncoding();

    //! gives the length of the string in characters
    /** @param p a pointer to the character data
        @param end a pointer to the next byte after the end of the character data
        @param invalid if true after executing the function, invalid input was given and the return value should be ignored

        @return the number of characters in the string
    */
    DLLEXPORT size_t getLength(const char* p, const char* end, bool& invalid) const;

    //! gives the length of the string in characters
    /** @param p a pointer to the character data
        @param end a pointer to the next byte after the end of the character data
        @param xsink Qore-language exceptions will be raised using this argument

        @return the number of characters in the string
    */
    DLLEXPORT size_t getLength(const char* p, const char* end, ExceptionSink* xsink) const;

    //! gives the number of bytes for the number of chars in the string or up to the end of the string
    /** @param p a pointer to the character data
        @param end a pointer to the next byte after the end of the character data
        @param c the number of characters to check
        @param invalid if true after executing the function, invalid input was given and the return value should be ignored

        @return the number of bytes for the given number of characters in the string or up to the end of the string
    */
    DLLEXPORT size_t getByteLen(const char* p, const char* end, size_t c, bool& invalid) const;

    //! gives the number of bytes for the number of chars in the string or up to the end of the string
    /** @param p a pointer to the character data
        @param end a pointer to the next byte after the end of the character data
        @param c the number of characters to check
        @param xsink Qore-language exceptions will be raised using this argument

        @return the number of bytes for the given number of characters in the string or up to the end of the string
    */
    DLLEXPORT size_t getByteLen(const char* p, const char* end, size_t c, ExceptionSink* xsink) const;

    //! gives the character position (number of characters) starting from the first pointer to the second
    /** @param p a pointer to the character data
        @param end a pointer to the next byte after the end of the character data
        @param invalid if true after executing the function, invalid input was given and the return value should be ignored

        @return the number of bytes for the given number of characters in the string
    */
    DLLEXPORT size_t getCharPos(const char* p, const char* end, bool& invalid) const;

    //! gives the character position (number of characters) starting from the first pointer to the second
    /** @param p a pointer to the character data
        @param end a pointer to the next byte after the end of the character data
        @param xsink Qore-language exceptions will be raised using this argument

        @return the number of bytes for the given number of characters in the string
    */
    DLLEXPORT size_t getCharPos(const char* p, const char* end, ExceptionSink* xsink) const;

    //! gives the number of total bytes for the next character at the given pointer
    /** always returns 1 for single-byte encodings
        @param p a pointer to the character data to check
        @param valid_len the number of valid bytes at the start of the character pointer

        @return 0=invalid, positive = number of bytes needed to represent the character (and all are present), negative numbers = number of additional bytes needed to perform the check
    */
    DLLEXPORT qore_offset_t getCharLen(const char* p, size_t valid_len) const;

    //! returns true if the encoding is a multi-byte encoding
    DLLEXPORT bool isMultiByte() const;

    //! returns the string code (ex: "UTF-8") for the encoding
    DLLEXPORT const char* getCode() const;

    //! returns the description for the encoding
    DLLEXPORT const char* getDesc() const;

    //! returns the maximum character width in bytes for the encoding
    DLLEXPORT int getMaxCharWidth() const;

    //! returns the minimum character width in bytes for the encoding
    /** @since Qore 0.8.12
    */
    DLLEXPORT unsigned getMinCharWidth() const;

    //! returns true if the character encoding is backwards-compatible with ASCII
    /** @since Qore 0.8.12
    */
    DLLEXPORT bool isAsciiCompat() const;

    //! returns the unicode code point for the given character; if there are any errors (invalid character, not enough space in the string, etc), a Qore-language exception is thrown
    /** @param p a pointer to a character
        @param end a pointer to the next byte after the end of the character data
        @param clen the length of the character
        @param xsink Qore-language exceptions will be raised using this argument

        @return the unicode code point for the character or -1 in case of an error

        @since Qore 0.8.13
    */
    DLLEXPORT int getUnicode(const char* p, const char* end, unsigned& clen, ExceptionSink* xsink) const;
};

// case-insensitive maps for encodings
typedef std::map<const char*, QoreEncoding*, ltcstrcase> encoding_map_t;
typedef std::map<const char*, const QoreEncoding*, ltcstrcase> const_encoding_map_t;

class QoreString;

//! manages encodings in Qore
/** there will always only be one of these, therefore all members and methods are static
 */
class QoreEncodingManager {
private:
   DLLLOCAL static encoding_map_t emap;
   DLLLOCAL static const_encoding_map_t amap;
   DLLLOCAL static QoreThreadLock mutex;

   DLLLOCAL static const QoreEncoding* addUnlocked(const char* n_code, const char* n_desc = 0, unsigned char n_minwidth = 1, unsigned char n_maxwidth = 1, mbcs_length_t l = 0, mbcs_end_t e = 0, mbcs_pos_t p = 0, mbcs_charlen_t c = 0,  mbcs_get_unicode_t gu = 0, bool n_ascii_compat = true);
   DLLLOCAL static const QoreEncoding* findUnlocked(const char* name);

public:
   //! adds an alias for an encoding
   DLLEXPORT static void addAlias(const QoreEncoding* qcs, const char* alias);

   //! finds an encoding if it exists (also looks up against alias names) and creates a new one if it doesn't
   DLLEXPORT static const QoreEncoding* findCreate(const char* name);

   //! finds an encoding if it exists (also looks up against alias names) and creates a new one if it doesn't
   DLLEXPORT static const QoreEncoding* findCreate(const QoreString* str);

   //! prints out all valid encodings to stdout
   DLLEXPORT static void showEncodings();

   //! prints out all aliases to stdout
   DLLEXPORT static void showAliases();

   //! adds a new encoding to the list
   DLLEXPORT static const QoreEncoding* add(const char* code, const char* desc = 0, unsigned char maxwidth = 1, mbcs_length_t l = 0, mbcs_end_t e = 0, mbcs_pos_t p = 0, mbcs_charlen_t = 0);

   DLLLOCAL static void init(const char* def);
   DLLLOCAL QoreEncodingManager();
   DLLLOCAL ~QoreEncodingManager();
};

DLLEXPORT size_t q_get_byte_len(const QoreEncoding* enc, const char* p, const char* end, size_t c, ExceptionSink* xsink);
DLLEXPORT qore_offset_t q_get_char_len(const QoreEncoding* enc, const char* p, size_t valid_len, ExceptionSink* xsink);

//! the QoreEncodingManager object
DLLEXPORT extern QoreEncodingManager QEM;

// builtin character encodings
DLLEXPORT extern const QoreEncoding* QCS_DEFAULT, //!< the default encoding for the Qore library
   *QCS_USASCII,                                  //!< ascii encoding
   *QCS_UTF8,                                     //!< UTF-8 multi-byte encoding (only UTF-8 and UTF-16 are multi-byte encodings)
   *QCS_UTF16,                                    //!< UTF-16 (only UTF-* are multi-byte encodings)
   *QCS_UTF16BE,                                  //!< UTF-16BE (only UTF-* are multi-byte encodings)
   *QCS_UTF16LE,                                  //!< UTF-16LE (only UTF-* are multi-byte encodings)
   *QCS_ISO_8859_1,                               //!< latin-1, Western European encoding
   *QCS_ISO_8859_2,                               //!< latin-2, Central European encoding
   *QCS_ISO_8859_3,                               //!< latin-3, Southern European character set
   *QCS_ISO_8859_4,                               //!< latin-4, Northern European character set
   *QCS_ISO_8859_5,                               //!< Cyrillic character set
   *QCS_ISO_8859_6,                               //!< Arabic character set
   *QCS_ISO_8859_7,                               //!< Greek character set
   *QCS_ISO_8859_8,                               //!< Hebrew character set
   *QCS_ISO_8859_9,                               //!< latin-5, Turkish character set
   *QCS_ISO_8859_10,                              //!< latin-6, Nordic character set
   *QCS_ISO_8859_11,                              //!< Thai character set
   *QCS_ISO_8859_13,                              //!< latin-7, Baltic rim character set
   *QCS_ISO_8859_14,                              //!< latin-8, Celtic character set
   *QCS_ISO_8859_15,                              //!< latin-9, Western European with euro symbol
   *QCS_ISO_8859_16,                              //!< latin-10, Southeast European character set
   *QCS_KOI8_R,                                   //!< Russian: Kod Obmena Informatsiey, 8 bit
   *QCS_KOI8_U,                                   //!< Ukrainian: Kod Obmena Informatsiey, 8 bit
   *QCS_KOI7,                                     //!< Russian: Kod Obmena Informatsiey, 7 bit characters
   *QCS_WINDOWS_874,                              //!< Windows 874: Latin/Thai - similar to ISO-8859-11
   *QCS_WINDOWS_936,                              //!< Windows 936: Simplified Chinese
   *QCS_WINDOWS_1250,                             //!< Windows 1250: Central/Eastern European
   *QCS_WINDOWS_1251,                             //!< Windows 1251: Cyrillic: Russian, Ukrainian, Balarusian, Bulgarian, Serbian Cyrillic, Macedonian, ...
   *QCS_WINDOWS_1252,                             //!< Windows 1252: European: Spanish, French, German
   *QCS_WINDOWS_1253,                             //!< Windows 1253: Greek
   *QCS_WINDOWS_1254,                             //!< Windows 1254: Turkish
   *QCS_WINDOWS_1255,                             //!< Windows 1255: Hebrew
   *QCS_WINDOWS_1256,                             //!< Windows 1256: Arabic
   *QCS_WINDOWS_1257,                             //!< Windows 1257: Baltic
   *QCS_WINDOWS_1258;                             //!< Windows 1258: Vietnamese

#endif // _QORE_ENCODING_H
