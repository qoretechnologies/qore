/*
    SwitchStatement.cpp

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
#include <qore/QoreEnumDecl.h>
#include "qore/intern/SwitchStatement.h"
#include "qore/intern/StatementBlock.h"
#include "qore/intern/CaseNodeWithOperator.h"
#include "qore/intern/CaseNodeRegex.h"
#include "qore/intern/qore_aot_deps.h"
#include "qore/intern/qore_program_private.h"

CaseNode::~CaseNode() {
    val.discard(nullptr);
    delete code;
}

bool CaseNode::isCaseNodeImpl() const {
    return !def;
}

bool qore_switch_case_equal(QoreValue lhs_value, QoreValue rhs_value, ExceptionSink* xsink) {
    // Unwrap TAG_ENUM for switch/case comparison so enum constants match base type values
    QoreValue lhs = lhs_value.isEnum() ? lhs_value.getEnumMember()->getValue() : lhs_value;
    QoreValue rhs = rhs_value.isEnum() ? rhs_value.getEnumMember()->getValue() : rhs_value;

    if (lhs.isEqualHard(rhs)) {
        return true;
    }

    qore_type_t lt = lhs.getType();
    qore_type_t rt = rhs.getType();
    if ((lt == NT_CHAR && rt == NT_STRING) || (lt == NT_STRING && rt == NT_CHAR)) {
        ExceptionSink local_xsink;
        ExceptionSink* eq_xsink = xsink ? xsink : &local_xsink;
        bool rv = QoreLogicalEqualsOperatorNode::softEqual(lhs, rhs, eq_xsink);
        if (!xsink && local_xsink) {
            local_xsink.clear();
        }
        return rv;
    }

    return false;
}

bool CaseNode::matches(QoreValue lhs_value, ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder case_val(val, xsink);
    if (xsink && *xsink) {
        return false;
    }
    QoreValue eval_case_val = case_val.takeReferencedValue();
    bool rv = qore_switch_case_equal(lhs_value, eval_case_val, xsink);
    eval_case_val.discard(xsink);
    return rv;
}

bool CaseNode::isCaseNode() const {
    return isCaseNodeImpl();
}

// start and end line are set later
SwitchStatement::SwitchStatement(CaseNode *f) : AbstractStatement(-1, -1), head(f), tail(f),
    deflt(f->isDefault() ? f : nullptr) {
}

SwitchStatement::~SwitchStatement() {
    while (head) {
        CaseNode *w = head->next;
        delete head;
        head = w;
    }
    sexp.discard(nullptr);
    delete lvars;
}

void SwitchStatement::setSwitch(QoreValue s) {
    sexp = s;
}

void SwitchStatement::addCase(CaseNode *c) {
    if (tail)
        tail->next = c;
    else
        head = c;
    tail = c;
    if (c->isDefault()) {
        if (deflt)
            parse_error(*c->loc, "multiple defaults in switch statement");
        deflt = c;
    }
}

int SwitchStatement::execImpl(QoreValue& return_value, ExceptionSink *xsink) {
    int rc = 0;

    // instantiate local variables
    LVListInstantiator lvi(xsink, lvars, pwo.parse_options);

    ValueEvalOptimizedRefHolder se(sexp, xsink);

    if (!*xsink) {
        // find match
        CaseNode *w = head;
        while (w) {
            if (w->matches(*se, xsink)) {
                break;
            }
            w = w->next;
        }
        if (!w && deflt) {
            w = deflt;
        }

        while (w && !rc && !*xsink) {
            if (w->code) {
                rc = w->code->execImpl(return_value, xsink);
            }

            w = w->next;
        }

        if (rc == RC_BREAK
            || ((getProgram()->getParseOptions() & PO_BROKEN_LOOP_STATEMENT) != 0 && rc == RC_CONTINUE)) {
            rc = 0;
        }
    }

    return rc;
}

int SwitchStatement::execImpl(RuntimeConfig& rc, QoreValue& return_value, ExceptionSink* xsink) {
    int rc_state = 0;

    // instantiate local variables
    LVListInstantiator lvi(xsink, lvars, pwo.parse_options);

    ValueEvalRefHolder se(rc, sexp, xsink);

    if (!*xsink) {
        // find match
        CaseNode* w = head;
        while (w) {
            if (w->matches(*se, xsink)) {
                break;
            }
            w = w->next;
        }
        if (!w && deflt) {
            w = deflt;
        }

        while (w && !rc_state && !*xsink) {
            if (w->code) {
                rc_state = w->code->execImpl(rc, return_value, xsink);
            }

            w = w->next;
        }

        if (rc_state == RC_BREAK
            || ((getProgram()->getParseOptions() & PO_BROKEN_LOOP_STATEMENT) != 0 && rc_state == RC_CONTINUE)) {
            rc_state = 0;
        }
    }

    return rc_state;
}

// raises a warning if the switch operand has an enum type, there is no default case, and one or
// more enum members are not handled by a simple (non-relational) case value; helps human and AI
// coders catch unhandled cases when an enum gains a new member
static void warnNonExhaustiveSwitch(QoreProgram* pgm, const QoreProgramLocation* loc,
        const QoreTypeInfo* switch_type_info, const CaseNode* head) {
    const QoreEnumDecl* ed = QoreTypeInfo::getReturnEnum(switch_type_info);
    if (!ed) {
        return;
    }

    // if any case uses a relational or regex match (not a simple value), coverage cannot be
    // determined reliably, so do not warn
    for (const CaseNode* w = head; w; w = w->next) {
        if (!w->isDefault() && !w->isCaseNode()) {
            return;
        }
    }

    // collect the enum members not covered by any simple case value
    ExceptionSink xsink;
    QoreString missing;
    unsigned missing_count = 0;
    QoreEnumMemberIterator mi(*ed);
    while (mi.next()) {
        const QoreValue mval = mi.getValue();
        bool covered = false;
        for (const CaseNode* w = head; w; w = w->next) {
            if (w->isDefault() || !w->isCaseNode()) {
                continue;
            }
            if (qore_switch_case_equal(mval, w->val, &xsink)) {
                covered = true;
                break;
            }
            // ignore any comparison error; such case values simply do not match this member
            if (xsink) {
                xsink.clear();
            }
        }
        if (!covered) {
            if (missing_count++) {
                missing.concat(", ");
            }
            missing.concat(mi.getName());
        }
    }

    if (missing_count) {
        qore_program_private::makeParseWarning(pgm, *loc, QP_WARN_NONEXHAUSTIVE_SWITCH,
            "NON-EXHAUSTIVE-SWITCH",
            "switch over enum '%s' does not handle %d of its values (%s); add the missing "
            "case%s or a default: label", ed->getName(), missing_count, missing.c_str(),
            missing_count == 1 ? "" : "s");
    }
}

int SwitchStatement::parseInitImpl(QoreParseContext& parse_context) {
    // turn off top-level flag for statement vars
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_TOP_LEVEL);

    // saves local variables after parsing
    QoreParseContextLvarHelper lh(parse_context, lvars);

    parse_context.typeInfo = nullptr;
    int err = parse_init_value(sexp, parse_context);

    // save the type of the switch operand for enum-exhaustiveness analysis below; the case loop
    // overwrites parse_context.typeInfo
    const QoreTypeInfo* switch_type_info = parse_context.typeInfo;

    // Track parse facts across switch branches
    NarrowedTypeHelper nth;
    AssignedStateHelper ash;
    nth.saveState();
    ash.saveState();

    CaseNode* w = head;
    ExceptionSink xsink;
    while (w) {
        {
            QoreParseContextFlagHelper fh0(parse_context);
            fh0.setFlags(PF_CONST_EXPRESSION);

            parse_context.typeInfo = nullptr;
            if (parse_init_value(w->val, parse_context) && !err) {
                err = -1;
            }
        }
        if (parse_context.lvids) {
            parse_error(*w->loc, "illegal local variable declaration in assignment expression for case block");
            while (parse_context.lvids--) {
                pop_local_var();
            }
            if (!err) {
                err = -1;
            }

            w = w->next;
            continue;
        }

        // evaluate case expression if necessary and no parse expressions have been raised
        if (!w->val.isValue()) {
            if (err || parse_context.pgm->parseExceptionRaised()) {
                w = w->next;
                continue;
            }

            ValueEvalOptimizedRefHolder se(w->val, &xsink);
            if (!xsink) {
                QoreValue nv = se.takeReferencedValue();
                w->val.discard(nullptr);
                w->val = nv;
            } else if (qore_aot_source_parse_active()) {
                QoreValue ex_err = xsink.getExceptionErr();
                bool defer_case = false;
                if (ex_err.getType() == NT_STRING) {
                    QoreStringValueHelper ex_err_str(ex_err);
                    defer_case = !strcmp(ex_err_str->c_str(), "AOT-PENDING-CONSTANT");
                }
                if (defer_case) {
                    xsink.clear();
                } else {
                    qore_program_private::addParseException(parse_context.pgm, xsink);
                }
            } else {
                qore_program_private::addParseException(parse_context.pgm, xsink);
            }
        }
        //printd(5, "SwitchStatement::parseInit() this=%p case exp: %p %s\n", this, w->val, get_type_name(w->val));

        // check for duplicate values
        CaseNode* cw = head;
        while (cw != w) {
            // Check only the simple case blocks (case 1: ...),
            // not those with relational operators. Could be changed later to provide more checking.
            // note that no exception can be raised here as the case node values are parse values
            if (w->isCaseNode() && cw->isCaseNode() && w->val.isValue() && cw->val.isValue()) {
                bool duplicate = qore_switch_case_equal(w->val, cw->val, &xsink);
                if (xsink) {
                    qore_program_private::addParseException(parse_context.pgm, xsink);
                    if (!err) {
                        err = -1;
                    }
                } else if (duplicate) {
                    parse_error(*w->loc, "duplicate case values in switch");
                    if (!err) {
                        err = -1;
                    }
                }
            }
            assert(!xsink);
            cw = cw->next;
        }

        if (w->code) {
            QoreParseContextFlagHelper fh0(parse_context);
            fh0.setFlags(PF_BREAK_OK);

            if (w->code->parseInitImpl(parse_context) && !err) {
                err = -1;
            }
            // Record facts after this case block and restore for next case
            nth.recordBranchAndRestore();
            ash.recordBranchAndRestore();
        }
        w = w->next;
    }

    // If there's no default case, the implicit "no match" path preserves original types
    if (!deflt) {
        nth.recordSavedAsImplicitBranch();
        ash.recordSavedAsImplicitBranch();
    }

    // Merge facts from all branches
    nth.mergeAndApply();
    ash.mergeAndApply();

    // warn if the switch is over an enum value, has no default case, and does not handle all
    // enum members (only when the switch parsed cleanly, to avoid noise on broken code)
    if (!err && !deflt && !parse_context.pgm->parseExceptionRaised()) {
        warnNonExhaustiveSwitch(parse_context.pgm, loc, switch_type_info, head);
    }

    return err;
}

void SwitchStatement::parseCommit(QoreProgram* pgm) {
    AbstractStatement::parseCommit(pgm);
    CaseNode* w = head;
    while (w) {
        if (w->code) {
            w->code->parseCommit(pgm);
        }
        w = w->next;
    }
}

CaseNodeWithOperator::CaseNodeWithOperator(const QoreProgramLocation* loc, QoreValue v, StatementBlock* c,
        op_log_func_t op) : CaseNode(loc, v, c), op_func(op) {
}

bool CaseNodeWithOperator::isCaseNodeImpl() const {
    return false;
}

bool CaseNodeWithOperator::matches(QoreValue lhs_value, ExceptionSink* xsink) const {
    return op_func(lhs_value, val, xsink);
}

CaseNodeRegex::CaseNodeRegex(const QoreProgramLocation* loc, QoreRegex* m_re, StatementBlock* blk)
        : CaseNode(loc, QoreValue(), blk), re(m_re) {
}

bool CaseNodeRegex::matches(QoreValue lhs_value, ExceptionSink* xsink) const {
    QoreStringValueHelper str(lhs_value);

    return re->exec(*str, xsink);
}

bool CaseNodeNegRegex::matches(QoreValue lhs_value, ExceptionSink* xsink) const {
    QoreStringValueHelper str(lhs_value);

    return !re->exec(*str, xsink);
}
