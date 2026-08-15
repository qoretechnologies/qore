/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreValue.h

    NaN-boxed QoreValue implementation - 8 bytes instead of 16

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

#ifndef _QORE_QOREVALUE_H
#define _QORE_QOREVALUE_H

#include <qore/common.h>
#include <qore/node_types.h>

#include <cstdint>
#include <cstring>
#include <cassert>
#include <type_traits>
#include <string>

// ============================================================================
// Legacy type definitions - kept for QoreLValue and binary module compatibility
// ============================================================================

typedef unsigned char valtype_t;

//! @defgroup QoreValue type constants
/** the possible values for QoreValue::type (legacy - use type checking methods instead)
 */
///@{
#define QV_Bool  (valtype_t)0  //!< for boolean values
#define QV_Int   (valtype_t)1  //!< for integer values
#define QV_Float (valtype_t)2  //!< for floating-point values
#define QV_Node  (valtype_t)3  //!< for heap-allocated values
#define QV_Ref   (valtype_t)4  //!< for references (when used with lvalues)
#define QV_Enum  (valtype_t)5  //!< for enum values (stores const QoreEnumMember*)
#define QV_Value (valtype_t)6  //!< for inline QoreValue storage in lvalues
///@}

// Forward declarations
class AbstractQoreNode;
class QoreStringNode;
class QoreString;
class QoreEncoding;
class ExceptionSink;
class QoreTypeInfo;
class QoreValue;
class RuntimeConfig;
class QoreEnumMember;

//! this is the union that stores values in QoreLValue (legacy - kept for QoreLValue compatibility)
/** The may_alias attribute tells the compiler that pointers to this type may alias with any other
    pointer type, preventing strict-aliasing-based misoptimizations when the same memory is accessed
    through different pointer contexts (e.g., via LValueHelper's dual val/qv pointer pattern).
*/
#if defined(__GNUC__) || defined(__clang__)
union __attribute__((may_alias)) qore_value_u {
#else
union qore_value_u {
#endif
    bool b;                     //!< for boolean values
    int64 i;                    //!< for integer values
    double f;                   //!< for double values
    AbstractQoreNode* n;        //!< for all heap-allocated values
    const QoreEnumMember* em;   //!< for enum member pointers (QV_Enum)
    uint64_t qv;                //!< for inline QoreValue bits (QV_Value)
};

// ============================================================================
// QoreSimpleValue - Trivially copyable value type for unions and varargs
// ============================================================================

//! Minimal value type with uninitialized default constructor for use in unions
/** This struct is designed for use in:
    - C unions (requires trivial default constructor)
    - varargs passing (requires trivial copy)

    WARNING: Default constructor leaves bits UNINITIALIZED.
    Convert to QoreValue before calling any methods.

    @code
    // In union:
    union { QoreSimpleValue qv; ... };

    // When using:
    QoreValue(u.qv).someMethod();  // Convert first!
    @endcode

    @see QoreValue for the safe version with initialized default constructor
*/
struct QoreSimpleValue {
    //! The raw 64-bit NaN-boxed value - PUBLIC for direct access in parser
    uint64_t bits;

    //! Default constructor - leaves bits UNINITIALIZED (required for unions)
    QoreSimpleValue() noexcept = default;

    //! Copy constructor - trivial
    QoreSimpleValue(const QoreSimpleValue&) noexcept = default;

    //! Copy assignment - trivial
    QoreSimpleValue& operator=(const QoreSimpleValue&) noexcept = default;

    //! Construct from QoreValue (implicit conversion)
    DLLLOCAL inline QoreSimpleValue(const QoreValue& v) noexcept;

    //! Assignment from QoreValue
    DLLLOCAL inline QoreSimpleValue& operator=(const QoreValue& v) noexcept;

    //! Returns the raw bits for debugging/serialization
    DLLLOCAL uint64_t rawBits() const noexcept { return bits; }

    //! Set raw bits directly (for deserialization)
    DLLLOCAL void setRawBits(uint64_t b) noexcept { bits = b; }

    //! Explicit bool conversion - true if not Nothing (bits != 0) and not Null
    /** Note: This is a quick check, not a full type analysis */
    DLLLOCAL explicit operator bool() const noexcept {
        // Nothing is 0, Null is 0xFFFB000000000001
        return bits != 0 && bits != 0xFFFB000000000001ULL;
    }
};

// Compile-time verification that QoreSimpleValue is trivially copyable (required for unions/varargs)
static_assert(std::is_trivially_copyable<QoreSimpleValue>::value,
    "QoreSimpleValue must be trivially copyable for use in unions and varargs");
static_assert(std::is_trivially_default_constructible<QoreSimpleValue>::value,
    "QoreSimpleValue must be trivially default constructible for use in unions");
static_assert(sizeof(QoreSimpleValue) == 8, "QoreSimpleValue must be exactly 8 bytes");

//! namespace for implementation details of QoreValue functions
namespace detail {
    //! used in QoreLValue::get() - returns T* for class types
    template<typename Type>
    struct QoreValueCastHelper {
        typedef Type * Result;

        //! used by QoreValue::get()
        template<typename QV>
        static Result cast(QV *qv) {
            assert(qv->isPointer());
            assert(!qv->getInternalNode() || dynamic_cast<Result>(qv->getInternalNode()));
            return reinterpret_cast<Result>(qv->getInternalNode());
        }

        //! used by QoreLValue::get() - takes type parameter for compatibility
        template<typename QV>
        static Result cast(QV *qv, valtype_t type) {
            assert(type == QV_Node);
            return reinterpret_cast<Result>(qv->v.n);
        }
    };

    //! used in QoreLValue::get() - specialization for bool
    template<>
    struct QoreValueCastHelper<bool> {
        typedef bool Result;

        template<typename QV>
        static bool cast(QV *qv) {
            return qv->getAsBool();
        }

        template<typename QV>
        static bool cast(QV *qv, valtype_t type) {
            return qv->v.b;
        }
    };

    //! used in QoreLValue::get() - specialization for double
    template<>
    struct QoreValueCastHelper<double> {
        typedef double Result;

        template<typename QV>
        static double cast(QV *qv) {
            return qv->getAsFloat();
        }

        template<typename QV>
        static double cast(QV *qv, valtype_t type) {
            return qv->v.f;
        }
    };

