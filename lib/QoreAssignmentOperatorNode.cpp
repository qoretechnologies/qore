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
#include "qore/intern/QoreStatementTemps.h"

#include "qore/intern/qore_program_private.h"
#include "qore/intern/LocalVar.h"
#include "qore/intern/QoreHashObjectDereferenceOperatorNode.h"
#include "qore/intern/QoreMapOperatorNode.h"
#include "qore/intern/QoreSquareBracketsOperatorNode.h"
#include "qore/intern/QoreQuestionMarkOperatorNode.h"
#include "qore/intern/QoreNullCoalescingOperatorNode.h"
#include "qore/intern/QoreValueCoalescingOperatorNode.h"
#include "qore/intern/Variable.h"
#include "qore/intern/WeakHashReferenceNode.h"
#include "qore/intern/WeakListReferenceNode.h"
#include "qore/intern/StaticClassVarRefNode.h"
#include "qore/intern/SelfVarrefNode.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/QoreParseListNode.h"
#include "qore/intern/QoreProgramObjectLockOps.h"

QoreString QoreAssignmentOperatorNode::op_str("assignment (=) operator expression");
QoreString QoreWeakAssignmentOperatorNode::op_str("weak assignment (:=) operator expression");
QoreString QoreOpaqueAssignmentOperatorNode::op_str("opaque assignment (@=) operator expression");

static const VarRefNode* get_lvalue_root_var_ref(QoreValue value) {
    unsigned depth = 0;
    while (value.hasNode()) {
        if (++depth % 100 == 0 && qore_check_cancel(nullptr, "parse lvalue narrowing analysis")) {
            return nullptr;
        }
        if (value.getType() == NT_VARREF) {
            return value.get<const VarRefNode>();
        }
        const AbstractQoreNode* node = value.getInternalNode();
        if (const auto* op = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(node)) {
            value = op->getLeft();
            continue;
        }
        if (const auto* op = qore_exact_node_cast<QoreSquareBracketsOperatorNode>(node)) {
            value = op->getLeft();
            continue;
        }
        break;
    }
    return nullptr;
}

static const char* get_type_name(const QoreTypeInfo* type_info) {
    const char* name = QoreTypeInfo::getName(type_info);
    return name ? name : "unknown";
}

static bool is_optional_map_result_mismatch(const QoreValue& right, const QoreTypeInfo* expected,
        const QoreTypeInfo* actual) {
    if (!right.hasNode() || !dynamic_cast<const QoreMapOperatorNode*>(right.getInternalNode())) {
        return false;
    }
    bool expected_is_list = QoreTypeInfo::isType(expected, NT_LIST)
        || QoreTypeInfo::getReturnComplexListOrNothing(expected);
    bool actual_is_list_or_nothing = QoreTypeInfo::getReturnComplexListOrNothing(actual);
    return expected_is_list && actual_is_list_or_nothing
        && QoreTypeInfo::parseReturns(expected, NT_NOTHING) == QTI_NOT_EQUAL
        && QoreTypeInfo::parseReturns(actual, NT_NOTHING) != QTI_NOT_EQUAL;
}

bool qore_is_narrowed_lvalue(const QoreValue& left) {
    const VarRefNode* root = get_lvalue_root_var_ref(left);
    if (!root) {
        return false;
    }

    qore_var_t variable_type = root->getType();
    if (variable_type == VT_LOCAL || variable_type == VT_CLOSURE || variable_type == VT_LOCAL_TS) {
        const LocalVar* variable = root->ref.id;
        return variable && variable->isAutoType() && !variable->isNoNarrowing()
            && variable->parseGetNarrowedType();
    }
    if (variable_type == VT_GLOBAL || variable_type == VT_THREAD_LOCAL) {
        const Var* variable = root->ref.var;
        return variable && variable->isAutoType() && !variable->isNoNarrowing()
            && variable->parseGetNarrowedType();
    }
    return false;
}

