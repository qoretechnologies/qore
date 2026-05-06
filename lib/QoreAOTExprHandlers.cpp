/* -*- mode: c++ -*- */
/*
  QoreAOTExprHandlers.cpp

  Expression kind handlers for AOT binary serialization registry

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

  Note that the Qore library is dual-licensed under a choice of two
  licenses.
*/

#include <cstdint>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "qore/intern/QoreAOT.h"
#include "qore/intern/QoreAOTBinary.h"
#include "qore/intern/QoreAOTExprRegistry.h"
#include "qore/intern/QoreIR.h"
#include "qore/intern/QoreParseListNode.h"
#include "qore/intern/Function.h"
#include "qore/intern/QoreClosureParseNode.h"
#include "qore/intern/qore_thread_intern.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/QoreDotEvalOperatorNode.h"
#include "qore/intern/QorePlusOperatorNode.h"
#include "qore/intern/QoreSquareBracketsOperatorNode.h"
#include "qore/intern/QoreSquareBracketsRangeOperatorNode.h"
#include "qore/intern/QoreExistsOperatorNode.h"
#include "qore/intern/QoreImplicitArgumentNode.h"
#include "qore/intern/QoreMinusOperatorNode.h"
#include "qore/intern/QoreKeysOperatorNode.h"
#include "qore/intern/QoreMultiplicationOperatorNode.h"
#include "qore/intern/QoreDivisionOperatorNode.h"
#include "qore/intern/QoreModuloOperatorNode.h"
#include "qore/intern/QoreBinaryAndOperatorNode.h"
#include "qore/intern/QoreBinaryOrOperatorNode.h"
#include "qore/intern/QoreBinaryXorOperatorNode.h"
#include "qore/intern/QoreShiftLeftOperatorNode.h"
#include "qore/intern/QoreShiftRightOperatorNode.h"
#include "qore/intern/QoreImplicitElementNode.h"
#include "qore/intern/QoreInstanceOfOperatorNode.h"
#include "qore/intern/QoreRegexMatchOperatorNode.h"
#include "qore/intern/QoreRegexNMatchOperatorNode.h"
#include "qore/intern/QoreRegexExtractOperatorNode.h"
#include "qore/intern/QoreRegex.h"
#include "qore/intern/QorePreIncrementOperatorNode.h"
#include "qore/intern/QorePreDecrementOperatorNode.h"
#include "qore/intern/QorePostIncrementOperatorNode.h"
#include "qore/intern/QorePostDecrementOperatorNode.h"
#include "qore/intern/QoreIntPostIncrementOperatorNode.h"
#include "qore/intern/QoreIntPostDecrementOperatorNode.h"
#include "qore/intern/QoreLogicalAndOperatorNode.h"
#include "qore/intern/QoreLogicalOrOperatorNode.h"
#include "qore/intern/QoreLogicalEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalNotEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalLessThanOperatorNode.h"
#include "qore/intern/QoreLogicalGreaterThanOperatorNode.h"
#include "qore/intern/QoreLogicalLessThanOrEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalGreaterThanOrEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalNotOperatorNode.h"
#include "qore/intern/QoreNullCoalescingOperatorNode.h"
#include "qore/intern/QoreValueCoalescingOperatorNode.h"
#include "qore/intern/QoreQuestionMarkOperatorNode.h"
#include "qore/intern/QoreFoldlOperatorNode.h"
#include "qore/intern/QoreMapOperatorNode.h"
#include "qore/intern/QoreMapSelectOperatorNode.h"
#include "qore/intern/QoreHashMapOperatorNode.h"
#include "qore/intern/QoreHashMapSelectOperatorNode.h"
#include "qore/intern/QoreSelectOperatorNode.h"
#include "qore/intern/QoreElementsOperatorNode.h"
#include "qore/intern/QoreDeleteOperatorNode.h"
#include "qore/intern/QoreRemoveOperatorNode.h"
#include "qore/intern/QoreBackgroundOperatorNode.h"
#include "qore/intern/QoreTrimOperatorNode.h"
#include "qore/intern/QoreChompOperatorNode.h"
#include "qore/intern/QorePopOperatorNode.h"
#include "qore/intern/QoreShiftOperatorNode.h"
#include "qore/intern/QorePushOperatorNode.h"
#include "qore/intern/QoreUnshiftOperatorNode.h"
#include "qore/intern/QoreRangeOperatorNode.h"
#include "qore/intern/QoreAssignmentOperatorNode.h"
#include "qore/intern/ContextrefNode.h"
#include "qore/intern/ContextRowNode.h"
#include "qore/intern/ComplexContextrefNode.h"
#include "qore/intern/CallReferenceCallNode.h"
#include "qore/intern/qore_list_private.h"
#include <qore/QoreBigFloatNode.h>
#include <qore/QoreBigIntNode.h>
#include <qore/QoreNothingNode.h>

static void makeExprDeserializedClosureIRNameUnique(QoreIRFunction& ir, const UserClosureVariant* variant) {
    static std::atomic<uint64_t> closure_ir_counter{0};
    ir.name += "@" + std::to_string(reinterpret_cast<uintptr_t>(variant))
        + "_" + std::to_string(closure_ir_counter.fetch_add(1));
}

// ============================================================================
// FUNC_CALL (1)
// ============================================================================

static bool write_expr_func_call(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* call = dynamic_cast<const FunctionCallNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::FUNC_CALL));
        // Emit the namespace-qualified name, not the bare base name.
        // Without qualification, a caller in namespace `OMQ` whose body
        // calls `Util::substitute_env_vars` serialized the same bare
        // string `"substitute_env_vars"` the runtime reader resolved
        // inside the current scope — which found `OMQ::substitute_env_vars`
        // (the calling wrapper itself) → infinite recursion, stack
        // overflow (observed live in `qrest -h` → `substitute_env_vars`
        // / `QorusClientBase.qmod`).  Qualified emission pins the
        // resolution to the same function the parser resolved at
        // compile time.
        const FunctionEntry* fe = call->getFunctionEntry();
        if (fe && fe->getNamespace()) {
            std::string qualified;
            fe->getNamespace()->getPath(qualified);  // unanchored, e.g. "Util"
            if (!qualified.empty()) {
                qualified += "::";
            }
            qualified += fe->getName();
            ctx.writer.writeStringRef(qualified.c_str());
        } else {
            // No resolved FE (rare — e.g. unresolved parse node) —
            // fall back to the bare name.  Reader tolerates both.
            ctx.writer.writeStringRef(call->getName());
        }
        if (const AbstractQoreFunctionVariant* v = call->getVariant()) {
            if (AbstractFunctionSignature* sig = const_cast<AbstractQoreFunctionVariant*>(v)->getSignature()) {
                std::string sig_ref = "sig:";
                sig_ref += sig->getSignatureText();
                ctx.writer.writeStringRef(sig_ref.c_str());
            } else {
                ctx.writer.writeStringRef("");
            }
        } else {
            ctx.writer.writeStringRef("");
        }
        const QoreListNode* args = call->getArgs();
        const QoreParseListNode* pargs = call->getParseArgs();
        size_t nargs = args ? args->size() : (pargs ? pargs->size() : 0);
        if (nargs > 255) {
            return false;
        }
        ctx.writer.writeU8(static_cast<uint8_t>(nargs));
        for (size_t j = 0; j < nargs; ++j) {
            const QoreValue arg = args ? args->retrieveEntry(j) : pargs->get(j);
            if (!::classifyAndWriteExpr(ctx.writer, arg,
                    ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)) {
                return false;
            }
        }
        return true;
    }
    return false;
}

static QoreValue read_expr_func_call(AOTExprReadCtx& ctx) {
    const char* func_name = ctx.reader.readStringRef(ctx.ptr);
    const char* sig_ref = nullptr;
    if ((ctx.reader.getHeader().feature_flags & QORE_AOT_FEAT_FUNC_CALL_VARIANT) != 0) {
        sig_ref = ctx.reader.readStringRef(ctx.ptr);
    }
    QoreParseListNode* pln = nullptr;
    if ((ctx.reader.getHeader().feature_flags & QORE_AOT_FEAT_INLINE_CALL_ARGS) != 0) {
        uint8_t num_args = QoreAOTBinaryReader::readU8(ctx.ptr);
        if (num_args > 0) {
            pln = new QoreParseListNode(&loc_builtin);
            for (uint8_t j = 0; j < num_args; ++j) {
                std::string arg_err;
                QoreValue arg = readOneExpr(ctx.reader, ctx.ptr, ctx.end, arg_err, ctx.pgm,
                    ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
                if (!arg_err.empty()) {
                    ctx.error = arg_err;
                    arg.discard(nullptr);
                    pln->deref();
                    return QoreValue();
                }
                pln->add(arg, &loc_builtin);
            }
        }
    }
    if (!func_name || !*func_name) {
        if (pln) {
            pln->deref();
        }
        ctx.error = "empty function name in inline FUNC_CALL expression";
        return QoreValue();
    }
    qore_program_private* pp = qore_program_private::get(*ctx.pgm);
    // `runtimeFindFunctionEntry` already routes `"Foo::bar"` through
    // NamedScope (qualified) and bare names through flat lookup —
    // perfect for the writer's new qualified-name emission and still
    // correct for legacy bare-name blobs (pre-fix AOT artifacts).
    const FunctionEntry* fe = qore_root_ns_private::runtimeFindFunctionEntry(
        *pp->RootNS, func_name);
    if (!fe) {
        if (pln) {
            pln->deref();
        }
        ctx.error = "cannot resolve function '";
        ctx.error += func_name;
        ctx.error += "' in inline FUNC_CALL expression";
        return QoreValue();
    }
    // Match normal parsed calls: keep the resolved FunctionEntry, but do not
    // bake the deserialization program into the call node. Builtins must use
    // the active runtime program when they execute.
    FunctionCallNode* fcn = new FunctionCallNode(
        &loc_builtin, fe, pln);
    if (sig_ref && strncmp(sig_ref, "sig:", 4) == 0) {
        if (const AbstractQoreFunctionVariant* v =
                fe->getFunction()->findVariantBySignatureText(sig_ref + 4)) {
            fcn->setVariant(v);
        }
    }
    if (pln) {
        fcn->resolveParseArgs();
    }
    return QoreValue(fcn);
}

// ============================================================================
// SELF_METHOD_CALL (2)
// ============================================================================

static const QoreMethod* resolve_aot_self_method(const QoreClass* qc, const char* method_name,
        qore_class_private*& qcp) {
    qcp = qore_class_private::get(*const_cast<QoreClass*>(qc));
    // parseFindSelfMethod() calls initialize().  During AOT method/signature
    // deserialization that can commit a class before all of its methods have
    // been attached, which breaks later abstract-method registration.  Leave
    // pending AOT classes name-based until normal class commit/finalization.
    if (!qcp->initialized) {
        return nullptr;
    }
    const QoreMethod* m = qcp->parseFindSelfMethod(method_name);
    if (!m) {
        m = qc->findMethod(method_name);
        if (!m) {
            m = qc->findStaticMethod(method_name);
        }
    }
    return m;
}

static QoreValue make_unresolved_aot_self_method_call(const char* method_ref, QoreParseListNode* pln,
        const QoreClass* qc) {
    SelfFunctionCallNode* sfcn = new SelfFunctionCallNode(&loc_builtin, strdup(method_ref), pln, qc);
    if (pln) {
        sfcn->resolveParseArgs();
    }
    return QoreValue(sfcn);
}

static bool write_expr_self_method_call(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* call = dynamic_cast<const SelfFunctionCallNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::SELF_METHOD_CALL));
        const QoreMethod* method = call->getMethod();
        const QoreClass* qc = call->getClass() ? call->getClass() : (method ? method->getClass() : nullptr);
        std::string class_ref = qore_aot_encode_class_ref(qc);
        ctx.writer.writeStringRef(class_ref.c_str());
        ctx.writer.writeStringRef(call->getName());
        const QoreListNode* args = call->getArgs();
        const QoreParseListNode* pargs = call->getParseArgs();
        size_t nargs = args ? args->size() : (pargs ? pargs->size() : 0);
        if (nargs > 255) {
            return false;
        }
        ctx.writer.writeU8(static_cast<uint8_t>(nargs));
        for (size_t j = 0; j < nargs; ++j) {
            const QoreValue arg = args ? args->retrieveEntry(j) : pargs->get(j);
            if (!::classifyAndWriteExpr(ctx.writer, arg,
                    ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)) {
                return false;
            }
        }
        return true;
    }
    return false;
}

static QoreValue read_expr_self_method_call(AOTExprReadCtx& ctx) {
    const char* class_path = ctx.reader.readStringRef(ctx.ptr);
    const char* method_ref = ctx.reader.readStringRef(ctx.ptr);
    QoreParseListNode* pln = nullptr;
    if ((ctx.reader.getHeader().feature_flags & QORE_AOT_FEAT_SELF_CALL_ARGS) != 0) {
        uint8_t num_args = QoreAOTBinaryReader::readU8(ctx.ptr);
        if (num_args > 0) {
            pln = new QoreParseListNode(&loc_builtin);
            for (uint8_t j = 0; j < num_args; ++j) {
                std::string arg_err;
                QoreValue arg = readOneExpr(ctx.reader, ctx.ptr, ctx.end, arg_err, ctx.pgm,
                    ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
                if (!arg_err.empty()) {
                    ctx.error = arg_err;
                    arg.discard(nullptr);
                    pln->deref();
                    return QoreValue();
                }
                pln->add(arg, &loc_builtin);
            }
        }
    }
    if (!method_ref || !*method_ref) {
        if (pln) {
            pln->deref();
        }
        return QoreValue();
    }
    const char* method_name = method_ref;
    const char* last_sep = strrchr(method_ref, ':');
    if (last_sep && last_sep > method_ref && *(last_sep - 1) == ':') {
        method_name = last_sep + 1;
    }
    const QoreClass* qc = nullptr;
    if (class_path && *class_path) {
        qc = qore_aot_resolve_class_ref(ctx.pgm, class_path, false);
    }
    if (!qc) {
        if (pln) {
            pln->deref();
        }
        return QoreValue();
    }
    if (!strcmp(method_ref, "copy")) {
        if (pln && pln->size()) {
            ctx.error = "implicit self copy() call cannot have arguments";
            pln->deref();
            return QoreValue();
        }
        return QoreValue(new SelfFunctionCallNode(&loc_builtin, strdup(method_ref), pln, qc, true));
    }
    qore_class_private* qcp = nullptr;
    const QoreMethod* m = resolve_aot_self_method(qc, method_name, qcp);
    if (!m) {
        return make_unresolved_aot_self_method_call(method_ref, pln, qc);
    }
    if (m->isStatic()) {
        StaticMethodCallNode* smcn = new StaticMethodCallNode(&loc_builtin, m, pln);
        if (pln) {
            smcn->resolveParseArgs();
        }
        return QoreValue(smcn);
    }
    SelfFunctionCallNode* sfcn = new SelfFunctionCallNode(&loc_builtin, strdup(method_ref), pln, m, qc, qcp);
    if (pln) {
        sfcn->resolveParseArgs();
    }
    return QoreValue(sfcn);
}

// ============================================================================
// STATIC_METHOD_CALL (3)
// ============================================================================

static bool write_expr_static_method_call(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* call = dynamic_cast<const StaticMethodCallNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::STATIC_METHOD_CALL));
        const QoreMethod* method = call->getMethod();
        if (method) {
            const QoreClass* qc = method->getClass();
            std::string class_ref = qore_aot_encode_class_ref(qc);
            ctx.writer.writeStringRef(class_ref.c_str());
        } else {
            ctx.writer.writeStringRef("");
        }
        ctx.writer.writeStringRef(call->getName());
        // Serialize method args
        const QoreListNode* args = call->getArgs();
        size_t nargs = args ? args->size() : 0;
        if (nargs > 255) {
            return false;
        }
        if (nargs > 0) {
            ctx.writer.writeU8(static_cast<uint8_t>(nargs));
            for (size_t j = 0; j < args->size(); ++j) {
                if (!::classifyAndWriteExpr(ctx.writer, args->retrieveEntry(j),
                        ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)) {
                    return false;
                }
            }
        } else {
            ctx.writer.writeU8(0);
        }
        return true;
    }
    return false;
}

