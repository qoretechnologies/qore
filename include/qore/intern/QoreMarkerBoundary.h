//--------------------------------------------------------------------*- C++ -*-
//
//  Qore Programming Language
//
//  Copyright (C) 2026 Qore Technologies, s.r.o.
//
//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the "Software"),
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.
//
//------------------------------------------------------------------------------
///
/// \file
/// \brief Finds a marker at the end of text read byte by byte only at a character boundary
///
//------------------------------------------------------------------------------
#ifndef INCLUDE_QORE_INTERN_QOREMARKERBOUNDARY_H_
#define INCLUDE_QORE_INTERN_QOREMARKERBOUNDARY_H_

#include "qore/intern/qore_encoding_private.h"

#include <cassert>
#include <cstddef>
#include <cstring>
#include <vector>

//! Finds a marker (such as an end-of-line marker) at the end of text that is read byte by byte, only where it starts
//! at a character boundary
/** A marker is a whole number of characters, and it only ends the text where its first byte is the first byte of a
    character:
    - in an encoding with characters of more than one byte (UTF-16, UTF-32), the bytes of a marker can also be the end
      of one character and the start of the next one; the marker is found only at a multiple of the code unit width
    - in a variable-width encoding whose characters can contain bytes that are other characters (Big5, Shift_JIS, GBK:
      the second byte of Shift_JIS "ソ", 83 5C, is the byte of "¥" in Shift_JIS, as of "\" in ASCII), the characters of
      the text are followed from its start to find their boundaries

    The text starts at a character boundary.  update() is called each time a byte is added; endsWith() then says if the
    text ends with a marker.  memcmp() is used, as an encoding can have null bytes in characters (UTF-16*).
*/
class QoreMarkerBoundary {
public:
    //! Creates the object for text in the given encoding
    /** @param enc the encoding of the text
        @param max_marker_size the size in bytes of the largest marker that will be looked for
    */
    DLLLOCAL QoreMarkerBoundary(const QoreEncoding* enc, size_t max_marker_size) : width(enc->getMinCharWidth()),
            max_marker_size(max_marker_size) {
        const qore_encoding_private* ep = qore_encoding_private::get(*enc);
        charlen = ep->needs_boundary_check ? ep->fcharlen : nullptr;
        // the text starts at a character boundary; nothing is allocated if the characters are not followed
        if (charlen) {
            starts.push_back(0);
        }
    }

    //! Follows the characters of the text after bytes were added
    /** @param buf the text read so far
        @param size the size of the text in bytes
    */
    DLLLOCAL void update(const char* buf, size_t size) {
        if (!charlen) {
            return;
        }
        while (char_start < size) {
            qore_offset_t l = charlen(buf + char_start, size - char_start);
            if (l < 0) {
                // an incomplete character
                break;
            }
            // an invalid byte is skipped as a character of its own
            char_start += l ? static_cast<size_t>(l) : 1;
            starts.push_back(char_start);
        }
        // only the boundaries in the last max_marker_size bytes can be the start of a marker
        size_t n = 0;
        while (n < starts.size() && starts[n] + max_marker_size < size) {
            ++n;
        }
        if (n) {
            starts.erase(starts.begin(), starts.begin() + n);
        }
    }

    //! Returns true if the text ends with the given marker, starting at a character boundary
    /** @param buf the text read so far
        @param size the size of the text in bytes
        @param marker the marker, in the encoding of the text
        @param marker_size the size of the marker in bytes; at most the max_marker_size given to the constructor
    */
    DLLLOCAL bool endsWith(const char* buf, size_t size, const char* marker, size_t marker_size) const {
        assert(marker_size <= max_marker_size);
        if (!marker_size || size < marker_size || memcmp(buf + size - marker_size, marker, marker_size)) {
            return false;
        }
        const size_t start = size - marker_size;
        if (!charlen) {
            return !(start % width);
        }
        // the text ends at a character boundary, and the marker starts at one
        if (char_start != size) {
            return false;
        }
        for (size_t s : starts) {
            if (s == start) {
                return true;
            }
            if (s > start) {
                break;
            }
        }
        return false;
    }

private:
    //! the width of the smallest character
    size_t width;
    //! the size in bytes of the largest marker
    size_t max_marker_size;
    //! the character length function of the encoding if its characters have to be followed, otherwise nullptr
    mbcs_charlen_t charlen;
    //! the byte offset of the start of the next character, if charlen is set
    size_t char_start = 0;
    //! the offsets of the character boundaries in the last max_marker_size bytes (at most max_marker_size + 1), if
    //! charlen is set
    std::vector<size_t> starts;
};

#endif // INCLUDE_QORE_INTERN_QOREMARKERBOUNDARY_H_
