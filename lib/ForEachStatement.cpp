/*
    ForEachStatement.cpp

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

#include <qore/Qore.h>
#include "qore/intern/QoreStatementTemps.h"
#include "qore/intern/ForEachStatement.h"
#include "qore/intern/StatementBlock.h"
#include "qore/intern/AbstractIteratorHelper.h"
#include "qore/intern/VarRefNode.h"

#include <memory>
#include "qore/intern/QoreDeferredRelease.h"

namespace {
//! sets the thread's implicit index (\c $#) for an element of a foreach statement whose body reads it, and restores
//! the index of the code around the statement when it goes out of scope (qore#7270)
/** A body that reads no implicit index of its own (ForEachStatement::readsImplicitElement()) runs with the index of
    the code around the statement, which nothing in it can read.
*/
class ForEachElementHelper {
public:
    DLLLOCAL ForEachElementHelper(bool set, int n_element) : set(set),
            element(set ? save_implicit_element(n_element) : 0) {
    }

    DLLLOCAL ~ForEachElementHelper() {
        if (set) {
            save_implicit_element(element);
        }
    }

    ForEachElementHelper(const ForEachElementHelper&) = delete;
    ForEachElementHelper& operator=(const ForEachElementHelper&) = delete;

private:
    //! true if the helper set the index
    bool set;
    //! the index that the helper replaced
    int element;
};

class LexicalForEachStatement : public ForEachStatement {
public:
    using ForEachStatement::ForEachStatement;
private:
    int execImpl(QoreValue& return_value, ExceptionSink* xsink) override {
        return execCommon<true>(return_value, xsink);
    }
    int execImpl(RuntimeConfig& rc, QoreValue& return_value, ExceptionSink* xsink) override {
        return execCommon<true>(rc, return_value, xsink);
    }
};
}

ForEachStatement* ForEachStatement::create(LexicalSemanticPolicy policy, int start_line, int end_line,
        QoreValue var, QoreValue list, StatementBlock* code) {
    ForEachStatement* rv = policy.needsFilter()
        ? new LexicalForEachStatement(start_line, end_line, var, list, code)
        : new ForEachStatement(start_line, end_line, var, list, code);
    rv->setSemanticPolicy(policy);
    return rv;
}

template <bool Lexical>
int ForEachStatement::assignLoopValue(LValueHelper& lvalue, QoreValue value, const char* desc) const {
    if constexpr (Lexical) {
        return lvalue.assign(value, desc, semantic_policy);
    } else {
        return lvalue.assign(value, desc);
    }
}

ForEachStatement::ForEachStatement(int start_line, int end_line, QoreValue v, QoreValue l, StatementBlock* cd) : AbstractStatement(start_line, end_line), var(v), list(l), code(cd) {
}

ForEachStatement::~ForEachStatement() {
    var.discard(nullptr);
    list.discard(nullptr);
    qore_delete_statement(code);
    delete lvars;
}

int ForEachStatement::execImpl(QoreValue& return_value, ExceptionSink* xsink) {
    return execCommon<false>(return_value, xsink);
}

