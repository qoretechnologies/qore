/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreStatementTemps.h

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

#ifndef _QORE_INTERN_QORESTATEMENTTEMPS_H
#define _QORE_INTERN_QORESTATEMENTTEMPS_H

#include <unordered_map>
#include <memory>
#include <vector>

//! Holds the temporary values that the AST interpreter keeps until the end of a statement
/** A temporary that the code of a statement reads from - the object or value whose member, element or method a
    chain such as <tt>f().a.b()</tt> uses, or that a cast converts - lives until the end of the statement that
    created it, so that every part of the chain can use it; the IR interpreter and native code keep it in the temp
    scope of the statement (QoreIROpcode::PushTempMark / QoreIROpcode::DiscardTemps).

    A scope is created for each statement executed (AbstractStatement::exec()), and for each part of a statement
    that the other execution modes evaluate as a statement of its own: a condition, the initializer and the
    iterator of a for statement, the list of a foreach statement, the value of a switch statement, the operand of a
    change to a container, and each element of a map whose value is not used.

    A temporary is held by the innermost scope of the thread.  An expression evaluated again in the same scope, as
    the expression of a map or select is for each element, also when a foreach statement gets the elements one by one,
    replaces the temporary that it held: the previous one is released when the new one is created, as the other
    execution modes release the previous value of the same instruction.  When the scope ends, its temporaries are
    released in the reverse order of their creation.
*/
class QoreStatementTempScope {
public:
    //! creates the scope and makes it the innermost scope of the thread
    /** @param xsink the exception sink for exceptions raised by the destructors that releasing the temporaries runs
    */
    DLLLOCAL explicit QoreStatementTempScope(ExceptionSink* xsink);

    //! releases the temporaries held and restores the enclosing scope
    DLLLOCAL ~QoreStatementTempScope();

    //! releases the temporaries held so far, in the reverse order of their creation; the scope stays active
    DLLLOCAL void release();

    //! hands the temporary value of a holder to the innermost scope of the thread
    /** The holder keeps the value but no longer owns it; nothing is done if the holder does not own its value
        (it was read from a variable, for example), if the value cannot be referred to after the statement - a
        string, a number, a binary or a date, which hold no other values - or if there is no scope, in which case
        the holder releases the value as before.

        @param site the expression that created the value; a value created again by the same expression in the same
        scope replaces the previous one, which is released
        @param holder the holder of the value
        @param xsink for exceptions raised by the destructor of a value replaced
    */
    DLLLOCAL static void hold(const void* site, ValueOptionalRefHolder& holder, ExceptionSink* xsink);

    //! hands the value that a holder got by evaluating an expression to the innermost scope of the thread
    /** As hold(), except that nothing is done for a variable: its value is not a temporary, and a value read from a
        variable is not kept after the variable is assigned another value in the same statement, as in the other
        execution modes

        @param site the expression whose value the holder has (the receiver of a chain); see hold()
    */
    DLLLOCAL static void holdReceiver(const QoreValue& site, ValueOptionalRefHolder& holder, ExceptionSink* xsink) {
        if (site.getType() == NT_VARREF) {
            return;
        }
        hold(site.getInternalNode(), holder, xsink);
    }

    //! hands a value that the caller owns to the innermost scope of the thread
    /** @return true if the scope took the value; false if there is no scope, in which case the caller still owns it
    */
    DLLLOCAL static bool hold(const void* site, QoreValue value, ExceptionSink* xsink);

    //! returns true if the value has a lifetime that code can observe after the statement
    DLLLOCAL static bool isObservable(const QoreValue& value) {
        if (!value.hasNode()) {
            return false;
        }
        switch (value.getType()) {
            case NT_STRING:
            case NT_BINARY:
            case NT_NUMBER:
            case NT_DATE:
                return false;
            default:
                return true;
        }
    }

private:
    struct Entry {
        const void* site;
        QoreValue value;
    };

    //! the number of temporaries above which the sites are looked up in an index instead of by a scan
    static constexpr size_t IndexThreshold = 16;

