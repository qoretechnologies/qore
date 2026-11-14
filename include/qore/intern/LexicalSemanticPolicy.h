/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    LexicalSemanticPolicy.h

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

#ifndef _QORE_LEXICALSEMANTICPOLICY_H
#define _QORE_LEXICALSEMANTICPOLICY_H

#include <qore/QoreParseOptions.h>
#include <qore/Restrictions.h>

class QoreValue;
class QoreProgram;

//! Immutable semantic projection owned by parsed code, independent of Program authority and current options.
/** Constructed while the operation's parse options are installed. Zero is the complete default policy, including
    when no Program exists. Values, public type descriptors and native compiled operations do not carry this policy.
*/
class LexicalSemanticPolicy {
public:
    constexpr LexicalSemanticPolicy() = default;

    explicit LexicalSemanticPolicy(const QoreParseOptions& po)
        : bits((po.hasAny(PO_BROKEN_RANGE) ? BrokenRange : 0)
            | (po.hasAny(PO_BROKEN_LIST_RANGE) ? BrokenListRange : 0)
            | (po.hasAny(PO_STRICT_BOOLEAN_EVAL) ? StrictBoolean : 0)
            | (po.hasAny(PO_BROKEN_SPRINTF) ? BrokenSprintf : 0)
            | (po.hasAny(PO_BROKEN_NAMESPACE_RESOLUTION) ? BrokenNamespace : 0)
            | (po.hasAny(PO_BROKEN_LOOP_STATEMENT) ? BrokenLoop : 0)
            | (po.hasAny(QoreParseOptions::BROKEN_SOFT_TYPES) ? BrokenSoftTypes : 0)) {
    }

    constexpr bool isDefault() const { return !bits; }
    constexpr bool brokenRange() const { return bits & BrokenRange; }
    constexpr bool brokenListRange() const { return bits & BrokenListRange; }
    constexpr bool strictBoolean() const { return bits & StrictBoolean; }
    constexpr bool brokenSprintf() const { return bits & BrokenSprintf; }
    constexpr bool brokenNamespace() const { return bits & BrokenNamespace; }
    constexpr bool brokenLoop() const { return bits & BrokenLoop; }
    constexpr bool brokenSoftTypes() const { return bits & BrokenSoftTypes; }
    constexpr bool needsFilter() const { return bits & (StrictBoolean | BrokenSoftTypes); }

    //! Internal RuntimeConfig projection; never exposed as public QCF flags or serialized.
    static constexpr unsigned NativeShift = 48;
    static constexpr uint64_t NativeMask = uint64_t{0x7f} << NativeShift;

    constexpr uint64_t nativeFlags() const { return uint64_t{bits} << NativeShift; }

    static constexpr LexicalSemanticPolicy fromNativeFlags(uint64_t flags) {
        LexicalSemanticPolicy policy;
        policy.bits = (flags & NativeMask) >> NativeShift;
        return policy;
    }

    //! Converts a value at this lexical operation, without consulting mutable Program options.
    DLLLOCAL bool getAsBool(const QoreValue& value) const;

    //! Canonical conversion for compiled code and default filters; has no compatibility argument or branch.
    DLLLOCAL static bool getAsBoolDefault(const QoreValue& value);

    //! Strict conversion for parse-selected legacy condition entries; no runtime policy selection.
    DLLLOCAL static bool getAsBoolStrict(const QoreValue& value);

    //! Compile-time conversion entry for AST nodes and iterators selected while parsing.
    template <bool StrictBoolean>
    DLLLOCAL static bool getAsBoolFor(const QoreValue& value) {
        if constexpr (StrictBoolean) {
            return getAsBoolStrict(value);
        } else {
            return getAsBoolDefault(value);
        }
    }

    constexpr bool operator==(LexicalSemanticPolicy other) const { return bits == other.bits; }
    constexpr bool operator!=(LexicalSemanticPolicy other) const { return bits != other.bits; }

private:
    enum : unsigned char {
        BrokenRange = 1,
        BrokenListRange = 1 << 1,
        StrictBoolean = 1 << 2,
        BrokenSprintf = 1 << 3,
        BrokenNamespace = 1 << 4,
        BrokenLoop = 1 << 5,
        BrokenSoftTypes = 1 << 6,
    };
    unsigned char bits = 0;
};

//! Parse-only context for nodes constructed by a grammar reduction or a parse-time replacement.
/** Token provenance supplies the parser's mutable source; parse initialization supplies an owned copy of the
    original operation's policy. This never installs execution context and adds no runtime call-path work.
*/
class LexicalParsePolicyHelper {
public:
    DLLLOCAL explicit LexicalParsePolicyHelper(LexicalSemanticPolicy policy);
    DLLLOCAL explicit LexicalParsePolicyHelper(const LexicalSemanticPolicy* policy);
    DLLLOCAL ~LexicalParsePolicyHelper();

    //! Returns false outside the owning Program's parse context; a present zero policy remains bound.
    DLLLOCAL static bool capture(LexicalSemanticPolicy& policy);

    LexicalParsePolicyHelper(const LexicalParsePolicyHelper&) = delete;
    LexicalParsePolicyHelper& operator=(const LexicalParsePolicyHelper&) = delete;

private:
    LexicalSemanticPolicy owned_policy;
    const LexicalSemanticPolicy* policy;
    QoreProgram* pgm;
    LexicalParsePolicyHelper* previous;
};

//! Policy of the active native invocation; absent a source-bound native entry, the result is canonical.
DLLEXPORT LexicalSemanticPolicy qore_get_native_semantic_policy();

#endif
