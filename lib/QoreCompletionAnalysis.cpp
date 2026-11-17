/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreCompletionAnalysis.cpp

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

/*  The analysis of the completion of code for the MISSING-RETURN check (issue #5940)

    The end of the code of a function, method or closure whose declared return type does not accept NOTHING must not
    be reachable without a return statement.  Each statement and expression is given, for each way in which it can
    end (completing normally, returning, breaking, continuing, raising an exception), whether that way is possible
    (YES), impossible (NO) or not known (MAYBE), and MISSING-RETURN is raised only when completing is YES.

    - Every condition that is not a constant boolean or integer is assumed to go both ways, as in the definite return
      analysis of other languages, and a switch statement without a default case to be able to match no case (unless
      its cases are not all values or its type has a few values: an enum, bool, or no declared type): the end of
      "if (x) { return 1; }" is reachable, the end of "while (True) {...}" without a break is not.  Another constant
      (a string) has a boolean value that depends on the semantics of the code: either branch may run (MAYBE).  A
      switch over a constant value runs the case that the value matches.
    - A call completes if its target can return, which is found from the code of the target: a user function, a
      static method, or a method called where no override can be called instead (the call names the class of the
      method, the class of the call is final), of the same Program, whose code can complete or reach a return
      statement; the call raises an exception if that code does.  A builtin function of the Qore namespace returns,
      except Qore::exit(), Qore::abort() and Qore::exec(), which do not, and the builtins that can run user code
      (MAYBE): the call_*() functions, load_module(), throw_thread_resource_exceptions*(), any builtin given an
      argument that can be code or an object, any builtin method (which can call the methods of its object that a
      user class overrides), and a pseudo-method of a value that can be an object, a hash or code.  Whether a builtin
      raises an exception is not known.  A call whose target is not known when the code is parsed (a call reference,
      a closure, a method that a subclass can override, a variant of another Program or AOT-compiled code without a
      parse tree, a recursive call) is MAYBE, as is a construct that the analysis does not know.  A MAYBE makes the
      end MAYBE, and no error is raised.
    - A catch block is reached when the try block raises an exception: a throw or rethrow statement, or a call of
      analyzed code that raises one; an assertion, an operator or another call may raise one (MAYBE).
    - Code that runs implicitly is MAYBE where the construct is known to run it: "delete" (a destructor), the
      member of an object (memberGate()), an iterated object (next()), the creation of a value of a hashdecl (the
      default values of its members).  Destructors that run when the last reference to an object is released (by an
      assignment or at the end of a scope) are not taken into account.

    The analysis is monotone: an answer of NO or MAYBE where YES would be right can only hide an error, never raise
    one, so every construct that is not analyzed precisely is given the lowest answer that is still correct.
*/

#include <qore/Qore.h>
#include "qore/intern/QoreCompletionAnalysis.h"
#include "qore/intern/StatementBlock.h"
#include "qore/intern/IfStatement.h"
#include "qore/intern/WhileStatement.h"
#include "qore/intern/DoWhileStatement.h"
#include "qore/intern/ForStatement.h"
#include "qore/intern/ForEachStatement.h"
#include "qore/intern/SwitchStatement.h"
#include "qore/intern/TryStatement.h"
#include "qore/intern/ReturnStatement.h"
#include "qore/intern/ThrowStatement.h"
#include "qore/intern/RethrowStatement.h"
#include "qore/intern/ThreadExitStatement.h"
#include "qore/intern/BreakStatement.h"
#include "qore/intern/ContinueStatement.h"
#include "qore/intern/ExpressionStatement.h"
#include "qore/intern/AssertStatement.h"
#include "qore/intern/DebugStatement.h"
#include "qore/intern/OnBlockExitStatement.h"
#include "qore/intern/ContextStatement.h"
#include "qore/intern/FunctionCallNode.h"
#include "qore/intern/VarRefNode.h"
#include "qore/intern/ScopedObjectCallNode.h"
#include "qore/intern/ObjectMethodReferenceNode.h"
#include "qore/intern/ParseReferenceNode.h"
#include "qore/intern/QoreParseListNode.h"
#include "qore/intern/QoreParseHashNode.h"
#include "qore/intern/QoreOperatorNode.h"
#include "qore/intern/QoreDotEvalOperatorNode.h"
#include "qore/intern/QoreHashObjectDereferenceOperatorNode.h"
#include "qore/intern/QoreBackgroundOperatorNode.h"
#include "qore/intern/QoreDeleteOperatorNode.h"
#include "qore/intern/QoreCastOperatorNode.h"
#include "qore/intern/QoreLogicalAndOperatorNode.h"
#include "qore/intern/QoreNullCoalescingOperatorNode.h"
#include "qore/intern/QoreValueCoalescingOperatorNode.h"
#include "qore/intern/QoreQuestionMarkOperatorNode.h"
#include "qore/intern/QoreMapOperatorNode.h"
#include "qore/intern/QoreSelectOperatorNode.h"
#include "qore/intern/QoreFoldlOperatorNode.h"
#include "qore/intern/QoreMapSelectOperatorNode.h"
#include "qore/intern/QoreHashMapOperatorNode.h"
#include "qore/intern/QoreHashMapSelectOperatorNode.h"
#include "qore/intern/QoreStreamingOperatorNode.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/qore_thread_intern.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <set>

namespace {
//! Whether a way of ending is possible
enum qore_reach_e : unsigned char {
    QR_NO = 0,      //!< certainly impossible
    QR_MAYBE = 1,   //!< not known
    QR_YES = 2,     //!< certainly possible
};

qore_reach_e qcr_min(qore_reach_e a, qore_reach_e b) {
    return a < b ? a : b;
}

qore_reach_e qcr_max(qore_reach_e a, qore_reach_e b) {
    return a > b ? a : b;
}

//! How a statement or expression can end when it is reached
struct QoreCompletion {
    //! completes normally (execution continues after it)
    qore_reach_e normal = QR_YES;
    //! returns from the code
    qore_reach_e ret = QR_NO;
    //! leaves the innermost loop or switch with break
    qore_reach_e brk = QR_NO;
    //! continues the innermost loop
    qore_reach_e cont = QR_NO;
    //! raises an exception that a catch block can handle
    qore_reach_e thr = QR_NO;

    //! Returns the completion of code that cannot be analyzed
    static QoreCompletion unknown() {
        QoreCompletion c;
        c.normal = c.ret = c.brk = c.cont = c.thr = QR_MAYBE;
        return c;
    }

    //! Returns the completion of code that is never reached
    static QoreCompletion unreached() {
        QoreCompletion c;
        c.normal = QR_NO;
        return c;
    }

