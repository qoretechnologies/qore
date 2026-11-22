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
    //! whether a weak assignment makes the container of its source unique (see
    //! qore_weak_assignment_source_made_unique()), found when it is first evaluated: 0 not known yet, 1 no, 2 yes
    //! (qore#7242); the source does not change after the parse
    mutable std::atomic<signed char> weak_source_unique{0};

    //! returns true if a weak assignment makes the container of its source unique (see weak_source_unique)
    DLLLOCAL bool weakSourceMadeUnique() const;

    DLLLOCAL int parseInitIntern(QoreParseContext& parse_context, AssignmentMode mode);

    DLLLOCAL int parseInitMode(QoreValue& val, QoreParseContext& parse_context, AssignmentMode mode);

    DLLLOCAL QoreValue evalLexical(ExceptionSink* xsink, bool& needs_deref, AssignmentMode mode) const;

    //! evaluates the assignment; \a Weak is true for the weak assignment operator, whose entry points
    //! (evalInternWeak(), evalLexicalWeak()) are their own, so that the other assignments keep their code (qore#7085)
    template <bool Lexical, bool Weak>
    DLLLOCAL QoreValue evalInternImpl(ExceptionSink* xsink, bool& needs_deref, AssignmentMode mode) const;

    DLLLOCAL QoreValue evalIntern(ExceptionSink* xsink, bool& needs_deref, AssignmentMode mode) const;

    //! evaluates a weak assignment
    DLLLOCAL QoreValue evalInternWeak(ExceptionSink* xsink, bool& needs_deref) const;

    //! evaluates a weak assignment with lexical semantics
    DLLLOCAL QoreValue evalLexicalWeak(ExceptionSink* xsink, bool& needs_deref) const;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
        return parseInitMode(val, parse_context, AssignmentMode::Normal);
    }

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
        return evalIntern(xsink, needs_deref, AssignmentMode::Normal);
    }
};

//! returns true if a variable of the given declared type can hold a hash or a list
/** A variable declared with a type that excludes both (an int, a string, an object type) is never made unique by a
    weak assignment and is not touched (qore#5847)
*/
static inline bool qore_weak_source_type_may_hold_container(const QoreTypeInfo* ti) {
    return !QoreTypeInfo::hasType(ti)
        || QoreTypeInfo::parseReturns(ti, NT_HASH) != QTI_NOT_EQUAL
        || QoreTypeInfo::parseReturns(ti, NT_LIST) != QTI_NOT_EQUAL;
}

//! returns true if the right side of a weak assignment is a variable or an lvalue path whose container the assignment
//! makes unique
/** the right side must be a variable (a local, closure-bound, thread-safe, global or thread-local variable, or a class
    static variable), whatever its declared type, or a path from one of them or from the object: a member of the
    object, an object member, a hash key or a list element (qore#7242): a weak reference then always designates the
    container of the variable or of the path, never one that it shares with a literal of the program or another
    owner.  A value that is not a hash or a list (an object, a scalar, NOTHING) and a weak reference held by the
    variable or the path are left as they are (qore#5756, qore#5847)
*/
DLLLOCAL bool qore_weak_assignment_source_made_unique(const QoreValue& right);

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
        return evalInternWeak(xsink, needs_deref);
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
// Concrete bases also make every lexical operator visible to the IR operand-slot coverage scan.

class LexicalAssignmentOperatorNode : public QoreAssignmentOperatorNode {
public:
    using QoreAssignmentOperatorNode::QoreAssignmentOperatorNode;

    DLLLOCAL QoreOperatorNode* copyBackground(ExceptionSink* xsink) const override {
        auto* copy = copyBackgroundExplicit<LexicalAssignmentOperatorNode>(xsink);
        if (copy) {
            copy->broken_int = broken_int;
            copy->ident = ident;
        }
        return copy;
    }

protected:
    DLLLOCAL QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const override {
        return evalLexical(xsink, needs_deref, AssignmentMode::Normal);
    }
};

class LexicalWeakAssignmentOperatorNode : public QoreWeakAssignmentOperatorNode {
public:
    using QoreWeakAssignmentOperatorNode::QoreWeakAssignmentOperatorNode;

    DLLLOCAL QoreOperatorNode* copyBackground(ExceptionSink* xsink) const override {
        auto* copy = copyBackgroundExplicit<LexicalWeakAssignmentOperatorNode>(xsink);
        if (copy) {
            copy->broken_int = broken_int;
            copy->ident = ident;
        }
        return copy;
    }

protected:
    DLLLOCAL QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const override {
        return evalLexicalWeak(xsink, needs_deref);
    }
};

class LexicalOpaqueAssignmentOperatorNode : public QoreOpaqueAssignmentOperatorNode {
public:
    using QoreOpaqueAssignmentOperatorNode::QoreOpaqueAssignmentOperatorNode;

    DLLLOCAL QoreOperatorNode* copyBackground(ExceptionSink* xsink) const override {
        auto* copy = copyBackgroundExplicit<LexicalOpaqueAssignmentOperatorNode>(xsink);
        if (copy) {
            copy->broken_int = broken_int;
            copy->ident = ident;
        }
        return copy;
    }

protected:
    DLLLOCAL QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const override {
        return evalLexical(xsink, needs_deref, AssignmentMode::Opaque);
    }
};

#endif