QoreDiagnosticMetadata qore_make_lvalue_type_diagnostic(const QoreValue& left,
        const QoreTypeInfo* expected, const QoreTypeInfo* actual, bool narrowed, const char* base_id,
        const char* operation) {
    QoreDiagnosticMetadata metadata(narrowed ? "NARROWED-CONTAINER-TYPE-MISMATCH" : base_id);
    metadata.addFact("expectedType", get_type_name(expected));
    metadata.addFact("actualType", get_type_name(actual));
    metadata.addFact("operation", operation);

    if (!narrowed) {
        QoreStringMaker hint("use a value compatible with '%s' or change the explicitly declared type",
            get_type_name(expected));
        metadata.hint = hint.c_str();
        return metadata;
    }

    metadata.addFact("narrowed", "true");
    const VarRefNode* root = get_lvalue_root_var_ref(left);
    const QoreTypeInfo* declared = nullptr;
    const QoreTypeInfo* effective = nullptr;
    const QoreProgramLocation* narrowed_loc = nullptr;
    const char* variable_name = nullptr;
    if (root) {
        qore_var_t variable_type = root->getType();
        if (variable_type == VT_LOCAL || variable_type == VT_CLOSURE || variable_type == VT_LOCAL_TS) {
            const LocalVar* variable = root->ref.id;
            if (variable) {
                declared = variable->getTypeInfo();
                effective = variable->parseGetNarrowedType();
                narrowed_loc = variable->parseGetNarrowedLoc();
                variable_name = variable->getName();
            }
        } else if (variable_type == VT_GLOBAL || variable_type == VT_THREAD_LOCAL) {
            const Var* variable = root->ref.var;
            if (variable) {
                declared = variable->getTypeInfo();
                effective = variable->parseGetNarrowedType();
                narrowed_loc = variable->parseGetNarrowedLoc();
                variable_name = variable->getName();
            }
        }
    }

    const char* recommended_type = "auto!";
    const char* container = "container";
    const QoreTypeInfo* container_type = effective ? effective : declared;
    if (QoreTypeInfo::isHashType(container_type)) {
        container = "hash";
        recommended_type = declared == autoHashOrNothingTypeInfo ? "*hash<auto!>" : "hash<auto!>";
    } else if (QoreTypeInfo::isListType(container_type)) {
        container = "list";
        recommended_type = declared == autoListOrNothingTypeInfo ? "*list<auto!>" : "list<auto!>";
    }

    metadata.addFact("containerKind", container);
    metadata.addFact("recommendedType", recommended_type);
    metadata.addSuggestion(recommended_type);
    if (variable_name) {
        metadata.addFact("variable", variable_name);
    }
    if (declared) {
        metadata.addFact("declaredType", get_type_name(declared));
    }
    if (effective) {
        metadata.addFact("effectiveType", get_type_name(effective));
    }
    if (narrowed_loc) {
        metadata.addRelatedLocation("narrowing-initializer", *narrowed_loc,
            "this assignment established the container's effective element type");
    }

    if (variable_name) {
        QoreStringMaker hint("the %s variable '%s' was narrowed by an earlier assignment; declare it as '%s' "
            "only if it is intentionally heterogeneous", container, variable_name, recommended_type);
        metadata.hint = hint.c_str();
    } else {
        QoreStringMaker hint("the %s was narrowed by an earlier assignment; declare it as '%s' only if it is "
            "intentionally heterogeneous", container, recommended_type);
        metadata.hint = hint.c_str();
    }
    metadata.addFix(QoreStringMaker("declare the %s as '%s'", container, recommended_type).c_str());
    return metadata;
}

//! records the local variables that a weak assignment of \a expr can take a weak reference to the container of
/** the variable itself, and the variables that the branches of a conditional (<tt>c ? a : b</tt>) and the operands of
    the coalescing operators (<tt>a ?? b</tt>, <tt>a ?: b</tt>) can give (qore#5846)
*/
static void qore_mark_weak_assignment_sources(const QoreValue& expr) {
    const AbstractQoreNode* node = expr.getInternalNode();
    if (!node) {
        return;
    }
    if (const VarRefNode* source = dynamic_cast<const VarRefNode*>(node)) {
        qore_var_t source_type = source->getType();
        if ((source_type == VT_LOCAL || source_type == VT_CLOSURE || source_type == VT_LOCAL_TS) && source->ref.id) {
            source->ref.id->parseWeakSource();
        }
        return;
    }
    if (const QoreQuestionMarkOperatorNode* q = dynamic_cast<const QoreQuestionMarkOperatorNode*>(node)) {
        qore_mark_weak_assignment_sources(q->getExpression(1));
        qore_mark_weak_assignment_sources(q->getExpression(2));
        return;
    }
    if (const QoreNullCoalescingOperatorNode* n = dynamic_cast<const QoreNullCoalescingOperatorNode*>(node)) {
        qore_mark_weak_assignment_sources(n->getLeft());
        qore_mark_weak_assignment_sources(n->getRight());
        return;
    }
    if (const QoreValueCoalescingOperatorNode* v = dynamic_cast<const QoreValueCoalescingOperatorNode*>(node)) {
        qore_mark_weak_assignment_sources(v->getLeft());
        qore_mark_weak_assignment_sources(v->getRight());
    }
}