    //! Appends code that runs when this code completes normally
    void then(const QoreCompletion& c) {
        ret = qcr_max(ret, qcr_min(normal, c.ret));
        brk = qcr_max(brk, qcr_min(normal, c.brk));
        cont = qcr_max(cont, qcr_min(normal, c.cont));
        thr = qcr_max(thr, qcr_min(normal, c.thr));
        normal = qcr_min(normal, c.normal);
    }

    //! Adds the ways of ending of alternative code
    void join(const QoreCompletion& c) {
        normal = qcr_max(normal, c.normal);
        ret = qcr_max(ret, c.ret);
        brk = qcr_max(brk, c.brk);
        cont = qcr_max(cont, c.cont);
        thr = qcr_max(thr, c.thr);
    }

    //! Returns the completion of this code when it is reached as given
    QoreCompletion reachedWith(qore_reach_e r) const {
        QoreCompletion c;
        c.normal = qcr_min(r, normal);
        c.ret = qcr_min(r, ret);
        c.brk = qcr_min(r, brk);
        c.cont = qcr_min(r, cont);
        c.thr = qcr_min(r, thr);
        return c;
    }

    //! Limits each way of ending to at most @a r
    void limit(qore_reach_e r) {
        *this = reachedWith(r);
    }

    //! Returns the completion of code that is either @a a or @a b, which is not known
    static QoreCompletion either(const QoreCompletion& a, const QoreCompletion& b) {
        QoreCompletion c;
        c.normal = a.normal == b.normal ? a.normal : QR_MAYBE;
        c.ret = a.ret == b.ret ? a.ret : QR_MAYBE;
        c.brk = a.brk == b.brk ? a.brk : QR_MAYBE;
        c.cont = a.cont == b.cont ? a.cont : QR_MAYBE;
        c.thr = a.thr == b.thr ? a.thr : QR_MAYBE;
        return c;
    }
};

//! What a call of code can do
struct QoreCallOutcome {
    //! returns to the caller
    qore_reach_e returns = QR_MAYBE;
    //! raises an exception
    qore_reach_e thr = QR_MAYBE;

    //! Returns the outcome of a call of code that is not known
    static QoreCallOutcome unknown() {
        return QoreCallOutcome();
    }

    //! Returns the outcome of a call of a builtin that returns; whether it raises an exception is not known
    static QoreCallOutcome builtin() {
        QoreCallOutcome o;
        o.returns = QR_YES;
        return o;
    }
};

//! The value of a constant condition
enum qore_completion_cond_e {
    //! not a constant: can be true and false
    QCR_COND_UNKNOWN,
    //! a constant that is true
    QCR_COND_TRUE,
    //! a constant that is false
    QCR_COND_FALSE,
    //! a constant whose boolean value depends on the boolean semantics of the code (ex: a string)
    QCR_COND_CONSTANT,
};

qore_completion_cond_e qcr_constant_condition(const QoreValue& v) {
    if (v.needsEval()) {
        return QCR_COND_UNKNOWN;
    }
    // only the boolean value of a boolean or an integer is the same in every boolean semantics
    if (v.isBool() || v.isInt()) {
        return v.getAsBool() ? QCR_COND_TRUE : QCR_COND_FALSE;
    }
    return QCR_COND_CONSTANT;
}

//! Returns the type of the value of an expression as its parse gave it, or nullptr if it is not known
const QoreTypeInfo* qcr_value_type(const QoreValue& v) {
    if (v.hasNode() && v.needsEval()) {
        const ParseNode* pn = dynamic_cast<const ParseNode*>(v.getInternalNode());
        return pn ? pn->getParseTypeInfo() : nullptr;
    }
    return v.getFullTypeInfo();
}

//! Returns true if a value of the type can be a closure or a call reference
bool qcr_type_may_be_code(const QoreTypeInfo* ti) {
    return QoreTypeInfo::parseReturns(ti, NT_RUNTIME_CLOSURE) != QTI_NOT_EQUAL
        || QoreTypeInfo::parseReturns(ti, NT_FUNCREF) != QTI_NOT_EQUAL;
}

//! Returns true if a builtin given a value of the type can run user code: the value can be code or an object, whose
//! methods the builtin can call
bool qcr_type_may_run_user_code(const QoreTypeInfo* ti) {
    return qcr_type_may_be_code(ti) || QoreTypeInfo::parseReturns(ti, NT_OBJECT) != QTI_NOT_EQUAL;
}

//! Returns true if a value of the type can be an object, a hash, a closure or a call reference, whose methods, code
//! members or code a call through the dot operator (such as the <callref>::exec() pseudo-method) can call
bool qcr_type_may_call_user_code(const QoreTypeInfo* ti) {
    return QoreTypeInfo::parseReturns(ti, NT_OBJECT) != QTI_NOT_EQUAL
        || QoreTypeInfo::parseReturns(ti, NT_HASH) != QTI_NOT_EQUAL
        || qcr_type_may_be_code(ti);
}

//! Returns what a call of the builtin function of the given name in the Qore namespace can do
/** Qore::exit(), Qore::abort() and Qore::exec() do not return; the functions that call code that they are given by
    name (the call_*() functions), that run the code of modules (load_module()) or that run the cleanup code of thread
    resources (throw_thread_resource_exceptions*()) can call any code; the other builtin functions return
*/
QoreCallOutcome qcr_qore_function_outcome(const char* name) {
    static const char* never[] = {"exit", "abort", "exec"};
    for (const char* n : never) {
        if (!strcmp(name, n)) {
            QoreCallOutcome o;
            o.returns = QR_NO;
            return o;
        }
    }
    static const char* prefixes[] = {"call_", "load_module", "throw_thread_resource_exceptions"};
    for (const char* n : prefixes) {
        if (!strncmp(name, n, strlen(n))) {
            return QoreCallOutcome::unknown();
        }
    }
    return QoreCallOutcome::builtin();
}

//! Returns true if a value of the type can be an object, which an iteration calls the next() method of
bool qcr_type_may_be_object(const QoreTypeInfo* ti) {
    return QoreTypeInfo::parseReturns(ti, NT_OBJECT) != QTI_NOT_EQUAL;
}

//! What a function called is
enum qore_call_kind_e {
    //! a method
    QCK_METHOD,
    //! a function of the Qore namespace
    QCK_QORE_FUNCTION,
    //! another function
    QCK_FUNCTION,
};

//! The analysis of the code of a Program that the parse of the Program has initialized
class QoreCompletionAnalyzer {
public:
    DLLLOCAL QoreCompletionAnalyzer(QoreProgram* pgm, QoreMissingReturnChecks& cache) : pgm(pgm), cache(cache) {
    }