static QoreValue read_expr_static_method_call(AOTExprReadCtx& ctx) {
    const char* class_path = ctx.reader.readStringRef(ctx.ptr);
    const char* method_name = ctx.reader.readStringRef(ctx.ptr);
    uint8_t num_args = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreListNode* args_list = nullptr;
    if (num_args > 0) {
        args_list = qore_list_private::newList(true);
        for (uint8_t j = 0; j < num_args; ++j) {
            std::string arg_err;
            QoreValue arg = readOneExpr(ctx.reader, ctx.ptr, ctx.end, arg_err, ctx.pgm,
                ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
            if (!arg_err.empty()) {
                ctx.error = arg_err;
                args_list->deref(nullptr);
                return QoreValue();
            }
            args_list->push(arg, nullptr);
        }
    }
    if (!class_path || !method_name) {
        if (args_list) {
            args_list->deref(nullptr);
        }
        return QoreValue();
    }
    const QoreClass* qc = qore_aot_resolve_class_ref(ctx.pgm, class_path, false);
    if (!qc) {
        if (args_list) {
            args_list->deref(nullptr);
        }
        return QoreValue();
    }
    const QoreMethod* m = qc->findStaticMethod(method_name);
    if (!m) {
        qore_class_private* qcp = qore_class_private::get(*const_cast<QoreClass*>(qc));
        m = qcp->parseFindLocalStaticMethod(method_name);
    }
    if (!m) {
        if (args_list) {
            args_list->deref(nullptr);
        }
        return QoreValue();
    }
    // Create with evaluated args list directly via the copy constructor pattern
    // StaticMethodCallNode needs QoreParseListNode for its primary constructor,
    // so we create a minimal node and set the args list for evaluation
    StaticMethodCallNode* smcn = new StaticMethodCallNode(&loc_builtin, m, (QoreParseListNode*)nullptr);
    if (args_list) {
        // Set the args as a member; since StaticMethodCallNode inherits FunctionCallBase,
        // we use resolveParseArgs after setting parse_args, or set args directly.
        // The args_list already has needs_eval_flag=true for proper evaluation.
        // We need to set the args field directly — create a temporary parse_args list
        // from the args, then resolve.
        // Actually, just delete the smcn and use NewObjectCallNode approach:
        // The simplest correct path is to create a wrapper that holds the args.
        delete smcn;

        // Create a new StaticMethodCallNode by first building a QoreParseListNode
        // from the evaluated args (they may contain AST nodes needing evaluation)
        QoreParseListNode* pln = new QoreParseListNode(&loc_builtin);
        ConstListIterator li(args_list);
        while (li.next()) {
            QoreValue v = li.getValue();
            v.refSelf();
            pln->add(v, &loc_builtin);
        }
        args_list->deref(nullptr);
        smcn = new StaticMethodCallNode(&loc_builtin, m, pln);
        smcn->resolveParseArgs();
    }
    return QoreValue(smcn);
}

// ============================================================================
// NEW_OBJECT (4)
// ============================================================================

static const AbstractQoreFunctionVariant* resolve_expr_constructor_variant(const QoreClass* qc,
        const QoreListNode* args, const char* class_path, std::string& error) {
    if (args && args->needs_eval()) {
        return nullptr;
    }

    const QoreMethod* constructor = qc ? qc->getConstructor() : nullptr;
    if (!constructor) {
        if (args && !args->empty()) {
            error = "class '";
            error += class_path ? class_path : (qc ? qc->getName() : "<unknown>");
            error += "' has no constructor accepting ";
            error += std::to_string(args->size());
            error += " argument(s)";
        }
        return nullptr;
    }

    ExceptionSink xsink;
    const AbstractQoreFunctionVariant* variant = qore_method_private::get(*constructor)->getFunction()
        ->runtimeFindVariant(&xsink, args, false, nullptr);
    if (xsink) {
        error = "exception resolving constructor variant for class '";
        error += class_path ? class_path : qc->getName();
        error += "'";
        QoreValue ex_err = xsink.getExceptionErr();
        QoreValue ex_desc = xsink.getExceptionDesc();
        if (ex_err.getType() == NT_STRING) {
            QoreStringValueHelper ex_err_str(ex_err);
            error += ": ";
            error += ex_err_str->c_str();
        }
        if (ex_desc.getType() == NT_STRING) {
            QoreStringValueHelper ex_desc_str(ex_desc);
            error += ": ";
            error += ex_desc_str->c_str();
        }
        xsink.clear();
        return nullptr;
    }
    if (!variant) {
        error = "cannot resolve constructor variant for class '";
        error += class_path ? class_path : qc->getName();
        error += "' with ";
        error += std::to_string(args ? args->size() : 0);
        error += " argument(s)";
    }
    return variant;
}

static bool write_expr_new_object(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* vrn = dynamic_cast<const VarRefNewObjectNode*>(node)) {
        const QoreClass* qc = QoreTypeInfo::getUniqueReturnClass(vrn->getTypeInfo());
        if (qc) {
            ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::NEW_OBJECT));
            std::string class_ref = qore_aot_encode_class_ref(qc);
            ctx.writer.writeStringRef(class_ref.c_str());
            const QoreListNode* args = vrn->getArgs();
            size_t nargs = args ? args->size() : 0;
            if (nargs > 255) {
                return false;
            }
            if (nargs > 0) {
                ctx.writer.writeU8(static_cast<uint8_t>(nargs));
                for (size_t j = 0; j < args->size(); ++j) {
                    if (!classifyAndWriteExpr(ctx.writer, args->retrieveEntry(j),
                            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)) {
                        return false;
                    }
                }
            } else {
                ctx.writer.writeU8(0);
            }
            return true;
        }
    }
    if (auto* no = dynamic_cast<const NewObjectCallNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::NEW_OBJECT));
        const QoreClass* qc = no->getClass();
        std::string class_ref = qore_aot_encode_class_ref(qc);
        ctx.writer.writeStringRef(class_ref.c_str());
        ctx.writer.writeU8(0);
        return true;
    }
    return false;
}

static QoreValue read_expr_new_object(AOTExprReadCtx& ctx) {
    const char* class_path = ctx.reader.readStringRef(ctx.ptr);
    uint8_t num_args = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreListNode* args_list = nullptr;
    if (num_args > 0) {
        args_list = qore_list_private::newList(true);
        for (uint8_t j = 0; j < num_args; ++j) {
            std::string arg_err;
            QoreValue arg = readOneExpr(ctx.reader, ctx.ptr, ctx.end, arg_err, ctx.pgm,
                ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
            if (!arg_err.empty()) {
                ctx.error = arg_err;
                args_list->deref(nullptr);
                return QoreValue();
            }
            args_list->push(arg, nullptr);
        }
    }
    if (!class_path || !*class_path) {
        if (args_list) {
            args_list->deref(nullptr);
        }
        return QoreValue();
    }
    const QoreClass* qc = qore_aot_resolve_class_ref(ctx.pgm, class_path, false);
    if (!qc) {
        if (args_list) {
            args_list->deref(nullptr);
        }
        return QoreValue();
    }
    std::string variant_err;
    const AbstractQoreFunctionVariant* variant = resolve_expr_constructor_variant(qc, args_list, class_path,
        variant_err);
    if (!variant_err.empty()) {
        if (args_list) {
            args_list->deref(nullptr);
        }
        ctx.error = variant_err;
        return QoreValue();
    }
    NewObjectCallNode* nocn = new NewObjectCallNode(qc, args_list);
    if (variant) {
        nocn->setVariant(variant);
    }
    return QoreValue(nocn);
}

// ============================================================================
// RUNTIME_CONST_REF (5)
// ============================================================================

static bool write_expr_runtime_const_ref(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (ctx.const_reverse_map) {
        auto it = ctx.const_reverse_map->find(node);
        if (it != ctx.const_reverse_map->end()) {
            ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::RUNTIME_CONST_REF));
            ctx.writer.writeStringRef(it->second.c_str());
            return true;
        }
    }
    return false;
}

static QoreValue read_expr_runtime_const_ref(AOTExprReadCtx& ctx) {
    const char* const_name = ctx.reader.readStringRef(ctx.ptr);
    if (!const_name || !*const_name) {
        return QoreValue();
    }
    QoreValue rv = qore_aot_resolve_constant_path_value(ctx.pgm, const_name,
        true, ctx.reader.wrap_const_ref_in_rcr);
    if (!rv) {
        ctx.error = std::string("cannot resolve runtime constant reference '") + const_name
            + "' in the current program; if this reference came from qcc --stub, "
            "the runtime host must inject the external constant before loading the AOT binary";
        return QoreValue();
    }
    return rv;
}

// ============================================================================
// SELF_VARREF (6)
// ============================================================================

static bool write_expr_self_varref(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* svn = dynamic_cast<const SelfVarrefNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::SELF_VARREF));
        ctx.writer.writeStringRef(svn->str ? svn->str : "");
        return true;
    }
    return false;
}

static QoreValue read_expr_self_varref(AOTExprReadCtx& ctx) {
    const char* member_name = ctx.reader.readStringRef(ctx.ptr);
    SelfVarrefNode* svn = new SelfVarrefNode(&loc_builtin, strdup(member_name ? member_name : ""));
    return QoreValue(svn);
}

// ============================================================================
// LOCAL_VARREF (7)
// ============================================================================

static bool write_expr_local_varref(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* varref = dynamic_cast<const VarRefNode*>(node)) {
        if (varref->getType() == VT_LOCAL || varref->getType() == VT_LOCAL_TS ||
                varref->getType() == VT_CLOSURE) {
            // First pass: match by pointer identity (handles same-named variables
            // in different scopes correctly)
            const void* var_ptr = varref->ref.id;
            if (var_ptr) {
                for (size_t i = 0; i < ctx.parent_locals.size(); ++i) {
                    if (ctx.parent_locals[i].local_var_ptr == var_ptr) {
                        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::LOCAL_VARREF));
                        ctx.writer.writeStringRef(std::to_string(i).c_str());
                        return true;
                    }
                }
            }
            // Second pass: fall back to name match (for cases where pointer isn't available)
            for (size_t i = 0; i < ctx.parent_locals.size(); ++i) {
                if (varref->getName() && ctx.parent_locals[i].name == varref->getName()) {
                    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::LOCAL_VARREF));
                    ctx.writer.writeStringRef(std::to_string(i).c_str());
                    return true;
                }
            }
        }
    }
    return false;
}

static QoreValue read_expr_local_varref(AOTExprReadCtx& ctx) {
    const char* index_str = ctx.reader.readStringRef(ctx.ptr);
    if (!index_str) {
        return QoreValue();
    }
    int local_slot = std::atoi(index_str);
    if (local_slot < 0 || local_slot >= ctx.num_locals || !ctx.locals || !ctx.locals[local_slot]) {
        return QoreValue();
    }
    LocalVar* lv = ctx.locals[local_slot];
    // NOTE: always use false for in_closure — VT_LOCAL type calls ref.id->eval() which
    // internally checks closure_use and uses the correct lookup (local stack vs closure stack).
    // VT_CLOSURE uses thread_get_runtime_closure_var() (pointer-based runtime closure env lookup)
    // which returns null outside a closure execution context.
    VarRefNode* vrn = new VarRefNode(&loc_builtin, lv->getName(), lv, false);
    return QoreValue(vrn);
}

// ============================================================================
// GLOBAL_VARREF (8)
// ============================================================================

static bool is_numeric_global_slot_ref(const char* ref) {
    if (!ref || !*ref) {
        return false;
    }
    for (const char* p = ref; *p; ++p) {
        if (*p < '0' || *p > '9') {
            return false;
        }
    }
    return true;
}

static Var* resolve_global_varref_payload(const char* ref, QoreProgram* pgm,
        Var** globals, int num_globals) {
    if (!ref || !*ref) {
        return nullptr;
    }

    if (is_numeric_global_slot_ref(ref)) {
        int global_slot = std::atoi(ref);
        if (global_slot >= 0 && global_slot < num_globals && globals) {
            return globals[global_slot];
        }
        return nullptr;
    }

    constexpr const char* prefix = "name:";
    constexpr size_t prefix_len = 5;
    const char* name = !strncmp(ref, prefix, prefix_len) ? ref + prefix_len : ref;
    if (!pgm || !*name) {
        return nullptr;
    }

    qore_program_private* pp = qore_program_private::get(*pgm);
    const qore_ns_private* vns = nullptr;
    return qore_root_ns_private::runtimeFindGlobalVar(*pp->RootNS, name, vns);
}

static bool write_expr_global_varref(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* varref = dynamic_cast<const VarRefNode*>(node)) {
        if (varref->getType() == VT_GLOBAL || varref->getType() == VT_THREAD_LOCAL) {
            Var* global_var = varref->ref.var;
            if (global_var) {
                for (size_t i = 0; i < ctx.parent_globals.size(); ++i) {
                    if (ctx.parent_globals[i].name == global_var->getName()) {
                        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::GLOBAL_VARREF));
                        ctx.writer.writeStringRef(std::to_string(i).c_str());
                        return true;
                    }
                }
                const char* name = global_var->getName();
                if (name && *name) {
                    std::string ref = "name:";
                    ref += name;
                    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::GLOBAL_VARREF));
                    ctx.writer.writeStringRef(ref.c_str());
                    return true;
                }
            }
        }
    }
    return false;
}

static QoreValue read_expr_global_varref(AOTExprReadCtx& ctx) {
    const char* index_str = ctx.reader.readStringRef(ctx.ptr);
    if (!index_str) {
        ctx.error = "missing global variable reference payload";
        return QoreValue();
    }
    Var* gvar = resolve_global_varref_payload(index_str, ctx.pgm, ctx.globals, ctx.num_globals);
    if (!gvar) {
        ctx.error = "cannot resolve global variable reference '" + std::string(index_str)
            + "' while deserializing native AOT expression";
        return QoreValue();
    }
    GlobalVarRefNode* vrn = new GlobalVarRefNode(&loc_builtin, strdup(gvar->getName()), gvar);
    return QoreValue(vrn);
}

// ============================================================================
// CONST_NUMBER (9)
// ============================================================================

static bool write_expr_const_number(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* num = dynamic_cast<const QoreNumberNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_NUMBER));
        QoreString str;
        num->toString(str);
        ctx.writer.writeStringRef(str.c_str());
        return true;
    }
    return false;
}

static QoreValue read_expr_const_number(AOTExprReadCtx& ctx) {
    const char* num_str = ctx.reader.readStringRef(ctx.ptr);
    if (!num_str || !*num_str) {
        return QoreValue();
    }
    QoreNumberNode* num = new QoreNumberNode(num_str);
    return QoreValue(num);
}

// ============================================================================
// CONST_BINARY (10)
// ============================================================================

static bool write_expr_const_binary(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* bin = dynamic_cast<const BinaryNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_BINARY));
        std::string hex;
        const unsigned char* data = static_cast<const unsigned char*>(bin->getPtr());
        size_t len = bin->size();
        hex.reserve(len * 2);
        for (size_t i = 0; i < len; ++i) {
            char buf[3];
            snprintf(buf, sizeof(buf), "%02x", data[i]);
            hex.append(buf);
        }
        ctx.writer.writeStringRef(hex.c_str());
        return true;
    }
    return false;
}

static QoreValue read_expr_const_binary(AOTExprReadCtx& ctx) {
    const char* hex_str = ctx.reader.readStringRef(ctx.ptr);
    if (!hex_str) {
        return QoreValue();
    }
    size_t hex_len = strlen(hex_str);
    size_t bin_len = hex_len / 2;
    SimpleRefHolder<BinaryNode> bin(new BinaryNode);
    if (bin_len > 0) {
        bin->preallocate(bin_len);
        unsigned char* dst = static_cast<unsigned char*>(
            const_cast<void*>(bin->getPtr()));
        for (size_t i = 0; i < bin_len; ++i) {
            unsigned int byte;
            sscanf(hex_str + i * 2, "%2x", &byte);
            dst[i] = static_cast<unsigned char>(byte);
        }
    }
    return QoreValue(bin.release());
}

// ============================================================================
// CLOSURE_CREATE (11)
// ============================================================================

static bool write_expr_closure_create(AOTExprWriteCtx& ctx) {
    // Closures in expression context are now serialized by classifyAndWriteExpr()
    // in QoreAOTBinary.cpp. This handler is only called from the expression registry
    // path which is not used for closure writes.
    return false;
}