int QoreAssignmentOperatorNode::parseInitMode(QoreValue& val, QoreParseContext& parse_context,
        AssignmentMode mode) {
    int err = parseInitIntern(parse_context, mode);
    if (err || ident || !getSemanticPolicy().needsFilter()) {
        return err;
    }
    // makeSpecialization() transfers the operands and deletes this parse-only node.
    const bool saved_broken_int = broken_int;
    const bool saved_ident = ident;
    const QoreTypeInfo* saved_type = ti;
    QoreAssignmentOperatorNode* replacement;
    switch (mode) {
        case AssignmentMode::Weak:
            replacement = static_cast<QoreAssignmentOperatorNode*>(
                makeSpecialization<LexicalWeakAssignmentOperatorNode>());
            break;
        case AssignmentMode::Opaque:
            replacement = static_cast<QoreAssignmentOperatorNode*>(
                makeSpecialization<LexicalOpaqueAssignmentOperatorNode>());
            break;
        default:
            replacement = static_cast<QoreAssignmentOperatorNode*>(
                makeSpecialization<LexicalAssignmentOperatorNode>());
            break;
    }
    replacement->broken_int = saved_broken_int;
    replacement->ident = saved_ident;
    replacement->ti = saved_type;
    replacement->setParseAnalysis(parse_context.analysis);
    val = replacement;
    return 0;
}