    //! used in QoreLValue::get() - specialization for int64
    template<>
    struct QoreValueCastHelper<int64> {
        typedef int64 Result;

        template<typename QV>
        static int64 cast(QV *qv) {
            return qv->getAsBigInt();
        }

        template<typename QV>
        static int64 cast(QV *qv, valtype_t type) {
            return qv->v.i;
        }
    };
} // namespace detail

/*
 * ============================================================================
 * NaN-Boxing Encoding Scheme
 * ============================================================================
 *
 * IEEE 754 double-precision format:
 *   [sign:1][exponent:11][mantissa:52]
 *
 * ENCODING OVERVIEW:
 *   Doubles are stored with a +2^48 offset to shift them below the tag space.
 *   All other types use high bits as tags.
 *
 * BIT LAYOUT:
 *   Doubles:       Encoded value < 0xFFF9... (after adding 2^48 offset)
 *   Short strings: Bits 63-52 = 0xFFC, bits 51-48 = length (0-6), bits 47-0 = data
 *   Integers:      Bits 63-48 = 0xFFF9, bits 47-0 = 48-bit signed value
 *   Pointers:      Bits 63-48 = 0xFFFA, bits 47-0 = 48-bit address
 *   Special:       Bits 63-48 = 0xFFFB, bits 47-0 = discriminator (0-3)
 *   Char:          Bits 63-48 = 0xFFFC, bits 20-0 = Unicode codepoint
 *
 * TAG ALLOCATION (bits 63-48):
 *   < 0xFFC0:      Encoded doubles (raw bits + 2^48)
 *   0xFFC0-0xFFC6: Short strings (length in bits 51-48)
 *   0xFFC7-0xFFCF: Reserved by the short-string decoder family
 *   0xFFF9:        48-bit signed integers
 *   0xFFFA:        Pointers to AbstractQoreNode
 *   0xFFFB:        Special values (nothing=0, null=1, false=2, true=3)
 *   0xFFFC:        Unicode character codepoint
 *   0xFFFD:        Enum member pointers
 *   0xFFFE-0xFFFF: Reserved for future plugin immediate tags
 *
 * INTEGER RANGE: +/-140,737,488,355,328 (+/-2^47)
 *   - Covers virtually all practical integer usage
 *   - Larger integers stored as QoreBigIntNode pointers (transparent to user)
 *
 * SHORT STRING CAPACITY: Up to 6 bytes UTF-8
 *   - ASCII: up to 6 characters ("status", "value", etc.)
 *   - CJK: up to 2 characters
 *   - Comparison is single uint64 compare (18x faster than strcmp)
 *
 * PERFORMANCE IMPROVEMENTS (vs 16-byte QoreValue):
 *   - Memory: 50% reduction
 *   - Type checking: 1.9x faster
 *   - Mixed operations: 1.2x faster
 *   - Short string compare: 18x faster
 *   - Function calls: 10-20% faster (single register vs. two)
 */

//! The main value class in Qore, designed to be passed by value
/** This class uses NaN-boxing to store multiple types in a single 64-bit word:
    - doubles stored with offset encoding
    - 48-bit signed integers stored inline
    - short strings (up to 6 bytes) stored inline
    - pointers to AbstractQoreNode for heap-allocated values
    - special values (nothing, null, true, false)
*/
#if defined(__GNUC__) || defined(__clang__)
class __attribute__((may_alias)) QoreValue {
#else
class QoreValue {
#endif
    friend class ValueHolder;
    friend class ValueOptionalRefHolder;
    friend class ValueEvalRefHolder;

private:
    uint64_t bits;

    // ========================================================================
    // Tag constants
    // ========================================================================

    //! Double encoding offset: Adding 2^48 to doubles ensures that after encoding,
    //! all valid doubles (including -inf, negative NaNs) are < DOUBLE_BOUNDARY.
    static constexpr uint64_t DOUBLE_ENCODE_OFFSET = 0x0001000000000000ULL;  // 2^48

    //! Boundary for double detection: encoded doubles are always below this value.
    static constexpr uint64_t DOUBLE_BOUNDARY   = 0xFFF9000000000000ULL;

    // Non-double tag values and tag families.
    static constexpr uint16_t TAG16_INT48       = 0xFFF9;
    static constexpr uint16_t TAG16_POINTER     = 0xFFFA;
    static constexpr uint16_t TAG16_SPECIAL     = 0xFFFB;
    static constexpr uint16_t TAG16_CHAR        = 0xFFFC;
    static constexpr uint16_t TAG16_ENUM        = 0xFFFD;
    static constexpr uint16_t TAG12_SHORTSTR    = 0xFFC;
    static constexpr uint16_t TAG16_SHORTSTR_FIRST = 0xFFC0;
    static constexpr uint16_t TAG16_SHORTSTR_LAST  = 0xFFCF;
    static constexpr uint16_t TAG16_PLUGIN_IMMEDIATE_FIRST = 0xFFFE;
    static constexpr uint16_t TAG16_PLUGIN_IMMEDIATE_LAST  = 0xFFFF;

    static constexpr uint64_t TAG_INT48         = static_cast<uint64_t>(TAG16_INT48) << 48;
    static constexpr uint64_t TAG_POINTER       = static_cast<uint64_t>(TAG16_POINTER) << 48;
    static constexpr uint64_t TAG_SPECIAL       = static_cast<uint64_t>(TAG16_SPECIAL) << 48;
    static constexpr uint64_t TAG_CHAR          = static_cast<uint64_t>(TAG16_CHAR) << 48;
    //! Short strings: 0xFFC + (length in bits 48-50), so 0xFFC0-0xFFC6
    static constexpr uint64_t TAG_SHORTSTR_BASE = static_cast<uint64_t>(TAG12_SHORTSTR) << 52;
    //! Enum values: stores pointer to QoreEnumMember, zero allocation, zero ref-counting
    static constexpr uint64_t TAG_ENUM          = static_cast<uint64_t>(TAG16_ENUM) << 48;