template <bool Lexical>
int ForEachStatement::execCommon(QoreValue& return_value, ExceptionSink* xsink) {
    if (is_ref)
        return execRef<Lexical>(return_value, xsink);

    // instantiate local variables
    LVListInstantiator lvi(xsink, lvars, pwo.parse_options);

    // get iterator object
    FunctionalOperator::FunctionalValueType value_type;
    // the list is a statement of its own: the value iterated is kept, but the temporaries created by its expression
    // are released before the body runs
    std::unique_ptr<FunctionalOperatorInterface> f;
    {
        QoreStatementTempScope temps(xsink);
        f.reset(iterator_func ? iterator_func->getFunctionalIterator(value_type, xsink) : FunctionalOperatorInterface::getFunctionalIterator(value_type, list, true, "foreach statement", xsink));
    }
    if (*xsink || value_type == FunctionalOperator::nothing || !code)
        return 0;

    // execute "foreach" body
    unsigned i = 0;

    int rc = 0;

    while (true) {
        // Check for cancellation or program interrupt
        if (qore_check_cancel(xsink, "foreach loop")) {
            break;
        }

        {
            // get first value
            // an element that a functional operator evaluates when the loop asks for it keeps its temporaries in
            // the scope of the foreach statement: the expression that creates them again for the next element
            // releases them, as the other execution modes do
            ValueOptionalRefHolder iv(xsink);
            if (f->getNext(iv, xsink))
                break;
            if (*xsink)
                break;

            // a store whose check records reference elements is made again after they have been bound with no
            // lvalue lock held (qore#7391)
            QoreBindRetry retry(xsink);
            bool stored = false;
            do {
                LValueHelper n(var, xsink, false, retry);
                if (!n)
                    break;

                // assign variable to current value; a retry assigns the value that the first round converted
                QoreValue value = retry.hasValue() ? retry.takeValue() : iv.takeReferencedValue();
                if (assignLoopValue<Lexical>(n, value, "<foreach lvalue assignment>"))
                    continue;
                stored = true;
                break;
            } while (retry.again());
            if (!stored)
                break;
        }

        // set offset in thread-local data for "$#" if the body reads it (qore#7270)
        ForEachElementHelper eh(reads_implicit_element, i++);

        // execute "foreach" body
        if (((rc = code->execImpl(return_value, xsink)) == RC_BREAK) || *xsink) {
            rc = 0;
            break;
        }

        if (rc == RC_RETURN)
            break;
        else if (rc == RC_CONTINUE)
            rc = 0;
    }

    return rc;
}

int ForEachStatement::execImpl(RuntimeConfig& rc, QoreValue& return_value, ExceptionSink* xsink) {
    return execCommon<false>(rc, return_value, xsink);
}

template <bool Lexical>
int ForEachStatement::execCommon(RuntimeConfig& rc, QoreValue& return_value, ExceptionSink* xsink) {
    if (is_ref)
        return execRef<Lexical>(rc, return_value, xsink);

    // instantiate local variables
    LVListInstantiator lvi(xsink, lvars, pwo.parse_options);

    // get iterator object
    FunctionalOperator::FunctionalValueType value_type;
    // the list is a statement of its own: the value iterated is kept, but the temporaries created by its expression
    // are released before the body runs
    std::unique_ptr<FunctionalOperatorInterface> f;
    {
        QoreStatementTempScope temps(xsink);
        f.reset(iterator_func ? iterator_func->getFunctionalIterator(value_type, xsink) : FunctionalOperatorInterface::getFunctionalIterator(value_type, list, true, "foreach statement", xsink));
    }
    if (*xsink || value_type == FunctionalOperator::nothing || !code)
        return 0;

    // execute "foreach" body
    unsigned i = 0;

    int rc_state = 0;

    while (true) {
        // Check for cancellation or program interrupt
        if (qore_check_cancel(xsink, "foreach loop")) {
            break;
        }

        {
            // get first value
            // an element that a functional operator evaluates when the loop asks for it keeps its temporaries in
            // the scope of the foreach statement: the expression that creates them again for the next element
            // releases them, as the other execution modes do
            ValueOptionalRefHolder iv(xsink);
            if (f->getNext(iv, xsink))
                break;
            if (*xsink)
                break;

            // a store whose check records reference elements is made again after they have been bound with no
            // lvalue lock held (qore#7391)
            QoreBindRetry retry(xsink);
            bool stored = false;
            do {
                LValueHelper n(var, rc, xsink, false, retry);
                if (!n)
                    break;

                // assign variable to current value; a retry assigns the value that the first round converted
                QoreValue value = retry.hasValue() ? retry.takeValue() : iv.takeReferencedValue();
                if (assignLoopValue<Lexical>(n, value, "<foreach lvalue assignment>"))
                    continue;
                stored = true;
                break;
            } while (retry.again());
            if (!stored)
                break;
        }

        // set offset in thread-local data for "$#" if the body reads it (qore#7270)
        ForEachElementHelper eh(reads_implicit_element, i++);

        // execute "foreach" body
        if (((rc_state = code->execImpl(rc, return_value, xsink)) == RC_BREAK) || *xsink) {
            rc_state = 0;
            break;
        }

        if (rc_state == RC_RETURN)
            break;
        else if (rc_state == RC_CONTINUE)
            rc_state = 0;
    }

    return rc_state;
}