int QoreAssignmentOperatorNode::parseInitIntern(QoreParseContext& parse_context, AssignmentMode mode) {
    // the operator's spelling, used in diagnostics
    const char* op_name = mode == AssignmentMode::Weak
        ? ":="
        : (mode == AssignmentMode::Opaque ? "@=" : "=");
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
    // a local variable assigned with := or @= can hold a weak or an opaque reference
    if (mode != AssignmentMode::Normal && left.getType() == NT_VARREF) {
        const VarRefNode* var = left.get<const VarRefNode>();
        qore_var_t var_type = var->getType();
        if ((var_type == VT_LOCAL || var_type == VT_CLOSURE || var_type == VT_LOCAL_TS) && var->ref.id) {
            var->ref.id->parseWeakAssigned();
        }
    }
    // return type info is the same as the lvalue's typeinfo
    ti = parse_context.typeInfo;

    //printd(0, "QoreAssignmentOperatorNode::parseInitImpl() this: %p left: '%s' nt: %d ti: %p '%s'\n", this,
    //    left.getFullTypeName(), left.getType(), ti, QoreTypeInfo::getName(ti));
    int lvalue_err = 0;
    if (!err && (lvalue_err = checkLValue(left, parse_context.pflag))) {
        err = -1;
    }

    // if "broken-int-assignments" is set, then set flag if applicable
    if ((ti == bigIntTypeInfo || ti == softBigIntTypeInfo)
        && (parse_context.pgm->getParseOptions() & PO_BROKEN_INT_ASSIGNMENTS)) {
        broken_int = true;
    }

    parse_context.typeInfo = nullptr;
    // Forward the lvalue's declared type as an inference hint to the
    // rvalue.  Consumers (map operator, hash literal) use it to narrow
    // when their own inference would otherwise land on `auto` — see
    // QoreParseContext::expected_type_info documentation.  The hint
    // never overrides concrete types; the authoritative compatibility
    // check is parseAccepts() below.
    //
    // Restore `expected_type_info` to its prior value so the hint
    // doesn't leak to sibling expressions when we unwind from nested
    // assignments (e.g., `a = b = map ...`).
    const QoreTypeInfo* prev_expected = parse_context.expected_type_info;
    parse_context.expected_type_info = ti;
    QoreParseAnalysis right_analysis;
    // the variable declared by the lvalue, whose initialization expression the right-hand side is (qore#526)
    const VarRefNode* decl = left.getType() == NT_VARREF && left.get<const VarRefNode>()->isGlobalDecl()
        ? left.get<const VarRefNode>()
        : nullptr;
    // a global or thread-local variable is not checked: it keeps its value when its declaration runs again (in a
    // function, a loop, or the top-level code of a program that is run again), so the declaration can use the value
    const void* decl_target = nullptr;
    const char* decl_what = nullptr;
    if (decl) {
        qore_var_t decl_type = decl->getType();
        if (decl_type == VT_LOCAL || decl_type == VT_CLOSURE || decl_type == VT_LOCAL_TS) {
            decl_target = decl->ref.id;
            decl_what = "local variable";
        }
    }
    // the location of the first use of the declared variable in the right-hand side; the error is raised after the
    // type checks of the assignment, which are reported first
    const QoreProgramLocation* decl_use_loc = nullptr;
    {
        QoreParseInitializerTarget init_target(decl_target);
        {
            // the right-hand side is the value stored in the lvalue (issue #5782)
            QoreParseTargetHelper target(parse_context, right, ti);
            if (parse_init_value(right, parse_context) && !err) {
                err = -1;
            }
        }
        // the identity assignment of a declared variable is reported below as for any other variable
        if (decl_target && !(right.getType() == NT_VARREF && decl->parseEqualTo(*right.get<VarRefNode>()))) {
            decl_use_loc = init_target.getUseLoc();
        }
    }
    // raises the error for a use of the declared variable in the right-hand side; returns the error status
    auto raise_decl_use = [&]() {
        if (QoreParseInitializerTarget::raiseUseError(decl_use_loc, decl_what, decl ? decl->getName() : nullptr)
                && !err) {
            err = -1;
        }
        return err;
    };
    parse_context.expected_type_info = prev_expected;
    right_analysis = parse_context.analysis;
    // a local variable that a weak reference is taken of must be changed in place (qore#5846)
    if (mode == AssignmentMode::Weak) {
        qore_mark_weak_assignment_sources(right);
    }

    // check for illegal assignment to $self
    if (parse_context.oflag && check_self_assignment(loc, left, parse_context.oflag) && !err) {
        err = -1;
    }

    // a local variable that cannot have been assigned gives NOTHING (qore#5937); an assignment through a reference
    // stores the value in the target of the reference
    if (!lvalue_err && mode == AssignmentMode::Normal) {
        const QoreTypeInfo* target_ti = QoreTypeInfo::isReference(ti) ? QoreTypeInfo::getReferenceTarget(ti) : ti;
        if (qore_parse_check_unassigned_read(parse_context.pgm, loc, right, target_ti, "the assigned lvalue")
                && !err) {
            err = -1;
        }
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

    if (lvalue_err == -2) {
        parse_context.typeInfo = ti;
        parse_context.analysis.clear();
        return raise_decl_use();
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

        // Dereference LHS if it's a reference type
        // Assignment through a reference targets the referenced location, not the reference itself
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

        // Special handling for hash type mismatches that parseAccepts treats as ambiguous
        // When assigning untyped/auto hash to a specific typed hash, enforce strict checking
        if (res == QTI_AMBIGUOUS && check_rhs_ti && check_ti != check_rhs_ti) {
            // Check LHS type name for element types (looks like "hash<key, value>")
            const char* lhs_name = QoreTypeInfo::getName(check_ti);
            // Check RHS type name
            const char* rhs_name = QoreTypeInfo::getName(check_rhs_ti);

            // LHS is a specific typed hash if it contains "<" but not "<auto>"
            bool lhs_is_typed_hash = false;
            if (lhs_name) {
                const char* bracket = strchr(lhs_name, '<');
                if (bracket && bracket[1] != 'a') {  // Not "hash<auto>" or "*hash<auto>"
                    lhs_is_typed_hash = true;
                }
            }
            // hash<auto!> is named "hash<string, auto>", which the name check above takes for a typed hash, but
            // its values are untyped like those of hash<auto>: an untyped hash (ex: the empty hash literal {})
            // assigned to it was refused, while the same value assigned through hash() was accepted
            if (lhs_is_typed_hash) {
                const QoreTypeInfo* lhs_vti = QoreTypeInfo::getComplexHashValueType(check_ti);
                if (lhs_vti && !QoreTypeInfo::hasType(lhs_vti)) {
                    lhs_is_typed_hash = false;
                }
            }

            // RHS is an untyped hash if it's exactly "hash" (not "hash<auto>" or any specific type)
            bool rhs_is_untyped_hash = false;
            if (rhs_name) {
                if (!strcmp(rhs_name, "hash")) {
                    rhs_is_untyped_hash = true;
                }
            }

            // Check if the RHS is a reference-creation expression (\expr)
            bool rhs_is_ref_expr = (right.getType() == NT_PARSEREFERENCE);

            // Only apply special check if:
            // - LHS is a specific typed hash (has element types like hash<string, int>)
            // - RHS is an untyped/auto hash (plain "hash" or "hash<auto>")
            // - RHS is not a reference expression
            if (lhs_is_typed_hash && rhs_is_untyped_hash && !rhs_is_ref_expr) {
                // LHS is typed hash, RHS is untyped/auto hash
                res = QTI_NOT_EQUAL;
            }
        }

        // Additional check for typed callable signature compatibility
        if (res && !QoreTypeInfo::checkComplexCodeCompatibility(check_ti, check_rhs_ti)) {
            res = QTI_NOT_EQUAL;
        }
    } else {
        res = QTI_AMBIGUOUS;
    }

    // Check for type mismatch - either with parse exception sink enabled, or for narrowed types
    bool type_mismatch = !res;
    bool optional_map_result = is_optional_map_result_mismatch(right, ti, parse_context.typeInfo);

    // Check the lvalue itself rather than the shared parse flag: parsing a narrowed variable on
    // the right-hand side can also set PF_NARROWED_TYPE and must not make an explicitly typed
    // target look inferred.
    bool has_narrowed_type = qore_is_narrowed_lvalue(left);

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
                if (!declared_res && qore_program_ops(parse_context.pgm).getParseExceptionSink()) {
                    raise_exception = true;
                    error_ti = declared_ti;
                    error_rhs_ti = parse_context.typeInfo;
                }
            }
        } else {
            // Not a direct auto assignment - use normal logic
            if (has_narrowed_type && !(parse_context.pgm->getParseOptions() & PO_BROKEN_NARROWED_TYPES)) {
                // For narrowed types assigned to soft types, defer to runtime
                // because soft types perform runtime conversion and the auto variable's
                // actual runtime type may be different from the narrowed parse type
                const char* lhs_name = QoreTypeInfo::getName(ti);
                bool lhs_is_soft = lhs_name && !strncmp(lhs_name, "soft", 4);
                if (!lhs_is_soft) {
                    raise_exception = true;
                }
            } else {
                raise_exception = (bool)qore_program_ops(parse_context.pgm).getParseExceptionSink();
            }
        }
    }

    // Preserve the language's existing runtime handling of possibly-NOTHING assignments.  The parser can identify
    // this map-specific risk precisely, but it cannot prove that an enclosing runtime guard has not already excluded
    // NOTHING.  Report it through the opt-in trap warning instead of rejecting valid guarded code.
    if (optional_map_result && !raise_exception) {
        const auto* map = dynamic_cast<const QoreMapOperatorNode*>(right.getInternalNode());
        assert(map);
        const QoreTypeInfo* source_ti = map->getIteratorTypeInfo();
        QoreStringNode* desc = new QoreStringNodeMaker(
            "a direct map result with type '%s' is assigned to required list type '%s'; the map source's "
            "static type '%s' permits NOTHING, so the result can also be NOTHING; confirm that the source is "
            "non-NOTHING on every path, coalesce the result with '?? ()', or make the target type optional",
            get_type_name(parse_context.typeInfo), get_type_name(ti), get_type_name(source_ti));
        QoreDiagnosticMetadata metadata("OPTIONAL-MAP-RESULT",
            "the map source's static type permits NOTHING; if control flow does not guarantee a value, use "
            "'(map expression) ?? ()' for a required list or declare the target as an optional list");
        metadata.addFact("expectedType", get_type_name(ti));
        metadata.addFact("actualType", get_type_name(parse_context.typeInfo));
        metadata.addFact("operation", op_name);
        metadata.addFact("sourceType", get_type_name(source_ti));
        metadata.addFact("sourceTypeAllowsNothing", "true");
        metadata.addSuggestion("(map expression) ?? ()");
        metadata.addSuggestion("make the target list type optional");
        metadata.addFix("coalesce the map result with '?? ()'", "review-required");
        qore_program_private::makeParseWarning(parse_context.pgm, *loc, QP_WARN_LANGUAGE_TRAPS,
            "LANGUAGE-TRAP", desc, metadata);
    }

    if (raise_exception) {
        QoreStringNode* edesc = new QoreStringNodeMaker("lvalue for %sassignment operator '%s' expects ",
            mode == AssignmentMode::Weak ? "weak " : (mode == AssignmentMode::Opaque ? "opaque " : ""),
            op_name);
        QoreTypeInfo::getThisType(error_ti, *edesc);
        edesc->concat(", but right-hand side is ");
        QoreTypeInfo::getThisType(error_rhs_ti, *edesc);

        // Add context about type narrowing if applicable
        if (has_narrowed_type && !is_direct_auto_assignment) {
            edesc->concat("; the container's element type was inferred from an earlier assignment; "
                "to use mixed types, include values of all needed types in that assignment, "
                "or use hash<auto!> or list<auto!> to disable type narrowing for the variable; "
                "note: %broken-narrowed-types will suppress this error but move it to runtime");
        }

        QoreDiagnosticMetadata metadata = qore_make_lvalue_type_diagnostic(left, error_ti, error_rhs_ti,
            has_narrowed_type && !is_direct_auto_assignment, "INCOMPATIBLE-ASSIGNMENT-TYPE",
            op_name);
        qore_program_private::makeParseException(parse_context.pgm, *loc, "PARSE-TYPE-ERROR", edesc, metadata);
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
            if (lvar && lvar->isAutoType()) {
                // Direct assignment replaces the narrowed type, store location for error messages
                lvar->parseSetNarrowedType(narrow_type, loc);
            }
        } else if (vtype == VT_GLOBAL || vtype == VT_THREAD_LOCAL) {
            Var* gvar = vrn->ref.var;
            if (gvar && gvar->isAutoType()) {
                // Direct assignment replaces the narrowed type, store location for error messages
                gvar->parseSetNarrowedType(narrow_type, loc);
            }
        }
    }

    // For auto-typed variables, use the declared type (auto) as the expression result
    // type, not the narrowed type. This prevents map/select/etc. from creating typed
    // lists (e.g., list<hash<auto>>) when the actual values may be of different types.
    if (is_direct_auto_assignment) {
        parse_context.typeInfo = autoTypeInfo;
    } else {
        parse_context.typeInfo = ti;
    }
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
    raise_decl_use();
    if (!err && left.getType() == NT_VARREF) {
        VarRefNode* vrn = left.get<VarRefNode>();
        qore_var_t vtype = vrn->getType();
        if (vrn->isGlobalDecl() && (vtype == VT_GLOBAL || vtype == VT_THREAD_LOCAL) && vrn->ref.var) {
            vrn->ref.var->preserveAOTInitExpr(right);
        }
    }
    return err;
}