    static_assert(TAG16_SHORTSTR_LAST < TAG16_INT48,
        "short-string tag family must remain below the non-double value tag boundary");
    static_assert(TAG16_INT48 < TAG16_PLUGIN_IMMEDIATE_FIRST,
        "inline integer tag must not collide with plugin immediate tags");
    static_assert(TAG16_POINTER < TAG16_PLUGIN_IMMEDIATE_FIRST,
        "pointer tag must not collide with plugin immediate tags");
    static_assert(TAG16_SPECIAL < TAG16_PLUGIN_IMMEDIATE_FIRST,
        "special-value tag must not collide with plugin immediate tags");
    static_assert(TAG16_CHAR < TAG16_PLUGIN_IMMEDIATE_FIRST,
        "char tag must not collide with plugin immediate tags");
    static_assert(TAG16_ENUM < TAG16_PLUGIN_IMMEDIATE_FIRST,
        "enum tag must not collide with plugin immediate tags");

    // Masks
    static constexpr uint64_t TAG_MASK          = 0xFFFF000000000000ULL;
    static constexpr uint64_t PAYLOAD_MASK      = 0x0000FFFFFFFFFFFFULL;  // 48 bits

    // Special value encodings
    // NOTHING is encoded as 0 so that default/zero-initialized QoreValue is NOTHING
    // This allows QoreValue() = default to work correctly for static storage and value initialization
    static constexpr uint64_t VAL_NOTHING       = 0;
    // Other special values use TAG_SPECIAL with discriminator in low bits
    static constexpr uint64_t VAL_NULL          = TAG_SPECIAL | 1;
    static constexpr uint64_t VAL_FALSE         = TAG_SPECIAL | 2;
    static constexpr uint64_t VAL_TRUE          = TAG_SPECIAL | 3;

    //! Minimum 48-bit signed integer value: -140,737,488,355,328
    static constexpr int64_t INT48_MIN = -(1LL << 47);
    //! Maximum 48-bit signed integer value: 140,737,488,355,327
    static constexpr int64_t INT48_MAX = (1LL << 47) - 1;

    //! Maximum Unicode codepoint value.
    static constexpr unsigned CHAR_MAX_CODEPOINT = 0x10FFFF;

    // ========================================================================
    // Private helpers
    // ========================================================================

    //! Get tag (high 16 bits)
    DLLLOCAL uint64_t tag() const { return bits & TAG_MASK; }

    //! Get payload (low 48 bits)
    DLLLOCAL uint64_t payload() const { return bits & PAYLOAD_MASK; }

    //! Returns pointer without type check (internal use only)
    DLLLOCAL AbstractQoreNode* getPointerUnsafe() const {
        return reinterpret_cast<AbstractQoreNode*>(payload());
    }

    //! Sets a large integer value (allocates QoreBigIntNode)
    DLLEXPORT void setLargeInt(int64 i);

    //! Returns the internal node pointer without reference count change
    DLLLOCAL AbstractQoreNode* takeNodeIntern();

    //! Aborts in debug builds if a string-typed get<T>() is applied to an inline short string
    /** An inline short string has no AbstractQoreNode, so get<T>() must return nullptr for it even
        though getType() returns NT_STRING; callers that dereference the result then crash.  Use
        QoreStringDataHelper for representation-independent access instead.
    */
    template <typename T>
    DLLLOCAL void checkShortStringAccess() const {
        using Base = typename std::remove_cv<typename std::remove_pointer<T>::type>::type;
        if constexpr (std::is_same<Base, QoreStringNode>::value || std::is_same<Base, QoreString>::value) {
            assert(!isShortString());
        }
    }

public:
    // ========================================================================
    // Constructors
    // ========================================================================

    //! Default constructor: initializes to NOTHING (safe)
    /** Unlike QoreSimpleValue, QoreValue's default constructor is safe and
        initializes the value to NOTHING. Use QoreSimpleValue only in unions
        or varargs where trivial construction is required.
    */
    DLLLOCAL constexpr QoreValue() noexcept : bits(0) {}

    //! Construct from QoreSimpleValue (implicit conversion)
    DLLLOCAL constexpr QoreValue(QoreSimpleValue sv) noexcept : bits(sv.bits) {}

    //! Creates a boolean value
    DLLLOCAL constexpr QoreValue(bool b) noexcept : bits(b ? VAL_TRUE : VAL_FALSE) {}

    //! Creates an integer value (stores inline if in 48-bit range, otherwise allocates QoreBigIntNode)
    DLLEXPORT QoreValue(int i);

    //! Creates an integer value
    DLLEXPORT QoreValue(unsigned int i);

    //! Creates an integer value
    DLLEXPORT QoreValue(long i);

    //! Creates an integer value
    DLLEXPORT QoreValue(unsigned long i);

    //! Creates an integer value
    DLLEXPORT QoreValue(unsigned long long i);

    //! Creates an integer value
    DLLEXPORT QoreValue(int64 i);

    //! Creates a floating-point value
    DLLEXPORT QoreValue(double f);

    //! Creates a value from an AbstractQoreNode pointer (takes reference)
    DLLEXPORT QoreValue(AbstractQoreNode* n);

    //! Creates a value from a const AbstractQoreNode pointer (sanitizes and may take reference)
    DLLEXPORT QoreValue(const AbstractQoreNode* n);

    //! Copy constructor (trivial - just copies bits)
    constexpr QoreValue(const QoreValue& other) noexcept = default;

    // ========================================================================
    // Type checking - all are very fast (single comparison or mask+compare)
    // ========================================================================

    //! Returns true if the value is a double
    DLLLOCAL bool isFloat() const {
        // Encoded doubles are below DOUBLE_BOUNDARY, but we must exclude:
        // - bits=0 (NOTHING)
        // - short strings (tag 0xFFC)
        return bits != 0 && bits < DOUBLE_BOUNDARY && (bits >> 52) != 0xFFC;
    }

    //! Returns true if the value is an inline 48-bit integer
    DLLLOCAL bool isInt() const {
        return tag() == TAG_INT48;
    }

    //! Returns true if the value is a pointer to AbstractQoreNode
    DLLLOCAL bool isPointer() const {
        return tag() == TAG_POINTER;
    }

    //! Returns true if the value is a short string stored inline
    DLLLOCAL bool isShortString() const {
        return (bits >> 52) == 0xFFC;
    }

    //! Returns true if the value is a TAG_ENUM value (inline enum member pointer)
    DLLLOCAL bool isEnum() const {
        return tag() == TAG_ENUM;
    }

    //! Returns true if the value is an inline Unicode character.
    DLLLOCAL bool isChar() const {
        return tag() == TAG_CHAR;
    }

