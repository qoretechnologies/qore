/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreAssignmentOperatorNode.h

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

#ifndef _QORE_QOREASSIGNMENTOPERATORNODE_H
#define _QORE_QOREASSIGNMENTOPERATORNODE_H

class QoreAssignmentOperatorNode : public QoreBinaryLValueOperatorNode {
OP_COMMON
public:
    DLLLOCAL QoreAssignmentOperatorNode(const QoreProgramLocation* loc, QoreValue left, QoreValue right) :
            QoreBinaryLValueOperatorNode(loc, left, right) {
    }

    DLLLOCAL virtual QoreOperatorNode* copyBackground(ExceptionSink* xsink) const {
        return copyBackgroundExplicit<QoreAssignmentOperatorNode>(xsink);
    }

protected:
    // to support "broken-int-assignments"
    bool broken_int = false;
    // flag for identical match with assignment types (lvalue & rvalue)
    bool ident = false;

    DLLLOCAL int parseInitIntern(QoreParseContext& parse_context, AssignmentMode mode);

    DLLLOCAL int parseInitMode(QoreValue& val, QoreParseContext& parse_context, AssignmentMode mode);

    DLLLOCAL QoreValue evalLexical(ExceptionSink* xsink, bool& needs_deref, AssignmentMode mode) const;

    template <bool Lexical>
    DLLLOCAL QoreValue evalInternImpl(ExceptionSink* xsink, bool& needs_deref, AssignmentMode mode) const;

    DLLLOCAL QoreValue evalIntern(ExceptionSink* xsink, bool& needs_deref, AssignmentMode mode) const;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
        return parseInitMode(val, parse_context, AssignmentMode::Normal);
    }

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
        return evalIntern(xsink, needs_deref, AssignmentMode::Normal);
    }
};

//! returns true if a weak assignment from a variable of the given declared type makes the variable's container unique
/** the variable must be declared with a type that can be a hash or a list and cannot be an object: the other
    variables (untyped, object, scalar) are not touched, so that the weak assignment of an object, whose lifetime the
    execution modes manage by the way the variable is accessed, is not changed (qore#5756)
*/
static inline bool qore_weak_assignment_source_made_unique(const QoreTypeInfo* ti) {
    return QoreTypeInfo::hasType(ti)
        && QoreTypeInfo::parseReturns(ti, NT_OBJECT) == QTI_NOT_EQUAL
        && (QoreTypeInfo::parseReturns(ti, NT_HASH) != QTI_NOT_EQUAL
            || QoreTypeInfo::parseReturns(ti, NT_LIST) != QTI_NOT_EQUAL);
}

class QoreWeakAssignmentOperatorNode : public QoreAssignmentOperatorNode {
OP_COMMON
public:
    DLLLOCAL QoreWeakAssignmentOperatorNode(const QoreProgramLocation* loc, QoreValue left, QoreValue right)
            : QoreAssignmentOperatorNode(loc, left, right) {
    }

    DLLLOCAL virtual QoreOperatorNode* copyBackground(ExceptionSink* xsink) const {
        return copyBackgroundExplicit<QoreWeakAssignmentOperatorNode>(xsink);
    }

protected:
    DLLLOCAL int parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
        return parseInitMode(val, parse_context, AssignmentMode::Weak);
    }

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
        return evalIntern(xsink, needs_deref, AssignmentMode::Weak);
    }
};

//! the opaque reference assignment operator ('@=')
/** Assigns a strong reference that the deterministic garbage collector does not follow.  Unlike the
    weak assignment operator, the target is owned, so it cannot be collected while the holder holds
    it; unlike an ordinary assignment, the edge is invisible to cycle detection, so a cycle running
    through it can only be broken by the holder releasing it or by QoreProgram teardown.
*/
class QoreOpaqueAssignmentOperatorNode : public QoreAssignmentOperatorNode {
OP_COMMON
public:
    DLLLOCAL QoreOpaqueAssignmentOperatorNode(const QoreProgramLocation* loc, QoreValue left, QoreValue right)
            : QoreAssignmentOperatorNode(loc, left, right) {
    }

    DLLLOCAL virtual QoreOperatorNode* copyBackground(ExceptionSink* xsink) const {
        return copyBackgroundExplicit<QoreOpaqueAssignmentOperatorNode>(xsink);
    }

protected:
    DLLLOCAL int parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
        return parseInitMode(val, parse_context, AssignmentMode::Opaque);
    }

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
        return evalIntern(xsink, needs_deref, AssignmentMode::Opaque);
    }
};

//! Selected during parse initialization; canonical assignment nodes keep their original execution entry.
template <typename Base, AssignmentMode Mode>
class LexicalAssignmentOperatorNode : public Base {
public:
    using Base::Base;

    DLLLOCAL QoreOperatorNode* copyBackground(ExceptionSink* xsink) const override {
        auto* copy = this->template copyBackgroundExplicit<LexicalAssignmentOperatorNode>(xsink);
        if (copy) {
            copy->broken_int = this->broken_int;
            copy->ident = this->ident;
        }
        return copy;
    }

protected:
    DLLLOCAL QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const override {
        return this->evalLexical(xsink, needs_deref, Mode);
    }
};

#endif