//! returns true if \a node is the root of an lvalue path whose container a weak assignment makes unique: a variable,
//! a member of the object (self) or a class static variable
static bool qore_weak_assignment_path_root(const AbstractQoreNode* node) {
    if (auto* var = dynamic_cast<const VarRefNode*>(node)) {
        return var->getType() != VT_IMMEDIATE;
    }
    return dynamic_cast<const SelfVarrefNode*>(node) != nullptr
        || dynamic_cast<const StaticClassVarRefNode*>(node) != nullptr
        || dynamic_cast<const DeferredStaticClassMemberRefNode*>(node) != nullptr;
}

//! returns the parse type of an operand of an lvalue path (nullptr if it has none: no type)
static const QoreTypeInfo* qore_weak_path_operand_type(const QoreValue& v) {
    if (const AbstractQoreNode* n = v.getInternalNode()) {
        if (auto* var = dynamic_cast<const VarRefNode*>(n)) {
            return var->getTypeInfo();
        }
        if (auto* pn = dynamic_cast<const ParseNode*>(n)) {
            return pn->getParseTypeInfo();
        }
    }
    return v.getFullTypeInfo();
}

//! returns true if the key or index of a step of an lvalue path cannot be a list, which would make the step a slice,
//! which is not an lvalue that a weak assignment's source can be made unique through (qore#7242)
static bool qore_weak_path_selector_not_list(const QoreValue& sel) {
    if (dynamic_cast<const QoreParseListNode*>(sel.getInternalNode())) {
        return false;
    }
    const QoreTypeInfo* ti = qore_weak_path_operand_type(sel);
    return QoreTypeInfo::hasType(ti) && QoreTypeInfo::parseReturns(ti, NT_LIST) == QTI_NOT_EQUAL;
}