static QoreValue read_expr_closure_create(AOTExprReadCtx& ctx) {
    // Read flags
    const char* flags_str = ctx.reader.readStringRef(ctx.ptr);
    const char* class_type_path = ctx.reader.readStringRef(ctx.ptr);

    bool is_lambda = false, is_in_method = false;
    if (flags_str) {
        is_lambda = (flags_str[0] == '1');
        is_in_method = (strlen(flags_str) >= 3 && flags_str[2] == '1');
    }

    // Read return type
    const char* ret_type_path = ctx.reader.readStringRef(ctx.ptr);
    std::string type_error;
    QoreAOTTypeResolver type_resolver(ctx.pgm);
    const QoreTypeInfo* ret_type = (ret_type_path && *ret_type_path)
        ? type_resolver.resolve(ret_type_path, type_error) : nullptr;

    // Read params
    uint16_t closure_num_params = QoreAOTBinaryReader::readU16(ctx.ptr);
    std::vector<std::string> param_names(closure_num_params);
    std::vector<const QoreTypeInfo*> param_types(closure_num_params);
    std::vector<QoreValue> defaults(closure_num_params);
    for (uint16_t p = 0; p < closure_num_params; ++p) {
        const char* pname = ctx.reader.readStringRef(ctx.ptr);
        param_names[p] = pname ? pname : "";
        const char* ptype = ctx.reader.readStringRef(ctx.ptr);
        param_types[p] = (ptype && *ptype)
            ? type_resolver.resolve(ptype, type_error) : nullptr;
        uint8_t has_default = QoreAOTBinaryReader::readU8(ctx.ptr);
        if (has_default) {
            std::string val_error;
            defaults[p] = ctx.reader.readValue(ctx.ptr, ctx.end, val_error);
        }
    }
    bool closure_sig_has_varargs = false;
    bool closure_needs_extra_args = false;
    if ((ctx.reader.getHeader().feature_flags & QORE_AOT_FEAT_CLOSURE_VARARGS_FLAGS) != 0) {
        uint16_t closure_flags = QoreAOTBinaryReader::readU16(ctx.ptr);
        closure_needs_extra_args = (closure_flags & 0x0001) != 0;
        closure_sig_has_varargs = (closure_flags & 0x0004) != 0;
    } else {
        bool old_varargs = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;
        closure_sig_has_varargs = old_varargs;
        closure_needs_extra_args = old_varargs;
    }

    // Read captured variable names and parent slot indices
    uint16_t num_captured = QoreAOTBinaryReader::readU16(ctx.ptr);
    std::vector<std::string> captured_names(num_captured);
    std::vector<int32_t> captured_parent_slots(num_captured, -1);
    for (uint16_t c = 0; c < num_captured; ++c) {
        const char* cname = ctx.reader.readStringRef(ctx.ptr);
        captured_names[c] = cname ? cname : "";
        captured_parent_slots[c] = static_cast<int32_t>(QoreAOTBinaryReader::readU32(ctx.ptr));
    }

    // Read closure body IR
    uint8_t has_ir = QoreAOTBinaryReader::readU8(ctx.ptr);
    if (!has_ir) {
        ctx.error = "closure has no IR data";
        return QoreValue();
    }

    uint32_t ir_size = QoreAOTBinaryReader::readU32(ctx.ptr);
    const uint8_t* ir_end_ptr = ctx.ptr + ir_size;

    // Resolve class for method context
    const QoreClass* closure_class = nullptr;
    if (class_type_path && *class_type_path) {
        closure_class = qore_aot_resolve_class_ref(ctx.pgm, class_type_path, false);
    }

    // Construct UserClosureFunction + UserClosureVariant
    ExceptionSink closure_xsink;
    ProgramRuntimeParseContextHelper closure_pch(&closure_xsink, ctx.pgm);
    if (closure_xsink.isException()) {
        closure_xsink.clear();
        ctx.ptr = ir_end_ptr;
        ctx.error = "failed to acquire parse context for closure";
        return QoreValue();
    }
    auto* ucf = new UserClosureFunction(nullptr, 0, 0, QoreValue(), nullptr);
    auto* closure_variant = static_cast<UserClosureVariant*>(
        const_cast<AbstractQoreFunctionVariant*>(ucf->first()));
    UserSignature* closure_sig = closure_variant->getUserSignature();
    closure_sig->setupFromAOTMetadata(
        ctx.pgm, ret_type,
        std::move(param_names), std::move(param_types), std::move(defaults),
        closure_sig_has_varargs, closure_class);
    if (closure_needs_extra_args) {
        closure_variant->setFlag(QCF_USES_EXTRA_ARGS);
    }

    // Build enclosing locals map for IR deserialization
    std::unordered_map<std::string, LocalVar*> enclosing_locals;
    // 1. Parent function's locals
    for (int l = 0; l < ctx.num_locals; ++l) {
        if (ctx.locals[l] && ctx.locals[l]->getName()) {
            enclosing_locals[ctx.locals[l]->getName()] = ctx.locals[l];
        }
    }
    // 1b. Override with slot-indexed captured variables for disambiguation
    for (uint16_t ci = 0; ci < num_captured; ++ci) {
        int32_t parent_slot = captured_parent_slots[ci];
        if (parent_slot >= 0 && parent_slot < ctx.num_locals
                && ctx.locals[parent_slot]) {
            enclosing_locals[captured_names[ci]] = ctx.locals[parent_slot];
        }
    }
    // 1c. Top-level locals live in the program LVList, not in the enclosing
    // method/function context.  Add them as a fallback so closures created
    // inside methods can still capture program-scope locals.
    if (ctx.pgm) {
        if (const LVList* top_lvars = qore_program_private::get(*ctx.pgm)->sb.getLVList()) {
            for (unsigned i = 0; i < top_lvars->size(); ++i) {
                LocalVar* lv = top_lvars->lv[i];
                if (lv && lv->getName() && !enclosing_locals.count(lv->getName())) {
                    enclosing_locals[lv->getName()] = lv;
                }
            }
        }
    }
    // 2. Closure's own parameter locals from its signature
    for (unsigned p = 0; p < closure_sig->numParams(); ++p) {
        const char* pname = closure_sig->getName(p);
        if (pname && *pname) {
            enclosing_locals[pname] = closure_sig->lv[p];
        }
    }
    if (closure_sig->argvid) {
        enclosing_locals["argv"] = closure_sig->argvid;
    }
    if (closure_sig->selfid) {
        enclosing_locals["self"] = closure_sig->selfid;
    }

    // Build the locals array used by EXPR_TREE blob deserialization.  Closure
    // body expressions are serialized against the closure IR local slot table;
    // deserializeIRFunction() fills this vector by slot ID after reading that
    // table and before reading instruction expression fields.
    std::vector<LocalVar*> closure_locals_vec;

    // Deserialize closure body IR
    std::string ir_error;
    auto readExprCb = [&ctx, &closure_locals_vec]
            (const QoreAOTBinaryReader& rdr, const uint8_t*& p,
            const uint8_t* e, std::string& err) -> QoreValue {
        LocalVar** arr = closure_locals_vec.empty()
            ? nullptr : closure_locals_vec.data();
        int cnt = static_cast<int>(closure_locals_vec.size());
        return readOneTopLevelIRExpr(rdr, p, e, err, ctx.pgm,
            arr, cnt, ctx.globals, ctx.num_globals);
    };
    auto closure_ir = deserializeIRFunction(ctx.reader, ctx.ptr, ir_end_ptr, ctx.pgm,
        readExprCb, &enclosing_locals, ir_error,
        ctx.locals, ctx.num_locals, &closure_locals_vec);
    ctx.ptr = ir_end_ptr;  // Ensure we advance past IR data

    if (!closure_ir) {
        printd(2, "AOT: closure IR deser failed in expr context: %s\n", ir_error.c_str());
        delete ucf;
        ctx.error = "closure IR deserialization failed: " + ir_error;
        return QoreValue();
    }
    // Set up captured variables in LVarSet and ensure closureUse
    LVarSet* closure_vlist = ucf->getVList();
    for (uint16_t ci = 0; ci < num_captured; ++ci) {
        int32_t parent_slot = captured_parent_slots[ci];
        LocalVar* lv = nullptr;
        if (parent_slot >= 0 && parent_slot < ctx.num_locals
                && ctx.locals[parent_slot]) {
            lv = ctx.locals[parent_slot];
        } else {
            auto it = enclosing_locals.find(captured_names[ci]);
            if (it != enclosing_locals.end()) {
                lv = it->second;
            }
        }
        if (lv) {
            closure_vlist->add(lv);
            if (!lv->closureUse()) {
                lv->setClosureUse();
            }
        }
    }

    LocalVar* closure_selfid = closure_sig->selfid;
    if (!closure_selfid) {
        auto self_it = enclosing_locals.find("self");
        if (self_it != enclosing_locals.end()) {
            closure_selfid = self_it->second;
        }
    }
    if (closure_selfid && !closure_sig->selfid) {
        closure_sig->setSelfId(closure_selfid);
    }

    // Populate pre_instantiated_locals
    for (unsigned p = 0; p < closure_sig->numParams(); ++p) {
        if (closure_sig->lv[p]) {
            closure_ir->pre_instantiated_locals.insert(
                reinterpret_cast<const void*>(closure_sig->lv[p]));
        }
    }
    if (closure_sig->argvid) {
        closure_ir->pre_instantiated_locals.insert(
            reinterpret_cast<const void*>(closure_sig->argvid));
    }
    if (closure_selfid) {
        closure_ir->pre_instantiated_locals.insert(
            reinterpret_cast<const void*>(closure_selfid));
    }
    for (LocalVar* lv : closure_ir->all_body_locals) {
        closure_ir->pre_instantiated_locals.insert(
            reinterpret_cast<const void*>(lv));
    }

    // Build cached_pre_instantiated for the IR interpreter
    auto* cached_pre_inst = new std::unordered_set<const LocalVar*>();
    for (unsigned p = 0; p < closure_sig->numParams(); ++p) {
        if (closure_sig->lv[p]) {
            cached_pre_inst->insert(closure_sig->lv[p]);
        }
    }
    if (closure_sig->argvid) {
        cached_pre_inst->insert(closure_sig->argvid);
    }
    if (closure_selfid) {
        cached_pre_inst->insert(closure_selfid);
    }
    closure_ir->cached_pre_instantiated = cached_pre_inst;

    // Set cached IR on variant
    makeExprDeserializedClosureIRNameUnique(*closure_ir, closure_variant);
    closure_ir->computeSlotIdsAndEmbed();
    closure_variant->setCachedIR(closure_ir.release());
    closure_variant->pgm = ctx.pgm;

    // Set class type if in a method context
    if (class_type_path && *class_type_path) {
        ucf->setClassType(type_resolver.resolve(class_type_path, type_error));
    }

    // Create QoreClosureParseNode
    auto* closure_node = new QoreClosureParseNode(nullptr, ucf, is_lambda, is_in_method);
    return QoreValue(closure_node);
}

// ============================================================================
// CALL_REF (12)
// ============================================================================

static bool write_expr_call_ref(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* mcr = dynamic_cast<const LocalMethodCallReferenceNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::BOUND_METHOD_REF));
        const QoreMethod* method = mcr->getMethod();
        const QoreClass* qc = method ? method->getClass() : nullptr;
        std::string class_ref = qore_aot_encode_class_ref(qc);
        ctx.writer.writeStringRef(class_ref.c_str());
        ctx.writer.writeStringRef(method ? method->getName() : "");
        return true;
    }
    if (auto* scr = dynamic_cast<const LocalStaticMethodCallReferenceNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::STATIC_METHOD_REF));
        const QoreMethod* method = scr->getMethod();
        const QoreClass* qc = method ? method->getClass() : nullptr;
        std::string class_ref = qore_aot_encode_class_ref(qc);
        ctx.writer.writeStringRef(class_ref.c_str());
        ctx.writer.writeStringRef(method ? method->getName() : "");
        return true;
    }
    if (auto* fcr = dynamic_cast<const LocalFunctionCallReferenceNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::FUNC_CALL_REF));
        QoreFunction* f = fcr->getFunction();
        ctx.writer.writeStringRef(f ? f->getName() : "");
        return true;
    }
    return false;
}

static QoreValue read_expr_call_ref(AOTExprReadCtx& ctx) {
    const char* func_name = ctx.reader.readStringRef(ctx.ptr);
    if (!func_name || !*func_name) {
        ctx.error = "empty call reference function name";
        return QoreValue();
    }
    qore_program_private* pp = qore_program_private::get(*ctx.pgm);
    const FunctionEntry* fe = qore_root_ns_private::runtimeFindFunctionEntry(
        *pp->RootNS, func_name);
    QoreFunction* f = fe ? fe->getFunction() : nullptr;
    if (!f) {
        ctx.error = std::string("cannot resolve function call reference '") + func_name + "'";
        return QoreValue();
    }
    return QoreValue(new LocalFunctionCallReferenceNode(&loc_builtin, f));
}

static bool write_expr_callref_call(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* crc = dynamic_cast<const CallReferenceCallNode*>(node);
    if (!crc) {
        return false;
    }

    const QoreListNode* args = crc->getArgs();
    const QoreParseListNode* pargs = crc->getParseArgs();
    size_t nargs = args ? args->size() : (pargs ? pargs->size() : 0);
    if (nargs > 255) {
        return false;
    }

    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CALLREF_CALL));
    if (!classifyAndWriteExpr(ctx.writer, crc->getExp(), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map)) {
        return false;
    }
    ctx.writer.writeU8(static_cast<uint8_t>(nargs));
    for (size_t j = 0; j < nargs; ++j) {
        QoreValue arg = args ? args->retrieveEntry(j) : pargs->get(j);
        if (!classifyAndWriteExpr(ctx.writer, arg, ctx.parent_locals,
                ctx.parent_globals, ctx.const_reverse_map)) {
            return false;
        }
    }
    return true;
}

static QoreValue read_expr_callref_call(AOTExprReadCtx& ctx) {
    std::string callee_err;
    QoreValue callee = readOneExpr(ctx.reader, ctx.ptr, ctx.end, callee_err,
        ctx.pgm, ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!callee_err.empty()) {
        ctx.error = "CALLREF_CALL callee: " + callee_err;
        return QoreValue();
    }
    if (ctx.ptr >= ctx.end) {
        callee.discard(nullptr);
        ctx.error = "truncated CALLREF_CALL argument count";
        return QoreValue();
    }
    uint8_t nargs = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreParseListNode* args = nullptr;
    if (nargs) {
        args = new QoreParseListNode(&loc_builtin);
        for (uint8_t i = 0; i < nargs; ++i) {
            std::string arg_err;
            QoreValue arg = readOneExpr(ctx.reader, ctx.ptr, ctx.end, arg_err,
                ctx.pgm, ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
            if (!arg_err.empty()) {
                delete args;
                callee.discard(nullptr);
                arg.discard(nullptr);
                ctx.error = "CALLREF_CALL argument " + std::to_string(i) + ": " + arg_err;
                return QoreValue();
            }
            args->add(arg, &loc_builtin);
        }
    }
    auto* call = new CallReferenceCallNode(&loc_builtin, callee, args);
    call->resolveParseArgs();
    return QoreValue(call);
}

// ============================================================================
// OBJ_METHOD_REF (13)
// ============================================================================

static bool write_expr_obj_method_ref(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* smr = dynamic_cast<const ParseSelfMethodReferenceNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::SELF_METHOD_REF));
        ctx.writer.writeStringRef(smr->getMethodName().c_str());
        return true;
    }
    if (auto* omr = dynamic_cast<const ParseObjectMethodReferenceNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::OBJ_METHOD_REF_EXPR));
        ctx.writer.writeStringRef(omr->getMethodName().c_str());
        return classifyAndWriteExpr(ctx.writer, omr->getExp(), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map);
    }
    return false;
}

static QoreValue read_expr_obj_method_ref(AOTExprReadCtx& ctx) {
    const char* method_name = ctx.reader.readStringRef(ctx.ptr);
    if (!method_name || !*method_name) {
        ctx.error = "empty object method reference name";
        return QoreValue();
    }
    return QoreValue(new ParseSelfMethodReferenceNode(&loc_builtin, strdup(method_name)));
}

static QoreValue read_aot_function_call_ref(AOTExprReadCtx& ctx) {
    const char* func_name = ctx.reader.readStringRef(ctx.ptr);
    if (!func_name || !*func_name) {
        ctx.error = "empty function call reference name";
        return QoreValue();
    }

    qore_program_private* pp = qore_program_private::get(*ctx.pgm);
    const FunctionEntry* fe = qore_root_ns_private::runtimeFindFunctionEntry(
        *pp->RootNS, func_name);
    QoreFunction* f = fe ? fe->getFunction() : nullptr;
    if (!f) {
        ctx.error = std::string("cannot resolve function call reference '") + func_name + "'";
        return QoreValue();
    }
    return QoreValue(new LocalFunctionCallReferenceNode(&loc_builtin, f));
}

static const QoreMethod* read_aot_method_ref_target(AOTExprReadCtx& ctx, const char* kind,
        bool prefer_static) {
    const char* class_path = ctx.reader.readStringRef(ctx.ptr);
    const char* method_name = ctx.reader.readStringRef(ctx.ptr);
    if (!class_path || !*class_path || !method_name || !*method_name) {
        ctx.error = std::string("empty ") + kind + " method reference metadata";
        return nullptr;
    }

    const QoreClass* qc = qore_aot_resolve_class_ref(ctx.pgm, class_path, false);
    if (!qc) {
        ctx.error = std::string("cannot resolve class '") + class_path + "' for "
            + kind + " method reference";
        return nullptr;
    }

    const QoreMethod* method = prefer_static
        ? qc->findStaticMethod(method_name) : qc->findMethod(method_name);
    if (!method) {
        method = prefer_static ? qc->findMethod(method_name) : qc->findStaticMethod(method_name);
    }
    if (!method) {
        ctx.error = std::string("cannot resolve ") + kind + " method reference '"
            + class_path + "::" + method_name + "'";
    }
    return method;
}

static bool write_expr_func_call_ref(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* fcr = dynamic_cast<const LocalFunctionCallReferenceNode*>(node);
    if (!fcr) {
        return false;
    }
    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::FUNC_CALL_REF));
    QoreFunction* f = fcr->getFunction();
    ctx.writer.writeStringRef(f ? f->getName() : "");
    return true;
}

static QoreValue read_expr_func_call_ref(AOTExprReadCtx& ctx) {
    return read_aot_function_call_ref(ctx);
}

static bool write_expr_bound_method_ref(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* mcr = dynamic_cast<const LocalMethodCallReferenceNode*>(node);
    if (!mcr) {
        return false;
    }
    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::BOUND_METHOD_REF));
    const QoreMethod* method = mcr->getMethod();
    const QoreClass* qc = method ? method->getClass() : nullptr;
    std::string class_ref = qore_aot_encode_class_ref(qc);
    ctx.writer.writeStringRef(class_ref.c_str());
    ctx.writer.writeStringRef(method ? method->getName() : "");
    return true;
}

static QoreValue read_expr_bound_method_ref(AOTExprReadCtx& ctx) {
    const QoreMethod* method = read_aot_method_ref_target(ctx, "bound", false);
    return method ? QoreValue(new LocalMethodCallReferenceNode(&loc_builtin, method)) : QoreValue();
}

static bool write_expr_static_method_ref(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (dynamic_cast<const LocalMethodCallReferenceNode*>(node)) {
        return false;
    }
    auto* scr = dynamic_cast<const LocalStaticMethodCallReferenceNode*>(node);
    if (!scr) {
        return false;
    }
    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::STATIC_METHOD_REF));
    const QoreMethod* method = scr->getMethod();
    const QoreClass* qc = method ? method->getClass() : nullptr;
    std::string class_ref = qore_aot_encode_class_ref(qc);
    ctx.writer.writeStringRef(class_ref.c_str());
    ctx.writer.writeStringRef(method ? method->getName() : "");
    return true;
}