    //! Returns true if the value is a boolean (true or false)
    DLLLOCAL bool isBool() const {
        return bits == VAL_TRUE || bits == VAL_FALSE;
    }

    //! Returns true if the value is boolean true
    DLLLOCAL bool isTrue() const {
        return bits == VAL_TRUE;
    }

    //! Returns true if the value is boolean false
    DLLLOCAL bool isFalse() const {
        return bits == VAL_FALSE;
    }

    //! Returns true if the value is NOTHING
    DLLEXPORT bool isNothing() const;

    //! Returns true if the value is NULL
    DLLEXPORT bool isNull() const;

    //! Returns true if the value is NOTHING or NULL
    DLLEXPORT bool isNullOrNothing() const;

    //! Returns true if the value is a special value (nothing, null, true, false)
    DLLLOCAL bool isSpecial() const {
        return bits == 0 || tag() == TAG_SPECIAL;
    }

    //! First reserved high-16-bit tag for future plugin immediates.
    DLLLOCAL static constexpr uint16_t firstPluginImmediateTag() noexcept {
        return TAG16_PLUGIN_IMMEDIATE_FIRST;
    }

    //! Last reserved high-16-bit tag for future plugin immediates.
    DLLLOCAL static constexpr uint16_t lastPluginImmediateTag() noexcept {
        return TAG16_PLUGIN_IMMEDIATE_LAST;
    }

    //! Returns true if a high-16-bit tag is reserved for future plugin immediates.
    DLLLOCAL static constexpr bool isReservedPluginImmediateTag(uint16_t tag) noexcept {
        return tag >= TAG16_PLUGIN_IMMEDIATE_FIRST && tag <= TAG16_PLUGIN_IMMEDIATE_LAST;
    }

    //! Returns true if a high-16-bit tag is owned by the built-in QoreValue encoding.
    DLLLOCAL static constexpr bool isBuiltinNanboxTag(uint16_t tag) noexcept {
        return tag == TAG16_INT48
            || tag == TAG16_POINTER
            || tag == TAG16_SPECIAL
            || tag == TAG16_CHAR
            || (tag >= TAG16_SHORTSTR_FIRST && tag <= TAG16_SHORTSTR_LAST)
            || tag == TAG16_ENUM;
    }

    // ========================================================================
    // Value extraction (fast, no type conversion)
    // ========================================================================

    //! Extracts double value (asserts if not a double)
    DLLLOCAL double getDouble() const {
        assert(isFloat());
        uint64_t rawBits = bits - DOUBLE_ENCODE_OFFSET;
        double d;
        memcpy(&d, &rawBits, sizeof(d));
        return d;
    }

    //! Extracts inline 48-bit integer value (asserts if not an int)
    DLLLOCAL int64 getInt() const {
        assert(isInt());
        // Sign-extend from 48 bits to 64 bits
        int64 v = static_cast<int64>(payload());
        if (v & (1LL << 47)) {
            v |= 0xFFFF000000000000LL;
        }
        return v;
    }

    //! Extracts boolean value (asserts if not a bool)
    DLLLOCAL bool getBool() const {
        assert(isBool());
        return bits == VAL_TRUE;
    }

    //! Extracts a Unicode codepoint (asserts if not a char)
    DLLLOCAL unsigned getChar() const {
        assert(isChar());
        return static_cast<unsigned>(payload());
    }

    //! Extracts pointer value (asserts if not a pointer)
    DLLLOCAL AbstractQoreNode* getPtr() const {
        assert(isPointer());
        return getPointerUnsafe();
    }

    // ========================================================================
    // Short string operations
    // ========================================================================

    //! Try to create a short string inline.
    /** @param out receives the inline value on success
        @param str string bytes to store
        @param len byte length of \a str
        @return true if the string fits inline, false otherwise
    */
    DLLEXPORT static bool tryMakeShortString(QoreValue& out, const char* str, size_t len);

    //! Create a short string.
    /** @param str string bytes to store
        @param len byte length of \a str
        @return inline string value
        @note asserts if \a len is greater than SHORTSTR_MAX_BYTES
    */
    DLLEXPORT static QoreValue makeShortString(const char* str, size_t len);

    //! Create a Qore string value using default encoding.
    /** @param str string bytes to store
        @param len byte length of \a str
        @return string value, using inline storage when the effective encoding is UTF-8 and the byte length fits
    */
    DLLEXPORT static QoreValue makeStringValue(const char* str, size_t len);

    //! Create a Qore string value using default encoding.
    /** @param str null-terminated string bytes to store
        @return string value, using inline storage when the effective encoding is UTF-8 and the byte length fits
    */
    DLLEXPORT static QoreValue makeStringValue(const char* str);

    //! Create a Qore string value with the given encoding.
    /** @param str string bytes to store
        @param len byte length of \a str
        @param enc string encoding; QCS_DEFAULT is used when null
        @return string value, using inline storage when \a enc is UTF-8 and the byte length fits
    */
    DLLEXPORT static QoreValue makeStringValue(const char* str, size_t len, const QoreEncoding* enc);

    //! Create a Qore string value with the given encoding.
    /** @param str null-terminated string bytes to store
        @param enc string encoding; QCS_DEFAULT is used when null
        @return string value, using inline storage when \a enc is UTF-8 and the byte length fits
    */
    DLLEXPORT static QoreValue makeStringValue(const char* str, const QoreEncoding* enc);

    //! Create a Qore string value using default encoding.
    /** @param str string bytes to store
        @return string value, using inline storage when the effective encoding is UTF-8 and the byte length fits
    */
    DLLEXPORT static QoreValue makeStringValue(const std::string& str);

    //! Create a Qore string value with the given encoding.
    /** @param str string bytes to store
        @param enc string encoding; QCS_DEFAULT is used when null
        @return string value, using inline storage when \a enc is UTF-8 and the byte length fits
    */
    DLLEXPORT static QoreValue makeStringValue(const std::string& str, const QoreEncoding* enc);

    //! Create a UTF-8 Qore string value.
    /** @param str UTF-8 string bytes to store
        @param len byte length of \a str
        @return UTF-8 string value, using inline storage when the byte length fits
    */
    DLLEXPORT static QoreValue makeUtf8StringValue(const char* str, size_t len);

