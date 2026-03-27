/*
    QoreAssignmentOperatorNode.cpp

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

#include <qore/Qore.h>

#include "qore/intern/qore_program_private.h"
#include "qore/intern/LocalVar.h"
#include "qore/intern/Variable.h"

QoreString QoreAssignmentOperatorNode::op_str("assignment (=) operator expression");
QoreString QoreWeakAssignmentOperatorNode::op_str("weak assignment (:=) operator expression");

int QoreAssignmentOperatorNode::parseInitIntern(QoreParseContext& parse_context, bool weak_assignment) {
    // turn off "reference ok" and "return value ignored" flags
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    int err;
    {
        QoreParseContextFlagHelper fh0(parse_context);
        fh0.setFlags(PF_FOR_ASSIGNMENT);
        err = parse_init_value(left, parse_context);
        // Preserve the PF_NARROWED_TYPE flag if set during left side parsing
        fh0.preserveFlags(PF_NARROWED_TYPE);
    }
    // return type info is the same as the lvalue's typeinfo
    ti = parse_context.typeInfo;

    //printd(0, "QoreAssignmentOperatorNode::parseInitImpl() this: %p left: '%s' nt: %d ti: %p '%s'\n", this,
    //    left.getFullTypeName(), left.getType(), ti, QoreTypeInfo::getName(ti));
    if (!err && checkLValue(left, parse_context.pflag)) {
        err = -1;
    }

    // if "broken-int-assignments" is set, then set flag if applicable
    if ((ti == bigIntTypeInfo || ti == softBigIntTypeInfo)
        && (parse_context.pgm->getParseOptions() & PO_BROKEN_INT_ASSIGNMENTS)) {
        broken_int = true;
    }

    parse_context.typeInfo = nullptr;
    QoreParseAnalysis right_analysis;
    if (parse_init_value(right, parse_context) && !err) {
        err = -1;
    }
    right_analysis = parse_context.analysis;

    // check for illegal assignment to $self
    if (parse_context.oflag && check_self_assignment(loc, left, parse_context.oflag) && !err) {
        err = -1;
    }

    //printd(5, "QoreAssignmentOperatorNode::parseInitImpl() this: %p left: %s ti: %p '%s', right: %s ti: %s\n",
    //  this, get_type_name(left), ti, QoreTypeInfo::getName(ti), get_type_name(right),
    //  QoreTypeInfo::getName(parse_context.typeInfo));

    // issue #3337: make sure that the two varrefs are pointing to the same variable
    if (left.getType() == NT_VARREF && right.getType() == NT_VARREF
        && left.get<VarRefNode>()->parseEqualTo(*right.get<VarRefNode>())) {
        qore_program_private::makeParseException(parse_context.pgm, *loc, "PARSE-EXCEPTION",
            new QoreStringNodeMaker("illegal assignment of variable \"%s\" to itself",
                left.get<VarRefNode>()->getName()));
        if (!err) {
            err = -1;
        }
    }

    qore_type_result_e res;
    if (ti == autoTypeInfo) {
        if (parse_context.pflag & PF_FOR_ASSIGNMENT) {
            ident = true;
        }
        res = QTI_IDENT;
    } else if (QoreTypeInfo::hasType(ti)) {
        // For reference-typed lvalues, dereference before type-checking.
        // Assignment through a reference targets the referenced location, not the reference itself.
        // - Write-through (ref = val): LHS reference<T> → T; RHS is plain T — compare T vs T
        // - Initial binding (ref = \var): LHS reference<T> → T; RHS reference<T> → T — compare T vs T
        const QoreTypeInfo* check_ti = ti;
        const QoreTypeInfo* check_rhs_ti = parse_context.typeInfo;

        // For unassigned variables, use declared type instead of parse-time nullptr
        // This ensures type mismatches are caught in assignment checking
        if (!check_rhs_ti && right.getType() == NT_VARREF) {
            VarRefNode* vrn = right.get<VarRefNode>();
            qore_var_t vtype = vrn->getType();
            if (vtype == VT_LOCAL || vtype == VT_CLOSURE || vtype == VT_LOCAL_TS) {
                LocalVar* lvar = vrn->ref.id;
                if (lvar) {
                    check_rhs_ti = lvar->getTypeInfo();
                }
            } else if (vtype == VT_GLOBAL || vtype == VT_THREAD_LOCAL) {
                Var* gvar = vrn->ref.var;
                if (gvar) {
                    check_rhs_ti = gvar->getTypeInfo();
                }
            }
        }

        if (QoreTypeInfo::isReference(ti)) {
            const QoreTypeInfo* deref_ti = QoreTypeInfo::getReferenceTarget(ti);
            if (deref_ti) {
                check_ti = deref_ti;
            }
        }
        // Dereference RHS if it's a reference — for transparent reference variable access
        // and for initial binding (reference<T> r = \var).
        // Exception: do NOT dereference a reference-creation expression (\expr) when LHS is
        // not a reference type — this catches invalid assignments like:
        //   softint i = \i;  (self-reference to uninitialized variable)
        //   softint x = \n;  (assigning reference creation to a non-reference variable)
        if (QoreTypeInfo::isReference(check_rhs_ti)) {
            // Only dereference ParseReferenceNode (\expr) when LHS is also a reference type
            if (right.getType() != NT_PARSEREFERENCE || QoreTypeInfo::isReference(ti)) {
                const QoreTypeInfo* deref_rhs_ti = QoreTypeInfo::getReferenceTarget(check_rhs_ti);
                if (deref_rhs_ti) {
                    check_rhs_ti = deref_rhs_ti;
                }
            }
        }

        bool may_not_match = false;
        bool may_need_filter = false;
        qore_type_result_e max_result = QTI_NOT_EQUAL;
        // only set the initial assignment flag if the lvalue is a declaration
        bool initial_assignment = (left.getType() == NT_VARREF && left.get<VarRefNode>()->parseIsDecl());
        res = QoreTypeInfo::parseAccepts(check_ti, check_rhs_ti, may_not_match, may_need_filter, max_result,
            initial_assignment);
        // issue #2106 do not set the ident flag for any other type in case runtime types are more specific (complex)
        // than parse types and require filtering
        //printd(5, "QoreAssignmentOperatorNode::parseInitImpl() '%s' <- '%s' res: %d may_not_match: %d "
        //    "may_need_filter: %d ident: %d\n", QoreTypeInfo::getName(ti),
        //    QoreTypeInfo::getName(parse_context.typeInfo), res, may_not_match, may_need_filter, ident);

        // Additional check for typed callable signature compatibility
        if (res && !QoreTypeInfo::checkComplexCodeCompatibility(check_ti, check_rhs_ti)) {
            res = QTI_NOT_EQUAL;
        }
    } else {
        res = QTI_AMBIGUOUS;
    }

    // Check for type mismatch - either with parse exception sink enabled, or for narrowed types
    bool type_mismatch = !res;

    // Check if the lvalue involves a narrowed auto-type (via PF_NARROWED_TYPE flag)
    bool has_narrowed_type = (parse_context.pflag & PF_NARROWED_TYPE) != 0;

    // Check if this is a direct assignment to an auto-type variable
    // Direct assignment to auto-type variables should NOT raise narrowed type errors
    // because the assignment updates the narrowed type (not a member assignment)
    bool is_direct_auto_assignment = false;
    if (left.getType() == NT_VARREF) {
        VarRefNode* vrn = left.get<VarRefNode>();
        qore_var_t vtype = vrn->getType();
        if (vtype == VT_LOCAL || vtype == VT_CLOSURE || vtype == VT_LOCAL_TS) {
            LocalVar* lvar = vrn->ref.id;
            if (lvar && lvar->isAutoType()) {
                is_direct_auto_assignment = true;
            }
        } else if (vtype == VT_GLOBAL || vtype == VT_THREAD_LOCAL) {
            Var* gvar = vrn->ref.var;
            if (gvar && gvar->isAutoType()) {
                is_direct_auto_assignment = true;
            }
        }
    }

    // Raise parse exception for type mismatch
    // - For direct auto-type assignments, check against declared type (not narrowed)
    // - Always raise for narrowed types (unless PO_BROKEN_NARROWED_TYPES is set)
    // - But NOT for direct assignment to auto-type variables (reassignment updates the type)
    // - Otherwise only raise if parse exception sink is enabled
    bool raise_exception = false;
    const QoreTypeInfo* error_ti = ti;  // Type info to use in error message
    const QoreTypeInfo* error_rhs_ti = parse_context.typeInfo;

    if (type_mismatch) {
        if (is_direct_auto_assignment) {
            // For direct auto-type assignments, re-check against the declared type
            // The narrowed type will be updated by this assignment, so we only need
            // to verify compatibility with the declared type
            const QoreTypeInfo* declared_ti = nullptr;
            VarRefNode* vrn = left.get<VarRefNode>();
            qore_var_t vtype = vrn->getType();
            if (vtype == VT_LOCAL || vtype == VT_CLOSURE || vtype == VT_LOCAL_TS) {
                LocalVar* lvar = vrn->ref.id;
                if (lvar) {
                    declared_ti = lvar->getTypeInfo();
                }
            } else if (vtype == VT_GLOBAL || vtype == VT_THREAD_LOCAL) {
                Var* gvar = vrn->ref.var;
                if (gvar) {
                    declared_ti = gvar->getTypeInfo();
                }
            }

            if (declared_ti) {
                // Dereference for type checking if it's a reference type
                const QoreTypeInfo* check_declared_ti = declared_ti;
                const QoreTypeInfo* check_declared_rhs_ti = parse_context.typeInfo;
                if (QoreTypeInfo::isReference(declared_ti)) {
                    const QoreTypeInfo* deref_declared = QoreTypeInfo::getReferenceTarget(declared_ti);
                    if (deref_declared) {
                        check_declared_ti = deref_declared;
                    }
                }
                if (QoreTypeInfo::isReference(check_declared_rhs_ti)) {
                    const QoreTypeInfo* deref_declared_rhs = QoreTypeInfo::getReferenceTarget(check_declared_rhs_ti);
                    if (deref_declared_rhs) {
                        check_declared_rhs_ti = deref_declared_rhs;
                    }
                }

                // Re-check compatibility against declared type
                bool may_not_match = false;
                bool may_need_filter = false;
                qore_type_result_e max_result = QTI_NOT_EQUAL;
                qore_type_result_e declared_res = QoreTypeInfo::parseAccepts(check_declared_ti,
                    check_declared_rhs_ti, may_not_match, may_need_filter, max_result);
                // Only raise error if incompatible with declared type and parse exception sink is enabled
                if (!declared_res && parse_context.pgm->getParseExceptionSink()) {
                    raise_exception = true;
                    error_ti = declared_ti;
                    error_rhs_ti = parse_context.typeInfo;
                }
            }
        } else {
            // Not a direct auto assignment - use normal logic
            raise_exception = parse_context.pgm->getParseExceptionSink() ||
                (has_narrowed_type &&
                 !(parse_context.pgm->getParseOptions() & PO_BROKEN_NARROWED_TYPES));
        }
    }

    if (raise_exception) {
        QoreStringNode* edesc = new QoreStringNodeMaker("lvalue for %sassignment operator '%s' expects ",
            weak_assignment ? "weak " : "", weak_assignment ? ":=" : "=");
        QoreTypeInfo::getThisType(error_ti, *edesc);
        edesc->concat(", but right-hand side is ");
        QoreTypeInfo::getThisType(error_rhs_ti, *edesc);

        // Add context about type narrowing if applicable
        if (has_narrowed_type && !is_direct_auto_assignment) {
            edesc->concat("; the container's element type was inferred from the initial value; "
                "to use mixed types, include values of all needed types in the initial assignment, "
                "or use hash<auto!> or list<auto!> to disable type narrowing for the variable; "
                "note: %broken-narrowed-types will suppress this error but move it to runtime");
        }

        qore_program_private::makeParseException(parse_context.pgm, *loc, "PARSE-TYPE-ERROR", edesc);
        if (!err) {
            err = -1;
        }
    }

    // Update narrowed type for auto-typed variables
    // Direct assignment replaces the narrowed type completely (use parseSetNarrowedType)
    // Branch merging is handled by NarrowedTypeHelper which uses parseMergeNarrowedType
    if (!err && left.getType() == NT_VARREF) {
        VarRefNode* vrn = left.get<VarRefNode>();
        qore_var_t vtype = vrn->getType();
        // Get the type to narrow to: use inferred type if available, otherwise use declared type
        const QoreTypeInfo* narrow_type = parse_context.typeInfo;
        if (!narrow_type && right.getType() == NT_VARREF) {
            // If assigning from a variable reference with no inferred type (e.g., unassigned),
            // use the declared type of the variable
            VarRefNode* right_vrn = right.get<VarRefNode>();
            qore_var_t right_vtype = right_vrn->getType();
            if (right_vtype == VT_LOCAL || right_vtype == VT_CLOSURE || right_vtype == VT_LOCAL_TS) {
                LocalVar* right_lvar = right_vrn->ref.id;
                if (right_lvar) {
                    narrow_type = right_lvar->getTypeInfo();
                }
            } else if (right_vtype == VT_GLOBAL || right_vtype == VT_THREAD_LOCAL) {
                Var* right_gvar = right_vrn->ref.var;
                if (right_gvar) {
                    narrow_type = right_gvar->getTypeInfo();
                }
            }
        }
        if (vtype == VT_LOCAL || vtype == VT_CLOSURE || vtype == VT_LOCAL_TS) {
            LocalVar* lvar = vrn->ref.id;
            if (lvar && lvar->isAutoType() && QoreTypeInfo::hasType(narrow_type)) {
                // Direct assignment replaces the narrowed type, store location for error messages
                lvar->parseSetNarrowedType(narrow_type, loc);
            }
        } else if (vtype == VT_GLOBAL || vtype == VT_THREAD_LOCAL) {
            Var* gvar = vrn->ref.var;
            if (gvar && gvar->isAutoType() && QoreTypeInfo::hasType(narrow_type)) {
                // Direct assignment replaces the narrowed type, store location for error messages
                gvar->parseSetNarrowedType(narrow_type, loc);
            }
        }
    }

    parse_context.typeInfo = ti;
    parse_context.analysis.clear();
    if (parse_context.typeInfo) {
        parse_context.analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
        parse_context.analysis.known_type = parse_context.typeInfo;
        if (right_analysis.hasFlag(QoreParseAnalysis::NeverNothing)
            && QoreTypeInfo::parseReturns(parse_context.typeInfo, NT_NOTHING) == QTI_NOT_EQUAL) {
            parse_context.analysis.setFlag(QoreParseAnalysis::NeverNothing);
        }
    } else if (right_analysis.hasFlag(QoreParseAnalysis::KnownTypeInfo)) {
        parse_context.analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
        parse_context.analysis.known_type = right_analysis.known_type;
        if (right_analysis.hasFlag(QoreParseAnalysis::NeverNothing)) {
            parse_context.analysis.setFlag(QoreParseAnalysis::NeverNothing);
        }
    }
    if (right_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned)) {
        parse_context.analysis.setFlag(QoreParseAnalysis::DefinitelyAssigned);
    }
    return err;
}

QoreValue QoreAssignmentOperatorNode::evalIntern(ExceptionSink* xsink, bool& needs_deref,
        bool weak_assignment) const {
    /* assign new value, this value gets referenced with the
        eval(xsink) call, so there's no need to reference it again
        for the variable assignment - however it does need to be
        copied/referenced for the return value
    */
    ValueEvalOptimizedRefHolder new_value(right, xsink);
    if (*xsink)
        return QoreValue();

    if (broken_int) {
        // convert the value to an int unconditionally
        new_value.setValue(new_value->getAsBigInt());
        if (*xsink)
            return QoreValue();
    } else {
        // we have to ensure that the value is referenced before the assignment in case the lvalue
        // is the same value, so it can be copied in the LValueHelper constructor
        new_value.ensureReferencedValue();
    }

    // get ptr to current value (lvalue is locked for the scope of the LValueHelper object)
    LValueHelper v(left, xsink);
    if (!v)
        return QoreValue();

    assert(!*xsink);

    // assign new value
    if (v.assign(new_value.takeReferencedValue(), "<lvalue>", !ident, weak_assignment))
        return QoreValue();

    // reference return value if necessary
    return ref_rv ? v.getReferencedValue() : QoreValue();
}