static QoreValue read_expr_static_method_ref(AOTExprReadCtx& ctx) {
    const QoreMethod* method = read_aot_method_ref_target(ctx, "static", true);
    return method ? QoreValue(new LocalStaticMethodCallReferenceNode(&loc_builtin, method)) : QoreValue();
}

static bool write_expr_self_method_ref(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* smr = dynamic_cast<const ParseSelfMethodReferenceNode*>(node);
    if (!smr) {
        return false;
    }
    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::SELF_METHOD_REF));
    ctx.writer.writeStringRef(smr->getMethodName().c_str());
    return true;
}

static QoreValue read_expr_self_method_ref(AOTExprReadCtx& ctx) {
    const char* method_name = ctx.reader.readStringRef(ctx.ptr);
    if (!method_name || !*method_name) {
        ctx.error = "empty self method reference name";
        return QoreValue();
    }
    return QoreValue(new ParseSelfMethodReferenceNode(&loc_builtin, strdup(method_name)));
}

static bool write_expr_obj_method_ref_expr(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* omr = dynamic_cast<const ParseObjectMethodReferenceNode*>(node);
    if (!omr) {
        return false;
    }
    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::OBJ_METHOD_REF_EXPR));
    ctx.writer.writeStringRef(omr->getMethodName().c_str());
    return classifyAndWriteExpr(ctx.writer, omr->getExp(), ctx.parent_locals,
        ctx.parent_globals, ctx.const_reverse_map);
}

static QoreValue read_expr_obj_method_ref_expr(AOTExprReadCtx& ctx) {
    const char* method_name = ctx.reader.readStringRef(ctx.ptr);
    if (!method_name || !*method_name) {
        ctx.error = "empty object method reference name";
        return QoreValue();
    }

    std::string target_err;
    QoreValue target = readOneExpr(ctx.reader, ctx.ptr, ctx.end, target_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!target_err.empty()) {
        ctx.error = target_err;
        target.discard(nullptr);
        return QoreValue();
    }
    return QoreValue(new ParseObjectMethodReferenceNode(&loc_builtin, target, strdup(method_name)));
}

// ============================================================================
// STATIC_VARREF (14)
// ============================================================================

static bool write_expr_static_varref(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* sv = dynamic_cast<const StaticClassVarRefNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::STATIC_VARREF));
        std::string class_ref = qore_aot_encode_class_ref(&sv->qc);
        ctx.writer.writeStringRef(class_ref.c_str());
        ctx.writer.writeStringRef(sv->str.c_str());
        return true;
    }
    return false;
}

static QoreValue read_expr_static_varref(AOTExprReadCtx& ctx) {
    const char* class_name = ctx.reader.readStringRef(ctx.ptr);
    const char* var_name = ctx.reader.readStringRef(ctx.ptr);
    if (!class_name || !var_name) {
        return QoreValue();
    }
    const QoreClass* qc = qore_aot_resolve_class_ref(ctx.pgm, class_name, false);
    if (!qc) {
        return QoreValue();
    }
    const QoreClass* owner_qc = qc;
    const QoreExternalStaticMember* m = qc->findLocalStaticMember(var_name);
    if (!m) {
        QoreClassHierarchyIterator hi(*qc);
        while (hi.next()) {
            const QoreClass& pqc = hi.get();
            m = pqc.findLocalStaticMember(var_name);
            if (m) {
                owner_qc = &pqc;
                break;
            }
        }
    }
    if (!m) {
        return QoreValue();
    }
    QoreVarInfo* vi = const_cast<QoreVarInfo*>(
        reinterpret_cast<const QoreVarInfo*>(m));
    StaticClassVarRefNode* node = new StaticClassVarRefNode(&loc_builtin, var_name,
        *owner_qc, *vi);
    return QoreValue(node);
}

// ============================================================================
// SCOPED_NEW_OBJECT (15)
// ============================================================================

static bool write_expr_scoped_new_object(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* socn = dynamic_cast<const ScopedObjectCallNode*>(node)) {
        if (socn->oc) {
            ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::SCOPED_NEW_OBJECT));
            std::string class_ref = qore_aot_encode_class_ref(socn->oc);
            ctx.writer.writeStringRef(class_ref.c_str());
            const QoreListNode* args = socn->getArgs();
            if (args && args->size() > 255) {
                return false;
            }
            uint8_t num_args = args ? static_cast<uint8_t>(args->size()) : 0;
            ctx.writer.writeU8(num_args);
            for (uint8_t j = 0; j < num_args; ++j) {
                if (!classifyAndWriteExpr(ctx.writer, args->retrieveEntry(j),
                        ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)) {
                    return false;
                }
            }
            return true;
        }
    }
    return false;
}

static QoreValue read_expr_scoped_new_object(AOTExprReadCtx& ctx) {
    const char* class_path = ctx.reader.readStringRef(ctx.ptr);
    uint8_t num_args = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreListNode* args_list = nullptr;
    if (num_args > 0) {
        args_list = qore_list_private::newList(true);
        for (uint8_t j = 0; j < num_args; ++j) {
            std::string arg_err;
            QoreValue arg = readOneExpr(ctx.reader, ctx.ptr, ctx.end, arg_err, ctx.pgm,
                ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
            if (!arg_err.empty()) {
                ctx.error = arg_err;
                args_list->deref(nullptr);
                return QoreValue();
            }
            args_list->push(arg, nullptr);
        }
    }
    if (!class_path || !*class_path) {
        if (args_list) {
            args_list->deref(nullptr);
        }
        return QoreValue();
    }
    const QoreClass* qc = qore_aot_resolve_class_ref(ctx.pgm, class_path, false);
    if (!qc) {
        if (args_list) {
            args_list->deref(nullptr);
        }
        return QoreValue();
    }
    std::string variant_err;
    const AbstractQoreFunctionVariant* variant = resolve_expr_constructor_variant(qc, args_list, class_path,
        variant_err);
    if (!variant_err.empty()) {
        if (args_list) {
            args_list->deref(nullptr);
        }
        ctx.error = variant_err;
        return QoreValue();
    }
    // ScopedObjectCallNode expects QoreParseListNode, not QoreListNode
    QoreParseListNode* pln = nullptr;
    if (args_list) {
        pln = new QoreParseListNode(&loc_builtin);
        ConstListIterator li(args_list);
        while (li.next()) {
            QoreValue v = li.getValue();
            v.refSelf();
            pln->add(v, &loc_builtin);
        }
        args_list->deref(nullptr);
    }
    ScopedObjectCallNode* socn = new ScopedObjectCallNode(&loc_builtin, qc, pln);
    // Convert parse_args to args so evalImpl() doesn't hit the assertion
    if (pln) {
        socn->resolveParseArgs();
    }
    if (variant) {
        socn->setVariant(variant);
    }
    return QoreValue(socn);
}

// ============================================================================
// HASHDECL_NEW (16)
// ============================================================================

static bool write_expr_hashdecl_new(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* nhd = dynamic_cast<const NewHashDeclNode*>(node)) {
        if (nhd->hd) {
            ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASHDECL_NEW));
            ctx.writer.writeStringRef(nhd->hd->getNamespacePath().c_str());
            // Serialize constructor args (typically a single hash initializer)
            size_t nargs = nhd->args ? nhd->args->size() : 0;
            if (nargs > 255) {
                return false;
            }
            if (nargs > 0) {
                ctx.writer.writeU8(static_cast<uint8_t>(nargs));
                for (size_t j = 0; j < nhd->args->size(); ++j) {
                    if (!::classifyAndWriteExpr(ctx.writer, nhd->args->get(j),
                            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)) {
                        return false;
                    }
                }
            } else {
                ctx.writer.writeU8(0);
            }
            return true;
        }
    }
    return false;
}

static QoreValue read_expr_hashdecl_new(AOTExprReadCtx& ctx) {
    const char* hashdecl_path = ctx.reader.readStringRef(ctx.ptr);
    if (!hashdecl_path || !*hashdecl_path) {
        return QoreValue();
    }
    uint8_t num_args = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreListNode* call_args = nullptr;
    if (num_args > 0) {
        call_args = qore_list_private::newList(true);
        for (uint8_t j = 0; j < num_args; ++j) {
            std::string arg_err;
            QoreValue arg = readOneExpr(ctx.reader, ctx.ptr, ctx.end, arg_err, ctx.pgm,
                ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
            if (!arg_err.empty()) {
                arg.discard(nullptr);
                call_args->push(QoreValue(), nullptr);
            } else {
                call_args->push(arg, nullptr);
            }
        }
    }
    qore_program_private* pp = qore_program_private::get(*ctx.pgm);
    const qore_ns_private* found_ns = nullptr;
    const TypedHashDecl* hd = qore_root_ns_private::runtimeFindHashDecl(
        *pp->RootNS, hashdecl_path, found_ns);
    if (!hd) {
        if (call_args) {
            call_args->deref(nullptr);
        }
        return QoreValue();
    }
    // Convert call_args to QoreParseListNode for NewHashDeclNode
    QoreParseListNode* pln = nullptr;
    if (call_args) {
        pln = new QoreParseListNode(&loc_builtin);
        ConstListIterator li(call_args);
        while (li.next()) {
            QoreValue v = li.getValue();
            v.refSelf();
            pln->add(v, &loc_builtin);
        }
        call_args->deref(nullptr);
    }
    NewHashDeclNode* nhd = new NewHashDeclNode(&loc_builtin, hd, pln, false);
    return QoreValue(nhd);
}

// ============================================================================
// COMPLEX_HASH_NEW (17)
// ============================================================================

static bool write_expr_complex_hash_new(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* nch = dynamic_cast<const NewComplexHashNode*>(node)) {
        if (nch->typeInfo) {
            ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::COMPLEX_HASH_NEW));
            ctx.writer.writeStringRef(QoreTypeInfo::getPath(nch->typeInfo));
            // Serialize constructor args
            size_t nargs = nch->args ? nch->args->size() : 0;
            if (nargs > 255) {
                return false;
            }
            if (nargs > 0) {
                ctx.writer.writeU8(static_cast<uint8_t>(nargs));
                for (size_t j = 0; j < nch->args->size(); ++j) {
                    if (!::classifyAndWriteExpr(ctx.writer, nch->args->get(j),
                            ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)) {
                        return false;
                    }
                }
            } else {
                ctx.writer.writeU8(0);
            }
            return true;
        }
    }
    return false;
}

static QoreValue read_expr_complex_hash_new(AOTExprReadCtx& ctx) {
    const char* type_path = ctx.reader.readStringRef(ctx.ptr);
    if (!type_path || !*type_path) {
        return QoreValue();
    }
    uint8_t num_args = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreListNode* call_args = nullptr;
    if (num_args > 0) {
        call_args = qore_list_private::newList(true);
        for (uint8_t j = 0; j < num_args; ++j) {
            std::string arg_err;
            QoreValue arg = readOneExpr(ctx.reader, ctx.ptr, ctx.end, arg_err, ctx.pgm,
                ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
            if (!arg_err.empty()) {
                arg.discard(nullptr);
                call_args->push(QoreValue(), nullptr);
            } else {
                call_args->push(arg, nullptr);
            }
        }
    }
    std::string type_error;
    QoreAOTTypeResolver type_resolver(ctx.pgm);
    const QoreTypeInfo* ti = type_resolver.resolve(type_path, type_error);
    if (!ti) {
        if (call_args) {
            call_args->deref(nullptr);
        }
        return QoreValue();
    }
    // Convert call_args to QoreParseListNode for NewComplexHashNode
    QoreParseListNode* pln = nullptr;
    if (call_args) {
        pln = new QoreParseListNode(&loc_builtin);
        ConstListIterator li(call_args);
        while (li.next()) {
            QoreValue v = li.getValue();
            v.refSelf();
            pln->add(v, &loc_builtin);
        }
        call_args->deref(nullptr);
    }
    NewComplexHashNode* nch = new NewComplexHashNode(&loc_builtin, ti, pln);
    return QoreValue(nch);
}

// ============================================================================
// COMPLEX_LIST_NEW (18)
// ============================================================================

static bool write_expr_complex_list_new(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* ncl = dynamic_cast<const NewComplexListNode*>(node)) {
        if (ncl->typeInfo) {
            ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::COMPLEX_LIST_NEW));
            ctx.writer.writeStringRef(QoreTypeInfo::getPath(ncl->typeInfo));
            // Serialize constructor arg (single QoreValue)
            if (ncl->args.hasNode()) {
                ctx.writer.writeU8(1);
                if (!::classifyAndWriteExpr(ctx.writer, ncl->args,
                        ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)) {
                    return false;
                }
            } else {
                ctx.writer.writeU8(0);
            }
            return true;
        }
    }
    return false;
}

static QoreValue read_expr_complex_list_new(AOTExprReadCtx& ctx) {
    const char* type_path = ctx.reader.readStringRef(ctx.ptr);
    if (!type_path || !*type_path) {
        return QoreValue();
    }
    uint8_t num_args = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreValue arg_val;
    if (num_args > 0) {
        std::string arg_err;
        arg_val = readOneExpr(ctx.reader, ctx.ptr, ctx.end, arg_err, ctx.pgm,
            ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
        if (!arg_err.empty()) {
            arg_val.discard(nullptr);
            arg_val = QoreValue();
        }
    }
    std::string type_error;
    QoreAOTTypeResolver type_resolver(ctx.pgm);
    const QoreTypeInfo* ti = type_resolver.resolve(type_path, type_error);
    if (!ti) {
        arg_val.discard(nullptr);
        return QoreValue();
    }
    NewComplexListNode* ncl = new NewComplexListNode(&loc_builtin, ti, arg_val);
    return QoreValue(ncl);
}

// ============================================================================
// CONST_ENUM (19)
// ============================================================================

static bool write_expr_const_enum(AOTExprWriteCtx& ctx) {
    if (!ctx.expr.hasNode() && ctx.expr.isEnum()) {
        const QoreEnumMember* member = ctx.expr.getEnumMember();
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_ENUM));
        std::string ns_path = member->getEnumDecl()->getNamespacePath();
        ctx.writer.writeStringRef(ns_path.c_str());
        ctx.writer.writeStringRef(member->getName());
        return true;
    }
    return false;
}

static QoreValue read_expr_const_enum(AOTExprReadCtx& ctx) {
    const char* enum_path = ctx.reader.readStringRef(ctx.ptr);
    const char* member_name = ctx.reader.readStringRef(ctx.ptr);
    if (!enum_path || !member_name) {
        return QoreValue();
    }
    const QoreNamespace* pns = nullptr;
    const QoreEnumDecl* ed = ctx.pgm->findEnum(enum_path, pns);
    if (!ed) {
        return QoreValue();
    }
    const QoreEnumMember* member = ed->findMember(member_name);
    if (!member) {
        return QoreValue();
    }
    return QoreValue::makeEnum(member);
}

// ============================================================================
// CONST_STRING (20)
// ============================================================================

static bool write_expr_const_string(AOTExprWriteCtx& ctx) {
    if (ctx.expr.isShortString()) {
        char buf[8];
        ctx.expr.getShortString(buf);
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_STRING));
        ctx.writer.writeStringRef(buf, ctx.expr.shortStringLen());
        return true;
    }

    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* str = dynamic_cast<const QoreStringNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_STRING));
        ctx.writer.writeStringRef(str->c_str(), str->size());
        return true;
    }
    return false;
}

static QoreValue read_expr_const_string(AOTExprReadCtx& ctx) {
    const char* str_content = ctx.reader.readStringRef(ctx.ptr);
    return QoreValue::makeStringValue(str_content);
}

// ============================================================================
// HASH_LITERAL (21)
// ============================================================================

static bool write_expr_hash_literal(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* qhn = dynamic_cast<const QoreHashNode*>(node)) {
        if (qhn->size() <= 255) {
            ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASH_LITERAL));
            ctx.writer.writeU8(static_cast<uint8_t>(qhn->size()));
            ConstHashIterator it(qhn);
            while (it.next()) {
                ctx.writer.writeStringRef(it.getKey());
                if (!classifyAndWriteExpr(ctx.writer, it.get(), ctx.parent_locals,
                        ctx.parent_globals, ctx.const_reverse_map)) {
                    return false;
                }
            }
            return true;
        }
    }
    if (auto* phn = dynamic_cast<const QoreParseHashNode*>(node)) {
        const QoreParseHashNode::nvec_t& keys = phn->getKeys();
        const QoreParseHashNode::nvec_t& vals = phn->getValues();
        if (keys.size() <= 255) {
            for (const QoreValue& key : keys) {
                if (key.needsEval()) {
                    return false;
                }
            }
            ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASH_LITERAL));
            ctx.writer.writeU8(static_cast<uint8_t>(keys.size()));
            for (size_t i = 0; i < keys.size(); ++i) {
                QoreStringValueHelper key(keys[i]);
                ctx.writer.writeStringRef(key->c_str());
                if (!classifyAndWriteExpr(ctx.writer, vals[i], ctx.parent_locals,
                        ctx.parent_globals, ctx.const_reverse_map)) {
                    return false;
                }
            }
            return true;
        }
    }
    return false;
}