    //! Create a UTF-8 Qore string value.
    /** @param str null-terminated UTF-8 string bytes to store
        @return UTF-8 string value, using inline storage when the byte length fits
    */
    DLLEXPORT static QoreValue makeUtf8StringValue(const char* str);

    //! Returns true if the codepoint is valid for a char value.
    DLLLOCAL static bool isValidCharCodepoint(unsigned codepoint) {
        return codepoint <= CHAR_MAX_CODEPOINT;
    }

    //! Create a char value from a Unicode codepoint.
    DLLEXPORT static QoreValue makeChar(unsigned codepoint);

    //! Create a char value from a string that must contain exactly one codepoint.
    DLLEXPORT static QoreValue makeCharFromString(const QoreString& str, ExceptionSink* xsink);

    //! Create a char value from a single-character substring at the given character offset.
    DLLEXPORT static QoreValue makeCharFromStringAt(const QoreString& str, qore_offset_t offset,
            ExceptionSink* xsink);

    //! Create a one-character string from a Unicode codepoint.
    DLLEXPORT static QoreStringNode* makeCharString(unsigned codepoint, const QoreEncoding* enc,
            ExceptionSink* xsink);

    //! Get length of short string (asserts if not a short string)
    DLLLOCAL size_t shortStringLen() const {
        assert(isShortString());
        size_t len = (bits >> 48) & 0xF;
        if (len > SHORTSTR_MAX_BYTES) {
            len = SHORTSTR_MAX_BYTES;
        }
        return len;
    }

    //! Extract short string into buffer (must have space for at least 7 bytes)
    DLLEXPORT void getShortString(char* buf) const;

    //! Fast short string comparison (single uint64 compare)
    DLLLOCAL bool shortStringEquals(const QoreValue& other) const {
        assert(isShortString() && other.isShortString());
        return bits == other.bits;
    }

    // ========================================================================
    // Type conversions (with type coercion)
    // ========================================================================

    //! Returns the value as a 64-bit integer with type conversion
    DLLEXPORT int64 getAsBigInt() const;

    //! Returns the value as a double with type conversion
    DLLEXPORT double getAsFloat() const;

    //! Returns the value as a boolean with type conversion
    DLLEXPORT bool getAsBool() const;

    // ========================================================================
    // QoreValue API
    // ========================================================================

    //! Returns the type code of the value
    DLLEXPORT qore_type_t getType() const;

    //! Returns the type name as a string
    DLLEXPORT const char* getTypeName() const;

    //! Returns the internal AbstractQoreNode pointer (nullptr if not a node type)
    DLLEXPORT AbstractQoreNode* getInternalNode();

    //! Returns the internal AbstractQoreNode pointer (nullptr if not a node type)
    DLLEXPORT const AbstractQoreNode* getInternalNode() const;

    //! Returns true if the object holds a value (not a parse expression)
    DLLEXPORT bool isValue() const;

    //! Returns true if the value needs evaluation
    DLLEXPORT bool needsEval() const;

    //! Returns true if the value has side effects
    DLLEXPORT bool hasEffect() const;

    //! Returns true if the value is a scalar (int, bool, float, number, string)
    DLLEXPORT bool isScalar() const;

    //! Returns true if the value is a constant (does not require evaluation)
    DLLEXPORT bool isConstant() const;

    //! Returns true if the value contains a non-null AbstractQoreNode pointer
    DLLEXPORT bool hasNode() const;

    //! Returns true if the value is reference-counted
    DLLEXPORT bool isReferenceCounted() const;

    //! Returns true if dereferencing could throw an exception
    DLLEXPORT bool derefCanThrowException() const;

    // ========================================================================
    // Comparison operations
    // ========================================================================

    //! Hard comparison (no type conversion)
    DLLEXPORT bool isEqualHard(const QoreValue& other) const;

    //! Soft comparison (with type conversion)
    DLLEXPORT bool isEqualSoft(const QoreValue& other, ExceptionSink* xsink) const;

    //! Value comparison (checks if same pointer for nodes)
    DLLEXPORT bool isEqualValue(const QoreValue& other) const;

    // ========================================================================
    // Value assignment and manipulation
    // ========================================================================

    //! Sets an integer value (any current value is overwritten without dereferencing)
    DLLLOCAL void set(int64 i) {
        if (i >= INT48_MIN && i <= INT48_MAX) {
            bits = TAG_INT48 | (static_cast<uint64_t>(i) & PAYLOAD_MASK);
        } else {
            setLargeInt(i);
        }
    }

    //! Sets a double value (any current value is overwritten without dereferencing)
    /** For most doubles, stores them inline using offset encoding. However, doubles
        with bit patterns >= 0xFFF8000000000000 (including negative NaN values)
        would collide with internal NaN-boxing tags after encoding, so they are
        stored as QoreFloatNode instead.
    */
    DLLEXPORT void set(double f);

    //! Sets a boolean value (any current value is overwritten without dereferencing)
    DLLLOCAL void set(bool b) {
        bits = b ? VAL_TRUE : VAL_FALSE;
    }

    //! Sets a Unicode character value.
    DLLLOCAL void setChar(unsigned codepoint) {
        assert(isValidCharCodepoint(codepoint));
        bits = TAG_CHAR | codepoint;
    }

    //! Sets a value from another QoreValue (any current value is overwritten without dereferencing)
    DLLEXPORT void set(const QoreValue& val);

    //! Sets a node value (any current value is overwritten without dereferencing)
    DLLEXPORT void set(AbstractQoreNode* n);

    //! Sets the value to NOTHING (does not dereference any current node)
    DLLEXPORT void clear();

    //! Dereferences any contained node and sets to NOTHING
    DLLEXPORT void discard(ExceptionSink* xsink);

    //! Assigns a new value and returns any previous node value
    DLLEXPORT AbstractQoreNode* assign(const QoreValue& n);

    //! Assigns a node value and returns any previous node value
    DLLEXPORT AbstractQoreNode* assign(AbstractQoreNode* n);

    //! Assigns an integer and returns any previous node value
    DLLEXPORT AbstractQoreNode* assign(int64 n);

    //! Assigns a double and returns any previous node value
    DLLEXPORT AbstractQoreNode* assign(double n);

    //! Assigns a boolean and returns any previous node value
    DLLEXPORT AbstractQoreNode* assign(bool n);