    //! Returns whether the end of the code of the variant is certainly reachable
    /** The analysis of the code that the code calls is kept for the following calls
    */
    DLLLOCAL qore_reach_e bodyEnd(const UserVariantBase* uvb) {
        const StatementBlock* b = uvb->getStatementBlock();
        if (!b || !b->isParseInitialized()) {
            return QR_MAYBE;
        }
        // a recursive call of the code is not known to return
        InProgressHelper iph(in_progress, uvb);
        return block(b).normal;
    }

private:
    //! The Program whose code is analyzed; the code of other Programs is not
    QoreProgram* pgm;
    //! The analysis of the variants of the Program, kept until code is added to it
    QoreMissingReturnChecks& cache;

    //! The variants whose code is being analyzed
    std::set<const UserVariantBase*> in_progress;
    //! The depth of the analysis
    unsigned depth = 0;

    //! Marks the code of a variant as being analyzed while it lives
    class InProgressHelper {
    public:
        DLLLOCAL InProgressHelper(std::set<const UserVariantBase*>& in_progress, const UserVariantBase* uvb)
                : in_progress(in_progress), uvb(uvb) {
            in_progress.insert(uvb);
        }

        DLLLOCAL ~InProgressHelper() {
            in_progress.erase(uvb);
        }

    private:
        std::set<const UserVariantBase*>& in_progress;
        const UserVariantBase* uvb;
    };

    //! Limits the depth of the analysis, which is recursive; deeper code is not analyzed
    class DepthHelper {
    public:
        DLLLOCAL DepthHelper(unsigned& depth) : depth(depth) {
            ++depth;
        }

        DLLLOCAL ~DepthHelper() {
            --depth;
        }

        //! Returns true if the code at this depth cannot be analyzed
        DLLLOCAL bool tooDeep() const {
            return depth > 4096 || q_thread_stack_reserve_exceeded(QORE_PARSE_STACK_RESERVE);
        }

    private:
        unsigned& depth;
    };

    DLLLOCAL QoreCompletion block(const StatementBlock* b);
    DLLLOCAL QoreCompletion statement(const AbstractStatement* s);
    DLLLOCAL QoreCompletion optionalBlock(const StatementBlock* b) {
        return b ? block(b) : QoreCompletion();
    }

    //! The completion of an expression: only normal and thr are set
    DLLLOCAL QoreCompletion expr(const QoreValue& v);
    DLLLOCAL QoreCompletion operatorExpr(const QoreOperatorNode* op);
    //! The completion of the arguments of a call; @a user_args is set if an argument can be code or an object, with
    //! which a builtin can run user code
    DLLLOCAL QoreCompletion callArgs(const FunctionCallBase* call, bool& user_args);
    //! Returns the completion of the call of the target after its arguments
    DLLLOCAL QoreCompletion call(QoreCompletion args, const QoreCallOutcome& target);

    //! Returns what a call of the function or method can do
    /** @param f the function or method
        @param user_args true if the arguments can be code or objects, with which a builtin can run user code
        @param kind what @a f is
    */
    DLLLOCAL QoreCallOutcome functionOutcome(const QoreFunction* f, bool user_args, qore_call_kind_e kind);
    //! Returns what a call of the method, which is called without virtual dispatch, can do; a builtin method, which
    //! can call methods of its object that a user class overrides, is not known
    DLLLOCAL QoreCallOutcome methodOutcome(const QoreMethod* m, bool user_args);
    //! Returns what a call of the user variant can do
    DLLLOCAL QoreCallOutcome variantOutcome(const AbstractQoreFunctionVariant* v);

