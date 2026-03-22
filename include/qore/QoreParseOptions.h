/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreParseOptions.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_QOREPARSEOPTIONS_H
#define _QORE_QOREPARSEOPTIONS_H

/** @file QoreParseOptions.h
    Defines the QoreParseOptions class providing opaque 128-bit parse option storage.

    This class replaces raw int64 bitfields for parse options, allowing the option space
    to grow beyond 64 bits. Implicit construction from int64 ensures backward compatibility.
*/

#include <qore/common.h>

//! Opaque parse option container supporting extended options beyond 64 bits
/** This class encapsulates 128-bit storage for parse options, providing bitwise operators
    and query methods. An implicit constructor from int64 ensures backward compatibility
    with existing PO_* constants.

    @note There is intentionally no operator int64() to prevent silent loss of extended bits.

    @since %Qore 2.1
*/
class QoreParseOptions {
public:
    //! creates an empty parse options object (no options set)
    constexpr QoreParseOptions() : lo(0), hi(0) {
    }

    //! creates a parse options object from a 64-bit integer (implicit conversion)
    /** @param po the 64-bit parse option value (bits 0-63)
    */
    constexpr QoreParseOptions(int64 po) : lo(po), hi(0) {
    }

    //! creates a parse options object from two 64-bit integers
    /** @param lo bits 0-63
        @param hi bits 64-127
    */
    constexpr QoreParseOptions(int64 lo, int64 hi) : lo(lo), hi(hi) {
    }

    //! returns the bitwise OR of two parse option objects
    constexpr QoreParseOptions operator|(const QoreParseOptions& o) const {
        return QoreParseOptions(lo | o.lo, hi | o.hi);
    }

    //! returns the bitwise AND of two parse option objects
    constexpr QoreParseOptions operator&(const QoreParseOptions& o) const {
        return QoreParseOptions(lo & o.lo, hi & o.hi);
    }

    //! returns the bitwise NOT of the parse option object
    constexpr QoreParseOptions operator~() const {
        return QoreParseOptions(~lo, ~hi);
    }

    //! returns the bitwise XOR of two parse option objects
    constexpr QoreParseOptions operator^(const QoreParseOptions& o) const {
        return QoreParseOptions(lo ^ o.lo, hi ^ o.hi);
    }

    //! sets bits from another parse option object (bitwise OR assignment)
    QoreParseOptions& operator|=(const QoreParseOptions& o) {
        lo |= o.lo;
        hi |= o.hi;
        return *this;
    }

    //! clears bits using another parse option object (bitwise AND assignment)
    QoreParseOptions& operator&=(const QoreParseOptions& o) {
        lo &= o.lo;
        hi &= o.hi;
        return *this;
    }

    //! sets bits from another parse option object (bitwise XOR assignment)
    QoreParseOptions& operator^=(const QoreParseOptions& o) {
        lo ^= o.lo;
        hi ^= o.hi;
        return *this;
    }

    //! returns true if both parse option objects are equal
    constexpr bool operator==(const QoreParseOptions& o) const {
        return lo == o.lo && hi == o.hi;
    }

    //! returns true if the parse option objects are not equal
    constexpr bool operator!=(const QoreParseOptions& o) const {
        return lo != o.lo || hi != o.hi;
    }

    //! lexicographic less-than comparison (for use as map key)
    constexpr bool operator<(const QoreParseOptions& o) const {
        return hi < o.hi || (hi == o.hi && lo < o.lo);
    }

    //! returns true if any bit is set
    constexpr explicit operator bool() const {
        return lo != 0 || hi != 0;
    }

    //! returns true if all bits in the mask are set in this object
    /** @param mask the bits to check
        @return true if (this & mask) == mask
    */
    constexpr bool hasAll(const QoreParseOptions& mask) const {
        return (lo & mask.lo) == mask.lo && (hi & mask.hi) == mask.hi;
    }

    //! returns true if any bit in the mask is set in this object
    /** @param mask the bits to check
        @return true if (this & mask) != 0
    */
    constexpr bool hasAny(const QoreParseOptions& mask) const {
        return (lo & mask.lo) != 0 || (hi & mask.hi) != 0;
    }

    //! returns true if no bits are set
    constexpr bool empty() const {
        return lo == 0 && hi == 0;
    }

    //! returns the low 64 bits (bits 0-63)
    constexpr int64 getLo() const {
        return lo;
    }

    //! returns the high 64 bits (bits 64-127)
    constexpr int64 getHi() const {
        return hi;
    }

    //! extended option: disallow class definitions (bit 64)
    /** @since %Qore 2.1
    */
    DLLEXPORT static const QoreParseOptions NO_CLASS_DEFS;

    //! extended option: disallow constant definitions (bit 65)
    /** @since %Qore 2.1
    */
    DLLEXPORT static const QoreParseOptions NO_CONSTANT_DEFS;

    //! extended option: disallow namespace definitions (bit 66)
    /** @since %Qore 2.1
    */
    DLLEXPORT static const QoreParseOptions NO_NAMESPACE_DEFS;

    //! extended option: disallow the 'new' keyword (bit 67)
    /** @since %Qore 2.1
    */
    DLLEXPORT static const QoreParseOptions NO_NEW;

    //! extended option: allow old behavior where soft types accept NULL like NOTHING (bit 68)
    /** Enables pre-2.4 behavior where non-optional soft types (softint, softnumber, etc.)
        accepted SQL NULL values and treated them like NOTHING. This is an AST-only option
        for backward compatibility.
        @since %Qore 2.4
    */
    DLLEXPORT static const QoreParseOptions BROKEN_SOFT_TYPES;

private:
    int64 lo;  //!< bits 0-63 (compatible with legacy int64 parse options)
    int64 hi;  //!< bits 64-127 (extended parse options)
};

//! free-standing bitwise OR for mixed int64|QoreParseOptions expressions
/** Required because left-hand int64 cannot find member operators on QoreParseOptions.
*/
constexpr inline QoreParseOptions operator|(int64 lhs, const QoreParseOptions& rhs) {
    return QoreParseOptions(lhs) | rhs;
}

//! free-standing bitwise AND for mixed int64|QoreParseOptions expressions
constexpr inline QoreParseOptions operator&(int64 lhs, const QoreParseOptions& rhs) {
    return QoreParseOptions(lhs) & rhs;
}

//! free-standing bitwise XOR for mixed int64|QoreParseOptions expressions
constexpr inline QoreParseOptions operator^(int64 lhs, const QoreParseOptions& rhs) {
    return QoreParseOptions(lhs) ^ rhs;
}

#endif // _QORE_QOREPARSEOPTIONS_H