    //! Assigns NOTHING and returns any previous node value
    DLLEXPORT AbstractQoreNode* assignNothing();

    //! Swaps values with another QoreValue
    DLLEXPORT void swap(QoreValue& val);

    //! Converts node values to efficient inline representation if possible
    DLLEXPORT void sanitize();

    // ========================================================================
    // Reference counting
    // ========================================================================

    //! Increments reference count if the value is a node
    DLLEXPORT void ref() const;

    //! Increments reference count if the value is a node and returns *this
    DLLEXPORT QoreValue refSelf() const;

    // ========================================================================
    // Node operations
    // ========================================================================

    //! Takes and returns the node value, leaving this empty (asserts if not a node)
    DLLEXPORT AbstractQoreNode* takeNode();

    //! Takes and returns the node value if present, otherwise returns nullptr
    DLLEXPORT AbstractQoreNode* takeIfNode();

    //! Returns a pointer to an object of the given class (takes the pointer)
    template<typename T>
    DLLLOCAL T* take() {
        assert(isPointer());
        assert(dynamic_cast<T*>(getPointerUnsafe()));
        T* rv = reinterpret_cast<T*>(getPointerUnsafe());
        bits = VAL_NOTHING;
        return rv;
    }

    //! Returns the value as the given type (for pointer types, returns T directly)
    //! Returns nullptr if the value is not a pointer (e.g., NOTHING, inline int/float/bool)
    //! For TAG_ENUM values, delegates to the base value's get<T>()
    /** @warning this also returns nullptr for a string held in inline short string storage, even
        though getType() returns NT_STRING for such a value; never write
        <tt>v.get<QoreStringNode>()->c_str()</tt> after a <tt>getType() == NT_STRING</tt> check, as
        that is a null pointer dereference for any string of QoreValue::SHORTSTR_MAX_BYTES bytes or
        fewer.  Use QoreStringDataHelper for allocation-free, conversion-free access to the bytes,
        byte length, and encoding of a Qore string value in either representation.
    */
    template<typename T>
    DLLLOCAL typename std::enable_if<std::is_pointer<T>::value, T>::type get() {
        if (isEnum()) {
            QoreValue base = getEnumBaseValue();
            return base.get<T>();
        }
        if (!isPointer()) {
            checkShortStringAccess<T>();
            return nullptr;
        }
        assert(!getPointerUnsafe() || dynamic_cast<T>(getPointerUnsafe()));
        return reinterpret_cast<T>(getPointerUnsafe());
    }

    //! Returns the value as T* (for non-pointer types like const ReferenceNode, returns pointer to T)
    //! Returns nullptr if the value is not a pointer (e.g., NOTHING, inline int/float/bool)
    //! For TAG_ENUM values, delegates to the base value's get<T>()
    /** @warning this also returns nullptr for a string held in inline short string storage, even
        though getType() returns NT_STRING for such a value; never write
        <tt>v.get<QoreStringNode>()->c_str()</tt> after a <tt>getType() == NT_STRING</tt> check, as
        that is a null pointer dereference for any string of QoreValue::SHORTSTR_MAX_BYTES bytes or
        fewer.  Use QoreStringDataHelper for allocation-free, conversion-free access to the bytes,
        byte length, and encoding of a Qore string value in either representation.
    */
    template<typename T>
    DLLLOCAL typename std::enable_if<!std::is_pointer<T>::value && std::is_class<T>::value, T*>::type get() {
        if (isEnum()) {
            QoreValue base = getEnumBaseValue();
            return base.get<T>();
        }
        if (!isPointer()) {
            checkShortStringAccess<T>();
            return nullptr;
        }
        assert(!getPointerUnsafe() || dynamic_cast<T*>(getPointerUnsafe()));
        return reinterpret_cast<T*>(getPointerUnsafe());
    }

    //! Returns the value as T* const version (for non-pointer types like const ReferenceNode)
    //! Returns nullptr if the value is not a pointer (e.g., NOTHING, inline int/float/bool)
    //! For TAG_ENUM values, delegates to the base value's get<T>()
    /** @warning this also returns nullptr for a string held in inline short string storage, even
        though getType() returns NT_STRING for such a value; never write
        <tt>v.get<QoreStringNode>()->c_str()</tt> after a <tt>getType() == NT_STRING</tt> check, as
        that is a null pointer dereference for any string of QoreValue::SHORTSTR_MAX_BYTES bytes or
        fewer.  Use QoreStringDataHelper for allocation-free, conversion-free access to the bytes,
        byte length, and encoding of a Qore string value in either representation.
    */
    template<typename T>
    DLLLOCAL typename std::enable_if<!std::is_pointer<T>::value && std::is_class<T>::value, T*>::type get() const {
        if (isEnum()) {
            QoreValue base = getEnumBaseValue();
            return base.get<T>();
        }
        if (!isPointer()) {
            checkShortStringAccess<T>();
            return nullptr;
        }
        assert(!getPointerUnsafe() || dynamic_cast<T*>(const_cast<AbstractQoreNode*>(getPointerUnsafe())));
        return reinterpret_cast<T*>(const_cast<AbstractQoreNode*>(getPointerUnsafe()));
    }

    //! Returns the value as the given type with conversion (specialization for int64)
    template<typename T>
    DLLLOCAL typename std::enable_if<std::is_same<T, int64>::value, T>::type get() const {
        return getAsBigInt();
    }

    //! Returns the value as the given type with conversion (specialization for double)
    template<typename T>
    DLLLOCAL typename std::enable_if<std::is_same<T, double>::value, T>::type get() const {
        return getAsFloat();
    }

    //! Returns the value as the given type with conversion (specialization for bool)
    template<typename T>
    DLLLOCAL typename std::enable_if<std::is_same<T, bool>::value, T>::type get() const {
        return getAsBool();
    }

    // ========================================================================
    // String conversion
    // ========================================================================

    //! Appends string representation to the given QoreString
    DLLEXPORT int getAsString(QoreString& str, int format_offset, ExceptionSink* xsink) const;

    //! Returns string representation
    DLLEXPORT QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;

    // ========================================================================
    // Evaluation
    // ========================================================================

    //! Evaluates the value and returns the result
    DLLEXPORT QoreValue eval(ExceptionSink* xsink) const;

    //! Evaluates the value and returns the result with deref flag
    DLLEXPORT QoreValue eval(bool& needs_deref, ExceptionSink* xsink) const;