    DLLLOCAL QoreCompletion selfCall(const SelfFunctionCallNode* call);
    DLLLOCAL QoreCompletion dotCall(const QoreDotEvalOperatorNode* op);
};

QoreCompletion QoreCompletionAnalyzer::block(const StatementBlock* b) {
    DepthHelper dh(depth);
    if (dh.tooDeep()) {
        return QoreCompletion::unknown();
    }
    QoreCompletion rv;
    // the most that the on_exit and on_success handlers registered so far let code leave the block: a handler that
    // cannot complete keeps the block from completing or returning
    qore_reach_e handlers = QR_YES;
    // the same for the on_exit and on_error handlers and the exceptions that leave the block
    qore_reach_e error_handlers = QR_YES;
    // the on_exit and on_success handlers that can raise an exception, which they do when they run as the code
    // leaves the block without an exception after their registration: what they raise, and how the code leaves
    struct handler_exit_t {
        qore_reach_e thr;
        qore_reach_e exits;
    };
    std::vector<handler_exit_t> raising_handlers;
    for (const AbstractStatement* s : b->getStatements()) {
        if (!s) {
            continue;
        }
        if (const OnBlockExitStatement* obe = dynamic_cast<const OnBlockExitStatement*>(s)) {
            if (obe->getCode()) {
                QoreCompletion hc = block(obe->getCode());
                if (obe->getType() != OBE_Error) {
                    handlers = qcr_min(handlers, hc.normal);
                    if (hc.thr != QR_NO) {
                        raising_handlers.push_back({hc.thr, QR_NO});
                    }
                }
                if (obe->getType() != OBE_Success) {
                    // a handler that raises an exception replaces the exception; one that neither completes nor
                    // raises an exception (that ends the thread or the process) keeps it from leaving the block
                    error_handlers = qcr_min(error_handlers, qcr_max(hc.normal, hc.thr));
                }
            }
            continue;
        }
        QoreCompletion c = statement(s);
        if (!raising_handlers.empty()) {
            qore_reach_e exits = qcr_min(rv.normal, qcr_max(c.ret, qcr_max(c.brk, c.cont)));
            for (handler_exit_t& h : raising_handlers) {
                h.exits = qcr_max(h.exits, exits);
            }
        }
        c.ret = qcr_min(c.ret, handlers);
        c.brk = qcr_min(c.brk, handlers);
        c.cont = qcr_min(c.cont, handlers);
        c.thr = qcr_min(c.thr, error_handlers);
        rv.then(c);
        if (rv.normal == QR_NO) {
            // the rest of the block is not reached
            break;
        }
    }
    // the block completes and the handlers run
    for (handler_exit_t& h : raising_handlers) {
        rv.thr = qcr_max(rv.thr, qcr_min(qcr_max(h.exits, rv.normal), h.thr));
    }
    rv.normal = qcr_min(rv.normal, handlers);
    return rv;
}

QoreCompletion QoreCompletionAnalyzer::statement(const AbstractStatement* s) {
    if (const StatementBlock* b = dynamic_cast<const StatementBlock*>(s)) {
        return block(b);
    }
    if (const ExpressionStatement* es = dynamic_cast<const ExpressionStatement*>(s)) {
        return expr(es->getExpression());
    }
    if (const ReturnStatement* rs = dynamic_cast<const ReturnStatement*>(s)) {
        QoreCompletion rv = expr(rs->getExpression());
        rv.ret = rv.normal;
        rv.normal = QR_NO;
        return rv;
    }
    if (const IfStatement* is = dynamic_cast<const IfStatement*>(s)) {
        QoreCompletion rv = expr(is->getCond());
        qore_completion_cond_e cond = qcr_constant_condition(is->getCond());
        QoreCompletion if_code = optionalBlock(is->getIfCode());
        QoreCompletion else_code = optionalBlock(is->getElseCode());
        QoreCompletion branches;
        switch (cond) {
            case QCR_COND_TRUE: branches = if_code; break;
            case QCR_COND_FALSE: branches = else_code; break;
            case QCR_COND_CONSTANT: branches = QoreCompletion::either(if_code, else_code); break;
            default: branches = if_code; branches.join(else_code); break;
        }
        rv.then(branches);
        return rv;
    }
    if (const ThrowStatement* ts = dynamic_cast<const ThrowStatement*>(s)) {
        QoreCompletion rv = expr(ts->getArgs());
        rv.thr = qcr_max(rv.thr, rv.normal);
        rv.normal = QR_NO;
        return rv;
    }
    if (dynamic_cast<const RethrowStatement*>(s)) {
        QoreCompletion rv;
        rv.thr = QR_YES;
        rv.normal = QR_NO;
        return rv;
    }
    if (dynamic_cast<const ThreadExitStatement*>(s)) {
        return QoreCompletion::unreached();
    }
    if (dynamic_cast<const BreakStatement*>(s)) {
        QoreCompletion rv;
        rv.brk = QR_YES;
        rv.normal = QR_NO;
        return rv;
    }
    if (dynamic_cast<const ContinueStatement*>(s)) {
        QoreCompletion rv;
        rv.cont = QR_YES;
        rv.normal = QR_NO;
        return rv;
    }
    if (const TryStatement* ts = dynamic_cast<const TryStatement*>(s)) {
        QoreCompletion rv = optionalBlock(ts->getTryBlock());
        // the catch block runs if the try block raises an exception; the exceptions of the try block are handled
        qore_reach_e caught = rv.thr;
        rv.thr = QR_NO;
        rv.join(optionalBlock(ts->getCatchBlock()).reachedWith(caught));
        return rv;
    }
    if (const DoWhileStatement* ds = dynamic_cast<const DoWhileStatement*>(s)) {
        QoreCompletion body = optionalBlock(ds->getCode());
        qore_reach_e after_body = qcr_max(body.normal, body.cont);
        QoreCompletion cond = expr(ds->getCond()).reachedWith(after_body);
        qore_completion_cond_e cv = qcr_constant_condition(ds->getCond());
        QoreCompletion rv;
        rv.normal = qcr_max(cv == QCR_COND_TRUE ? QR_NO : cond.normal, body.brk);
        rv.ret = body.ret;
        rv.thr = qcr_max(body.thr, cond.thr);
        // a constant condition whose value is not known makes the loop either loop until it is left or not run
        if (cv == QCR_COND_CONSTANT) {
            rv.limit(QR_MAYBE);
        }
        return rv;
    }
    if (const WhileStatement* ws = dynamic_cast<const WhileStatement*>(s)) {
        QoreCompletion cond = expr(ws->getCond());
        qore_completion_cond_e cv = qcr_constant_condition(ws->getCond());
        qore_reach_e body_entry = cv == QCR_COND_FALSE ? QR_NO : cond.normal;
        QoreCompletion body = optionalBlock(ws->getCode()).reachedWith(body_entry);
        QoreCompletion rv;
        rv.normal = qcr_max(cv == QCR_COND_TRUE ? QR_NO : cond.normal, body.brk);
        rv.ret = body.ret;
        rv.thr = qcr_max(cond.thr, body.thr);
        if (cv == QCR_COND_CONSTANT) {
            rv.limit(QR_MAYBE);
        }
        return rv;
    }
    if (const ForStatement* fs = dynamic_cast<const ForStatement*>(s)) {
        QoreCompletion rv = expr(fs->getAssignment());
        // a for statement without a condition loops until it is left
        qore_completion_cond_e cv = fs->getCond().isNothing()
            ? QCR_COND_TRUE
            : qcr_constant_condition(fs->getCond());
        QoreCompletion cond = expr(fs->getCond());
        rv.then(cond);
        qore_reach_e cond_reached = rv.normal;
        qore_reach_e body_entry = cv == QCR_COND_FALSE ? QR_NO : cond_reached;
        QoreCompletion body = optionalBlock(fs->getCode()).reachedWith(body_entry);
        QoreCompletion iter = expr(fs->getIterator()).reachedWith(qcr_max(body.normal, body.cont));
        rv.normal = qcr_max(cv == QCR_COND_TRUE ? QR_NO : cond_reached, body.brk);
        rv.ret = body.ret;
        rv.brk = rv.cont = QR_NO;
        rv.thr = qcr_max(rv.thr, qcr_max(body.thr, iter.thr));
        if (cv == QCR_COND_CONSTANT) {
            rv.limit(QR_MAYBE);
        }
        return rv;
    }
    if (const ForEachStatement* fs = dynamic_cast<const ForEachStatement*>(s)) {
        QoreCompletion list = expr(fs->getList());
        // the iteration of an object calls its next() method, which can be user code
        qore_reach_e iterated = qcr_min(list.normal,
            qcr_type_may_be_object(qcr_value_type(fs->getList())) ? QR_MAYBE : QR_YES);
        QoreCompletion body = optionalBlock(fs->getCode()).reachedWith(iterated);
        QoreCompletion rv;
        rv.normal = qcr_max(iterated, body.brk);
        rv.ret = body.ret;
        rv.thr = qcr_max(list.thr, body.thr);
        return rv;
    }
    if (const SwitchStatement* ss = dynamic_cast<const SwitchStatement*>(s)) {
        QoreValue sexp = ss->getSwitchExp();
        QoreCompletion rv = expr(sexp);
        qore_reach_e dispatched = rv.normal;
        // the case that a constant value matches, which is nullptr if it matches no case and there is no default
        // case
        const CaseNode* target = nullptr;
        bool constant = !sexp.needsEval();
        if (constant) {
            ExceptionSink xsink;
            for (const CaseNode* w = ss->getCases(); w; w = w->next) {
                if (!w->isDefault() && w->matches(sexp, &xsink)) {
                    target = w;
                    break;
                }
                if (xsink) {
                    break;
                }
            }
            if (xsink) {
                // the comparison cannot be made here
                xsink.clear();
                constant = false;
                target = nullptr;
            } else if (!target) {
                for (const CaseNode* w = ss->getCases(); w; w = w->next) {
                    if (w->isDefault()) {
                        target = w;
                        break;
                    }
                }
            }
        }
        QoreCompletion cases = QoreCompletion::unreached();
        // a case block that completes falls through to the next one
        qore_reach_e fall_through = QR_NO;
        for (const CaseNode* w = ss->getCases(); w; w = w->next) {
            qore_reach_e entry = qcr_max(constant && w != target ? QR_NO : dispatched, fall_through);
            QoreCompletion c = optionalBlock(w->code).reachedWith(entry);
            fall_through = c.normal;
            c.normal = QR_NO;
            cases.join(c);
        }
        // a value that matches no case leaves the switch statement
        qore_reach_e no_match;
        if (constant) {
            no_match = target ? QR_NO : dispatched;
        } else if (ss->noMatchIsPossible()) {
            no_match = dispatched;
        } else {
            no_match = qcr_min(dispatched, QR_MAYBE);
        }
        QoreCompletion out;
        out.normal = qcr_max(qcr_max(fall_through, cases.brk), no_match);
        out.ret = cases.ret;
        if (ss->getSemanticPolicy().brokenLoop()) {
            // with %broken-loop-statement, continue leaves the switch statement, as break does
            out.normal = qcr_max(out.normal, cases.cont);
        } else {
            out.cont = cases.cont;
        }
        out.thr = qcr_max(rv.thr, cases.thr);
        return out;
    }
    if (const AssertStatement* as = dynamic_cast<const AssertStatement*>(s)) {
        QoreCompletion rv = expr(as->getCondition());
        // a failed assertion raises an exception, but assertions can be disabled and the condition true
        rv.thr = qcr_max(rv.thr, qcr_min(rv.normal, QR_MAYBE));
        return rv;
    }
    if (const DebugStatement* ds = dynamic_cast<const DebugStatement*>(s)) {
        // debug code runs only when debugging is enabled: it can keep the code from completing, but what it does
        // otherwise is not certain
        QoreCompletion c = ds->getBlock() ? block(ds->getBlock()) : expr(ds->getExpression());
        QoreCompletion rv = c;
        rv.limit(QR_MAYBE);
        rv.normal = c.normal;
        return rv;
    }
    if (const ContextStatement* cs = dynamic_cast<const ContextStatement*>(s)) {
        // context, summarize and subcontext statements iterate the rows of a query; whether they complete is not
        // analyzed
        QoreCompletion rv = expr(cs->exp);
        QoreCompletion body = optionalBlock(cs->code).reachedWith(qcr_min(rv.normal, QR_MAYBE));
        rv.normal = qcr_min(rv.normal, QR_MAYBE);
        rv.ret = body.ret;
        rv.thr = qcr_max(rv.thr, body.thr);
        return rv;
    }
    return QoreCompletion::unknown();
}

QoreCompletion QoreCompletionAnalyzer::call(QoreCompletion args, const QoreCallOutcome& target) {
    args.thr = qcr_max(args.thr, qcr_min(args.normal, target.thr));
    args.normal = qcr_min(args.normal, target.returns);
    return args;
}

QoreCompletion QoreCompletionAnalyzer::callArgs(const FunctionCallBase* call, bool& user_args) {
    user_args = false;
    QoreCompletion rv;
    auto arg = [&] (const QoreValue& v) {
        rv.then(expr(v));
        if (!user_args && qcr_type_may_run_user_code(qcr_value_type(v))) {
            user_args = true;
        }
    };
    if (const QoreListNode* args = call->getArgs()) {
        for (size_t i = 0, e = args->size(); i < e; ++i) {
            arg(args->retrieveEntry(i));
        }
    } else if (const QoreParseListNode* pargs = call->getParseArgs()) {
        for (size_t i = 0, e = pargs->size(); i < e; ++i) {
            arg(pargs->get(i));
        }
    }
    return rv;
}

QoreCallOutcome QoreCompletionAnalyzer::variantOutcome(const AbstractQoreFunctionVariant* v) {
    assert(v->isUser());
    const UserVariantBase* uvb = v->getUserVariantBase();
    if (!uvb || uvb->pgm != pgm) {
        return QoreCallOutcome::unknown();
    }
    const MethodVariantBase* mvb = v->getMethodVariantBase();
    if (mvb && mvb->isAbstract()) {
        return QoreCallOutcome::unknown();
    }
    unsigned char returns, thr;
    if (cache.get(uvb->serial, returns, thr)) {
        QoreCallOutcome o;
        o.returns = static_cast<qore_reach_e>(returns);
        o.thr = static_cast<qore_reach_e>(thr);
        return o;
    }
    if (in_progress.find(uvb) != in_progress.end()) {
        // a recursive call
        return QoreCallOutcome::unknown();
    }
    const StatementBlock* b = uvb->getStatementBlock();
    if (!b || !b->isParseInitialized()) {
        return QoreCallOutcome::unknown();
    }
    QoreCompletion c;
    {
        InProgressHelper iph(in_progress, uvb);
        c = block(b);
    }
    // the code returns if a return statement is reached or its end is; it raises the exceptions that leave it
    QoreCallOutcome o;
    o.returns = qcr_max(c.normal, c.ret);
    o.thr = c.thr;
    cache.set(uvb->serial, o.returns, o.thr);
    return o;
}

QoreCallOutcome QoreCompletionAnalyzer::functionOutcome(const QoreFunction* f, bool user_args,
        qore_call_kind_e kind) {
    if (!f || !f->numVariants() || !f->hasOnlyOwnVariants()) {
        return QoreCallOutcome::unknown();
    }
    // a call resolves to one of the variants: the outcome is certain if it is the same for all of them
    bool first = true;
    QoreCallOutcome rv;
    QoreFunctionIterator i(*f);
    while (i.next()) {
        const AbstractQoreFunctionVariant* v = i.getVariant();
        QoreCallOutcome o;
        if (!v->isUser()) {
            if (kind == QCK_METHOD) {
                // a builtin method can call the methods of its object that a user class overrides
                return QoreCallOutcome::unknown();
            }
            o = kind == QCK_QORE_FUNCTION ? qcr_qore_function_outcome(f->getName()) : QoreCallOutcome::builtin();
            if (user_args) {
                o.returns = qcr_min(o.returns, QR_MAYBE);
            }
        } else {
            o = variantOutcome(v);
        }
        if (first) {
            rv = o;
            first = false;
        } else {
            if (rv.returns != o.returns) {
                rv.returns = QR_MAYBE;
            }
            if (rv.thr != o.thr) {
                rv.thr = QR_MAYBE;
            }
        }
    }
    return rv;
}

QoreCallOutcome QoreCompletionAnalyzer::methodOutcome(const QoreMethod* m, bool user_args) {
    if (!m) {
        return QoreCallOutcome::unknown();
    }
    return functionOutcome(qore_method_private::get(*m)->getFunction(), user_args, QCK_METHOD);
}

QoreCompletion QoreCompletionAnalyzer::selfCall(const SelfFunctionCallNode* sc) {
    bool user_args;
    QoreCompletion args = callArgs(sc, user_args);
    if (sc->isCopyCall() || sc->isAbstractCall() || !sc->getMethod()) {
        return call(args, QoreCallOutcome::unknown());
    }
    if (sc->hasSingleName()) {
        // an unqualified call is dispatched on the class of the object: the method of the call is called only if
        // the class of the call has no subclass
        const qore_class_private* ctx = sc->getClassContext();
        const QoreClass* cls = ctx ? ctx->cls : sc->getClass();
        if (!cls || !qore_class_private::isFinal(*cls)) {
            return call(args, QoreCallOutcome::unknown());
        }
    }
    // a qualified call (Base::m()) calls the method that it names
    return call(args, methodOutcome(sc->getMethod(), user_args));
}

QoreCompletion QoreCompletionAnalyzer::dotCall(const QoreDotEvalOperatorNode* op) {
    QoreCompletion rv = expr(op->getExpression());
    const MethodCallNode* m = op->getMethodCall();
    if (!m) {
        rv.then(QoreCompletion::unknown());
        return rv;
    }
    bool user_args;
    rv.then(callArgs(m, user_args));
    QoreCallOutcome target;
    if (m->isPseudo()) {
        // a pseudo-method is builtin, but an object, a hash or code can be called instead or by it, as can code or
        // an object that it is given
        if (!user_args && !qcr_type_may_call_user_code(qcr_value_type(op->getExpression()))) {
            target = QoreCallOutcome::builtin();
        }
    } else if (m->getMethod() && m->getClass() && qore_class_private::isFinal(*m->getClass())) {
        // a method call is dispatched on the class of the object, which is the final class of the call: its method
        // is called
        target = methodOutcome(m->getMethod(), user_args);
    }
    return call(rv, target);
}

QoreCompletion QoreCompletionAnalyzer::operatorExpr(const QoreOperatorNode* op) {
    if (dynamic_cast<const QoreBackgroundOperatorNode*>(op)) {
        // the expression runs in a new thread
        return QoreCompletion();
    }
    if (const QoreDotEvalOperatorNode* dot = dynamic_cast<const QoreDotEvalOperatorNode*>(op)) {
        return dotCall(dot);
    }
    if (const QoreDeleteOperatorNode* d = dynamic_cast<const QoreDeleteOperatorNode*>(op)) {
        // runs the destructor of an object
        return call(expr(d->getExp()), QoreCallOutcome::unknown());
    }
    if (const QoreHashObjectDereferenceOperatorNode* d
            = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(op)) {
        // the member of an object can be read with its memberGate() method
        QoreCompletion rv = expr(d->getLeft());
        rv.then(expr(d->getRight()));
        if (qcr_type_may_be_object(qcr_value_type(d->getLeft()))) {
            rv.normal = qcr_min(rv.normal, QR_MAYBE);
        }
        return rv;
    }
    if (const QoreHashDeclCastOperatorNode* c = dynamic_cast<const QoreHashDeclCastOperatorNode*>(op)) {
        // can evaluate the default values of the members of the hashdecl
        return call(expr(c->getExp()), QoreCallOutcome::unknown());
    }
    // operators that evaluate their right operand only for some values of the left one
    if (dynamic_cast<const QoreLogicalAndOperatorNode*>(op) || dynamic_cast<const QoreNullCoalescingOperatorNode*>(op)
            || dynamic_cast<const QoreValueCoalescingOperatorNode*>(op)) {
        const QoreBinaryOperatorNode<>* b = static_cast<const QoreBinaryOperatorNode<>*>(op);
        QoreCompletion rv = expr(b->getLeft());
        QoreCompletion right = expr(b->getRight());
        rv.thr = qcr_max(rv.thr, qcr_min(rv.normal, qcr_min(right.thr, QR_MAYBE)));
        return rv;
    }
    if (const QoreQuestionMarkOperatorNode* q = dynamic_cast<const QoreQuestionMarkOperatorNode*>(op)) {
        QoreCompletion rv = expr(q->get(0));
        qore_completion_cond_e cond = qcr_constant_condition(q->get(0));
        QoreCompletion t = expr(q->get(1));
        QoreCompletion f = expr(q->get(2));
        QoreCompletion branches;
        switch (cond) {
            case QCR_COND_TRUE: branches = t; break;
            case QCR_COND_FALSE: branches = f; break;
            case QCR_COND_CONSTANT: branches = QoreCompletion::either(t, f); break;
            default: branches = t; branches.join(f); break;
        }
        rv.then(branches);
        return rv;
    }
    // operators that iterate a list or an iterator object, evaluating their other operands for each element
    unsigned n = 0;
    QoreValue operands[4];
    if (const QoreMapOperatorNode* o = dynamic_cast<const QoreMapOperatorNode*>(op)) {
        operands[n++] = o->getLeft();
        operands[n++] = o->getRight();
    } else if (const QoreSelectOperatorNode* o = dynamic_cast<const QoreSelectOperatorNode*>(op)) {
        operands[n++] = o->getLeft();
        operands[n++] = o->getRight();
    } else if (const QoreFoldlOperatorNode* o = dynamic_cast<const QoreFoldlOperatorNode*>(op)) {
        operands[n++] = o->getLeft();
        operands[n++] = o->getRight();
    } else if (const QoreStreamingOperatorNode* o = dynamic_cast<const QoreStreamingOperatorNode*>(op)) {
        operands[n++] = o->getLeft();
        operands[n++] = o->getRight();
    } else if (const QoreMapSelectOperatorNode* o = dynamic_cast<const QoreMapSelectOperatorNode*>(op)) {
        for (unsigned i = 0; i < 3; ++i) {
            operands[n++] = o->get(i);
        }
    } else if (const QoreHashMapOperatorNode* o = dynamic_cast<const QoreHashMapOperatorNode*>(op)) {
        for (unsigned i = 0; i < 3; ++i) {
            operands[n++] = o->get(i);
        }
    } else if (const QoreHashMapSelectOperatorNode* o = dynamic_cast<const QoreHashMapSelectOperatorNode*>(op)) {
        for (unsigned i = 0; i < 4; ++i) {
            operands[n++] = o->get(i);
        }
    }
    if (n) {
        // an operand that is evaluated for each element may not be evaluated at all, and the iteration of an
        // object calls its next() method: the operator completes if all of its operands can be evaluated, and
        // none of them can be an object; the exceptions of the operands are not certain
        QoreCompletion rv;
        for (unsigned i = 0; i < n; ++i) {
            QoreCompletion c = expr(operands[i]);
            rv.normal = qcr_min(rv.normal, c.normal);
            rv.thr = qcr_max(rv.thr, qcr_min(c.thr, QR_MAYBE));
            if (qcr_type_may_be_object(qcr_value_type(operands[i]))) {
                rv.normal = qcr_min(rv.normal, QR_MAYBE);
            }
        }
        return rv;
    }
    // operators that evaluate all of their operands
    if (const QoreSingleExpressionOperatorNode<QoreOperatorNode>* o
            = dynamic_cast<const QoreSingleExpressionOperatorNode<QoreOperatorNode>*>(op)) {
        return expr(o->getExp());
    }
    if (const QoreSingleExpressionOperatorNode<LValueOperatorNode>* o
            = dynamic_cast<const QoreSingleExpressionOperatorNode<LValueOperatorNode>*>(op)) {
        return expr(o->getExp());
    }
    if (const QoreSingleValueExpressionOperatorNode<QoreOperatorNode>* o
            = dynamic_cast<const QoreSingleValueExpressionOperatorNode<QoreOperatorNode>*>(op)) {
        return expr(o->getExp());
    }
    if (const QoreBinaryOperatorNode<QoreOperatorNode>* o
            = dynamic_cast<const QoreBinaryOperatorNode<QoreOperatorNode>*>(op)) {
        QoreCompletion rv = expr(o->getLeft());
        rv.then(expr(o->getRight()));
        return rv;
    }
    if (const QoreBinaryOperatorNode<LValueOperatorNode>* o
            = dynamic_cast<const QoreBinaryOperatorNode<LValueOperatorNode>*>(op)) {
        QoreCompletion rv = expr(o->getLeft());
        rv.then(expr(o->getRight()));
        return rv;
    }
    if (const QoreNOperatorNodeBase<3>* o = dynamic_cast<const QoreNOperatorNodeBase<3>*>(op)) {
        QoreCompletion rv;
        for (unsigned i = 0; i < 3; ++i) {
            rv.then(expr(o->get(i)));
        }
        return rv;
    }
    if (const QoreNOperatorNodeBase<4>* o = dynamic_cast<const QoreNOperatorNodeBase<4>*>(op)) {
        QoreCompletion rv;
        for (unsigned i = 0; i < 4; ++i) {
            rv.then(expr(o->get(i)));
        }
        return rv;
    }
    return QoreCompletion::unknown();
}

QoreCompletion QoreCompletionAnalyzer::expr(const QoreValue& v) {
    if (!v.hasNode() || !v.needsEval()) {
        return QoreCompletion();
    }
    DepthHelper dh(depth);
    if (dh.tooDeep()) {
        return QoreCompletion::unknown();
    }
    const AbstractQoreNode* n = v.getInternalNode();
    switch (n->getType()) {
        case NT_VARREF: {
            // a declaration of a variable that creates an object (Class c(args)) calls its constructor: that of a
            // builtin class returns, that of a user class is not analyzed
            const VarRefNewObjectNode* vn = dynamic_cast<const VarRefNewObjectNode*>(n);
            if (!vn) {
                return QoreCompletion();
            }
            const QoreClass* cls
                = QoreTypeInfo::getUniqueReturnClass(static_cast<const ParseNode*>(vn)->getParseTypeInfo());
            bool user_args;
            QoreCompletion args = callArgs(vn, user_args);
            return call(args, cls && cls->isSystem() && !user_args ? QoreCallOutcome::builtin()
                : QoreCallOutcome::unknown());
        }

        // values, references to variables and creations of call references and closures, which call nothing
        case NT_SELF_VARREF:
        case NT_CLASS_VARREF:
        case NT_IMPLICIT_ARG:
        case NT_IMPLICIT_ELEMENT:
        case NT_CONTEXTREF:
        case NT_COMPLEXCONTEXTREF:
        case NT_CONTEXT_ROW:
        case NT_CLASSREF:
        case NT_FUNCREF:
        case NT_CLOSURE:
        case NT_RUNTIME_CLOSURE:
        case NT_BACKQUOTE:
            return QoreCompletion();

        case NT_OBJMETHREF: {
            const ParseObjectMethodReferenceNode* r = dynamic_cast<const ParseObjectMethodReferenceNode*>(n);
            return r ? expr(r->getExpression()) : QoreCompletion();
        }

        case NT_PARSEREFERENCE: {
            const ParseReferenceNode* r = dynamic_cast<const ParseReferenceNode*>(n);
            return r ? expr(r->getLVExp()) : QoreCompletion::unknown();
        }

        case NT_PARSE_LIST: {
            const QoreParseListNode* l = dynamic_cast<const QoreParseListNode*>(n);
            if (!l) {
                return QoreCompletion::unknown();
            }
            QoreCompletion rv;
            for (size_t i = 0, e = l->size(); i < e && rv.normal != QR_NO; ++i) {
                rv.then(expr(l->get(i)));
            }
            return rv;
        }

        case NT_PARSE_HASH: {
            const QoreParseHashNode* h = dynamic_cast<const QoreParseHashNode*>(n);
            if (!h) {
                return QoreCompletion::unknown();
            }
            QoreCompletion rv;
            const QoreParseHashNode::nvec_t& keys = h->getKeys();
            const QoreParseHashNode::nvec_t& values = h->getValues();
            for (size_t i = 0, e = keys.size(); i < e && rv.normal != QR_NO; ++i) {
                rv.then(expr(keys[i]));
                if (i < values.size()) {
                    rv.then(expr(values[i]));
                }
            }
            return rv;
        }

        case NT_LIST: {
            const QoreListNode* l = static_cast<const QoreListNode*>(n);
            QoreCompletion rv;
            for (size_t i = 0, e = l->size(); i < e && rv.normal != QR_NO; ++i) {
                rv.then(expr(l->retrieveEntry(i)));
            }
            return rv;
        }

        case NT_HASH: {
            QoreCompletion rv;
            ConstHashIterator i(static_cast<const QoreHashNode*>(n));
            while (i.next() && rv.normal != QR_NO) {
                rv.then(expr(i.get()));
            }
            return rv;
        }

        case NT_FUNCTION_CALL:
        case NT_PROGRAM_FUNC_CALL: {
            const FunctionCallNode* fc = qore_get_function_call_node(n);
            if (!fc) {
                return QoreCompletion::unknown();
            }
            bool user_args;
            QoreCompletion args = callArgs(fc, user_args);
            // the builtin functions of the Qore namespace are known by their name
            const FunctionEntry* fe = fc->getFunctionEntry();
            const qore_ns_private* ns = fe ? fe->getNamespace() : nullptr;
            qore_call_kind_e kind = ns && ns->depth == 1 && ns->name == "Qore" ? QCK_QORE_FUNCTION : QCK_FUNCTION;
            return call(args, functionOutcome(fc->getFunction(), user_args, kind));
        }

        case NT_STATIC_METHOD_CALL: {
            const StaticMethodCallNode* sc = dynamic_cast<const StaticMethodCallNode*>(n);
            if (!sc) {
                return QoreCompletion::unknown();
            }
            bool user_args;
            QoreCompletion args = callArgs(sc, user_args);
            return call(args, methodOutcome(sc->getMethod(), user_args));
        }

        case NT_SELF_CALL: {
            const SelfFunctionCallNode* sc = dynamic_cast<const SelfFunctionCallNode*>(n);
            return sc ? selfCall(sc) : QoreCompletion::unknown();
        }

        case NT_NEW_OBJECT: {
            // the constructor of a builtin class returns; that of a user class is not analyzed
            const NewObjectCallNode* nc = dynamic_cast<const NewObjectCallNode*>(n);
            if (!nc) {
                return QoreCompletion::unknown();
            }
            bool user_args;
            QoreCompletion args = callArgs(nc, user_args);
            return call(args, nc->getClass() && nc->getClass()->isSystem() && !user_args ? QoreCallOutcome::builtin()
                : QoreCallOutcome::unknown());
        }

        case NT_SCOPE_REF: {
            // also the creation of a value of a complex type (new hash<...>(), new list<...>()), which can
            // evaluate the default values of the members of a hashdecl and is not analyzed
            const ScopedObjectCallNode* sc = dynamic_cast<const ScopedObjectCallNode*>(n);
            if (!sc) {
                return QoreCompletion::unknown();
            }
            bool user_args;
            QoreCompletion args = callArgs(sc, user_args);
            return call(args, sc->oc && sc->oc->isSystem() && !user_args ? QoreCallOutcome::builtin()
                : QoreCallOutcome::unknown());
        }

        case NT_OPERATOR: {
            const QoreOperatorNode* op = dynamic_cast<const QoreOperatorNode*>(n);
            return op ? operatorExpr(op) : QoreCompletion::unknown();
        }

        default:
            break;
    }
    return QoreCompletion::unknown();
}

//! Returns the declared return type of the variant if it does not accept NOTHING and the end of its code is
//! certainly reachable, otherwise nullptr
const QoreTypeInfo* qore_missing_return_type(QoreCompletionAnalyzer& a, const UserVariantBase* uvb) {
    const QoreTypeInfo* rti = uvb->getUserSignature()->getReturnTypeInfo();
    if (!QoreTypeInfo::hasType(rti) || QoreTypeInfo::parseAccepts(rti, nothingTypeInfo)) {
        return nullptr;
    }
    const StatementBlock* b = uvb->getStatementBlock();
    if (!b || a.bodyEnd(uvb) != QR_YES) {
        return nullptr;
    }
    return rti;
}

//! Raises MISSING-RETURN for the code of the variant
void qore_raise_missing_return(QoreProgram* pgm, const UserVariantBase* uvb, const QoreTypeInfo* rti) {
    const StatementBlock* b = uvb->getStatementBlock();
    QoreStringNode* desc = new QoreStringNodeMaker("this code block has declared return type '%s' but the end of "
        "the block can be reached without a return statement", QoreTypeInfo::getName(rti));
    qore_program_private::makeParseException(pgm, *b->loc, "MISSING-RETURN", desc);
}
}

