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
    /** Enables pre-3.0 behavior where non-optional soft types (softint, softnumber, etc.)
        accepted SQL NULL values and treated them like NOTHING. This is an AST-only option
        for backward compatibility.
        @since %Qore 3.0
    */
    DLLEXPORT static const QoreParseOptions BROKEN_SOFT_TYPES;

    //! extended option: disallow the \c summarize statement (bit 69)
    /** Enforced at parse time: when set, any \c summarize statement triggers a parse error.
        Included in \c PO_MODERN so modern code always rejects \c summarize.
        @since %Qore 3.0
    */
    DLLEXPORT static const QoreParseOptions NO_SUMMARIZE;

    //! extended option: disallow %prepend-module-path / %append-module-path directives (bit 70)
    /** Enforced at parse time: when set, either directive raises a parse error.  Intended for
        embedders that want to prevent programs from extending their own module search path
        (see design/parse-directive-prepend-module-path.md "Sandboxing").  Not included in
        \c PO_MODERN — modern scripts that legitimately vendor modules should be able to do so.
        @since %Qore 3.0
    */
    DLLEXPORT static const QoreParseOptions NO_MODULE_PATH_DIRECTIVES;

    //! extended option: allow IEEE-unsafe floating-point optimizer rewrites (bit 71)
    /** This option is off by default. Optimizers may only reassociate floating-point
        plugin operations when this Program option is set and the registered operation
        descriptor also sets QorePluginOpcodeInfoExtended::fp_reassociation_allowed.
        @since %Qore 3.0
    */
    DLLEXPORT static const QoreParseOptions FP_FAST_MATH;

    //! extended option: disable the \c iterate streaming keyword (bit 72)
    /** @since %Qore 3.0
    */
    DLLEXPORT static const QoreParseOptions NO_ITERATE;

    //! extended option: disable the \c first streaming keyword and \c find \c first modifier (bit 73)
    /** @since %Qore 3.0
    */
    DLLEXPORT static const QoreParseOptions NO_FIRST;

    //! extended option: disable the \c any streaming operator keyword (bit 74)
    /** The \c any type name remains available.
        @since %Qore 3.0
    */
    DLLEXPORT static const QoreParseOptions NO_ANY_OPERATOR;

    //! extended option: disable the \c all streaming operator keyword (bit 75)
    /** @since %Qore 3.0
    */
    DLLEXPORT static const QoreParseOptions NO_ALL_OPERATOR;

    //! extended option: disable the \c count streaming operator keyword (bit 76)
    /** @since %Qore 3.0
    */
    DLLEXPORT static const QoreParseOptions NO_COUNT;

    //! extended option: disable the \c take streaming operator keyword (bit 77)
    /** @since %Qore 3.0
    */
    DLLEXPORT static const QoreParseOptions NO_TAKE;

    //! extended option: disable the \c drop streaming operator keyword (bit 78)
    /** @since %Qore 3.0
    */
    DLLEXPORT static const QoreParseOptions NO_DROP;

    //! extended option: disable the \c takewhile streaming operator keyword (bit 79)
    /** @since %Qore 3.0
    */
    DLLEXPORT static const QoreParseOptions NO_TAKEWHILE;

    //! extended option: disable the \c takeuntil streaming operator keyword (bit 80)
    /** @since %Qore 3.0
    */
    DLLEXPORT static const QoreParseOptions NO_TAKEUNTIL;

    //! extended option: disable \c find modifiers added for streaming support (bit 81)
    /** Disables \c find \c first, \c find \c last, and \c find \c one. The legacy
        \c find form remains available.
        @since %Qore 3.0
    */
    DLLEXPORT static const QoreParseOptions NO_FIND_MODIFIERS;

    //! extended option: disable streaming-chain compiler fusion (bit 82)
    /** Streaming operators remain available; only compiler fusion is disabled.
        @since %Qore 3.0
    */
    DLLEXPORT static const QoreParseOptions NO_STREAM_FUSION;

    //! extended option: prefer the \c any streaming operator in legacy ambiguous expression contexts (bit 83)
    /** Source-only \c any operator usage should use the parenthesized form (\c any \c (source)) so
        \c any \c name; remains a declaration. This option is implied by \c PO_MODERN for compatibility
        with the initial streaming-operator implementation. \c NO_ANY_OPERATOR disables the operator.
        @since %Qore 3.0
    */
    DLLEXPORT static const QoreParseOptions STREAMING_ANY;

    //! extended option: disable the \c char builtin type name and char literals (bit 84)
    /** This option keeps \c char available for legacy code that uses it as an identifier or user-defined type name.
        @since %Qore 3.0
    */
    DLLEXPORT static const QoreParseOptions NO_CHAR_TYPE;

    //! extended option: keep string single-index access returning a string (bit 85)
    /** When set, \c str[i] preserves the historical \c *string behavior instead of returning \c *char.
        @since %Qore 3.0
    */
    DLLEXPORT static const QoreParseOptions NO_STRING_INDEX_CHAR;

    //! extended option: interpret negative container offsets relative to the end (bit 86)
    /** When set, negative list, string, binary, and buffer indexes and slice endpoints
        are interpreted as offsets from the end of the container.
        @since %Qore 3.0
    */
    DLLEXPORT static const QoreParseOptions NEGATIVE_OFFSETS;

    //! extended option bundle: disable all streaming operator keywords and find modifiers
    /** This is a convenience mask combining the individual keyword opt-outs.
        @since %Qore 3.0
    */
    DLLEXPORT static const QoreParseOptions NO_STREAMING_OPERATORS;

    //! extended option: allow misleading no-op \c cast<hash<auto>> / \c cast<list<auto>> casts (bit 87)
    /** When set, \c cast<hash<auto>>(...), \c cast<list<auto>>(...) and their \c "or nothing" variants
        are accepted (as no-ops) instead of raising a parse error.  Provided for backward compatibility
        with code written before these casts were rejected; new code should use \c cast<hash>(...) /
        \c cast<list>(...) instead.  Not included in \c PO_MODERN.
        @since %Qore 3.0
    */
    DLLEXPORT static const QoreParseOptions BROKEN_AUTO_CAST;

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