static QoreValue read_expr_hash_literal(AOTExprReadCtx& ctx) {
    uint8_t num_pairs = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreParseHashNode* phn = new QoreParseHashNode(&loc_builtin);
    for (uint8_t j = 0; j < num_pairs; ++j) {
        const char* key_str = ctx.reader.readStringRef(ctx.ptr);
        std::string val_err;
        QoreValue val = readOneExpr(ctx.reader, ctx.ptr, ctx.end, val_err, ctx.pgm,
            ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
        if (!val_err.empty()) {
            ctx.error = val_err;
            val.discard(nullptr);
            phn->deref(nullptr);
            return QoreValue();
        }
        phn->add(new QoreStringNode(key_str ? key_str : ""), val, &loc_builtin);
    }
    return QoreValue(phn);
}

// ============================================================================
// PARSE_HASH (44)
// ============================================================================

static bool write_expr_parse_hash(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* phn = dynamic_cast<const QoreParseHashNode*>(node);
    if (!phn) {
        return false;
    }

    const QoreParseHashNode::nvec_t& keys = phn->getKeys();
    const QoreParseHashNode::nvec_t& vals = phn->getValues();
    if (keys.size() > 255 || keys.size() != vals.size()) {
        return false;
    }

    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::PARSE_HASH));
    ctx.writer.writeU8(static_cast<uint8_t>(keys.size()));
    for (size_t i = 0; i < keys.size(); ++i) {
        if (!classifyAndWriteExpr(ctx.writer, keys[i], ctx.parent_locals,
                ctx.parent_globals, ctx.const_reverse_map)
                || !classifyAndWriteExpr(ctx.writer, vals[i], ctx.parent_locals,
                ctx.parent_globals, ctx.const_reverse_map)) {
            return false;
        }
    }
    return true;
}

static QoreValue read_expr_parse_hash(AOTExprReadCtx& ctx) {
    uint8_t num_pairs = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreParseHashNode* phn = new QoreParseHashNode(&loc_builtin);
    for (uint8_t j = 0; j < num_pairs; ++j) {
        std::string key_err;
        QoreValue key = readOneExpr(ctx.reader, ctx.ptr, ctx.end, key_err, ctx.pgm,
            ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
        std::string val_err;
        QoreValue val = readOneExpr(ctx.reader, ctx.ptr, ctx.end, val_err, ctx.pgm,
            ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
        if (!key_err.empty() || !val_err.empty()) {
            key.discard(nullptr);
            val.discard(nullptr);
            phn->deref(nullptr);
            ctx.error = !key_err.empty() ? key_err : val_err;
            return QoreValue();
        }
        phn->add(key, val, &loc_builtin);
    }
    return QoreValue(phn);
}

// ============================================================================
// HASH_DEREF (22)
// ============================================================================

static bool write_expr_hash_deref(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* hd = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASH_DEREF));
        return classifyAndWriteExpr(ctx.writer, hd->getLeft(), ctx.parent_locals,
                ctx.parent_globals, ctx.const_reverse_map)
            && classifyAndWriteExpr(ctx.writer, hd->getRight(), ctx.parent_locals,
                ctx.parent_globals, ctx.const_reverse_map);
    }
    return false;
}

static QoreValue read_expr_hash_deref(AOTExprReadCtx& ctx) {
    std::string left_err;
    QoreValue left = readOneExpr(ctx.reader, ctx.ptr, ctx.end, left_err, ctx.pgm, ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!left_err.empty()) {
        ctx.error = left_err;
        return QoreValue();
    }
    std::string right_err;
    QoreValue right = readOneExpr(ctx.reader, ctx.ptr, ctx.end, right_err, ctx.pgm, ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!right_err.empty()) {
        ctx.error = right_err;
        left.discard(nullptr);
        return QoreValue();
    }
    return QoreValue(new QoreHashObjectDereferenceOperatorNode(&loc_builtin, left, right));
}

// ============================================================================
// PARSE_REF (23)
// ============================================================================

static bool write_expr_parse_ref(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* prn = dynamic_cast<const ParseReferenceNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::PARSE_REF));
        ctx.writer.writeStringRef(prn->getTypeInfo() ? QoreTypeInfo::getPath(prn->getTypeInfo()) : "");
        return classifyAndWriteExpr(ctx.writer, prn->getLVExp(), ctx.parent_locals, ctx.parent_globals,
            ctx.const_reverse_map);
    }
    return false;
}

static void mark_parse_ref_root_thread_safe(QoreValue n) {
    while (n.hasNode()) {
        qore_type_t ntype = n.getType();
        if (ntype == NT_VARREF) {
            VarRefNode* vr = n.get<VarRefNode>();
            if (vr->getType() == VT_LOCAL && vr->ref.id) {
                vr->setThreadSafe();
            }
            return;
        }
        if (ntype == NT_SELF_VARREF || ntype == NT_CLASS_VARREF || ntype != NT_OPERATOR) {
            return;
        }
        auto* sq = dynamic_cast<QoreSquareBracketsOperatorNode*>(n.getInternalNode());
        if (sq) {
            n = sq->getLeft();
            continue;
        }
        auto* hd = dynamic_cast<QoreHashObjectDereferenceOperatorNode*>(n.getInternalNode());
        if (hd) {
            n = hd->getLeft();
            continue;
        }
        return;
    }
}

static QoreValue read_expr_parse_ref(AOTExprReadCtx& ctx) {
    const char* type_path = nullptr;
    if ((ctx.reader.getHeader().feature_flags & QORE_AOT_FEAT_PARSE_REF_TYPE) != 0) {
        type_path = ctx.reader.readStringRef(ctx.ptr);
    }
    std::string inner_err;
    QoreValue inner = readOneExpr(ctx.reader, ctx.ptr, ctx.end, inner_err, ctx.pgm, ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!inner_err.empty()) {
        ctx.error = inner_err;
        return QoreValue();
    }
    const QoreTypeInfo* ref_ti = referenceTypeInfo;
    if (type_path && *type_path) {
        std::string type_error;
        QoreAOTTypeResolver type_resolver(ctx.pgm);
        ref_ti = type_resolver.resolve(type_path, type_error);
        if (!ref_ti) {
            inner.discard(nullptr);
            ctx.error = "cannot resolve parse-reference type '" + std::string(type_path) + "': " + type_error;
            return QoreValue();
        }
        if (!QoreTypeInfo::isReference(ref_ti)) {
            inner.discard(nullptr);
            ctx.error = "resolved parse-reference type '" + std::string(type_path)
                + "' is not a reference type";
            return QoreValue();
        }
    }
    // Keep this reader aligned with the source parser and the other AOT
    // expression readers: \local{key} must resolve through the closure-var
    // reference chain when passed as reference<auto> recursively.
    mark_parse_ref_root_thread_safe(inner);
    return QoreValue(new ParseReferenceNode(&loc_builtin, inner, ref_ti));
}

// ============================================================================
// CAST_HASHDECL (24)
// ============================================================================

static bool write_expr_cast_inner(AOTExprWriteCtx& ctx, QoreValue inner) {
    ctx.writer.writeU8(1);
    return ::classifyAndWriteExpr(ctx.writer, inner,
        ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map);
}

static bool read_expr_cast_inner(AOTExprReadCtx& ctx, QoreValue& inner);

static const char* get_expr_cast_type_path(const QoreTypeInfo* ti) {
    if (ti == autoNoNarrowTypeInfo) {
        return "auto!";
    }
    if (ti == autoNoNarrowHashTypeInfo) {
        return "hash<auto!>";
    }
    if (ti == autoNoNarrowHashOrNothingTypeInfo) {
        return "*hash<auto!>";
    }
    if (ti == autoNoNarrowListTypeInfo) {
        return "list<auto!>";
    }
    if (ti == autoNoNarrowListOrNothingTypeInfo) {
        return "*list<auto!>";
    }
    return QoreTypeInfo::getPath(ti);
}

// ============================================================================
// CAST_SCALAR (96)
// ============================================================================

static bool write_expr_cast_scalar(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* sc = dynamic_cast<const QoreScalarCastOperatorNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_SCALAR));
        ctx.writer.writeStringRef(get_expr_cast_type_path(sc->getCastTypeInfo()));
        ctx.writer.writeU8(sc->isOrNothing() ? 1 : 0);
        return write_expr_cast_inner(ctx, sc->getExp());
    }
    return false;
}

static QoreValue read_expr_cast_scalar(AOTExprReadCtx& ctx) {
    const char* type_path = ctx.reader.readStringRef(ctx.ptr);
    uint8_t or_nothing = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreValue inner;
    if (!read_expr_cast_inner(ctx, inner)) {
        return QoreValue();
    }
    if (!type_path || !*type_path) {
        inner.discard(nullptr);
        return QoreValue();
    }
    std::string type_error;
    QoreAOTTypeResolver type_resolver(ctx.pgm);
    const QoreTypeInfo* ti = type_resolver.resolve(type_path, type_error);
    if (!ti || !QoreScalarCastOperatorNode::isSupportedCastType(ti)) {
        inner.discard(nullptr);
        return QoreValue();
    }
    return QoreValue(new QoreScalarCastOperatorNode(&loc_builtin, ti, inner, or_nothing != 0));
}

static bool read_expr_cast_inner(AOTExprReadCtx& ctx, QoreValue& inner) {
    uint8_t has_inner = QoreAOTBinaryReader::readU8(ctx.ptr);
    if (!has_inner) {
        return true;
    }
    std::string inner_err;
    inner = readOneExpr(ctx.reader, ctx.ptr, ctx.end, inner_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!inner_err.empty()) {
        ctx.error = inner_err;
        inner.discard(nullptr);
        inner = QoreValue();
        return false;
    }
    return true;
}

static bool write_expr_cast_hashdecl(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* hdc = dynamic_cast<const QoreHashDeclCastOperatorNode*>(node)) {
        const TypedHashDecl* hd = QoreTypeInfo::getUniqueReturnHashDecl(hdc->getCastTypeInfo());
        if (hd || hdc->getCastTypeInfo() == hashTypeInfo) {
            ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_HASHDECL));
            ctx.writer.writeStringRef(hd ? hd->getNamespacePath().c_str() : "hash");
            ctx.writer.writeU8(hdc->isOrNothing() ? 1 : 0);
            return write_expr_cast_inner(ctx, hdc->getExp());
        }
    }
    return false;
}

static QoreValue read_expr_cast_hashdecl(AOTExprReadCtx& ctx) {
    const char* hashdecl_path = ctx.reader.readStringRef(ctx.ptr);
    uint8_t or_nothing = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreValue inner;
    if (!read_expr_cast_inner(ctx, inner)) {
        return QoreValue();
    }
    if (!hashdecl_path || !*hashdecl_path) {
        inner.discard(nullptr);
        return QoreValue();
    }
    // If no inner expression, use empty hash for the cast (common case: <HashdeclType>{})
    if (!inner.hasNode()) {
        inner = QoreValue(new QoreHashNode(autoTypeInfo));
    }
    if (!strcmp(hashdecl_path, "hash")) {
        return QoreValue(new QoreHashDeclCastOperatorNode(&loc_builtin, nullptr, inner, or_nothing != 0));
    }
    qore_program_private* pp = qore_program_private::get(*ctx.pgm);
    const qore_ns_private* found_ns = nullptr;
    const TypedHashDecl* hd = qore_root_ns_private::runtimeFindHashDecl(
        *pp->RootNS, hashdecl_path, found_ns);
    if (!hd) {
        inner.discard(nullptr);
        return QoreValue();
    }
    auto* node = new QoreHashDeclCastOperatorNode(&loc_builtin, hd, inner, or_nothing != 0);
    return QoreValue(node);
}

// ============================================================================
// CAST_COMPLEX_HASH (25)
// ============================================================================

static bool write_expr_cast_complex_hash(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* chc = dynamic_cast<const QoreComplexHashCastOperatorNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_COMPLEX_HASH));
        ctx.writer.writeStringRef(QoreTypeInfo::getPath(chc->getCastTypeInfo()));
        ctx.writer.writeU8(chc->isOrNothing() ? 1 : 0);
        return write_expr_cast_inner(ctx, chc->getExp());
    }
    return false;
}

static QoreValue read_expr_cast_complex_hash(AOTExprReadCtx& ctx) {
    const char* type_path = ctx.reader.readStringRef(ctx.ptr);
    uint8_t or_nothing = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreValue inner;
    if (!read_expr_cast_inner(ctx, inner)) {
        return QoreValue();
    }
    if (!type_path || !*type_path) {
        inner.discard(nullptr);
        return QoreValue();
    }
    std::string type_error;
    QoreAOTTypeResolver type_resolver(ctx.pgm);
    const QoreTypeInfo* ti = type_resolver.resolve(type_path, type_error);
    if (!ti) {
        inner.discard(nullptr);
        return QoreValue();
    }
    auto* node = new QoreComplexHashCastOperatorNode(&loc_builtin, ti, inner, or_nothing != 0);
    return QoreValue(node);
}

// ============================================================================
// CAST_COMPLEX_LIST (26)
// ============================================================================

static bool write_expr_cast_complex_list(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* clc = dynamic_cast<const QoreComplexListCastOperatorNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_COMPLEX_LIST));
        const QoreTypeInfo* ti = clc->getCastTypeInfo();
        ctx.writer.writeStringRef(ti ? QoreTypeInfo::getPath(ti) : "list");
        ctx.writer.writeU8(clc->isOrNothing() ? 1 : 0);
        return write_expr_cast_inner(ctx, clc->getExp());
    }
    return false;
}

static QoreValue read_expr_cast_complex_list(AOTExprReadCtx& ctx) {
    const char* type_path = ctx.reader.readStringRef(ctx.ptr);
    uint8_t or_nothing = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreValue inner;
    if (!read_expr_cast_inner(ctx, inner)) {
        return QoreValue();
    }
    if (!type_path || !*type_path) {
        inner.discard(nullptr);
        return QoreValue();
    }
    if (!strcmp(type_path, "list")) {
        return QoreValue(new QoreComplexListCastOperatorNode(&loc_builtin, nullptr, inner, or_nothing != 0));
    }
    std::string type_error;
    QoreAOTTypeResolver type_resolver(ctx.pgm);
    const QoreTypeInfo* ti = type_resolver.resolve(type_path, type_error);
    if (!ti) {
        inner.discard(nullptr);
        return QoreValue();
    }
    auto* node = new QoreComplexListCastOperatorNode(&loc_builtin, ti, inner, or_nothing != 0);
    return QoreValue(node);
}

// ============================================================================
// CAST_CLASS (27)
// ============================================================================

static bool write_expr_cast_class(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* cc = dynamic_cast<const QoreClassCastOperatorNode*>(node)) {
        const QoreClass* qc = QoreTypeInfo::getUniqueReturnClass(cc->getCastTypeInfo());
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_CLASS));
        // A null class pointer represents the generic cast<object>(...) case.
        std::string class_ref = qc ? qore_aot_encode_class_ref(qc) : "object";
        ctx.writer.writeStringRef(class_ref.c_str());
        ctx.writer.writeU8(cc->isOrNothing() ? 1 : 0);
        return write_expr_cast_inner(ctx, cc->getExp());
    }
    return false;
}

static QoreValue read_expr_cast_class(AOTExprReadCtx& ctx) {
    const char* class_path = ctx.reader.readStringRef(ctx.ptr);
    uint8_t or_nothing = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreValue inner;
    if (!read_expr_cast_inner(ctx, inner)) {
        return QoreValue();
    }
    if (!class_path || !*class_path) {
        inner.discard(nullptr);
        return QoreValue();
    }
    if (!strcmp(class_path, "object")) {
        return QoreValue(new QoreClassCastOperatorNode(&loc_builtin, nullptr, inner, or_nothing != 0));
    }
    const QoreClass* qc = qore_aot_resolve_class_ref(ctx.pgm, class_path, false);
    if (!qc) {
        inner.discard(nullptr);
        return QoreValue();
    }
    auto* node = new QoreClassCastOperatorNode(&loc_builtin, qc, inner, or_nothing != 0);
    return QoreValue(node);
}

// ============================================================================
// CAST_ENUM (28)
// ============================================================================

static bool write_expr_cast_enum(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* ec = dynamic_cast<const QoreEnumCastOperatorNode*>(node)) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CAST_ENUM));
        ctx.writer.writeStringRef(QoreTypeInfo::getPath(ec->getCastTypeInfo()));
        ctx.writer.writeU8(ec->isOrNothing() ? 1 : 0);
        return write_expr_cast_inner(ctx, ec->getExp());
    }
    return false;
}

static QoreValue read_expr_cast_enum(AOTExprReadCtx& ctx) {
    const char* enum_path = ctx.reader.readStringRef(ctx.ptr);
    uint8_t or_nothing = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreValue inner;
    if (!read_expr_cast_inner(ctx, inner)) {
        return QoreValue();
    }
    if (!enum_path || !*enum_path) {
        inner.discard(nullptr);
        return QoreValue();
    }
    std::string type_error;
    QoreAOTTypeResolver type_resolver(ctx.pgm);
    const QoreTypeInfo* ti = type_resolver.resolve(enum_path, type_error);
    if (!ti) {
        inner.discard(nullptr);
        return QoreValue();
    }
    const QoreEnumDecl* ed = QoreTypeInfo::getUniqueReturnEnum(ti);
    if (!ed) {
        inner.discard(nullptr);
        return QoreValue();
    }
    auto* node = new QoreEnumCastOperatorNode(&loc_builtin, ed, ti, inner, or_nothing != 0);
    return QoreValue(node);
}

// ============================================================================
// CONST_INT (29)
// ============================================================================

static bool write_expr_const_int(AOTExprWriteCtx& ctx) {
    if (!ctx.expr.hasNode() && ctx.expr.getType() == NT_INT) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_INT));
        ctx.writer.writeI64(ctx.expr.getAsBigInt());
        return true;
    }
    if (ctx.expr.hasNode()) {
        if (auto* i = dynamic_cast<const QoreBigIntNode*>(ctx.expr.getInternalNode())) {
            ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_INT));
            ctx.writer.writeI64(i->getValue());
            return true;
        }
    }
    return false;
}