bool QoreMissingReturnChecks::defer(const UserVariantBase* uvb) {
    if (!active) {
        return false;
    }
    AbstractQoreFunctionVariant* v = const_cast<AbstractQoreFunctionVariant*>(uvb->getAbstractFunctionVariant());
    pending.reserve(pending.size() + 1);
    pending.push_back(v->ref());
    return true;
}

void QoreMissingReturnChecks::run(QoreProgram* pgm) {
    active = false;
    std::vector<AbstractQoreFunctionVariant*> checks;
    checks.swap(pending);
    // releases the variants, also if a check throws
    class ReleaseHelper {
    public:
        DLLLOCAL ReleaseHelper(std::vector<AbstractQoreFunctionVariant*>& v) : v(v) {
        }
        DLLLOCAL ~ReleaseHelper() {
            for (AbstractQoreFunctionVariant* i : v) {
                i->deref();
            }
        }
    private:
        std::vector<AbstractQoreFunctionVariant*>& v;
    } rh(checks);

    // the errors are raised in the order of their locations
    std::vector<std::pair<const UserVariantBase*, const QoreTypeInfo*>> missing;
    QoreCompletionAnalyzer a(pgm, *this);
    for (AbstractQoreFunctionVariant* v : checks) {
        const UserVariantBase* uvb = v->getUserVariantBase();
        const QoreTypeInfo* rti = qore_missing_return_type(a, uvb);
        if (rti) {
            missing.push_back(std::make_pair(uvb, rti));
        }
    }
    typedef std::pair<const UserVariantBase*, const QoreTypeInfo*> missing_t;
    std::stable_sort(missing.begin(), missing.end(), [] (const missing_t& l, const missing_t& r) -> bool {
        const QoreProgramLocation* ll = l.first->getStatementBlock()->loc;
        const QoreProgramLocation* rl = r.first->getStatementBlock()->loc;
        int c = strcmp(ll->getFileValue() ? ll->getFileValue() : "", rl->getFileValue() ? rl->getFileValue() : "");
        if (c) {
            return c < 0;
        }
        return ll->start_line < rl->start_line;
    });
    for (auto& i : missing) {
        qore_raise_missing_return(pgm, i.first, i.second);
    }
    // the cache is cleared at the end of the parse (see clear())
}

void QoreMissingReturnChecks::clear() noexcept {
    active = false;
    clearCache();
    for (AbstractQoreFunctionVariant* v : pending) {
        v->deref();
    }
    pending.clear();
}

int qore_parse_check_missing_return(QoreProgram* pgm, const UserVariantBase* uvb) {
    QoreCompletionAnalyzer a(pgm, qore_program_private::get(*pgm)->getMissingReturnChecks());
    const QoreTypeInfo* rti = qore_missing_return_type(a, uvb);
    if (!rti) {
        return 0;
    }
    qore_raise_missing_return(pgm, uvb, rti);
    return -1;
}