//! returns true if a member or key step of an lvalue path can be made unique as the AST evaluates it (qore#7242): a
//! hash key that is not a list, or a public member declared in the known class of an object whose class has no
//! memberGate() method, which the read of the member calls for a member that the lvalue cannot access
static bool qore_weak_path_member_step(const QoreHashObjectDereferenceOperatorNode* hd) {
    QoreValue key = hd->getRight();
    if (!qore_weak_path_selector_not_list(key)) {
        return false;
    }
    const QoreTypeInfo* lti = qore_weak_path_operand_type(hd->getLeft());
    if (QoreTypeInfo::hasType(lti) && QoreTypeInfo::parseReturns(lti, NT_OBJECT) == QTI_NOT_EQUAL) {
        // a hash
        return true;
    }
    const QoreClass* qc = QoreTypeInfo::getUniqueReturnClass(lti);
    if (!qc || key.getType() != NT_STRING || key.needsEval()) {
        return false;
    }
    char buf[8];
    const char* mem;
    if (key.isShortString()) {
        key.getShortString(buf);
        mem = buf;
    } else {
        mem = key.get<const QoreStringNode>()->c_str();
    }
    const qore_class_private* cls = qore_class_private::get(*qc);
    if (cls->parseHasMemberGate()) {
        return false;
    }
    // the class was initialized by the parse of the member access that gives the step its type; a class that is not
    // initialized has no members yet, and the step is then not made unique.  No initialization is started here: the
    // check also runs when the code is first evaluated or lowered to IR, in any thread
    const qore_class_private* member_class = nullptr;
    ClassAccess access = Public;
    const QoreMemberInfo* mi = cls->parseFindMemberNoInit(mem, member_class, access, true);
    return mi && access == Public;
}

//! returns true if an index step of an lvalue path can be made unique as the AST evaluates it (qore#7242): an index
//! that is not a list, of a value whose type cannot be a string, a binary or a buffer, which an lvalue cannot be
//! indexed in; the read of a character, a byte or a buffer element would otherwise be refused by the lvalue
static bool qore_weak_path_index_step(const QoreSquareBracketsOperatorNode* sb) {
    if (!qore_weak_path_selector_not_list(sb->getRight())) {
        return false;
    }
    const QoreTypeInfo* lti = qore_weak_path_operand_type(sb->getLeft());
    return QoreTypeInfo::hasType(lti) && QoreTypeInfo::parseReturns(lti, NT_STRING) == QTI_NOT_EQUAL
        && QoreTypeInfo::parseReturns(lti, NT_BINARY) == QTI_NOT_EQUAL
        && QoreTypeInfo::parseReturns(lti, NT_BUFFER) == QTI_NOT_EQUAL;
}