static QoreValue read_expr_const_int(AOTExprReadCtx& ctx) {
    return QoreValue(QoreAOTBinaryReader::readI64(ctx.ptr));
}

// ============================================================================
// CONST_FLOAT (30)
// ============================================================================

static bool write_expr_const_float(AOTExprWriteCtx& ctx) {
    if (!ctx.expr.hasNode() && ctx.expr.getType() == NT_FLOAT) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_FLOAT));
        ctx.writer.writeF64(ctx.expr.getAsFloat());
        return true;
    }
    if (ctx.expr.hasNode()) {
        if (auto* f = dynamic_cast<const QoreBigFloatNode*>(ctx.expr.getInternalNode())) {
            ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_FLOAT));
            ctx.writer.writeF64(f->getValue());
            return true;
        }
    }
    return false;
}

static QoreValue read_expr_const_float(AOTExprReadCtx& ctx) {
    return QoreValue(QoreAOTBinaryReader::readF64(ctx.ptr));
}

// ============================================================================
// CONST_BOOL (31)
// ============================================================================

static bool write_expr_const_bool(AOTExprWriteCtx& ctx) {
    if (!ctx.expr.hasNode() && ctx.expr.getType() == NT_BOOLEAN) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_BOOL));
        ctx.writer.writeU8(ctx.expr.getAsBool() ? 1 : 0);
        return true;
    }
    return false;
}

static QoreValue read_expr_const_bool(AOTExprReadCtx& ctx) {
    return QoreValue((bool)QoreAOTBinaryReader::readU8(ctx.ptr));
}

// ============================================================================
// CONST_NOTHING (32)
// ============================================================================

static bool write_expr_const_nothing(AOTExprWriteCtx& ctx) {
    if (!ctx.expr.hasNode() && ctx.expr.getType() == NT_NOTHING) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_NOTHING));
        return true;
    }
    if (ctx.expr.hasNode() && dynamic_cast<const QoreNothingNode*>(ctx.expr.getInternalNode())) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_NOTHING));
        return true;
    }
    return false;
}

static QoreValue read_expr_const_nothing(AOTExprReadCtx& ctx) {
    (void)ctx;
    return QoreValue();
}

// ============================================================================
// CONST_NULL (34)
// ============================================================================

static bool write_expr_const_null(AOTExprWriteCtx& ctx) {
    if ((!ctx.expr.hasNode() && ctx.expr.getType() == NT_NULL)
            || (ctx.expr.hasNode() && dynamic_cast<const QoreNullNode*>(ctx.expr.getInternalNode()))) {
        ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_NULL));
        return true;
    }
    return false;
}

static QoreValue read_expr_const_null(AOTExprReadCtx& ctx) {
    (void)ctx;
    return QoreValue(null());
}

// ============================================================================
// LIST_LITERAL (33)
// ============================================================================

static bool write_expr_list_literal(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (auto* qln = dynamic_cast<const QoreListNode*>(node)) {
        if (qln->size() <= 255) {
            ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::LIST_LITERAL));
            ctx.writer.writeU8(static_cast<uint8_t>(qln->size()));
            for (size_t i = 0; i < qln->size(); ++i) {
                if (!classifyAndWriteExpr(ctx.writer, qln->retrieveEntry(i),
                        ctx.parent_locals, ctx.parent_globals, ctx.const_reverse_map)) {
                    return false;
                }
            }
            return true;
        }
    }
    if (auto* pln = dynamic_cast<const QoreParseListNode*>(node)) {
        if (pln->size() <= 255) {
            ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::LIST_LITERAL));
            ctx.writer.writeU8(static_cast<uint8_t>(pln->size()));
            for (size_t i = 0; i < pln->size(); ++i) {
                if (!classifyAndWriteExpr(ctx.writer, pln->get(i), ctx.parent_locals,
                        ctx.parent_globals, ctx.const_reverse_map)) {
                    return false;
                }
            }
            return true;
        }
    }
    return false;
}

static QoreValue read_expr_list_literal(AOTExprReadCtx& ctx) {
    uint8_t count = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreParseListNode* pln = new QoreParseListNode(&loc_builtin);
    for (uint8_t j = 0; j < count; ++j) {
        std::string val_err;
        QoreValue val = readOneExpr(ctx.reader, ctx.ptr, ctx.end, val_err, ctx.pgm,
            ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
        if (!val_err.empty()) {
            ctx.error = val_err;
            val.discard(nullptr);
            pln->deref();
            return QoreValue();
        }
        pln->add(val, &loc_builtin);
    }
    return QoreValue(pln);
}

// ============================================================================
// DOT_EVAL_TARGET (35)
// ============================================================================

static bool write_expr_dot_eval_target(AOTExprWriteCtx& ctx) {
    // Written by classifyAndWriteExpr in QoreAOTBinary.cpp when encountering a
    // QoreDotEvalOperatorNode as an inline sub-expression. The writer there is
    // responsible for emitting the full payload (class/method/is_pseudo + target
    // expression + args); this entry is only here so the registry has a symbol.
    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::DOT_EVAL_TARGET));
    return true;
}

static QoreValue read_expr_dot_eval_target(AOTExprReadCtx& ctx) {
    // Reconstruct an inline QoreDotEvalOperatorNode from class_path + method_name
    // + is_pseudo + target expression + args list. The write side is in
    // classifyAndWriteExpr in QoreAOTBinary.cpp.
    const char* class_path = ctx.reader.readStringRef(ctx.ptr);
    const char* method_name_ref = ctx.reader.readStringRef(ctx.ptr);
    std::string method_name_storage;
    const char* method_name = method_name_ref;
    if (method_name_ref) {
        const char* sig_sep = strchr(method_name_ref, '\n');
        if (sig_sep) {
            method_name_storage.assign(method_name_ref, sig_sep - method_name_ref);
            method_name = method_name_storage.c_str();
        }
    }
    uint8_t is_pseudo = QoreAOTBinaryReader::readU8(ctx.ptr);

    // Target expression
    std::string target_err;
    QoreValue target = readOneExpr(ctx.reader, ctx.ptr, ctx.end, target_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!target_err.empty()) {
        ctx.error = "DOT_EVAL_TARGET target: " + target_err;
        target.discard(nullptr);
        return QoreValue();
    }

    // Args list
    uint8_t num_args = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreParseListNode* pln = nullptr;
    if (num_args > 0) {
        pln = new QoreParseListNode(&loc_builtin);
        for (uint8_t j = 0; j < num_args; ++j) {
            std::string arg_err;
            QoreValue arg = readOneExpr(ctx.reader, ctx.ptr, ctx.end, arg_err, ctx.pgm,
                ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
            if (!arg_err.empty()) {
                ctx.error = "DOT_EVAL_TARGET arg " + std::to_string(j) + ": " + arg_err;
                arg.discard(nullptr);
                pln->deref();
                target.discard(nullptr);
                return QoreValue();
            }
            pln->add(arg, &loc_builtin);
        }
    }

    if (!method_name || !*method_name) {
        ctx.error = "DOT_EVAL_TARGET: empty method name";
        if (pln) {
            pln->deref();
        }
        target.discard(nullptr);
        return QoreValue();
    }

    // Build MethodCallNode with parse_args list; call resolveParseArgs() to
    // populate the evaluated args list so AbstractMethodCallNode::exec can find
    // them at runtime (parseInit is never re-run for AOT-deserialized nodes).
    MethodCallNode* mc = new MethodCallNode(&loc_builtin, strdup(method_name), pln);

    // Try to resolve class + method for optimized dispatch. For regular
    // (non-pseudo) calls these are pure optimizations: if left null,
    // AbstractMethodCallNode::exec() falls back to dynamic method lookup on
    // the target object's runtime class, and QoreDotEvalOperatorNode::
    // evalWithBase() falls back to pseudo_classes_eval() for non-object base
    // types.  For pseudo calls, we only tag the node as pseudo when we can
    // resolve both class and method — otherwise the dynamic
    // pseudo_classes_eval() path handles it correctly without needing
    // the pseudo flag set.
    const QoreClass* resolved_qc = nullptr;
    const QoreMethod* resolved_m = nullptr;
    if (class_path && *class_path) {
        resolved_qc = qore_aot_resolve_class_ref(ctx.pgm, class_path, is_pseudo != 0);
        if (resolved_qc) {
            resolved_m = resolved_qc->findMethod(method_name);
            if (!resolved_m) {
                resolved_m = resolved_qc->findStaticMethod(method_name);
            }
        }
    }
    if (resolved_qc && resolved_m) {
        mc->parseSetClassAndMethod(resolved_qc, resolved_m);
        if (is_pseudo) {
            mc->setPseudo(resolved_qc->getTypeInfo());
        }
    }
    mc->resolveParseArgs();

    return QoreValue(new QoreDotEvalOperatorNode(&loc_builtin, target, mc));
}

// ============================================================================
// DOT_EVAL_EXPR (103)
// ============================================================================

static bool write_expr_dot_eval_expr(AOTExprWriteCtx& ctx) {
    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::DOT_EVAL_EXPR));
    return classifyAndWriteExpr(ctx.writer, ctx.expr, ctx.parent_locals,
        ctx.parent_globals, ctx.const_reverse_map);
}

static QoreValue read_expr_dot_eval_expr(AOTExprReadCtx& ctx) {
    return readOneExpr(ctx.reader, ctx.ptr, ctx.end, ctx.error, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
}

// ============================================================================
// CONST_VALUE (41)
// ============================================================================

static bool write_expr_const_value(AOTExprWriteCtx& ctx) {
    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONST_VALUE));
    return ctx.writer.writeValue(ctx.expr);
}

static QoreValue read_expr_const_value(AOTExprReadCtx& ctx) {
    std::string value_error;
    QoreValue rv = ctx.reader.readValue(ctx.ptr, ctx.end, value_error);
    if (!value_error.empty()) {
        ctx.error = value_error;
    }
    return rv;
}

// ============================================================================
// PLUS (42)
// ============================================================================

static bool write_expr_plus(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* op = dynamic_cast<const QorePlusOperatorNode*>(node);
    if (!op) {
        return false;
    }

    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::PLUS));
    return classifyAndWriteExpr(ctx.writer, op->getLeft(), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->getRight(), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map);
}

static QoreValue read_expr_plus(AOTExprReadCtx& ctx) {
    std::string left_err;
    QoreValue left = readOneExpr(ctx.reader, ctx.ptr, ctx.end, left_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    std::string right_err;
    QoreValue right = readOneExpr(ctx.reader, ctx.ptr, ctx.end, right_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!left_err.empty() || !right_err.empty()) {
        left.discard(nullptr);
        right.discard(nullptr);
        ctx.error = !left_err.empty() ? left_err : right_err;
        return QoreValue();
    }
    return QoreValue(new QorePlusOperatorNode(&loc_builtin, left, right));
}

// ============================================================================
// SQUARE_BRACKET (43)
// ============================================================================

static bool write_expr_square_bracket(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* op = dynamic_cast<const QoreSquareBracketsOperatorNode*>(node);
    if (!op) {
        return false;
    }

    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::SQUARE_BRACKET));
    return classifyAndWriteExpr(ctx.writer, op->getLeft(), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->getRight(), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map);
}

static QoreValue read_expr_square_bracket(AOTExprReadCtx& ctx) {
    std::string left_err;
    QoreValue left = readOneExpr(ctx.reader, ctx.ptr, ctx.end, left_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    std::string right_err;
    QoreValue right = readOneExpr(ctx.reader, ctx.ptr, ctx.end, right_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!left_err.empty() || !right_err.empty()) {
        left.discard(nullptr);
        right.discard(nullptr);
        ctx.error = !left_err.empty() ? left_err : right_err;
        return QoreValue();
    }
    return QoreValue(new QoreSquareBracketsOperatorNode(&loc_builtin, left, right));
}

static bool write_expr_square_bracket_range(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* op = dynamic_cast<const QoreSquareBracketsRangeOperatorNode*>(node);
    if (!op) {
        return false;
    }

    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::SQUARE_BRACKET_RANGE));
    return classifyAndWriteExpr(ctx.writer, op->get(0), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->get(1), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->get(2), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map);
}

static QoreValue read_expr_square_bracket_range(AOTExprReadCtx& ctx) {
    std::string src_err;
    QoreValue src = readOneExpr(ctx.reader, ctx.ptr, ctx.end, src_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    std::string start_err;
    QoreValue start = readOneExpr(ctx.reader, ctx.ptr, ctx.end, start_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    std::string stop_err;
    QoreValue stop = readOneExpr(ctx.reader, ctx.ptr, ctx.end, stop_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!src_err.empty() || !start_err.empty() || !stop_err.empty()) {
        src.discard(nullptr);
        start.discard(nullptr);
        stop.discard(nullptr);
        ctx.error = !src_err.empty() ? src_err : (!start_err.empty() ? start_err : stop_err);
        return QoreValue();
    }
    return QoreValue(new QoreSquareBracketsRangeOperatorNode(&loc_builtin, src, start, stop));
}

// ============================================================================
// EXISTS (45)
// ============================================================================

static bool write_expr_exists(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* op = dynamic_cast<const QoreExistsOperatorNode*>(node);
    if (!op) {
        return false;
    }

    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::EXISTS));
    return classifyAndWriteExpr(ctx.writer, op->getExp(), ctx.parent_locals,
        ctx.parent_globals, ctx.const_reverse_map);
}

static QoreValue read_expr_exists(AOTExprReadCtx& ctx) {
    std::string operand_err;
    QoreValue operand = readOneExpr(ctx.reader, ctx.ptr, ctx.end, operand_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!operand_err.empty()) {
        operand.discard(nullptr);
        ctx.error = operand_err;
        return QoreValue();
    }
    return QoreValue(new QoreExistsOperatorNode(&loc_builtin, operand));
}

// ============================================================================
// IMPLICIT_ARG (46)
// ============================================================================

static bool write_expr_implicit_arg(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* ia = dynamic_cast<const QoreImplicitArgumentNode*>(node);
    if (!ia) {
        return false;
    }

    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::IMPLICIT_ARG));
    ctx.writer.writeI64(static_cast<int64_t>(ia->getOffset()));
    return true;
}

static QoreValue read_expr_implicit_arg(AOTExprReadCtx& ctx) {
    int64_t offset = QoreAOTBinaryReader::readI64(ctx.ptr);
    int ctor_offset = offset >= 0 ? static_cast<int>(offset + 1) : static_cast<int>(offset);
    return QoreValue(new QoreImplicitArgumentNode(&loc_builtin, ctor_offset));
}

// ============================================================================
// MINUS (47)
// ============================================================================

static bool write_expr_minus(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* op = dynamic_cast<const QoreMinusOperatorNode*>(node);
    if (!op) {
        return false;
    }

    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::MINUS));
    return classifyAndWriteExpr(ctx.writer, op->getLeft(), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->getRight(), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map);
}

static QoreValue read_expr_minus(AOTExprReadCtx& ctx) {
    std::string left_err;
    QoreValue left = readOneExpr(ctx.reader, ctx.ptr, ctx.end, left_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    std::string right_err;
    QoreValue right = readOneExpr(ctx.reader, ctx.ptr, ctx.end, right_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!left_err.empty() || !right_err.empty()) {
        left.discard(nullptr);
        right.discard(nullptr);
        ctx.error = !left_err.empty() ? left_err : right_err;
        return QoreValue();
    }
    return QoreValue(new QoreMinusOperatorNode(&loc_builtin, left, right));
}

// ============================================================================
// MULTIPLY/DIVIDE/MODULO (49-51)
// ============================================================================

static bool write_expr_multiply(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* op = dynamic_cast<const QoreMultiplicationOperatorNode*>(node);
    if (!op) {
        return false;
    }

    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::MULTIPLY));
    return classifyAndWriteExpr(ctx.writer, op->getLeft(), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->getRight(), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map);
}

static QoreValue read_expr_multiply(AOTExprReadCtx& ctx) {
    std::string left_err;
    QoreValue left = readOneExpr(ctx.reader, ctx.ptr, ctx.end, left_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    std::string right_err;
    QoreValue right = readOneExpr(ctx.reader, ctx.ptr, ctx.end, right_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!left_err.empty() || !right_err.empty()) {
        left.discard(nullptr);
        right.discard(nullptr);
        ctx.error = !left_err.empty() ? left_err : right_err;
        return QoreValue();
    }
    return QoreValue(new QoreMultiplicationOperatorNode(&loc_builtin, left, right));
}

static bool write_expr_divide(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* op = dynamic_cast<const QoreDivisionOperatorNode*>(node);
    if (!op) {
        return false;
    }

    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::DIVIDE));
    return classifyAndWriteExpr(ctx.writer, op->getLeft(), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->getRight(), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map);
}

static QoreValue read_expr_divide(AOTExprReadCtx& ctx) {
    std::string left_err;
    QoreValue left = readOneExpr(ctx.reader, ctx.ptr, ctx.end, left_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    std::string right_err;
    QoreValue right = readOneExpr(ctx.reader, ctx.ptr, ctx.end, right_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!left_err.empty() || !right_err.empty()) {
        left.discard(nullptr);
        right.discard(nullptr);
        ctx.error = !left_err.empty() ? left_err : right_err;
        return QoreValue();
    }
    return QoreValue(new QoreDivisionOperatorNode(&loc_builtin, left, right));
}

static bool write_expr_modulo(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* op = dynamic_cast<const QoreModuloOperatorNode*>(node);
    if (!op) {
        return false;
    }

    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::MODULO));
    return classifyAndWriteExpr(ctx.writer, op->getLeft(), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->getRight(), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map);
}