template <bool Lexical>
int ForEachStatement::execRef(QoreValue& return_value, ExceptionSink* xsink) {
    int rc = 0;

    // instantiate local variables
    LVListInstantiator lvi(xsink, lvars, pwo.parse_options);

    ParseReferenceNode* r = list.get<ParseReferenceNode>();

    // here we get the runtime reference
    ReferenceHolder<ReferenceNode> vr(r->evalToRef(xsink), xsink);
    // a reference to NOTHING (\NOTHING) has no lvalue: the loop has no iterations (issue #6480)
    if (*xsink || !vr) {
        return 0;
    }

    // get the current value of the lvalue expression
    ValueHolder tlist(vr->eval(xsink), xsink);
    if (!code || *xsink || tlist->isNothing()) {
        return 0;
    }

    QoreListNode* l_tlist = tlist->getType() == NT_LIST ? tlist->get<QoreListNode>() : nullptr;
    if (l_tlist && l_tlist->empty()) {
        return 0;
    }

    // execute "foreach" body
    ValueHolder ln(xsink);
    unsigned i = 0;

    if (l_tlist) {
        ln = new QoreListNode(autoTypeInfo);
    }
    //printd(5, "ForEachStatement::execRef() l_tlist: %p ln: %s\n", l_tlist, ln->getFullTypeName());

    while (true) {
        // Check for cancellation or program interrupt
        if (qore_check_cancel(xsink, "foreach loop")) {
            break;
        }

        {
            // a store whose check records reference elements is made again after they have been bound with no
            // lvalue lock held (qore#7391)
            QoreBindRetry retry(xsink);
            bool stored = false;
            do {
                LValueHelper n(var, xsink, false, retry);
                if (!n) {
                    return 0;
                }

                // assign variable to current value in list; a retry assigns the value that the first round converted
                QoreValue value = retry.hasValue()
                    ? retry.takeValue()
                    : (l_tlist ? l_tlist->getReferencedEntry(i) : tlist.release());
                if (assignLoopValue<Lexical>(n, value)) {
                    continue;
                }
                stored = true;
                break;
            } while (retry.again());
            if (!stored) {
                return 0;
            }
        }

        // set offset in thread-local data for "$#" if the body reads it (qore#7270)
        ForEachElementHelper eh(reads_implicit_element, l_tlist ? (int)i : 0);

        // execute "for" body
        rc = code->execImpl(return_value, xsink);
        if (*xsink) {
            return 0;
        }

        // get value of foreach variable
        ValueEvalOptimizedRefHolder nv(var, xsink);
        if (*xsink) {
            return 0;
        }

        // assign new value to temporary variable for later assignment to referenced lvalue
        if (l_tlist) {
            ln->get<QoreListNode>()->push(nv.takeReferencedValue(), nullptr);
        } else {
            ln = nv.takeReferencedValue();
        }

        if (rc == RC_BREAK || rc == RC_RETURN) {
            // leaving the loop early: assign remaining values to list unchanged
            if (l_tlist) {
                while (++i < l_tlist->size()) {
                    ln->get<QoreListNode>()->push(l_tlist->getReferencedEntry(i), nullptr);
                }
            }

            if (rc == RC_BREAK) {
                rc = 0;
            }
            break;
        }

        if (rc == RC_CONTINUE) {
            rc = 0;
        }
        ++i;

        // break out of loop if appropriate
        if (!l_tlist || i == l_tlist->size()) {
            break;
        }
    }

    // an exception (e.g. cancellation) leaves the list unchanged, as when the body throws
    if (*xsink) {
        return 0;
    }

    // write the value back to the lvalue; a store whose check records reference elements is made again after they have
    // been bound with no lvalue lock held, and the reference is resolved again for it (qore#7391)
    QoreBindRetry retry(xsink);
    do {
        LValueHelper val(**vr, xsink);
        if (!val) {
            return 0;
        }
        val.setBindRetry(retry);

        QoreValue value = retry.hasValue() ? retry.takeValue() : ln.release();
        if (assignLoopValue<Lexical>(val, value)) {
            continue;
        }

        return rc;
    } while (retry.again());
    return 0;
}