    //! Evaluates the value with RuntimeConfig and returns the result with deref flag
    DLLLOCAL QoreValue eval(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const;

    // ========================================================================
    // Type information
    // ========================================================================

    //! Returns the QoreTypeInfo for the value
    DLLEXPORT const QoreTypeInfo* getTypeInfo() const;

    //! Returns the full QoreTypeInfo for the value
    DLLEXPORT const QoreTypeInfo* getFullTypeInfo() const;

    //! Returns the full type name
    DLLEXPORT const char* getFullTypeName() const;

    //! Returns the full type name with optional namespace paths
    DLLEXPORT const char* getFullTypeName(bool with_namespaces) const;

    //! Returns the full type name with optional namespace paths and scratch buffer
    DLLEXPORT const char* getFullTypeName(bool with_namespaces, QoreString& scratch) const;

    // ========================================================================
    // Operators
    // ========================================================================

    //! Assignment operator (trivial - just copies bits)
    QoreValue& operator=(const QoreValue& n) noexcept = default;

    //! Returns true if the value is neither NOTHING nor NULL
    DLLLOCAL explicit operator bool() const { return bits != VAL_NOTHING && bits != VAL_NULL; }

    // ========================================================================
    // Raw access (for debugging and low-level operations)
    // ========================================================================

    //! Returns the raw 64-bit representation
    DLLLOCAL uint64_t rawBits() const { return bits; }

    // ========================================================================
    // Static factory methods for special values
    // ========================================================================

    //! Creates a NOTHING value
    DLLLOCAL static QoreValue makeNothing() { QoreValue v; v.bits = VAL_NOTHING; return v; }

    //! Creates a NULL value
    DLLLOCAL static QoreValue makeNull() { QoreValue v; v.bits = VAL_NULL; return v; }

    //! Creates a TRUE value
    DLLLOCAL static QoreValue makeTrue() { QoreValue v; v.bits = VAL_TRUE; return v; }

    //! Creates a FALSE value
    DLLLOCAL static QoreValue makeFalse() { QoreValue v; v.bits = VAL_FALSE; return v; }

    //! Creates a TAG_ENUM value from an enum member pointer (zero allocation, zero ref-counting)
    DLLLOCAL static QoreValue makeEnum(const QoreEnumMember* member) {
        QoreValue v;
        v.bits = TAG_ENUM | (reinterpret_cast<uint64_t>(member) & PAYLOAD_MASK);
        return v;
    }

    //! Extracts the QoreEnumMember pointer from a TAG_ENUM value (asserts if not an enum)
    DLLLOCAL const QoreEnumMember* getEnumMember() const {
        assert(isEnum());
        return reinterpret_cast<const QoreEnumMember*>(payload());
    }

    //! Returns the base value of a TAG_ENUM value (defined in QoreValue.cpp to avoid incomplete type)
    DLLEXPORT QoreValue getEnumBaseValue() const;

    // ========================================================================
    // Integer range constants
    // ========================================================================

    //! Maximum bytes for inline short string storage
    /** @see QoreStringDataHelper for representation-independent access to Qore string values
    */
    static constexpr size_t SHORTSTR_MAX_BYTES = 6;

    //! Minimum value that can be stored inline (larger values use QoreBigIntNode)
    static constexpr int64 InlineIntMin = INT48_MIN;

    //! Maximum value that can be stored inline (larger values use QoreBigIntNode)
    static constexpr int64 InlineIntMax = INT48_MAX;

    //! Returns true if the given integer fits in inline storage
    DLLLOCAL static bool fitsInline(int64 i) {
        return i >= INT48_MIN && i <= INT48_MAX;
    }
};

// Compile-time size verification
static_assert(sizeof(QoreValue) == 8, "QoreValue must be exactly 8 bytes");

// ============================================================================
// QoreSimpleValue inline implementations (must be after QoreValue is defined)
// ============================================================================

inline QoreSimpleValue::QoreSimpleValue(const QoreValue& v) noexcept : bits(v.rawBits()) {}

inline QoreSimpleValue& QoreSimpleValue::operator=(const QoreValue& v) noexcept {
    bits = v.rawBits();
    return *this;
}

// ============================================================================
// RAII Helper Classes
// ============================================================================

//! Base class for holding a QoreValue object
class ValueHolderBase {
protected:
    //! the value held
    QoreValue v{};
    //! for possible Qore-language exceptions
    ExceptionSink* xsink;

public:
    //! creates an empty object
    DLLLOCAL ValueHolderBase(ExceptionSink* xs) : v(), xsink(xs) {
    }

    //! creates the object with the given value
    DLLLOCAL ValueHolderBase(QoreValue n_v, ExceptionSink* xs) : v(n_v), xsink(xs) {
    }

    //! returns the value being managed
    DLLLOCAL QoreValue* operator->() { return &v; }

    //! returns the value being managed
    DLLLOCAL const QoreValue* operator->() const { return &v; }

    //! returns the value being managed
    DLLLOCAL QoreValue& operator*() { return v; }

    //! returns the value being managed
    DLLLOCAL const QoreValue& operator*() const { return v; }
};

//! holds an object and dereferences it in the destructor
class ValueHolder : public ValueHolderBase {
public:
    //! creates an empty object
    DLLLOCAL ValueHolder(ExceptionSink* xs) : ValueHolderBase(xs) {
    }

    //! creates the object with the given value
    DLLLOCAL ValueHolder(QoreValue n_v, ExceptionSink* xs) : ValueHolderBase(n_v, xs) {
    }

    //! dereferences any contained node
    DLLEXPORT ~ValueHolder();

    //! returns a referenced value; caller owns the reference; the current object is left undisturbed
    DLLEXPORT QoreValue getReferencedValue();

    //! returns a QoreValue object and leaves the current object empty; the caller owns any reference contained in the return value
    DLLEXPORT QoreValue release();

    //! returns a pointer to a value of the given class and leaves the current object empty
    template<typename T>
    DLLLOCAL T* releaseAs() {
        T* rv = v.get<T*>();
        release();
        return rv;
    }

    //! assigns the object, any currently-held value is dereferenced before the assignment
    DLLLOCAL QoreValue& operator=(QoreValue nv) {
        v.discard(xsink);
        v = nv;
        return v;
    }

