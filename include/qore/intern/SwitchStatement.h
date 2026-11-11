/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SwitchStatement.h

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

#ifndef _QORE_SWITCHSTATEMENT_H

#define _QORE_SWITCHSTATEMENT_H

#include "qore/intern/AbstractStatement.h"
#include "qore/intern/StatementBlock.h"

class RuntimeConfig;

DLLLOCAL bool qore_switch_case_equal(QoreValue lhs_value, QoreValue rhs_value, ExceptionSink* xsink);

//! The value of a switch on a string value whose cases are ASCII strings without null bytes, compared with the cases as
//! qore_switch_case_equal() compares them
/** The compiled tiers dispatch such a switch (QoreIRSwitchString) with this comparison: a case is the value if they
    have the same characters, in any encoding of the value.  A value in an encoding in which every ASCII character is
    known to be the same byte (qore_encoding_private::isKnownAsciiIdentical()) has the length and the bytes of the case
    then; any other value is compared with the case converted to its encoding (QoreString::equalSoft()), as the AST
    interpreter compares it: the same bytes are other characters in some encodings that are otherwise ASCII-compatible
    (0x5c is U+00E7 in ISO646-FR), and text in an encoding that iconv does not know cannot be converted, which means
    that it is not the case.  The value's data and encoding are got once, for all the cases of a lookup.

    A comparison never raises an exception: as with QoreValue::isEqualHard(), which compares a plain case in the AST
    interpreter, a conversion error means that the value is not the case, so a lookup needs no exception check and its
    instruction no exception target.
*/
class QoreSwitchStringValue {
public:
    //! For a value of any type; a value that is not a string (after an enum value is unwrapped) matches no case
    /** an inline short string is read into this object, without a string node
    */
    DLLLOCAL QoreSwitchStringValue(QoreValue v);

    //! For a string
    DLLLOCAL QoreSwitchStringValue(const QoreString& value);

    //! a copy's data could be the original's inline short string
    QoreSwitchStringValue(const QoreSwitchStringValue&) = delete;
    QoreSwitchStringValue& operator=(const QoreSwitchStringValue&) = delete;

    //! Returns true if the value is the case
    /** @param case_str the case: ASCII characters without a null byte
        @param case_len the length of the case in bytes
    */
    DLLLOCAL bool equals(const char* case_str, size_t case_len) const {
        assert(case_str);
        if (ascii_identical) {
            return len == case_len && bytesEqual(buf, case_str, len);
        }
        return str && equalsConverted(case_str, case_len);
    }

private:
    //! the string value; null for an inline short string and for a value that is not a string
    const QoreString* str = nullptr;
    //! the data of the value; null if it is not a string
    const char* buf = nullptr;
    size_t len = 0;
    //! true if every ASCII character is known to be the same byte in the value's encoding; false if it is not a string
    bool ascii_identical = false;
    //! the data of an inline short string
    char short_buf[QoreValue::SHORTSTR_MAX_BYTES + 1];

    //! Sets the data of a string
    DLLLOCAL void setString(const QoreString& value);

    //! Returns true if \a n bytes at \a a and \a b are the same
    /** cases are short: a string of up to 16 bytes is compared inline as two words that overlap for a length that is
        not twice the word's size, which is faster than a call to memcmp()
    */
    DLLLOCAL static bool bytesEqual(const char* a, const char* b, size_t n) {
        if (n >= 8) {
            if (n > 16) {
                return !memcmp(a, b, n);
            }
            uint64_t a0, b0, a1, b1;
            memcpy(&a0, a, 8);
            memcpy(&b0, b, 8);
            memcpy(&a1, a + n - 8, 8);
            memcpy(&b1, b + n - 8, 8);
            return a0 == b0 && a1 == b1;
        }
        if (n >= 4) {
            uint32_t a0, b0, a1, b1;
            memcpy(&a0, a, 4);
            memcpy(&b0, b, 4);
            memcpy(&a1, a + n - 4, 4);
            memcpy(&b1, b + n - 4, 4);
            return a0 == b0 && a1 == b1;
        }
        for (size_t i = 0; i < n; ++i) {
            if (a[i] != b[i]) {
                return false;
            }
        }
        return true;
    }

    //! Compares a value with the case converted to its encoding
    /** an encoding conversion error means that the strings are not equal
    */
    DLLLOCAL bool equalsConverted(const char* case_str, size_t case_len) const;
};

class CaseNode {
private:
    DLLLOCAL virtual bool isCaseNodeImpl() const;

public:
    const QoreProgramLocation* loc;
    QoreValue val{};
    StatementBlock* code;
    CaseNode* next = nullptr;
    bool def;

    DLLLOCAL CaseNode(const QoreProgramLocation* loc, QoreValue v, StatementBlock* c, bool def = false)
            : loc(loc), val(v), code(c), def(def) {
    }

    DLLLOCAL virtual ~CaseNode();

    DLLLOCAL virtual bool matches(QoreValue lhs_value, ExceptionSink* xsink) const;

    DLLLOCAL virtual bool isDefault() const {
        return def;
    }

    DLLLOCAL bool isCaseNode() const;
};

class SwitchStatement : public AbstractStatement {
public:
    LVList* lvars = nullptr;

    // start and end line are set later
    DLLLOCAL SwitchStatement(CaseNode* f);
    DLLLOCAL virtual ~SwitchStatement();
    DLLLOCAL void setSwitch(QoreValue s);
    DLLLOCAL void addCase(CaseNode* c);

    DLLLOCAL QoreValue getSwitchExp() const {
        return sexp;
    }

    DLLLOCAL const CaseNode* getCases() const {
        return head;
    }

    // fake it here and let it be checked at runtime
    DLLLOCAL virtual bool hasFinalReturn() const {
        return true;
    }
    DLLLOCAL virtual void parseCommit(QoreProgram* pgm);

private:
    CaseNode* head = nullptr, *tail = nullptr;
    QoreValue sexp{};
    CaseNode* deflt = nullptr;

    DLLLOCAL virtual int parseInitImpl(QoreParseContext& parse_context0);
    DLLLOCAL virtual int execImpl(QoreValue& return_value, ExceptionSink* xsink);
    DLLLOCAL virtual int execImpl(RuntimeConfig& rc, QoreValue& return_value, ExceptionSink* xsink);
};

#endif