template <bool Lexical>
int ForEachStatement::execRef(RuntimeConfig& rc, QoreValue& return_value, ExceptionSink* xsink) {
    int rc_state = 0;

    // instantiate local variables
    LVListInstantiator lvi(xsink, lvars, pwo.parse_options);

    ParseReferenceNode* r = list.get<ParseReferenceNode>();

    // here we get the runtime reference
    ReferenceHolder<ReferenceNode> vr(r->evalToRef(rc, xsink), xsink);
    // a reference to NOTHING (\NOTHING) has no lvalue: the loop has no iterations (issue #6480)
    if (*xsink || !vr) {
        return 0;
    }

    // get the current value of the lvalue expression
    ValueEvalRefHolder tlist(rc, *vr, xsink);
    if (!code || *xsink || tlist->isNothing()) {
        return 0;
    }

    QoreListNode* l_tlist = tlist->getType() == NT_LIST ? tlist->get<QoreListNode>() : nullptr;
    if (l_tlist && l_tlist->empty()) {
        return 0;
    }

    // execute "foreach" body
    ValueHolder ln(xsink);
    unsigned i = 0;

    if (l_tlist) {
        ln = new QoreListNode(autoTypeInfo);
    }

    while (true) {
        // Check for cancellation or program interrupt
        if (qore_check_cancel(xsink, "foreach loop")) {
            break;
        }

        {
            // a store whose check records reference elements is made again after they have been bound with no
            // lvalue lock held (qore#7391)
            QoreBindRetry retry(xsink);
            bool stored = false;
            do {
                LValueHelper n(var, rc, xsink, false, retry);
                if (!n) {
                    return 0;
                }

                // assign variable to current value in list; a retry assigns the value that the first round converted
                QoreValue value = retry.hasValue()
                    ? retry.takeValue()
                    : (l_tlist ? l_tlist->getReferencedEntry(i) : tlist.takeReferencedValue());
                if (assignLoopValue<Lexical>(n, value)) {
                    continue;
                }
                stored = true;
                break;
            } while (retry.again());
            if (!stored) {
                return 0;
            }
        }

        // set offset in thread-local data for "$#" if the body reads it (qore#7270)
        ForEachElementHelper eh(reads_implicit_element, l_tlist ? (int)i : 0);

        // execute "for" body
        rc_state = code->execImpl(rc, return_value, xsink);
        if (*xsink) {
            return 0;
        }

        // get value of foreach variable
        ValueEvalRefHolder nv(rc, var, xsink);
        if (*xsink) {
            return 0;
        }

        // assign new value to temporary variable for later assignment to referenced lvalue
        if (l_tlist) {
            if (!ln) {
                ln = new QoreListNode(autoTypeInfo);
            }
            ln->get<QoreListNode>()->push(nv.takeReferencedValue(), nullptr);
        } else {
            ln = nv.takeReferencedValue();
        }

        if (rc_state == RC_BREAK || rc_state == RC_RETURN) {
            // leaving the loop early: assign remaining values to list unchanged
            if (l_tlist) {
                while (++i < l_tlist->size()) {
                    ln->get<QoreListNode>()->push(l_tlist->getReferencedEntry(i), nullptr);
                }
            }

            if (rc_state == RC_BREAK) {
                rc_state = 0;
            }
            break;
        }

        if (rc_state == RC_CONTINUE) {
            rc_state = 0;
        }
        ++i;

        // break out of loop if appropriate
        if (!l_tlist || i == l_tlist->size()) {
            break;
        }
    }

    if (l_tlist) {
        // assign remaining values to list unchanged
        if (!ln) {
            ln = new QoreListNode(autoTypeInfo);
        }
    }

    // an exception (e.g. cancellation) leaves the list unchanged, as when the body throws
    if (*xsink) {
        return 0;
    }

    // write the value back to the lvalue; a store whose check records reference elements is made again after they have
    // been bound with no lvalue lock held, and the reference is resolved again for it (qore#7391)
    QoreBindRetry retry(xsink);
    do {
        LValueHelper val(**vr, rc, xsink);
        if (!val) {
            return 0;
        }
        val.setBindRetry(retry);

        QoreValue value = retry.hasValue() ? retry.takeValue() : ln.release();
        if (assignLoopValue<Lexical>(val, value)) {
            continue;
        }

        return rc_state;
    } while (retry.again());
    return 0;
}