    //! the enclosing scope
    QoreStatementTempScope* prev;
    //! the exception sink for the destructors run by releasing the temporaries
    ExceptionSink* xsink;
    //! the temporaries, in the order of their creation
    std::vector<Entry> entries;
    //! site -> position in entries, for a statement that holds many temporaries (a large hash literal with a chain
    //! in each value, for example), so that each hold does not scan them all
    std::unique_ptr<std::unordered_map<const void*, size_t>> site_index;

    //! returns the position of the entry of the site, or entries.size() if there is none
    DLLLOCAL size_t find(const void* site);

    //! puts the value at the end of the entries; returns the value that the site held before, if any
    DLLLOCAL QoreValue put(const void* site, QoreValue value);

    DLLLOCAL QoreStatementTempScope(const QoreStatementTempScope&) = delete;
    DLLLOCAL QoreStatementTempScope& operator=(const QoreStatementTempScope&) = delete;
};

//! returns true if the operand of a change to an lvalue is evaluated as a statement of its own
/** The temporaries created by such an operand are released before the change, so that they cannot keep a
    container that the change makes shared and copy it; an assignment to a variable changes no container, nor does
    a change to a variable whose value holds no reference, so their operands keep their temporaries until the end of
    the statement, as the other execution modes do (see QoreIRLowering::lowerMutationOperand())

    @param lvalue the lvalue changed
    @param assignment true for an assignment, false for a compound assignment, push or unshift
*/
DLLLOCAL inline bool qore_mutation_operand_has_own_scope(const QoreValue& lvalue, bool assignment) {
    if (lvalue.getType() != NT_VARREF) {
        return true;
    }
    if (assignment) {
        return false;
    }
    const QoreTypeInfo* type_info = lvalue.get<const VarRefNode>()->getTypeInfo();
    return !(QoreTypeInfo::isType(type_info, NT_BOOLEAN) || QoreTypeInfo::isType(type_info, NT_INT)
        || QoreTypeInfo::isType(type_info, NT_FLOAT) || QoreTypeInfo::isType(type_info, NT_CHAR)
        || QoreTypeInfo::isType(type_info, NT_NOTHING) || QoreTypeInfo::isType(type_info, NT_NULL));
}

//! evaluates an expression, as a statement of its own if requested: its temporaries are released when it returns
class QoreScopedEvalHolder : public ValueEvalOptimizedRefHolder {
public:
    //! evaluates @p exp; if @p own_scope, the temporaries that it creates are released before this returns
    DLLLOCAL QoreScopedEvalHolder(const QoreValue& exp, bool own_scope, ExceptionSink* xsink)
            : ValueEvalOptimizedRefHolder(xsink) {
        if (own_scope) {
            QoreStatementTempScope temps(xsink);
            eval(exp);
        } else {
            eval(exp);
        }
    }
};

//! evaluates the operand of a change to an lvalue, as a statement of its own where the other modes do
class QoreMutationOperandHolder : public QoreScopedEvalHolder {
public:
    //! evaluates @p exp, the operand of a change to @p lvalue; see qore_mutation_operand_has_own_scope()
    DLLLOCAL QoreMutationOperandHolder(const QoreValue& lvalue, const QoreValue& exp, bool assignment,
            ExceptionSink* xsink)
            : QoreScopedEvalHolder(exp, qore_mutation_operand_has_own_scope(lvalue, assignment), xsink) {
    }
};

//! evaluates a condition as a statement of its own
/** The value of the condition and the temporaries created by it are released before the code it selects runs, as
    in the other execution modes

    @return 0 if the condition was evaluated, -1 if an exception was raised
*/
template <typename... RcArgs>
DLLLOCAL int qore_eval_condition(const QoreValue& cond, bool& result, ExceptionSink* xsink, RcArgs&... rc) {
    QoreStatementTempScope temps(xsink);
    {
        ValueEvalRefHolder val(rc..., cond, xsink);
        if (*xsink) {
            return -1;
        }
        result = val->getAsBool();
    }
    temps.release();
    return *xsink ? -1 : 0;
}

#endif
