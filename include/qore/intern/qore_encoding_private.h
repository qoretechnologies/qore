/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_encoding_private.h

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

#ifndef _QORE_INTERN_QORE_ENCODING_PRIVATE_H
#define _QORE_INTERN_QORE_ENCODING_PRIVATE_H

#include <functional>
#include <string>

struct form_functions;

struct qore_encoding_private {
    std::string code;
    std::string desc;

    unsigned char minwidth;
    unsigned char maxwidth;

    mbcs_length_t flength;
    mbcs_end_t fend;
    mbcs_pos_t fpos;
    mbcs_charlen_t fcharlen;

    mbcs_get_unicode_t get_unicode;
    bool ascii_compat;

    //! the name passed to iconv to convert text to this encoding; empty if it is the code of the encoding
    /** Set for an encoding created on the fly whose iconv conversion writes a byte order mark at the start of the
        output (ex: \c "UTF-32"); the character functions of the encoding decode text in the byte order of the
        characters that iconv writes, without a byte order mark, so text converted to the encoding is requested in
        that byte order explicitly (ex: \c "UTF-32BE"), as for \c QCS_UTF16
    */
    std::string iconv_target_code;

    //! the name passed to iconv to convert text from this encoding; empty if it is the code of the encoding
    /** Set for a generic Unicode encoding created on the fly (ex: \c "UTF-32"), whose text without a byte order mark
        is big-endian on every platform: it is converted from with the explicit big-endian name (ex: \c "UTF-32BE"),
        as iconv implementations disagree on the byte order of such text; a byte order mark at the start of the text
        is found with getBomEncoding() before the conversion
    */
    std::string iconv_source_code;

    //! true if every Unicode character can be represented in the encoding (UTF-8, UTF-16*, UTF-32*)
    bool unicode_complete = false;

    //! the byte order mark in the byte order of an encoding created on the fly with the characters of a Unicode form
    /** Empty for any other encoding; the byte order mark of the built-in UTF-16 encodings is handled with them
    */
    std::string bom;

    //! the byte order mark in the other byte order, for an encoding created on the fly with a byte order mark
    /** Set when \c iconv_target_code is set (ex: for \c "UTF-32"): text in the encoding can start with a byte order
        mark in either byte order, which gives the byte order of the text
    */
    std::string swapped_bom;

    //! the encoding in the other byte order, if \c swapped_bom is set (ex: \c "UTF-32LE")
    std::string swapped_code;

    //! true for a variable-width encoding whose multi-byte characters can contain bytes that are other characters
    /** (ex: Shift_JIS, GBK, Big5, EUC-JP); a byte-oriented search must check that a match starts at a character
        boundary; see qore_char_boundary
    */
    bool needs_boundary_check = false;

    //! true if iconv decodes the "-" that ends a UTF-7 base64 run as a character at the end of its input
    /** (Apple's libiconv: "+AOk-" gives "é-"); see IconvHelper::iconvUtf7()
    */
    bool utf7_final_dash_literal = false;

    //! true for a stateful encoding: one whose bytes for a character depend on the characters before it
    /** (ex: UTF-7, ISO-2022-JP, ISO-2022-KR, the mixed EBCDIC code pages such as IBM930); a character has no byte
        sequence of its own, as shift sequences switch between character sets, so text in the encoding cannot be
        decoded, cut, or searched one byte or character at a time.  Character operations on a string in such an
        encoding are made on its text in UTF-8, and text results are converted back to the encoding; see
        qore_string_private::isStateful().  Such an encoding is not ASCII-compatible.
    */
    bool stateful = false;

    //! the bytes that iconv writes at the start of text converted to a stateful encoding, if any
    /** (ex: the designation of the character set that glibc writes at the start of ISO-2022-KR text); a character
        or a separator converted on its own has them, but they are not in the middle of text in the encoding, so they
        are removed from a sequence that is searched for in text (an end-of-line marker); see
        qore_string_private::removeStatefulPrefix()
    */
    std::string stateful_prefix;

    DLLLOCAL qore_encoding_private(const char* code, const char* desc = nullptr, unsigned char minwidth = 1,
            unsigned char maxwidth = 1, mbcs_length_t flength = nullptr, mbcs_end_t fend = nullptr,
            mbcs_pos_t fpos = nullptr, mbcs_charlen_t fcharlen = nullptr, mbcs_get_unicode_t get_unicode = nullptr,
            bool ascii_compat = true)
            : code(code), desc(desc ? desc : ""), minwidth(minwidth), maxwidth(maxwidth), flength(flength), fend(fend),
            fpos(fpos), fcharlen(fcharlen), get_unicode(get_unicode), ascii_compat(ascii_compat) {
    }

    DLLLOCAL unsigned getMinCharWidth() const {
        return minwidth;
    }

    DLLLOCAL bool isAsciiCompat() const {
        return ascii_compat;
    }

    //! returns true if the encoding is stateful; see \c stateful
    DLLLOCAL bool isStateful() const {
        return stateful;
    }

    //! returns true if the given encoding is stateful; see \c stateful
    DLLLOCAL static bool isStateful(const QoreEncoding* enc) {
        return enc->priv->stateful;
    }

    //! returns the name to pass to iconv to convert text to this encoding
    DLLLOCAL const char* getIconvTargetCode() const {
        return iconv_target_code.empty() ? code.c_str() : iconv_target_code.c_str();
    }

    //! returns the name to pass to iconv to convert text from this encoding
    DLLLOCAL const char* getIconvSourceCode() const {
        return iconv_source_code.empty() ? code.c_str() : iconv_source_code.c_str();
    }

    //! converts UTF-8 text to the encoding when it is probed; returns false if the text cannot be represented in it
    typedef std::function<bool(const std::string&, std::string&)> probe_convert_t;

    //! sets the properties of an encoding created on the fly from its name from the encoding itself
    /** The properties are determined by converting sample text to the encoding with iconv; see
        lib/charset.cpp.  If iconv does not know the encoding, the properties are not changed: text in the encoding
        cannot be converted, so it is handled as single-byte, ASCII-compatible text, as before.
    */
    DLLLOCAL void probe();

    //! sets the properties of an encoding that is not stateful; see probe()
    DLLLOCAL void probeProperties(const probe_convert_t& convert);

    //! marks the encoding as stateful; see probe()
    DLLLOCAL void setStateful(const probe_convert_t& convert);

    //! returns the encoding given by a byte order mark at the start of text in an encoding created on the fly
    /** @param enc the encoding of the text; this object must be its private implementation
        @param p the text
        @param len the byte length of the text
        @param bom_len set to the byte length of the byte order mark found

        @return the encoding of the text after the byte order mark: the encoding itself, or for an encoding with a
        byte order mark (ex: \c "UTF-32"), the encoding in the byte order of the mark (ex: \c "UTF-32BE" or
        \c "UTF-32LE"); nullptr if the text does not start with a byte order mark of the encoding
    */
    DLLLOCAL const QoreEncoding* getBomEncoding(const QoreEncoding* enc, const char* p, size_t len,
            size_t& bom_len) const;

    //! sets the character functions and widths of the encoding
    DLLLOCAL void setCharFunctions(const form_functions& f);

    DLLLOCAL unsigned getUnicode(const char* p) const {
        assert(get_unicode);
        return get_unicode(p);
    }

    DLLLOCAL static qore_encoding_private* get(QoreEncoding& enc) {
        return enc.priv;
    }

    DLLLOCAL static const qore_encoding_private* get(const QoreEncoding& enc) {
        return enc.priv;
    }
};

#endif