    //! returns true if the value is neither NOTHING nor NULL
    DLLLOCAL operator bool() const {
        return (bool)v;
    }
};

//! allows storing a value and setting a boolean flag that indicates if the value should be dereference in the destructor or not
class ValueOptionalRefHolder : public ValueHolderBase {
private:
    // not implemented
    DLLLOCAL QoreValue& operator=(QoreValue& nv);

protected:
    //! flag indicating if the value should be dereferenced in the destructor or not
    bool needs_deref;

public:
    //! creates the object with the given values
    DLLLOCAL ValueOptionalRefHolder(QoreValue n_v, bool nd, ExceptionSink* xs) : ValueHolderBase(n_v, xs), needs_deref(nd) {
    }

    //! creates an empty object
    DLLLOCAL ValueOptionalRefHolder(ExceptionSink* xs) : ValueHolderBase(xs), needs_deref(false) {
    }

    DLLEXPORT ~ValueOptionalRefHolder();

    //! returns true if the value is temporary (needs dereferencing)
    DLLLOCAL bool isTemp() const { return needs_deref; }

    //! sets needs_deref = false
    DLLLOCAL void clearTemp() {
        if (needs_deref)
            needs_deref = false;
    }

    //! returns true if the value is neither NOTHING nor NULL
    DLLLOCAL operator bool() const {
        return (bool)v;
    }

    //! assigns a new non-temporary value
    DLLLOCAL void setValue(QoreValue nv) {
        if (needs_deref) {
            v.discard(xsink);
            needs_deref = false;
        }
        v = nv;
    }

    //! assigns a new value
    DLLLOCAL void setValue(QoreValue nv, bool temp) {
        if (needs_deref)
            v.discard(xsink);
        if (needs_deref != temp)
            needs_deref = temp;
        v = nv;
    }

    // ensures that the held value is referenced
    DLLEXPORT void ensureReferencedValue();

    //! returns the stored node value and leaves the current object empty
    template<typename T>
    DLLLOCAL T* takeReferencedNode() {
        T* rv = v.take<T>();
        if (needs_deref)
            needs_deref = false;
        else
            rv->ref();

        return rv;
    }

    //! returns a referenced value; caller owns the reference; the current object is not disturbed
    DLLEXPORT QoreValue getReferencedValue();

    //! returns the stored AbstractQoreNode pointer and sets the dereference flag as an output variable
    DLLLOCAL AbstractQoreNode* takeNode(bool& nd) {
        if (v.hasNode()) {
            nd = needs_deref;
            return v.takeNodeIntern();
        }
        nd = true;
        return v.takeNode();
    }

    //! returns the stored value and sets the dereference flag as an output variable
    DLLLOCAL QoreValue takeValue(bool& nd) {
        if (v.hasNode()) {
            nd = needs_deref;
            return v.takeNodeIntern();
        }
        nd = false;
        return v;
    }

    //! returns the stored value which must be dereferenced if it is a node object
    DLLLOCAL void takeValueFrom(ValueOptionalRefHolder& val) {
        if (needs_deref)
            v.discard(xsink);
        v = val.takeValue(needs_deref);
    }

    //! returns a QoreValue after incrementing the reference count of any node value stored if necessary
    DLLEXPORT QoreValue takeReferencedValue();
};

//! evaluates an AbstractQoreNode and dereferences the stored value in the destructor
class ValueEvalRefHolder : public ValueOptionalRefHolder {
public:
    //! creates the object with with no evaluation
    DLLEXPORT ValueEvalRefHolder(ExceptionSink* xs);

    //! evaluates the exp argument (gets RuntimeConfig from TLS)
    DLLEXPORT ValueEvalRefHolder(const AbstractQoreNode* exp, ExceptionSink* xs);

    //! evaluates the exp argument (gets RuntimeConfig from TLS)
    DLLEXPORT ValueEvalRefHolder(const QoreValue exp, ExceptionSink* xs);

    //! evaluates the exp argument with RuntimeConfig
    DLLLOCAL ValueEvalRefHolder(RuntimeConfig& rc, const AbstractQoreNode* exp, ExceptionSink* xs);

    //! evaluates the exp argument with RuntimeConfig
    DLLLOCAL ValueEvalRefHolder(RuntimeConfig& rc, const QoreValue exp, ExceptionSink* xs);

    //! evaluates the argument (gets RuntimeConfig from TLS), returns -1 for error, 0 = OK
    DLLEXPORT int eval(const AbstractQoreNode* exp);

    //! evaluates the argument (gets RuntimeConfig from TLS), returns -1 for error, 0 = OK
    DLLEXPORT int eval(const QoreValue exp);

    //! evaluates the argument with RuntimeConfig, returns -1 for error, 0 = OK
    DLLLOCAL int eval(RuntimeConfig& rc, const AbstractQoreNode* exp);

    //! evaluates the argument with RuntimeConfig, returns -1 for error, 0 = OK
    DLLLOCAL int eval(RuntimeConfig& rc, const QoreValue exp);

protected:
    //! evaluates the argument with RuntimeConfig, returns -1 for error, 0 = OK
    DLLLOCAL int evalIntern(RuntimeConfig& rc, const AbstractQoreNode* exp);

    //! evaluates the argument with RuntimeConfig, returns -1 for error, 0 = OK
    DLLLOCAL int evalIntern(RuntimeConfig& rc, const QoreValue& exp);
};

//! evaluates an AbstractQoreNode and dereferences the stored value in the destructor
class ValueEvalOptimizedRefHolder : public ValueEvalRefHolder {
public:
    //! evaluates the exp argument
    DLLEXPORT ValueEvalOptimizedRefHolder(const QoreValue& exp, ExceptionSink* xs);

    //! creates the object with with no evaluation
    DLLEXPORT ValueEvalOptimizedRefHolder(ExceptionSink* xs);

    //! evaluates the argument, returns -1 for error, 0 = OK
    DLLEXPORT int eval(const QoreValue& exp);
};

//! "bool"
DLLEXPORT extern const char* qoreBoolTypeName;
//! "int"
DLLEXPORT extern const char* qoreIntTypeName;
//! "float"
DLLEXPORT extern const char* qoreFloatTypeName;
//! "char"
DLLEXPORT extern const char* qoreCharTypeName;

#endif // _QORE_QOREVALUE_H