int ForEachStatement::parseInitImpl(QoreParseContext& parse_context) {
    // code of the loop can run more than once, so a read in it can follow an assignment parsed after it (qore#5937)
    ParseDeferredScopeHelper deferred_scope;
    // turn off top-level flag for statement vars
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_TOP_LEVEL);

    // saves local variables after parsing
    QoreParseContextLvarHelper lh(parse_context, lvars);

    int err = 0;

    parse_context.typeInfo = nullptr;
    {
        // the loop variable is the target of the loop's assignment of each element, not a read
        QoreParseContextFlagHelper fh0(parse_context);
        fh0.setFlags(PF_FOR_ASSIGNMENT);
        err = parse_init_value(var, parse_context);
    }

    qore_type_t t = var.getType();
    if (!err && t != NT_VARREF && t != NT_SELF_VARREF) {
        parse_error(*loc, "foreach variable expression is not a variable reference (got type '%s' instead)",
            var.getTypeName());
        err = -1;
    }

    parse_context.typeInfo = nullptr;
    {
        // a local loop variable declared by the statement has no value in the iterated expression (qore#526); a
        // global keeps its value when the statement runs again, so it is not checked
        const VarRefNode* decl = t == NT_VARREF && var.get<const VarRefNode>()->isGlobalDecl()
            ? var.get<const VarRefNode>()
            : nullptr;
        const void* decl_target = nullptr;
        const char* decl_what = nullptr;
        if (decl) {
            qore_var_t decl_type = decl->getType();
            if (decl_type == VT_LOCAL || decl_type == VT_CLOSURE || decl_type == VT_LOCAL_TS) {
                decl_target = decl->ref.id;
                decl_what = "local variable";
            }
        }
        QoreParseInitializerTarget init_target(decl_target);
        if (parse_init_value(list, parse_context) && !err) {
            err = -1;
        }
        if (decl_target && init_target.raiseUseError(decl_what, decl->getName()) && !err) {
            err = -1;
        }
    }

    if (code) {
        // The foreach body may not execute if the iterator is empty.
        NarrowedTypeHelper nth;
        AssignedStateHelper ash;
        nth.saveState();
        ash.saveState();

        // every iteration of the body runs with the loop variable assigned to the current element; it is
        // marked after the state is saved, so that after the loop (which may run zero times) a variable
        // declared before the loop is still only as assigned as it was before it
        if (!err && t == NT_VARREF) {
            reinterpret_cast<VarRefNode*>(var.getInternalNode())->parseAssigned();
        }

        QoreParseContextFlagHelper fh0(parse_context);
        // the body runs for each element with the element's index as $# (qore#5690)
        fh0.setFlags(PF_BREAK_OK | PF_CONTINUE_OK | PF_IMPLICIT_ELEMENT);
        // a $# in the body reads the index of this statement, not that of the code around it (qore#7270)
        QoreParseImplicitElementScope element_scope(parse_context);

        if (code->parseInitImpl(parse_context) && !err) {
            err = -1;
        }
        reads_implicit_element = element_scope.readsImplicitElement();

        nth.recordBranchAndRestore();
        ash.recordBranchAndRestore();
        nth.recordSavedAsImplicitBranch();
        ash.recordSavedAsImplicitBranch();
        nth.mergeAndApply();
        ash.mergeAndApply();
        // break and continue leave the body before its end (qore#5937)
        ash.applyChangesSinceSave();
    } else {
        // a statement without a body has no code that could read its index
        reads_implicit_element = false;
    }

    qore_type_t typ = list.getType();
    is_ref = (typ == NT_PARSEREFERENCE);
    // use lazy evaluation if the iterator expression supports it
    iterator_func = dynamic_cast<FunctionalOperator*>(list.getInternalNode());

    return err;
}

void ForEachStatement::parseCommit(QoreProgram* pgm) {
    AbstractStatement::parseCommit(pgm);
    if (code) {
        code->parseCommit(pgm);
    }
}