//! returns true if \a node is an lvalue path whose container a weak assignment makes unique (qore#7242): a member of
//! the object, or an object member, a hash key or a list element reached from a variable, the object or a class
//! static variable; out of the path of a variable source
static QORE_INTERN_NOINLINE bool qore_weak_assignment_path_source_made_unique(const AbstractQoreNode* node) {
    // a weak reference designates the container of the path as it does that of a variable
    if (auto* self_ref = dynamic_cast<const SelfVarrefNode*>(node)) {
        return qore_weak_source_type_may_hold_container(self_ref->getParseTypeInfo());
    }
    if (!dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(node)
            && !dynamic_cast<const QoreSquareBracketsOperatorNode*>(node)) {
        return false;
    }
    if (!qore_weak_source_type_may_hold_container(static_cast<const ParseNode*>(node)->getParseTypeInfo())) {
        return false;
    }
    // the root of the path; a step that is a slice, or a member that the lvalue could not access, leaves the path as
    // it was, which is read as an rvalue
    const AbstractQoreNode* root = node;
    while (true) {
        if (auto* hd = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(root)) {
            if (!qore_weak_path_member_step(hd)) {
                return false;
            }
            root = hd->getLeft().getInternalNode();
        } else if (auto* sb = dynamic_cast<const QoreSquareBracketsOperatorNode*>(root)) {
            if (!qore_weak_path_index_step(sb)) {
                return false;
            }
            root = sb->getLeft().getInternalNode();
        } else {
            break;
        }
        if (!root) {
            return false;
        }
    }
    return qore_weak_assignment_path_root(root);
}

bool qore_weak_assignment_source_made_unique(const QoreValue& right) {
    const AbstractQoreNode* node = right.getInternalNode();
    if (!node) {
        return false;
    }
    if (auto* var = dynamic_cast<const VarRefNode*>(node)) {
        return var->getType() != VT_IMMEDIATE && qore_weak_source_type_may_hold_container(var->getTypeInfo());
    }
    // a class static variable, or a static or global variable that AOT source parsing has not resolved: the value is
    // checked when the step runs
    if (dynamic_cast<const StaticClassVarRefNode*>(node) != nullptr
            || dynamic_cast<const DeferredStaticClassMemberRefNode*>(node) != nullptr) {
        return true;
    }
    return qore_weak_assignment_path_source_made_unique(node);
}

bool QoreAssignmentOperatorNode::weakSourceMadeUnique() const {
    signed char v = weak_source_unique.load(std::memory_order_relaxed);
    if (!v) {
        v = qore_weak_assignment_source_made_unique(right) ? 2 : 1;
        weak_source_unique.store(v, std::memory_order_relaxed);
    }
    return v == 2;
}

QoreValue QoreAssignmentOperatorNode::evalIntern(ExceptionSink* xsink, bool& needs_deref,
        AssignmentMode mode) const {
    return evalInternImpl<false, false>(xsink, needs_deref, mode);
}

QoreValue QoreAssignmentOperatorNode::evalLexical(ExceptionSink* xsink, bool& needs_deref,
        AssignmentMode mode) const {
    return evalInternImpl<true, false>(xsink, needs_deref, mode);
}

QoreValue QoreAssignmentOperatorNode::evalInternWeak(ExceptionSink* xsink, bool& needs_deref) const {
    return evalInternImpl<false, true>(xsink, needs_deref, AssignmentMode::Weak);
}

QoreValue QoreAssignmentOperatorNode::evalLexicalWeak(ExceptionSink* xsink, bool& needs_deref) const {
    return evalInternImpl<true, true>(xsink, needs_deref, AssignmentMode::Weak);
}

//! Makes a weak assignment of the value of \a new_value to \a left and returns the value of the assignment
/** The value assigned, if it is a temporary, and a container that the type of the lvalue converts to a copy of its
    own (ex: a hash<string, int> assigned to a hash variable), a temporary of the statement that only the weak reference
    designates, live until the end of the statement (qore#7085); the copy is held after the lvalue is unlocked, as
    releasing a value that the statement held before can run a destructor.  Out of line, so that the other assignments
    keep their code.

    @param policy the lexical semantic policy of the assignment, or nullptr for an assignment without one
    @param site the expression, which the copy held is keyed by
*/
static QORE_INTERN_NOINLINE QoreValue qore_weak_assign_hold_copy(const QoreValue& left,
        QoreMutationOperandHolder& new_value, bool ident, bool ref_rv, const LexicalSemanticPolicy* policy,
        const void* site, ExceptionSink* xsink) {
    // a weak reference does not keep the value assigned: a temporary lives until the end of the statement, as in the
    // other execution modes, so that the weak reference and the value of the assignment designate it until then
    // (qore#7085)
    QoreStatementTempScope::hold(site, new_value, xsink);
    if (*xsink) {
        return QoreValue();
    }
    // the copy made by the conversion, owned by this function until it is held
    QoreValue copy;
    QoreValue rv;
    {
        // get ptr to current value (lvalue is locked for the scope of the LValueHelper object)
        LValueHelper v(left, xsink, false, true);
        if (!v) {
            return QoreValue();
        }
        assert(!*xsink);

        // the container assigned, to recognize a copy made by the conversion
        const AbstractQoreNode* assigned_node = new_value->hasNode() ? new_value->getInternalNode() : nullptr;

        if (policy) {
            if (v.assign(new_value.takeReferencedValue(), "<lvalue>", *policy, !ident, AssignmentMode::Weak)) {
                return QoreValue();
            }
        } else if (v.assign(new_value.takeReferencedValue(), "<lvalue>", !ident, AssignmentMode::Weak)) {
            return QoreValue();
        }

        // the target is compared by its address; a copy is kept with a reference of its own, taken through the weak
        // reference, or is a new empty container if it was freed (qore#7198)
        switch (v.getType()) {
            case NT_WEAKREF_HASH: {
                const WeakHashReferenceNode* w = reinterpret_cast<const WeakHashReferenceNode*>(v.getInternalNode());
                if (w->borrow() != assigned_node) {
                    copy = w->lock();
                }
                break;
            }
            case NT_WEAKREF_LIST: {
                const WeakListReferenceNode* w = reinterpret_cast<const WeakListReferenceNode*>(v.getInternalNode());
                if (w->borrow() != assigned_node) {
                    copy = w->lock();
                }
                break;
            }
            default:
                break;
        }

        // reference return value if necessary
        if (ref_rv) {
            rv = v.getReferencedValue();
        }
    }
    // the site differs from that of the operand held by the caller
    if (copy.hasNode() && !QoreStatementTempScope::hold(reinterpret_cast<const char*>(site) + 1, copy, xsink)) {
        // there is no scope: the copy is released here, as before
        copy.discard(xsink);
    }
    return rv;
}