static QoreValue read_expr_modulo(AOTExprReadCtx& ctx) {
    std::string left_err;
    QoreValue left = readOneExpr(ctx.reader, ctx.ptr, ctx.end, left_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    std::string right_err;
    QoreValue right = readOneExpr(ctx.reader, ctx.ptr, ctx.end, right_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!left_err.empty() || !right_err.empty()) {
        left.discard(nullptr);
        right.discard(nullptr);
        ctx.error = !left_err.empty() ? left_err : right_err;
        return QoreValue();
    }
    return QoreValue(new QoreModuloOperatorNode(&loc_builtin, left, right));
}

// ============================================================================
// KEYS (48)
// ============================================================================

static bool write_expr_keys(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* op = dynamic_cast<const QoreKeysOperatorNode*>(node);
    if (!op) {
        return false;
    }

    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::KEYS));
    return classifyAndWriteExpr(ctx.writer, op->getExp(), ctx.parent_locals,
        ctx.parent_globals, ctx.const_reverse_map);
}

static QoreValue read_expr_keys(AOTExprReadCtx& ctx) {
    std::string operand_err;
    QoreValue operand = readOneExpr(ctx.reader, ctx.ptr, ctx.end, operand_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!operand_err.empty()) {
        operand.discard(nullptr);
        ctx.error = operand_err;
        return QoreValue();
    }
    return QoreValue(new QoreKeysOperatorNode(&loc_builtin, operand));
}

// ============================================================================
// IMPLICIT_ELEM (52)
// ============================================================================

static bool write_expr_implicit_elem(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* ien = dynamic_cast<const QoreImplicitElementNode*>(node);
    if (!ien) {
        return false;
    }

    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::IMPLICIT_ELEM));
    return true;
}

static QoreValue read_expr_implicit_elem(AOTExprReadCtx& ctx) {
    (void)ctx;
    return QoreValue(new QoreImplicitElementNode(&loc_builtin));
}

// ============================================================================
// INSTANCEOF (53)
// ============================================================================

static bool write_expr_instanceof(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* inst = dynamic_cast<const QoreInstanceOfOperatorNode*>(node);
    if (!inst) {
        return false;
    }
    const QoreTypeInfo* ti = inst->getInstanceTypeInfo();
    const char* type_path = ti ? QoreTypeInfo::getPath(ti) : "";
    if (!type_path || !*type_path) {
        return false;
    }

    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::INSTANCEOF));
    ctx.writer.writeStringRef(type_path);
    return classifyAndWriteExpr(ctx.writer, inst->getExp(), ctx.parent_locals,
        ctx.parent_globals, ctx.const_reverse_map);
}

static QoreValue read_expr_instanceof(AOTExprReadCtx& ctx) {
    const char* type_path = ctx.reader.readStringRef(ctx.ptr);
    std::string operand_err;
    QoreValue operand = readOneExpr(ctx.reader, ctx.ptr, ctx.end, operand_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!operand_err.empty()) {
        operand.discard(nullptr);
        ctx.error = operand_err;
        return QoreValue();
    }
    if (!type_path || !*type_path) {
        operand.discard(nullptr);
        ctx.error = "missing INSTANCEOF type path";
        return QoreValue();
    }
    std::string type_error;
    QoreAOTTypeResolver type_resolver(ctx.pgm);
    const QoreTypeInfo* ti = type_resolver.resolve(type_path, type_error);
    if (!ti) {
        operand.discard(nullptr);
        ctx.error = "cannot resolve INSTANCEOF type '" + std::string(type_path)
            + "': " + type_error;
        return QoreValue();
    }
    return QoreValue(new QoreInstanceOfOperatorNode(&loc_builtin, operand, ti));
}

// ============================================================================
// REGEX_MATCH/REGEX_NMATCH/REGEX_EXTRACT (54-56)
// ============================================================================

static bool write_regex_match_payload(AOTExprWriteCtx& ctx, const QoreRegexMatchOperatorNode* op,
        AOTExprKind kind) {
    QoreRegex* re = op->getRegex();
    const char* pattern = re ? re->getPatternCStr() : nullptr;
    if (!pattern) {
        return false;
    }
    ctx.writer.writeU8(static_cast<uint8_t>(kind));
    ctx.writer.writeStringRef(pattern);
    ctx.writer.writeI64(re->getOptions());
    return classifyAndWriteExpr(ctx.writer, op->getExp(), ctx.parent_locals,
        ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_expr_regex_match(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* op = dynamic_cast<const QoreRegexMatchOperatorNode*>(node);
    if (!op || dynamic_cast<const QoreRegexNMatchOperatorNode*>(node)
            || dynamic_cast<const QoreRegexExtractOperatorNode*>(node)) {
        return false;
    }
    return write_regex_match_payload(ctx, op, AOTExprKind::REGEX_MATCH);
}

static bool write_expr_regex_nmatch(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* op = dynamic_cast<const QoreRegexNMatchOperatorNode*>(node);
    if (!op) {
        return false;
    }
    return write_regex_match_payload(ctx, op, AOTExprKind::REGEX_NMATCH);
}

static bool write_expr_regex_extract(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* op = dynamic_cast<const QoreRegexExtractOperatorNode*>(node);
    if (!op) {
        return false;
    }
    return write_regex_match_payload(ctx, op, AOTExprKind::REGEX_EXTRACT);
}

static QoreValue read_regex_match_payload(AOTExprReadCtx& ctx, AOTExprKind kind) {
    const char* pattern = ctx.reader.readStringRef(ctx.ptr);
    int64_t options = QoreAOTBinaryReader::readI64(ctx.ptr);
    std::string operand_err;
    QoreValue operand = readOneExpr(ctx.reader, ctx.ptr, ctx.end, operand_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!operand_err.empty()) {
        operand.discard(nullptr);
        ctx.error = operand_err;
        return QoreValue();
    }
    if (!pattern) {
        operand.discard(nullptr);
        ctx.error = "missing regex pattern";
        return QoreValue();
    }
    ExceptionSink xsink;
    QoreRegex* re = new QoreRegex(pattern, options, &xsink);
    if (xsink) {
        delete re;
        operand.discard(nullptr);
        ctx.error = "regex compile error for pattern '" + std::string(pattern) + "'";
        return QoreValue();
    }
    if (kind == AOTExprKind::REGEX_NMATCH) {
        return QoreValue(new QoreRegexNMatchOperatorNode(&loc_builtin, operand, re));
    }
    if (kind == AOTExprKind::REGEX_EXTRACT) {
        return QoreValue(new QoreRegexExtractOperatorNode(&loc_builtin, operand, re));
    }
    return QoreValue(new QoreRegexMatchOperatorNode(&loc_builtin, operand, re));
}

static QoreValue read_expr_regex_match(AOTExprReadCtx& ctx) {
    return read_regex_match_payload(ctx, AOTExprKind::REGEX_MATCH);
}

static QoreValue read_expr_regex_nmatch(AOTExprReadCtx& ctx) {
    return read_regex_match_payload(ctx, AOTExprKind::REGEX_NMATCH);
}

static QoreValue read_expr_regex_extract(AOTExprReadCtx& ctx) {
    return read_regex_match_payload(ctx, AOTExprKind::REGEX_EXTRACT);
}

// ============================================================================
// PRE_INC/PRE_DEC/POST_INC/POST_DEC (57-60)
// ============================================================================

static bool write_expr_pre_inc(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* op = dynamic_cast<const QorePreIncrementOperatorNode*>(node);
    if (!op || dynamic_cast<const QorePreDecrementOperatorNode*>(node)) {
        return false;
    }
    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::PRE_INC));
    return classifyAndWriteExpr(ctx.writer, op->getExp(), ctx.parent_locals,
        ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_expr_pre_dec(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* op = dynamic_cast<const QorePreDecrementOperatorNode*>(node);
    if (!op) {
        return false;
    }
    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::PRE_DEC));
    return classifyAndWriteExpr(ctx.writer, op->getExp(), ctx.parent_locals,
        ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_expr_post_inc(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (dynamic_cast<const QorePostDecrementOperatorNode*>(node)
            || dynamic_cast<const QoreIntPostDecrementOperatorNode*>(node)) {
        return false;
    }
    QoreValue operand;
    if (auto* op = dynamic_cast<const QoreIntPostIncrementOperatorNode*>(node)) {
        operand = op->getExp();
    } else if (auto* op = dynamic_cast<const QorePostIncrementOperatorNode*>(node)) {
        operand = op->getExp();
    } else {
        return false;
    }
    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::POST_INC));
    return classifyAndWriteExpr(ctx.writer, operand, ctx.parent_locals,
        ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_expr_post_dec(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    QoreValue operand;
    if (auto* op = dynamic_cast<const QoreIntPostDecrementOperatorNode*>(node)) {
        operand = op->getExp();
    } else if (auto* op = dynamic_cast<const QorePostDecrementOperatorNode*>(node)) {
        operand = op->getExp();
    } else {
        return false;
    }
    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::POST_DEC));
    return classifyAndWriteExpr(ctx.writer, operand, ctx.parent_locals,
        ctx.parent_globals, ctx.const_reverse_map);
}

static QoreValue read_expr_pre_inc(AOTExprReadCtx& ctx) {
    std::string operand_err;
    QoreValue operand = readOneExpr(ctx.reader, ctx.ptr, ctx.end, operand_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!operand_err.empty()) {
        operand.discard(nullptr);
        ctx.error = operand_err;
        return QoreValue();
    }
    return QoreValue(new QorePreIncrementOperatorNode(&loc_builtin, operand));
}

static QoreValue read_expr_pre_dec(AOTExprReadCtx& ctx) {
    std::string operand_err;
    QoreValue operand = readOneExpr(ctx.reader, ctx.ptr, ctx.end, operand_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!operand_err.empty()) {
        operand.discard(nullptr);
        ctx.error = operand_err;
        return QoreValue();
    }
    return QoreValue(new QorePreDecrementOperatorNode(&loc_builtin, operand));
}

static QoreValue read_expr_post_inc(AOTExprReadCtx& ctx) {
    std::string operand_err;
    QoreValue operand = readOneExpr(ctx.reader, ctx.ptr, ctx.end, operand_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!operand_err.empty()) {
        operand.discard(nullptr);
        ctx.error = operand_err;
        return QoreValue();
    }
    return QoreValue(new QorePostIncrementOperatorNode(&loc_builtin, operand));
}

static QoreValue read_expr_post_dec(AOTExprReadCtx& ctx) {
    std::string operand_err;
    QoreValue operand = readOneExpr(ctx.reader, ctx.ptr, ctx.end, operand_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!operand_err.empty()) {
        operand.discard(nullptr);
        ctx.error = operand_err;
        return QoreValue();
    }
    return QoreValue(new QorePostDecrementOperatorNode(&loc_builtin, operand));
}

// ============================================================================
// LOG_EQ/LOG_NE (61-62)
// ============================================================================

template <typename NodeT>
static bool write_binary_expr(AOTExprWriteCtx& ctx, AOTExprKind kind) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* op = dynamic_cast<const NodeT*>(node);
    if (!op) {
        return false;
    }
    ctx.writer.writeU8(static_cast<uint8_t>(kind));
    return classifyAndWriteExpr(ctx.writer, op->getLeft(), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->getRight(), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map);
}

template <typename NodeT>
static QoreValue read_binary_expr(AOTExprReadCtx& ctx) {
    std::string left_err;
    QoreValue left = readOneExpr(ctx.reader, ctx.ptr, ctx.end, left_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    std::string right_err;
    QoreValue right = readOneExpr(ctx.reader, ctx.ptr, ctx.end, right_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!left_err.empty() || !right_err.empty()) {
        left.discard(nullptr);
        right.discard(nullptr);
        ctx.error = !left_err.empty() ? left_err : right_err;
        return QoreValue();
    }
    return QoreValue(new NodeT(&loc_builtin, left, right));
}

static bool write_expr_bit_and(AOTExprWriteCtx& ctx) {
    return write_binary_expr<QoreBinaryAndOperatorNode>(ctx, AOTExprKind::BIT_AND);
}

static bool write_expr_bit_or(AOTExprWriteCtx& ctx) {
    return write_binary_expr<QoreBinaryOrOperatorNode>(ctx, AOTExprKind::BIT_OR);
}

static bool write_expr_bit_xor(AOTExprWriteCtx& ctx) {
    return write_binary_expr<QoreBinaryXorOperatorNode>(ctx, AOTExprKind::BIT_XOR);
}

static bool write_expr_shift_left(AOTExprWriteCtx& ctx) {
    return write_binary_expr<QoreShiftLeftOperatorNode>(ctx, AOTExprKind::SHIFT_LEFT);
}

static bool write_expr_shift_right(AOTExprWriteCtx& ctx) {
    return write_binary_expr<QoreShiftRightOperatorNode>(ctx, AOTExprKind::SHIFT_RIGHT);
}

static QoreValue read_expr_bit_and(AOTExprReadCtx& ctx) {
    return read_binary_expr<QoreBinaryAndOperatorNode>(ctx);
}

static QoreValue read_expr_bit_or(AOTExprReadCtx& ctx) {
    return read_binary_expr<QoreBinaryOrOperatorNode>(ctx);
}

static QoreValue read_expr_bit_xor(AOTExprReadCtx& ctx) {
    return read_binary_expr<QoreBinaryXorOperatorNode>(ctx);
}

static QoreValue read_expr_shift_left(AOTExprReadCtx& ctx) {
    return read_binary_expr<QoreShiftLeftOperatorNode>(ctx);
}

static QoreValue read_expr_shift_right(AOTExprReadCtx& ctx) {
    return read_binary_expr<QoreShiftRightOperatorNode>(ctx);
}

static bool write_expr_log_eq(AOTExprWriteCtx& ctx) {
    if (dynamic_cast<const QoreLogicalNotEqualsOperatorNode*>(ctx.expr.getInternalNode())) {
        return false;
    }
    return write_binary_expr<QoreLogicalEqualsOperatorNode>(ctx, AOTExprKind::LOG_EQ);
}

static bool write_expr_log_ne(AOTExprWriteCtx& ctx) {
    return write_binary_expr<QoreLogicalNotEqualsOperatorNode>(ctx, AOTExprKind::LOG_NE);
}

static QoreValue read_expr_log_eq(AOTExprReadCtx& ctx) {
    return read_binary_expr<QoreLogicalEqualsOperatorNode>(ctx);
}

static QoreValue read_expr_log_ne(AOTExprReadCtx& ctx) {
    return read_binary_expr<QoreLogicalNotEqualsOperatorNode>(ctx);
}

static bool write_expr_range(AOTExprWriteCtx& ctx) {
    return write_binary_expr<QoreRangeOperatorNode>(ctx, AOTExprKind::RANGE);
}

static QoreValue read_expr_range(AOTExprReadCtx& ctx) {
    return read_binary_expr<QoreRangeOperatorNode>(ctx);
}

static bool write_expr_assign(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    if (dynamic_cast<const QoreWeakAssignmentOperatorNode*>(node)) {
        return false;
    }
    return write_binary_expr<QoreAssignmentOperatorNode>(ctx, AOTExprKind::ASSIGN);
}

static QoreValue read_expr_assign(AOTExprReadCtx& ctx) {
    return read_binary_expr<QoreAssignmentOperatorNode>(ctx);
}

static bool write_expr_log_lt(AOTExprWriteCtx& ctx) {
    return write_binary_expr<QoreLogicalLessThanOperatorNode>(ctx, AOTExprKind::LOG_LT);
}

static bool write_expr_log_gt(AOTExprWriteCtx& ctx) {
    return write_binary_expr<QoreLogicalGreaterThanOperatorNode>(ctx, AOTExprKind::LOG_GT);
}

static bool write_expr_log_le(AOTExprWriteCtx& ctx) {
    return write_binary_expr<QoreLogicalLessThanOrEqualsOperatorNode>(ctx, AOTExprKind::LOG_LE);
}

static bool write_expr_log_ge(AOTExprWriteCtx& ctx) {
    return write_binary_expr<QoreLogicalGreaterThanOrEqualsOperatorNode>(ctx, AOTExprKind::LOG_GE);
}

static QoreValue read_expr_log_lt(AOTExprReadCtx& ctx) {
    return read_binary_expr<QoreLogicalLessThanOperatorNode>(ctx);
}

static QoreValue read_expr_log_gt(AOTExprReadCtx& ctx) {
    return read_binary_expr<QoreLogicalGreaterThanOperatorNode>(ctx);
}

static QoreValue read_expr_log_le(AOTExprReadCtx& ctx) {
    return read_binary_expr<QoreLogicalLessThanOrEqualsOperatorNode>(ctx);
}

static QoreValue read_expr_log_ge(AOTExprReadCtx& ctx) {
    return read_binary_expr<QoreLogicalGreaterThanOrEqualsOperatorNode>(ctx);
}

static bool write_expr_log_and(AOTExprWriteCtx& ctx) {
    if (dynamic_cast<const QoreLogicalOrOperatorNode*>(ctx.expr.getInternalNode())) {
        return false;
    }
    return write_binary_expr<QoreLogicalAndOperatorNode>(ctx, AOTExprKind::LOG_AND);
}

static bool write_expr_log_or(AOTExprWriteCtx& ctx) {
    return write_binary_expr<QoreLogicalOrOperatorNode>(ctx, AOTExprKind::LOG_OR);
}

static QoreValue read_expr_log_and(AOTExprReadCtx& ctx) {
    return read_binary_expr<QoreLogicalAndOperatorNode>(ctx);
}

static QoreValue read_expr_log_or(AOTExprReadCtx& ctx) {
    return read_binary_expr<QoreLogicalOrOperatorNode>(ctx);
}

// ============================================================================
// LOG_NOT (63)
// ============================================================================

static bool write_expr_log_not(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* op = dynamic_cast<const QoreLogicalNotOperatorNode*>(node);
    if (!op) {
        return false;
    }
    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::LOG_NOT));
    return classifyAndWriteExpr(ctx.writer, op->getExp(), ctx.parent_locals,
        ctx.parent_globals, ctx.const_reverse_map);
}