template <bool Lexical, bool Weak>
QORE_INTERN_ALWAYS_INLINE QoreValue QoreAssignmentOperatorNode::evalInternImpl(ExceptionSink* xsink,
        bool& needs_deref, AssignmentMode mode) const {
    /* assign new value, this value gets referenced with the
        eval(xsink) call, so there's no need to reference it again
        for the variable assignment - however it does need to be
        copied/referenced for the return value
    */
    // a weak reference to a variable designates the variable's own container: a container that the variable shares
    // with another owner (a literal of the program, another variable) is made unique first, so that the owner's later
    // writes are seen through the weak reference (qore#5756)
    // The container made unique is the value assigned: the variable is not read again, as another thread can assign
    // a variable that is shared (a global, a variable that a closure captures) between the two reads, and the
    // weak reference would then designate a container that the step did not make unique (qore#5847).
    // the value of the source, read by the step, which is the value assigned whatever its type: the source is not
    // evaluated again, which would run the side effects of the keys or indexes of a path a second time (qore#7242); it
    // is held until the operand holds a reference of its own, as the operand can borrow it and another thread can
    // assign the source meanwhile
    ValueHolder unique_source(xsink);
    bool source_read = false;
    if (mode == AssignmentMode::Weak) {
        if (weakSourceMadeUnique()) {
            LValueHelper source(right, xsink, true);
            if (*xsink) {
                return QoreValue();
            }
            // a path that leads to no value (a missing key, an index beyond the end of a list, a step through NOTHING)
            // gives NOTHING, as an rvalue read of it does, without being evaluated again
            if (source) {
                // a weak reference held by the source and a value of another type are left as they are; the operand
                // evaluates a weak reference to its target
                if (source.getType() == NT_HASH || source.getType() == NT_LIST) {
                    source.ensureUnique();
                }
                unique_source = source.getReferencedValue();
            }
            source_read = true;
        }
    }

    // the operand is a statement of its own where the other execution modes evaluate it so
    QoreMutationOperandHolder new_value(left, source_read ? *unique_source : right, true, xsink);
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

    // a weak assignment holds the value assigned and the copy that the variable's type makes of a container until the
    // end of the statement (out of line: see qore_weak_assign_hold_copy())
    if constexpr (Weak) {
        assert(mode == AssignmentMode::Weak);
        if constexpr (Lexical) {
            LexicalSemanticPolicy policy = getSemanticPolicy();
            return qore_weak_assign_hold_copy(left, new_value, ident, ref_rv, &policy, this, xsink);
        } else {
            return qore_weak_assign_hold_copy(left, new_value, ident, ref_rv, nullptr, this, xsink);
        }
    }

    // get ptr to current value (lvalue is locked for the scope of the LValueHelper object)
    LValueHelper v(left, xsink, false, true);
    if (!v)
        return QoreValue();

    assert(!*xsink);

    // assign new value
    if constexpr (Lexical) {
        if (v.assign(new_value.takeReferencedValue(), "<lvalue>", getSemanticPolicy(), !ident, mode)) {
            return QoreValue();
        }
    } else if (v.assign(new_value.takeReferencedValue(), "<lvalue>", !ident, mode)) {
        return QoreValue();
    }

    // reference return value if necessary
    return ref_rv ? v.getReferencedValue() : QoreValue();
}