static QoreValue read_expr_log_not(AOTExprReadCtx& ctx) {
    std::string operand_err;
    QoreValue operand = readOneExpr(ctx.reader, ctx.ptr, ctx.end, operand_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!operand_err.empty()) {
        operand.discard(nullptr);
        ctx.error = operand_err;
        return QoreValue();
    }
    return QoreValue(new QoreLogicalNotOperatorNode(&loc_builtin, operand));
}

// ============================================================================
// NULL_COAL/VALUE_COAL (77-78)
// ============================================================================

static bool write_expr_null_coal(AOTExprWriteCtx& ctx) {
    return write_binary_expr<QoreNullCoalescingOperatorNode>(ctx, AOTExprKind::NULL_COAL);
}

static bool write_expr_value_coal(AOTExprWriteCtx& ctx) {
    return write_binary_expr<QoreValueCoalescingOperatorNode>(ctx, AOTExprKind::VALUE_COAL);
}

static QoreValue read_expr_null_coal(AOTExprReadCtx& ctx) {
    return read_binary_expr<QoreNullCoalescingOperatorNode>(ctx);
}

static QoreValue read_expr_value_coal(AOTExprReadCtx& ctx) {
    return read_binary_expr<QoreValueCoalescingOperatorNode>(ctx);
}

// ============================================================================
// QUESTION (79)
// ============================================================================

static bool write_expr_question(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* op = dynamic_cast<const QoreQuestionMarkOperatorNode*>(node);
    if (!op) {
        return false;
    }
    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::QUESTION));
    return classifyAndWriteExpr(ctx.writer, op->get(0), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->get(1), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->get(2), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map);
}

static QoreValue read_expr_question(AOTExprReadCtx& ctx) {
    std::string cond_err;
    QoreValue cond = readOneExpr(ctx.reader, ctx.ptr, ctx.end, cond_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    std::string true_err;
    QoreValue true_expr = readOneExpr(ctx.reader, ctx.ptr, ctx.end, true_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    std::string false_err;
    QoreValue false_expr = readOneExpr(ctx.reader, ctx.ptr, ctx.end, false_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!cond_err.empty() || !true_err.empty() || !false_err.empty()) {
        cond.discard(nullptr);
        true_expr.discard(nullptr);
        false_expr.discard(nullptr);
        ctx.error = !cond_err.empty() ? cond_err : (!true_err.empty() ? true_err : false_err);
        return QoreValue();
    }
    return QoreValue(new QoreQuestionMarkOperatorNode(&loc_builtin, cond, true_expr, false_expr));
}

// ============================================================================
// FOLDL/FOLDR (80-81)
// ============================================================================

static bool write_expr_foldl(AOTExprWriteCtx& ctx) {
    if (dynamic_cast<const QoreFoldrOperatorNode*>(ctx.expr.getInternalNode())) {
        return false;
    }
    return write_binary_expr<QoreFoldlOperatorNode>(ctx, AOTExprKind::FOLDL);
}

static bool write_expr_foldr(AOTExprWriteCtx& ctx) {
    return write_binary_expr<QoreFoldrOperatorNode>(ctx, AOTExprKind::FOLDR);
}

static QoreValue read_expr_foldl(AOTExprReadCtx& ctx) {
    return read_binary_expr<QoreFoldlOperatorNode>(ctx);
}

static QoreValue read_expr_foldr(AOTExprReadCtx& ctx) {
    return read_binary_expr<QoreFoldrOperatorNode>(ctx);
}

// ============================================================================
// MAP/MAP_SELECT/HASH_MAP/HASH_MAP_SELECT (82-85)
// ============================================================================

static bool write_expr_map(AOTExprWriteCtx& ctx) {
    return write_binary_expr<QoreMapOperatorNode>(ctx, AOTExprKind::MAP);
}

static bool write_expr_map_select(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* op = dynamic_cast<const QoreMapSelectOperatorNode*>(node);
    if (!op) {
        return false;
    }
    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::MAP_SELECT));
    return classifyAndWriteExpr(ctx.writer, op->get(0), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->get(1), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->get(2), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_expr_hash_map(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* op = dynamic_cast<const QoreHashMapOperatorNode*>(node);
    if (!op || dynamic_cast<const QoreHashMapSelectOperatorNode*>(node)) {
        return false;
    }
    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASH_MAP_OP));
    return classifyAndWriteExpr(ctx.writer, op->get(0), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->get(1), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->get(2), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map);
}

static bool write_expr_hash_map_select(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* op = dynamic_cast<const QoreHashMapSelectOperatorNode*>(node);
    if (!op) {
        return false;
    }
    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::HASH_MAP_SELECT_OP));
    return classifyAndWriteExpr(ctx.writer, op->get(0), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->get(1), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->get(2), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map)
        && classifyAndWriteExpr(ctx.writer, op->get(3), ctx.parent_locals,
            ctx.parent_globals, ctx.const_reverse_map);
}

static QoreValue read_expr_map(AOTExprReadCtx& ctx) {
    return read_binary_expr<QoreMapOperatorNode>(ctx);
}

static QoreValue read_expr_map_select(AOTExprReadCtx& ctx) {
    std::string map_err;
    QoreValue map_expr = readOneExpr(ctx.reader, ctx.ptr, ctx.end, map_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    std::string source_err;
    QoreValue source = readOneExpr(ctx.reader, ctx.ptr, ctx.end, source_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    std::string where_err;
    QoreValue where_expr = readOneExpr(ctx.reader, ctx.ptr, ctx.end, where_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!map_err.empty() || !source_err.empty() || !where_err.empty()) {
        map_expr.discard(nullptr);
        source.discard(nullptr);
        where_expr.discard(nullptr);
        ctx.error = !map_err.empty() ? map_err : (!source_err.empty() ? source_err : where_err);
        return QoreValue();
    }
    return QoreValue(new QoreMapSelectOperatorNode(&loc_builtin, map_expr, source, where_expr));
}

static QoreValue read_expr_hash_map(AOTExprReadCtx& ctx) {
    std::string key_err;
    QoreValue key_expr = readOneExpr(ctx.reader, ctx.ptr, ctx.end, key_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    std::string val_err;
    QoreValue val_expr = readOneExpr(ctx.reader, ctx.ptr, ctx.end, val_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    std::string source_err;
    QoreValue source = readOneExpr(ctx.reader, ctx.ptr, ctx.end, source_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!key_err.empty() || !val_err.empty() || !source_err.empty()) {
        key_expr.discard(nullptr);
        val_expr.discard(nullptr);
        source.discard(nullptr);
        ctx.error = !key_err.empty() ? key_err : (!val_err.empty() ? val_err : source_err);
        return QoreValue();
    }
    return QoreValue(new QoreHashMapOperatorNode(&loc_builtin, key_expr, val_expr, source));
}

static QoreValue read_expr_hash_map_select(AOTExprReadCtx& ctx) {
    std::string key_err;
    QoreValue key_expr = readOneExpr(ctx.reader, ctx.ptr, ctx.end, key_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    std::string val_err;
    QoreValue val_expr = readOneExpr(ctx.reader, ctx.ptr, ctx.end, val_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    std::string source_err;
    QoreValue source = readOneExpr(ctx.reader, ctx.ptr, ctx.end, source_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    std::string where_err;
    QoreValue where_expr = readOneExpr(ctx.reader, ctx.ptr, ctx.end, where_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!key_err.empty() || !val_err.empty() || !source_err.empty() || !where_err.empty()) {
        key_expr.discard(nullptr);
        val_expr.discard(nullptr);
        source.discard(nullptr);
        where_expr.discard(nullptr);
        ctx.error = !key_err.empty() ? key_err
            : (!val_err.empty() ? val_err : (!source_err.empty() ? source_err : where_err));
        return QoreValue();
    }
    return QoreValue(new QoreHashMapSelectOperatorNode(&loc_builtin,
        key_expr, val_expr, source, where_expr));
}

static bool write_expr_select(AOTExprWriteCtx& ctx) {
    return write_binary_expr<QoreSelectOperatorNode>(ctx, AOTExprKind::SELECT);
}

static QoreValue read_expr_select(AOTExprReadCtx& ctx) {
    return read_binary_expr<QoreSelectOperatorNode>(ctx);
}

// ============================================================================
// TRIM/CHOMP/POP/SHIFT/PUSH/UNSHIFT (64-69)
// ============================================================================

template <typename NodeT>
static bool write_unary_expr(AOTExprWriteCtx& ctx, AOTExprKind kind) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* op = dynamic_cast<const NodeT*>(node);
    if (!op) {
        return false;
    }
    ctx.writer.writeU8(static_cast<uint8_t>(kind));
    return classifyAndWriteExpr(ctx.writer, op->getExp(), ctx.parent_locals,
        ctx.parent_globals, ctx.const_reverse_map);
}

template <typename NodeT>
static QoreValue read_unary_expr(AOTExprReadCtx& ctx) {
    std::string operand_err;
    QoreValue operand = readOneExpr(ctx.reader, ctx.ptr, ctx.end, operand_err, ctx.pgm,
        ctx.locals, ctx.num_locals, ctx.globals, ctx.num_globals);
    if (!operand_err.empty()) {
        operand.discard(nullptr);
        ctx.error = operand_err;
        return QoreValue();
    }
    return QoreValue(new NodeT(&loc_builtin, operand));
}

static bool write_expr_trim(AOTExprWriteCtx& ctx) {
    return write_unary_expr<QoreTrimOperatorNode>(ctx, AOTExprKind::TRIM);
}

static bool write_expr_chomp(AOTExprWriteCtx& ctx) {
    return write_unary_expr<QoreChompOperatorNode>(ctx, AOTExprKind::CHOMP);
}

static bool write_expr_pop(AOTExprWriteCtx& ctx) {
    return write_unary_expr<QorePopOperatorNode>(ctx, AOTExprKind::POP);
}

static bool write_expr_shift(AOTExprWriteCtx& ctx) {
    if (dynamic_cast<const QorePopOperatorNode*>(ctx.expr.getInternalNode())) {
        return false;
    }
    return write_unary_expr<QoreShiftOperatorNode>(ctx, AOTExprKind::SHIFT);
}

static bool write_expr_push(AOTExprWriteCtx& ctx) {
    return write_binary_expr<QorePushOperatorNode>(ctx, AOTExprKind::PUSH);
}

static bool write_expr_unshift(AOTExprWriteCtx& ctx) {
    if (dynamic_cast<const QorePushOperatorNode*>(ctx.expr.getInternalNode())) {
        return false;
    }
    return write_binary_expr<QoreUnshiftOperatorNode>(ctx, AOTExprKind::UNSHIFT);
}

static bool write_expr_elements(AOTExprWriteCtx& ctx) {
    return write_unary_expr<QoreElementsOperatorNode>(ctx, AOTExprKind::ELEMENTS);
}

static bool write_expr_delete(AOTExprWriteCtx& ctx) {
    return write_unary_expr<QoreDeleteOperatorNode>(ctx, AOTExprKind::DELETE);
}

static bool write_expr_remove(AOTExprWriteCtx& ctx) {
    return write_unary_expr<QoreRemoveOperatorNode>(ctx, AOTExprKind::REMOVE);
}

static bool write_expr_background(AOTExprWriteCtx& ctx) {
    return write_unary_expr<QoreBackgroundOperatorNode>(ctx, AOTExprKind::BACKGROUND);
}

static QoreValue read_expr_trim(AOTExprReadCtx& ctx) {
    return read_unary_expr<QoreTrimOperatorNode>(ctx);
}

static QoreValue read_expr_chomp(AOTExprReadCtx& ctx) {
    return read_unary_expr<QoreChompOperatorNode>(ctx);
}

static QoreValue read_expr_pop(AOTExprReadCtx& ctx) {
    return read_unary_expr<QorePopOperatorNode>(ctx);
}

static QoreValue read_expr_shift(AOTExprReadCtx& ctx) {
    return read_unary_expr<QoreShiftOperatorNode>(ctx);
}

static QoreValue read_expr_push(AOTExprReadCtx& ctx) {
    return read_binary_expr<QorePushOperatorNode>(ctx);
}

static QoreValue read_expr_unshift(AOTExprReadCtx& ctx) {
    return read_binary_expr<QoreUnshiftOperatorNode>(ctx);
}

static QoreValue read_expr_elements(AOTExprReadCtx& ctx) {
    return read_unary_expr<QoreElementsOperatorNode>(ctx);
}

static QoreValue read_expr_delete(AOTExprReadCtx& ctx) {
    return read_unary_expr<QoreDeleteOperatorNode>(ctx);
}

static QoreValue read_expr_remove(AOTExprReadCtx& ctx) {
    return read_unary_expr<QoreRemoveOperatorNode>(ctx);
}

static QoreValue read_expr_background(AOTExprReadCtx& ctx) {
    return read_unary_expr<QoreBackgroundOperatorNode>(ctx);
}

// ============================================================================
// CONTEXT_REF/CONTEXT_ROW/COMPLEX_CONTEXT_REF (74-76)
// ============================================================================

static bool write_expr_context_ref(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* cr = dynamic_cast<const ContextrefNode*>(node);
    if (!cr) {
        return false;
    }
    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONTEXT_REF));
    ctx.writer.writeStringRef(cr->str ? cr->str : "");
    return true;
}

static bool write_expr_context_row(AOTExprWriteCtx& ctx) {
    if (!dynamic_cast<const ContextRowNode*>(ctx.expr.getInternalNode())) {
        return false;
    }
    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::CONTEXT_ROW));
    return true;
}

static bool write_expr_complex_context_ref(AOTExprWriteCtx& ctx) {
    const AbstractQoreNode* node = ctx.expr.getInternalNode();
    auto* ccr = dynamic_cast<const ComplexContextrefNode*>(node);
    if (!ccr) {
        return false;
    }
    ctx.writer.writeU8(static_cast<uint8_t>(AOTExprKind::COMPLEX_CONTEXT_REF));
    ctx.writer.writeStringRef(ccr->name ? ccr->name : "");
    ctx.writer.writeStringRef(ccr->member ? ccr->member : "");
    ctx.writer.writeI64(static_cast<int64_t>(ccr->stack_offset));
    return true;
}

static QoreValue read_expr_context_ref(AOTExprReadCtx& ctx) {
    const char* member = ctx.reader.readStringRef(ctx.ptr);
    return QoreValue(new ContextrefNode(&loc_builtin, strdup(member ? member : "")));
}

static QoreValue read_expr_context_row(AOTExprReadCtx& ctx) {
    return QoreValue(new ContextRowNode(&loc_builtin));
}

static QoreValue read_expr_complex_context_ref(AOTExprReadCtx& ctx) {
    const char* name = ctx.reader.readStringRef(ctx.ptr);
    const char* member = ctx.reader.readStringRef(ctx.ptr);
    int64_t stack_offset = QoreAOTBinaryReader::readI64(ctx.ptr);

    std::string spec = name ? name : "";
    spec += ":";
    spec += member ? member : "";
    auto* node = new ComplexContextrefNode(&loc_builtin, strdup(spec.c_str()));
    node->stack_offset = static_cast<int>(stack_offset);
    return QoreValue(node);
}

// ============================================================================
// EXPR_TREE (254)
// ============================================================================

static bool write_expr_expr_tree(AOTExprWriteCtx& ctx) {
    // EXPR_TREE is kept for reading older binaries only.  New AOT output must
    // use native AOTExprKind payloads or fail during serialization.
    return false;
}

static QoreValue read_expr_expr_tree(AOTExprReadCtx& ctx) {
    // Read length-prefixed EXPR_TREE blob and deserialize
    uint32_t blob_size = QoreAOTBinaryReader::readU32(ctx.ptr);
    if (blob_size == 0 || ctx.ptr + blob_size > ctx.end) {
        ctx.error = "invalid EXPR_TREE blob size";
        return QoreValue();
    }
    const uint8_t* blob_data = ctx.ptr;
    ctx.ptr += blob_size;
    QoreValue result = deserializeExprTreeFromBlob(
        blob_data, blob_size, ctx.pgm, ctx.locals, ctx.num_locals);
    if (result.isNothing()) {
        ctx.error = "EXPR_TREE deserialization failed";
    }
    return result;
}

// ============================================================================
// GENERIC_EVAL (255)
// ============================================================================

static bool write_expr_generic_eval(AOTExprWriteCtx& ctx) {
    // GENERIC_EVAL is kept for reading older binaries only.  New AOT output
    // must fail before this marker is emitted.
    return false;
}

static QoreValue read_expr_generic_eval(AOTExprReadCtx& ctx) {
    // Signal that this expression cannot be deserialized — the calling code
    // must set has_unsupported so the function is rejected.
    // Without this error, GENERIC_EVAL in nested expressions (e.g., constructor args)
    // silently becomes NOTHING, corrupting argument lists at runtime.
    ctx.error = "unsupported nested expression (GENERIC_EVAL)";
    return QoreValue();
}
