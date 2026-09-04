/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreAOTRuntime.cpp

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

#include "qore/intern/QoreJITIncludes.h"
#include "qore/intern/QoreJITException.h"

#include <time.h>
#include <atomic>
#include <cstdlib>
#include <cstring>

#include <qore/ModuleManager.h>
#include <qore/QoreThreadLock.h>
#include "qore/intern/QoreAOT.h"
#include "qore/intern/QoreAOTBinary.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/Variable.h"
#include "qore/intern/GlobalVariableList.h"
#include "qore/intern/FunctionCallNode.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/FunctionList.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/qore_enum_decl_private.h"
#include <qore/QoreEnumDecl.h>
#include "qore/intern/StatementBlock.h"
#include "qore/intern/Function.h"
#include "qore/intern/QoreIR.h"
#include "qore/intern/QoreIRBuilder.h"
#include "qore/intern/QoreIRLowering.h"
#include "qore/intern/QoreIRVerifier.h"
#include "qore/intern/QoreIRInterpreter.h"
#include "qore/intern/QoreOpcodeRegistry.h"
#include "qore/intern/QoreAOTInstRegistry.h"
#include "qore/intern/QoreAOTExprRegistry.h"
#include "qore/intern/QoreAOTExprNodeRegistry.h"
#include "qore/intern/AssertStatement.h"
#include "qore/intern/CaseNodeRegex.h"
#include "qore/intern/ContextStatement.h"
#include "qore/intern/DebugStatement.h"
#include "qore/intern/ExpressionStatement.h"
#include "qore/intern/ForEachStatement.h"
#include "qore/intern/ForStatement.h"
#include "qore/intern/IfStatement.h"
#include "qore/intern/OnBlockExitStatement.h"
#include "qore/intern/RethrowStatement.h"
#include "qore/intern/ReturnStatement.h"
#include "qore/intern/SwitchStatement.h"
#include "qore/intern/ThrowStatement.h"
#include "qore/intern/TryStatement.h"
#include "qore/intern/WhileStatement.h"

#include "qore/intern/ModuleInfo.h"
#include "qore/intern/VarRefNode.h"
#include "qore/intern/StaticClassVarRefNode.h"
#include "qore/intern/ParseReferenceNode.h"
#include "qore/intern/qore_thread_intern.h"
#include "qore/intern/SelfVarrefNode.h"
#include "qore/intern/ScopedObjectCallNode.h"
#include "qore/intern/QorePseudoMethods.h"
#include "qore/intern/StaticClassVarRefNode.h"
#include "qore/intern/QoreDotEvalOperatorNode.h"
#include "qore/intern/QoreHashObjectDereferenceOperatorNode.h"
#include "qore/intern/QoreSquareBracketsOperatorNode.h"
#include "qore/intern/QoreKeysOperatorNode.h"
#include "qore/intern/QoreElementsOperatorNode.h"
#include "qore/intern/QoreExistsOperatorNode.h"
#include "qore/intern/QoreDeleteOperatorNode.h"
#include "qore/intern/QoreRemoveOperatorNode.h"
#include "qore/intern/QoreBackgroundOperatorNode.h"
#include "qore/intern/QoreInstanceOfOperatorNode.h"
#include "qore/intern/ContextrefNode.h"
#include "qore/intern/ContextRowNode.h"
#include "qore/intern/ComplexContextrefNode.h"
#include "qore/intern/QoreTrimOperatorNode.h"
#include "qore/intern/QoreChompOperatorNode.h"
#include "qore/intern/QorePushOperatorNode.h"
#include "qore/intern/QorePopOperatorNode.h"
#include "qore/intern/QoreUnshiftOperatorNode.h"
#include "qore/intern/QoreShiftOperatorNode.h"
#include "qore/intern/QoreListAssignmentOperatorNode.h"
#include "qore/intern/QoreExtractOperatorNode.h"
#include "qore/intern/QoreSpliceOperatorNode.h"
#include "qore/intern/ObjectMethodReferenceNode.h"
#include "qore/intern/CallReferenceNode.h"
#include "qore/intern/QoreCastOperatorNode.h"
#include "qore/intern/ConstantList.h"
#include "qore/intern/QoreRegexMatchOperatorNode.h"
#include "qore/intern/QoreRegexNMatchOperatorNode.h"
#include "qore/intern/QoreRegexSubstOperatorNode.h"
#include "qore/intern/QoreRegexExtractOperatorNode.h"
#include "qore/intern/QoreTransliterationOperatorNode.h"
#include "qore/intern/QoreParseListNode.h"
#include "qore/intern/QoreParseHashNode.h"
#include "qore/intern/CallReferenceCallNode.h"
#include "qore/intern/QoreAssignmentOperatorNode.h"
#include "qore/intern/QorePlusEqualsOperatorNode.h"
#include "qore/intern/QoreMinusEqualsOperatorNode.h"
#include "qore/intern/QorePlusOperatorNode.h"
#include "qore/intern/QoreMinusOperatorNode.h"
#include "qore/intern/QoreMultiplicationOperatorNode.h"
#include "qore/intern/QoreDivisionOperatorNode.h"
#include "qore/intern/QoreModuloOperatorNode.h"
#include "qore/intern/QoreLogicalAndOperatorNode.h"
#include "qore/intern/QoreLogicalOrOperatorNode.h"
#include "qore/intern/QoreLogicalEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalNotEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalAbsoluteEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalAbsoluteNotEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalLessThanOperatorNode.h"
#include "qore/intern/QoreLogicalGreaterThanOperatorNode.h"
#include "qore/intern/QoreLogicalLessThanOrEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalGreaterThanOrEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalComparisonOperatorNode.h"
#include "qore/intern/QoreLogicalNotOperatorNode.h"
#include "qore/intern/QoreNullCoalescingOperatorNode.h"
#include "qore/intern/QoreValueCoalescingOperatorNode.h"
#include "qore/intern/QoreQuestionMarkOperatorNode.h"
#include "qore/intern/QoreRangeOperatorNode.h"
#include "qore/intern/QoreUnaryMinusOperatorNode.h"
#include "qore/intern/QoreUnaryPlusOperatorNode.h"
#include "qore/intern/QoreBinaryNotOperatorNode.h"
#include "qore/intern/QoreShiftLeftOperatorNode.h"
#include "qore/intern/QoreShiftRightOperatorNode.h"
#include "qore/intern/QoreBinaryAndOperatorNode.h"
#include "qore/intern/QoreBinaryOrOperatorNode.h"
#include "qore/intern/QoreBinaryXorOperatorNode.h"
#include "qore/intern/QorePreIncrementOperatorNode.h"
#include "qore/intern/QorePreDecrementOperatorNode.h"
#include "qore/intern/QorePostIncrementOperatorNode.h"
#include "qore/intern/QorePostDecrementOperatorNode.h"
#include "qore/intern/QoreMultiplyEqualsOperatorNode.h"
#include "qore/intern/QoreDivideEqualsOperatorNode.h"
#include "qore/intern/QoreModuloEqualsOperatorNode.h"
#include "qore/intern/QoreAndEqualsOperatorNode.h"
#include "qore/intern/QoreOrEqualsOperatorNode.h"
#include "qore/intern/QoreXorEqualsOperatorNode.h"
#include "qore/intern/QoreShiftLeftEqualsOperatorNode.h"
#include "qore/intern/QoreShiftRightEqualsOperatorNode.h"
#include "qore/intern/QoreCastOperatorNode.h"
#include "qore/intern/QoreImplicitArgumentNode.h"
#include "qore/intern/QoreImplicitElementNode.h"
#include "qore/intern/ParseReferenceNode.h"
#include "qore/intern/NewComplexTypeNode.h"
#include "qore/intern/QoreSquareBracketsRangeOperatorNode.h"
#include "qore/intern/QoreMapOperatorNode.h"
#include "qore/intern/QoreMapSelectOperatorNode.h"
#include "qore/intern/QoreHashMapOperatorNode.h"
#include "qore/intern/QoreHashMapSelectOperatorNode.h"
#include "qore/intern/QoreSelectOperatorNode.h"
#include "qore/intern/QoreFoldlOperatorNode.h"
#include "qore/intern/QoreIterateOperatorNode.h"
#include "qore/intern/QoreStreamingOperatorNode.h"
#include "qore/intern/QoreRegex.h"
#include "qore/intern/QoreRegexSubst.h"
#include "qore/intern/QoreTransliteration.h"
#include "qore/intern/QoreClosureNode.h"
#include "qore/intern/QoreClosureParseNode.h"
#include "qore/intern/qore_list_private.h"
#include "qore/intern/QoreTimeZoneManager.h"
#include <qore/QoreNumberNode.h>
#include <qore/BinaryNode.h>

#include <cassert>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <deque>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <vector>
#include <dlfcn.h>
#include <mutex>
#include <unwind.h>
#ifdef __GLIBC__
#include <link.h>
#endif

// Defined in Function.cpp - collects all local variables from a StatementBlock and nested blocks
extern void collectAllStatementLocals(const StatementBlock* block, std::vector<LocalVar*>& locals);
extern void removeBlockLocalsFromBodyLocals(const StatementBlock* block, std::vector<LocalVar*>& locals);
extern void removeSignatureLocalsFromBodyLocals(std::vector<LocalVar*>& locals, const UserSignature* sig);
// collectStmtSlotStatements() declared in QoreAOT.h, defined in Function.cpp

// Defined in QoreAOT.cpp - generates unique variant key with parameter types
extern std::string getVariantKey(const char* name, const AbstractQoreFunctionVariant* variant);

static bool isAOTInitFunctionName(const char* name) {
    return name && (!strncmp(name, "__const_init::", 14)
        || !strncmp(name, "__svar_init::", 13)
        || !strncmp(name, "__gvar_init::", 13)
        || !strncmp(name, "__module_init::", 15));
}

static std::string describeAOTClassRef(const char* class_ref);
static std::string normalizeTypePaths(const std::string& sig);
static const QoreMethod* findAOTStaticMethod(const QoreClass* qc, const char* method_name);
static const QoreMethod* findAOTInstanceMethod(const QoreClass* qc, const char* method_name,
        const qore_class_private* class_ctx);
static const AbstractQoreFunctionVariant* findAOTStaticMethodVariantByRef(
        QoreProgram* pgm, const QoreMethod*& method, const QoreAOTStaticMethodRef& method_ref,
        bool pseudo);
static qore_class_private* getAOTVariantClassContext(const UserVariantBase* uvb);
static std::string makeAOTQualifiedMethodName(const char* class_path, const char* method_name);

static void makeRuntimeDeserializedClosureIRNameUnique(QoreIRFunction& ir, const UserClosureVariant* variant) {
    static std::atomic<uint64_t> closure_ir_counter{0};
    ir.name += "@" + std::to_string(reinterpret_cast<uintptr_t>(variant))
        + "_" + std::to_string(closure_ir_counter.fetch_add(1));
}

static const AbstractQoreZoneInfo* runtimeReadAOTDateZone(const char* zone_name) {
    if (!zone_name || !*zone_name || !strcmp(zone_name, "UTC")) {
        return nullptr;
    }

    ExceptionSink xsink;
    const AbstractQoreZoneInfo* zone = (*zone_name == '+' || *zone_name == '-')
        ? QTZM.findCreateOffsetZone(zone_name, &xsink)
        : QTZM.findLoadRegion(zone_name, &xsink);
    if (xsink) {
        xsink.clear();
        return nullptr;
    }
    return zone;
}

static std::string getAOTTypePathForLValue(const QoreTypeInfo* ti) {
    return qore_get_aot_serializable_type_path(ti);
}

static std::string normalizeAOTTypePathForMatch(const char* s) {
    std::string out;
    if (!s) {
        return out;
    }
    out.reserve(strlen(s));
    for (size_t k = 0; s[k]; ++k) {
        if (s[k] == ':' && s[k + 1] == ':') {
            ++k;
            continue;
        }
        out.push_back(s[k]);
    }
    return out;
}

static bool aotLocalTypeMatches(const LocalVar* lv, const char* type_path,
        QoreAOTTypeResolver* type_resolver) {
    if (!lv || !type_path || !*type_path) {
        return true;
    }
    const QoreTypeInfo* declared_ti = lv->getTypeInfo();
    std::string declared_path = qore_get_aot_serializable_type_path(declared_ti);
    std::string lvalue_path = getAOTTypePathForLValue(lv->getTypeInfoForLValue());
    std::string normalized_type_path = normalizeAOTTypePathForMatch(type_path);
    if (normalizeAOTTypePathForMatch(declared_path.c_str()) == normalized_type_path
            || normalizeAOTTypePathForMatch(lvalue_path.c_str()) == normalized_type_path) {
        return true;
    }
    if (!type_resolver) {
        return false;
    }

    std::string type_error;
    const QoreTypeInfo* resolved_ti = type_resolver->resolve(type_path, type_error);
    return resolved_ti && type_error.empty()
        && (QoreTypeInfo::isOutputIdentical(declared_ti, resolved_ti)
            || QoreTypeInfo::isOutputIdentical(lv->getTypeInfoForLValue(), resolved_ti));
}

static bool isGenericAOTTypePath(const std::string& path) {
    std::string npath = normalizeAOTTypePathForMatch(path.c_str());
    return npath.empty() || npath == "auto" || npath == "*auto" || npath == "any" || npath == "*any";
}

static bool aotLocalTypeKnownMismatch(const LocalVar* lv, const char* type_path,
        QoreAOTTypeResolver* type_resolver) {
    if (!lv || !type_path || !*type_path || aotLocalTypeMatches(lv, type_path, type_resolver)
            || !type_resolver) {
        return false;
    }

    std::string declared_path = qore_get_aot_serializable_type_path(lv->getTypeInfo());
    std::string lvalue_path = getAOTTypePathForLValue(lv->getTypeInfoForLValue());
    std::string serialized_path = normalizeAOTTypePathForMatch(type_path);
    if (isGenericAOTTypePath(declared_path) || isGenericAOTTypePath(lvalue_path)
            || isGenericAOTTypePath(serialized_path)) {
        return false;
    }

    std::string type_error;
    const QoreTypeInfo* resolved_ti = type_resolver->resolve(type_path, type_error);
    return resolved_ti && type_error.empty()
        && !QoreTypeInfo::isOutputIdentical(lv->getTypeInfo(), resolved_ti)
        && !QoreTypeInfo::isOutputIdentical(lv->getTypeInfoForLValue(), resolved_ti);
}

static LocalVar* popMatchingAOTLocal(std::unordered_map<std::string, std::deque<LocalVar*>>& local_map,
        const char* name, const char* type_path, QoreAOTTypeResolver* type_resolver) {
    if (!name || !*name) {
        return nullptr;
    }
    auto it = local_map.find(name);
    if (it == local_map.end() || it->second.empty()) {
        return nullptr;
    }

    auto cit = it->second.end();
    if (type_path && *type_path) {
        for (auto qi = it->second.begin(); qi != it->second.end(); ++qi) {
            if (aotLocalTypeMatches(*qi, type_path, type_resolver)) {
                cit = qi;
                break;
            }
        }
    } else {
        cit = it->second.begin();
    }

    if (cit == it->second.end()) {
        // Keep source-order LocalVar identity even when type-path comparison
        // cannot prove equality.  Reflection/hashdecl-derived paths can
        // stringify differently or fail resolver equality during source-stripped
        // AOT context reconstruction.  The producer and collectAllStatementLocals()
        // both walk locals in source/depth-first order, so consuming the next
        // same-name candidate is safer than creating a fresh LocalVar that cannot
        // match evalTiered's lvstack pre-instantiation.
        cit = it->second.begin();
    }
    LocalVar* rv = *cit;
    it->second.erase(cit);
    return rv;
}

static void removeAOTLocalCandidate(std::unordered_map<std::string, std::deque<LocalVar*>>& local_map,
        const char* name, LocalVar* lv) {
    if (!name || !*name || !lv) {
        return;
    }
    auto it = local_map.find(name);
    if (it == local_map.end()) {
        return;
    }
    auto& locals = it->second;
    auto lit = std::find(locals.begin(), locals.end(), lv);
    if (lit != locals.end()) {
        locals.erase(lit);
    }
}

// ---- Slot Map Context Builder (V2 — no IR re-lowering) ----

// toBitsNB is defined in QoreJITIncludes.h (shared with other JIT files)

static const QoreMethod* resolveAOTSelfMethod(const QoreClass* qc, const char* method_name,
        qore_class_private*& qcp) {
    qcp = qore_class_private::get(*const_cast<QoreClass*>(qc));
    const QoreMethod* m = qcp->parseFindSelfMethod(method_name);
    if (!m) {
        m = qc->findMethod(method_name);
        if (!m) {
            m = findAOTStaticMethod(qc, method_name);
        }
    }
    return m;
}

static qore_class_private* getAOTVariantClassContext(const UserVariantBase* uvb) {
    if (!uvb) {
        return nullptr;
    }

    if (const MethodVariantBase* mvb = dynamic_cast<const MethodVariantBase*>(uvb)) {
        if (const QoreClass* method_class = mvb->getClass()) {
            return qore_class_private::get(*const_cast<QoreClass*>(method_class));
        }
    }

    LocalVar* selfid = uvb->getUserSignature() ? uvb->getUserSignature()->getSelfId() : nullptr;
    const QoreTypeInfo* self_type_info = selfid ? selfid->getTypeInfo() : nullptr;
    const QoreClass* self_class = self_type_info ? QoreTypeInfo::getUniqueReturnClass(self_type_info) : nullptr;
    return self_class ? qore_class_private::get(*const_cast<QoreClass*>(self_class)) : nullptr;
}

static const QoreMethod* findAOTStaticMethod(const QoreClass* qc, const char* method_name) {
    if (!qc || !method_name || !*method_name) {
        return nullptr;
    }

    if (const QoreMethod* m = qc->findStaticMethod(method_name)) {
        return m;
    }
    if (const QoreMethod* m = qore_class_private::get(*const_cast<QoreClass*>(qc))
            ->parseFindLocalStaticMethod(method_name)) {
        return m;
    }

    QoreClassHierarchyIterator hi(*qc);
    size_t hierarchy_count = 0;
    while (hi.next()) {
        if (++hierarchy_count % 100 == 0 && qore_check_cancel(nullptr, "AOT static method hierarchy lookup")) {
            return nullptr;
        }
        const QoreClass& parent_qc = hi.get();
        if (const QoreMethod* m = parent_qc.findStaticMethod(method_name)) {
            return m;
        }
        if (const QoreMethod* m = qore_class_private::get(*const_cast<QoreClass*>(&parent_qc))
                ->parseFindLocalStaticMethod(method_name)) {
            return m;
        }
    }

    return nullptr;
}

static const QoreMethod* findAOTInstanceMethod(const QoreClass* qc, const char* method_name,
        const qore_class_private* class_ctx) {
    if (!qc || !method_name || !*method_name) {
        return nullptr;
    }

    ClassAccess access = Public;
    return qore_class_private::get(*const_cast<QoreClass*>(qc))->runtimeFindCommittedMethodIntern(method_name,
        access, class_ctx);
}

static QoreParseListNode* makeAOTParseArgsFromList(QoreListNode*& call_args) {
    if (!call_args) {
        return nullptr;
    }

    QoreParseListNode* pln = new QoreParseListNode(&loc_builtin);
    ConstListIterator li(call_args);
    while (li.next()) {
        QoreValue v = li.getValue();
        v.refSelf();
        pln->add(v, &loc_builtin);
    }
    call_args->deref(nullptr);
    call_args = nullptr;
    return pln;
}

static std::string makeAOTVariantSignature(const AbstractQoreFunctionVariant* v) {
    std::string rv("(");
    if (v) {
        if (AbstractFunctionSignature* sig = v->getSignature()) {
            const type_vec_t& types = sig->getTypeList();
            for (size_t i = 0; i < types.size(); ++i) {
                if (i && i % 100 == 0 && qore_check_cancel(nullptr, "AOT variant signature formatting")) {
                    return {};
                }
                if (i > 0) {
                    rv.append(",");
                }
                rv.append(qore_get_aot_serializable_type_path(types[i]));
            }
        }
    }
    rv.append(")");
    return rv;
}

static std::string trimAOTSignatureParam(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return {};
    }
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

static std::vector<std::string> splitAOTSignatureParams(const std::string& sig) {
    std::vector<std::string> out;
    if (sig.size() < 2 || sig.front() != '(' || sig.back() != ')') {
        return out;
    }

    std::string cur;
    int angle_depth = 0;
    int paren_depth = 0;
    for (size_t i = 1; i + 1 < sig.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT signature parameter splitting")) {
            return {};
        }
        char c = sig[i];
        if (c == '<' && paren_depth == 0) {
            ++angle_depth;
        } else if (c == '>' && paren_depth == 0 && angle_depth > 0) {
            --angle_depth;
        } else if (c == '(') {
            ++paren_depth;
        } else if (c == ')' && paren_depth > 0) {
            --paren_depth;
        }

        if (c == ',' && angle_depth == 0 && paren_depth == 0) {
            out.push_back(trimAOTSignatureParam(cur));
            cur.clear();
            continue;
        }
        cur.push_back(c);
    }

    std::string trimmed = trimAOTSignatureParam(cur);
    if (!trimmed.empty()) {
        out.push_back(std::move(trimmed));
    }
    return out;
}

static std::string stripAOTLeadingColons(std::string path) {
    while (path.rfind("::", 0) == 0) {
        path.erase(0, 2);
    }
    return path;
}

static bool decodeAOTModuleClassRefForMatch(const std::string& class_ref,
        bool& module_qualified, std::string& class_path) {
    static constexpr const char module_prefix[] = "@qore-module:";
    static constexpr size_t module_prefix_len = sizeof(module_prefix) - 1;

    module_qualified = false;
    if (class_ref.rfind(module_prefix, 0) == 0) {
        size_t sep = class_ref.find('\n', module_prefix_len);
        if (sep == std::string::npos) {
            return false;
        }
        module_qualified = true;
        class_path = class_ref.substr(sep + 1);
    } else {
        class_path = class_ref;
    }
    class_path = stripAOTLeadingColons(std::move(class_path));
    return !class_path.empty();
}

static size_t findAOTMatchingTypeClose(const std::string& type_sig, size_t open_pos) {
    assert(open_pos < type_sig.size() && type_sig[open_pos] == '<');

    unsigned depth = 0;
    for (size_t i = open_pos; i < type_sig.size(); ++i) {
        if (type_sig[i] == '<') {
            ++depth;
        } else if (type_sig[i] == '>') {
            assert(depth);
            if (!--depth) {
                return i;
            }
        }
    }
    return std::string::npos;
}

static std::string canonicalizeAOTObjectClassRefsForMatch(const std::string& type_sig) {
    static constexpr const char object_prefix[] = "object<";
    static constexpr const char object_or_nothing_prefix[] = "*object<";
    static constexpr size_t object_prefix_len = sizeof(object_prefix) - 1;
    static constexpr size_t object_or_nothing_prefix_len = sizeof(object_or_nothing_prefix) - 1;

    std::string rv;
    rv.reserve(type_sig.size());

    for (size_t i = 0; i < type_sig.size();) {
        size_t prefix_len = 0;
        if (!type_sig.compare(i, object_or_nothing_prefix_len, object_or_nothing_prefix)) {
            prefix_len = object_or_nothing_prefix_len;
        } else if (!type_sig.compare(i, object_prefix_len, object_prefix)) {
            prefix_len = object_prefix_len;
        }

        if (prefix_len) {
            size_t open_pos = i + prefix_len - 1;
            size_t close_pos = findAOTMatchingTypeClose(type_sig, open_pos);
            if (close_pos != std::string::npos) {
                bool module_qualified = false;
                std::string class_path;
                std::string class_ref = type_sig.substr(open_pos + 1, close_pos - open_pos - 1);
                if (decodeAOTModuleClassRefForMatch(class_ref, module_qualified, class_path)) {
                    rv.append(type_sig, i, prefix_len);
                    rv += class_path;
                    rv += '>';
                    i = close_pos + 1;
                    continue;
                }
            }
        }

        rv += type_sig[i++];
    }
    return rv;
}

static std::string canonicalizeAOTSignatureTypeForMatch(const std::string& type_sig) {
    return canonicalizeAOTObjectClassRefsForMatch(normalizeTypePaths(type_sig));
}

static bool getAOTDirectObjectClassRefForMatch(const std::string& param, bool& or_nothing,
        std::string& class_ref);

static std::string getAOTClassPathTerminalName(const std::string& class_path) {
    std::string stripped = stripAOTLeadingColons(class_path);
    size_t sep = stripped.rfind("::");
    return sep == std::string::npos ? stripped : stripped.substr(sep + 2);
}

static bool aotDirectObjectClassNameParamsCompatible(const std::string& left,
        const std::string& right) {
    bool left_or_nothing = false;
    bool right_or_nothing = false;
    std::string left_class_ref;
    std::string right_class_ref;
    if (!getAOTDirectObjectClassRefForMatch(left, left_or_nothing, left_class_ref)
            || !getAOTDirectObjectClassRefForMatch(right, right_or_nothing, right_class_ref)
            || left_or_nothing != right_or_nothing) {
        return false;
    }

    bool left_module_qualified = false;
    bool right_module_qualified = false;
    std::string left_class_path;
    std::string right_class_path;
    if (!decodeAOTModuleClassRefForMatch(left_class_ref, left_module_qualified, left_class_path)
            || !decodeAOTModuleClassRefForMatch(right_class_ref, right_module_qualified, right_class_path)) {
        return false;
    }
    if (left_class_path == right_class_path) {
        return true;
    }

    // Source-stripped AOT can have one side serialized with the canonical
    // module namespace and the other with the source-visible imported name.
    // Only accept the short-name form when one side is actually unqualified;
    // two different qualified paths remain distinct and overload ambiguity is
    // still handled by the caller.
    bool left_bare = left_class_path.find("::") == std::string::npos;
    bool right_bare = right_class_path.find("::") == std::string::npos;
    return (left_bare || right_bare)
        && getAOTClassPathTerminalName(left_class_path) == getAOTClassPathTerminalName(right_class_path);
}

static bool aotSignatureParamsCompatible(const std::string& left, const std::string& right) {
    if (left == right) {
        return true;
    }

    if (canonicalizeAOTSignatureTypeForMatch(left) == canonicalizeAOTSignatureTypeForMatch(right)) {
        return true;
    }

    return aotDirectObjectClassNameParamsCompatible(left, right);
}

static bool aotSignatureStringsCompatible(const std::string& left_sig, const std::string& right_sig) {
    std::string normalized_left = canonicalizeAOTSignatureTypeForMatch(left_sig);
    std::string normalized_right = canonicalizeAOTSignatureTypeForMatch(right_sig);
    if (normalized_left == normalized_right) {
        return true;
    }

    std::vector<std::string> left_params = splitAOTSignatureParams(normalized_left);
    std::vector<std::string> right_params = splitAOTSignatureParams(normalized_right);
    if (left_params.size() != right_params.size()) {
        return false;
    }
    for (size_t i = 0; i < left_params.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT signature string compatibility matching")) {
            return false;
        }
        if (!aotSignatureParamsCompatible(left_params[i], right_params[i])) {
            return false;
        }
    }
    return true;
}

static bool getAOTDirectObjectClassRefForMatch(const std::string& param, bool& or_nothing,
        std::string& class_ref) {
    static constexpr const char object_prefix[] = "object<";
    static constexpr const char object_or_nothing_prefix[] = "*object<";
    static constexpr size_t object_prefix_len = sizeof(object_prefix) - 1;
    static constexpr size_t object_or_nothing_prefix_len = sizeof(object_or_nothing_prefix) - 1;

    std::string normalized = normalizeTypePaths(param);
    size_t prefix_len = 0;
    or_nothing = false;
    if (!normalized.compare(0, object_or_nothing_prefix_len, object_or_nothing_prefix)) {
        prefix_len = object_or_nothing_prefix_len;
        or_nothing = true;
    } else if (!normalized.compare(0, object_prefix_len, object_prefix)) {
        prefix_len = object_prefix_len;
    } else {
        return false;
    }

    size_t open_pos = prefix_len - 1;
    size_t close_pos = findAOTMatchingTypeClose(normalized, open_pos);
    if (close_pos == std::string::npos || close_pos + 1 != normalized.size()) {
        return false;
    }

    class_ref = normalized.substr(open_pos + 1, close_pos - open_pos - 1);
    bool module_qualified = false;
    std::string class_path;
    return decodeAOTModuleClassRefForMatch(class_ref, module_qualified, class_path);
}

static bool aotDirectObjectClassParamMatchesVariant(const std::string& var_param,
        const std::string& target_param, const QoreTypeInfo* variant_ti,
        QoreAOTTypeResolver* type_resolver) {
    if (!type_resolver) {
        return false;
    }

    bool var_or_nothing = false;
    bool target_or_nothing = false;
    std::string var_class_ref;
    std::string target_class_ref;
    if (!getAOTDirectObjectClassRefForMatch(var_param, var_or_nothing, var_class_ref)
            || !getAOTDirectObjectClassRefForMatch(target_param, target_or_nothing, target_class_ref)
            || var_or_nothing != target_or_nothing) {
        return false;
    }

    const QoreClass* variant_qc = QoreTypeInfo::getUniqueReturnClass(variant_ti);
    if (!variant_qc) {
        return false;
    }

    const QoreClass* target_qc = qore_aot_resolve_class_ref(type_resolver->getProgram(),
        target_class_ref.c_str(), false);
    return target_qc && target_qc == variant_qc;
}

static std::string getAOTFunctionKeyPrefix(const std::string& key) {
    size_t paren = key.find('(');
    return paren == std::string::npos ? key : key.substr(0, paren);
}

static bool isAOTScopedTypeBuiltin(const std::string& token) {
    return token == "auto" || token == "any" || token == "binary" || token == "bool"
        || token == "code" || token == "date" || token == "float" || token == "hash"
        || token == "int" || token == "list" || token == "nothing" || token == "number"
        || token == "object" || token == "reference" || token == "softbool"
        || token == "softdate" || token == "softfloat" || token == "softint"
        || token == "softlist" || token == "softnumber" || token == "softstring"
        || token == "string" || token == "timeout" || token == "void";
}

static bool isAOTIdentifierChar(char c) {
    unsigned char uc = static_cast<unsigned char>(c);
    return std::isalnum(uc) || c == '_';
}

static std::string getAOTEnclosingNamespacePath(const std::string& path) {
    std::string rv = path;
    while (rv.rfind("::", 0) == 0) {
        rv.erase(0, 2);
    }
    size_t sep = rv.rfind("::");
    return sep == std::string::npos ? std::string() : rv.substr(0, sep);
}

static std::string getAOTVariantScopeNamespace(const AbstractQoreFunctionVariant* v) {
    const MethodVariantBase* mvb = dynamic_cast<const MethodVariantBase*>(v);
    const QoreClass* qc = mvb ? mvb->getClass() : nullptr;
    if (!qc) {
        return {};
    }
    return getAOTEnclosingNamespacePath(qc->getNamespacePath());
}

static std::string qualifyAOTTypePathInScope(const std::string& path, const std::string& ns_path) {
    if (ns_path.empty()) {
        return path;
    }

    std::string out;
    out.reserve(path.size() + ns_path.size());
    for (size_t i = 0; i < path.size();) {
        char c = path[i];
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t start = i;
            ++i;
            while (i < path.size() && isAOTIdentifierChar(path[i])) {
                ++i;
            }

            std::string token = path.substr(start, i - start);
            bool qualified = i + 1 < path.size() && path[i] == ':' && path[i + 1] == ':';
            bool maybe_type_name = !isAOTScopedTypeBuiltin(token) && !qualified;
            if (maybe_type_name) {
                out.append(ns_path);
                out.append("::");
            }
            out.append(token);
            continue;
        }
        out.push_back(c);
        ++i;
    }
    return out;
}

static bool aotVariantSignatureMatches(const AbstractQoreFunctionVariant* v,
        const std::string& target_sig, QoreAOTTypeResolver* type_resolver,
        const char* scope_path = nullptr) {
    // The slot-map registration path supplies no shared type resolver.  Without
    // one, a parameter type whose path differs only by namespace qualification
    // between the compiled slot and the runtime variant cannot be reconciled and
    // the method is left unregistered as AOT-native (its body becomes a silent
    // no-op).  This affects every category that can carry a qualified name:
    //   - object<class>: stub OMQ::SegmentEventQueue vs builtin Qorus::SegmentEventQueue
    //   - hash<hashdecl>: compiled hash<SlaInfo> vs runtime hash<OMQ::SlaInfo>
    // Bind a fallback resolver to the current (loading) program so the
    // resolve()+isInputIdentical comparison below can run for all such types.
    QoreProgram* fallback_pgm = type_resolver ? nullptr : ::getProgram();
    QoreAOTTypeResolver fallback_resolver(fallback_pgm);
    if (!type_resolver && fallback_pgm) {
        type_resolver = &fallback_resolver;
    }

    std::string var_sig = normalizeTypePaths(makeAOTVariantSignature(v));
    std::string normalized_target_sig = normalizeTypePaths(target_sig);
    if (var_sig == normalized_target_sig) {
        return true;
    }

    std::vector<std::string> var_params = splitAOTSignatureParams(var_sig);
    std::vector<std::string> target_params = splitAOTSignatureParams(normalized_target_sig);
    if (var_params.size() != target_params.size()) {
        return false;
    }

    AbstractFunctionSignature* sig = v ? v->getSignature() : nullptr;
    const type_vec_t* types = sig ? &sig->getTypeList() : nullptr;
    if (!types || types->size() != target_params.size()) {
        return target_params.empty();
    }

    std::string scope_ns = scope_path ? getAOTEnclosingNamespacePath(scope_path) : std::string();
    if (scope_ns.empty()) {
        scope_ns = getAOTVariantScopeNamespace(v);
    }

    for (size_t i = 0; i < target_params.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "AOT variant signature semantic matching")) {
            return false;
        }
        if (aotSignatureParamsCompatible(var_params[i], target_params[i])) {
            continue;
        }
        if (aotDirectObjectClassParamMatchesVariant(var_params[i], target_params[i],
                (*types)[i], type_resolver)) {
            continue;
        }
        if (!scope_ns.empty()) {
            std::string scoped_target = normalizeTypePaths(qualifyAOTTypePathInScope(target_params[i], scope_ns));
            if (var_params[i] == scoped_target) {
                continue;
            }
            if (type_resolver) {
                std::string scoped_error;
                const QoreTypeInfo* scoped_ti = type_resolver->resolve(scoped_target.c_str(), scoped_error);
                if (scoped_ti && scoped_error.empty()
                        && QoreTypeInfo::isInputIdentical((*types)[i], scoped_ti)) {
                    continue;
                }
            }
        }
        if (!type_resolver) {
            return false;
        }
        std::string error;
        const QoreTypeInfo* target_ti = type_resolver->resolve(target_params[i].c_str(), error);
        if (!target_ti || !error.empty()) {
            return false;
        }
        if (!QoreTypeInfo::isInputIdentical((*types)[i], target_ti)) {
            return false;
        }
    }
    return true;
}

static const AbstractQoreFunctionVariant* findAOTVariantBySignatureText(MethodFunctionBase* mfb,
        const char* sig_text) {
    if (!mfb || !sig_text || !*sig_text) {
        return nullptr;
    }

    if (const AbstractQoreFunctionVariant* v = mfb->findVariantBySignatureText(sig_text)) {
        return v;
    }

    std::string target_sig = normalizeTypePaths(sig_text);
    QoreFunctionIterator vi(*mfb);
    size_t variant_count = 0;
    while (vi.next()) {
        if (++variant_count % 100 == 0 && qore_check_cancel(nullptr, "AOT variant signature lookup")) {
            return nullptr;
        }
        const AbstractQoreFunctionVariant* v = vi.getVariant();
        if (normalizeTypePaths(makeAOTVariantSignature(v)) == target_sig) {
            return v;
        }
    }

    return nullptr;
}

static const AbstractQoreFunctionVariant* resolveAOTConstructorVariant(const QoreClass* qc,
        const QoreListNode* args, const char* class_path, std::string& error) {
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

struct AOTSlotResolutionCache {
    QoreProgram* pgm;
    std::unordered_map<std::string, const FunctionEntry*> functions;
    std::unordered_map<std::string, const QoreClass*> classes;
};

static thread_local AOTSlotResolutionCache* current_aot_slot_resolution_cache = nullptr;

class AOTSlotResolutionCacheScope {
public:
    explicit AOTSlotResolutionCacheScope(AOTSlotResolutionCache* cache)
            : previous(current_aot_slot_resolution_cache) {
        current_aot_slot_resolution_cache = cache;
    }

    ~AOTSlotResolutionCacheScope() {
        current_aot_slot_resolution_cache = previous;
    }

private:
    AOTSlotResolutionCache* previous;
};

static const FunctionEntry* resolveAOTFunctionEntryForSlotUncached(QoreProgram* pgm, const char* name) {
    qore_program_private* pp = qore_program_private::get(*pgm);
    if (const FunctionEntry* fe = qore_root_ns_private::runtimeFindFunctionEntry(*pp->RootNS, name)) {
        return fe;
    }

    // Native slot-map registration in a script batch runs while deserialized
    // functions are still pending.  The namespace tree is populated, but
    // runtimeFindFunctionEntry() rejects committedEmpty() functions; the
    // parse lookup can still see pending same-batch declarations.
    if (strstr(name, "::")) {
        NamedScope nscope(name);
        if (nscope.size() <= 1) {
            return nullptr;
        }
        qore_ns_private* ns = qore_ns_private::get(*pp->RootNS);
        bool full_scope_found = true;
        for (unsigned i = 0; i + 1 < nscope.size(); ++i) {
            QoreNamespace* child = ns->parseFindLocalNamespace(nscope[i]);
            if (!child) {
                full_scope_found = false;
                break;
            }
            ns = qore_ns_private::get(*child);
        }
        if (full_scope_found) {
            if (FunctionEntry* fe = ns->func_list.findNode(nscope.getIdentifier())) {
                return fe;
            }
        }

        // Standalone script fragments can serialize an unqualified function
        // call as current-namespace qualified (for example OMQ::QDBG_LOG),
        // while the aggregate metadata resolves the same symbol through the
        // root function index.  Try the terminal identifier as a final
        // pending-safe fallback after exact scoped lookup fails.
        if (const FunctionEntry* fe = qore_root_ns_private::runtimeFindFunctionEntry(
                *pp->RootNS, nscope.getIdentifier())) {
            return fe;
        }
    }
    return nullptr;
}

const FunctionEntry* qore_aot_resolve_function_entry_for_slot(QoreProgram* pgm, const char* name) {
    AOTSlotResolutionCache* cache = current_aot_slot_resolution_cache;
    if (!cache || cache->pgm != pgm || !name || !*name) {
        return resolveAOTFunctionEntryForSlotUncached(pgm, name);
    }

    auto i = cache->functions.find(name);
    if (i != cache->functions.end()) {
        return i->second;
    }
    const FunctionEntry* fe = resolveAOTFunctionEntryForSlotUncached(pgm, name);
    if (fe) {
        cache->functions.emplace(name, fe);
    }
    return fe;
}

QoreValue qore_aot_make_deferred_function_call(QoreProgram* pgm, const char* name, QoreParseListNode* args) {
    if (!name || !*name) {
        if (args) {
            args->deref();
        }
        return QoreValue();
    }

    const FunctionEntry* fe = qore_aot_resolve_function_entry_for_slot(pgm, "call_function");
    if (!fe) {
        if (args) {
            args->deref();
        }
        return QoreValue();
    }

    QoreParseListNode* dynamic_args = new QoreParseListNode(&loc_builtin);
    dynamic_args->add(new QoreStringNode(name), &loc_builtin);
    if (args) {
        dynamic_args->appendFrom(args);
        args->deref();
    }

    FunctionCallNode* call = new FunctionCallNode(&loc_builtin, fe, dynamic_args);
    call->resolveParseArgs();
    return QoreValue(call);
}

static QoreValue makeDeferredObjectSlotCall(QoreProgram* pgm, const char* class_path, QoreParseListNode* args,
        const QoreTypeInfo* object_type_info) {
    ScopedObjectCallNode* call = new ScopedObjectCallNode(&loc_builtin, class_path, args, object_type_info, pgm);
    if (args) {
        call->resolveParseArgs();
    }
    return QoreValue(call);
}

//! Resolve an expression slot identity to NaN-boxed QoreValue bits
/** Looks up the referenced function/method/class in the program's namespace tree
    and creates the appropriate AST node.
    @param kind the expression kind
    @param ref1 primary reference (function name, class path)
    @param ref2 secondary reference (method name)
    @param pgm the QoreProgram for namespace lookups
    @return NaN-boxed bits, or 0 if unresolvable
*/
static bool applyAOTExplicitTypeArgs(FunctionCallBase& call,
        const QoreAOTStaticMethodRef& method_ref, QoreProgram* pgm,
        const UserSignature* containing_signature) {
    if (!method_ref.explicit_type_args_present) {
        return true;
    }
    const AbstractQoreFunctionVariant* variant = call.getVariant();
    const UserVariantBase* uvb = variant ? variant->getUserVariantBase() : nullptr;
    const UserSignature* callee_signature = uvb ? uvb->getUserSignature() : nullptr;
    if (!method_ref.explicit_type_args_valid || !callee_signature
            || method_ref.explicit_type_arg_paths.size()
                != callee_signature->getTypeParameterCount()) {
        return false;
    }
    QoreTypeParamInstantiation inst;
    inst.owner = callee_signature;
    QoreAOTTypeResolver type_resolver(pgm);
    for (const std::string& path : method_ref.explicit_type_arg_paths) {
        std::string type_error;
        const QoreTypeInfo* type_arg = containing_signature
            ? type_resolver.resolveForSignature(path.c_str(), type_error,
                containing_signature)
            : type_resolver.resolve(path.c_str(), type_error);
        if (!type_arg || !type_error.empty()) {
            return false;
        }
        inst.type_args.push_back(type_arg);
    }
    call.setExplicitTypeParamInstantiation(std::move(inst));
    return true;
}

//! Records why the most recent expression-slot resolution failed.
/** Slot resolution reports failure as a bare 0, which surfaces to the user as an
    "unsupported AOT slot metadata" error naming only the enclosing function -- with no hint that the
    real cause is a symbol that could not be resolved.  This carries the detail from the resolution
    site to the error that is finally reported.

    Thread-local because several Programs can register AOT slots concurrently. */
static thread_local std::string aot_slot_resolve_error;

//! Describes an unresolvable AOT symbol, naming the modules that could have provided it
/** An AOT artifact only references symbols that resolved when it was compiled, so a symbol that
    cannot be resolved now means a module providing it is missing, or is older than the one compiled
    against.  Listing the modules actually loaded in this Program turns an opaque failure into a
    version/staleness comparison the caller can act on.

    @param what the kind of symbol, e.g. \c "function"
    @param sym the symbol name as recorded in the slot map
    @param pgm the Program the symbol was looked up in

    @return a description naming the symbol and the loaded modules */
static std::string describeUnresolvedAOTSymbol(const char* what, const char* sym, QoreProgram* pgm) {
    std::string msg = what;
    msg += " '";
    msg += sym ? sym : "(unnamed)";
    msg += "' could not be resolved; the AOT artifact was compiled against a build where it existed, so a "
        "module providing it is missing or older than the one compiled against";
    if (pgm) {
        ReferenceHolder<QoreListNode> features(qore_program_private::get(*pgm)->getFeatureList(), nullptr);
        if (features && !features->empty()) {
            msg += " (modules loaded here: ";
            ConstListIterator li(*features);
            bool first = true;
            while (li.next()) {
                // note: feature names can be held in inline short string storage (ex: "json"),
                // which has no QoreStringNode, so the data helper must be used to read the bytes
                QoreStringDataHelper f(li.getValue());
                if (!f) {
                    continue;
                }
                if (!first) {
                    msg += ", ";
                }
                first = false;
                msg += f.c_str();
            }
            msg += ")";
        }
    }
    return msg;
}

//! Records why an expression slot could not be resolved, naming the slot that failed
/** The slot loop reports failure only through the function-wide unsupported flag, so without this the
    caller sees "unsupported AOT slot metadata" naming the enclosing function and has no way to tell
    which slot -- of possibly hundreds -- could not be resolved, or why.

    The first failure in a function wins: it is the one that made the function unregisterable, and later
    slots are skipped once the flag is set, so their diagnostics would be less specific.

    @param slot the expression slot ordinal
    @param kind_name the slot's expression kind name, e.g. \c "STATIC_METHOD_CALL"
    @param detail why the slot could not be resolved */
static void setAOTExprSlotResolveError(int slot, const char* kind_name, const std::string& detail) {
    if (!aot_slot_resolve_error.empty()) {
        return;
    }
    std::string msg = "expr slot " + std::to_string(slot);
    if (kind_name && *kind_name) {
        msg += " (";
        msg += kind_name;
        msg += ")";
    }
    msg += ": ";
    msg += detail;
    aot_slot_resolve_error = std::move(msg);
}

//! Renders a serialized static-call target as \c Class::method() for diagnostics
static std::string describeAOTStaticCallTarget(const char* class_path, const char* method_name) {
    std::string desc = class_path && *class_path ? class_path : "<unknown class>";
    desc += "::";
    desc += method_name && *method_name ? method_name : "<unknown method>";
    desc += "()";
    return desc;
}

static uint64_t resolveExprSlot(AOTExprKind kind, const char* ref1, const char* ref2,
        QoreProgram* pgm, const UserSignature* containing_signature = nullptr) {
    if (!pgm) {
        return 0;
    }
    qore_program_private* pp = qore_program_private::get(*pgm);

    switch (kind) {
        case AOTExprKind::FUNC_CALL: {
            if (!ref1 || !*ref1) {
                return 0;
            }
            // Look up function by name
            const FunctionEntry* fe = qore_aot_resolve_function_entry_for_slot(pgm, ref1);
            if (!fe) {
                printd(0, "AOT v2: cannot resolve function '%s' for expr slot\n", ref1);
                aot_slot_resolve_error = describeUnresolvedAOTSymbol("function", ref1, pgm);
                return 0;
            }
            // Create a FunctionCallNode with no args (args handled by native code).
            // Leave FunctionCallNode::pgm unset like normal parsed calls do, so
            // builtins such as load_module() execute in the active runtime
            // program instead of the qmod shadow program used for deserialization.
            FunctionCallNode* fcn = new FunctionCallNode(
                &loc_builtin, fe, static_cast<QoreParseListNode*>(nullptr));
            std::string encoded_ref = ref2 ? ref2 : "";
            static constexpr const char* type_arg_marker = "\ntypeargs:";
            size_t type_arg_pos = encoded_ref.find(type_arg_marker);
            std::string variant_ref = type_arg_pos == std::string::npos
                ? encoded_ref : encoded_ref.substr(0, type_arg_pos);
            const AbstractQoreFunctionVariant* variant = nullptr;
            if (variant_ref.compare(0, 4, "sig:") == 0) {
                if (const AbstractQoreFunctionVariant* v =
                        fe->getFunction()->findVariantBySignatureText(
                            variant_ref.c_str() + 4)) {
                    fcn->setVariant(v);
                    variant = v;
                }
            }
            if (type_arg_pos != std::string::npos) {
                size_t count_start = type_arg_pos + strlen(type_arg_marker);
                size_t count_end = encoded_ref.find('\n', count_start);
                std::string count_text = encoded_ref.substr(count_start,
                    count_end == std::string::npos
                        ? std::string::npos : count_end - count_start);
                char* count_tail = nullptr;
                unsigned long count = strtoul(count_text.c_str(), &count_tail, 10);
                const UserVariantBase* uvb = variant
                    ? variant->getUserVariantBase() : nullptr;
                const UserSignature* callee_signature = uvb
                    ? uvb->getUserSignature() : nullptr;
                if (!count_tail || *count_tail || !callee_signature
                        || count != callee_signature->getTypeParameterCount()) {
                    delete fcn;
                    return 0;
                }
                QoreTypeParamInstantiation inst;
                inst.owner = callee_signature;
                QoreAOTTypeResolver type_resolver(pgm);
                size_t path_start = count_end;
                for (unsigned long i = 0; i < count; ++i) {
                    if (path_start == std::string::npos) {
                        delete fcn;
                        return 0;
                    }
                    ++path_start;
                    size_t path_end = encoded_ref.find('\n', path_start);
                    std::string path = encoded_ref.substr(path_start,
                        path_end == std::string::npos
                            ? std::string::npos : path_end - path_start);
                    std::string type_error;
                    const QoreTypeInfo* type_arg = containing_signature
                        ? type_resolver.resolveForSignature(path.c_str(),
                            type_error, containing_signature)
                        : type_resolver.resolve(path.c_str(), type_error);
                    if (!type_arg || !type_error.empty()) {
                        delete fcn;
                        return 0;
                    }
                    inst.type_args.push_back(type_arg);
                    path_start = path_end;
                }
                fcn->setExplicitTypeParamInstantiation(std::move(inst));
            }
            return toBitsNB(QoreValue(fcn));
        }

        case AOTExprKind::STATIC_METHOD_CALL: {
            if (!ref1 || !ref2) {
                return 0;
            }
            QoreAOTStaticMethodRef method_ref(ref2);
            const char* method_name = method_ref.method_name;
            auto makeFunctionCallBits = [&](const FunctionEntry* fe) -> uint64_t {
                if (!fe) {
                    return 0;
                }
                FunctionCallNode* fcn = new FunctionCallNode(
                    &loc_builtin, fe, static_cast<QoreParseListNode*>(nullptr));
                QoreFunction* func = fe->getFunction();
                if (method_ref.sig_text) {
                    if (const AbstractQoreFunctionVariant* v = func
                            ? func->findVariantBySignatureText(method_ref.sig_text) : nullptr) {
                        fcn->setVariant(v);
                    }
                } else if (method_ref.arg_type_sig) {
                    QoreTypeParamInstantiation type_param_instantiation;
                    std::string variant_error;
                    if (const AbstractQoreFunctionVariant* v = qore_aot_resolve_variant_from_arg_type_signature(pgm,
                            func, method_ref.arg_type_sig, nullptr, nullptr, &type_param_instantiation,
                            variant_error)) {
                        fcn->setVariant(v);
                        fcn->setTypeParamInstantiation(std::move(type_param_instantiation));
                    }
                }
                if (!applyAOTExplicitTypeArgs(*fcn, method_ref, pgm,
                        containing_signature)) {
                    delete fcn;
                    return 0;
                }
                return toBitsNB(QoreValue(fcn));
            };

            // Look up class, then find static method
            const QoreClass* qc = qore_aot_resolve_class_ref(pgm, ref1, false);
            if (!qc) {
                if (const FunctionEntry* fe = qore_aot_resolve_function_entry_for_static_call_fallback(
                        pgm, nullptr, ref1, method_name)) {
                    return makeFunctionCallBits(fe);
                }
                std::string class_desc = describeAOTClassRef(ref1);
                printd(0, "AOT v2: cannot resolve class '%s' for static method '%s'\n",
                    class_desc.c_str(), ref2);
                aot_slot_resolve_error = describeUnresolvedAOTSymbol("class", class_desc.c_str(), pgm);
                return 0;
            }
            const QoreMethod* m = findAOTStaticMethod(qc, method_name);
            if (!m) {
                m = findAOTInstanceMethod(qc, method_name, nullptr);
            }
            if (!m) {
                if (const FunctionEntry* fe = qore_aot_resolve_function_entry_for_static_call_fallback(
                        pgm, qc, ref1, method_name)) {
                    return makeFunctionCallBits(fe);
                }
                printd(0, "AOT v2: cannot find class-qualified method '%s::%s'\n", ref1, method_name);
                return 0;
            }
            const AbstractQoreFunctionVariant* resolved_variant = nullptr;
            if (method_ref.sig_text) {
                resolved_variant = findAOTStaticMethodVariantByRef(pgm, m, method_ref, false);
            }
            if (!m->isStatic()) {
                std::string qualified_method_name = makeAOTQualifiedMethodName(ref1, method_name);
                SelfFunctionCallNode* sfcn = new SelfFunctionCallNode(&loc_builtin,
                    strdup(qualified_method_name.c_str()), nullptr, m, qc,
                    qore_class_private::get(*const_cast<QoreClass*>(qc)));
                if (resolved_variant) {
                    sfcn->setVariant(resolved_variant);
                } else if (method_ref.arg_type_sig) {
                    MethodFunctionBase* mfb = qore_method_private::get(*const_cast<QoreMethod*>(m))->getFunction();
                    QoreTypeParamInstantiation type_param_instantiation;
                    std::string variant_error;
                    if (const AbstractQoreFunctionVariant* v = qore_aot_resolve_variant_from_arg_type_signature(pgm,
                            mfb, method_ref.arg_type_sig, nullptr, nullptr, &type_param_instantiation,
                            variant_error)) {
                        sfcn->setVariant(v);
                        sfcn->setTypeParamInstantiation(std::move(type_param_instantiation));
                    }
                }
                if (!applyAOTExplicitTypeArgs(*sfcn, method_ref, pgm,
                        containing_signature)) {
                    delete sfcn;
                    return 0;
                }
                return toBitsNB(QoreValue(sfcn));
            }
            // Create StaticMethodCallNode
            StaticMethodCallNode* smcn = new StaticMethodCallNode(&loc_builtin, m,
                static_cast<QoreParseListNode*>(nullptr));
            if (resolved_variant) {
                smcn->setVariant(resolved_variant);
            } else if (method_ref.arg_type_sig) {
                MethodFunctionBase* mfb = qore_method_private::get(*const_cast<QoreMethod*>(m))->getFunction();
                QoreTypeParamInstantiation type_param_instantiation;
                std::string variant_error;
                if (const AbstractQoreFunctionVariant* v = qore_aot_resolve_variant_from_arg_type_signature(pgm, mfb,
                        method_ref.arg_type_sig, nullptr, nullptr, &type_param_instantiation, variant_error)) {
                    smcn->setVariant(v);
                    smcn->setTypeParamInstantiation(std::move(type_param_instantiation));
                }
            }
            if (!applyAOTExplicitTypeArgs(*smcn, method_ref, pgm,
                    containing_signature)) {
                delete smcn;
                return 0;
            }
            return toBitsNB(QoreValue(smcn));
        }

        case AOTExprKind::SELF_METHOD_CALL: {
            if (!ref2 || !*ref2) {
                return 0;
            }
            QoreAOTStaticMethodRef method_ref(ref2);
            if (!method_ref.method_name || !*method_ref.method_name) {
                return 0;
            }
            // ref2 may be qualified ("ClassName::methodName") for explicit base class calls
            // or unqualified ("methodName") for normal self calls — extract unqualified name
            // for method lookup while preserving the full name for NamedScope construction
            const char* method_full_name = method_ref.method_name;
            const char* method_name = method_full_name;
            const char* last_sep = strrchr(method_full_name, ':');
            if (last_sep && last_sep > method_full_name && *(last_sep - 1) == ':') {
                method_name = last_sep + 1;
            }
            // Look up class, then find method
            const QoreClass* qc = nullptr;
            if (ref1 && *ref1) {
                qc = qore_aot_resolve_class_ref(pgm, ref1, false);
            }
            if (!qc) {
                std::string class_desc = describeAOTClassRef(ref1);
                printd(1, "AOT SLOT: cannot resolve class '%s' for self method '%s'\n",
                    class_desc.c_str(), method_full_name);
                return 0;
            }
            if (!strcmp(method_full_name, "copy")) {
                return toBitsNB(QoreValue(new SelfFunctionCallNode(
                    &loc_builtin, strdup(method_full_name), nullptr, qc, true)));
            }
            qore_class_private* qcp = nullptr;
            const QoreMethod* m = resolveAOTSelfMethod(qc, method_name, qcp);
            if (!m) {
                printd(2, "AOT SLOT: deferring self method '%s::%s' to name-based dispatch\n",
                    ref1 ? ref1 : "", method_name);
                return toBitsNB(QoreValue(new SelfFunctionCallNode(
                    &loc_builtin, strdup(method_full_name), nullptr, qc, qcp)));
            }
            const AbstractQoreFunctionVariant* resolved_variant = nullptr;
            QoreTypeParamInstantiation type_param_instantiation;
            if (method_ref.sig_text && *method_ref.sig_text) {
                resolved_variant = findAOTStaticMethodVariantByRef(pgm, m, method_ref, false);
            } else if (method_ref.arg_type_sig && *method_ref.arg_type_sig) {
                MethodFunctionBase* mfb = qore_method_private::get(*const_cast<QoreMethod*>(m))->getFunction();
                std::string variant_error;
                if (const AbstractQoreFunctionVariant* v = qore_aot_resolve_variant_from_arg_type_signature(pgm, mfb,
                        method_ref.arg_type_sig, qcp, nullptr, &type_param_instantiation, variant_error)) {
                    resolved_variant = v;
                }
            }
            printd(5, "AOT SLOT: resolved self method '%s::%s' -> %p\n", ref1, method_name, m);
            if (m->isStatic()) {
                StaticMethodCallNode* smcn = new StaticMethodCallNode(&loc_builtin, m, nullptr);
                if (resolved_variant) {
                    smcn->setVariant(resolved_variant);
                    if (!type_param_instantiation.type_args.empty()) {
                        smcn->setTypeParamInstantiation(std::move(type_param_instantiation));
                    }
                }
                return toBitsNB(QoreValue(smcn));
            }
            // Use the full ref2 for NamedScope: qualified names ("ClassName::method") produce
            // ns.size() > 1, making evalImpl use the method pointer directly (base class call);
            // unqualified names produce ns.size() == 1 for normal virtual dispatch
            SelfFunctionCallNode* sfcn = new SelfFunctionCallNode(&loc_builtin, strdup(method_full_name), nullptr, m,
                qc, qcp);
            if (resolved_variant) {
                sfcn->setVariant(resolved_variant);
                if (!type_param_instantiation.type_args.empty()) {
                    sfcn->setTypeParamInstantiation(std::move(type_param_instantiation));
                }
            }
            return toBitsNB(QoreValue(sfcn));
        }

        case AOTExprKind::NEW_OBJECT: {
            if (!ref1 || !*ref1) {
                return 0;
            }
            const QoreClass* qc = qore_aot_resolve_class_ref(pgm, ref1, false);
            if (!qc) {
                std::string class_desc = describeAOTClassRef(ref1);
                printd(0, "AOT v2: cannot resolve class '%s' for new object\n",
                    class_desc.c_str());
                aot_slot_resolve_error = describeUnresolvedAOTSymbol("class", class_desc.c_str(), pgm);
                return 0;
            }
            const QoreMethod* cons = qc->getConstructor();
            printd(5, "AOT NEW_OBJECT: class='%s' id=%d constructor=%p hm_size=%d\n",
                qc->getName(), qc->getID(), (void*)cons,
                (int)qore_class_private::get(*qc)->hm.size());
            if (cons) {
                const QoreFunction* cf = qore_method_private::get(*cons)->getFunction();
                printd(5, "  constructor vlist=%d\n",
                    (int)cf->numVariants());
                if (cf->numVariants() > 0) {
                    auto* sig = cf->first()->getSignature();
                    printd(5, "  first variant sig='%s' numParams=%d minParams=%d\n",
                        sig->getSignatureText(), sig->numParams(), sig->getMinParamTypes());
                    for (unsigned i = 0; i < sig->numParams(); ++i) {
                        printd(5, "    param[%d] type='%s' hasDefault=%d\n",
                            i, QoreTypeInfo::getName(sig->getParamTypeInfo(i)),
                            sig->hasDefaultArg(i));
                    }
                }
            }
            std::string variant_err;
            const AbstractQoreFunctionVariant* variant = resolveAOTConstructorVariant(qc, nullptr, ref1,
                variant_err);
            if (!variant_err.empty()) {
                printd(0, "AOT v2: %s\n", variant_err.c_str());
                return 0;
            }
            NewObjectCallNode* nocn = new NewObjectCallNode(qc, nullptr);
            if (variant) {
                nocn->setVariant(variant);
            }
            printd(5, "  nocn variant=%p\n", (void*)nocn->getVariant());
            return toBitsNB(QoreValue(nocn));
        }

        case AOTExprKind::SCOPED_NEW_OBJECT: {
            if (!ref1 || !*ref1) {
                return 0;
            }
            const QoreClass* qc = qore_aot_resolve_class_ref(pgm, ref1, false);
            if (!qc) {
                std::string class_desc = describeAOTClassRef(ref1);
                printd(0, "AOT v2: cannot resolve class '%s' for scoped new object\n",
                    class_desc.c_str());
                return 0;
            }
            std::string variant_err;
            const AbstractQoreFunctionVariant* variant = resolveAOTConstructorVariant(qc, nullptr, ref1,
                variant_err);
            if (!variant_err.empty()) {
                printd(0, "AOT v2: %s\n", variant_err.c_str());
                return 0;
            }
            ScopedObjectCallNode* socn = new ScopedObjectCallNode(&loc_builtin, qc, nullptr);
            if (variant) {
                socn->setVariant(variant);
            }
            return toBitsNB(QoreValue(socn));
        }

        case AOTExprKind::SELF_VARREF: {
            if (!ref1 || !*ref1) {
                return 0;
            }
            SelfVarrefNode* svn = new SelfVarrefNode(&loc_builtin, strdup(ref1));
            return toBitsNB(QoreValue(svn));
        }

        case AOTExprKind::GLOBAL_VARREF: {
            // Global variable references are normally handled in the loading code
            // (buildContextFromSlotMap) which has access to ctx->globals. This path is a
            // fallback for edge cases. Return 0 to indicate it should be handled elsewhere.
            return 0;
        }

        case AOTExprKind::CONST_NUMBER: {
            if (!ref1 || !*ref1) {
                return 0;
            }
            QoreNumberNode* num = new QoreNumberNode(ref1);
            return toBitsNB(QoreValue(num));
        }

        case AOTExprKind::CONST_STRING: {
            // String literal constant (e.g., "" passed as constructor arg)
            return toBitsNB(QoreValue::makeStringValue(ref1));
        }

        case AOTExprKind::CONST_BINARY: {
            if (!ref1) {
                return 0;
            }
            // Decode hex-encoded binary data
            size_t hex_len = strlen(ref1);
            size_t bin_len = hex_len / 2;
            SimpleRefHolder<BinaryNode> bin(new BinaryNode);
            if (bin_len > 0) {
                bin->preallocate(bin_len);
                unsigned char* dst = static_cast<unsigned char*>(
                    const_cast<void*>(bin->getPtr()));
                for (size_t i = 0; i < bin_len; ++i) {
                    unsigned int byte;
                    sscanf(ref1 + i * 2, "%2x", &byte);
                    dst[i] = static_cast<unsigned char>(byte);
                }
            }
            return toBitsNB(QoreValue(bin.release()));
        }

        case AOTExprKind::CLOSURE_CREATE:
            // Closures require the full AST context — they can't be symbolically
            // resolved from just a name. Mark as resolvable so the function doesn't
            // get flagged as unsupported; the CreateClosure opcode delegates to AST
            // at runtime and will use the expr slot which stores the original
            // QoreClosureParseNode from the parsed source.
            // Return 0 to indicate no resolution needed — the slot will be filled
            // from the re-parsed source at runtime via buildContextForVariant().
            return 0;

        case AOTExprKind::STATIC_VARREF: {
            if (!ref1 || !ref2) {
                return 0;
            }
            const QoreClass* qc = qore_aot_resolve_class_ref(pgm, ref1, false);
            if (!qc) {
                DeferredStaticClassMemberRefNode* node = new DeferredStaticClassMemberRefNode(&loc_builtin, ref1,
                    ref2);
                return toBitsNB(QoreValue(node));
            }
            // Walk the class hierarchy to find the static member.  Source code like
            // `DbDataProvider::table_lookup` where `table_lookup` is defined on
            // `DbDataProviderBase` (a parent class) serializes ref1 as the class
            // in whose scope the reference was made (the derived class), but
            // `findLocalStaticMember` only looks at own-vars — not inherited.
            // Walk parents to match AST-side name resolution.
            const QoreClass* owner_qc = qc;
            const QoreExternalStaticMember* m = qc->findLocalStaticMember(ref2);
            if (!m) {
                QoreClassHierarchyIterator hi(*qc);
                while (hi.next()) {
                    const QoreClass& pqc = hi.get();
                    m = pqc.findLocalStaticMember(ref2);
                    if (m) {
                        owner_qc = &pqc;
                        break;
                    }
                }
            }
            if (!m) {
                DeferredStaticClassMemberRefNode* node = new DeferredStaticClassMemberRefNode(&loc_builtin, ref1,
                    ref2);
                return toBitsNB(QoreValue(node));
            }
            // QoreExternalStaticMember is the public API facade for QoreVarInfo
            QoreVarInfo* vi = const_cast<QoreVarInfo*>(
                reinterpret_cast<const QoreVarInfo*>(m));
            StaticClassVarRefNode* node = new StaticClassVarRefNode(&loc_builtin, ref2,
                *owner_qc, *vi);
            return toBitsNB(QoreValue(node));
        }

        case AOTExprKind::RUNTIME_CONST_REF: {
            if (!ref1 || !*ref1) {
                return 0;
            }
            bool resolved = false;
            QoreValue rv = qore_aot_resolve_constant_path_value(pgm, ref1, true, true, &resolved);
            if (!resolved) {
                printd(0, "AOT v2: cannot resolve constant '%s'\n", ref1);
                return 0;
            }
            return toBitsNB(rv);
        }

        case AOTExprKind::CALL_REF:
        case AOTExprKind::OBJ_METHOD_REF:
            // These need dedicated binary metadata for proper resolution.
            printd(1, "AOT v2: expression kind %d is not supported in serialized metadata\n", (int)kind);
            return 0;

        case AOTExprKind::EXPR_TREE:
            // Handled inline in buildContextFromSlotMap
            return 0;

        case AOTExprKind::HASHDECL_NEW: {
            if (!ref1 || !*ref1) {
                return 0;
            }
            const TypedHashDecl* hd = qore_aot_resolve_hashdecl_path(pgm, ref1);
            if (!hd) {
                printd(0, "AOT v2: cannot resolve hashdecl '%s' for new hashdecl\n", ref1);
                return 0;
            }
            // Create a NewHashDeclNode with no args (args handled by native code)
            NewHashDeclNode* nhd = new NewHashDeclNode(&loc_builtin, hd,
                static_cast<QoreParseListNode*>(nullptr), false);
            return toBitsNB(QoreValue(nhd));
        }

        // COMPLEX_HASH_NEW and COMPLEX_LIST_NEW are handled inline in the read path
        // (they use 'continue' to skip resolveExprSlot)

        case AOTExprKind::CONST_ENUM: {
            if (!ref1 || !ref2) {
                return 0;
            }
            const QoreNamespace* pns = nullptr;
            const QoreEnumDecl* ed = pgm->findEnum(ref1, pns);
            if (!ed) {
                printd(0, "AOT v2: cannot resolve enum '%s' for const enum\n", ref1);
                return 0;
            }
            const QoreEnumMember* member = ed->findMember(ref2);
            if (!member) {
                printd(0, "AOT v2: cannot find enum member '%s::%s'\n", ref1, ref2);
                return 0;
            }
            QoreValue enum_val = QoreValue::makeEnum(member);
            uint64_t bits;
            memcpy(&bits, &enum_val, sizeof(bits));
            return bits;
        }

        case AOTExprKind::GENERIC_EVAL:
        default:
            // Unsupported expression metadata.
            return 0;
    }
}

// ---- Cast Expression Slot Resolver ----

//! Resolves a cast operator expression slot from its serialized representation
static uint64_t resolveCastExprSlot(AOTExprKind kind, const char* ref1, bool or_nothing,
        QoreProgram* pgm) {
    if (!ref1 || !*ref1) {
        return 0;
    }
    qore_program_private* pp = qore_program_private::get(*pgm);
    qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);

    switch (kind) {
        case AOTExprKind::CAST_HASHDECL: {
            if (!strcmp(ref1, "hash")) {
                auto* node = new QoreHashDeclCastOperatorNode(&loc_builtin,
                    static_cast<const TypedHashDecl*>(nullptr), QoreValue(), or_nothing);
                return toBitsNB(QoreValue(node));
            }
            const TypedHashDecl* hd = qore_aot_resolve_hashdecl_path(pgm, ref1);
            if (!hd && (!ref1 || !*ref1)) {
                printd(0, "AOT v2: cannot resolve hashdecl '%s' for cast\n", ref1);
                return 0;
            }
            // Use empty hash as inner expression (default for top-level slot resolution)
            auto* node = hd
                ? new QoreHashDeclCastOperatorNode(&loc_builtin, hd, QoreValue(new QoreHashNode(autoTypeInfo)),
                    or_nothing)
                : new QoreHashDeclCastOperatorNode(&loc_builtin, ref1, QoreValue(new QoreHashNode(autoTypeInfo)),
                    or_nothing);
            return toBitsNB(QoreValue(node));
        }
        case AOTExprKind::CAST_COMPLEX_HASH: {
            std::string type_error;
            QoreAOTTypeResolver type_resolver(pgm);
            const QoreTypeInfo* ti = type_resolver.resolve(ref1, type_error);
            if (!ti) {
                printd(0, "AOT v2: cannot resolve type '%s' for complex hash cast: %s\n",
                    ref1, type_error.c_str());
                return 0;
            }
            auto* node = new QoreComplexHashCastOperatorNode(&loc_builtin, ti, QoreValue(), or_nothing);
            return toBitsNB(QoreValue(node));
        }
        case AOTExprKind::CAST_COMPLEX_LIST: {
            if (!strcmp(ref1, "list")) {
                auto* node = new QoreComplexListCastOperatorNode(&loc_builtin, nullptr, QoreValue(), or_nothing);
                return toBitsNB(QoreValue(node));
            }
            std::string type_error;
            QoreAOTTypeResolver type_resolver(pgm);
            const QoreTypeInfo* ti = type_resolver.resolve(ref1, type_error);
            if (!ti) {
                printd(0, "AOT v2: cannot resolve type '%s' for complex list cast: %s\n",
                    ref1, type_error.c_str());
                return 0;
            }
            auto* node = new QoreComplexListCastOperatorNode(&loc_builtin, ti, QoreValue(), or_nothing);
            return toBitsNB(QoreValue(node));
        }
        case AOTExprKind::CAST_CLASS: {
            if (!strcmp(ref1, "object")) {
                auto* node = new QoreClassCastOperatorNode(&loc_builtin, nullptr, QoreValue(), or_nothing);
                return toBitsNB(QoreValue(node));
            }
            const QoreClass* qc = qore_aot_resolve_class_ref(pgm, ref1, false);
            if (!qc) {
                std::string class_desc = describeAOTClassRef(ref1);
                printd(0, "AOT v2: cannot resolve class '%s' for cast\n",
                    class_desc.c_str());
                return 0;
            }
            auto* node = new QoreClassCastOperatorNode(&loc_builtin, qc, QoreValue(), or_nothing);
            return toBitsNB(QoreValue(node));
        }
        case AOTExprKind::CAST_ENUM: {
            std::string type_error;
            QoreAOTTypeResolver type_resolver(pgm);
            const QoreTypeInfo* ti = type_resolver.resolve(ref1, type_error);
            if (!ti) {
                printd(0, "AOT v2: cannot resolve type '%s' for enum cast: %s\n",
                    ref1, type_error.c_str());
                return 0;
            }
            const QoreEnumDecl* ed = QoreTypeInfo::getUniqueReturnEnum(ti);
            if (!ed) {
                printd(0, "AOT v2: cannot extract enum from type '%s' for cast\n", ref1);
                return 0;
            }
            auto* node = new QoreEnumCastOperatorNode(&loc_builtin, ed, ti, QoreValue(), or_nothing);
            return toBitsNB(QoreValue(node));
        }
        case AOTExprKind::CAST_SCALAR: {
            std::string type_error;
            QoreAOTTypeResolver type_resolver(pgm);
            const QoreTypeInfo* ti = type_resolver.resolve(ref1, type_error);
            if (!ti || !QoreScalarCastOperatorNode::isSupportedCastType(ti)) {
                printd(0, "AOT v2: cannot resolve scalar type '%s' for cast: %s\n",
                    ref1, type_error.c_str());
                return 0;
            }
            auto* node = new QoreScalarCastOperatorNode(&loc_builtin, ti, QoreValue(), or_nothing);
            return toBitsNB(QoreValue(node));
        }
        default:
            return 0;
    }
}

// ---- Inline IR Expression Reader ----

//! Read one expression from inline closure/handler IR binary data.
/** Used by readExprCb lambdas in buildContextFromSlotMap to deserialize expressions
    stored inline in closure and handler IR bodies. The format is produced by
    classifyAndWriteExpr(): kind(u8) + kind-specific data (stringrefs, recursive exprs).

    For NEW_OBJECT/SCOPED_NEW_OBJECT: class_path(stringref)
    + [object_type_path(stringref)] + num_args(u8) + N×readOneExpr().

    @param rdr binary reader (for readStringRef)
    @param p current read pointer (advanced by reading)
    @param e end of valid data
    @param err set to error message on failure
    @param pgm the Qore program (for symbol resolution)
    @param locals LocalVar* array for LOCAL_VARREF resolution (may be null)
    @param num_locals number of entries in locals
    @param globals Var* array for GLOBAL_VARREF resolution (may be null)
    @param num_globals number of entries in globals
    @return reconstructed expression, or NOTHING on failure
*/
//! Resolve a pseudo-class by its serialized path (e.g. "::Qore::<value>")
//! Pseudo-classes are not in the normal namespace hierarchy, so runtimeFindClass
//! cannot find them.  This helper iterates the pseudo-class table and matches by path.
static const QoreClass* findPseudoClassByPath(const char* path) {
    if (!path || !*path) {
        return nullptr;
    }
    // Check base types NT_NOTHING(0) through NT_NUMBER(11)
    for (qore_type_t t = 0; t <= NT_NUMBER; ++t) {
        const QoreClass* pc = qore_pseudo_get_class(t);
        if (pc && !strcmp(pc->getPath(), path)) {
            return pc;
        }
    }
    // Check funcref and closure pseudo-classes
    const QoreClass* pc = qore_pseudo_get_class(NT_FUNCREF);
    if (pc && !strcmp(pc->getPath(), path)) {
        return pc;
    }
    pc = qore_pseudo_get_class(NT_RUNTIME_CLOSURE);
    if (pc && !strcmp(pc->getPath(), path)) {
        return pc;
    }
    return nullptr;
}

static constexpr const char* AOT_RUNTIME_CLASS_REF_MODULE_PREFIX = "@qore-module:";
static constexpr size_t AOT_RUNTIME_CLASS_REF_MODULE_PREFIX_LEN = 13;

struct AOTClassRef {
    const char* path = nullptr;
    const char* module = nullptr;
    std::string path_storage;
    std::string module_storage;
};

static AOTClassRef decodeAOTClassRef(const char* class_ref) {
    AOTClassRef ref;
    if (!class_ref) {
        return ref;
    }

    if (!strncmp(class_ref, AOT_RUNTIME_CLASS_REF_MODULE_PREFIX,
            AOT_RUNTIME_CLASS_REF_MODULE_PREFIX_LEN)) {
        const char* module_start = class_ref + AOT_RUNTIME_CLASS_REF_MODULE_PREFIX_LEN;
        const char* sep = strchr(module_start, '\n');
        if (sep) {
            ref.module_storage.assign(module_start, sep - module_start);
            ref.path_storage.assign(sep + 1);
            ref.module = ref.module_storage.c_str();
            ref.path = ref.path_storage.c_str();
            return ref;
        }
    }

    ref.path = class_ref;
    return ref;
}

static bool isAOTBareClassIdentifier(const char* class_path) {
    if (!class_path || !*class_path || !std::isalpha(static_cast<unsigned char>(*class_path))
            && *class_path != '_') {
        return false;
    }
    for (const char* p = class_path + 1; *p; ++p) {
        if (!isAOTIdentifierChar(*p)) {
            return false;
        }
    }
    return true;
}

static bool isAOTNamespaceUnder(const qore_ns_private* ns, const qore_ns_private* root) {
    for (const qore_ns_private* cur = ns; cur; cur = cur->parent) {
        if (cur == root) {
            return true;
        }
    }
    return false;
}

static const QoreClass* resolveAOTBareClassRefByNamespacePriority(QoreProgram* pgm, const char* class_name) {
    if (!pgm || !isAOTBareClassIdentifier(class_name)) {
        return nullptr;
    }

    qore_program_private* pp = qore_program_private::get(*pgm);
    qore_ns_private* root = qore_ns_private::get(*pp->RootNS);
    qore_ns_private* qore_ns = nullptr;
    if (QoreNamespace* qore_ns_obj = root->parseFindLocalNamespace("Qore")) {
        qore_ns = qore_ns_private::get(*qore_ns_obj);
    }

    const QoreClass* best_qore = nullptr;
    unsigned best_qore_depth = 0;
    bool ambiguous_qore = false;
    const QoreClass* best_other = nullptr;
    unsigned best_other_depth = 0;
    bool ambiguous_other = false;

    QorePrivateNamespaceIterator nsi(root);
    while (nsi.next()) {
        qore_ns_private* ns = nsi.get();
        const QoreClass* qc = ns->parseFindLocalClass(class_name);
        if (!qc) {
            continue;
        }

        if (qore_ns && isAOTNamespaceUnder(ns, qore_ns)) {
            if (!best_qore || ns->depth < best_qore_depth) {
                best_qore = qc;
                best_qore_depth = ns->depth;
                ambiguous_qore = false;
            } else if (ns->depth == best_qore_depth && qc != best_qore) {
                ambiguous_qore = true;
            }
            continue;
        }

        if (!best_other || ns->depth < best_other_depth) {
            best_other = qc;
            best_other_depth = ns->depth;
            ambiguous_other = false;
        } else if (ns->depth == best_other_depth && qc != best_other) {
            ambiguous_other = true;
        }
    }

    if (best_qore) {
        return ambiguous_qore ? nullptr : best_qore;
    }
    return ambiguous_other ? nullptr : best_other;
}

static const QoreClass* resolveAOTClassRefInProgram(QoreProgram* pgm,
        const char* class_path, bool pseudo) {
    if (!pgm || !class_path || !*class_path) {
        return nullptr;
    }

    qore_program_private* pp = qore_program_private::get(*pgm);
    const qore_ns_private* found_ns = nullptr;
    const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
        *pp->RootNS, class_path, found_ns);
    if (!qc && class_path[0] == ':' && class_path[1] == ':') {
        qc = qore_root_ns_private::runtimeFindClass(*pp->RootNS,
            class_path + 2, found_ns);
    }
    if (!qc) {
        // AOT metadata is emitted from source parse, where private/internal
        // classes in the target Program are valid constructor/type targets.
        // Runtime lookup only sees public committed maps, and the current
        // thread can be executing another Program (for example a Qorus system
        // service calling into shared core code), so resolve against the target
        // Program's parse namespace directly.
        const char* req_path = class_path;
        if (req_path[0] == ':' && req_path[1] == ':') {
            req_path += 2;
        }
        if (*req_path) {
            qc = qore_root_ns_private::parseFindClass(*pp->RootNS, &loc_builtin, req_path);
        }
    }
    if (!qc && !pseudo) {
        qc = resolveAOTBareClassRefByNamespacePriority(pgm, class_path);
    }
    if (!qc && pseudo) {
        qc = findPseudoClassByPath(class_path);
    }
    if (!qc) {
        const char* req_path = class_path;
        if (req_path[0] == ':' && req_path[1] == ':') {
            req_path += 2;
        }
        QorePrivateNamespaceIterator nsi(qore_ns_private::get(*pp->RootNS));
        while (nsi.next()) {
            ConstClassListIterator cli(nsi.get()->classList);
            while (cli.next()) {
                const QoreClass* c = cli.get();
                const char* cpath = c ? c->getPath() : nullptr;
                if (!cpath || !*cpath) {
                    continue;
                }
                if (!strcmp(cpath, class_path)) {
                    return c;
                }
                if (cpath[0] == ':' && cpath[1] == ':' && !strcmp(cpath + 2, req_path)) {
                    return c;
                }
            }
        }
    }
    return qc;
}

const QoreClass* qore_aot_resolve_class_ref(QoreProgram* pgm,
        const char* class_ref, bool pseudo) {
    AOTSlotResolutionCache* cache = current_aot_slot_resolution_cache;
    std::string cache_key;
    if (cache && cache->pgm == pgm && class_ref && *class_ref) {
        cache_key.reserve(strlen(class_ref) + 1);
        cache_key.push_back(pseudo ? 'P' : 'C');
        cache_key.append(class_ref);
        auto i = cache->classes.find(cache_key);
        if (i != cache->classes.end()) {
            return i->second;
        }
    }

    AOTClassRef ref = decodeAOTClassRef(class_ref);
    const QoreClass* qc = resolveAOTClassRefInProgram(pgm, ref.path, pseudo);
    if (qc || !ref.module || !*ref.module) {
        if (qc && cache && cache->pgm == pgm) {
            cache->classes.emplace(std::move(cache_key), qc);
        }
        return qc;
    }

    QoreProgram* module_pgm = MM.findUserModuleProgram(ref.module);
    qc = resolveAOTClassRefInProgram(module_pgm, ref.path, pseudo);
    if (qc && cache && cache->pgm == pgm) {
        cache->classes.emplace(std::move(cache_key), qc);
    }
    return qc;
}

static std::string describeAOTClassRef(const char* class_ref) {
    AOTClassRef ref = decodeAOTClassRef(class_ref);
    if (!ref.module || !*ref.module) {
        return ref.path ? ref.path : "(null)";
    }
    std::string rv(ref.path ? ref.path : "(null)");
    rv += " [module ";
    rv += ref.module;
    rv += "]";
    return rv;
}

static std::string makeAOTQualifiedMethodName(const char* class_path, const char* method_name) {
    AOTClassRef ref = decodeAOTClassRef(class_path);
    std::string rv(ref.path && *ref.path ? ref.path : (class_path ? class_path : ""));
    if (!rv.empty()) {
        rv += "::";
    }
    rv += method_name ? method_name : "";
    return rv;
}

static void stripAOTLeadingScope(std::string& path) {
    if (path.size() >= 2 && path[0] == ':' && path[1] == ':') {
        path.erase(0, 2);
    }
}

static void addAOTFunctionCandidate(std::vector<std::string>& candidates,
        std::string ns_path, const char* method_name) {
    if (!method_name || !*method_name) {
        return;
    }
    stripAOTLeadingScope(ns_path);
    if (ns_path.empty()) {
        return;
    }
    ns_path += "::";
    ns_path += method_name;
    if (std::find(candidates.begin(), candidates.end(), ns_path) == candidates.end()) {
        candidates.push_back(std::move(ns_path));
    }
}

static bool aotFunctionPathMatchesSuffix(const std::string& full_path, const std::string& suffix) {
    if (full_path == suffix) {
        return true;
    }
    if (full_path.size() <= suffix.size()) {
        return false;
    }
    size_t pos = full_path.size() - suffix.size();
    return full_path.compare(pos, suffix.size(), suffix) == 0
        && pos >= 2 && full_path[pos - 1] == ':' && full_path[pos - 2] == ':';
}

static const FunctionEntry* findUniqueAOTFunctionBySuffix(QoreProgram* pgm,
        const char* terminal_name, const std::string& suffix) {
    if (!pgm || !terminal_name || !*terminal_name || suffix.empty()) {
        return nullptr;
    }

    qore_program_private* pp = qore_program_private::get(*pgm);
    qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);
    const FunctionEntry* match = nullptr;
    unsigned matches = 0;
    size_t visited = 0;
    bool cancelled = false;

    std::function<void(qore_ns_private*)> walk = [&](qore_ns_private* ns) {
        if (!ns || cancelled || matches > 1) {
            return;
        }
        if (++visited % 100 == 0 && qore_check_cancel(nullptr, "AOT function suffix lookup")) {
            cancelled = true;
            return;
        }

        if (FunctionEntry* fe = ns->func_list.findNode(terminal_name)) {
            std::string full_path;
            ns->getPath(full_path);
            if (!full_path.empty()) {
                full_path += "::";
            }
            full_path += terminal_name;
            if (aotFunctionPathMatchesSuffix(full_path, suffix)) {
                match = fe;
                ++matches;
                if (matches > 1) {
                    return;
                }
            }
        }

        for (auto ni = ns->nsl.nsmap.begin(), ne = ns->nsl.nsmap.end(); ni != ne; ++ni) {
            if (ni->second) {
                walk(qore_ns_private::get(*ni->second));
                if (cancelled || matches > 1) {
                    return;
                }
            }
        }
    };

    walk(root_ns);
    return matches == 1 ? match : nullptr;
}

const FunctionEntry* qore_aot_resolve_function_entry_for_static_call_fallback(
        QoreProgram* pgm, const QoreClass* qc, const char* class_ref, const char* method_name) {
    if (!pgm || !method_name || !*method_name) {
        return nullptr;
    }

    AOTClassRef ref = decodeAOTClassRef(class_ref);
    std::vector<std::string> candidates;
    addAOTFunctionCandidate(candidates, ref.path ? ref.path : "", method_name);

    if (qc) {
        qore_class_private* qcp = qore_class_private::get(*const_cast<QoreClass*>(qc));
        if (qcp && qcp->ns) {
            std::string ns_path;
            qcp->ns->getPath(ns_path);
            addAOTFunctionCandidate(candidates, std::move(ns_path), method_name);
        }
    }

    for (const std::string& candidate : candidates) {
        if (const FunctionEntry* fe = qore_aot_resolve_function_entry_for_slot(pgm, candidate.c_str())) {
            return fe;
        }
    }

    if (ref.module && *ref.module) {
        if (QoreProgram* module_pgm = MM.findUserModuleProgram(ref.module)) {
            for (const std::string& candidate : candidates) {
                if (const FunctionEntry* fe = qore_aot_resolve_function_entry_for_slot(
                        module_pgm, candidate.c_str())) {
                    return fe;
                }
            }
        }
    }

    std::string suffix(ref.path ? ref.path : "");
    stripAOTLeadingScope(suffix);
    if (!suffix.empty()) {
        suffix += "::";
        suffix += method_name;
        if (const FunctionEntry* fe = findUniqueAOTFunctionBySuffix(pgm, method_name, suffix)) {
            return fe;
        }
    }

    return nullptr;
}

struct AOTEncodedMethodRef {
    const char* method_name = nullptr;
    const char* variant_class_path = nullptr;
    const char* sig_text = nullptr;
    std::string method_name_storage;
    std::string variant_class_storage;

    AOTEncodedMethodRef(const char* encoded) : method_name(encoded) {
        if (!encoded) {
            return;
        }
        const char* first_sep = strchr(encoded, '\n');
        if (!first_sep) {
            return;
        }
        method_name_storage.assign(encoded, first_sep - encoded);
        method_name = method_name_storage.c_str();

        const char* payload = first_sep + 1;
        const char* second_sep = strrchr(payload, '\n');
        if (!second_sep) {
            // Backward-compatible form: method_name + "\n" + signature.
            sig_text = payload;
            return;
        }

        variant_class_storage.assign(payload, second_sep - payload);
        if (!variant_class_storage.empty()) {
            variant_class_path = variant_class_storage.c_str();
        }
        sig_text = second_sep + 1;
    }
};

static const QoreClass* findAOTClassByPath(QoreProgram* pgm, const char* class_path, bool pseudo) {
    return qore_aot_resolve_class_ref(pgm, class_path, pseudo);
}

static bool isAOTUnqualifiedRelativeClassRef(const AOTClassRef& ref) {
    return (!ref.module || !*ref.module) && ref.path && *ref.path
        && !(ref.path[0] == ':' && ref.path[1] == ':') && !strstr(ref.path, "::");
}

static const QoreClass* findAOTClassByPathInContext(QoreProgram* pgm, const char* class_path,
        const qore_class_private* class_ctx, bool pseudo) {
    if (!pgm || !class_path || !*class_path) {
        return nullptr;
    }

    AOTClassRef ref = decodeAOTClassRef(class_path);
    if (class_ctx && class_ctx->ns && isAOTUnqualifiedRelativeClassRef(ref)) {
        for (const qore_ns_private* ns = class_ctx->ns; ns; ns = ns->parent) {
            if (qore_check_cancel(nullptr, "AOT contextual class lookup")) {
                return nullptr;
            }
            std::string ns_path;
            ns->getPath(ns_path);
            if (ns_path.empty()) {
                continue;
            }
            ns_path += "::";
            ns_path += ref.path;
            if (const QoreClass* qc = findAOTClassByPath(pgm, ns_path.c_str(), pseudo)) {
                return qc;
            }
        }
    }

    return findAOTClassByPath(pgm, class_path, pseudo);
}

static const QoreMethod* findAOTMethodByName(const QoreClass* qc, const char* method_name) {
    if (!qc || !method_name || !*method_name) {
        return nullptr;
    }
    const QoreMethod* method = qc->findMethod(method_name);
    return method ? method : findAOTStaticMethod(qc, method_name);
}

static const AbstractQoreFunctionVariant* findAOTMethodVariantByRef(
        QoreProgram* pgm, const QoreMethod*& method, const AOTEncodedMethodRef& method_ref,
        bool pseudo) {
    if (!method || !method_ref.sig_text || !*method_ref.sig_text) {
        return nullptr;
    }

    const QoreMethod* variant_method = method;
    if (method_ref.variant_class_path && *method_ref.variant_class_path) {
        const QoreClass* variant_qc = findAOTClassByPath(pgm, method_ref.variant_class_path, pseudo);
        if (variant_qc) {
            if (const QoreMethod* m = findAOTMethodByName(variant_qc, method_ref.method_name)) {
                variant_method = m;
            }
        }
    }

    MethodFunctionBase* mfb = qore_method_private::get(
        *const_cast<QoreMethod*>(variant_method))->getFunction();
    const AbstractQoreFunctionVariant* variant = mfb
        ? mfb->findVariantBySignatureText(method_ref.sig_text) : nullptr;
    if (variant) {
        method = variant_method;
    }
    return variant;
}

static const QoreMethod* findAOTStaticCallMethodByName(const QoreClass* qc, const char* method_name) {
    if (!qc || !method_name || !*method_name) {
        return nullptr;
    }
    if (const QoreMethod* method = findAOTStaticMethod(qc, method_name)) {
        return method;
    }
    return findAOTInstanceMethod(qc, method_name, nullptr);
}

static const AbstractQoreFunctionVariant* findAOTStaticMethodVariantByRef(
        QoreProgram* pgm, const QoreMethod*& method, const QoreAOTStaticMethodRef& method_ref,
        bool pseudo) {
    if (!method || !method_ref.sig_text || !*method_ref.sig_text) {
        return nullptr;
    }

    const QoreMethod* variant_method = method;
    if (method_ref.variant_class_path && *method_ref.variant_class_path) {
        const QoreClass* variant_qc = findAOTClassByPath(pgm, method_ref.variant_class_path, pseudo);
        if (variant_qc) {
            // Re-resolve the method in the variant class while preserving the static-ness of the
            // original call.  findAOTStaticCallMethodByName() prefers a static overload, so for a
            // self/instance call it would wrongly rebind to a same-named static method (e.g.
            // Program::issueModuleCmd has both a static and a non-static variant with identical
            // signatures).  The static issueModuleCmd operates on getProgram() (the AOT module's
            // own program) instead of the receiver object, sending the command to the wrong
            // QoreProgram.  Match the original method kind: instance -> instance, static -> static.
            const QoreMethod* m = method->isStatic()
                ? findAOTStaticCallMethodByName(variant_qc, method_ref.method_name)
                : findAOTInstanceMethod(variant_qc, method_ref.method_name, nullptr);
            if (m) {
                variant_method = m;
            }
        }
    }

    MethodFunctionBase* mfb = qore_method_private::get(
        *const_cast<QoreMethod*>(variant_method))->getFunction();
    const AbstractQoreFunctionVariant* variant = findAOTVariantBySignatureText(mfb, method_ref.sig_text);
    if (variant) {
        method = variant_method;
    }
    return variant;
}

static bool isNumericGlobalSlotRef(const char* ref) {
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

static Var* resolveGlobalVarRefPayload(const char* ref, QoreProgram* pgm,
        Var** globals, int num_globals) {
    if (!ref || !*ref) {
        return nullptr;
    }

    if (isNumericGlobalSlotRef(ref)) {
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

//! Advance pointer past one AOTExprKind-encoded expression without allocating objects
static void skipOneExpr(const QoreAOTBinaryReader& rdr, const uint8_t*& p, const uint8_t* e) {
    if (p >= e) { return; }
    uint8_t ekind = QoreAOTBinaryReader::readU8(p);
    AOTExprKind ek = static_cast<AOTExprKind>(ekind);

    if (ek == AOTExprKind::HASH_DEREF) {
        skipOneExpr(rdr, p, e);  // left (base expression)
        skipOneExpr(rdr, p, e);  // right (key expression)
        return;
    }
    if (ek == AOTExprKind::PLUS || ek == AOTExprKind::SQUARE_BRACKET
            || ek == AOTExprKind::MULTIPLY || ek == AOTExprKind::DIVIDE
            || ek == AOTExprKind::MODULO || ek == AOTExprKind::PUSH
            || ek == AOTExprKind::UNSHIFT || ek == AOTExprKind::NULL_COAL
            || ek == AOTExprKind::VALUE_COAL || ek == AOTExprKind::FOLDL
            || ek == AOTExprKind::FOLDR || ek == AOTExprKind::MAP
            || ek == AOTExprKind::SELECT || ek == AOTExprKind::RANGE
            || ek == AOTExprKind::ASSIGN || ek == AOTExprKind::LOG_AND
            || ek == AOTExprKind::LOG_OR || ek == AOTExprKind::BIT_AND
            || ek == AOTExprKind::BIT_OR || ek == AOTExprKind::BIT_XOR
            || ek == AOTExprKind::SHIFT_LEFT || ek == AOTExprKind::SHIFT_RIGHT
            || ek == AOTExprKind::PLUS_EQ || ek == AOTExprKind::MINUS_EQ
            || ek == AOTExprKind::MULTIPLY_EQ || ek == AOTExprKind::DIVIDE_EQ
            || ek == AOTExprKind::MODULO_EQ || ek == AOTExprKind::AND_EQ
            || ek == AOTExprKind::OR_EQ || ek == AOTExprKind::XOR_EQ
            || ek == AOTExprKind::SHL_EQ || ek == AOTExprKind::SHR_EQ) {
        skipOneExpr(rdr, p, e);  // left operand
        skipOneExpr(rdr, p, e);  // right operand
        return;
    }
    if (ek == AOTExprKind::ITERATE) {
        skipOneExpr(rdr, p, e);
        return;
    }
    if (ek == AOTExprKind::STREAMING) {
        if (p < e) {
            ++p;  // streaming operator kind
        }
        skipOneExpr(rdr, p, e);
        skipOneExpr(rdr, p, e);
        return;
    }
    if (ek == AOTExprKind::SQUARE_BRACKET_RANGE) {
        skipOneExpr(rdr, p, e);  // source expression
        skipOneExpr(rdr, p, e);  // start expression
        skipOneExpr(rdr, p, e);  // stop expression
        return;
    }
    if (ek == AOTExprKind::MAP_SELECT || ek == AOTExprKind::HASH_MAP_OP) {
        skipOneExpr(rdr, p, e);
        skipOneExpr(rdr, p, e);
        skipOneExpr(rdr, p, e);
        return;
    }
    if (ek == AOTExprKind::HASH_MAP_SELECT_OP) {
        skipOneExpr(rdr, p, e);
        skipOneExpr(rdr, p, e);
        skipOneExpr(rdr, p, e);
        skipOneExpr(rdr, p, e);
        return;
    }
    if (ek == AOTExprKind::PARSE_REF) {
        if ((rdr.getHeader().feature_flags & QORE_AOT_FEAT_PARSE_REF_TYPE) != 0) {
            rdr.readStringRef(p);  // reference type path
        }
        skipOneExpr(rdr, p, e);  // inner lvalue expression
        return;
    }
    if (ek == AOTExprKind::EXISTS) {
        skipOneExpr(rdr, p, e);  // operand
        return;
    }
    if (ek == AOTExprKind::IMPLICIT_ARG) {
        (void)QoreAOTBinaryReader::readI64(p);  // offset
        return;
    }
    if (ek == AOTExprKind::KEYS) {
        skipOneExpr(rdr, p, e);  // operand
        return;
    }
    if (ek == AOTExprKind::IMPLICIT_ELEM) {
        return;
    }
    if (ek == AOTExprKind::INSTANCEOF) {
        rdr.readStringRef(p);  // type path
        skipOneExpr(rdr, p, e);  // operand
        return;
    }
    if (ek == AOTExprKind::REGEX_MATCH || ek == AOTExprKind::REGEX_NMATCH
            || ek == AOTExprKind::REGEX_EXTRACT) {
        rdr.readStringRef(p);  // pattern
        (void)QoreAOTBinaryReader::readI64(p);  // options
        skipOneExpr(rdr, p, e);  // operand
        return;
    }
    if (ek == AOTExprKind::PRE_INC || ek == AOTExprKind::PRE_DEC
            || ek == AOTExprKind::POST_INC || ek == AOTExprKind::POST_DEC) {
        skipOneExpr(rdr, p, e);  // lvalue operand
        return;
    }
    if (ek == AOTExprKind::LOG_EQ || ek == AOTExprKind::LOG_NE
            || ek == AOTExprKind::LOG_LT || ek == AOTExprKind::LOG_GT
            || ek == AOTExprKind::LOG_LE || ek == AOTExprKind::LOG_GE) {
        skipOneExpr(rdr, p, e);  // left operand
        skipOneExpr(rdr, p, e);  // right operand
        return;
    }
    if (ek == AOTExprKind::LOG_NOT || ek == AOTExprKind::UNARY_MINUS) {
        skipOneExpr(rdr, p, e);  // operand
        return;
    }
    if (ek == AOTExprKind::QUESTION) {
        skipOneExpr(rdr, p, e);  // condition
        skipOneExpr(rdr, p, e);  // true expression
        skipOneExpr(rdr, p, e);  // false expression
        return;
    }
    if (ek == AOTExprKind::TRIM || ek == AOTExprKind::CHOMP
            || ek == AOTExprKind::POP || ek == AOTExprKind::SHIFT
            || ek == AOTExprKind::ELEMENTS || ek == AOTExprKind::DELETE
            || ek == AOTExprKind::REMOVE || ek == AOTExprKind::BACKGROUND) {
        skipOneExpr(rdr, p, e);  // operand
        return;
    }
    if (ek == AOTExprKind::CONTEXT_REF) {
        rdr.readStringRef(p);  // member
        return;
    }
    if (ek == AOTExprKind::CONTEXT_ROW) {
        return;
    }
    if (ek == AOTExprKind::COMPLEX_CONTEXT_REF) {
        rdr.readStringRef(p);  // context name
        rdr.readStringRef(p);  // member
        (void)QoreAOTBinaryReader::readI64(p);  // stack offset
        return;
    }
    if (ek == AOTExprKind::MINUS) {
        skipOneExpr(rdr, p, e);  // left operand
        skipOneExpr(rdr, p, e);  // right operand
        return;
    }
    if (ek == AOTExprKind::HASH_LITERAL) {
        uint8_t n = QoreAOTBinaryReader::readU8(p);
        for (uint8_t i = 0; i < n; ++i) {
            rdr.readStringRef(p);  // key
            skipOneExpr(rdr, p, e);  // value (recursive)
        }
        return;
    }
    if (ek == AOTExprKind::PARSE_HASH) {
        uint8_t n = QoreAOTBinaryReader::readU8(p);
        for (uint8_t i = 0; i < n; ++i) {
            skipOneExpr(rdr, p, e);  // key expression
            skipOneExpr(rdr, p, e);  // value expression
        }
        return;
    }
    if (ek == AOTExprKind::LIST_LITERAL) {
        uint8_t n = QoreAOTBinaryReader::readU8(p);
        for (uint8_t i = 0; i < n; ++i) {
            skipOneExpr(rdr, p, e);  // each element (recursive)
        }
        return;
    }
    if (ek == AOTExprKind::NEW_OBJECT || ek == AOTExprKind::SCOPED_NEW_OBJECT) {
        rdr.readStringRef(p);  // class path
        if ((rdr.getHeader().feature_flags & QORE_AOT_FEAT_NEW_OBJECT_TYPEINFO) != 0) {
            rdr.readStringRef(p);  // instantiated object type path
        }
        uint8_t na = QoreAOTBinaryReader::readU8(p);
        for (uint8_t i = 0; i < na; ++i) {
            skipOneExpr(rdr, p, e);  // each arg
        }
        return;
    }
    if (ek == AOTExprKind::CALLREF_CALL) {
        skipOneExpr(rdr, p, e);  // callee expression
        uint8_t na = QoreAOTBinaryReader::readU8(p);
        for (uint8_t i = 0; i < na; ++i) {
            skipOneExpr(rdr, p, e);  // each arg
        }
        return;
    }
    // STATIC_METHOD_CALL: ref1(stringref) + ref2(stringref) + optional receiver type + num_args(u8)
    // + N×skipOneExpr
    if (ek == AOTExprKind::STATIC_METHOD_CALL) {
        rdr.readStringRef(p);  // class path
        rdr.readStringRef(p);  // method name
        if ((rdr.getHeader().feature_flags & QORE_AOT_FEAT_STATIC_CALL_RECEIVER_TYPE) != 0) {
            rdr.readStringRef(p);  // receiver type path
        }
        uint8_t na = QoreAOTBinaryReader::readU8(p);
        for (uint8_t i = 0; i < na; ++i) {
            skipOneExpr(rdr, p, e);  // each arg
        }
        return;
    }
    if (ek == AOTExprKind::SELF_METHOD_CALL) {
        rdr.readStringRef(p);
        rdr.readStringRef(p);
        if ((rdr.getHeader().feature_flags & QORE_AOT_FEAT_SELF_CALL_ARGS) != 0) {
            uint8_t na = QoreAOTBinaryReader::readU8(p);
            for (uint8_t i = 0; i < na; ++i) {
                skipOneExpr(rdr, p, e);
            }
        }
        return;
    }
    // Two-stringref kinds
    if (ek == AOTExprKind::STATIC_VARREF || ek == AOTExprKind::CONST_ENUM) {
        rdr.readStringRef(p);
        rdr.readStringRef(p);
        return;
    }
    // HASHDECL_NEW: stringref + u8 num_args + N×classifyAndWriteExpr-encoded args
    if (ek == AOTExprKind::HASHDECL_NEW) {
        rdr.readStringRef(p);  // hashdecl path
        uint8_t na = QoreAOTBinaryReader::readU8(p);
        for (uint8_t i = 0; i < na; ++i) {
            skipOneExpr(rdr, p, e);  // each arg
        }
        return;
    }
    // COMPLEX_HASH_NEW/COMPLEX_LIST_NEW: stringref + u8 num_args + N×encoded args
    // COMPLEX_BUFFER_NEW recursive expressions always carry an init-kind byte.
    if (ek == AOTExprKind::COMPLEX_HASH_NEW || ek == AOTExprKind::COMPLEX_LIST_NEW
            || ek == AOTExprKind::COMPLEX_BUFFER_NEW) {
        rdr.readStringRef(p);  // type path
        if (ek == AOTExprKind::COMPLEX_BUFFER_NEW) {
            QoreAOTBinaryReader::readU8(p);  // QoreComplexBufferInitKind
        }
        uint8_t na = QoreAOTBinaryReader::readU8(p);
        for (uint8_t i = 0; i < na; ++i) {
            skipOneExpr(rdr, p, e);  // each arg
        }
        return;
    }
    // CLOSURE_CREATE: flags(stringref) + class_type_path(stringref)
    //   + optional native_body_key(stringref) + return_type(stringref)
    //   + num_params(u16) + params + varargs flags + num_captured(u16) + captured
    //   + has_ir(u8) + [ir_size(u32) + ir_data]
    if (ek == AOTExprKind::CLOSURE_CREATE) {
        rdr.readStringRef(p);  // flags
        rdr.readStringRef(p);  // class_type_path
        if ((rdr.getHeader().feature_flags & QORE_AOT_FEAT_NATIVE_CLOSURE_BODY) != 0) {
            rdr.readStringRef(p);  // native body key
        }
        rdr.readStringRef(p);  // return type
        uint16_t np = QoreAOTBinaryReader::readU16(p);
        for (uint16_t i = 0; i < np; ++i) {
            rdr.readStringRef(p);  // param name
            rdr.readStringRef(p);  // param type
            uint8_t hd = QoreAOTBinaryReader::readU8(p);
            if (hd) {
                std::string val_error;
                QoreValue dv = rdr.readValue(p, e, val_error);
                dv.discard(nullptr);
            }
        }
        if ((rdr.getHeader().feature_flags & QORE_AOT_FEAT_CLOSURE_VARARGS_FLAGS) != 0) {
            QoreAOTBinaryReader::readU16(p);  // closure flags
        } else {
            QoreAOTBinaryReader::readU8(p);  // legacy varargs
        }
        uint16_t nc = QoreAOTBinaryReader::readU16(p);
        for (uint16_t i = 0; i < nc; ++i) {
            rdr.readStringRef(p);  // captured name
            QoreAOTBinaryReader::readU32(p);  // parent slot
        }
        uint8_t has_ir = QoreAOTBinaryReader::readU8(p);
        if (has_ir) {
            uint32_t ir_size = QoreAOTBinaryReader::readU32(p);
            p += ir_size;  // skip IR data
        }
        return;
    }
    // Inline FUNC_CALL: function name + optional variant signature + optional serialized args.
    // Expression slots use a separate compact slot payload and are not skipped here.
    if (ek == AOTExprKind::FUNC_CALL) {
        rdr.readStringRef(p);
        if ((rdr.getHeader().feature_flags & QORE_AOT_FEAT_FUNC_CALL_VARIANT) != 0) {
            rdr.readStringRef(p);
        }
        if ((rdr.getHeader().feature_flags & QORE_AOT_FEAT_INLINE_CALL_ARGS) != 0) {
            uint8_t na = QoreAOTBinaryReader::readU8(p);
            for (uint8_t i = 0; i < na; ++i) {
                skipOneExpr(rdr, p, e);
            }
        }
        return;
    }
    // One-stringref kinds
    if (ek == AOTExprKind::RUNTIME_CONST_REF
            || ek == AOTExprKind::CONST_NUMBER || ek == AOTExprKind::CONST_BINARY
            || ek == AOTExprKind::CONST_STRING || ek == AOTExprKind::SELF_VARREF
            || ek == AOTExprKind::LOCAL_VARREF || ek == AOTExprKind::GLOBAL_VARREF) {
        rdr.readStringRef(p);
        return;
    }
    // Cast kinds: stringref + u8 or_nothing + u8 has_inner + optional inner expression
    if (ek == AOTExprKind::CAST_HASHDECL || ek == AOTExprKind::CAST_COMPLEX_HASH
            || ek == AOTExprKind::CAST_COMPLEX_LIST || ek == AOTExprKind::CAST_CLASS
            || ek == AOTExprKind::CAST_ENUM || ek == AOTExprKind::CAST_SCALAR) {
        rdr.readStringRef(p);
        QoreAOTBinaryReader::readU8(p);
        uint8_t has_inner = QoreAOTBinaryReader::readU8(p);
        if (has_inner) {
            skipOneExpr(rdr, p, e);
        }
        return;
    }
    // Inline constants
    if (ek == AOTExprKind::CONST_INT) {
        p += 8;
        return;
    }
    if (ek == AOTExprKind::CONST_FLOAT) {
        p += 8;
        return;
    }
    if (ek == AOTExprKind::CONST_BOOL) {
        QoreAOTBinaryReader::readU8(p);
        return;
    }
    if (ek == AOTExprKind::CONST_VALUE) {
        std::string value_error;
        QoreValue v = rdr.readValue(p, e, value_error);
        v.discard(nullptr);
        return;
    }
    // CONST_NOTHING, GENERIC_EVAL or unknown: no bytes to skip
}

// Forward declaration for use by expression registry handlers (Phase 3.2)
QoreValue readOneExpr(
        const QoreAOTBinaryReader& rdr, const uint8_t*& p, const uint8_t* e,
        std::string& err, QoreProgram* pgm,
        LocalVar** locals, int num_locals,
        Var** globals, int num_globals,
        QoreProgram* local_owner_pgm) {
    uint8_t ekind = QoreAOTBinaryReader::readU8(p);
    AOTExprKind ek = static_cast<AOTExprKind>(ekind);

    // Dispatch via registry
    const auto* kinfo = getAOTExprKindInfo(ekind);
    if (!kinfo || !kinfo->is_supported || !kinfo->read_fn) {
        err = "unsupported expression kind " + std::to_string(ekind);
        return QoreValue();
    }
    AOTExprReadCtx rctx{
        rdr, p, e, pgm, err, locals, num_locals, globals, num_globals, local_owner_pgm
    };
    return kinfo->read_fn(rctx);
}

QoreValue readOneTopLevelIRExpr(
        const QoreAOTBinaryReader& rdr, const uint8_t*& p, const uint8_t* e,
        std::string& err, QoreProgram* pgm,
        LocalVar** locals, int num_locals,
        Var** globals, int num_globals,
        QoreProgram* local_owner_pgm) {
    const uint8_t* start = p;
    uint8_t kind_byte = QoreAOTBinaryReader::readU8(p);
    if (static_cast<AOTExprKind>(kind_byte) == AOTExprKind::GENERIC_EVAL) {
        return QoreValue();
    }
    p = start;
    return readOneExpr(rdr, p, e, err, pgm, locals, num_locals, globals, num_globals,
        local_owner_pgm);
}

// ---- Expression Tree Deserializer ----

//! Deserializes an AOT EXPR_TREE binary blob back into AST nodes
class ExprTreeDeserializer {
    const uint8_t* ptr;
    const uint8_t* end;
    QoreProgram* pgm;
    QoreAOTContext* ctx;
    bool failed = false;

    uint8_t readU8() {
        if (ptr >= end) {
            return 0;
        }
        return *ptr++;
    }

    uint16_t readU16() {
        if (ptr + 2 > end) {
            return 0;
        }
        uint16_t v = ptr[0] | (static_cast<uint16_t>(ptr[1]) << 8);
        ptr += 2;
        return v;
    }

    uint32_t readU32() {
        if (ptr + 4 > end) {
            return 0;
        }
        uint32_t v = ptr[0] | (static_cast<uint32_t>(ptr[1]) << 8)
            | (static_cast<uint32_t>(ptr[2]) << 16) | (static_cast<uint32_t>(ptr[3]) << 24);
        ptr += 4;
        return v;
    }

    int32_t readI32() {
        if (ptr + 4 > end) {
            return 0;
        }
        uint32_t v = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
        ptr += 4;
        int32_t r;
        memcpy(&r, &v, sizeof(r));
        return r;
    }

    int64_t readI64() {
        if (ptr + 8 > end) {
            return 0;
        }
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) {
            v |= static_cast<uint64_t>(ptr[i]) << (i * 8);
        }
        ptr += 8;
        int64_t r;
        memcpy(&r, &v, sizeof(r));
        return r;
    }

    double readF64() {
        if (ptr + 8 > end) {
            return 0.0;
        }
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) {
            v |= static_cast<uint64_t>(ptr[i]) << (i * 8);
        }
        ptr += 8;
        double r;
        memcpy(&r, &v, sizeof(r));
        return r;
    }

    std::string readStr() {
        uint16_t len = readU16();
        if (len == 0 || ptr + len > end) {
            if (len > 0) {
                failed = true;
                ptr = end; // mark exhausted
            }
            return {};
        }
        std::string s(reinterpret_cast<const char*>(ptr), len);
        ptr += len;
        return s;
    }

    //! Mark deserialization as failed and return nothing
    QoreValue fail() {
        failed = true;
        return QoreValue();
    }

    //! Resolve a class by name using the program's namespace tree
    const QoreClass* resolveClass(const std::string& name) {
        if (name.empty()) {
            return nullptr;
        }
        return qore_aot_resolve_class_ref(pgm, name.c_str(), false);
    }

    //! Deserialize a single QoreValue from the blob
    /** Returns QoreValue(). On error (corrupted data), returns nothing.
    */
    QoreValue deserializeValue() {
        if (ptr >= end) {
            return QoreValue();
        }

        uint8_t kind_byte = readU8();
        const auto* kinfo = getAOTExprNodeKindInfo(kind_byte);
        if (!kinfo || !kinfo->is_supported || !kinfo->read_fn) {
            printd(0, "AOT EXPR_TREE: unknown node kind %d\n", (int)kind_byte);
            failed = true;
            return QoreValue();
        }
        AOTExprNodeReadCtx rctx{ptr, end, pgm, ctx, failed,
            [this](AOTExprNodeReadCtx&) -> QoreValue { return deserializeValue(); }};
        return kinfo->read_fn(rctx);
    }

public:
    ExprTreeDeserializer(const uint8_t* data, uint32_t size, QoreProgram* p, QoreAOTContext* c)
        : ptr(data), end(data + size), pgm(p), ctx(c) {
    }

    //! Deserialize an expression tree from the blob and return NaN-boxed bits
    uint64_t deserialize() {
        QoreValue v = deserializeValue();
        if (failed) {
            v.discard(nullptr);
            return 0;
        }
        return toBitsNB(v);
    }

    //! Returns true if deserialization failed (distinct from a successful NOTHING result)
    bool hasFailed() const { return failed; }
};

QoreValue deserializeExprTreeFromBlob(const uint8_t* data, uint32_t size, QoreProgram* pgm,
        LocalVar** locals, int num_locals) {
    // Build a minimal QoreAOTContext for the deserializer with just local var slots.
    // We borrow the locals pointer; must null it before ctx destructor runs (which frees it).
    QoreAOTContext ctx;
    ctx.pgm = pgm;
    ctx.locals = locals;
    ctx.num_locals = num_locals;
    ExprTreeDeserializer deser(data, size, pgm, &ctx);
    uint64_t bits = deser.deserialize();
    // Null out borrowed pointer before destructor frees it
    ctx.locals = nullptr;
    ctx.num_locals = 0;
    if (!bits) {
        return QoreValue();
    }
    QoreValue v;
    memcpy(&v, &bits, sizeof(v));
    return v;
}

// Forward declaration for hybrid closure resolution
static QoreAOTContext* buildContextForVariant(UserVariantBase* uvb, const char* name,
        QoreProgram* pgm, const QoreAOTFunc& aot_func);
static void finalizeDeserializedDebugIR(QoreIRFunction& ir, QoreProgram* pgm);

struct QoreAOTDebugMetadata {
    std::vector<uint8_t> metadata;

    QoreAOTDebugMetadata(const QoreAOTBinaryReader& reader, const uint8_t* data, uint32_t size) {
        if (reader.getHeader().compression != QORE_AOT_COMPRESSION_NONE) {
            metadata.assign(data, data + size);
            return;
        }

        std::vector<const QoreAOTSectionHeader*> retained_sections;
        auto retainSection = [&reader, &retained_sections](QoreAOTSectionType type) {
            if (const QoreAOTSectionHeader* sec = reader.findSection(type)) {
                retained_sections.push_back(sec);
            }
        };
        retainSection(QoreAOTSectionType::SLOT_MAPS);
        retainSection(QoreAOTSectionType::DEBUG_IR);
        // Serialized plugin values resolve their module through this section.
        retainSection(QoreAOTSectionType::PLUGIN_IMPORTS);

        uint64_t pool_size_offset = QORE_AOT_HEADER_SIZE
            + static_cast<uint64_t>(reader.getSectionCount()) * sizeof(QoreAOTSectionHeader);
        if (pool_size_offset > size || size - pool_size_offset < sizeof(uint32_t)) {
            metadata.assign(data, data + size);
            return;
        }
        const uint8_t* pool_size_ptr = data + pool_size_offset;
        const uint8_t* pool_size_read_ptr = pool_size_ptr;
        uint32_t pool_size = QoreAOTBinaryReader::readU32(pool_size_read_ptr);
        uint64_t pool_end_offset = pool_size_offset + sizeof(uint32_t) + pool_size;
        if (pool_end_offset > size) {
            metadata.assign(data, data + size);
            return;
        }

        uint64_t compact_size = QORE_AOT_HEADER_SIZE + sizeof(uint32_t) + pool_size
            + retained_sections.size() * sizeof(QoreAOTSectionHeader);
        for (const QoreAOTSectionHeader* sec : retained_sections) {
            compact_size += sec->size;
        }
        if (compact_size >= size || compact_size > UINT32_MAX) {
            metadata.assign(data, data + size);
            return;
        }

        metadata.reserve(static_cast<size_t>(compact_size));
        metadata.insert(metadata.end(), data, data + QORE_AOT_HEADER_SIZE);
        uint32_t section_count = static_cast<uint32_t>(retained_sections.size());
        metadata[16] = static_cast<uint8_t>(section_count);
        metadata[17] = static_cast<uint8_t>(section_count >> 8);
        metadata[18] = static_cast<uint8_t>(section_count >> 16);
        metadata[19] = static_cast<uint8_t>(section_count >> 24);

        auto writeU16 = [this](uint16_t value) {
            metadata.push_back(static_cast<uint8_t>(value));
            metadata.push_back(static_cast<uint8_t>(value >> 8));
        };
        auto writeU32 = [this](uint32_t value) {
            metadata.push_back(static_cast<uint8_t>(value));
            metadata.push_back(static_cast<uint8_t>(value >> 8));
            metadata.push_back(static_cast<uint8_t>(value >> 16));
            metadata.push_back(static_cast<uint8_t>(value >> 24));
        };
        uint32_t offset = 0;
        for (const QoreAOTSectionHeader* sec : retained_sections) {
            writeU16(sec->type);
            writeU16(0);
            writeU32(offset);
            writeU32(sec->size);
            offset += sec->size;
        }
        metadata.insert(metadata.end(), pool_size_ptr, pool_size_read_ptr + pool_size);
        for (const QoreAOTSectionHeader* sec : retained_sections) {
            const uint8_t* section_data = reader.getSectionData(*sec);
            if (!section_data) {
                metadata.assign(data, data + size);
                return;
            }
            metadata.insert(metadata.end(), section_data, section_data + sec->size);
        }
        assert(metadata.size() == compact_size);
    }
};

struct QoreAOTLazyClosureIR {
    std::shared_ptr<const QoreAOTDebugMetadata> metadata;
    uint32_t ir_offset = 0;
    uint32_t ir_size = 0;
    QoreProgram* pgm = nullptr;
    QoreProgram* local_owner_pgm = nullptr;
    std::vector<LocalVar*> parent_locals;
    std::vector<Var*> globals;
    std::unordered_map<std::string, LocalVar*> enclosing_locals;
    std::vector<LocalVar*> body_locals;
};

struct QoreAOTLazyFunctionIR {
    std::shared_ptr<const QoreAOTDebugMetadata> metadata;
    uint32_t slot_entry_offset = 0;
    QoreProgram* pgm = nullptr;
    const qore_class_private* class_ctx = nullptr;
    std::string name;
};

static std::shared_ptr<const QoreAOTDebugMetadata> makeAOTDebugMetadata(
        const QoreAOTBinaryReader& reader, const uint8_t* metadata, int metadata_len) {
    if ((reader.getHeader().feature_flags
            & (QORE_AOT_FEAT_DEBUG_IR | QORE_AOT_FEAT_NATIVE_CLOSURE_BODY)) == 0
            || !metadata || metadata_len <= 0) {
        return nullptr;
    }
    auto retained = std::make_shared<QoreAOTDebugMetadata>(
        reader, metadata, static_cast<uint32_t>(metadata_len));
    if (std::getenv("QORE_AOT_TRACE_RETAINED_METADATA")) {
        fprintf(stderr,
            "[aot-retained-metadata] label=%s blob=%d retained=%zu compression=%u\n",
            reader.getLabel() ? reader.getLabel() : "<unknown>", metadata_len,
            retained->metadata.size(), reader.getHeader().compression);
    }
    return retained;
}

static std::unique_ptr<QoreIRFunction> deserializeDebugIRForContext(
        const QoreAOTBinaryReader& reader, const uint8_t* debug_ir_start,
        const uint8_t* debug_ir_end, QoreAOTContext* ctx, const char* name,
        std::string& error) {
    std::unordered_map<std::string, LocalVar*> debug_local_map;
    for (auto* lv : ctx->all_body_locals) {
        if (lv && lv->getName() && *lv->getName()) {
            debug_local_map[lv->getName()] = lv;
            std::string tpath = getAOTTypePathForLValue(lv->getTypeInfoForLValue());
            if (!tpath.empty()) {
                std::string ck(lv->getName());
                ck += '\x1f';
                ck += tpath;
                debug_local_map[ck] = lv;
            }
        }
    }
    for (int i = 0; i < ctx->num_locals; ++i) {
        LocalVar* lv = ctx->locals[i];
        if (lv && lv->getName() && *lv->getName()) {
            debug_local_map[lv->getName()] = lv;
            std::string tpath = getAOTTypePathForLValue(lv->getTypeInfoForLValue());
            if (!tpath.empty()) {
                std::string ck(lv->getName());
                ck += '\x1f';
                ck += tpath;
                debug_local_map.emplace(std::move(ck), lv);
            }
        }
    }

    auto readExprCb = [ctx](const QoreAOTBinaryReader& rdr, const uint8_t*& p,
            const uint8_t* e, std::string& err) -> QoreValue {
        uint8_t kind_byte = QoreAOTBinaryReader::readU8(p);
        auto kind = static_cast<AOTExprKind>(kind_byte);
        if (kind == AOTExprKind::GENERIC_EVAL) {
            return QoreValue();
        }
        if (kind == AOTExprKind::EXPR_TREE) {
            uint32_t blob_size = QoreAOTBinaryReader::readU32(p);
            const uint8_t* blob_data = p;
            p += blob_size;
            ExprTreeDeserializer deser(blob_data, blob_size, ctx->pgm, ctx);
            uint64_t bits = deser.deserialize();
            if (bits) {
                QoreValue v;
                memcpy(&v, &bits, sizeof(v));
                return v;
            }
            return QoreValue();
        }
        --p;
        return readOneExpr(rdr, p, e, err, ctx->pgm,
            ctx->locals, ctx->num_locals, ctx->globals, ctx->num_globals,
            ctx->local_owner_pgm);
    };

    const uint8_t* ptr = debug_ir_start;
    std::unique_ptr<QoreIRFunction> debug_ir = deserializeIRFunction(reader, ptr, debug_ir_end,
        ctx->pgm, readExprCb, &debug_local_map, error, ctx->locals, ctx->num_locals,
        nullptr, true, &ctx->all_body_locals, ctx->local_owner_pgm);
    if (!debug_ir) {
        if (error.empty()) {
            error = "debug IR deserialization failed";
        }
        printd(2, "AOT debug IR: '%s' lazy deserialization failed: %s\n",
            name ? name : "<unknown>", error.c_str());
        return nullptr;
    }
    finalizeDeserializedDebugIR(*debug_ir, ctx->pgm);
    return debug_ir;
}

std::unique_ptr<QoreIRFunction> QoreAOTContext::materializeDebugIR(
        const char* name, std::string& error) {
    if (!debug_metadata || !debug_ir_size) {
        error = "no serialized debug IR metadata is available";
        return nullptr;
    }

    QoreAOTBinaryReader reader;
    std::string open_error;
    if (!reader.open(debug_metadata->metadata.data(),
            static_cast<uint32_t>(debug_metadata->metadata.size()), open_error)) {
        error = "metadata open failed: " + open_error;
        return nullptr;
    }

    QoreAOTSectionType section_type = debug_ir_separate_section
        ? QoreAOTSectionType::DEBUG_IR : QoreAOTSectionType::SLOT_MAPS;
    const QoreAOTSectionHeader* sec = reader.findSection(section_type);
    if (!sec) {
        error = debug_ir_separate_section
            ? "metadata has no DEBUG_IR section" : "metadata has no SLOT_MAPS section";
        return nullptr;
    }
    const uint8_t* section_data = reader.getSectionData(*sec);
    if (!section_data) {
        error = debug_ir_separate_section
            ? "metadata has invalid DEBUG_IR section data"
            : "metadata has invalid SLOT_MAPS section data";
        return nullptr;
    }
    if (debug_ir_offset > sec->size || debug_ir_size > sec->size - debug_ir_offset) {
        error = debug_ir_separate_section
            ? "serialized debug IR range exceeds DEBUG_IR section"
            : "serialized debug IR range exceeds SLOT_MAPS section";
        return nullptr;
    }

    const uint8_t* debug_ir_start = section_data + debug_ir_offset;
    const uint8_t* debug_ir_end = debug_ir_start + debug_ir_size;
    return deserializeDebugIRForContext(reader, debug_ir_start, debug_ir_end, this, name, error);
}

//! Build QoreAOTContext from deserialized slot map identities (no IR re-lowering needed)
/** Resolves local/global/expression slot identities by looking up objects
    in the program's namespace tree and the function's UserSignature.
    @param reader the binary reader
    @param func_data pointer to the function's slot data in the binary
    @param func_data_end pointer past end of valid data
    @param uvb the user variant base (provides UserSignature with LocalVar* for params)
    @param pgm the QoreProgram
    @param aot_func the AOT function descriptor (for slot counts)
    @param name the function name (for debug output)
    @return heap-allocated QoreAOTContext, or nullptr on failure
*/
// ---------------------------------------------------------------------------
// Lazy AOT exception source-location registry.
//
// At module load we read the per-artifact PC->loc trailer (see QoreAOTBinary.h),
// resolve each function's (native-offset -> loc-index) entries through the freshly
// built ctx->locs table into (offset -> QoreProgramLocation*), and record the
// function's native base in a global sorted registry. At throw, the raising AOT
// frame's PC is mapped base+offset -> QoreProgramLocation* with no per-line runtime
// cost (replacing the eager updater). Trailer files are parsed once per path.
// ---------------------------------------------------------------------------
namespace {
struct AotPcRange {
    uintptr_t base = 0;     //!< native function start (dladdr dli_saddr)
    uintptr_t end = 0;      //!< exclusive upper bound (set when the registry is sorted)
    uint32_t max_off = 0;   //!< largest mapped function-relative offset
    //! sorted by offset: function-relative native offset -> source location
    std::vector<std::pair<uint32_t, const QoreProgramLocation*>> offmap;
};

QoreThreadLock g_aot_pcmap_lock;  // guards all registry state below
//! One symbol's lazy PC->loc data: the (offset -> loc-index) rows plus the literal locations that
//! indices at or above the function's loc-table size address (code inlined from another function).
struct AotSymEntry {
    std::vector<std::pair<uint32_t, uint32_t>> entries;
    std::vector<AOTCompiledFuncWithSlots::AOTLocEntry> extra_locs;
};
using AotSymMap = std::unordered_map<std::string, AotSymEntry>;
std::unordered_map<std::string, std::shared_ptr<const AotSymMap>> g_aot_trailer_cache;
std::vector<AotPcRange> g_aot_pc_ranges;  //!< kept sorted by base once finalized
bool g_aot_pc_ranges_sorted = true;
//! Lock-free fast-path: true once any AOT PC range is registered. Lets the throw
//! path skip the lock entirely in non-AOT programs (the common case).
std::atomic<bool> g_aot_have_ranges{false};

//! Lazy AOT exception source-location resolution is ON by default (Step 6, Phase A):
//! the registry is built at load and the innermost-frame resolver runs at throw. Opt
//! out with QORE_AOT_LOC_NO_LAZY (falls back to the eager runtime_loc). QORE_AOT_LOC_GATE
//! still forces the build for diagnostic comparison. Validated lazy-active 789/789.
inline bool aotLazyLocEnabled() {
    static const bool enabled = getenv("QORE_AOT_LOC_NO_LAZY") == nullptr
        || getenv("QORE_AOT_LOC_GATE") != nullptr;
    return enabled;
}

// Load + cache the PC->loc trailer for `path`; returns null when absent/garbage.
std::shared_ptr<const AotSymMap> getOrLoadPcLocTrailer(const char* path) {
    std::string key(path);
    {
        AutoLocker al(g_aot_pcmap_lock);
        auto it = g_aot_trailer_cache.find(key);
        if (it != g_aot_trailer_cache.end()) {
            return it->second;
        }
    }
    // Parse outside the lock (file I/O). A concurrent loader may parse the same file
    // too; the data is identical, last writer wins.
    //
    // Prefer the `qore_aot_pcloc` ELF SECTION (survives downstream relinking — e.g.
    // qorus links per-file .qo's into the qorus-core executable, which drops any EOF
    // trailer but keeps the section). Fall back to the EOF trailer for legacy artifacts
    // that predate the section. The section may carry several concatenated records (one
    // per input object linked into the artifact); the reader accumulates all of them.
    std::vector<AOTPcLocFuncEntry> entries;
    std::shared_ptr<AotSymMap> symmap;
    if ((qoreAOTReadPcLocSection(key, entries) && !entries.empty())
            || (qoreAOTReadPcLocTrailer(key, entries) && !entries.empty())) {
        symmap = std::make_shared<AotSymMap>();
        for (auto& e : entries) {
            AotSymEntry se;
            se.entries = std::move(e.entries);
            se.extra_locs = std::move(e.extra_locs);
            (*symmap)[e.symbol] = std::move(se);
        }
    }
    AutoLocker al(g_aot_pcmap_lock);
    auto it = g_aot_trailer_cache.find(key);
    if (it != g_aot_trailer_cache.end()) {
        return it->second;
    }
    g_aot_trailer_cache[key] = symmap;  // cache negatives (null) too
    return symmap;
}

// Cached static-symbol table for an artifact, used to name AOT functions that dladdr
// cannot (functions linked into an EXECUTABLE live only in .symtab, not .dynsym).
struct AotSymtab {
    bool is_et_dyn = false;                  //!< true => runtime addr = fbase + st_value
    std::vector<AOTElfFuncSym> syms;         //!< sorted by value for address lookup
};
std::unordered_map<std::string, std::shared_ptr<const AotSymtab>> g_aot_symtab_cache;

// Load + cache the .symtab FUNC symbols for `path` (sorted by value). Null when absent.
std::shared_ptr<const AotSymtab> getOrLoadSymtab(const char* path) {
    std::string key(path);
    {
        AutoLocker al(g_aot_pcmap_lock);
        auto it = g_aot_symtab_cache.find(key);
        if (it != g_aot_symtab_cache.end()) {
            return it->second;
        }
    }
    std::shared_ptr<AotSymtab> st;
    std::vector<AOTElfFuncSym> syms;
    bool is_dyn = false;
    if ((qoreAOTReadElfFuncSymbols(key, syms, is_dyn)
            || qoreAOTReadMachoFuncSymbols(key, syms, is_dyn)) && !syms.empty()) {
        std::sort(syms.begin(), syms.end(),
            [](const AOTElfFuncSym& a, const AOTElfFuncSym& b) { return a.value < b.value; });
        st = std::make_shared<AotSymtab>();
        st->is_et_dyn = is_dyn;
        st->syms = std::move(syms);
    }
    AutoLocker al(g_aot_pcmap_lock);
    auto it = g_aot_symtab_cache.find(key);
    if (it != g_aot_symtab_cache.end()) {
        return it->second;
    }
    g_aot_symtab_cache[key] = st;  // cache negatives too
    return st;
}
} // anonymous namespace

// Attach the lazy PC->loc map for an AOT function to the global registry, resolving
// its offset->loc-index entries through ctx->locs into offset->QoreProgramLocation*.
// Best-effort: silently no-ops when the artifact carries no trailer (graceful for
// --strip-debug-info / pre-feature builds, where the eager path remains the source).
static void aotAttachPcLocMap(AotFunctionPtr fn_ptr, const QoreAOTContext* ctx) {
    // Skip all lazy-location bookkeeping (dladdr1, trailer parse, registry, segment
    // snapshot) unless the feature is enabled — default builds pay nothing.
    if (!aotLazyLocEnabled()) {
        return;
    }
    if (!fn_ptr || !ctx || !ctx->locs || ctx->num_locs <= 0) {
        return;
    }
    Dl_info info;
    uintptr_t sym_size = 0;
    // Require only fname/fbase: dli_sname/dli_saddr may be ABSENT for functions that
    // exist solely in .symtab (AOT functions linked into an executable like qorus-core
    // are not in .dynsym, which is all dladdr sees) — those are resolved via .symtab.
#ifdef __GLIBC__
    const ElfW(Sym)* sym = nullptr;
    if (!dladdr1(reinterpret_cast<void*>(fn_ptr), &info, reinterpret_cast<void**>(
            const_cast<ElfW(Sym)**>(&sym)), RTLD_DL_SYMENT)
            || !info.dli_fname) {
        return;
    }
    if (sym && sym->st_size) {
        sym_size = static_cast<uintptr_t>(sym->st_size);
    }
#else
    if (!dladdr(reinterpret_cast<void*>(fn_ptr), &info) || !info.dli_fname) {
        return;
    }
#endif
    std::shared_ptr<const AotSymMap> symmap = getOrLoadPcLocTrailer(info.dli_fname);
    if (!symmap) {
        if (getenv("QORE_AOT_LOC_DEBUG")) {
            fprintf(stderr, "AOT-LOC: no map for fname=%s sym=%s\n",
                info.dli_fname ? info.dli_fname : "?", info.dli_sname ? info.dli_sname : "?");
        }
        return;
    }
    // Resolve the function's name (-> map key), runtime start, and size. Prefer the
    // dladdr/.dynsym result; fall back to .symtab when the symbol is absent there or
    // its name isn't in the map (covers executable-resident AOT functions).
    const AotSymEntry* sym_entries = nullptr;
    uintptr_t fn_start = 0;
    uintptr_t fn_size = 0;
    if (info.dli_sname && info.dli_saddr) {
        auto it = symmap->find(info.dli_sname);
        if (it != symmap->end()) {
            sym_entries = &it->second;
            fn_start = reinterpret_cast<uintptr_t>(info.dli_saddr);
            fn_size = sym_size;
        }
    }
    if (sym_entries && !fn_size) {
        // dladdr matched the function but gave no size (macOS: plain dladdr has no
        // st_size). Recover the size from the symbol table so the registered PC range
        // spans the WHOLE function — a return address past the last mapped offset (e.g.
        // the throw call site) must stay in-range or lazy lookup misses and falls back
        // to the function's declaration line.
        std::shared_ptr<const AotSymtab> stab = getOrLoadSymtab(info.dli_fname);
        if (stab && !stab->syms.empty()) {
            uintptr_t bias = stab->is_et_dyn
                ? reinterpret_cast<uintptr_t>(info.dli_fbase) : 0;
            uint64_t link_addr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(fn_ptr) - bias);
            const auto& v = stab->syms;
            size_t lo = 0, hi = v.size();
            while (lo < hi) {
                size_t mid = (lo + hi) / 2;
                if (v[mid].value <= link_addr) { lo = mid + 1; } else { hi = mid; }
            }
            if (lo > 0) {
                const AOTElfFuncSym& e = v[lo - 1];
                if (e.size && link_addr < e.value + e.size) {
                    fn_size = e.size;
                }
            }
        }
    }
    if (!sym_entries) {
        std::shared_ptr<const AotSymtab> stab = getOrLoadSymtab(info.dli_fname);
        if (stab && !stab->syms.empty()) {
            uintptr_t bias = stab->is_et_dyn
                ? reinterpret_cast<uintptr_t>(info.dli_fbase) : 0;
            uint64_t link_addr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(fn_ptr) - bias);
            // Largest st_value <= link_addr, then bounds-check against its size.
            const auto& v = stab->syms;
            size_t lo = 0, hi = v.size();
            while (lo < hi) {
                size_t mid = (lo + hi) / 2;
                if (v[mid].value <= link_addr) { lo = mid + 1; } else { hi = mid; }
            }
            if (lo > 0) {
                const AOTElfFuncSym& e = v[lo - 1];
                if (link_addr < e.value + e.size) {
                    auto it = symmap->find(e.name);
                    if (it != symmap->end()) {
                        sym_entries = &it->second;
                        fn_start = bias + e.value;
                        fn_size = e.size;
                    }
                }
            }
        }
    }
    if (!sym_entries) {
        if (getenv("QORE_AOT_LOC_DEBUG")) {
            fprintf(stderr, "AOT-LOC: sym MISS fname=%s dladdr-sym=%s (map has %zu syms)\n",
                info.dli_fname ? info.dli_fname : "?", info.dli_sname ? info.dli_sname : "?",
                symmap->size());
        }
        return;
    }
    AotPcRange range;
    range.base = fn_start;
    if (fn_size) {
        range.end = range.base + fn_size;
    }
    range.offmap.reserve(sym_entries->entries.size());
    // Locations for inlined code are carried literally by the map and addressed with indices at or
    // above num_locs; materialize each one once, owned by the context (see QoreAOTContext::
    // pc_extra_locs).  A blob written before that block existed simply has none, and an index past
    // the end is dropped rather than resolved to something unrelated.
    QoreAOTContext* mctx = const_cast<QoreAOTContext*>(ctx);
    std::vector<const QoreProgramLocation*> materialized(sym_entries->extra_locs.size(), nullptr);
    for (const auto& e : sym_entries->entries) {
        const QoreProgramLocation* loc = nullptr;
        if (e.second < static_cast<uint32_t>(ctx->num_locs)) {
            loc = ctx->locs[e.second];
        } else {
            const size_t xi = e.second - static_cast<uint32_t>(ctx->num_locs);
            if (xi >= sym_entries->extra_locs.size()) {
                continue;
            }
            if (!materialized[xi]) {
                const auto& x = sym_entries->extra_locs[xi];
                if (x.start_line <= 0) {
                    continue;
                }
                mctx->pc_extra_files.push_back(x.file);
                auto* nl = new QoreProgramLocation(mctx->pc_extra_files.back().c_str(),
                    x.start_line, x.end_line);
                mctx->pc_extra_locs.push_back(nl);
                materialized[xi] = nl;
            }
            loc = materialized[xi];
        }
        if (loc) {
            range.offmap.emplace_back(e.first, loc);
            if (e.first > range.max_off) {
                range.max_off = e.first;
            }
        }
    }
    if (range.offmap.empty()) {
        return;
    }
    std::sort(range.offmap.begin(), range.offmap.end(),
        [](const std::pair<uint32_t, const QoreProgramLocation*>& a,
           const std::pair<uint32_t, const QoreProgramLocation*>& b) {
            return a.first < b.first;
        });
    AutoLocker al(g_aot_pcmap_lock);
    g_aot_pc_ranges.push_back(std::move(range));
    g_aot_pc_ranges_sorted = false;
    g_aot_have_ranges.store(true, std::memory_order_release);
    if (getenv("QORE_AOT_LOC_DEBUG")) {
        fprintf(stderr, "AOT-LOC: registered PC range base=0x%lx sym=%s entries=%zu (total=%zu)\n",
            (unsigned long)fn_start, info.dli_sname ? info.dli_sname : "(symtab)",
            g_aot_pc_ranges.back().offmap.size(),
            g_aot_pc_ranges.size());
    }
}

namespace {
// Sort the registry by base and fill any missing upper bounds. Call with the lock
// held. A range whose precise size was unavailable (no ELF symbol size) is bounded
// by the next range's base, falling back to its last mapped offset for the final one.
void finalizeAotPcRangesLocked() {
    if (g_aot_pc_ranges_sorted) {
        return;
    }
    std::sort(g_aot_pc_ranges.begin(), g_aot_pc_ranges.end(),
        [](const AotPcRange& a, const AotPcRange& b) { return a.base < b.base; });
    for (size_t i = 0; i < g_aot_pc_ranges.size(); ++i) {
        AotPcRange& r = g_aot_pc_ranges[i];
        if (r.end > r.base) {
            continue;  // precise size already known
        }
        uintptr_t next = (i + 1 < g_aot_pc_ranges.size())
            ? g_aot_pc_ranges[i + 1].base : 0;
        uintptr_t loose = r.base + r.max_off + 64;
        r.end = next > r.base ? std::min(next, loose) : loose;
    }
    g_aot_pc_ranges_sorted = true;
}

// Map a native PC to its source location via the registry, or nullptr if the PC is
// not within a known AOT function.
const QoreProgramLocation* aotLookupLocForPc(uintptr_t pc) {
    AutoLocker al(g_aot_pcmap_lock);
    if (g_aot_pc_ranges.empty()) {
        return nullptr;
    }
    finalizeAotPcRangesLocked();
    // Largest base <= pc.
    size_t lo = 0, hi = g_aot_pc_ranges.size();
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (g_aot_pc_ranges[mid].base <= pc) { lo = mid + 1; } else { hi = mid; }
    }
    if (lo == 0) {
        return nullptr;
    }
    const AotPcRange& r = g_aot_pc_ranges[lo - 1];
    if (pc < r.base || pc >= r.end) {
        return nullptr;
    }
    uint32_t off = static_cast<uint32_t>(pc - r.base);
    // Largest mapped offset <= off.
    size_t olo = 0, ohi = r.offmap.size();
    while (olo < ohi) {
        size_t mid = (olo + ohi) / 2;
        if (r.offmap[mid].first <= off) { olo = mid + 1; } else { ohi = mid; }
    }
    if (olo == 0) {
        return nullptr;
    }
    return r.offmap[olo - 1].second;
}

struct AotUnwindState {
    const QoreProgramLocation* found = nullptr;
    uintptr_t aot_cfa = 0;  //!< CFA of the innermost AOT frame (for the SP comparison)
    int idx = 0;
    int skip = 0;
};
_Unwind_Reason_Code aotUnwindCb(struct _Unwind_Context* uctx, void* arg) {
    AotUnwindState* st = static_cast<AotUnwindState*>(arg);
    uintptr_t ip = static_cast<uintptr_t>(_Unwind_GetIP(uctx));
    if (!ip) {
        return _URC_END_OF_STACK;
    }
    if (st->idx++ < st->skip) {
        return _URC_NO_REASON;
    }
    // _Unwind_GetIP returns the return address (one past the call); look up ip-1 so we
    // land inside the call instruction's line, matching the eager updater. Find the
    // innermost AOT frame; whether it is the innermost USER frame is decided by the SP
    // comparison in the resolver (runtime_loc_sp vs this CFA).
    const QoreProgramLocation* loc = aotLookupLocForPc(ip - 1);
    if (loc) {
        st->found = loc;
        st->aot_cfa = static_cast<uintptr_t>(_Unwind_GetCFA(uctx));
        return _URC_END_OF_STACK;
    }
    return _URC_NO_REASON;
}

} // anonymous namespace

// Resolve the source location of the innermost AOT stack frame for an exception
// being raised on the current thread, or nullptr if no AOT frame is on the stack.
// When QORE_AOT_LOC_GATE is set, compares against `eager` and logs any divergence;
// the comparison phase keeps using the eager value (returns nullptr) so behavior is
// unchanged until the lazy path is proven identical across the suite.
const QoreProgramLocation* qore_aot_resolve_throw_location(const QoreProgramLocation* eager) {
    // Lazy resolution is ON by default (Step 6, Phase A). Opt out with
    // QORE_AOT_LOC_NO_LAZY (returns nullptr -> eager). QORE_AOT_LOC_GATE forces the
    // comparison-only mode: compute + log the decision, but still return eager.
    static const char* gate_env = getenv("QORE_AOT_LOC_GATE");
    static const bool no_lazy = getenv("QORE_AOT_LOC_NO_LAZY") != nullptr;
    if (!gate_env && no_lazy) {
        return nullptr;
    }
    // Lock-free fast-path: no AOT code loaded -> nothing to do (common case).
    if (!g_aot_have_ranges.load(std::memory_order_acquire)) {
        return nullptr;
    }
    AotUnwindState st;
    _Unwind_Backtrace(aotUnwindCb, &st);

    // Decide whether the innermost USER frame is AOT. runtime_loc_sp is the stack-frame
    // address of the innermost live non-AOT (AST/IR/JIT) frame that set runtime_loc; it
    // is 0 while an AOT frame owns the location. Stacks grow down (inner = smaller addr):
    //   - no AOT frame found            -> non-AOT exception, eager is authoritative
    //   - sp == 0                       -> an AOT frame owns the location -> AOT innermost
    //   - aot_cfa < sp                  -> the AOT frame is deeper than the live non-AOT
    //                                      frame -> AOT innermost
    //   - else                          -> a non-AOT frame is innermost -> eager
    // See design/aot-lazy-loc-innermost-frame.md.
    const QoreProgramLocation* lazy = nullptr;
    bool aot_innermost = false;
    if (st.found) {
        uintptr_t sp = get_runtime_loc_sp();
        aot_innermost = (sp == 0) || (st.aot_cfa < sp);
        if (aot_innermost) {
            lazy = st.found;
        }
    }

    if (gate_env) {
        // Report only when an AOT frame is on the stack (otherwise eager is trivially
        // authoritative). The gate logs the eager-vs-lazy divergence tag AND the
        // mechanism's decision so validation can confirm REGRESS? -> DEFER and that
        // IMPROVE cases stay LAZY.
        if (st.found) {
            FILE* gate_out = stderr;
            bool gate_close = false;
            if (strcmp(gate_env, "1") && strcmp(gate_env, "on")) {
                FILE* gf = fopen(gate_env, "a");
                if (gf) { gate_out = gf; gate_close = true; }
            }
            auto fileOf = [](const QoreProgramLocation* l) -> const char* {
                const char* f = l ? l->getFile() : nullptr;
                return f ? f : "";
            };
            const char* ef = fileOf(eager);
            int el = eager ? eager->start_line : -1;
            const char* lf = fileOf(st.found);
            int ll = st.found->start_line;
            bool eager_real = eager && el > 0 && *ef && strcmp(ef, "<builtin>");
            const char* tag = !eager_real ? "IMPROVE"
                : (el == ll && !strcmp(ef, lf)) ? "OK" : "REGRESS?";
            fprintf(gate_out, "AOT-LOC-GATE: %s decision=%s sp=%p aot_cfa=%p eager=%s:%d lazy=%s:%d\n",
                tag, aot_innermost ? "LAZY" : "EAGER",
                reinterpret_cast<void*>(get_runtime_loc_sp()),
                reinterpret_cast<void*>(st.aot_cfa), ef, el, lf, ll);
            if (gate_close) {
                fclose(gate_out);
            }
        }
        return nullptr;  // gate phase: keep eager behavior
    }
    return lazy;
}

namespace {
struct AotCollectState {
    std::vector<const QoreProgramLocation*>* out = nullptr;
};
_Unwind_Reason_Code aotCollectCb(struct _Unwind_Context* uctx, void* arg) {
    AotCollectState* st = static_cast<AotCollectState*>(arg);
    uintptr_t ip = static_cast<uintptr_t>(_Unwind_GetIP(uctx));
    if (!ip) {
        return _URC_END_OF_STACK;
    }
    // ip-1: land inside the call instruction's line (matches the eager updater).
    const QoreProgramLocation* loc = aotLookupLocForPc(ip - 1);
    if (loc) {
        st->out->push_back(loc);
    }
    return _URC_NO_REASON;
}
} // anonymous namespace

size_t qore_aot_collect_backtrace_locs(std::vector<const QoreProgramLocation*>& out) {
    out.clear();
    static const bool no_lazy = getenv("QORE_AOT_LOC_NO_LAZY") != nullptr;
    if (no_lazy) {
        return 0;
    }
    if (!g_aot_have_ranges.load(std::memory_order_acquire)) {
        return 0;
    }
    AotCollectState st;
    st.out = &out;
    _Unwind_Backtrace(aotCollectCb, &st);
    if (getenv("QORE_AOT_LOC_DEBUG") && !out.empty()) {
        fprintf(stderr, "AOT-LOC: backtrace collected %zu AOT frames:\n", out.size());
        for (size_t i = 0; i < out.size(); ++i) {
            const char* f = out[i] ? out[i]->getFile() : nullptr;
            fprintf(stderr, "AOT-LOC:   R[%zu] = %s:%d\n", i, f ? f : "?",
                out[i] ? out[i]->start_line : -1);
        }
    }
    return out.size();
}

struct AOTClosureRuntimeBinding {
    UserVariantBase* uvb = nullptr;
    std::vector<LocalVar*> local_slots;
    const qore_class_private* class_ctx = nullptr;
};

using AOTClosureRuntimeBindingMap =
    std::unordered_map<std::string, AOTClosureRuntimeBinding>;

static QoreAOTContext* buildContextFromSlotMap(
        const QoreAOTBinaryReader& reader,
        const uint8_t*& ptr, const uint8_t* end,
        UserVariantBase* uvb, QoreProgram* pgm,
        const QoreAOTFunc& aot_func, const char* name,
        const uint8_t* entry_end = nullptr,
        QoreAOTTypeResolver* shared_type_resolver = nullptr,
        std::string* build_error = nullptr,
        std::shared_ptr<const QoreAOTDebugMetadata> debug_metadata = nullptr,
        const uint8_t* slot_maps_start = nullptr,
        const qore_class_private* variant_class_ctx = nullptr,
        const std::vector<LocalVar*>* direct_local_slots = nullptr,
        AOTClosureRuntimeBindingMap* closure_bindings = nullptr,
        QoreProgram* local_owner_pgm = nullptr) {
    const uint8_t* entry_payload_start = ptr;
    auto setBuildError = [name, build_error](const std::string& msg) {
        if (build_error && build_error->empty()) {
            *build_error = "AOT slot map registration failed for '";
            *build_error += name ? name : "<unknown>";
            *build_error += "': ";
            *build_error += msg;
        }
    };

    // Slot-resolution detail is only consumed when a function is rejected; a slot that resolves to 0
    // legitimately (CLOSURE_CREATE) leaves it set, so a stale message from an earlier, successfully
    // registered function would otherwise be reported against this one.  Start each function clean.
    aot_slot_resolve_error.clear();

    // Read the per-function slot map header
    // Format: name_ref(u32), num_locals(u16), num_globals(u16), num_exprs(u16),
    //         num_stmts(u16), num_regex_cases(u16), num_body_locals(u16), has_unsupported(u8), padding(u8)
    // Note: caller has already positioned ptr after the 4-byte entry-size prefix
    /*const char* func_name =*/ reader.readStringRef(ptr);
    uint16_t num_locals = QoreAOTBinaryReader::readU16(ptr);
    uint16_t num_globals = QoreAOTBinaryReader::readU16(ptr);
    uint16_t num_exprs = QoreAOTBinaryReader::readU16(ptr);
    uint16_t num_stmts = QoreAOTBinaryReader::readU16(ptr);
    uint16_t num_regex_cases = QoreAOTBinaryReader::readU16(ptr);
    uint16_t num_body_locals = QoreAOTBinaryReader::readU16(ptr);
    uint8_t has_unsupported = QoreAOTBinaryReader::readU8(ptr);
    uint8_t num_lv_path_insts = QoreAOTBinaryReader::readU8(ptr); // was: padding byte
    int aot_num_regex_cases = qore_aot_func_num_regex_cases(aot_func);
    printd(5, "AOT buildCtx '%s': num_lv_path=%d (from binary)\n", name, num_lv_path_insts);
    const char* trace_slot_reg_env = getenv("QORE_AOT_TRACE_SLOT_REG");
    const bool trace_slot_reg = trace_slot_reg_env
        && (!*trace_slot_reg_env || (name && std::strstr(name, trace_slot_reg_env)));
    if (trace_slot_reg) {
        fprintf(stderr, "[aot-slot-reg] buildCtx '%s': binary(locals=%u globals=%u exprs=%u stmts=%u "
            "regex=%u body_locals=%u lvpath=%u unsupported=%u) func(locals=%d globals=%d exprs=%d "
            "stmts=%d regex=%d)\n", name, num_locals, num_globals, num_exprs, num_stmts,
            num_regex_cases, num_body_locals, num_lv_path_insts, has_unsupported,
            aot_func.num_locals, aot_func.num_globals, aot_func.num_exprs, aot_func.num_stmts,
            aot_num_regex_cases);
    }

    if (debug > 1 && has_unsupported) {
        printd(5, "AOT buildCtx: '%s' has_unsupported=1 FROM BINARY (pre-flagged)\n", name);
    }

    // Validate slot counts match the AOT function descriptor
    if (num_locals != aot_func.num_locals || num_globals != aot_func.num_globals
            || num_exprs != aot_func.num_exprs || num_stmts != aot_func.num_stmts
            || num_regex_cases != aot_num_regex_cases) {
        std::string msg = "slot count mismatch: binary("
            + std::to_string(num_locals) + "," + std::to_string(num_globals) + ","
            + std::to_string(num_exprs) + "," + std::to_string(num_stmts) + ","
            + std::to_string(num_regex_cases) + ") vs function descriptor("
            + std::to_string(aot_func.num_locals) + "," + std::to_string(aot_func.num_globals) + ","
            + std::to_string(aot_func.num_exprs) + "," + std::to_string(aot_func.num_stmts) + ","
            + std::to_string(aot_num_regex_cases) + ")";
        setBuildError(msg);
        printd(0, "AOT v2: slot count mismatch for '%s': binary(%d,%d,%d,%d,%d) vs func(%d,%d,%d,%d,%d)\n",
            name, num_locals, num_globals, num_exprs, num_stmts, num_regex_cases,
            aot_func.num_locals, aot_func.num_globals, aot_func.num_exprs, aot_func.num_stmts,
            aot_num_regex_cases);
        if (trace_slot_reg) {
            fprintf(stderr, "[aot-slot-reg] slot count mismatch for '%s'\n", name);
        }
        return nullptr;
    }

    printd(2, "AOT v2: buildContextFromSlotMap '%s': locals=%d globals=%d exprs=%d stmts=%d regex_cases=%d "
        "body_locals=%d has_unsupported=%d uvb=%p\n", name, num_locals, num_globals, num_exprs, num_stmts,
        num_regex_cases, num_body_locals, has_unsupported, (void*)uvb);

    auto* ctx = new QoreAOTContext();
    ctx->pgm = pgm;
    ctx->local_owner_pgm = local_owner_pgm;
    ctx->num_locals = num_locals;
    ctx->num_globals = num_globals;
    ctx->num_exprs = num_exprs;
    ctx->num_stmts = num_stmts;
    ctx->num_regex_cases = num_regex_cases;
    ctx->num_lv_path_insts = num_lv_path_insts;
    ctx->uses_argv = qore_aot_func_uses_argv(aot_func);
    ctx->uses_self = qore_aot_func_uses_self(aot_func);
    ctx->allocate();

    qore_program_private* pp = qore_program_private::get(*pgm);
    qore_program_private* local_pp = local_owner_pgm
        ? qore_program_private::get(*local_owner_pgm) : pp;

    // Get UserSignature for param resolution
    UserSignature* sig = uvb ? uvb->getUserSignature() : nullptr;
    printd(2, "AOT v2: '%s' sig=%p sig->lv.size()=%d\n", name,
        (void*)sig, sig ? (int)sig->lv.size() : -1);

    // For user function variants, collect all statement locals from the AST
    // so we can reuse the same LocalVar* that the compiled code expects.
    // This is critical: JIT code uses pointer identity to find locals on the
    // thread-local variable stack (ThreadLocalVariableData::find()).
    std::vector<LocalVar*> stmt_locals;
    if (uvb) {
        StatementBlock* statements = uvb->getStatementBlock();
        if (statements) {
            collectAllStatementLocals(statements, stmt_locals);
        }
    }
    // Build name→LocalVar* deque map for body local resolution
    // A deque is needed because nested scopes can have variables with the same name
    // (e.g., 'i' in nested for loops). Both AOT serialization and collectAllStatementLocals()
    // walk in the same depth-first order, so consuming front-to-back gives correct matches.
    std::unordered_map<std::string, std::deque<LocalVar*>> stmt_local_deque;
    for (LocalVar* slv : stmt_locals) {
        if (slv && slv->getName()) {
            stmt_local_deque[slv->getName()].push_back(slv);
        }
    }

    // Single type resolver across every local slot in this function.
    // When the caller hands us its own (typically the deserializer
    // session's resolver — already warmed by reading every variant
    // signature's type path), body-local resolutions for common types
    // hit a cache populated across the whole batch.  Falls back to a
    // function-scope resolver when the caller doesn't share one.
    QoreAOTTypeResolver local_type_resolver(pgm);
    QoreAOTTypeResolver* ctx_type_resolver = shared_type_resolver
        ? shared_type_resolver : &local_type_resolver;

    // Top-level `my` locals have program-wide lexical scope and thread-local
    // storage.  Methods and functions can reference them even though they do
    // not appear in that variant's statement-local list.  Reuse the Program
    // LVList entry instead of creating a fresh LocalVar, because local-stack
    // lookup uses LocalVar name pointer identity.
    std::unordered_map<std::string, std::vector<LocalVar*>> top_level_locals;
    if (const LVList* top_lvars = pp->sb.getLVList()) {
        for (unsigned i = 0; i < top_lvars->size(); ++i) {
            LocalVar* tlv = top_lvars->lv[i];
            if (tlv && tlv->getName()) {
                top_level_locals[tlv->getName()].push_back(tlv);
            }
        }
    }
    auto findTopLevelLocal = [&top_level_locals, ctx_type_resolver](
            const char* lname, const char* ltype) -> LocalVar* {
        if (!lname || !*lname) {
            return nullptr;
        }
        auto it = top_level_locals.find(lname);
        if (it == top_level_locals.end()) {
            return nullptr;
        }
        for (LocalVar* tlv : it->second) {
            if (aotLocalTypeMatches(tlv, ltype, ctx_type_resolver)) {
                return tlv;
            }
        }
        return nullptr;
    };

    // Read and resolve local slot identities
    bool has_local_decl_ordinal = (reader.getHeader().feature_flags & QORE_AOT_FEAT_LOCAL_DECL_ORDINAL) != 0;
    for (int i = 0; i < num_locals; ++i) {
        const char* lname = reader.readStringRef(ptr);
        const char* ltype = reader.readStringRef(ptr);
        uint8_t lflags = QoreAOTBinaryReader::readU8(ptr);
        uint16_t param_idx = QoreAOTBinaryReader::readU16(ptr);
        uint32_t body_ordinal = has_local_decl_ordinal
            ? QoreAOTBinaryReader::readU32(ptr) : UINT32_MAX;

        LocalVar* lv = direct_local_slots && static_cast<size_t>(i) < direct_local_slots->size()
            ? (*direct_local_slots)[i] : nullptr;
        if (!lv && (lflags & 0x04)) {
            // is_self
            if (sig) {
                lv = sig->selfid;
            }
        } else if (!lv && (lflags & 0x08)) {
            // is_argv
            if (sig) {
                lv = sig->argvid;
            }
        } else if (!lv && (lflags & 0x01)) {
            // is_param — resolve by index first, fall back to name match
            if (sig && param_idx < sig->lv.size()) {
                lv = sig->lv[param_idx];
            }
            if (!lv && sig && lname && *lname) {
                // Index-based lookup failed; try name-based resolution as fallback.
                // This handles cases where the parameter count differs between
                // serialization and deserialization (e.g., type resolution changes).
                for (unsigned pi = 0; pi < sig->numParams() && pi < sig->lv.size(); ++pi) {
                    if (sig->lv[pi] && strcmp(sig->lv[pi]->getName(), lname) == 0) {
                        lv = sig->lv[pi];
                        printd(2, "AOT v2: '%s' local[%d] param '%s' resolved by name (idx %d != serialized %d)\n",
                            name, i, lname, pi, param_idx);
                        break;
                    }
                }
            }
        } else if (!lv) {
            // Body local — try to find the actual LocalVar* from the function's AST
            // first, then fall back to creating a new one (toplevel case).
            // New AOT records carry the source body-local ordinal so duplicate
            // names in sibling switch/if blocks resolve by identity instead of
            // depending on local slot order matching source walk order.
            if (body_ordinal != UINT32_MAX && body_ordinal < stmt_locals.size()) {
                LocalVar* candidate = stmt_locals[body_ordinal];
                if (candidate && candidate->getName()
                        && strcmp(candidate->getName(), lname ? lname : "") == 0
                        && aotLocalTypeMatches(candidate, ltype, ctx_type_resolver)) {
                    lv = candidate;
                    removeAOTLocalCandidate(stmt_local_deque, lname, lv);
                }
            }
            // Legacy fallback: match on both name and type when available,
            // consuming in walk order. This is kept only for old blobs without
            // QORE_AOT_FEAT_LOCAL_DECL_ORDINAL.
            if (lname && *lname && !stmt_local_deque.empty()) {
                if (!lv) {
                    lv = popMatchingAOTLocal(stmt_local_deque, lname, ltype, ctx_type_resolver);
                }
            }
            if (!lv) {
                lv = findTopLevelLocal(lname, ltype);
            }
            if (!lv) {
                // Toplevel or not found in AST — create a new LocalVar
                std::string type_error;
                const QoreTypeInfo* ti = nullptr;
                if (ltype && *ltype) {
                    ti = ctx_type_resolver->resolve(ltype, type_error);
                    if (!type_error.empty()) {
                        type_error.clear();
                    }
                }
                lv = local_pp->createLocalVar(lname ? lname : "", ti);
            }
        }

        if (lv) {
            // If compile-time flags say closure_use but resolved LocalVar doesn't have it,
            // propagate the flag (source-stripped binaries create new LocalVars without closure_use)
            if ((lflags & 0x02) && !lv->closureUse()) {
                printd(2, "AOT v2: '%s' local[%d] = '%s' closure_use mismatch: flags=0x%x, propagating\n",
                    name, i, lname ? lname : "", lflags);
                lv->setClosureUse();
            }
            if ((lflags & 0x10) && !lv->isReadOnly()) {
                lv->setReadOnly();
            }
            ctx->locals[i] = lv;
            printd(3, "AOT v2: '%s' local[%d] = '%s' (flags=0x%x param_idx=%d) -> %p\n",
                name, i, lname ? lname : "", lflags, param_idx, (void*)lv);
        } else {
            printd(0, "AOT v2: '%s' unresolved local slot %d ('%s' flags=0x%x param_idx=%d)\n",
                name, i, lname ? lname : "", lflags, param_idx);
            // If this is a param and we have LValuePath instructions, mark unsupported
            // to prevent crash from null locals in LValuePath navigation
            if ((lflags & 0x01) && num_lv_path_insts > 0) {
                if (aot_slot_resolve_error.empty()) {
                    aot_slot_resolve_error = "parameter slot " + std::to_string(i) + " ('"
                        + (lname && *lname ? lname : "<unnamed>")
                        + "') could not be bound to a local variable in the parsed signature";
                }
                has_unsupported = true;
            }
        }
    }

    // Read and resolve global slot identities
    // Use qore_root_ns_private::runtimeFindGlobalVar() which searches via the varmap index
    // across all namespaces — not just the root namespace's local vmap.
    // This is needed for builtin globals like Qore::ARGV, Qore::QORE_ARGV, Qore::ENV
    // which live in the Qore sub-namespace.
    bool has_global_slot_flags = (reader.getHeader().feature_flags & QORE_AOT_FEAT_GLOBAL_SLOT_FLAGS) != 0;
    ctx->global_names.resize(num_globals);
    ctx->global_required_imports.resize(num_globals);
    for (int i = 0; i < num_globals; ++i) {
        const char* gname = reader.readStringRef(ptr);
        const char* gtype = reader.readStringRef(ptr);
        uint8_t is_tl = QoreAOTBinaryReader::readU8(ptr);
        uint8_t is_required_import = has_global_slot_flags ? QoreAOTBinaryReader::readU8(ptr) : 0;
        ctx->global_names[i] = gname ? gname : "";
        ctx->global_required_imports[i] = is_required_import;

        if (gname && *gname) {
            // Look up global variable by name across all namespaces
            const qore_ns_private* vns = nullptr;
            Var* v = qore_root_ns_private::runtimeFindGlobalVar(*pp->RootNS, gname, vns);
            if (v) {
                ctx->globals[i] = v;
            } else {
                printd(2, "AOT v2: unresolved global slot %d ('%s') for '%s'\n",
                    i, gname, name);
            }
        }
        (void)gtype;
        (void)is_tl;
    }

    // Read and resolve expression slot identities
    // Deferred EXPR_TREE blobs: processed after all other slots are resolved
    // (EXPR_TREE may reference CLOSURE_CREATE slots that come later in the stream)
    struct DeferredExprTree {
        int slot;
        const uint8_t* blob_data;
        uint32_t blob_size;
    };
    std::vector<DeferredExprTree> deferred_expr_trees;
    bool closure_ir_missing = false;
    std::string closure_ir_error;
    auto setClosureIRError = [&closure_ir_error](std::string msg) {
        if (closure_ir_error.empty()) {
            closure_ir_error = std::move(msg);
        }
    };
    struct ExprUnsupportedTraceGuard {
        bool enabled;
        const char* func_name;
        int slot;
        uint8_t kind_byte;
        const char* kind_name;
        uint8_t& has_unsupported;
        uint8_t initially_unsupported;

        ~ExprUnsupportedTraceGuard() {
            if (enabled && !initially_unsupported && has_unsupported) {
                fprintf(stderr, "[aot-slot-reg] '%s': expr[%d] kind=%s(%u) marked unsupported\n",
                    func_name, slot, kind_name ? kind_name : "UNKNOWN", kind_byte);
            }
        }
    };
    for (int i = 0; i < num_exprs; ++i) {
        const uint8_t* before_expr = ptr;  // Track ptr position for validation
        uint8_t kind_byte = QoreAOTBinaryReader::readU8(ptr);
        AOTExprKind kind = static_cast<AOTExprKind>(kind_byte);
        const auto* expr_kind_info = getAOTExprKindInfo(kind_byte);
        const char* expr_kind_name = expr_kind_info && expr_kind_info->name
            ? expr_kind_info->name : "UNKNOWN";
        const char* ref1 = nullptr;
        const char* ref2 = nullptr;
        const char* ref3 = nullptr;

        // Validate expression kind through the registry instead of keeping a
        // second hard-coded max native opcode here.
        bool kind_is_valid = expr_kind_info && expr_kind_info->is_supported;
        if (!kind_is_valid) {
            printd(2, "AOT buildCtx '%s': unsupported kind_byte=%d at expr slot %d\n",
                name, kind_byte, i);
            if (trace_slot_reg) {
                fprintf(stderr, "[aot-slot-reg] '%s': unsupported expr kind byte %u (%s) at slot %d\n",
                    name, kind_byte, expr_kind_name, i);
            }
            setAOTExprSlotResolveError(i, expr_kind_name,
                "unsupported expression kind byte " + std::to_string(kind_byte)
                    + "; the artifact was written by a newer Qore than the one reading it");
            has_unsupported = true;
            break;
        }
        ExprUnsupportedTraceGuard unsupported_trace{
            trace_slot_reg, name, i, kind_byte, expr_kind_name, has_unsupported, has_unsupported};

        switch (kind) {
            case AOTExprKind::NEW_OBJECT:
            case AOTExprKind::SCOPED_NEW_OBJECT: {
                // ref1 = class path, ref2 = variant signature (e.g. "(string)").
                // ref3 = instantiated object type path when present.
                // Constructor args are IR operands here, not inline AST values.
                // If no exact variant signature was serialized, leave the
                // variant unset so constructor overload resolution runs after
                // the operand values have been evaluated.
                ref1 = reader.readStringRef(ptr);
                ref2 = reader.readStringRef(ptr);
                if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_NEW_OBJECT_TYPEINFO) != 0) {
                    ref3 = reader.readStringRef(ptr);
                }
                const QoreTypeInfo* object_type_info = nullptr;
                if (ref3 && *ref3) {
                    QoreAOTTypeResolver type_resolver(pgm);
                    std::string type_error;
                    object_type_info = type_resolver.resolve(ref3, type_error);
                    if (!object_type_info || !type_error.empty()) {
                        std::string msg = "cannot resolve new-object type path '";
                        msg += ref3;
                        msg += "'";
                        if (!type_error.empty()) {
                            msg += ": ";
                            msg += type_error;
                        }
                        if (trace_slot_reg) {
                            fprintf(stderr, "[aot-slot-reg] '%s': expr[%d] %s cannot resolve "
                                "object type '%s'%s%s\n", name, i, expr_kind_name, ref3,
                                type_error.empty() ? "" : ": ", type_error.c_str());
                        }
                        setBuildError(msg);
                        has_unsupported = true;
                        ctx->exprs[i] = toBitsNB(QoreValue());
                        continue;
                    }
                }
                if (ref2 && !strcmp(ref2, QORE_AOT_DEFERRED_CREATE_OBJECT_SLOT)) {
                    uint8_t num_args = QoreAOTBinaryReader::readU8(ptr);
                    QoreParseListNode* args = nullptr;
                    bool deferred_error = false;
                    if (num_args > 0) {
                        args = new QoreParseListNode(&loc_builtin);
                        for (uint8_t j = 0; j < num_args; ++j) {
                            std::string arg_err;
                            QoreValue arg = readOneExpr(reader, ptr, end, arg_err, pgm,
                                ctx->locals, ctx->num_locals, ctx->globals, ctx->num_globals,
                                local_owner_pgm);
                            if (!arg_err.empty()) {
                                if (trace_slot_reg) {
                                    fprintf(stderr, "[aot-slot-reg] '%s': expr[%d] %s cannot read "
                                        "deferred constructor arg %u for class '%s': %s\n",
                                        name, i, expr_kind_name, static_cast<unsigned>(j),
                                        ref1 ? ref1 : "", arg_err.c_str());
                                }
                                arg.discard(nullptr);
                                if (args) {
                                    args->deref();
                                }
                                setBuildError(arg_err);
                                has_unsupported = true;
                                ctx->exprs[i] = toBitsNB(QoreValue());
                                args = nullptr;
                                deferred_error = true;
                                break;
                            }
                            args->add(arg, &loc_builtin);
                        }
                    }
                    if (deferred_error) {
                        continue;
                    }
                    if (ref1 && *ref1) {
                        ctx->owned_call_target_strings.emplace_back(ref1);
                        ctx->call_targets[i].class_path = ctx->owned_call_target_strings.back().c_str();
                    }
                    ctx->call_targets[i].object_type_info = object_type_info;
                    QoreValue call = makeDeferredObjectSlotCall(pgm, ref1, args, object_type_info);
                    if (!call) {
                        std::string msg = "cannot create deferred constructor call for class '";
                        msg += ref1 ? ref1 : "";
                        msg += "'";
                        setBuildError(msg);
                        has_unsupported = true;
                        ctx->exprs[i] = toBitsNB(QoreValue());
                        continue;
                    }
                    if (trace_slot_reg) {
                        fprintf(stderr, "[aot-slot-reg] '%s': expr[%d] %s defers constructor class '%s' "
                            "to dynamic object resolution\n", name, i, expr_kind_name, ref1 ? ref1 : "");
                    }
                    ctx->exprs[i] = toBitsNB(call);
                    continue;
                }
                if (ref1 && *ref1) {
                    ctx->owned_call_target_strings.emplace_back(ref1);
                    ctx->call_targets[i].class_path = ctx->owned_call_target_strings.back().c_str();
                }
                if (ref2 && *ref2) {
                    ctx->owned_call_target_strings.emplace_back(ref2);
                    ctx->call_targets[i].variant_sig = ctx->owned_call_target_strings.back().c_str();
                }
                const QoreClass* qc = nullptr;
                const AbstractQoreFunctionVariant* resolved_variant = nullptr;
                if (ref1 && *ref1) {
                    qc = findAOTClassByPath(pgm, ref1, false);
                    if (qc && ref2 && *ref2) {
                        // Match variant by signature
                        const QoreMethod* cons = qc->getConstructor();
                        if (cons) {
                            MethodFunctionBase* cf = qore_method_private::get(*const_cast<QoreMethod*>(cons))
                                ->getFunction();
                            resolved_variant = findAOTVariantBySignatureText(cf, ref2);
                        }
                    }
                }
                if (!qc) {
                    std::string class_desc = describeAOTClassRef(ref1);
                    printd(0, "AOT v2: cannot resolve class '%s' for new object\n",
                        class_desc.c_str());
                    if (trace_slot_reg) {
                        fprintf(stderr, "[aot-slot-reg] '%s': expr[%d] %s cannot resolve class '%s' variant='%s'\n",
                            name, i, expr_kind_name, class_desc.c_str(), ref2 ? ref2 : "");
                        fprintf(stderr, "[aot-slot-reg] '%s': expr[%d] %s defers class resolution to runtime\n",
                            name, i, expr_kind_name);
                    }
                    ctx->exprs[i] = toBitsNB(QoreValue());
                    continue;
                }
                if (!resolved_variant && ref2 && *ref2) {
                    std::string class_desc = describeAOTClassRef(ref1);
                    printd(0, "AOT v2: cannot resolve constructor variant '%s' for class '%s'\n",
                        ref2, class_desc.c_str());
                    if (trace_slot_reg) {
                        fprintf(stderr, "[aot-slot-reg] '%s': expr[%d] %s cannot resolve constructor "
                            "variant '%s' for class '%s'; deferring overload resolution to runtime\n",
                            name, i, expr_kind_name, ref2, class_desc.c_str());
                    }
                    ctx->call_targets[i].qc = qc;
                    ctx->call_targets[i].object_type_info = object_type_info;
                    ctx->exprs[i] = toBitsNB(QoreValue());
                    continue;
                }
                if (!resolved_variant && trace_slot_reg) {
                    fprintf(stderr, "[aot-slot-reg] '%s': expr[%d] %s defers constructor variant "
                        "resolution for class '%s' variant='%s'\n",
                        name, i, expr_kind_name, ref1 ? ref1 : "", ref2 ? ref2 : "");
                }
                // Store in call_targets for LLVM to load qc/variant at runtime
                ctx->call_targets[i].qc = qc;
                ctx->call_targets[i].variant = resolved_variant;
                ctx->call_targets[i].object_type_info = object_type_info;
                // exprs[i] not used — LLVM uses call_targets directly
                ctx->exprs[i] = toBitsNB(QoreValue());
                continue;
            }
            case AOTExprKind::FUNC_CALL:
                ref1 = reader.readStringRef(ptr);
                if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_FUNC_CALL_VARIANT) != 0) {
                    ref2 = reader.readStringRef(ptr);
                }
                break;
            case AOTExprKind::RUNTIME_CONST_REF:
            case AOTExprKind::LOCAL_VARREF:
            case AOTExprKind::GLOBAL_VARREF:
            case AOTExprKind::CONST_NUMBER:
            case AOTExprKind::CONST_BINARY:
            case AOTExprKind::CONST_STRING:
            case AOTExprKind::SELF_VARREF:
                ref1 = reader.readStringRef(ptr);
                break;
            case AOTExprKind::COMPLEX_HASH_NEW: {
                // ref1 = type path, followed by serialized constructor args
                ref1 = reader.readStringRef(ptr);
                uint8_t num_args = QoreAOTBinaryReader::readU8(ptr);
                QoreListNode* call_args = nullptr;
                if (num_args > 0) {
                    call_args = qore_list_private::newList(true);
                    for (uint8_t j = 0; j < num_args; ++j) {
                        std::string arg_err;
                        QoreValue arg = readOneExpr(reader, ptr, end, arg_err, pgm,
                            ctx->locals, ctx->num_locals, ctx->globals, ctx->num_globals, local_owner_pgm);
                        if (!arg_err.empty()) {
                            printd(0, "AOT v2: error reading complex hash arg %d for '%s': %s\n",
                                j, ref1 ? ref1 : "", arg_err.c_str());
                            arg.discard(nullptr);
                            call_args->push(QoreValue(), nullptr);
                            has_unsupported = true;
                        } else {
                            call_args->push(arg, nullptr);
                        }
                    }
                }
                if (has_unsupported) {
                    if (call_args) {
                        call_args->deref(nullptr);
                    }
                    continue;
                }
                // Resolve type and create node with args
                if (ref1 && *ref1) {
                    std::string type_error;
                    QoreAOTTypeResolver type_resolver(pgm);
                    const QoreTypeInfo* ti = type_resolver.resolve(ref1, type_error);
                    if (ti) {
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
                            call_args = nullptr;
                        }
                        NewComplexHashNode* nch = new NewComplexHashNode(&loc_builtin, ti, pln);
                        ctx->exprs[i] = toBitsNB(QoreValue(nch));
                    } else {
                        printd(0, "AOT v2: cannot resolve type '%s' for complex hash: %s\n",
                            ref1, type_error.c_str());
                        if (call_args) {
                            call_args->deref(nullptr);
                        }
                        has_unsupported = true;
                    }
                } else {
                    if (call_args) {
                        call_args->deref(nullptr);
                    }
                    has_unsupported = true;
                }
                continue;
            }
            case AOTExprKind::COMPLEX_LIST_NEW: {
                // ref1 = type path, followed by one serialized constructor value
                ref1 = reader.readStringRef(ptr);
                uint8_t num_args = QoreAOTBinaryReader::readU8(ptr);
                QoreValue arg_val;
                if (num_args > 0) {
                    std::string arg_err;
                    arg_val = readOneExpr(reader, ptr, end, arg_err, pgm,
                        ctx->locals, ctx->num_locals, ctx->globals, ctx->num_globals, local_owner_pgm);
                    if (!arg_err.empty()) {
                        printd(0, "AOT v2: error reading complex list arg for '%s': %s\n",
                            ref1 ? ref1 : "", arg_err.c_str());
                        arg_val.discard(nullptr);
                        arg_val = QoreValue();
                        has_unsupported = true;
                    }
                }
                if (has_unsupported) {
                    continue;
                }
                // Resolve type and create node with arg
                if (ref1 && *ref1) {
                    std::string type_error;
                    QoreAOTTypeResolver type_resolver(pgm);
                    const QoreTypeInfo* ti = type_resolver.resolve(ref1, type_error);
                    if (ti) {
                        NewComplexListNode* ncl = new NewComplexListNode(&loc_builtin, ti, arg_val);
                        ctx->exprs[i] = toBitsNB(QoreValue(ncl));
                    } else {
                        printd(0, "AOT v2: cannot resolve type '%s' for complex list: %s\n",
                            ref1, type_error.c_str());
                        arg_val.discard(nullptr);
                        has_unsupported = true;
                    }
                } else {
                    arg_val.discard(nullptr);
                    has_unsupported = true;
                }
                continue;
            }
            case AOTExprKind::COMPLEX_BUFFER_NEW: {
                // ref1 = type path, followed by one serialized list initializer
                ref1 = reader.readStringRef(ptr);
                QoreComplexBufferInitKind init_kind = QoreComplexBufferInitKind::Constructor;
                if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_COMPLEX_BUFFER_INIT_KIND) != 0) {
                    init_kind = static_cast<QoreComplexBufferInitKind>(QoreAOTBinaryReader::readU8(ptr));
                }
                uint8_t num_args = QoreAOTBinaryReader::readU8(ptr);
                QoreValue arg_val;
                if (num_args > 0) {
                    std::string arg_err;
                    arg_val = readOneExpr(reader, ptr, end, arg_err, pgm,
                        ctx->locals, ctx->num_locals, ctx->globals, ctx->num_globals, local_owner_pgm);
                    if (!arg_err.empty()) {
                        printd(0, "AOT v2: error reading complex buffer arg for '%s': %s\n",
                            ref1 ? ref1 : "", arg_err.c_str());
                        arg_val.discard(nullptr);
                        arg_val = QoreValue();
                        has_unsupported = true;
                    }
                }
                if (has_unsupported) {
                    continue;
                }
                if (ref1 && *ref1) {
                    std::string type_error;
                    QoreAOTTypeResolver type_resolver(pgm);
                    const QoreTypeInfo* ti = type_resolver.resolve(ref1, type_error);
                    if (ti) {
                        NewComplexBufferNode* ncb = new NewComplexBufferNode(&loc_builtin, ti, arg_val, init_kind);
                        ctx->exprs[i] = toBitsNB(QoreValue(ncb));
                    } else {
                        printd(0, "AOT v2: cannot resolve type '%s' for complex buffer: %s\n",
                            ref1, type_error.c_str());
                        arg_val.discard(nullptr);
                        has_unsupported = true;
                    }
                } else {
                    arg_val.discard(nullptr);
                    has_unsupported = true;
                }
                continue;
            }
            case AOTExprKind::HASHDECL_NEW: {
                // ref1 = hashdecl path, followed by serialized constructor args
                ref1 = reader.readStringRef(ptr);
                uint8_t num_args = QoreAOTBinaryReader::readU8(ptr);
                QoreListNode* call_args = nullptr;
                if (num_args > 0) {
                    call_args = qore_list_private::newList(true);
                    for (uint8_t j = 0; j < num_args; ++j) {
                        std::string arg_err;
                        QoreValue arg = readOneExpr(reader, ptr, end, arg_err, pgm,
                            ctx->locals, ctx->num_locals, ctx->globals, ctx->num_globals, local_owner_pgm);
                        if (!arg_err.empty()) {
                            printd(0, "AOT v2: error reading hashdecl arg %d for '%s': %s\n",
                                j, ref1 ? ref1 : "", arg_err.c_str());
                            arg.discard(nullptr);
                            call_args->push(QoreValue(), nullptr);
                            has_unsupported = true;
                        } else {
                            call_args->push(arg, nullptr);
                        }
                    }
                }
                if (has_unsupported) {
                    if (call_args) {
                        call_args->deref(nullptr);
                    }
                    continue;
                }
                // Resolve hashdecl and create node with args
                if (ref1 && *ref1) {
                    const TypedHashDecl* hd = qore_aot_resolve_hashdecl_path(pgm, ref1);
                    if (hd) {
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
                            call_args = nullptr;
                        }
                        NewHashDeclNode* nhd = new NewHashDeclNode(&loc_builtin, hd, pln, false);
                        ctx->exprs[i] = toBitsNB(QoreValue(nhd));
                    } else {
                        printd(0, "AOT v2: cannot resolve hashdecl '%s' for new hashdecl\n", ref1);
                        if (call_args) {
                            call_args->deref(nullptr);
                        }
                        has_unsupported = true;
                    }
                } else {
                    if (call_args) {
                        call_args->deref(nullptr);
                    }
                    has_unsupported = true;
                }
                continue;
            }
            case AOTExprKind::DOT_EVAL_TARGET: {
                // Dot-eval method target: resolve class/method from strings
                // and populate call_targets directly — no AST node needed
                ref1 = reader.readStringRef(ptr);    // class_path
                // method_name[\nvariant_class\nvariant_signature]
                AOTEncodedMethodRef method_ref(reader.readStringRef(ptr));
                uint8_t is_pseudo = QoreAOTBinaryReader::readU8(ptr);

                const QoreClass* qc = findAOTClassByPath(pgm, ref1, is_pseudo != 0);
                const QoreMethod* method = findAOTMethodByName(qc, method_ref.method_name);
                const AbstractQoreFunctionVariant* variant = findAOTMethodVariantByRef(
                    pgm, method, method_ref, is_pseudo != 0);
                ctx->call_targets[i].method = method;
                ctx->call_targets[i].qc = qc;
                ctx->call_targets[i].is_pseudo = is_pseudo != 0;
                // Make an owned copy of the method name since the source buffer
                // (decompressed metadata) may be freed after deserialization
                if (method_ref.method_name && *method_ref.method_name) {
                    ctx->owned_call_target_strings.emplace_back(method_ref.method_name);
                    ctx->call_targets[i].method_name = ctx->owned_call_target_strings.back().c_str();
                }
                if (variant) {
                    ctx->call_targets[i].variant = variant;
                } else if (method && !method_ref.sig_text) {
                    MethodFunctionBase* mfb = qore_method_private::get(
                        *method)->getFunction();
                    if (mfb && mfb->numVariants() == 1) {
                        // Resolve variant from method only when the method has exactly
                        // ONE variant — safe fast-dispatch shortcut. For overloaded
                        // methods, leave variant null so the runtime does proper
                        // arg-type-based overload resolution via findVariant. Falling
                        // back to first() is unsafe: try_dispatch_method_fast binds
                        // caller args directly to the picked variant's signature.
                        ctx->call_targets[i].variant = mfb->first();
                    }
                }
                // Store NOTHING in exprs — slot-based dispatch uses call_targets
                ctx->exprs[i] = toBitsNB(QoreValue());
                continue;
            }
            case AOTExprKind::FUNC_CALL_REF: {
                // Function call reference: resolve function by name
                ref1 = reader.readStringRef(ptr);   // function_name
                if (ref1 && *ref1) {
                    const FunctionEntry* fe = qore_aot_resolve_function_entry_for_slot(pgm, ref1);
                    if (fe) {
                        QoreFunction* f = fe->getFunction();
                        if (f) {
                            ctx->exprs[i] = toBitsNB(QoreValue(
                                new LocalFunctionCallReferenceNode(&loc_builtin, f)));
                            continue;
                        }
                    }
                }
                printd(0, "AOT v2: cannot resolve function ref '%s'\n",
                    ref1 ? ref1 : "(null)");
                has_unsupported = true;
                continue;
            }
            case AOTExprKind::CALLREF_CALL: {
                std::string callee_err;
                QoreValue callee = readOneExpr(reader, ptr, end, callee_err, pgm,
                    ctx->locals, ctx->num_locals, ctx->globals, ctx->num_globals, local_owner_pgm);
                if (!callee_err.empty()) {
                    printd(0, "AOT v2: error reading callref callee for slot %d: %s\n",
                        i, callee_err.c_str());
                    callee.discard(nullptr);
                    has_unsupported = true;
                    continue;
                }
                uint8_t num_args = QoreAOTBinaryReader::readU8(ptr);
                QoreParseListNode* args = nullptr;
                if (num_args > 0) {
                    args = new QoreParseListNode(&loc_builtin);
                    for (uint8_t j = 0; j < num_args; ++j) {
                        std::string arg_err;
                        QoreValue arg = readOneExpr(reader, ptr, end, arg_err, pgm,
                            ctx->locals, ctx->num_locals, ctx->globals, ctx->num_globals, local_owner_pgm);
                        if (!arg_err.empty()) {
                            printd(0, "AOT v2: error reading callref arg %d for slot %d: %s\n",
                                j, i, arg_err.c_str());
                            delete args;
                            callee.discard(nullptr);
                            arg.discard(nullptr);
                            has_unsupported = true;
                            args = nullptr;
                            break;
                        }
                        args->add(arg, &loc_builtin);
                    }
                }
                if (has_unsupported) {
                    continue;
                }
                auto* call = new CallReferenceCallNode(&loc_builtin, callee, args);
                call->resolveParseArgs();
                ctx->exprs[i] = toBitsNB(QoreValue(call));
                continue;
            }
            case AOTExprKind::BOUND_METHOD_REF: {
                // Bound method reference: resolve class + method
                ref1 = reader.readStringRef(ptr);   // class_path
                ref2 = reader.readStringRef(ptr);   // method_name
                const QoreMethod* method = nullptr;
                if (ref1 && *ref1) {
                    const QoreClass* qc = findAOTClassByPath(pgm, ref1, false);
                    if (qc && ref2 && *ref2) {
                        method = qc->findMethod(ref2);
                        if (!method) {
                            method = findAOTStaticMethod(qc, ref2);
                        }
                    }
                }
                if (method) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new LocalMethodCallReferenceNode(&loc_builtin, method)));
                } else {
                    printd(0, "AOT v2: cannot resolve bound method ref '%s::%s'\n",
                        ref1 ? ref1 : "", ref2 ? ref2 : "");
                    has_unsupported = true;
                }
                continue;
            }
            case AOTExprKind::STATIC_METHOD_REF: {
                // Static method reference: resolve class + static method
                ref1 = reader.readStringRef(ptr);   // class_path
                ref2 = reader.readStringRef(ptr);   // method_name
                const QoreMethod* method = nullptr;
                if (ref1 && *ref1) {
                    const QoreClass* qc = findAOTClassByPath(pgm, ref1, false);
                    if (qc && ref2 && *ref2) {
                        method = findAOTStaticMethod(qc, ref2);
                        if (!method) {
                            method = qc->findMethod(ref2);
                        }
                    }
                }
                if (method) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new LocalStaticMethodCallReferenceNode(&loc_builtin, method)));
                } else {
                    printd(0, "AOT v2: cannot resolve static method ref '%s::%s'\n",
                        ref1 ? ref1 : "", ref2 ? ref2 : "");
                    has_unsupported = true;
                }
                continue;
            }
            case AOTExprKind::DEFERRED_STATIC_METHOD_REF: {
                ref1 = reader.readStringRef(ptr);   // class_path
                ref2 = reader.readStringRef(ptr);   // method_name
                if (ref1 && *ref1 && ref2 && *ref2) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new DeferredStaticMethodCallReferenceNode(&loc_builtin, ref1, ref2)));
                } else {
                    printd(0, "AOT v2: empty deferred static method ref '%s::%s'\n",
                        ref1 ? ref1 : "", ref2 ? ref2 : "");
                    has_unsupported = true;
                }
                continue;
            }
            case AOTExprKind::SELF_METHOD_REF: {
                // Self method reference: just needs method name
                ref1 = reader.readStringRef(ptr);   // method_name
                if (ref1 && *ref1) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new ParseSelfMethodReferenceNode(&loc_builtin, strdup(ref1))));
                } else {
                    printd(0, "AOT v2: empty self method ref name\n");
                    has_unsupported = true;
                }
                continue;
            }
            case AOTExprKind::OBJ_METHOD_REF_EXPR: {
                // Object method reference with target expression
                ref1 = reader.readStringRef(ptr);   // method_name
                std::string child_err;
                QoreValue target = readOneExpr(reader, ptr, end, child_err, pgm,
                    ctx->locals, ctx->num_locals, ctx->globals, ctx->num_globals, local_owner_pgm);
                if (!child_err.empty()) {
                    printd(0, "AOT v2: error reading obj method ref target for '%s': %s\n",
                        ref1 ? ref1 : "", child_err.c_str());
                    target.discard(nullptr);
                    has_unsupported = true;
                } else if (ref1 && *ref1) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new ParseObjectMethodReferenceNode(&loc_builtin,
                            target, strdup(ref1))));
                } else {
                    printd(0, "AOT v2: empty obj method ref name\n");
                    target.discard(nullptr);
                    has_unsupported = true;
                }
                continue;
            }
            case AOTExprKind::CAST_HASHDECL:
            case AOTExprKind::CAST_COMPLEX_HASH:
            case AOTExprKind::CAST_COMPLEX_LIST:
            case AOTExprKind::CAST_CLASS:
            case AOTExprKind::CAST_ENUM:
            case AOTExprKind::CAST_SCALAR: {
                // Slot-map cast payloads are compact: type path + flags.  The
                // evaluated input is carried by the native IR operand, unlike
                // inline nested expressions which also serialize an inner expr.
                ref1 = reader.readStringRef(ptr);
                uint8_t flags = QoreAOTBinaryReader::readU8(ptr);
                uint64_t bits = resolveCastExprSlot(kind, ref1, (flags & 1) != 0, pgm);
                if (bits) {
                    ctx->exprs[i] = bits;
                    continue;
                }
                printd(0, "AOT v2: cannot resolve cast slot kind %d type '%s'\n",
                    static_cast<int>(kind), ref1 ? ref1 : "(null)");
                has_unsupported = true;
                continue;
            }
            case AOTExprKind::SELF_METHOD_CALL: {
                ref1 = reader.readStringRef(ptr);
                ref2 = reader.readStringRef(ptr);
                // When the producer carried slot args (QORE_AOT_FEAT_SELF_CALL_SLOT_ARGS), read
                // them here: the pointer MUST advance past them to stay in sync, and the
                // reconstructed self-call node needs them so AST/IR-interpreter fallback
                // evaluation dispatches the call with its arguments (native dispatch passes args
                // as separate operands and is unaffected).
                if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_SELF_CALL_SLOT_ARGS) != 0) {
                    uint8_t num_args = QoreAOTBinaryReader::readU8(ptr);
                    ReferenceHolder<QoreListNode> self_args(
                        num_args ? qore_list_private::newList(true) : nullptr, nullptr);
                    bool self_args_ok = true;
                    for (uint8_t j = 0; j < num_args; ++j) {
                        std::string arg_err;
                        QoreValue arg = readOneExpr(reader, ptr, end, arg_err, pgm,
                            ctx->locals, ctx->num_locals, ctx->globals, ctx->num_globals,
                            local_owner_pgm);
                        if (!arg_err.empty()) {
                            arg.discard(nullptr);
                            self_args->push(QoreValue(), nullptr);
                            self_args_ok = false;
                        } else {
                            self_args->push(arg, nullptr);
                        }
                    }
                    // Build the base (method/class/variant-resolved) self-call node, then wrap it
                    // with the args via the copy constructor (mirrors read_node_EN_SELF_CALL).
                    uint64_t base_bits = resolveExprSlot(kind, ref1, ref2, pgm);
                    if (base_bits) {
                        QoreValue base_val = fromBits(base_bits);
                        const SelfFunctionCallNode* base_sfcn = dynamic_cast<const SelfFunctionCallNode*>(
                            base_val.getInternalNode());
                        if (base_sfcn && num_args && self_args_ok) {
                            SelfFunctionCallNode* sfcn = new SelfFunctionCallNode(*base_sfcn,
                                self_args.release());
                            base_val.discard(nullptr);
                            ctx->exprs[i] = toBitsNB(QoreValue(sfcn));
                        } else {
                            // no args (or a static-method self call, or arg error): keep base node
                            ctx->exprs[i] = base_bits;
                        }
                    }
                    continue;
                }
                break;
            }
            case AOTExprKind::STATIC_VARREF:
            case AOTExprKind::CONST_ENUM:
                ref1 = reader.readStringRef(ptr);
                ref2 = reader.readStringRef(ptr);
                break;
            case AOTExprKind::STATIC_METHOD_CALL: {
                // ref1 = class path, ref2 = method name, followed by serialized args
                ref1 = reader.readStringRef(ptr);
                ref2 = reader.readStringRef(ptr);
                const char* ref3 = nullptr;
                if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_STATIC_CALL_RECEIVER_TYPE) != 0) {
                    ref3 = reader.readStringRef(ptr);
                }
                QoreAOTStaticMethodRef method_ref(ref2);
                const char* method_name = method_ref.method_name;
                uint8_t num_args = QoreAOTBinaryReader::readU8(ptr);
                QoreListNode* call_args = nullptr;
                if (num_args > 0) {
                    call_args = qore_list_private::newList(true);
                    for (uint8_t j = 0; j < num_args; ++j) {
                        std::string arg_err;
                        QoreValue arg = readOneExpr(reader, ptr, end, arg_err, pgm,
                            ctx->locals, ctx->num_locals, ctx->globals, ctx->num_globals, local_owner_pgm);
                        if (!arg_err.empty()) {
                            printd(0, "AOT v2: error reading static method arg %d for '%s::%s': %s\n",
                                j, ref1 ? ref1 : "", ref2 ? ref2 : "", arg_err.c_str());
                            setAOTExprSlotResolveError(i, expr_kind_name,
                                "cannot read argument " + std::to_string(j) + " of the call to '"
                                    + describeAOTStaticCallTarget(ref1, method_name) + "': " + arg_err);
                            arg.discard(nullptr);
                            call_args->push(QoreValue(), nullptr);
                            has_unsupported = true;
                        } else {
                            call_args->push(arg, nullptr);
                        }
                    }
                }
                if (has_unsupported) {
                    if (call_args) {
                        call_args->deref(nullptr);
                    }
                    continue;
                }
                // Resolve class and method, create node with args
                const QoreClass* qc = nullptr;
                const qore_class_private* call_class_ctx = variant_class_ctx
                    ? variant_class_ctx : getAOTVariantClassContext(uvb);
                if (ref1 && method_name) {
                    if (trace_slot_reg) {
                        std::string ctx_ns_path;
                        if (call_class_ctx && call_class_ctx->ns) {
                            call_class_ctx->ns->getPath(ctx_ns_path, true);
                        }
                        fprintf(stderr, "[aot-slot-reg] '%s': expr[%d] STATIC_METHOD_CALL context class='%s' "
                            "ns='%s' receiver='%s'\n", name, i,
                            call_class_ctx ? call_class_ctx->path.c_str() : "",
                            ctx_ns_path.c_str(), ref1 ? ref1 : "");
                    }
                    qc = findAOTClassByPathInContext(pgm, ref1, call_class_ctx, false);
                    if (qc) {
                        const QoreMethod* m = findAOTStaticMethod(qc, method_name);
                        if (!m) {
                            m = findAOTInstanceMethod(qc, method_name, call_class_ctx);
                        }
                        if (m) {
                            const QoreTypeInfo* receiver_type_info = nullptr;
                            if (ref3 && *ref3) {
                                QoreAOTTypeResolver type_resolver(pgm);
                                std::string type_error;
                                receiver_type_info = type_resolver.resolve(ref3, type_error);
                                if (!receiver_type_info || !type_error.empty()) {
                                    if (trace_slot_reg) {
                                        fprintf(stderr, "[aot-slot-reg] '%s': expr[%d] STATIC_METHOD_CALL cannot "
                                            "resolve receiver type '%s': %s\n", name, i, ref3,
                                            type_error.c_str());
                                    }
                                    setAOTExprSlotResolveError(i, expr_kind_name,
                                        "cannot resolve receiver type '" + std::string(ref3)
                                            + "' of the call to '"
                                            + describeAOTStaticCallTarget(ref1, method_name) + "'"
                                            + (type_error.empty() ? "" : ": " + type_error));
                                    if (call_args) {
                                        call_args->deref(nullptr);
                                    }
                                    has_unsupported = true;
                                    continue;
                                }
                            }
                            const AbstractQoreFunctionVariant* resolved_variant = nullptr;
                            if (method_ref.sig_text) {
                                resolved_variant = findAOTStaticMethodVariantByRef(pgm, m, method_ref, false);
                            }
                            QoreParseListNode* pln = makeAOTParseArgsFromList(call_args);
                            AbstractFunctionCallNode* call_node = nullptr;
                            if (m->isStatic()) {
                                ctx->call_targets[i].method = m;
                                ctx->call_targets[i].is_static_method = true;
                                ctx->call_targets[i].receiver_type_info = receiver_type_info;
                                StaticMethodCallNode* smcn = new StaticMethodCallNode(&loc_builtin, m, pln);
                                smcn->setReceiverTypeInfo(receiver_type_info);
                                call_node = smcn;
                            } else {
                                std::string qualified_method_name = makeAOTQualifiedMethodName(ref1, method_name);
                                SelfFunctionCallNode* sfcn = new SelfFunctionCallNode(&loc_builtin,
                                    strdup(qualified_method_name.c_str()), pln, m, qc, call_class_ctx);
                                call_node = sfcn;
                            }
                            if (resolved_variant) {
                                ctx->call_targets[i].variant = resolved_variant;
                                ctx->call_targets[i].uvb = resolved_variant->getUserVariantBase();
                                call_node->setVariant(resolved_variant);
                                if (trace_slot_reg) {
                                    fprintf(stderr, "[aot-slot-reg] '%s': expr[%d] STATIC_METHOD_CALL "
                                        "matched signature '%s' -> '%s'\n", name, i, method_ref.sig_text,
                                        makeAOTVariantSignature(resolved_variant).c_str());
                                }
                            } else if (method_ref.sig_text) {
                                if (trace_slot_reg) {
                                    fprintf(stderr, "[aot-slot-reg] '%s': expr[%d] STATIC_METHOD_CALL "
                                        "could not match signature '%s'\n", name, i, method_ref.sig_text);
                                }
                            } else if (method_ref.arg_type_sig) {
                                MethodFunctionBase* mfb = qore_method_private::get(
                                    *const_cast<QoreMethod*>(m))->getFunction();
                                QoreTypeParamInstantiation type_param_instantiation;
                                std::string variant_error;
                                if (const AbstractQoreFunctionVariant* v = qore_aot_resolve_variant_from_arg_type_signature(
                                            pgm, mfb, method_ref.arg_type_sig, nullptr, receiver_type_info,
                                            &type_param_instantiation, variant_error)) {
                                    ctx->call_targets[i].variant = v;
                                    call_node->setVariant(v);
                                    call_node->setTypeParamInstantiation(std::move(type_param_instantiation));
                                    if (trace_slot_reg) {
                                        fprintf(stderr, "[aot-slot-reg] '%s': expr[%d] STATIC_METHOD_CALL "
                                            "matched arg types '%s' -> '%s'\n", name, i, method_ref.arg_type_sig,
                                            makeAOTVariantSignature(v).c_str());
                                    }
                                } else if (trace_slot_reg) {
                                    fprintf(stderr, "[aot-slot-reg] '%s': expr[%d] STATIC_METHOD_CALL "
                                        "could not match arg types '%s': %s\n", name, i, method_ref.arg_type_sig,
                                        variant_error.c_str());
                                }
                            } else if (trace_slot_reg) {
                                fprintf(stderr, "[aot-slot-reg] '%s': expr[%d] STATIC_METHOD_CALL has no "
                                    "serialized variant signature or arg types for '%s::%s'\n",
                                    name, i, ref1 ? ref1 : "", method_name ? method_name : "");
                            }
                            if (!applyAOTExplicitTypeArgs(*call_node, method_ref,
                                    pgm, uvb ? uvb->getUserSignature() : nullptr)) {
                                setAOTExprSlotResolveError(i, expr_kind_name,
                                    "cannot apply the explicit type arguments recorded for the call to '"
                                        + describeAOTStaticCallTarget(ref1, method_name) + "'");
                                delete call_node;
                                has_unsupported = true;
                                continue;
                            }
                            ctx->call_targets[i].explicit_type_param_instantiation =
                                call_node->getExplicitTypeParamInstantiation();
                            if (pln) {
                                call_node->resolveParseArgs();
                            }
                            ctx->exprs[i] = toBitsNB(QoreValue(call_node));
                            continue;
                        }
                    }
                }
                if (const FunctionEntry* fe = qore_aot_resolve_function_entry_for_static_call_fallback(
                        pgm, qc, ref1, method_name)) {
                    QoreParseListNode* pln = makeAOTParseArgsFromList(call_args);
                    FunctionCallNode* fcn = new FunctionCallNode(&loc_builtin, fe, pln);
                    QoreFunction* func = fe->getFunction();
                    ctx->call_targets[i].func = func;
                    if (method_ref.sig_text) {
                        if (const AbstractQoreFunctionVariant* v = func
                                ? func->findVariantBySignatureText(method_ref.sig_text) : nullptr) {
                            ctx->call_targets[i].variant = v;
                            ctx->call_targets[i].uvb = v->getUserVariantBase();
                            fcn->setVariant(v);
                        }
                    } else if (method_ref.arg_type_sig) {
                        QoreTypeParamInstantiation type_param_instantiation;
                        std::string variant_error;
                        if (const AbstractQoreFunctionVariant* v = qore_aot_resolve_variant_from_arg_type_signature(
                                pgm, func, method_ref.arg_type_sig, nullptr, nullptr,
                                &type_param_instantiation, variant_error)) {
                            ctx->call_targets[i].variant = v;
                            ctx->call_targets[i].uvb = v->getUserVariantBase();
                            fcn->setVariant(v);
                            fcn->setTypeParamInstantiation(std::move(type_param_instantiation));
                        }
                    }
                    if (pln) {
                        fcn->resolveParseArgs();
                    }
                    ctx->exprs[i] = toBitsNB(QoreValue(fcn));
                    continue;
                }
                if (trace_slot_reg) {
                    fprintf(stderr, "[aot-slot-reg] '%s': expr[%d] STATIC_METHOD_CALL cannot resolve "
                        "class='%s' method='%s' signature='%s' arg_types='%s'\n",
                        name, i, ref1 ? ref1 : "", method_name ? method_name : "",
                        method_ref.sig_text ? method_ref.sig_text : "",
                        method_ref.arg_type_sig ? method_ref.arg_type_sig : "");
                }
                // Name the call that could not be bound.  An artifact only references call targets that
                // resolved when it was written, so one that cannot be resolved now means the declaration
                // is missing from this Program: in split AOT builds a sibling `.qo` absent from the `-L`
                // preload set, or one older than the source that referenced it; otherwise a missing or
                // stale module.  None of that is recoverable from the enclosing function's name alone.
                {
                    std::string detail = "cannot resolve the call to '"
                        + describeAOTStaticCallTarget(ref1, method_name) + "': ";
                    if (qc) {
                        detail += "class '";
                        detail += ref1 ? ref1 : "";
                        detail += "' was found but has no such method";
                    } else {
                        detail += "class '";
                        detail += ref1 && *ref1 ? ref1 : "<unknown>";
                        detail += "' is not declared in this Program";
                    }
                    detail += "; the artifact was compiled against a build where it existed, so the "
                        "sibling object or module providing it is missing here or is older than the one "
                        "compiled against";
                    setAOTExprSlotResolveError(i, expr_kind_name, detail);
                }
                if (call_args) {
                    call_args->deref(nullptr);
                }
                has_unsupported = true;
                continue;
            }
            case AOTExprKind::EXPR_TREE: {
                // Defer EXPR_TREE deserialization until after all other slots
                // (especially CLOSURE_CREATE) are resolved, since EXPR_TREEs
                // may reference closure slots that appear later in the stream
                uint32_t blob_size = QoreAOTBinaryReader::readU32(ptr);
                const uint8_t* blob_data = ptr;
                ptr += blob_size;
                deferred_expr_trees.push_back({i, blob_data, blob_size});
                continue;
            }
            case AOTExprKind::HASH_LITERAL: {
                // num_pairs(u8) + [key_str(stringref) + value(readOneExpr)] * N
                uint8_t num_pairs = QoreAOTBinaryReader::readU8(ptr);
                QoreParseHashNode* phn = new QoreParseHashNode(&loc_builtin);
                bool hash_ok = true;
                for (uint8_t j = 0; j < num_pairs; ++j) {
                    const char* key_str = reader.readStringRef(ptr);
                    std::string val_err;
                    // readOneExpr reads its own ekind byte from ptr
                    QoreValue val = readOneExpr(reader, ptr, end, val_err, pgm,
                        ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                    if (!hash_ok) {
                        val.discard(nullptr);
                    } else if (!val_err.empty()) {
                        printd(2, "AOT v2: HASH_LITERAL value error for expr slot %d of '%s': %s\n",
                            i, name, val_err.c_str());
                        val.discard(nullptr);
                        hash_ok = false;
                    } else {
                        phn->add(new QoreStringNode(key_str ? key_str : ""), val, &loc_builtin);
                    }
                }
                if (hash_ok) {
                    ctx->exprs[i] = toBitsNB(QoreValue(phn));
                } else {
                    phn->deref(nullptr);
                    has_unsupported = true;
                }
                continue;
            }
            case AOTExprKind::PARSE_HASH: {
                uint8_t num_pairs = QoreAOTBinaryReader::readU8(ptr);
                QoreParseHashNode* phn = new QoreParseHashNode(&loc_builtin);
                bool hash_ok = true;
                for (uint8_t j = 0; j < num_pairs; ++j) {
                    std::string key_err;
                    QoreValue key = readOneExpr(reader, ptr, end, key_err, pgm,
                        ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                    std::string val_err;
                    QoreValue val = readOneExpr(reader, ptr, end, val_err, pgm,
                        ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                    if (!hash_ok) {
                        key.discard(nullptr);
                        val.discard(nullptr);
                    } else if (!key_err.empty() || !val_err.empty()) {
                        printd(2, "AOT v2: PARSE_HASH error for expr slot %d of '%s': %s\n",
                            i, name, !key_err.empty() ? key_err.c_str() : val_err.c_str());
                        key.discard(nullptr);
                        val.discard(nullptr);
                        hash_ok = false;
                    } else {
                        phn->add(key, val, &loc_builtin);
                    }
                }
                if (hash_ok) {
                    ctx->exprs[i] = toBitsNB(QoreValue(phn));
                } else {
                    phn->deref(nullptr);
                    has_unsupported = true;
                }
                continue;
            }
            case AOTExprKind::LIST_LITERAL: {
                // count(u8) + [value(readOneExpr)] * N
                uint8_t count = QoreAOTBinaryReader::readU8(ptr);
                QoreParseListNode* pln = new QoreParseListNode(&loc_builtin);
                bool list_ok = true;
                for (uint8_t j = 0; j < count; ++j) {
                    std::string val_err;
                    QoreValue val = readOneExpr(reader, ptr, end, val_err, pgm,
                        ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                    if (!list_ok) {
                        val.discard(nullptr);
                    } else if (!val_err.empty()) {
                        printd(2, "AOT v2: LIST_LITERAL value error for expr slot %d of '%s': %s\n",
                            i, name, val_err.c_str());
                        val.discard(nullptr);
                        list_ok = false;
                    } else {
                        pln->add(val, &loc_builtin);
                    }
                }
                if (list_ok) {
                    ctx->exprs[i] = toBitsNB(QoreValue(pln));
                } else {
                    pln->deref();
                    has_unsupported = true;
                }
                continue;
            }
            case AOTExprKind::PARSE_REF: {
                // \var lvalue reference: optional type path + inner lvalue expression.
                const char* ref_type_path = nullptr;
                if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_PARSE_REF_TYPE) != 0) {
                    ref_type_path = reader.readStringRef(ptr);
                }
                if (trace_slot_reg) {
                    fprintf(stderr, "[aot-slot-reg] '%s': PARSE_REF expr[%d] type='%s'\n",
                        name, i, ref_type_path ? ref_type_path : "");
                }
                std::string inner_err;
                QoreValue inner = readOneExpr(reader, ptr, end, inner_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!inner_err.empty()) {
                    printd(2, "AOT v2: PARSE_REF inner error for expr slot %d of '%s': %s\n",
                        i, name, inner_err.c_str());
                    if (trace_slot_reg) {
                        fprintf(stderr, "[aot-slot-reg] '%s': PARSE_REF expr[%d] inner error: %s\n",
                            name, i, inner_err.c_str());
                    }
                    inner.discard(nullptr);
                    has_unsupported = true;
                } else {
                    // Mirror the source parser's setThreadSafe() call (ReferenceNode.cpp:281):
                    // walk down through hash-deref/square-bracket operators to find the
                    // root VarRefNode and mark it VT_LOCAL_TS + closure_use.  Without this,
                    // doPartialEval takes the VT_LOCAL fallthrough path which returns the
                    // VarRefNode itself instead of resolving through the closure-var chain,
                    // causing CIRCULAR-REFERENCE-ERROR on recursive reference<auto> params.
                    {
                        QoreValue n = inner;
                        while (n.hasNode()) {
                            qore_type_t ntype = n.getType();
                            if (ntype == NT_VARREF) {
                                VarRefNode* vr = n.get<VarRefNode>();
                                if (vr->getType() == VT_LOCAL && vr->ref.id) {
                                    vr->setThreadSafe();
                                }
                                break;
                            }
                            if (ntype == NT_SELF_VARREF || ntype == NT_CLASS_VARREF) {
                                break;
                            }
                            if (ntype != NT_OPERATOR) {
                                break;
                            }
                            auto* sq = dynamic_cast<QoreSquareBracketsOperatorNode*>(
                                n.getInternalNode());
                            if (sq) {
                                n = sq->getLeft();
                                continue;
                            }
                            auto* hd = dynamic_cast<QoreHashObjectDereferenceOperatorNode*>(
                                n.getInternalNode());
                            if (hd) {
                                n = hd->getLeft();
                                continue;
                            }
                            break;
                        }
                    }
                    const QoreTypeInfo* ref_ti = referenceTypeInfo;
                    if (ref_type_path && *ref_type_path) {
                        std::string type_error;
                        QoreAOTTypeResolver type_resolver(pgm);
                        ref_ti = type_resolver.resolve(ref_type_path, type_error);
                        if (!ref_ti) {
                            printd(0, "AOT v2: cannot resolve PARSE_REF type '%s' for expr slot %d of '%s': %s\n",
                                ref_type_path, i, name, type_error.c_str());
                            if (trace_slot_reg) {
                                fprintf(stderr, "[aot-slot-reg] '%s': PARSE_REF expr[%d] type resolve error: %s\n",
                                    name, i, type_error.c_str());
                            }
                            inner.discard(nullptr);
                            has_unsupported = true;
                            continue;
                        }
                        if (!QoreTypeInfo::isReference(ref_ti)) {
                            printd(0, "AOT v2: resolved PARSE_REF type '%s' for expr slot %d of '%s' "
                                "is not a reference type\n", ref_type_path, i, name);
                            if (trace_slot_reg) {
                                fprintf(stderr, "[aot-slot-reg] '%s': PARSE_REF expr[%d] type is not a reference: %s\n",
                                    name, i, ref_type_path);
                            }
                            inner.discard(nullptr);
                            has_unsupported = true;
                            continue;
                        }
                    }
                    ctx->exprs[i] = toBitsNB(QoreValue(new ParseReferenceNode(&loc_builtin, inner, ref_ti)));
                }
                continue;
            }
            case AOTExprKind::HASH_DEREF: {
                // left (base expr) + right (key expr) — uses readOneExpr to consume both sub-expressions
                const QoreTypeInfo* result_type_info = nullptr;
                if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_HASH_DEREF_TYPEINFO) != 0) {
                    const char* type_path = reader.readStringRef(ptr);
                    if (type_path && *type_path) {
                        QoreAOTTypeResolver type_resolver(pgm);
                        std::string type_error;
                        result_type_info = type_resolver.resolve(type_path, type_error);
                        if (!result_type_info || !type_error.empty()) {
                            if (trace_slot_reg) {
                                fprintf(stderr, "[aot-slot-reg] '%s': HASH_DEREF expr[%d] cannot resolve "
                                    "result type '%s': %s\n", name, i, type_path, type_error.c_str());
                            }
                            has_unsupported = true;
                            continue;
                        }
                    }
                }
                std::string left_err;
                QoreValue left = readOneExpr(reader, ptr, end, left_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                std::string right_err;
                QoreValue right = readOneExpr(reader, ptr, end, right_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!left_err.empty() || !right_err.empty()) {
                    left.discard(nullptr);
                    right.discard(nullptr);
                    has_unsupported = true;
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreHashObjectDereferenceOperatorNode(&loc_builtin, left, right, result_type_info)));
                }
                continue;
            }
            case AOTExprKind::PLUS: {
                std::string left_err;
                QoreValue left = readOneExpr(reader, ptr, end, left_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                std::string right_err;
                QoreValue right = readOneExpr(reader, ptr, end, right_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!left_err.empty() || !right_err.empty()) {
                    left.discard(nullptr);
                    right.discard(nullptr);
                    has_unsupported = true;
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QorePlusOperatorNode(&loc_builtin, left, right)));
                }
                continue;
            }
            case AOTExprKind::RANGE: {
                std::string left_err;
                QoreValue left = readOneExpr(reader, ptr, end, left_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                std::string right_err;
                QoreValue right = readOneExpr(reader, ptr, end, right_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!left_err.empty() || !right_err.empty()) {
                    left.discard(nullptr);
                    right.discard(nullptr);
                    has_unsupported = true;
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreRangeOperatorNode(&loc_builtin, left, right)));
                }
                continue;
            }
            case AOTExprKind::SQUARE_BRACKET: {
                std::string left_err;
                QoreValue left = readOneExpr(reader, ptr, end, left_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                std::string right_err;
                QoreValue right = readOneExpr(reader, ptr, end, right_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!left_err.empty() || !right_err.empty()) {
                    left.discard(nullptr);
                    right.discard(nullptr);
                    has_unsupported = true;
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreSquareBracketsOperatorNode(&loc_builtin, left, right)));
                }
                continue;
            }
            case AOTExprKind::SQUARE_BRACKET_RANGE: {
                std::string src_err;
                QoreValue src = readOneExpr(reader, ptr, end, src_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                std::string start_err;
                QoreValue start = readOneExpr(reader, ptr, end, start_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                std::string stop_err;
                QoreValue stop = readOneExpr(reader, ptr, end, stop_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!src_err.empty() || !start_err.empty() || !stop_err.empty()) {
                    src.discard(nullptr);
                    start.discard(nullptr);
                    stop.discard(nullptr);
                    has_unsupported = true;
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreSquareBracketsRangeOperatorNode(&loc_builtin, src, start, stop)));
                }
                continue;
            }
            case AOTExprKind::EXISTS: {
                std::string operand_err;
                QoreValue operand = readOneExpr(reader, ptr, end, operand_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!operand_err.empty()) {
                    operand.discard(nullptr);
                    has_unsupported = true;
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreExistsOperatorNode(&loc_builtin, operand)));
                }
                continue;
            }
            case AOTExprKind::IMPLICIT_ARG: {
                int64_t offset = QoreAOTBinaryReader::readI64(ptr);
                int ctor_offset = offset >= 0 ? static_cast<int>(offset + 1) : static_cast<int>(offset);
                ctx->exprs[i] = toBitsNB(QoreValue(
                    new QoreImplicitArgumentNode(&loc_builtin, ctor_offset)));
                continue;
            }
            case AOTExprKind::MINUS: {
                std::string left_err;
                QoreValue left = readOneExpr(reader, ptr, end, left_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                std::string right_err;
                QoreValue right = readOneExpr(reader, ptr, end, right_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!left_err.empty() || !right_err.empty()) {
                    left.discard(nullptr);
                    right.discard(nullptr);
                    has_unsupported = true;
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreMinusOperatorNode(&loc_builtin, left, right)));
                }
                continue;
            }
            case AOTExprKind::MULTIPLY: {
                std::string left_err;
                QoreValue left = readOneExpr(reader, ptr, end, left_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                std::string right_err;
                QoreValue right = readOneExpr(reader, ptr, end, right_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!left_err.empty() || !right_err.empty()) {
                    left.discard(nullptr);
                    right.discard(nullptr);
                    has_unsupported = true;
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreMultiplicationOperatorNode(&loc_builtin, left, right)));
                }
                continue;
            }
            case AOTExprKind::DIVIDE: {
                std::string left_err;
                QoreValue left = readOneExpr(reader, ptr, end, left_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                std::string right_err;
                QoreValue right = readOneExpr(reader, ptr, end, right_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!left_err.empty() || !right_err.empty()) {
                    left.discard(nullptr);
                    right.discard(nullptr);
                    has_unsupported = true;
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreDivisionOperatorNode(&loc_builtin, left, right)));
                }
                continue;
            }
            case AOTExprKind::MODULO: {
                std::string left_err;
                QoreValue left = readOneExpr(reader, ptr, end, left_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                std::string right_err;
                QoreValue right = readOneExpr(reader, ptr, end, right_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!left_err.empty() || !right_err.empty()) {
                    left.discard(nullptr);
                    right.discard(nullptr);
                    has_unsupported = true;
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreModuloOperatorNode(&loc_builtin, left, right)));
                }
                continue;
            }
            case AOTExprKind::BIT_AND:
            case AOTExprKind::BIT_OR:
            case AOTExprKind::BIT_XOR:
            case AOTExprKind::SHIFT_LEFT:
            case AOTExprKind::SHIFT_RIGHT: {
                std::string left_err;
                QoreValue left = readOneExpr(reader, ptr, end, left_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                std::string right_err;
                QoreValue right = readOneExpr(reader, ptr, end, right_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!left_err.empty() || !right_err.empty()) {
                    left.discard(nullptr);
                    right.discard(nullptr);
                    has_unsupported = true;
                } else if (kind == AOTExprKind::BIT_AND) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreBinaryAndOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::BIT_OR) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreBinaryOrOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::BIT_XOR) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreBinaryXorOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::SHIFT_LEFT) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreShiftLeftOperatorNode(&loc_builtin, left, right)));
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreShiftRightOperatorNode(&loc_builtin, left, right)));
                }
                continue;
            }
            case AOTExprKind::KEYS: {
                std::string operand_err;
                QoreValue operand = readOneExpr(reader, ptr, end, operand_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!operand_err.empty()) {
                    operand.discard(nullptr);
                    has_unsupported = true;
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreKeysOperatorNode(&loc_builtin, operand)));
                }
                continue;
            }
            case AOTExprKind::IMPLICIT_ELEM: {
                ctx->exprs[i] = toBitsNB(QoreValue(new QoreImplicitElementNode(&loc_builtin)));
                continue;
            }
            case AOTExprKind::INSTANCEOF: {
                const char* type_path = reader.readStringRef(ptr);
                std::string operand_err;
                QoreValue operand = readOneExpr(reader, ptr, end, operand_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!operand_err.empty()) {
                    operand.discard(nullptr);
                    has_unsupported = true;
                    continue;
                }
                if (!type_path || !*type_path) {
                    operand.discard(nullptr);
                    has_unsupported = true;
                    continue;
                }
                std::string type_error;
                QoreAOTTypeResolver resolver(pgm);
                const QoreTypeInfo* ti = resolver.resolve(type_path, type_error);
                if (!ti) {
                    printd(0, "AOT v2: cannot resolve type '%s' for INSTANCEOF: %s\n",
                        type_path, type_error.c_str());
                    operand.discard(nullptr);
                    has_unsupported = true;
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreInstanceOfOperatorNode(&loc_builtin, operand, ti)));
                }
                continue;
            }
            case AOTExprKind::REGEX_MATCH:
            case AOTExprKind::REGEX_NMATCH:
            case AOTExprKind::REGEX_EXTRACT: {
                const char* pattern = reader.readStringRef(ptr);
                int64_t options = QoreAOTBinaryReader::readI64(ptr);
                std::string operand_err;
                QoreValue operand = readOneExpr(reader, ptr, end, operand_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!operand_err.empty() || !pattern) {
                    operand.discard(nullptr);
                    has_unsupported = true;
                    continue;
                }
                ExceptionSink xsink;
                QoreRegex* re = new QoreRegex(pattern, options, &xsink);
                if (xsink) {
                    printd(0, "AOT v2: regex compile error for pattern '%s'\n", pattern);
                    delete re;
                    operand.discard(nullptr);
                    has_unsupported = true;
                    continue;
                }
                if (kind == AOTExprKind::REGEX_NMATCH) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreRegexNMatchOperatorNode(&loc_builtin, operand, re)));
                } else if (kind == AOTExprKind::REGEX_EXTRACT) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreRegexExtractOperatorNode(&loc_builtin, operand, re)));
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreRegexMatchOperatorNode(&loc_builtin, operand, re)));
                }
                continue;
            }
            case AOTExprKind::PRE_INC:
            case AOTExprKind::PRE_DEC:
            case AOTExprKind::POST_INC:
            case AOTExprKind::POST_DEC: {
                std::string operand_err;
                QoreValue operand = readOneExpr(reader, ptr, end, operand_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!operand_err.empty()) {
                    operand.discard(nullptr);
                    has_unsupported = true;
                    continue;
                }
                if (kind == AOTExprKind::PRE_INC) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QorePreIncrementOperatorNode(&loc_builtin, operand)));
                } else if (kind == AOTExprKind::PRE_DEC) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QorePreDecrementOperatorNode(&loc_builtin, operand)));
                } else if (kind == AOTExprKind::POST_INC) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QorePostIncrementOperatorNode(&loc_builtin, operand)));
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QorePostDecrementOperatorNode(&loc_builtin, operand)));
                }
                continue;
            }
            case AOTExprKind::LOG_EQ:
            case AOTExprKind::LOG_NE:
            case AOTExprKind::LOG_AEQ:
            case AOTExprKind::LOG_ANE:
            case AOTExprKind::LOG_LT:
            case AOTExprKind::LOG_GT:
            case AOTExprKind::LOG_LE:
            case AOTExprKind::LOG_GE:
            case AOTExprKind::LOG_AND:
            case AOTExprKind::LOG_OR:
            case AOTExprKind::NULL_COAL:
            case AOTExprKind::VALUE_COAL:
            case AOTExprKind::FOLDL:
            case AOTExprKind::FOLDR:
            case AOTExprKind::MAP:
            case AOTExprKind::SELECT:
            case AOTExprKind::PLUS_EQ:
            case AOTExprKind::MINUS_EQ:
            case AOTExprKind::MULTIPLY_EQ:
            case AOTExprKind::DIVIDE_EQ:
            case AOTExprKind::MODULO_EQ:
            case AOTExprKind::AND_EQ:
            case AOTExprKind::OR_EQ:
            case AOTExprKind::XOR_EQ:
            case AOTExprKind::SHL_EQ:
            case AOTExprKind::SHR_EQ:
            case AOTExprKind::ASSIGN: {
                std::string left_err;
                QoreValue left = readOneExpr(reader, ptr, end, left_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                std::string right_err;
                QoreValue right = readOneExpr(reader, ptr, end, right_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!left_err.empty() || !right_err.empty()) {
                    left.discard(nullptr);
                    right.discard(nullptr);
                    has_unsupported = true;
                } else if (kind == AOTExprKind::NULL_COAL) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreNullCoalescingOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::VALUE_COAL) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreValueCoalescingOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::FOLDL) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreFoldlOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::FOLDR) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreFoldrOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::MAP) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreMapOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::SELECT) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreSelectOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::ASSIGN) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreAssignmentOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::LOG_NE) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreLogicalNotEqualsOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::LOG_AEQ) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreLogicalAbsoluteEqualsOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::LOG_ANE) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreLogicalAbsoluteNotEqualsOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::LOG_LT) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreLogicalLessThanOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::LOG_GT) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreLogicalGreaterThanOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::LOG_LE) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreLogicalLessThanOrEqualsOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::LOG_GE) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreLogicalGreaterThanOrEqualsOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::LOG_AND) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreLogicalAndOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::LOG_OR) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreLogicalOrOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::PLUS_EQ) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QorePlusEqualsOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::MINUS_EQ) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreMinusEqualsOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::MULTIPLY_EQ) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreMultiplyEqualsOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::DIVIDE_EQ) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreDivideEqualsOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::MODULO_EQ) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreModuloEqualsOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::AND_EQ) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreAndEqualsOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::OR_EQ) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreOrEqualsOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::XOR_EQ) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreXorEqualsOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::SHL_EQ) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreShiftLeftEqualsOperatorNode(&loc_builtin, left, right)));
                } else if (kind == AOTExprKind::SHR_EQ) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreShiftRightEqualsOperatorNode(&loc_builtin, left, right)));
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreLogicalEqualsOperatorNode(&loc_builtin, left, right)));
                }
                continue;
            }
            case AOTExprKind::ITERATE: {
                std::string source_err;
                QoreValue source = readOneExpr(reader, ptr, end, source_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!source_err.empty()) {
                    source.discard(nullptr);
                    has_unsupported = true;
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreIterateOperatorNode(&loc_builtin, source)));
                }
                continue;
            }
            case AOTExprKind::STREAMING: {
                auto streaming_kind = static_cast<QoreStreamingOperatorNode::Kind>(
                    QoreAOTBinaryReader::readU8(ptr));
                std::string predicate_err;
                QoreValue predicate = readOneExpr(reader, ptr, end, predicate_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                std::string source_err;
                QoreValue source = readOneExpr(reader, ptr, end, source_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!predicate_err.empty() || !source_err.empty()) {
                    predicate.discard(nullptr);
                    source.discard(nullptr);
                    has_unsupported = true;
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreStreamingOperatorNode(&loc_builtin, streaming_kind, predicate, source)));
                }
                continue;
            }
            case AOTExprKind::MAP_SELECT: {
                std::string map_err;
                QoreValue map_expr = readOneExpr(reader, ptr, end, map_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                std::string source_err;
                QoreValue source = readOneExpr(reader, ptr, end, source_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                std::string where_err;
                QoreValue where_expr = readOneExpr(reader, ptr, end, where_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!map_err.empty() || !source_err.empty() || !where_err.empty()) {
                    map_expr.discard(nullptr);
                    source.discard(nullptr);
                    where_expr.discard(nullptr);
                    has_unsupported = true;
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreMapSelectOperatorNode(&loc_builtin, map_expr, source, where_expr)));
                }
                continue;
            }
            case AOTExprKind::HASH_MAP_OP:
            case AOTExprKind::HASH_MAP_SELECT_OP: {
                std::string key_err;
                QoreValue key_expr = readOneExpr(reader, ptr, end, key_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                std::string val_err;
                QoreValue val_expr = readOneExpr(reader, ptr, end, val_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                std::string source_err;
                QoreValue source = readOneExpr(reader, ptr, end, source_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (kind == AOTExprKind::HASH_MAP_OP) {
                    if (!key_err.empty() || !val_err.empty() || !source_err.empty()) {
                        key_expr.discard(nullptr);
                        val_expr.discard(nullptr);
                        source.discard(nullptr);
                        has_unsupported = true;
                    } else {
                        ctx->exprs[i] = toBitsNB(QoreValue(
                            new QoreHashMapOperatorNode(&loc_builtin, key_expr, val_expr, source)));
                    }
                    continue;
                }
                std::string where_err;
                QoreValue where_expr = readOneExpr(reader, ptr, end, where_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!key_err.empty() || !val_err.empty() || !source_err.empty() || !where_err.empty()) {
                    key_expr.discard(nullptr);
                    val_expr.discard(nullptr);
                    source.discard(nullptr);
                    where_expr.discard(nullptr);
                    has_unsupported = true;
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreHashMapSelectOperatorNode(&loc_builtin,
                            key_expr, val_expr, source, where_expr)));
                }
                continue;
            }
            case AOTExprKind::LOG_NOT: {
                std::string operand_err;
                QoreValue operand = readOneExpr(reader, ptr, end, operand_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!operand_err.empty()) {
                    operand.discard(nullptr);
                    has_unsupported = true;
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreLogicalNotOperatorNode(&loc_builtin, operand)));
                }
                continue;
            }
            case AOTExprKind::UNARY_MINUS: {
                std::string operand_err;
                QoreValue operand = readOneExpr(reader, ptr, end, operand_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!operand_err.empty()) {
                    operand.discard(nullptr);
                    has_unsupported = true;
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreUnaryMinusOperatorNode(&loc_builtin, operand)));
                }
                continue;
            }
            case AOTExprKind::QUESTION: {
                std::string cond_err;
                QoreValue cond = readOneExpr(reader, ptr, end, cond_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                std::string true_err;
                QoreValue true_expr = readOneExpr(reader, ptr, end, true_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                std::string false_err;
                QoreValue false_expr = readOneExpr(reader, ptr, end, false_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!cond_err.empty() || !true_err.empty() || !false_err.empty()) {
                    cond.discard(nullptr);
                    true_expr.discard(nullptr);
                    false_expr.discard(nullptr);
                    has_unsupported = true;
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreQuestionMarkOperatorNode(&loc_builtin, cond, true_expr, false_expr)));
                }
                continue;
            }
            case AOTExprKind::TRIM:
            case AOTExprKind::CHOMP:
            case AOTExprKind::POP:
            case AOTExprKind::SHIFT:
            case AOTExprKind::ELEMENTS:
            case AOTExprKind::DELETE:
            case AOTExprKind::REMOVE:
            case AOTExprKind::BACKGROUND: {
                std::string operand_err;
                QoreValue operand = readOneExpr(reader, ptr, end, operand_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!operand_err.empty()) {
                    operand.discard(nullptr);
                    has_unsupported = true;
                } else if (kind == AOTExprKind::TRIM) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreTrimOperatorNode(&loc_builtin, operand)));
                } else if (kind == AOTExprKind::CHOMP) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreChompOperatorNode(&loc_builtin, operand)));
                } else if (kind == AOTExprKind::POP) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QorePopOperatorNode(&loc_builtin, operand)));
                } else if (kind == AOTExprKind::SHIFT) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreShiftOperatorNode(&loc_builtin, operand)));
                } else if (kind == AOTExprKind::ELEMENTS) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreElementsOperatorNode(&loc_builtin, operand)));
                } else if (kind == AOTExprKind::DELETE) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreDeleteOperatorNode(&loc_builtin, operand)));
                } else if (kind == AOTExprKind::REMOVE) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreRemoveOperatorNode(&loc_builtin, operand)));
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreBackgroundOperatorNode(&loc_builtin, operand)));
                }
                continue;
            }
            case AOTExprKind::PUSH:
            case AOTExprKind::UNSHIFT: {
                std::string left_err;
                QoreValue left = readOneExpr(reader, ptr, end, left_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                std::string right_err;
                QoreValue right = readOneExpr(reader, ptr, end, right_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!left_err.empty() || !right_err.empty()) {
                    left.discard(nullptr);
                    right.discard(nullptr);
                    has_unsupported = true;
                } else if (kind == AOTExprKind::PUSH) {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QorePushOperatorNode(&loc_builtin, left, right)));
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreUnshiftOperatorNode(&loc_builtin, left, right)));
                }
                continue;
            }
            case AOTExprKind::DOT_EVAL_EXPR: {
                std::string dot_err;
                QoreValue dot_expr = readOneExpr(reader, ptr, end, dot_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals, local_owner_pgm);
                if (!dot_err.empty()) {
                    dot_expr.discard(nullptr);
                    has_unsupported = true;
                } else {
                    ctx->exprs[i] = toBitsNB(dot_expr);
                }
                continue;
            }
            case AOTExprKind::CONTEXT_REF: {
                const char* member = reader.readStringRef(ptr);
                ctx->exprs[i] = toBitsNB(QoreValue(
                    new ContextrefNode(&loc_builtin, strdup(member ? member : ""))));
                continue;
            }
            case AOTExprKind::CONTEXT_ROW: {
                ctx->exprs[i] = toBitsNB(QoreValue(new ContextRowNode(&loc_builtin)));
                continue;
            }
            case AOTExprKind::COMPLEX_CONTEXT_REF: {
                const char* ctx_name = reader.readStringRef(ptr);
                const char* member = reader.readStringRef(ptr);
                int64_t stack_offset = QoreAOTBinaryReader::readI64(ptr);

                std::string spec = ctx_name ? ctx_name : "";
                spec += ":";
                spec += member ? member : "";
                auto* node = new ComplexContextrefNode(&loc_builtin, strdup(spec.c_str()));
                node->stack_offset = static_cast<int>(stack_offset);
                ctx->exprs[i] = toBitsNB(QoreValue(node));
                continue;
            }
            case AOTExprKind::CLOSURE_CREATE: {
                // Read flags
                const char* flags_str = reader.readStringRef(ptr);
                const char* class_type_path = reader.readStringRef(ptr);
                const char* native_body_key = nullptr;
                if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_NATIVE_CLOSURE_BODY) != 0) {
                    native_body_key = reader.readStringRef(ptr);
                }

                bool is_lambda = false, is_in_method = false;
                if (flags_str) {
                    is_lambda = (flags_str[0] == '1');
                    is_in_method = (strlen(flags_str) >= 3 && flags_str[2] == '1');
                }

                // Read return type
                const char* ret_type_path = reader.readStringRef(ptr);
                std::string type_error;
                QoreAOTTypeResolver type_resolver(pgm);
                const QoreTypeInfo* ret_type = (ret_type_path && *ret_type_path)
                    ? type_resolver.resolve(ret_type_path, type_error) : nullptr;

                // Read params
                uint16_t closure_num_params = QoreAOTBinaryReader::readU16(ptr);
                std::vector<std::string> param_names(closure_num_params);
                std::vector<const QoreTypeInfo*> param_types(closure_num_params);
                std::vector<QoreValue> defaults(closure_num_params);
                for (uint16_t p = 0; p < closure_num_params; ++p) {
                    const char* pname = reader.readStringRef(ptr);
                    param_names[p] = pname ? pname : "";
                    const char* ptype = reader.readStringRef(ptr);
                    param_types[p] = (ptype && *ptype)
                        ? type_resolver.resolve(ptype, type_error) : nullptr;
                    uint8_t has_default = QoreAOTBinaryReader::readU8(ptr);
                    if (has_default) {
                        std::string val_error;
                        defaults[p] = reader.readValue(ptr, end, val_error);
                    }
                }
                bool closure_sig_has_varargs = false;
                bool closure_needs_extra_args = false;
                if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_CLOSURE_VARARGS_FLAGS) != 0) {
                    uint16_t closure_flags = QoreAOTBinaryReader::readU16(ptr);
                    closure_needs_extra_args = (closure_flags & 0x0001) != 0;
                    closure_sig_has_varargs = (closure_flags & 0x0004) != 0;
                } else {
                    bool old_varargs = QoreAOTBinaryReader::readU8(ptr) != 0;
                    closure_sig_has_varargs = old_varargs;
                    closure_needs_extra_args = old_varargs;
                }

                // Read captured variable names and parent slot indices
                uint16_t num_captured = QoreAOTBinaryReader::readU16(ptr);
                std::vector<std::string> captured_names(num_captured);
                std::vector<int32_t> captured_parent_slots(num_captured, -1);
                for (uint16_t c = 0; c < num_captured; ++c) {
                    const char* cname = reader.readStringRef(ptr);
                    captured_names[c] = cname ? cname : "";
                    captured_parent_slots[c] = static_cast<int32_t>(QoreAOTBinaryReader::readU32(ptr));
                }

                // Read closure body IR
                uint8_t has_ir = QoreAOTBinaryReader::readU8(ptr);
                if (!has_ir) {
                    // No IR: invalid for source-fallback-free AOT.
                    std::string msg = "closure expression slot " + std::to_string(i)
                        + " has no serialized IR";
                    setClosureIRError(msg);
                    if (trace_slot_reg) {
                        fprintf(stderr, "[aot-slot-reg] '%s': %s\n", name, msg.c_str());
                    }
                    closure_ir_missing = true;
                    continue;
                }

                uint32_t ir_size = QoreAOTBinaryReader::readU32(ptr);
                const uint8_t* ir_start_ptr = ptr;
                const uint8_t* ir_boundary = entry_end ? entry_end : end;
                if (ptr > ir_boundary
                        || ir_size > static_cast<size_t>(ir_boundary - ptr)) {
                    std::string msg = "closure expression slot " + std::to_string(i)
                        + " IR range exceeds slot map entry boundary";
                    setClosureIRError(msg);
                    closure_ir_missing = true;
                    ptr = ir_boundary;
                    continue;
                }
                const uint8_t* ir_end_ptr = ptr + ir_size;

                // Resolve class for method context
                // Establish a current-Program context for BOTH the class-ref
                // resolution below and the closure constructor.  Class resolution
                // can fall back to parseFindClass() (for private/internal classes
                // that are not in the committed runtime maps), which reads the parse
                // options of the *current* Program; without this context getProgram()
                // is null and the lookup dereferences it — crashing, e.g., for a
                // closure inside a method capturing a parameter, where the method's
                // class is resolved here at load time.
                //
                // A plain current-Program context (not a parse lock) is used: this
                // AOT slot-map reconstruction can run while the global AOT init lock
                // is held, and taking a Program parse lock here can invert lock order
                // with another loader thread that already holds the Program parse lock
                // and is waiting for AOT init.
                QoreProgramContextHelper closure_pch(pgm);

                const QoreClass* closure_class = nullptr;
                if (class_type_path && *class_type_path) {
                    closure_class = qore_aot_resolve_class_ref(pgm, class_type_path, false);
                }

                // Construct UserClosureFunction + UserClosureVariant FIRST
                // so signature locals are available for IR deserialization.
                auto* ucf = new UserClosureFunction(nullptr, 0, 0, QoreValue(), nullptr);
                auto* closure_variant = static_cast<UserClosureVariant*>(
                    const_cast<AbstractQoreFunctionVariant*>(ucf->first()));
                UserSignature* closure_sig = closure_variant->getUserSignature();
                closure_sig->setupFromAOTMetadata(
                    pgm, ret_type,
                    std::move(param_names), std::move(param_types), std::move(defaults),
                    closure_sig_has_varargs, closure_class, nullptr, 0, 0,
                    std::vector<uint8_t>(), local_owner_pgm);
                if (closure_needs_extra_args) {
                    closure_variant->setFlag(QCF_USES_EXTRA_ARGS);
                }

                // Build enclosing locals map so IR deserialization reuses the same
                // LocalVar* objects that the parent function and closure signature use.
                // This is critical: closure variable lookup uses pointer identity.
                std::unordered_map<std::string, LocalVar*> enclosing_locals;
                // 1. Parent function's body locals from ctx->locals[]
                for (int l = 0; l < num_locals; ++l) {
                    if (ctx->locals[l] && ctx->locals[l]->getName()) {
                        enclosing_locals[ctx->locals[l]->getName()] = ctx->locals[l];
                    }
                }
                // 1b. Override with slot-indexed captured variables for disambiguation
                // of same-named variables in different scopes. The captured_parent_slots
                // array has the correct parent slot index for each captured variable.
                for (uint16_t ci = 0; ci < num_captured; ++ci) {
                    int32_t parent_slot = captured_parent_slots[ci];
                    if (parent_slot >= 0 && parent_slot < num_locals
                            && ctx->locals[parent_slot]) {
                        enclosing_locals[captured_names[ci]] = ctx->locals[parent_slot];
                    }
                }
                // 1c. Top-level locals live in the program LVList, not in the
                // enclosing method/function context.
                if (const LVList* top_lvars = qore_program_private::get(*pgm)->sb.getLVList()) {
                    for (unsigned tl = 0; tl < top_lvars->size(); ++tl) {
                        LocalVar* lv = top_lvars->lv[tl];
                        if (lv && lv->getName() && !enclosing_locals.count(lv->getName())) {
                            enclosing_locals[lv->getName()] = lv;
                        }
                    }
                }
                // 2. Parent function's parameter locals from the parent variant's
                // signature — these are NOT in ctx->locals[] when the parameter
                // is only used inside the closure body (not in the parent's AOT code)
                if (sig) {
                    for (unsigned p = 0; p < sig->numParams(); ++p) {
                        if (sig->lv[p] && sig->lv[p]->getName()) {
                            // Don't overwrite if already present from ctx->locals[]
                            enclosing_locals.emplace(sig->lv[p]->getName(), sig->lv[p]);
                        }
                    }
                    if (sig->argvid) {
                        enclosing_locals.emplace("argv", sig->argvid);
                    }
                    if (sig->selfid) {
                        enclosing_locals.emplace("self", sig->selfid);
                    }
                }
                // 3. Closure's own parameter locals from its signature
                for (unsigned p = 0; p < closure_sig->numParams(); ++p) {
                    const char* pname = closure_sig->getName(p);
                    if (pname && *pname) {
                        enclosing_locals[pname] = closure_sig->lv[p];
                    }
                }
                // 4. Closure's own argv and self from its signature
                if (closure_sig->argvid) {
                    enclosing_locals["argv"] = closure_sig->argvid;
                }
                LocalVar* captured_selfid = nullptr;
                auto captured_self_it = enclosing_locals.find("self");
                if (captured_self_it != enclosing_locals.end()) {
                    captured_selfid = captured_self_it->second;
                }
                if (closure_sig->selfid) {
                    if (captured_selfid) {
                        enclosing_locals["self"] = captured_selfid;
                    } else {
                        enclosing_locals["self"] = closure_sig->selfid;
                    }
                }

                // Build the locals array used by EXPR_TREE blob deserialization.
                // Closure body expressions are serialized against the closure IR
                // local slot table; deserializeIRFunction() fills this vector by
                // slot ID after reading that table and before reading instruction
                // expression fields.
                std::vector<LocalVar*> closure_locals_vec;

                // Deserialize closure body IR.  Capture closure_locals_vec by
                // reference so deserializeIRFunction's local-slot-table fill is
                // visible to readExprCb when instruction reading fires readExpr
                // calls.
                std::string ir_error;
                auto readExprCb = [pgm, ctx, num_globals, &closure_locals_vec, local_owner_pgm]
                        (const QoreAOTBinaryReader& rdr, const uint8_t*& p,
                        const uint8_t* e, std::string& err) -> QoreValue {
                    LocalVar** arr = closure_locals_vec.empty()
                        ? nullptr : closure_locals_vec.data();
                    int cnt = static_cast<int>(closure_locals_vec.size());
                    return readOneTopLevelIRExpr(rdr, p, e, err, pgm,
                        arr, cnt, ctx->globals, num_globals, local_owner_pgm);
                };
                const bool native_metadata_only = closure_bindings
                    && native_body_key && *native_body_key
                    && std::getenv("QORE_DISABLE_AOT_NATIVE_CLOSURES") == nullptr;
                const bool lazy_fallback = !native_metadata_only
                    && debug_metadata && slot_maps_start
                    && ir_start_ptr >= slot_maps_start
                    && std::getenv("QORE_DISABLE_AOT_LAZY_CLOSURE_IR") == nullptr;
                auto closure_ir = deserializeIRFunction(reader, ptr, ir_end_ptr, pgm,
                    readExprCb, &enclosing_locals, ir_error,
                    ctx->locals, num_locals, &closure_locals_vec,
                    false, nullptr, local_owner_pgm,
                    native_metadata_only || lazy_fallback);
                ptr = ir_end_ptr;  // Ensure we advance past IR data

                if (!closure_ir) {
                    std::string msg = "closure expression slot " + std::to_string(i)
                        + " IR deserialization failed";
                    if (!ir_error.empty()) {
                        msg += ": ";
                        msg += ir_error;
                    }
                    setClosureIRError(msg);
                    printd(2, "AOT: %s for '%s'\n", msg.c_str(), name);
                    if (trace_slot_reg) {
                        fprintf(stderr, "[aot-slot-reg] '%s': %s\n", name, msg.c_str());
                    }
                    delete ucf;
                    closure_ir_missing = true;
                    continue;
                }

                // Set up captured variables in LVarSet and ensure closureUse
                // is set on parent-scope LocalVars.  This is critical: setupCall()
                // uses closureUse to decide whether to instantiate a parameter on
                // the cvstack (closure variable stack) vs lvstack.  Without this,
                // thread_find_closure_var() returns null at closure creation time.
                //
                // Use the closure IR's local_var_slots to find the correct LocalVar*
                // for each captured variable. This handles same-named variables in
                // different scopes correctly, since the IR deserialization already
                // resolved variables using the enclosing_locals map (which was
                // updated by deserializeIRFunction for non-duplicate entries).
                LVarSet* closure_vlist = ucf->getVList();
                for (uint16_t ci = 0; ci < num_captured; ++ci) {
                    const std::string& cap_name = captured_names[ci];
                    int32_t parent_slot = captured_parent_slots[ci];
                    LocalVar* lv = nullptr;

                    // Use parent slot index for disambiguation when available
                    if (parent_slot >= 0 && parent_slot < num_locals
                            && ctx->locals[parent_slot]) {
                        lv = ctx->locals[parent_slot];
                    } else {
                        // Fall back to name-based lookup
                        auto it = enclosing_locals.find(cap_name);
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

                LocalVar* closure_selfid = captured_selfid ? captured_selfid : closure_sig->selfid;
                if (!closure_selfid) {
                    auto self_it = enclosing_locals.find("self");
                    if (self_it != enclosing_locals.end()) {
                        closure_selfid = self_it->second;
                    }
                }
                if (closure_selfid && !closure_sig->selfid) {
                    closure_sig->setSelfId(closure_selfid);
                }

                if (lazy_fallback) {
                    auto lazy_ir = std::make_shared<QoreAOTLazyClosureIR>();
                    lazy_ir->metadata = debug_metadata;
                    lazy_ir->ir_offset = static_cast<uint32_t>(ir_start_ptr - slot_maps_start);
                    lazy_ir->ir_size = ir_size;
                    lazy_ir->pgm = pgm;
                    lazy_ir->local_owner_pgm = local_owner_pgm;
                    if (num_locals) {
                        lazy_ir->parent_locals.assign(ctx->locals, ctx->locals + num_locals);
                    }
                    if (num_globals) {
                        lazy_ir->globals.assign(ctx->globals, ctx->globals + num_globals);
                    }
                    lazy_ir->enclosing_locals = std::move(enclosing_locals);
                    lazy_ir->body_locals = closure_ir->all_body_locals;
                    closure_variant->setLazyAOTClosureIR(std::move(lazy_ir));
                } else if (!native_metadata_only) {
                    // Restore the parameter mapping used by IR analysis.
                    // setCachedIR() derives the pre-instantiated local sets once
                    // after this mapping is complete.
                    bool parameter_restore_cancelled = false;
                    for (unsigned p = 0; p < closure_sig->numParams(); ++p) {
                        if (p && !(p % 100)
                                && qore_check_cancel(nullptr,
                                    "AOT closure parameter metadata restoration")) {
                            parameter_restore_cancelled = true;
                            break;
                        }
                        if (closure_sig->lv[p]) {
                            closure_ir->param_local_vars[static_cast<int>(p)] = closure_sig->lv[p];
                        }
                    }
                    if (parameter_restore_cancelled) {
                        setClosureIRError("closure parameter metadata restoration was cancelled");
                        delete ucf;
                        closure_ir_missing = true;
                        continue;
                    }

                    // Set cached IR on variant and promote to TIER_IR
                    makeRuntimeDeserializedClosureIRNameUnique(*closure_ir, closure_variant);
                    closure_ir->computeSlotIdsAndEmbed();
                    closure_variant->setCachedIR(closure_ir.release());
                }
                closure_variant->pgm = pgm;

                if (closure_bindings && native_body_key && *native_body_key) {
                    AOTClosureRuntimeBinding binding;
                    binding.uvb = closure_variant;
                    binding.local_slots = closure_locals_vec;
                    binding.class_ctx = closure_class
                        ? qore_class_private::get(*const_cast<QoreClass*>(closure_class)) : nullptr;
                    (*closure_bindings)[native_body_key] = std::move(binding);
                    if (getenv("QORE_AOT_DEBUG_NATIVE_CLOSURES")) {
                        fprintf(stderr, "AOT: resolved native closure '%s'\n", native_body_key);
                    }
                }

                // Set class type if in a method context
                if (class_type_path && *class_type_path) {
                    ucf->setClassType(type_resolver.resolve(class_type_path, type_error));
                }

                // Create QoreClosureParseNode
                auto* closure_node = new QoreClosureParseNode(nullptr, ucf, is_lambda, is_in_method);
                ctx->exprs[i] = toBitsNB(QoreValue(closure_node));
                continue;
            }
            case AOTExprKind::CONST_INT: {
                const char* sv = reader.readStringRef(ptr);
                ctx->exprs[i] = toBitsNB(QoreValue(strtoll(sv ? sv : "0", nullptr, 10)));
                continue;
            }
            case AOTExprKind::CONST_FLOAT: {
                const char* sv = reader.readStringRef(ptr);
                ctx->exprs[i] = toBitsNB(QoreValue(strtod(sv ? sv : "0", nullptr)));
                continue;
            }
            case AOTExprKind::CONST_BOOL: {
                const char* sv = reader.readStringRef(ptr);
                ctx->exprs[i] = toBitsNB(QoreValue((bool)(sv && sv[0] == '1')));
                continue;
            }
            case AOTExprKind::CONST_VALUE: {
                std::string value_error;
                QoreValue value = reader.readValue(ptr, end, value_error);
                if (!value_error.empty()) {
                    printd(0, "AOT v2: error reading const value for '%s' expr slot %d: %s\n",
                        name, i, value_error.c_str());
                    has_unsupported = true;
                    continue;
                }
                ctx->exprs[i] = toBitsNB(value);
                continue;
            }
            case AOTExprKind::CONST_NOTHING:
                ctx->exprs[i] = toBitsNB(QoreValue());
                continue;
            case AOTExprKind::CONST_NULL:
                ctx->exprs[i] = toBitsNB(QoreValue(null()));
                continue;
            case AOTExprKind::CALL_REF:
            case AOTExprKind::OBJ_METHOD_REF:
            case AOTExprKind::GENERIC_EVAL:
            default:
                break;
        }

        // Handle LOCAL_VARREF directly since it needs ctx->locals
        if (kind == AOTExprKind::LOCAL_VARREF && ref1) {
            int local_slot = std::atoi(ref1);
            if (local_slot >= 0 && local_slot < ctx->num_locals && ctx->locals[local_slot]) {
                // Create a VarRefNode pointing to the local variable
                // NOTE: always use false for in_closure — VT_LOCAL type calls ref.id->eval() which
                // internally checks closure_use and uses the correct lookup.
                LocalVar* lv = ctx->locals[local_slot];
                VarRefNode* vrn = new VarRefNode(&loc_builtin, lv->getName(), lv, false);
                ctx->exprs[i] = toBitsNB(QoreValue(vrn));
                continue;
            } else {
                printd(0, "AOT v2: invalid local slot %d for LOCAL_VARREF expr slot %d (num_locals=%d)\n",
                    local_slot, i, ctx->num_locals);
                has_unsupported = true;
            }
        }

        // Handle GLOBAL_VARREF directly since it needs ctx->globals
        if (kind == AOTExprKind::GLOBAL_VARREF && ref1) {
            Var* gvar = resolveGlobalVarRefPayload(ref1, pgm, ctx->globals, ctx->num_globals);
            if (gvar) {
                GlobalVarRefNode* vrn = new GlobalVarRefNode(&loc_builtin, strdup(gvar->getName()), gvar);
                ctx->exprs[i] = toBitsNB(QoreValue(vrn));
                continue;
            } else {
                printd(0, "AOT v2: invalid global ref '%s' for GLOBAL_VARREF expr slot %d (num_globals=%d)\n",
                    ref1, i, ctx->num_globals);
                has_unsupported = true;
            }
        }

        uint64_t bits = resolveExprSlot(kind, ref1, ref2, pgm,
            uvb ? uvb->getUserSignature() : nullptr);
        if (bits) {
            ctx->exprs[i] = bits;
        } else if (kind != AOTExprKind::GENERIC_EVAL) {
            printd(2, "AOT buildCtx: '%s' unresolved expr[%d] kind=%d ref1='%s' ref2='%s'\n",
                name, i, (int)kind, ref1 ? ref1 : "", ref2 ? ref2 : "");
            if (trace_slot_reg) {
                fprintf(stderr, "[aot-slot-reg] '%s': unresolved expr[%d] kind=%d ref1='%s' ref2='%s'\n",
                    name, i, static_cast<int>(kind), ref1 ? ref1 : "", ref2 ? ref2 : "");
            }
            if (kind == AOTExprKind::RUNTIME_CONST_REF) {
                std::string msg = "cannot resolve runtime constant reference '";
                msg += ref1 ? ref1 : "";
                msg += "' in expression slot ";
                msg += std::to_string(i);
                msg += "; if this reference came from qcc --stub, the runtime host must inject the external "
                    "constant before loading the AOT binary";
                setBuildError(msg);
            }
            has_unsupported = true;
        }

        // Phase 1 validation: detect pointer overrun (indicates serializer bug)
        if (ptr > end) {
            printd(0, "AOT buildCtx: '%s' expr slot %d overran section boundary by %d bytes\n",
                name, i, (int)(ptr - end));
            has_unsupported = true;
            break;
        }
    }

    // Phase 1 validation: ensure we didn't read past the section boundary
    assert(ptr <= end && "buildContextFromSlotMap overran section boundary");

    // Process deferred EXPR_TREE blobs now that all CLOSURE_CREATE slots are resolved
    // Init functions (__const_init::, __svar_init::) use LLVM-compiled code that
    // dispatches via invoke_opcode + operands for binary/unary ops. Their EXPR_TREE
    // slots are only used for call target resolution, which tolerates empty slots.
    // Constant references in init function EXPR_TREE blobs may evaluate to NOTHING
    // because the referenced constant's own init hasn't run yet (init order dependency).
    // Tolerating this is safe because the LLVM code doesn't use the slot for the
    // binary-op path (it uses qore_rt_binary_op directly).
    bool is_init_func = isAOTInitFunctionName(name);
    for (auto& dt : deferred_expr_trees) {
        ExprTreeDeserializer deser(dt.blob_data, dt.blob_size, pgm, ctx);
        uint64_t bits = deser.deserialize();
        if (bits || !deser.hasFailed()) {
            ctx->exprs[dt.slot] = bits;
        } else if (is_init_func) {
            // Init function: tolerate EXPR_TREE failure (init order dependency)
            printd(3, "AOT v2: EXPR_TREE slot %d of init func '%s' failed (tolerated)\n",
                dt.slot, name);
        } else {
            printd(2, "AOT v2: EXPR_TREE deserialization failed for expr slot %d of '%s'\n",
                dt.slot, name);
            has_unsupported = true;
        }
    }

    // Pre-resolve call targets for expr slots to avoid per-call dynamic_cast overhead
    // in qore_rt_call_direct_aot, qore_rt_call_static_method_direct_aot, etc.
    for (int i = 0; i < num_exprs; ++i) {
        if (!ctx->exprs[i]) {
            continue;
        }
        QoreValue expr_val;
        std::memcpy(&expr_val, &ctx->exprs[i], sizeof(expr_val));
        if (!expr_val.hasNode()) {
            continue;
        }
        const AbstractQoreNode* node = expr_val.getInternalNode();
        // Function call
        const auto* fcn = dynamic_cast<const FunctionCallNode*>(node);
        if (fcn && fcn->getFunction()) {
            ctx->call_targets[i].func = fcn->getFunction();
            ctx->call_targets[i].pgm = fcn->getProgram();
            ctx->call_targets[i].explicit_type_param_instantiation =
                fcn->getExplicitTypeParamInstantiation();
            // Only use variant if it was resolved at parse time (requires args for overload
            // resolution). Do NOT fall back to first() — for overloaded builtins like int(),
            // picking the wrong variant causes CodeEvaluationHelper to discard args during
            // re-dispatch, resulting in "got no value" errors. Leave variant=nullptr so the
            // call path uses dynamic dispatch (evalDynamic) which resolves correctly.
            const AbstractQoreFunctionVariant* v = fcn->getVariant();
            if (v) {
                ctx->call_targets[i].variant = v;
                ctx->call_targets[i].uvb = v->getUserVariantBase();
            }
            continue;
        }
        // Static method call
        const auto* smc = dynamic_cast<const StaticMethodCallNode*>(node);
        if (smc && smc->getMethod()) {
            ctx->call_targets[i].method = smc->getMethod();
            ctx->call_targets[i].is_static_method = true;
            ctx->call_targets[i].receiver_type_info = smc->getReceiverTypeInfo();
            ctx->call_targets[i].explicit_type_param_instantiation =
                smc->getExplicitTypeParamInstantiation();
            const AbstractQoreFunctionVariant* v = smc->getVariant();
            if (v) {
                ctx->call_targets[i].variant = v;
                ctx->call_targets[i].uvb = v->getUserVariantBase();
            }
            continue;
        }
        // Self method call
        const auto* self_call = dynamic_cast<const SelfFunctionCallNode*>(node);
        if (self_call && self_call->getMethod()) {
            ctx->call_targets[i].method = self_call->getMethod();
            ctx->call_targets[i].qc = self_call->getClass();
            ctx->call_targets[i].method_name = self_call->getName();
            ctx->call_targets[i].variant = self_call->getVariant();
            if (ctx->call_targets[i].variant) {
                ctx->call_targets[i].uvb = ctx->call_targets[i].variant->getUserVariantBase();
            }
            ctx->call_targets[i].class_ctx = self_call->getClassContext();
            ctx->call_targets[i].is_self_method = true;
            ctx->call_targets[i].self_ns_single = self_call->hasSingleName();
            ctx->call_targets[i].self_is_copy = self_call->isCopyCall();
            ctx->call_targets[i].self_is_abstract = self_call->isAbstractCall();
            continue;
        }
        // Dot-eval method call (obj.method())
        const auto* dot_eval = dynamic_cast<const QoreDotEvalOperatorNode*>(node);
        if (dot_eval) {
            auto* mc = dot_eval->getMethodCall();
            if (mc) {
                ctx->call_targets[i].method = mc->getMethod();
                ctx->call_targets[i].qc = mc->getClass();
                ctx->call_targets[i].is_pseudo = mc->isPseudo();
                ctx->call_targets[i].method_name = mc->getName();
                // Resolve variant from method only when the method has exactly
                // ONE variant — in that case, picking first() is safe and enables
                // the fast-dispatch path. For overloaded methods, leave variant
                // null so the runtime does proper arg-type-based overload
                // resolution via evalTmpArgs. Falling back to first() for
                // overloaded methods is UNSAFE: try_dispatch_method_fast uses
                // the variant's signature directly to bind args, so a 2-arg
                // call against a 1-arg first() variant binds the first arg to
                // the wrong parameter slot (e.g. `pipe.append(id, elem)` got
                // routed to `append(Processor)` with id standing in for
                // processor — a plain integer — producing
                // `append(integer, integer)` overload errors).
                if (ctx->call_targets[i].method) {
                    MethodFunctionBase* mfb = qore_method_private::get(
                        *ctx->call_targets[i].method)->getFunction();
                    if (mfb && mfb->numVariants() == 1) {
                        const AbstractQoreFunctionVariant* v = mc->getVariant();
                        ctx->call_targets[i].variant = v ? v : mfb->first();
                    }
                }
            }
            continue;
        }
    }

    // Reuse LocalVars from ctx->locals[] where possible so all_body_locals
    // (pre-instantiated by evalTiered) and ctx->locals[] (loaded/stored by AOT
    // code) share LocalVar/name.c_str() identity.  If a body local has no AOT
    // local slot, fall back to the matching AST statement local for callbacks.
    //
    // CRITICAL: params/self/argv must be EXCLUDED from the reuse pool — they
    // are already instantiated by the caller (setupCall / signature
    // instantiation), so reusing a param's LocalVar for a body local that
    // happens to share the same name would double-instantiate the param and
    // leave the real body-local slot dangling off the lvstack.
    std::unordered_set<const LocalVar*> sig_like_locals;
    if (sig) {
        for (unsigned p = 0; p < sig->lv.size(); ++p) {
            if (sig->lv[p]) {
                sig_like_locals.insert(sig->lv[p]);
            }
        }
        if (sig->selfid) {
            sig_like_locals.insert(sig->selfid);
        }
        if (sig->argvid) {
            sig_like_locals.insert(sig->argvid);
        }
    }

    std::unordered_map<std::string, std::deque<LocalVar*>> local_name_map;
    for (int i = 0; i < num_locals; ++i) {
        LocalVar* slot_lv = ctx->locals[i];
        if (!slot_lv || sig_like_locals.count(slot_lv)) {
            continue;
        }
        local_name_map[slot_lv->getName()].push_back(slot_lv);
    }

    std::unordered_map<std::string, std::deque<LocalVar*>> stmt_body_local_map;
    for (LocalVar* slv : stmt_locals) {
        if (slv && slv->getName()) {
            stmt_body_local_map[slv->getName()].push_back(slv);
        }
    }

    for (int i = 0; i < num_body_locals; ++i) {
        const char* blname = reader.readStringRef(ptr);
        const char* bltype = reader.readStringRef(ptr);
        uint8_t bl_flags = QoreAOTBinaryReader::readU8(ptr);
        uint32_t bl_slot_id = UINT32_MAX;
        if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_BODY_LOCAL_SLOT) != 0) {
            bl_slot_id = QoreAOTBinaryReader::readU32(ptr);
        }

        LocalVar* slot_lv = (bl_slot_id != UINT32_MAX && bl_slot_id < num_locals)
            ? ctx->locals[bl_slot_id] : nullptr;
        if (!slot_lv) {
            slot_lv = popMatchingAOTLocal(local_name_map, blname, bltype, ctx_type_resolver);
        }
        LocalVar* stmt_lv = popMatchingAOTLocal(stmt_body_local_map, blname, bltype, ctx_type_resolver);
        LocalVar* lv = slot_lv ? slot_lv : stmt_lv;
        if (!lv) {
            std::string type_error;
            const QoreTypeInfo* ti = nullptr;
            if (bltype && *bltype) {
                ti = ctx_type_resolver->resolve(bltype, type_error);
                if (!type_error.empty()) {
                    type_error.clear();
                }
            }

            lv = local_pp->createLocalVar(blname ? blname : "", ti);
        }
        if ((bl_flags & 0x01) && !lv->closureUse()) {
            lv->setClosureUse();
        }
        if ((bl_flags & 0x02) && !lv->isReadOnly()) {
            lv->setReadOnly();
        }
        ctx->all_body_locals.push_back(lv);
    }

    // Deserialize regex cases
    if (num_regex_cases > 0) {
        ExceptionSink xsink;
        for (int i = 0; i < num_regex_cases; ++i) {
            const char* pattern = reader.readStringRef(ptr);
            int64_t options = QoreAOTBinaryReader::readI64(ptr);
            bool is_negated = QoreAOTBinaryReader::readU8(ptr) != 0;
            QoreRegex* re = new QoreRegex(pattern, options, &xsink);
            if (xsink) {
                delete re;
                has_unsupported = true;
                continue;
            }
            ctx->regex_cases[i] = is_negated
                ? new CaseNodeNegRegex(&loc_builtin, re, nullptr)
                : new CaseNodeRegex(&loc_builtin, re, nullptr);
        }
    }

    // Deserialize LValuePath instructions
    if (num_lv_path_insts > 0) {
        for (int i = 0; i < num_lv_path_insts; ++i) {
            uint16_t opcode = QoreAOTBinaryReader::readU16(ptr);
            auto pi = std::make_unique<QoreIRLValuePathInstruction>(
                static_cast<QoreIROpcode>(opcode));
            pi->weak = QoreAOTBinaryReader::readU8(ptr) != 0;
            pi->compound_op = static_cast<LVCompoundOp>(QoreAOTBinaryReader::readU8(ptr));
            pi->unary_op = static_cast<LVUnaryOp>(QoreAOTBinaryReader::readU8(ptr));
            pi->binary_mut_op = static_cast<LVBinaryMutOp>(QoreAOTBinaryReader::readU8(ptr));
            pi->ternary_op = static_cast<LVTernaryOp>(QoreAOTBinaryReader::readU8(ptr));
            pi->ref_rv = QoreAOTBinaryReader::readU8(ptr) != 0;
            if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_LVPATH_DELETE_EXPR) != 0) {
                if (QoreAOTBinaryReader::readU8(ptr)) {
                    std::string expr_error;
                    QoreValue legacy_delete_lvalue_expr = readOneExpr(reader, ptr, end, expr_error, pgm,
                        ctx->locals, ctx->num_locals, ctx->globals, ctx->num_globals, local_owner_pgm);
                    legacy_delete_lvalue_expr.discard(nullptr);
                    if (!expr_error.empty()) {
                        printd(2, "AOT buildCtx: '%s' LValuePath delete expr deser failed: %s\n",
                            name, expr_error.c_str());
                        delete ctx;
                        return nullptr;
                    }
                }
            }
            // Pattern info for RegexSubst / Transliterate binary_mut ops.
            // The writer always emits a present_flag u8, followed by the pattern
            // data when present.  Reconstructs the matching Qore runtime node
            // and attaches it via pattern_expr so the RegexSubst/Transliterate
            // runtime helper can dynamic_cast it out and execute the regex.
            uint8_t pattern_present = QoreAOTBinaryReader::readU8(ptr);
            if (pattern_present) {
                const char* pattern_str = reader.readStringRef(ptr);
                const char* newstr_str = reader.readStringRef(ptr);
                int64_t pat_options = QoreAOTBinaryReader::readI64(ptr);
                uint8_t pat_global = QoreAOTBinaryReader::readU8(ptr);
                if (pi->binary_mut_op == LVBinaryMutOp::RegexSubst) {
                    // Use the default constructor (allocates str + newstr) and
                    // then feed the pattern/replacement/options via the concat /
                    // setGlobal helpers, mirroring what the parser does.  The
                    // runtime constructor taking a C-string expects newstr to be
                    // provided separately via the concatTarget API but does NOT
                    // allocate a newstr buffer, so using it here would crash the
                    // destructor.
                    auto* rs = new QoreRegexSubst();
                    if (pattern_str) {
                        for (const char* p = pattern_str; *p; ++p) {
                            rs->concatSource(*p);
                        }
                    }
                    // Apply options bits (combined with PCRE2_UTF already set by ctor)
                    rs->addOptions(static_cast<int>(pat_options));
                    if (pat_global) {
                        rs->setGlobal();
                    }
                    if (newstr_str) {
                        for (const char* p = newstr_str; *p; ++p) {
                            rs->concatTarget(*p);
                        }
                    }
                    if (rs->parse() == 0) {
                        // Construct the operator node that wraps the regex; the
                        // runtime helper dynamic_cast's inst->pattern_expr to this
                        // type, then calls getRegexSubst().
                        auto* op_node = new QoreRegexSubstOperatorNode(&loc_builtin,
                            QoreValue(), rs);
                        const_cast<QoreValue&>(pi->pattern_expr) = op_node;
                        ctx->owned_regex_subst_nodes.push_back(op_node);
                    } else {
                        rs->deref();
                    }
                } else if (pi->binary_mut_op == LVBinaryMutOp::Transliterate) {
                    auto* tr = new QoreTransliteration(&loc_builtin);
                    if (pattern_str) {
                        for (const char* p = pattern_str; *p; ++p) {
                            tr->concatSource(*p);
                        }
                    }
                    tr->finishSource();
                    if (newstr_str) {
                        for (const char* p = newstr_str; *p; ++p) {
                            tr->concatTarget(*p);
                        }
                    }
                    tr->finishTarget();
                    auto* op_node = new QoreTransliterationOperatorNode(&loc_builtin,
                        QoreValue(), tr);
                    const_cast<QoreValue&>(pi->pattern_expr) = op_node;
                    ctx->owned_transliteration_nodes.push_back(op_node);
                }
            }
            uint8_t num_steps = QoreAOTBinaryReader::readU8(ptr);
            for (uint8_t s = 0; s < num_steps; ++s) {
                LVPathStep step;
                step.kind = static_cast<LVPathStepKind>(QoreAOTBinaryReader::readU8(ptr));
                step.slot_id = QoreAOTBinaryReader::readU32(ptr);
                const char* sname = reader.readStringRef(ptr);
                step.name = sname ? sname : "";
                step.operand_idx = QoreAOTBinaryReader::readU32(ptr);
                // Slice steps carry an SSA id vector (HashKeySlice /
                // ListIndexSlice / ListRangeSlice) — matches writer's wire format.
                if (step.kind == LVPathStepKind::HashKeySlice
                        || step.kind == LVPathStepKind::ListIndexSlice
                        || step.kind == LVPathStepKind::ListRangeSlice) {
                    uint32_t num_slice_ops = QoreAOTBinaryReader::readU32(ptr);
                    step.slice_operand_ids.reserve(num_slice_ops);
                    for (uint32_t k = 0; k < num_slice_ops; ++k) {
                        step.slice_operand_ids.push_back(
                                QoreAOTBinaryReader::readU32(ptr));
                    }
                }
                // Resolve ref_ptr from context locals/globals
                if ((step.kind == LVPathStepKind::LocalVar
                        || step.kind == LVPathStepKind::ClosureVar)
                        && step.slot_id != UINT32_MAX
                        && step.slot_id < static_cast<uint32_t>(num_locals)) {
                    step.ref_ptr = ctx->locals[step.slot_id];
                } else if ((step.kind == LVPathStepKind::GlobalVar
                        || step.kind == LVPathStepKind::ThreadLocalVar)
                        && !step.name.empty()) {
                    const qore_ns_private* vns = nullptr;
                    step.ref_ptr = qore_root_ns_private::runtimeFindGlobalVar(
                        *pp->RootNS, step.name.c_str(), vns);
                } else if (step.kind == LVPathStepKind::StaticVar
                        && !step.name.empty()) {
                    // Keep the symbolic path. Standalone-compiled fragments can
                    // reference static vars or scoped globals provided by sibling
                    // .qo files that are not registered yet when this context is
                    // built. LValueHelper::navigatePath() resolves the path when
                    // the assignment actually runs.
                    step.ref_ptr = nullptr;
                    // Also resolve the static in the program that owns this code, if it can be resolved
                    // there now.  navigatePath() prefers the symbolic path, so a public module class
                    // merged into an importing program still resolves to that program's static; this is
                    // the only way to reach a class that is private to its module, which never appears in
                    // the namespace of the program that loaded the module.
                    if (QoreProgram* owner_pgm = local_owner_pgm ? local_owner_pgm : pgm) {
                        std::string sv_name;
                        step.aot_static_var_info = qore_find_static_var_by_path(*owner_pgm, step.name,
                            sv_name);
                    }
                }
                pi->path.push_back(std::move(step));
            }
            ctx->lv_path_insts[i] = pi.get();
            ctx->owned_lv_path_insts.push_back(std::move(pi));
        }
    }

    // Read handler IR for statement slots
    // For each stmt slot, read the handler IR flag and optionally deserialize the IR function
    bool all_stmt_slots_have_ir = true;
    if (num_stmts > 0) {
        ctx->handler_irs.resize(num_stmts);
        // Build local name map for handler IR deserialization.
        //
        // Include BOTH ctx->locals (parent function's slot vars) AND
        // ctx->all_body_locals (body locals the parent function pre-instantiates
        // via evalTiered — which also includes on_exit handler-body locals,
        // collected transitively via collectAllStatementLocals).
        //
        // When the handler's own local shadows a parent name (e.g. outer block
        // has `Bar al` and on_exit has `Foo al`), we must reuse the handler's
        // OWN LocalVar* from all_body_locals (the one parent's evalTiered
        // pre-instantiated on TLS with the handler's type), not the parent
        // slot's same-named var.  Resolution is by (name, type_path): see the
        // lookup in deserializeIRFunction that prefers the type-qualified key
        // and falls back to name-only.
        std::unordered_map<std::string, LocalVar*> handler_local_map;
        for (auto* lv : ctx->all_body_locals) {
            if (lv && lv->getName() && *lv->getName()) {
                handler_local_map[lv->getName()] = lv;
                // Also register under a (name|type_path) composite key so
                // handler slot deser can pick the right var among same-named
                // shadowing vars (parent has Bar al, handler has Foo al).
                std::string tpath = getAOTTypePathForLValue(lv->getTypeInfoForLValue());
                if (!tpath.empty()) {
                    std::string ck(lv->getName());
                    ck += '\x1f';
                    ck += tpath;
                    handler_local_map[ck] = lv;
                }
            }
        }
        for (int i = 0; i < num_locals; ++i) {
            if (ctx->locals[i] && ctx->locals[i]->getName()) {
                LocalVar* lv = ctx->locals[i];
                // ctx->locals wins on bare-name collision so generic name-based
                // lookups resolve to the parent slot by default.  The composite
                // (name|type) key above lets the shadowing handler local win
                // when the deserializer asks for it specifically.
                handler_local_map[lv->getName()] = lv;
                std::string tpath = getAOTTypePathForLValue(lv->getTypeInfoForLValue());
                if (!tpath.empty()) {
                    std::string ck(lv->getName());
                    ck += '\x1f';
                    ck += tpath;
                    // Only insert if not already set by all_body_locals (which
                    // covers handler-shadow vars). ctx->locals may duplicate
                    // all_body_locals for same (name, type), which is fine.
                    handler_local_map.emplace(std::move(ck), lv);
                }
            }
        }
        for (int i = 0; i < num_stmts; ++i) {
            uint8_t has_handler_ir = QoreAOTBinaryReader::readU8(ptr);
            if (has_handler_ir) {
                // Read the size prefix so we can skip on failure
                uint32_t handler_ir_size = QoreAOTBinaryReader::readU32(ptr);
                const uint8_t* handler_ir_end = ptr + handler_ir_size;
                // Deserialize handler IR function
                std::string ir_error;
                auto readExprCb = [pgm, ctx, local_owner_pgm](
                        const QoreAOTBinaryReader& rdr, const uint8_t*& p,
                        const uint8_t* e, std::string& err) -> QoreValue {
                    // Peek at kind byte for special cases
                    uint8_t kind_byte = QoreAOTBinaryReader::readU8(p);
                    auto kind = static_cast<AOTExprKind>(kind_byte);
                    if (kind == AOTExprKind::GENERIC_EVAL) {
                        return QoreValue();
                    }
                    // EXPR_TREE: recursive expression tree (handler-specific)
                    if (kind == AOTExprKind::EXPR_TREE) {
                        uint32_t blob_size = QoreAOTBinaryReader::readU32(p);
                        const uint8_t* blob_data = p;
                        p += blob_size;
                        ExprTreeDeserializer deser(blob_data, blob_size, pgm, ctx);
                        uint64_t bits = deser.deserialize();
                        if (bits) {
                            QoreValue v;
                            memcpy(&v, &bits, sizeof(v));
                            return v;
                        }
                        return QoreValue();
                    }
                    // Delegate to readOneExpr for all other kinds (including NEW_OBJECT with args)
                    // We already consumed the kind byte, so "put it back" by passing it directly
                    // via a temporary local buffer trick... actually readOneExpr reads the kind byte
                    // itself. Use a wrapper that re-reads from p-1 by adjusting p.
                    --p;  // unconsume the kind byte so readOneExpr can read it
                    return readOneExpr(rdr, p, e, err, pgm,
                        ctx->locals, ctx->num_locals, ctx->globals, ctx->num_globals, local_owner_pgm);
                };
                auto handler = deserializeIRFunction(reader, ptr, end, pgm, readExprCb,
                    &handler_local_map, ir_error,
                    ctx->locals, num_locals, nullptr, false, nullptr, local_owner_pgm);
                if (handler) {
                    // Compute slot IDs for the deserialized handler
                    handler->computeSlotIdsAndEmbed();
                    ctx->handler_irs[i] = std::move(handler);
                    printd(3, "AOT buildCtx: '%s' stmt[%d] handler IR deserialized OK\n", name, i);
                } else {
                    printd(2, "AOT buildCtx: '%s' stmt[%d] handler IR deser failed: %s\n",
                        name, i, ir_error.c_str());
                    std::string msg = "statement slot " + std::to_string(i)
                        + " handler IR deserialization failed";
                    if (!ir_error.empty()) {
                        msg += ": ";
                        msg += ir_error;
                    }
                    setBuildError(msg);
                    delete ctx;
                    ptr = handler_ir_end;
                    return nullptr;
                }
                // Ensure ptr is at the end of the handler IR data
                ptr = handler_ir_end;
            } else {
                all_stmt_slots_have_ir = false;
            }
        }
    }

    // Resolve stmt_slots from the function's AST (on_block_exit handlers + reference foreach)
    // Only needed for slots that don't have handler IR
    if (num_stmts > 0 && !all_stmt_slots_have_ir) {
        if (uvb) {
            StatementBlock* sb = uvb->getStatementBlock();
            if (sb) {
                std::vector<const AbstractStatement*> stmt_list;
                collectStmtSlotStatements(sb, stmt_list);
                // Deduplicate in order (matching buildAOTSlotMap's getStmtSlot() semantics)
                std::vector<const AbstractStatement*> unique_stmts;
                std::unordered_set<const void*> seen;
                for (const AbstractStatement* s : stmt_list) {
                    if (seen.insert(reinterpret_cast<const void*>(s)).second) {
                        unique_stmts.push_back(s);
                    }
                }
                if (static_cast<int>(unique_stmts.size()) == num_stmts) {
                    for (int i = 0; i < num_stmts; ++i) {
                        if (!ctx->handler_irs[i]) {
                            ctx->stmts[i] = unique_stmts[i];
                        }
                    }
                } else {
                    printd(2, "AOT buildCtx: '%s' stmt count mismatch: expected %d, got %d\n",
                        name, num_stmts, (int)unique_stmts.size());
                    has_unsupported = true;
                }
            } else {
                // No statement block (strip-source) — if all stmt slots have handler IR, that's OK
                if (!all_stmt_slots_have_ir) {
                    printd(2, "AOT buildCtx: '%s' has %d stmt_slots but no statement block and missing handler IR\n",
                        name, num_stmts);
                    has_unsupported = true;
                }
            }
        } else {
            // Toplevel with on_block_exit/foreach — not supported in slot map path
            // unless all stmt slots have handler IR
            if (!all_stmt_slots_have_ir) {
                has_unsupported = true;
            }
        }
    }

    // Read location table (AOT runtime_loc tracking)
    // Only read if entry_end is known and there's data remaining in the entry
    const uint8_t* loc_boundary = entry_end ? entry_end : end;
    const bool wide_loc_tables = (reader.getHeader().feature_flags & QORE_AOT_FEAT_WIDE_LOC_TABLES) != 0;
    const size_t loc_count_size = wide_loc_tables ? sizeof(uint32_t) : sizeof(uint16_t);
    auto readLocTableCount = [wide_loc_tables](const uint8_t*& p) -> uint32_t {
        return wide_loc_tables
            ? QoreAOTBinaryReader::readU32(p)
            : static_cast<uint32_t>(QoreAOTBinaryReader::readU16(p));
    };
    if (trace_slot_reg) {
        fprintf(stderr, "[aot-slot-reg] '%s': before loc table off=%zu entry_end=%zu\n",
            name, static_cast<size_t>(ptr - entry_payload_start),
            static_cast<size_t>(loc_boundary - entry_payload_start));
    }
    if (ptr + loc_count_size <= loc_boundary) {
        uint32_t num_loc_entries = readLocTableCount(ptr);
        if (trace_slot_reg) {
            fprintf(stderr, "[aot-slot-reg] '%s': loc entries=%u off=%zu\n",
                name, num_loc_entries, static_cast<size_t>(ptr - entry_payload_start));
        }
        if (num_loc_entries > 0) {
            if (num_loc_entries > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
                setBuildError("location table entry count exceeds runtime int capacity: "
                    + std::to_string(num_loc_entries));
                delete ctx;
                return nullptr;
            }
            ctx->num_locs = static_cast<int>(num_loc_entries);
            ctx->locs = static_cast<const QoreProgramLocation**>(
                calloc(num_loc_entries, sizeof(const QoreProgramLocation*)));
            for (uint32_t i = 0; i < num_loc_entries && ptr < loc_boundary; ++i) {
                int start_line = qore_aot_valid_line(qore_aot_read_line(reader, ptr));
                int end_line = qore_aot_valid_line(qore_aot_read_line(reader, ptr));
                const char* loc_file = reader.readStringRef(ptr);
                if (start_line > 0) {
                    // Intern the file name in the program's string pool —
                    // the reader's decompressed pool dies when the parent
                    // QoreAOTBinaryDeserializer goes out of scope, but
                    // QoreProgramLocation::file is a raw const char* and
                    // the loc object itself outlives the deserializer via
                    // the AOT context.  See the matching deser site in
                    // deserializeIRInstruction for the full rationale.
                    const char* interned_file = (loc_file && pgm)
                        ? qore_program_private::get(*pgm)->addString(loc_file)
                        : (loc_file ? loc_file : "");
                    ctx->locs[i] = new QoreProgramLocation(
                        interned_file, start_line, end_line);
                }
            }
        }
    }

    if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_STMT_LOC_TABLE) != 0
            && ptr + loc_count_size <= loc_boundary) {
        uint32_t num_stmt_locs = readLocTableCount(ptr);
        if (trace_slot_reg) {
            fprintf(stderr, "[aot-slot-reg] '%s': stmt loc entries=%u off=%zu\n",
                name, num_stmt_locs, static_cast<size_t>(ptr - entry_payload_start));
        }
        // Statement-location records exist only for ProgramControl/debugger
        // lookup.  Every public lookup checks PO_ALLOW_DEBUGGER first, so
        // materializing statement objects for programs that cannot be debugged
        // creates unreachable state and substantial startup allocation cost.
        // Keep an eager diagnostic mode for same-build performance and
        // compatibility comparisons.
        static const bool eager_stmt_locs =
            getenv("QORE_DISABLE_AOT_LAZY_STMT_LOCS") != nullptr;
        bool materialize_stmt_locs = pgm
            && (eager_stmt_locs || (pgm->getParseOptions() & PO_ALLOW_DEBUGGER));
        if (num_stmt_locs > 0 && materialize_stmt_locs) {
            qore_program_private* pp = qore_program_private::get(*pgm);
            AutoLocker al(&pp->plock);
            for (uint32_t i = 0; i < num_stmt_locs && ptr < loc_boundary; ++i) {
                int start_line = qore_aot_valid_line(qore_aot_read_line(reader, ptr));
                int end_line = qore_aot_valid_line(qore_aot_read_line(reader, ptr));
                int64_t offset = QoreAOTBinaryReader::readI64(ptr);
                const char* loc_file = reader.readStringRef(ptr);
                const char* loc_source = reader.readStringRef(ptr);
                // a non-positive start line carries no usable statement location
                if (start_line <= 0 || !loc_file) {
                    continue;
                }

                const char* interned_file = pp->addString(loc_file);
                const char* interned_source = loc_source && *loc_source
                    ? pp->addString(loc_source) : nullptr;
                if (pp->findIndexedStatementForLocation(interned_file, interned_source,
                        start_line, static_cast<int>(offset))) {
                    continue;
                }
                QoreProgramLocation tmp(interned_file, start_line, end_line,
                    interned_source, static_cast<int>(offset));
                const QoreProgramLocation* loc = pp->getLocation(tmp, start_line, end_line);
                StatementBlock* stmt = new StatementBlock(pp, loc);
                qore_program_private::registerStatement(pgm, stmt, true);
                ctx->owned_debug_statements.push_back(stmt);
            }
        } else {
            for (uint32_t i = 0; i < num_stmt_locs && ptr < loc_boundary; ++i) {
                (void)qore_aot_read_line(reader, ptr);
                (void)qore_aot_read_line(reader, ptr);
                (void)QoreAOTBinaryReader::readI64(ptr);
                (void)reader.readStringRef(ptr);
                (void)reader.readStringRef(ptr);
            }
        }
    }

    if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_DEBUG_IR) != 0) {
        if (reader.getHeader().version >= QORE_AOT_SPLIT_DEBUG_IR_VERSION) {
            if (ptr + 8 > loc_boundary) {
                setBuildError("missing split debug IR range");
                delete ctx;
                ptr = loc_boundary;
                return nullptr;
            }
            uint32_t debug_ir_offset = QoreAOTBinaryReader::readU32(ptr);
            uint32_t debug_ir_size = QoreAOTBinaryReader::readU32(ptr);
            if (debug_ir_size) {
                const QoreAOTSectionHeader* debug_sec =
                    reader.findSection(QoreAOTSectionType::DEBUG_IR);
                if (!debug_sec || debug_ir_offset < sizeof(uint32_t)
                        || debug_ir_offset > debug_sec->size
                        || debug_ir_size > debug_sec->size - debug_ir_offset) {
                    setBuildError("split debug IR range exceeds DEBUG_IR section");
                    delete ctx;
                    ptr = loc_boundary;
                    return nullptr;
                }
                if (debug_metadata) {
                    ctx->debug_metadata = std::move(debug_metadata);
                    ctx->debug_ir_offset = debug_ir_offset;
                    ctx->debug_ir_size = debug_ir_size;
                    ctx->debug_ir_separate_section = true;
                }
            }
            if (trace_slot_reg) {
                fprintf(stderr,
                    "[aot-slot-reg] '%s': split debug IR offset=%u size=%u\n",
                    name, debug_ir_offset, debug_ir_size);
            }
        } else if (ptr + 1 <= loc_boundary) {
            uint8_t has_debug_ir = QoreAOTBinaryReader::readU8(ptr);
            if (trace_slot_reg) {
                fprintf(stderr, "[aot-slot-reg] '%s': debug IR present=%u off=%zu\n",
                    name, has_debug_ir, static_cast<size_t>(ptr - entry_payload_start));
            }
            if (has_debug_ir && ptr + 4 <= loc_boundary) {
                uint32_t debug_ir_size = QoreAOTBinaryReader::readU32(ptr);
                const uint8_t* debug_ir_end = ptr + debug_ir_size;
                if (trace_slot_reg) {
                    fprintf(stderr, "[aot-slot-reg] '%s': debug IR size=%u payload_off=%zu end_off=%zu\n",
                        name, debug_ir_size, static_cast<size_t>(ptr - entry_payload_start),
                        static_cast<size_t>(debug_ir_end - entry_payload_start));
                }
                if (debug_ir_end <= loc_boundary) {
                    if (debug_metadata && slot_maps_start && ptr >= slot_maps_start) {
                        size_t debug_ir_offset = static_cast<size_t>(ptr - slot_maps_start);
                        if (debug_ir_offset <= UINT32_MAX) {
                            ctx->debug_metadata = std::move(debug_metadata);
                            ctx->debug_ir_offset = static_cast<uint32_t>(debug_ir_offset);
                            ctx->debug_ir_size = debug_ir_size;
                            if (trace_slot_reg) {
                                fprintf(stderr,
                                    "[aot-slot-reg] '%s': stored lazy debug IR offset=%u size=%u\n",
                                    name, ctx->debug_ir_offset, ctx->debug_ir_size);
                            }
                        } else {
                            printd(2, "AOT buildCtx: '%s' debug IR offset too large: %zu\n",
                                name, debug_ir_offset);
                            if (trace_slot_reg) {
                                fprintf(stderr,
                                    "[aot-slot-reg] '%s': debug IR offset too large: %zu\n",
                                    name, debug_ir_offset);
                            }
                        }
                    }
                    if (!ctx->debug_ir_size && trace_slot_reg) {
                        fprintf(stderr, "[aot-slot-reg] '%s': debug IR present but lazy metadata unavailable\n",
                            name);
                    }
                    ptr = debug_ir_end;
                } else {
                    printd(2, "AOT buildCtx: '%s' malformed debug IR size %u\n",
                        name, debug_ir_size);
                    std::string msg = "malformed debug IR size " + std::to_string(debug_ir_size)
                        + " exceeds slot map entry boundary";
                    setBuildError(msg);
                    delete ctx;
                    ptr = loc_boundary;
                    return nullptr;
                }
            }
        }
    }

    printd(2, "AOT v2: built context from slot map for '%s' "
        "(locals=%d, globals=%d, exprs=%d, stmts=%d, regex_cases=%d, body_locals=%d, unsupported=%d, closure_ir_missing=%d)\n",
        name, num_locals, num_globals, num_exprs, num_stmts, num_regex_cases, num_body_locals, has_unsupported,
        closure_ir_missing);

    // If any expression slots have unsupported types, skip AOT
    // registration for this function — it will fall through to JIT at runtime
    if (has_unsupported) {
        printd(2, "AOT buildCtx: SKIP '%s' (unsupported expr slots)\n", name);
        if (trace_slot_reg) {
            fprintf(stderr, "[aot-slot-reg] SKIP '%s': unsupported expr slots\n", name);
        }
        // name the symbol that failed to resolve when one was recorded; without it the caller sees only
        // "unsupported AOT slot metadata" and has no way to tell a stale module from a real metadata problem
        std::string msg = "unsupported AOT slot metadata";
        if (!aot_slot_resolve_error.empty()) {
            msg += ": ";
            msg += aot_slot_resolve_error;
            aot_slot_resolve_error.clear();
        }
        msg += "; source fallback is disabled";
        setBuildError(msg);
        delete ctx;
        return nullptr;
    }

    // Closure IR errors are hard failures (Phase 2: no source fallback)
    if (closure_ir_missing) {
        printd(2, "AOT buildCtx: '%s' failed (%s)\n", name,
            closure_ir_error.empty() ? "closure IR missing" : closure_ir_error.c_str());
        if (trace_slot_reg) {
            fprintf(stderr, "[aot-slot-reg] SKIP '%s': %s\n", name,
                closure_ir_error.empty() ? "closure IR missing" : closure_ir_error.c_str());
        }
        setBuildError((closure_ir_error.empty() ? std::string("closure IR missing") : closure_ir_error)
            + "; source fallback is disabled");
        delete ctx;
        return nullptr;
    }

    // Register this function's lazy PC->loc map (no-op if the artifact has no trailer).
    aotAttachPcLocMap(aot_func.fn_ptr, ctx);

    return ctx;
}

// ---- IR Function Deserialization (Phase 5) ----

//! Deserialize a single IR instruction from binary data
/** @return the deserialized instruction, or nullptr on error */
static std::unique_ptr<QoreIRInstruction> deserializeIRInstruction(
        const QoreAOTBinaryReader& reader,
        const uint8_t*& ptr, const uint8_t* end,
        const std::vector<std::unique_ptr<QoreIRBasicBlock>>& blocks,
        const std::unordered_map<std::string, LocalVar*>& local_map,
        QoreIRFunction* owner_func,
        const std::unordered_map<uint32_t, LocalVar*>* slot_to_local,
        const AOTExprReadFunc& readExpr,
        QoreProgram* pgm,
        QoreProgram* local_owner_pgm,
        std::string& error) {
    auto need = [&ptr, end, &error](size_t bytes, const char* field) -> bool {
        if (end < ptr || static_cast<size_t>(end - ptr) < bytes) {
            error = "truncated instruction while reading ";
            error += field ? field : "<unknown field>";
            error += ": need ";
            error += std::to_string(bytes);
            error += " byte(s), have ";
            error += end >= ptr ? std::to_string(static_cast<size_t>(end - ptr)) : "0";
            return false;
        }
        return true;
    };

    // Read opcode and group tag
    if (!need(2, "opcode")) {
        return nullptr;
    }
    uint16_t opcode_raw = QoreAOTBinaryReader::readU16(ptr);
    auto opcode = static_cast<QoreIROpcode>(opcode_raw);
    if (!need(1, "instruction group")) {
        return nullptr;
    }
    uint8_t group_byte = QoreAOTBinaryReader::readU8(ptr);
    auto group = static_cast<QoreIRInstGroup>(group_byte);

    // Read base fields: result, operands, exception_target
    if (!need(4, "result id")) {
        return nullptr;
    }
    uint32_t result_id = QoreAOTBinaryReader::readU32(ptr);
    uint32_t num_operands;
    if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_WIDE_IR_OPERANDS) != 0) {
        if (!need(2, "operand count")) {
            return nullptr;
        }
        num_operands = QoreAOTBinaryReader::readU16(ptr);
    } else {
        if (!need(1, "operand count")) {
            return nullptr;
        }
        num_operands = QoreAOTBinaryReader::readU8(ptr);
    }
    if (num_operands > 0 && !need(static_cast<size_t>(num_operands) * 4, "operands")) {
        return nullptr;
    }
    std::vector<QoreIRValue> operands;
    operands.reserve(num_operands);
    for (uint32_t j = 0; j < num_operands; ++j) {
        uint32_t op_id = QoreAOTBinaryReader::readU32(ptr);
        operands.push_back(QoreIRValue(op_id));
    }
    if (!need(2, "exception target")) {
        return nullptr;
    }
    uint16_t exc_target_idx = QoreAOTBinaryReader::readU16(ptr);
    QoreIRBasicBlock* exc_target = (exc_target_idx != 0xFFFF && exc_target_idx < blocks.size())
        ? blocks[exc_target_idx].get() : nullptr;

    std::unique_ptr<QoreIRInstruction> inst;

    // Dispatch via registry
    const auto* ginfo = getAOTInstGroupInfo(group_byte);
    if (!ginfo || !ginfo->is_serializable || !ginfo->read_fn) {
        const OpcodeInfo* oi = getOpcodeInfo(opcode_raw);
        error = "unsupported instruction group " + std::to_string(group_byte)
            + " while deserializing opcode "
            + (oi && oi->name ? oi->name : "<unknown>")
            + " (" + std::to_string(opcode_raw) + ")";
        if (!ginfo) {
            error += "; group is not registered";
        } else if (!ginfo->is_serializable) {
            error += "; group '";
            error += ginfo->name ? ginfo->name : "<unnamed>";
            error += "' is not AOT-serializable";
        } else {
            error += "; group has no reader";
        }
        error += "; this usually means qcc emitted malformed or schema-incompatible IR";
        return nullptr;
    }
    AOTInstReadCtx rctx{
        reader, ptr, end, blocks, local_map, readExpr, pgm, local_owner_pgm, error, slot_to_local
    };
    inst = ginfo->read_fn(opcode_raw, exc_target, operands, result_id, rctx);
    if (!inst) {
        const OpcodeInfo* oi = getOpcodeInfo(opcode_raw);
        error = std::string("opcode ") + (oi && oi->name ? oi->name : "<unknown>")
            + " (" + std::to_string(opcode_raw) + "), group "
            + (ginfo->name ? ginfo->name : "<unknown>") + " ("
            + std::to_string(group_byte) + "): " + error;
        return nullptr;
    }

    // This #if 0 block contains the original switch statement that was replaced by registry dispatch.
    // It is kept for reference only.
    #if 0
    switch (group) {
        case QoreIRInstGroup::Base: {
            inst = std::make_unique<QoreIRInstruction>(opcode);
            break;
        }

        case QoreIRInstGroup::Const: {
            auto* ci = new QoreIRConstInstruction();
            ci->opcode = opcode;
            uint8_t kind_byte = QoreAOTBinaryReader::readU8(ptr);
            ci->constant.kind = static_cast<QoreIRConstant::Kind>(kind_byte);
            switch (ci->constant.kind) {
                case QoreIRConstant::Kind::Int:
                    ci->constant.int_value = QoreAOTBinaryReader::readI64(ptr);
                    break;
                case QoreIRConstant::Kind::Float:
                    ci->constant.float_value = QoreAOTBinaryReader::readF64(ptr);
                    break;
                case QoreIRConstant::Kind::Bool:
                    ci->constant.bool_value = QoreAOTBinaryReader::readU8(ptr) != 0;
                    break;
                case QoreIRConstant::Kind::Nothing:
                case QoreIRConstant::Kind::Null:
                    break;
                case QoreIRConstant::Kind::String:
                    ci->constant.string_value = reader.readStringRef(ptr);
                    break;
                case QoreIRConstant::Kind::Date:
                    ci->constant.date_microseconds = QoreAOTBinaryReader::readI64(ptr);
                    ci->constant.date_is_relative = QoreAOTBinaryReader::readU8(ptr) != 0;
                    if (ci->constant.date_is_relative) {
                        ci->constant.rel_years = static_cast<int>(QoreAOTBinaryReader::readU32(ptr));
                        ci->constant.rel_months = static_cast<int>(QoreAOTBinaryReader::readU32(ptr));
                        ci->constant.rel_days = static_cast<int>(QoreAOTBinaryReader::readU32(ptr));
                        ci->constant.rel_hours = static_cast<int>(QoreAOTBinaryReader::readU32(ptr));
                        ci->constant.rel_minutes = static_cast<int>(QoreAOTBinaryReader::readU32(ptr));
                        ci->constant.rel_seconds = static_cast<int>(QoreAOTBinaryReader::readU32(ptr));
                        ci->constant.rel_us = static_cast<int>(QoreAOTBinaryReader::readU32(ptr));
                    } else {
                        const char* zone_name = reader.readStringRef(ptr);
                        if (zone_name && *zone_name) {
                            ci->constant.date_zone = runtimeReadAOTDateZone(zone_name);
                            ci->constant.date_zone_set = true;
                        }
                    }
                    break;
                case QoreIRConstant::Kind::Enum: {
                    const char* enum_path = reader.readStringRef(ptr);
                    const char* member_name = reader.readStringRef(ptr);
                    if (enum_path && *enum_path && member_name && *member_name) {
                        const QoreNamespace* pns = nullptr;
                        const QoreEnumDecl* ed = pgm->findEnum(enum_path, pns);
                        if (ed) {
                            ci->constant.enum_member = ed->findMember(member_name);
                            if (ci->constant.enum_member) {
                                ci->constant.int_value = ci->constant.enum_member->getValue().getAsBigInt();
                            }
                        }
                    }
                    break;
                }
            }
            inst.reset(ci);
            break;
        }

        case QoreIRInstGroup::Branch: {
            auto* bi = new QoreIRBranchInstruction();
            bi->opcode = opcode;
            uint16_t target_idx = QoreAOTBinaryReader::readU16(ptr);
            bi->target = resolveBlock(target_idx);
            inst.reset(bi);
            break;
        }

        case QoreIRInstGroup::BranchIf: {
            auto* bi = new QoreIRBranchIfInstruction();
            bi->opcode = opcode;
            bi->condition = QoreIRValue(QoreAOTBinaryReader::readU32(ptr));
            uint16_t true_idx = QoreAOTBinaryReader::readU16(ptr);
            uint16_t false_idx = QoreAOTBinaryReader::readU16(ptr);
            bi->true_target = resolveBlock(true_idx);
            bi->false_target = resolveBlock(false_idx);
            inst.reset(bi);
            break;
        }

        case QoreIRInstGroup::Return: {
            auto* ri = new QoreIRReturnInstruction();
            ri->opcode = opcode;
            ri->has_value = QoreAOTBinaryReader::readU8(ptr) != 0;
            if (ri->has_value) {
                ri->value = QoreIRValue(QoreAOTBinaryReader::readU32(ptr));
            }
            inst.reset(ri);
            break;
        }

        case QoreIRInstGroup::Throw: {
            auto* ti = new QoreIRThrowInstruction(opcode);
            uint16_t throw_exc_idx = QoreAOTBinaryReader::readU16(ptr);
            ti->exception_target = resolveBlock(throw_exc_idx);
            ti->catch_depth = static_cast<int>(QoreAOTBinaryReader::readU16(ptr));
            ti->synthetic = QoreAOTBinaryReader::readU8(ptr) != 0;
            inst.reset(ti);
            break;
        }

        case QoreIRInstGroup::Local: {
            const char* lname = reader.readStringRef(ptr);
            const char* ltype = reader.readStringRef(ptr);
            uint32_t slot_id = QoreAOTBinaryReader::readU32(ptr);
            bool auto_ref = QoreAOTBinaryReader::readU8(ptr) != 0;
            bool weak = QoreAOTBinaryReader::readU8(ptr) != 0;
            bool is_closure = QoreAOTBinaryReader::readU8(ptr) != 0;
            bool is_ref = QoreAOTBinaryReader::readU8(ptr) != 0;
            bool read_only = false;
            bool initial_assignment = false;
            if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_READONLY_LOCALS) != 0) {
                read_only = QoreAOTBinaryReader::readU8(ptr) != 0;
                initial_assignment = QoreAOTBinaryReader::readU8(ptr) != 0;
            }
            LocalVar* lv = resolveLocal(lname);
            if (!lv && lname && *lname) {
                // Create a new local variable for handler/closure locals
                std::string type_error;
                QoreAOTTypeResolver type_resolver(pgm);
                const QoreTypeInfo* ti = (ltype && *ltype)
                    ? type_resolver.resolve(ltype, type_error) : nullptr;
                qore_program_private* pp = qore_program_private::get(
                    *(local_owner_pgm ? local_owner_pgm : pgm));
                lv = pp->createLocalVar(lname, ti);
            }
            if (lv && read_only && !lv->isReadOnly()) {
                lv->setReadOnly();
            }
            auto* li = new QoreIRLocalInstruction(opcode, lv, auto_ref);
            li->weak = weak;
            li->initial_assignment = initial_assignment;
            li->is_closure = is_closure;
            li->is_ref = is_ref;
            li->slot_id = slot_id;
            inst.reset(li);
            break;
        }

        case QoreIRInstGroup::Var: {
            const char* vname = reader.readStringRef(ptr);
            bool weak = QoreAOTBinaryReader::readU8(ptr) != 0;
            Var* var = nullptr;
            if (vname && *vname) {
                qore_program_private* pp = qore_program_private::get(*pgm);
                const qore_ns_private* vns = nullptr;
                var = qore_root_ns_private::runtimeFindGlobalVar(*pp->RootNS, vname, vns);
            }
            auto* vi = new QoreIRVarInstruction(opcode, var);
            vi->weak = weak;
            inst.reset(vi);
            break;
        }

        case QoreIRInstGroup::LValue: {
            QoreValue lvalue = readExpr(reader, ptr, end, error);
            if (!error.empty()) {
                return nullptr;
            }
            bool weak = QoreAOTBinaryReader::readU8(ptr) != 0;
            uint32_t lvalue_slot_id = QoreAOTBinaryReader::readU32(ptr);
            auto* lvi = new QoreIRLValueInstruction(opcode, lvalue, weak);
            lvi->lvalue_slot_id = lvalue_slot_id;
            // LValue constructor refs the value, so deref our copy
            lvalue.discard(nullptr);
            inst.reset(lvi);
            break;
        }

        case QoreIRInstGroup::Expr: {
            QoreValue expr = readExpr(reader, ptr, end, error);
            if (!error.empty()) {
                return nullptr;
            }
            bool has_ref_args = QoreAOTBinaryReader::readU8(ptr) != 0;
            auto* ei = new QoreIRExprInstruction(opcode, expr);
            ei->has_ref_args = has_ref_args;
            // Expr constructor refs the value, so deref our copy
            expr.discard(nullptr);
            inst.reset(ei);
            break;
        }

        case QoreIRInstGroup::CallDirect: {
            QoreValue expr = readExpr(reader, ptr, end, error);
            if (!error.empty()) {
                return nullptr;
            }
            bool has_ref_args = QoreAOTBinaryReader::readU8(ptr) != 0;
            bool is_self_recursive = QoreAOTBinaryReader::readU8(ptr) != 0;
            // Resolve function from the expr (FunctionCallNode stores the func pointer)
            const QoreFunction* func = nullptr;
            QoreProgram* func_pgm = pgm;
            // The expr contains the FunctionCallNode we need for AOT
            auto* ci = new QoreIRCallDirectInstruction(func, nullptr, func_pgm, expr);
            ci->has_ref_args = has_ref_args;
            ci->is_self_recursive = is_self_recursive;
            expr.discard(nullptr);
            inst.reset(ci);
            break;
        }

        case QoreIRInstGroup::CallMethodDirect: {
            bool has_expr = QoreAOTBinaryReader::readU8(ptr) != 0;
            QoreValue expr;
            if (has_expr) {
                expr = readExpr(reader, ptr, end, error);
                if (!error.empty()) {
                    return nullptr;
                }
            }
            const char* class_path = reader.readStringRef(ptr);
            const char* method_name = reader.readStringRef(ptr);
            bool has_ref_args = QoreAOTBinaryReader::readU8(ptr) != 0;

            const QoreMethod* method = nullptr;
            const QoreClass* qc = nullptr;
            if (class_path && *class_path) {
                qc = findAOTClassByPath(pgm, class_path, true);
                if (qc && method_name && *method_name) {
                    method = qc->findMethod(method_name);
                    if (!method) {
                        method = findAOTStaticMethod(qc, method_name);
                    }
                }
            }
            auto* ci = new QoreIRCallMethodDirectInstruction(method, qc, nullptr, expr);
            ci->has_ref_args = has_ref_args;
            if (has_expr) {
                expr.discard(nullptr);
            }
            inst.reset(ci);
            break;
        }

        case QoreIRInstGroup::InvokeMethodDirect: {
            bool has_expr = QoreAOTBinaryReader::readU8(ptr) != 0;
            QoreValue expr;
            if (has_expr) {
                expr = readExpr(reader, ptr, end, error);
                if (!error.empty()) {
                    return nullptr;
                }
            }
            const char* class_path = reader.readStringRef(ptr);
            const char* method_name = reader.readStringRef(ptr);
            bool has_ref_args = QoreAOTBinaryReader::readU8(ptr) != 0;
            uint16_t normal_idx = QoreAOTBinaryReader::readU16(ptr);
            uint16_t exception_idx = QoreAOTBinaryReader::readU16(ptr);

            const QoreMethod* method = nullptr;
            const QoreClass* qc = nullptr;
            if (class_path && *class_path) {
                qc = findAOTClassByPath(pgm, class_path, true);
                if (qc && method_name && *method_name) {
                    method = qc->findMethod(method_name);
                    if (!method) {
                        method = findAOTStaticMethod(qc, method_name);
                    }
                }
            }
            auto* ci = new QoreIRInvokeMethodDirectInstruction(method, qc, nullptr,
                resolveBlock(normal_idx), resolveBlock(exception_idx), expr);
            ci->has_ref_args = has_ref_args;
            if (has_expr) {
                expr.discard(nullptr);
            }
            inst.reset(ci);
            break;
        }

        case QoreIRInstGroup::CallStaticDirect: {
            QoreValue expr = readExpr(reader, ptr, end, error);
            if (!error.empty()) {
                return nullptr;
            }
            const char* class_path = reader.readStringRef(ptr);
            const char* method_name = reader.readStringRef(ptr);
            bool has_ref_args = QoreAOTBinaryReader::readU8(ptr) != 0;

            const QoreMethod* method = nullptr;
            if (class_path && *class_path) {
                const QoreClass* qc = findAOTClassByPath(pgm, class_path, false);
                if (qc && method_name && *method_name) {
                    method = findAOTStaticMethod(qc, method_name);
                }
            }
            auto* ci = new QoreIRCallStaticDirectInstruction(method, nullptr, expr);
            ci->has_ref_args = has_ref_args;
            expr.discard(nullptr);
            inst.reset(ci);
            break;
        }

        case QoreIRInstGroup::DotEvalMethodDirect: {
            QoreValue expr = readExpr(reader, ptr, end, error);
            if (!error.empty()) {
                return nullptr;
            }
            const char* class_path = reader.readStringRef(ptr);
            AOTEncodedMethodRef method_ref(reader.readStringRef(ptr));
            bool pseudo = QoreAOTBinaryReader::readU8(ptr) != 0;
            bool has_ref_args = QoreAOTBinaryReader::readU8(ptr) != 0;
            uint8_t pseudo_flags =
                (reader.getHeader().feature_flags & QORE_AOT_FEAT_DOT_EVAL_PSEUDO_FLAGS) != 0
                    ? QoreAOTBinaryReader::readU8(ptr) : 0;

            const QoreClass* qc = findAOTClassByPath(pgm, class_path, pseudo);
            const QoreMethod* method = findAOTMethodByName(qc, method_ref.method_name);
            const AbstractQoreFunctionVariant* variant = findAOTMethodVariantByRef(
                pgm, method, method_ref, pseudo);
            auto* ci = new QoreIRDotEvalMethodDirectInstruction(method, qc, variant, expr, pseudo);
            ci->has_ref_args = has_ref_args;
            ci->intrinsic = pseudo
                ? qore_ir_resolve_pseudo_intrinsic(method, qc, method_ref.method_name)
                : QoreIRIntrinsic::None;
            ci->pseudo_base_known_string = (pseudo_flags & 0x01) != 0;
            ci->pseudo_base_known_assigned_string = (pseudo_flags & 0x02) != 0;
            ci->pseudo_base_safe_value_dispatch = (pseudo_flags & 0x04) != 0;
            ci->pseudo_base_known_assigned_collection = (pseudo_flags & 0x80) != 0;
            if (method_ref.method_name && *method_ref.method_name) {
                ci->fallback_method_name = strdup(method_ref.method_name);
            }
            expr.discard(nullptr);
            inst.reset(ci);
            break;
        }

        case QoreIRInstGroup::InvokeDotEvalMethodDirect: {
            QoreValue expr = readExpr(reader, ptr, end, error);
            if (!error.empty()) {
                return nullptr;
            }
            const char* class_path = reader.readStringRef(ptr);
            AOTEncodedMethodRef method_ref(reader.readStringRef(ptr));
            bool pseudo = QoreAOTBinaryReader::readU8(ptr) != 0;
            bool has_ref_args = QoreAOTBinaryReader::readU8(ptr) != 0;
            uint8_t pseudo_flags =
                (reader.getHeader().feature_flags & QORE_AOT_FEAT_DOT_EVAL_PSEUDO_FLAGS) != 0
                    ? QoreAOTBinaryReader::readU8(ptr) : 0;
            uint16_t normal_idx = QoreAOTBinaryReader::readU16(ptr);
            uint16_t exception_idx = QoreAOTBinaryReader::readU16(ptr);

            const QoreClass* qc = findAOTClassByPath(pgm, class_path, pseudo);
            const QoreMethod* method = findAOTMethodByName(qc, method_ref.method_name);
            const AbstractQoreFunctionVariant* variant = findAOTMethodVariantByRef(
                pgm, method, method_ref, pseudo);
            auto* ci = new QoreIRInvokeDotEvalMethodDirectInstruction(method, qc, variant, expr,
                pseudo, resolveBlock(normal_idx), resolveBlock(exception_idx));
            ci->has_ref_args = has_ref_args;
            ci->intrinsic = pseudo
                ? qore_ir_resolve_pseudo_intrinsic(method, qc, method_ref.method_name)
                : QoreIRIntrinsic::None;
            ci->pseudo_base_known_string = (pseudo_flags & 0x01) != 0;
            ci->pseudo_base_known_assigned_string = (pseudo_flags & 0x02) != 0;
            ci->pseudo_base_safe_value_dispatch = (pseudo_flags & 0x04) != 0;
            ci->pseudo_base_known_assigned_collection = (pseudo_flags & 0x80) != 0;
            if (method_ref.method_name && *method_ref.method_name) {
                ci->fallback_method_name = strdup(method_ref.method_name);
            }
            expr.discard(nullptr);
            inst.reset(ci);
            break;
        }

        case QoreIRInstGroup::Invoke: {
            QoreValue expr = readExpr(reader, ptr, end, error);
            if (!error.empty()) {
                return nullptr;
            }
            uint16_t invoke_opcode_raw = QoreAOTBinaryReader::readU16(ptr);
            const char* invoke_key_name = reader.readStringRef(ptr);
            bool weak = QoreAOTBinaryReader::readU8(ptr) != 0;
            uint16_t normal_idx = QoreAOTBinaryReader::readU16(ptr);
            uint16_t exception_idx = QoreAOTBinaryReader::readU16(ptr);
            auto* ii = new QoreIRInvokeInstruction(expr,
                resolveBlock(normal_idx), resolveBlock(exception_idx));
            ii->invoke_opcode = static_cast<QoreIROpcode>(invoke_opcode_raw);
            ii->invoke_key_name = invoke_key_name ? invoke_key_name : "";
            ii->weak = weak;
            expr.discard(nullptr);
            inst.reset(ii);
            break;
        }

        case QoreIRInstGroup::ScopeEnter: {
            uint32_t scope_id = QoreAOTBinaryReader::readU32(ptr);
            inst = std::make_unique<QoreIRScopeEnterInstruction>(scope_id);
            break;
        }

        case QoreIRInstGroup::ScopeExit: {
            uint32_t scope_id = QoreAOTBinaryReader::readU32(ptr);
            bool is_error = QoreAOTBinaryReader::readU8(ptr) != 0;
            inst = std::make_unique<QoreIRScopeExitInstruction>(scope_id, is_error);
            break;
        }

        case QoreIRInstGroup::LandingPad: {
            uint32_t scope_depth = QoreAOTBinaryReader::readU32(ptr);
            uint32_t try_scope_id = QoreAOTBinaryReader::readU32(ptr);
            inst = std::make_unique<QoreIRLandingPadInstruction>(
                static_cast<size_t>(scope_depth), try_scope_id);
            break;
        }

        case QoreIRInstGroup::SwitchInt: {
            auto* si = new QoreIRSwitchIntInstruction();
            si->opcode = opcode;
            si->switch_val = QoreIRValue(QoreAOTBinaryReader::readU32(ptr));
            uint16_t default_idx = QoreAOTBinaryReader::readU16(ptr);
            si->default_target = resolveBlock(default_idx);
            uint16_t num_cases = QoreAOTBinaryReader::readU16(ptr);
            si->cases.reserve(num_cases);
            for (int j = 0; j < num_cases; ++j) {
                int64_t value = QoreAOTBinaryReader::readI64(ptr);
                uint16_t target_idx = QoreAOTBinaryReader::readU16(ptr);
                si->cases.push_back({value, resolveBlock(target_idx)});
            }
            inst.reset(si);
            break;
        }

        case QoreIRInstGroup::SwitchString: {
            auto* si = new QoreIRSwitchStringInstruction();
            si->opcode = opcode;
            si->switch_val = QoreIRValue(QoreAOTBinaryReader::readU32(ptr));
            uint16_t default_idx = QoreAOTBinaryReader::readU16(ptr);
            si->default_target = resolveBlock(default_idx);
            uint16_t num_cases = QoreAOTBinaryReader::readU16(ptr);
            si->cases.reserve(num_cases);
            for (int j = 0; j < num_cases; ++j) {
                const char* value = reader.readStringRef(ptr);
                uint16_t target_idx = QoreAOTBinaryReader::readU16(ptr);
                si->cases.push_back({value ? value : "", resolveBlock(target_idx)});
            }
            inst.reset(si);
            break;
        }

        case QoreIRInstGroup::Phi: {
            auto* pi = new QoreIRPhiInstruction();
            pi->opcode = opcode;
            pi->value_kind = static_cast<QoreIRPhiValueKind>(QoreAOTBinaryReader::readU8(ptr));
            uint16_t num_incoming = QoreAOTBinaryReader::readU16(ptr);
            pi->incoming.reserve(num_incoming);
            for (int j = 0; j < num_incoming; ++j) {
                uint32_t val_id = QoreAOTBinaryReader::readU32(ptr);
                uint16_t block_idx = QoreAOTBinaryReader::readU16(ptr);
                pi->incoming.push_back({QoreIRValue(val_id), resolveBlock(block_idx)});
            }
            inst.reset(pi);
            break;
        }

        case QoreIRInstGroup::Guard: {
            auto* gi = new QoreIRGuardInstruction(opcode);
            uint16_t deopt_idx = QoreAOTBinaryReader::readU16(ptr);
            gi->deopt_target = resolveBlock(deopt_idx);
            const char* type_path = reader.readStringRef(ptr);
            gi->guard_id = QoreAOTBinaryReader::readU32(ptr);
            if (type_path && *type_path) {
                std::string type_error;
                QoreAOTTypeResolver type_resolver(pgm);
                gi->type_info = type_resolver.resolve(type_path, type_error);
            }
            inst.reset(gi);
            break;
        }

        case QoreIRInstGroup::ImplicitArg: {
            uint16_t offset = QoreAOTBinaryReader::readU16(ptr);
            inst = std::make_unique<QoreIRImplicitArgInstruction>(static_cast<int>(offset));
            break;
        }

        case QoreIRInstGroup::HashKeyAccess: {
            const char* key_name = reader.readStringRef(ptr);
            inst = std::make_unique<QoreIRHashKeyAccessInstruction>(key_name ? key_name : "", opcode);
            break;
        }

        case QoreIRInstGroup::SelfMember: {
            const char* member_name = reader.readStringRef(ptr);
            auto* si = new QoreIRSelfMemberInstruction(member_name ? member_name : "");
            si->opcode = opcode;
            inst.reset(si);
            break;
        }

        case QoreIRInstGroup::StaticVar: {
            const char* var_name = reader.readStringRef(ptr);
            QoreValue expr = readExpr(reader, ptr, end, error);
            if (!error.empty()) {
                return nullptr;
            }
            // Find the static var info from the class
            auto* si = new QoreIRStaticVarInstruction(nullptr, var_name ? var_name : "", expr);
            si->opcode = opcode;
            expr.discard(nullptr);
            inst.reset(si);
            break;
        }

        case QoreIRInstGroup::NewObject: {
            const char* class_path = reader.readStringRef(ptr);
            const char* variant_sig = reader.readStringRef(ptr);
            const QoreClass* qc = nullptr;
            const AbstractQoreFunctionVariant* variant = nullptr;
            if (class_path && *class_path) {
                qc = qore_aot_resolve_class_ref(pgm, class_path, false);
                if (qc && variant_sig && *variant_sig) {
                    if (const QoreMethod* cons = qc->getConstructor()) {
                        const QoreFunction* cf = qore_method_private::get(*cons)->getFunction();
                        variant = findAOTVariantBySignatureText(cf, variant_sig);
                    }
                }
            }
            auto* ni = new QoreIRNewObjectInstruction(qc, variant, QoreValue(), nullptr,
                class_path, variant_sig);
            ni->opcode = opcode;
            inst.reset(ni);
            break;
        }

        case QoreIRInstGroup::LoadConst: {
            QoreValue expr = readExpr(reader, ptr, end, error);
            if (!error.empty()) {
                return nullptr;
            }
            auto* rcr = dynamic_cast<const RuntimeConstantRefNode*>(expr.getInternalNode());
            auto* lci = new QoreIRLoadConstantInstruction(rcr, expr);
            lci->opcode = opcode;
            expr.discard(nullptr);
            inst.reset(lci);
            break;
        }

        case QoreIRInstGroup::CreateClosure: {
            QoreValue expr = readExpr(reader, ptr, end, error);
            if (!error.empty()) {
                return nullptr;
            }
            auto* cci = new QoreIRCreateClosureInstruction(nullptr, expr);
            cci->opcode = opcode;
            expr.discard(nullptr);
            inst.reset(cci);
            break;
        }

        case QoreIRInstGroup::CreateCallRef: {
            QoreValue expr = readExpr(reader, ptr, end, error);
            if (!error.empty()) {
                return nullptr;
            }
            auto* cri = new QoreIRCreateCallRefInstruction(expr);
            cri->opcode = opcode;
            expr.discard(nullptr);
            inst.reset(cri);
            break;
        }

        case QoreIRInstGroup::CreateMethodRef: {
            QoreValue expr = readExpr(reader, ptr, end, error);
            if (!error.empty()) {
                return nullptr;
            }
            auto* cri = new QoreIRCreateMethodRefInstruction(expr);
            cri->opcode = opcode;
            expr.discard(nullptr);
            inst.reset(cri);
            break;
        }

        case QoreIRInstGroup::CreateParseRef: {
            QoreValue expr = readExpr(reader, ptr, end, error);
            if (!error.empty()) {
                return nullptr;
            }
            auto* cri = new QoreIRCreateParseRefInstruction(nullptr, expr);
            cri->opcode = opcode;
            expr.discard(nullptr);
            inst.reset(cri);
            break;
        }

        case QoreIRInstGroup::NewHashDecl: {
            QoreValue expr = readExpr(reader, ptr, end, error);
            if (!error.empty()) {
                return nullptr;
            }
            auto* ni = new QoreIRNewHashDeclInstruction(nullptr, expr);
            ni->opcode = opcode;
            expr.discard(nullptr);
            inst.reset(ni);
            break;
        }

        case QoreIRInstGroup::NewComplexHash: {
            QoreValue expr = readExpr(reader, ptr, end, error);
            if (!error.empty()) {
                return nullptr;
            }
            auto* ni = new QoreIRNewComplexHashInstruction(nullptr, expr);
            ni->opcode = opcode;
            expr.discard(nullptr);
            inst.reset(ni);
            break;
        }

        case QoreIRInstGroup::NewComplexList: {
            QoreValue expr = readExpr(reader, ptr, end, error);
            if (!error.empty()) {
                return nullptr;
            }
            auto* ni = new QoreIRNewComplexListInstruction(nullptr, expr);
            ni->opcode = opcode;
            expr.discard(nullptr);
            inst.reset(ni);
            break;
        }

        case QoreIRInstGroup::NewComplexBuffer: {
            QoreValue expr = readExpr(reader, ptr, end, error);
            if (!error.empty()) {
                return nullptr;
            }
            auto* ni = new QoreIRNewComplexBufferInstruction(nullptr, expr);
            ni->opcode = opcode;
            expr.discard(nullptr);
            inst.reset(ni);
            break;
        }

        case QoreIRInstGroup::VrnConstruct: {
            QoreValue expr = readExpr(reader, ptr, end, error);
            if (!error.empty()) {
                return nullptr;
            }
            auto* vi = new QoreIRVrnConstructInstruction(nullptr, expr);
            vi->opcode = opcode;
            expr.discard(nullptr);
            inst.reset(vi);
            break;
        }

        case QoreIRInstGroup::HashKeyStore: {
            const char* key_name = reader.readStringRef(ptr);
            uint32_t container_slot_id = QoreAOTBinaryReader::readU32(ptr);
            auto* hi = new QoreIRHashKeyStoreInstruction(nullptr, key_name ? key_name : "");
            hi->opcode = opcode;
            hi->container_slot_id = container_slot_id;
            // Resolve container LocalVar for the COW branch; the container
            // VarRefNode is not serialized, so container==nullptr here and the
            // interpreter COW path must use container_lv (resolved from the slot).
            if (slot_to_local && container_slot_id != UINT32_MAX) {
                auto sti = slot_to_local->find(container_slot_id);
                if (sti != slot_to_local->end()) {
                    hi->container_lv = sti->second;
                }
            }
            inst.reset(hi);
            break;
        }

        case QoreIRInstGroup::ListIndexStore: {
            uint32_t container_slot_id = QoreAOTBinaryReader::readU32(ptr);
            auto* li = new QoreIRListIndexStoreInstruction(nullptr);
            li->opcode = opcode;
            li->container_slot_id = container_slot_id;
            // Resolve container LocalVar for the COW branch (see HashKeyStore above).
            if (slot_to_local && container_slot_id != UINT32_MAX) {
                auto sti = slot_to_local->find(container_slot_id);
                if (sti != slot_to_local->end()) {
                    li->container_lv = sti->second;
                }
            }
            inst.reset(li);
            break;
        }

        case QoreIRInstGroup::HashKeyStoreDynamic: {
            uint32_t container_slot_id = QoreAOTBinaryReader::readU32(ptr);
            auto* hi = new QoreIRHashKeyStoreDynamicInstruction(nullptr);
            hi->opcode = opcode;
            hi->container_slot_id = container_slot_id;
            // Resolve container LocalVar for the COW branch (see HashKeyStore above).
            if (slot_to_local && container_slot_id != UINT32_MAX) {
                auto sti = slot_to_local->find(container_slot_id);
                if (sti != slot_to_local->end()) {
                    hi->container_lv = sti->second;
                }
            }
            inst.reset(hi);
            break;
        }

        case QoreIRInstGroup::LValuePath: {
            auto* pi = new QoreIRLValuePathInstruction(opcode);
            pi->weak = QoreAOTBinaryReader::readU8(ptr) != 0;
            pi->compound_op = static_cast<LVCompoundOp>(QoreAOTBinaryReader::readU8(ptr));
            pi->unary_op = static_cast<LVUnaryOp>(QoreAOTBinaryReader::readU8(ptr));
            pi->binary_mut_op = static_cast<LVBinaryMutOp>(QoreAOTBinaryReader::readU8(ptr));
            pi->ternary_op = static_cast<LVTernaryOp>(QoreAOTBinaryReader::readU8(ptr));
            if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_LVPATH_DELETE_EXPR) != 0) {
                if (QoreAOTBinaryReader::readU8(ptr)) {
                    QoreValue legacy_delete_lvalue_expr = readExpr(reader, ptr, end, error);
                    legacy_delete_lvalue_expr.discard(nullptr);
                    if (!error.empty()) {
                        delete pi;
                        return nullptr;
                    }
                }
            }
            uint8_t num_steps = QoreAOTBinaryReader::readU8(ptr);
            auto resolve_global_lvalue_root = [&](LVPathStep& step, const std::string& name) -> bool {
                Var* var = qore_root_ns_private::runtimeFindGlobalVar(*pp->RootNS, name.c_str());
                if (!var) {
                    error = "cannot resolve global lvalue path root '" + name + "'";
                    return false;
                }
                step.kind = var->isThreadLocal() ? LVPathStepKind::ThreadLocalVar : LVPathStepKind::GlobalVar;
                step.name = name;
                step.ref_ptr = var;
                return true;
            };
            // Resolve a class static in the program that owns this code, so that a class private to its module
            // can be reached; such a class never appears in the namespace of the program that loads the module,
            // where the symbolic path below is resolved when the assignment runs.  Best-effort: the class may
            // legitimately not be registered yet (a sibling .qo file, for example), in which case the symbolic
            // path remains the only resolution mechanism.
            auto resolve_static_lvalue_fallback = [&](LVPathStep& step, const std::string& name) {
                QoreProgram* owner_pgm = local_owner_pgm ? local_owner_pgm : pgm;
                if (!owner_pgm) {
                    return;
                }
                std::string var_name;
                step.aot_static_var_info = qore_find_static_var_by_path(*owner_pgm, name, var_name);
            };
            for (uint8_t i = 0; i < num_steps; ++i) {
                LVPathStep step;
                step.kind = static_cast<LVPathStepKind>(QoreAOTBinaryReader::readU8(ptr));
                step.slot_id = QoreAOTBinaryReader::readU32(ptr);
                const char* name = reader.readStringRef(ptr);
                step.name = name ? name : "";
                step.operand_idx = QoreAOTBinaryReader::readU32(ptr);
                // Slice steps carry an SSA id vector (HashKeySlice /
                // ListIndexSlice / ListRangeSlice) — matches writer's wire format.
                if (step.kind == LVPathStepKind::HashKeySlice
                        || step.kind == LVPathStepKind::ListIndexSlice
                        || step.kind == LVPathStepKind::ListRangeSlice) {
                    uint32_t num_slice_ops = QoreAOTBinaryReader::readU32(ptr);
                    step.slice_operand_ids.reserve(num_slice_ops);
                    for (uint32_t k = 0; k < num_slice_ops; ++k) {
                        step.slice_operand_ids.push_back(
                                QoreAOTBinaryReader::readU32(ptr));
                    }
                }
                // Resolve local vars from AOT locals
                if ((step.kind == LVPathStepKind::LocalVar || step.kind == LVPathStepKind::ClosureVar)
                        && step.slot_id != UINT32_MAX && step.slot_id < ctx->num_locals) {
                    step.ref_ptr = ctx->locals[step.slot_id];
                }
                // GlobalVar/ThreadLocalVar: resolve by name via program namespace
                if ((step.kind == LVPathStepKind::GlobalVar || step.kind == LVPathStepKind::ThreadLocalVar)
                        && !step.name.empty()) {
                    if (!resolve_global_lvalue_root(step, step.name)) {
                        delete pi;
                        return nullptr;
                    }
                } else if (step.kind == LVPathStepKind::StaticVar && !step.name.empty()) {
                    if (step.name.size() > 2 && step.name[0] == ':' && step.name[1] == ':') {
                        std::string global_name = step.name.substr(2);
                        if (!resolve_global_lvalue_root(step, global_name)) {
                            delete pi;
                            return nullptr;
                        }
                    } else {
                        step.ref_ptr = nullptr;
                        resolve_static_lvalue_fallback(step, step.name);
                    }
                }
                pi->path.push_back(std::move(step));
            }
            inst.reset(pi);
            break;
        }

        case QoreIRInstGroup::FusedAddLocal: {
            const char* target_name = reader.readStringRef(ptr);
            const char* source_name = reader.readStringRef(ptr);
            uint32_t target_slot_id = QoreAOTBinaryReader::readU32(ptr);
            uint32_t source_slot_id = QoreAOTBinaryReader::readU32(ptr);
            bool target_ir_only = QoreAOTBinaryReader::readU8(ptr) != 0;
            LocalVar* target_lv = resolveLocal(target_name);
            LocalVar* source_lv = resolveLocal(source_name);
            auto* fi = new QoreIRAddAssignLocalIntInstruction(target_lv, source_lv);
            fi->target_slot_id = target_slot_id;
            fi->source_slot_id = source_slot_id;
            fi->target_ir_only = target_ir_only;
            inst.reset(fi);
            break;
        }

        case QoreIRInstGroup::FusedIncLocal: {
            const char* local_name = reader.readStringRef(ptr);
            int64_t delta = QoreAOTBinaryReader::readI64(ptr);
            uint32_t slot_id = QoreAOTBinaryReader::readU32(ptr);
            bool ir_only = QoreAOTBinaryReader::readU8(ptr) != 0;
            LocalVar* lv = resolveLocal(local_name);
            auto* fi = new QoreIRIncrementLocalIntInstruction(lv, delta);
            fi->slot_id = slot_id;
            fi->ir_only = ir_only;
            inst.reset(fi);
            break;
        }

        case QoreIRInstGroup::FusedBrLtLocal: {
            const char* lhs_name = reader.readStringRef(ptr);
            const char* rhs_name = reader.readStringRef(ptr);
            uint32_t lhs_slot_id = QoreAOTBinaryReader::readU32(ptr);
            uint32_t rhs_slot_id = QoreAOTBinaryReader::readU32(ptr);
            uint16_t true_idx = QoreAOTBinaryReader::readU16(ptr);
            uint16_t false_idx = QoreAOTBinaryReader::readU16(ptr);
            LocalVar* lhs_lv = resolveLocal(lhs_name);
            LocalVar* rhs_lv = resolveLocal(rhs_name);
            auto* fi = new QoreIRBranchIfLtLocalIntInstruction(
                lhs_lv, rhs_lv, resolveBlock(true_idx), resolveBlock(false_idx));
            fi->lhs_slot_id = lhs_slot_id;
            fi->rhs_slot_id = rhs_slot_id;
            inst.reset(fi);
            break;
        }

        case QoreIRInstGroup::MapHashKey: {
            const char* key1 = reader.readStringRef(ptr);
            const char* key2 = reader.readStringRef(ptr);
            inst = std::make_unique<QoreIRMapHashKeyInstruction>(opcode,
                key1 ? key1 : "", key2 ? key2 : "");
            break;
        }

        case QoreIRInstGroup::IteratorCreate: {
            uint32_t iterable_id = QoreAOTBinaryReader::readU32(ptr);
            auto* ii = new QoreIRIteratorCreateInstruction(QoreIRValue(iterable_id));
            ii->opcode = opcode;
            inst.reset(ii);
            break;
        }

        case QoreIRInstGroup::IteratorNext: {
            uint32_t iterator_id = QoreAOTBinaryReader::readU32(ptr);
            uint16_t done_idx = QoreAOTBinaryReader::readU16(ptr);
            uint16_t continue_idx = QoreAOTBinaryReader::readU16(ptr);
            if ((opcode == QoreIROpcode::TypedForeachNextInt
                    || opcode == QoreIROpcode::TypedForeachNextFloat
                    || opcode == QoreIROpcode::TypedForeachNextBool
                    || opcode == QoreIROpcode::TypedForeachNextString)
                    && operands.size() == 3) {
                inst = std::make_unique<QoreIRIteratorNextInstruction>(opcode,
                    QoreIRValue(iterator_id), operands[1], operands[2], resolveBlock(done_idx),
                    resolveBlock(continue_idx));
            } else {
                auto* next = new QoreIRIteratorNextInstruction(
                    QoreIRValue(iterator_id), resolveBlock(done_idx), resolveBlock(continue_idx));
                next->opcode = opcode;
                inst.reset(next);
            }
            break;
        }

        case QoreIRInstGroup::RefForeachInit: {
            QoreValue expr = readExpr(reader, ptr, end, error);
            if (!error.empty()) {
                return nullptr;
            }
            auto* ri = new QoreIRRefForeachInitInstruction(expr);
            ri->opcode = opcode;
            expr.discard(nullptr);
            inst.reset(ri);
            break;
        }

        case QoreIRInstGroup::OnBlockExit: {
            obe_type_e obe_type = static_cast<obe_type_e>(QoreAOTBinaryReader::readU8(ptr));
            uint8_t has_handler_ir = QoreAOTBinaryReader::readU8(ptr);
            std::unique_ptr<QoreIRFunction> nested_handler;
            if (has_handler_ir) {
                nested_handler = deserializeIRFunction(reader, ptr, end, pgm, readExpr,
                    &local_map, error, parent_locals_arr, num_parent_locals,
                    nullptr, false, nullptr, local_owner_pgm);
                if (!nested_handler) {
                    error = "failed to deserialize nested OnBlockExit handler IR: " + error;
                    return nullptr;
                }
                nested_handler->computeSlotIdsAndEmbed();
            } else {
                error = "OnBlockExit without handler IR in deserialized context";
                return nullptr;
            }
            auto* obe_inst = new QoreIROnBlockExitInstruction(obe_type, std::move(nested_handler));
            obe_inst->opcode = opcode;
            inst.reset(obe_inst);
            break;
        }

        case QoreIRInstGroup::SwitchRegexMatch: {
            const char* pattern = reader.readStringRef(ptr);
            int64_t options = QoreAOTBinaryReader::readI64(ptr);
            bool is_negated = QoreAOTBinaryReader::readU8(ptr) != 0;
            ExceptionSink xsink;
            QoreRegex* re = new QoreRegex(pattern ? pattern : "", options, &xsink);
            if (xsink) {
                delete re;
                error = "failed to compile regex pattern for SwitchRegexMatch";
                return nullptr;
            }
            const CaseNodeRegex* cnode = is_negated
                ? new CaseNodeNegRegex(&loc_builtin, re, nullptr)
                : new CaseNodeRegex(&loc_builtin, re, nullptr);
            auto* sri = new QoreIRSwitchRegexMatchInstruction(cnode);
            sri->opcode = opcode;
            sri->owns_regex_case = true;
            inst.reset(sri);
            break;
        }

        case QoreIRInstGroup::MakeHashConstKeys: {
            uint16_t key_count = QoreAOTBinaryReader::readU16(ptr);
            std::vector<std::string> keys;
            keys.reserve(key_count);
            for (uint16_t k = 0; k < key_count; ++k) {
                const char* key = reader.readStringRef(ptr);
                keys.push_back(key ? key : "");
            }
            inst.reset(new QoreIRMakeHashConstKeysInstruction(std::move(keys)));
            break;
        }

        case QoreIRInstGroup::SwitchCaseMatch: {
            uint8_t has_val = QoreAOTBinaryReader::readU8(ptr);
            QoreValue case_val;
            if (has_val) {
                case_val = readExpr(reader, ptr, end, error);
                if (!error.empty()) {
                    return nullptr;
                }
            }
            // Create a CaseNode with the deserialized value expression
            auto* cnode = new CaseNode(&loc_builtin, case_val, nullptr);
            auto* scm = new QoreIRSwitchCaseMatchInstruction(cnode);
            scm->owns_case_node = true;
            inst.reset(scm);
            break;
        }

        case QoreIRInstGroup::ListIndexAccess: {
            inst.reset(new QoreIRListIndexAccessInstruction());
            break;
        }

        // Summarize still holds a raw SummarizeStatement* pointer; encountering
        // it on deserialization is a serialization error (Context is now native
        // IR and handled via registry dispatch).
        case QoreIRInstGroup::Summarize:
            error = "unsupported AST-delegation instruction group " + std::to_string(group_byte);
            return nullptr;

        default:
            error = "unsupported IR instruction group " + std::to_string(group_byte);
            return nullptr;
    }
    #endif

    // Set base fields
    inst->result = QoreIRValue(result_id);
    inst->operands = std::move(operands);
    inst->exception_target = exc_target;

    // Read source location (AOT location table).
    //
    // readStringRef() returns a pointer INTO the binary reader's decompressed
    // string pool, whose storage dies when the enclosing
    // QoreAOTBinaryDeserializer goes out of scope at the end of its parent
    // function (qore_aot_module_init_v3, etc.).  `QoreProgramLocation::file`
    // is a raw `const char*` that does not copy — so without interning, the
    // file pointer dangles the moment deserialization returns and any later
    // exception throw reports garbage bytes for `ex.file` / callstack[*].file.
    //
    // Intern via qore_program_private::addString() so the string lives in the
    // program's str_vec pool for the program's lifetime, matching how the
    // parser's addFile() already stores filenames for JIT/source-parsed code.
    int start_line = qore_aot_valid_line(qore_aot_read_line(reader, ptr));
    int end_line = qore_aot_valid_line(qore_aot_read_line(reader, ptr));
    const char* loc_file = reader.readStringRef(ptr);
    if (start_line > 0 && owner_func) {
        const char* interned_file = (loc_file && pgm)
            ? qore_program_private::get(*pgm)->addString(loc_file)
            : (loc_file ? loc_file : "");
        auto* loc = new QoreProgramLocation(interned_file, start_line, end_line);
        owner_func->owned_locations.push_back(loc);
        inst->loc = loc;
    }

    return inst;
}

std::unique_ptr<QoreIRFunction> deserializeIRFunction(
        const QoreAOTBinaryReader& reader,
        const uint8_t*& ptr,
        const uint8_t* end,
        QoreProgram* pgm,
        const AOTExprReadFunc& readExpr,
        const std::unordered_map<std::string, LocalVar*>* enclosing_locals,
        std::string& error,
        LocalVar** parent_locals_arr,
        int num_parent_locals,
        std::vector<LocalVar*>* extended_closure_locals,
        bool use_parent_locals_for_all_slots,
        const std::vector<LocalVar*>* direct_body_locals,
        QoreProgram* local_owner_pgm,
        bool metadata_only,
        ExceptionSink* cancel_xsink) {
    const uint8_t* func_start = ptr;
    auto remaining = [&ptr, end]() -> size_t {
        return end >= ptr ? static_cast<size_t>(end - ptr) : 0;
    };
    auto need = [&remaining, &error](size_t bytes, const char* field) -> bool {
        if (remaining() < bytes) {
            error = "truncated IR function while reading ";
            error += field ? field : "<unknown field>";
            error += ": need ";
            error += std::to_string(bytes);
            error += " byte(s), have ";
            error += std::to_string(remaining());
            return false;
        }
        return true;
    };
    size_t cancel_ordinal = 0;
    auto checkCancel = [&cancel_ordinal, cancel_xsink, &error]() -> bool {
        if (!cancel_xsink || ++cancel_ordinal % 100
                || !qore_check_cancel(cancel_xsink, "AOT lazy closure IR materialization")) {
            return false;
        }
        error = "lazy closure IR materialization was cancelled";
        return true;
    };

    // 1. Function header
    if (!need(4, "function name")) {
        return nullptr;
    }
    const char* func_name = reader.readStringRef(ptr);
    if (!need(4, "max_value_id")) {
        return nullptr;
    }
    uint32_t max_value_id = QoreAOTBinaryReader::readU32(ptr);
    if (!need(4, "max_local_slot_id")) {
        return nullptr;
    }
    uint32_t max_local_slot_id = QoreAOTBinaryReader::readU32(ptr);
    if (!need(4, "num_guards")) {
        return nullptr;
    }
    uint32_t num_guards = QoreAOTBinaryReader::readU32(ptr);
    if (!need(4, "return type")) {
        return nullptr;
    }
    const char* return_type_path = reader.readStringRef(ptr);
    // Phase C: Deserialize parent_slot_count for handler IR functions
    if (!need(4, "parent_slot_count")) {
        return nullptr;
    }
    uint32_t parent_slot_count = QoreAOTBinaryReader::readU32(ptr);
    if (!need(2, "block count")) {
        return nullptr;
    }
    uint16_t num_blocks = QoreAOTBinaryReader::readU16(ptr);
    if (!need(2, "local slot count")) {
        return nullptr;
    }
    uint16_t num_local_slots = QoreAOTBinaryReader::readU16(ptr);
    if (!need(2, "body local count")) {
        return nullptr;
    }
    uint16_t num_body_locals = QoreAOTBinaryReader::readU16(ptr);

    const char* display_func_name = func_name && *func_name ? func_name : "<unnamed>";
    if (!num_blocks) {
        error = "malformed IR function '";
        error += display_func_name;
        error += "': block count is zero";
        return nullptr;
    }
    bool has_local_decl_ordinal = (reader.getHeader().feature_flags & QORE_AOT_FEAT_LOCAL_DECL_ORDINAL) != 0;
    const size_t min_local_bytes = static_cast<size_t>(num_local_slots) * (has_local_decl_ordinal ? 16 : 12);
    const size_t min_body_local_bytes = static_cast<size_t>(num_body_locals)
        * (((reader.getHeader().feature_flags & QORE_AOT_FEAT_BODY_LOCAL_SLOT) != 0) ? 12 : 8);
    const size_t min_block_bytes = static_cast<size_t>(num_blocks) * 7;
    if (min_local_bytes > remaining()
            || min_body_local_bytes > remaining() - std::min(min_local_bytes, remaining())
            || min_block_bytes > remaining() - std::min(min_local_bytes + min_body_local_bytes, remaining())) {
        error = "malformed IR function '";
        error += display_func_name;
        error += "': header counts exceed remaining payload";
        error += " (blocks=" + std::to_string(num_blocks);
        error += ", local_slots=" + std::to_string(num_local_slots);
        error += ", body_locals=" + std::to_string(num_body_locals);
        error += ", remaining=" + std::to_string(remaining());
        error += ")";
        return nullptr;
    }

    auto func = std::make_unique<QoreIRFunction>(func_name ? func_name : "");
    func->max_value_id = max_value_id;
    func->max_local_slot_id = max_local_slot_id;
    func->num_guards = num_guards;
    func->parent_slot_count = parent_slot_count;

    // Resolve return type
    if (return_type_path && *return_type_path) {
        std::string type_error;
        QoreAOTTypeResolver type_resolver(pgm);
        func->return_type_info = type_resolver.resolve(return_type_path, type_error);
    }
    QoreAOTTypeResolver local_type_resolver(pgm);

    // 2. Read local variable slot table and build name→LocalVar* map.
    //
    // The serialized slot table is authoritative for local identity.  Multiple
    // block-scoped locals can legitimately have the same name and type in one
    // function (for example two separate `string sql` declarations in if/else
    // branches).  Only locals supplied by the enclosing scope may be reused by
    // name; function-owned locals from the slot table must remain distinct by
    // slot ID.
    std::unordered_map<std::string, LocalVar*> local_map;
    std::unordered_set<LocalVar*> enclosing_local_set;
    if (enclosing_locals) {
        local_map = *enclosing_locals;
        for (const auto& i : *enclosing_locals) {
            if (checkCancel()) {
                return nullptr;
            }
            if (i.second) {
                enclosing_local_set.insert(i.second);
            }
        }
    }
    std::unordered_map<std::string, std::vector<LocalVar*>> created_slot_locals;
    auto makeLocalKey = [](const char* lname, const char* ltype) -> std::string {
        std::string key(lname ? lname : "");
        if (ltype && *ltype) {
            key += '\x1f';
            key += ltype;
        }
        return key;
    };
    auto rememberCreatedSlotLocal = [&created_slot_locals, &makeLocalKey](
            const char* lname, const char* ltype, LocalVar* lv) {
        if (!lv || !lname || !*lname) {
            return;
        }
        created_slot_locals[makeLocalKey(lname, ltype)].push_back(lv);
        created_slot_locals[lname].push_back(lv);
    };
    auto findEnclosingLocal = [&local_map, &enclosing_local_set, &makeLocalKey, &local_type_resolver](
            const char* lname, const char* ltype) -> LocalVar* {
        if (!lname || !*lname) {
            return nullptr;
        }
        if (ltype && *ltype) {
            auto cit = local_map.find(makeLocalKey(lname, ltype));
            if (cit != local_map.end() && enclosing_local_set.count(cit->second)) {
                return cit->second;
            }
            auto it = local_map.find(lname);
            if (it != local_map.end() && enclosing_local_set.count(it->second)) {
                if (aotLocalTypeMatches(it->second, ltype, &local_type_resolver)) {
                    return it->second;
                }
                if (aotLocalTypeKnownMismatch(it->second, ltype, &local_type_resolver)) {
                    return nullptr;
                }
                // Reference parameters are serialized as their lvalue slot type
                // (for example auto) while the signature local keeps the declared
                // reference type. Other source-stripped type paths can also fail
                // equality checks despite referring to the same source local.
                // Reuse only after excluding a provable concrete type mismatch.
                return it->second;
            }
            return nullptr;
        }
        auto it = local_map.find(lname);
        return it != local_map.end() && enclosing_local_set.count(it->second)
            ? it->second : nullptr;
    };
    auto createLocal = [pgm, local_owner_pgm](const char* lname, const char* ltype) -> LocalVar* {
        if (!pgm) {
            printd(5, "AOT IR deser: local '%s' not found in enclosing scope and no pgm\n", lname);
            return nullptr;
        }
        std::string type_error;
        QoreAOTTypeResolver type_resolver(pgm);
        const QoreTypeInfo* ti = (ltype && *ltype)
            ? type_resolver.resolve(ltype, type_error) : nullptr;
        qore_program_private* pp = qore_program_private::get(
            *(local_owner_pgm ? local_owner_pgm : pgm));
        return pp->createLocalVar(lname, ti);
    };
    for (int i = 0; i < num_local_slots; ++i) {
        if (checkCancel()) {
            return nullptr;
        }
        if (!need(has_local_decl_ordinal ? 16 : 12, "local slot table entry")) {
            return nullptr;
        }
        const char* lname = reader.readStringRef(ptr);
        const char* ltype = reader.readStringRef(ptr);
        uint32_t slot_id = QoreAOTBinaryReader::readU32(ptr);
        uint32_t body_ordinal = has_local_decl_ordinal
            ? QoreAOTBinaryReader::readU32(ptr) : UINT32_MAX;

        if (!lname || !*lname) {
            continue;
        }

        if (use_parent_locals_for_all_slots) {
            if (!parent_locals_arr || num_parent_locals <= 0) {
                error = "malformed IR function '";
                error += display_func_name;
                error += "': local slot table entry ";
                error += std::to_string(i);
                error += " ('";
                error += lname;
                error += "') requires parent local slots but none are available";
                return nullptr;
            }
            if (slot_id >= static_cast<uint32_t>(num_parent_locals) || !parent_locals_arr[slot_id]) {
                error = "malformed IR function '";
                error += display_func_name;
                error += "': local slot table entry ";
                error += std::to_string(i);
                error += " ('";
                error += lname;
                if (ltype && *ltype) {
                    error += "' type '";
                    error += ltype;
                }
                error += "') has slot id ";
                error += std::to_string(slot_id);
                error += " outside parent local slot count ";
                error += std::to_string(num_parent_locals);
                error += "; debug/source-stripped IR must round-trip exact AOT local slots";
                return nullptr;
            }
            LocalVar* parent_lv = parent_locals_arr[slot_id];
            func->local_var_slots[parent_lv] = slot_id;
            local_map[lname] = parent_lv;
            if (ltype && *ltype) {
                local_map[makeLocalKey(lname, ltype)] = parent_lv;
            }
            enclosing_local_set.insert(parent_lv);
            continue;
        }

        // For handler parent slots (slot_id < parent_slot_count), use direct
        // slot-indexed access to the parent locals array to avoid name-collision
        // ambiguity when the parent function has same-named variables.  Full
        // source-stripped debug IR is serialized against the enclosing AOT
        // context's complete local slot table, so allow the same direct mapping
        // for every slot on that path.
        if (parent_locals_arr
                && (use_parent_locals_for_all_slots || slot_id < (uint32_t)parent_slot_count)
                && num_parent_locals > 0
                && slot_id < static_cast<uint32_t>(num_parent_locals)
                && parent_locals_arr[slot_id]) {
            LocalVar* parent_lv = parent_locals_arr[slot_id];
            func->local_var_slots[parent_lv] = slot_id;
            local_map[lname] = parent_lv;
            if (ltype && *ltype) {
                local_map[makeLocalKey(lname, ltype)] = parent_lv;
            }
            enclosing_local_set.insert(parent_lv);
            continue;
        }

        // A declaration ordinal proves that this slot belongs to the current
        // function.  Resolve it before the enclosing-name lookup so a closure
        // body local cannot alias a same-named, same-typed parent local.
        if (body_ordinal != UINT32_MAX) {
            LocalVar* body_lv = nullptr;
            if (direct_body_locals
                    && body_ordinal < static_cast<uint32_t>(direct_body_locals->size())) {
                LocalVar* direct_lv = (*direct_body_locals)[body_ordinal];
                if (direct_lv && direct_lv->getName()
                        && strcmp(direct_lv->getName(), lname) == 0
                        && aotLocalTypeMatches(direct_lv, ltype, &local_type_resolver)) {
                    body_lv = direct_lv;
                }
            }
            if (!body_lv) {
                body_lv = createLocal(lname, ltype);
            }
            if (!body_lv) {
                error = "cannot reconstruct body local '";
                error += lname;
                error += "' in IR function '";
                error += display_func_name;
                error += "'";
                return nullptr;
            }
            func->local_var_slots[body_lv] = slot_id;
            local_map.emplace(lname, body_lv);
            if (ltype && *ltype) {
                local_map.emplace(makeLocalKey(lname, ltype), body_lv);
            }
            rememberCreatedSlotLocal(lname, ltype, body_lv);
            continue;
        }

        if (LocalVar* enclosing_lv = findEnclosingLocal(lname, ltype)) {
            func->local_var_slots[enclosing_lv] = slot_id;
            continue;
        }

        // Create a new local variable (handler-specific local not in enclosing scope)
        if (LocalVar* lv = createLocal(lname, ltype)) {
            func->local_var_slots[lv] = slot_id;
            local_map.emplace(lname, lv);
            if (ltype && *ltype) {
                local_map.emplace(makeLocalKey(lname, ltype), lv);
            }
            rememberCreatedSlotLocal(lname, ltype, lv);
        }
    }

    if (extended_closure_locals) {
        for (const auto& i : func->local_var_slots) {
            if (checkCancel()) {
                return nullptr;
            }
            if (extended_closure_locals->size() <= i.second) {
                extended_closure_locals->resize(i.second + 1, nullptr);
            }
            (*extended_closure_locals)[i.second] = const_cast<LocalVar*>(i.first);
        }
    }

    std::unordered_map<uint32_t, LocalVar*> body_slot_to_local;
    for (auto& [lv, sid] : func->local_var_slots) {
        if (checkCancel()) {
            return nullptr;
        }
        body_slot_to_local[sid] = const_cast<LocalVar*>(lv);
    }

    // 3. Read body locals
    for (int i = 0; i < num_body_locals; ++i) {
        if (checkCancel()) {
            return nullptr;
        }
        const char* blname = reader.readStringRef(ptr);
        const char* bltype = reader.readStringRef(ptr);
        uint32_t bl_slot_id = UINT32_MAX;
        if ((reader.getHeader().feature_flags & QORE_AOT_FEAT_BODY_LOCAL_SLOT) != 0) {
            bl_slot_id = QoreAOTBinaryReader::readU32(ptr);
        }

        if (blname && *blname) {
            LocalVar* lv = nullptr;
            if (direct_body_locals && i < static_cast<int>(direct_body_locals->size())) {
                lv = (*direct_body_locals)[i];
            }
            if (!lv && bl_slot_id != UINT32_MAX) {
                auto slot_it = body_slot_to_local.find(bl_slot_id);
                if (slot_it != body_slot_to_local.end()) {
                    lv = slot_it->second;
                }
            }
            if (!lv) {
                auto key = makeLocalKey(blname, bltype);
                auto cit = created_slot_locals.find(key);
                if (cit == created_slot_locals.end() || cit->second.empty()) {
                    cit = created_slot_locals.find(blname);
                }
                if (cit != created_slot_locals.end() && !cit->second.empty()) {
                    lv = cit->second.front();
                    cit->second.erase(cit->second.begin());
                } else if (LocalVar* enclosing_lv = findEnclosingLocal(blname, bltype)) {
                    lv = enclosing_lv;
                } else {
                    auto it = local_map.find(blname);
                    if (it != local_map.end()) {
                        lv = it->second;
                    }
                }
            }
            if (lv) {
                func->all_body_locals.push_back(lv);
            }
        }
    }

    // 4. Build slot_id -> LocalVar* map for slot-indexed instruction resolution.
    // This avoids name-collision ambiguity when the enclosing function has
    // multiple variables with the same name in different scopes.
    std::unordered_map<uint32_t, LocalVar*> slot_to_local;
    for (auto& [lv, sid] : func->local_var_slots) {
        if (checkCancel()) {
            return nullptr;
        }
        slot_to_local[sid] = const_cast<LocalVar*>(lv);
    }

    // 4b. For handler/closure IR expressions: always delegate to the caller's
    // readExpr, which routes through the enclosing (parent) AOT context.
    //
    // The writer side (classifyAndWriteExpr + its EXPR_TREE fallback) resolves
    // local/global variable references against the PARENT function's
    // parent_locals / parent_globals identity vectors (see QoreAOTBinary.cpp —
    // LOCAL_VARREF writes `parent_locals` index as a string, GLOBAL_VARREF
    // writes `parent_globals` index, EXPR_TREE blobs seed their temp_slots
    // from parent_locals). All slot references in this function's serialized
    // expression fields therefore live in the PARENT's AOT slot space.
    //
    // Using a locals array built from this function's own IR slot ids would
    // mis-index those references — so we keep `effectiveReadExpr = &readExpr`
    // and rely on the caller (typically the handler-specific readExprCb in
    // buildContextFromSlotMap) to resolve through the parent ctx.
    const AOTExprReadFunc* effectiveReadExpr = &readExpr;

    if (metadata_only) {
        ptr = end;
        return func;
    }

    // 5. Pre-create all blocks (needed for forward references)
    func->blocks.reserve(num_blocks);
    for (int i = 0; i < num_blocks; ++i) {
        if (checkCancel()) {
            return nullptr;
        }
        func->blocks.push_back(std::make_unique<QoreIRBasicBlock>(""));
    }

    // 6. Read blocks and instructions
    const bool disable_trace_cache = getenv("QORE_DISABLE_AOT_IR_TRACE_CACHE") != nullptr;
    const char* cached_trace = disable_trace_cache ? nullptr : getenv("QORE_AOT_TRACE_IR_DESER");
    for (int i = 0; i < num_blocks; ++i) {
        if (checkCancel()) {
            return nullptr;
        }
        const char* block_name = reader.readStringRef(ptr);
        bool is_loop_header = QoreAOTBinaryReader::readU8(ptr) != 0;
        uint16_t num_insts = QoreAOTBinaryReader::readU16(ptr);

        func->blocks[i]->name = block_name ? block_name : "";
        func->blocks[i]->is_loop_header = is_loop_header;

        for (int j = 0; j < num_insts; ++j) {
            if (checkCancel()) {
                return nullptr;
            }
            const uint8_t* inst_start = ptr;
            if (const char* trace = disable_trace_cache
                    ? getenv("QORE_AOT_TRACE_IR_DESER") : cached_trace) {
                bool match = !*trace || (func->name.find(trace) != std::string::npos);
                if (match && ptr + 3 <= end) {
                    uint16_t peek_opcode = static_cast<uint16_t>(ptr[0] | (static_cast<uint16_t>(ptr[1]) << 8));
                    uint8_t peek_group = ptr[2];
                    const OpcodeInfo* oi = getOpcodeInfo(peek_opcode);
                    const auto* gi = getAOTInstGroupInfo(peek_group);
                    fprintf(stderr,
                        "[aot-ir-deser] func=%s block=%d inst=%d off=%zu opcode=%s(%u) group=%s(%u)\n",
                        func->name.c_str(), i, j, static_cast<size_t>(inst_start - func_start),
                        oi && oi->name ? oi->name : "<unknown>", peek_opcode,
                        gi && gi->name ? gi->name : "<unknown>", peek_group);
                }
            }
            auto inst = deserializeIRInstruction(reader, ptr, end, func->blocks, local_map,
                func.get(), &slot_to_local, *effectiveReadExpr, pgm, local_owner_pgm, error);
            if (!inst) {
                error = "failed to deserialize instruction " + std::to_string(j)
                    + " in block " + std::to_string(i)
                    + " at offset " + std::to_string(static_cast<size_t>(inst_start - func_start))
                    + ": " + error;
                return nullptr;
            }
            if (const char* trace = disable_trace_cache
                    ? getenv("QORE_AOT_TRACE_IR_DESER") : cached_trace) {
                bool match = !*trace || (func->name.find(trace) != std::string::npos);
                if (match) {
                    fprintf(stderr,
                        "[aot-ir-deser] func=%s block=%d inst=%d consumed=%zu next_off=%zu\n",
                        func->name.c_str(), i, j, static_cast<size_t>(ptr - inst_start),
                        static_cast<size_t>(ptr - func_start));
                }
            }
            func->blocks[i]->instructions.push_back(std::move(inst));
        }
    }

    return func;
}

std::unique_ptr<QoreIRFunction> qore_aot_materialize_lazy_closure_ir(
        const QoreAOTLazyClosureIR& lazy_ir, UserVariantBase* uvb,
        ExceptionSink* xsink, std::string& error) {
    if (!lazy_ir.metadata || !lazy_ir.pgm || !uvb) {
        error = "incomplete lazy closure metadata";
        return nullptr;
    }
    auto* closure_variant = dynamic_cast<UserClosureVariant*>(uvb);
    if (!closure_variant) {
        error = "lazy closure IR is attached to a non-closure variant";
        return nullptr;
    }

    QoreAOTBinaryReader reader;
    std::string open_error;
    if (!reader.open(lazy_ir.metadata->metadata.data(),
            static_cast<uint32_t>(lazy_ir.metadata->metadata.size()), open_error)) {
        error = "metadata open failed: " + open_error;
        return nullptr;
    }
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::SLOT_MAPS);
    const uint8_t* section_data = sec ? reader.getSectionData(*sec) : nullptr;
    if (!sec || !section_data) {
        error = "metadata has no valid SLOT_MAPS section";
        return nullptr;
    }
    if (lazy_ir.ir_offset > sec->size || lazy_ir.ir_size > sec->size - lazy_ir.ir_offset) {
        error = "serialized closure IR range exceeds SLOT_MAPS section";
        return nullptr;
    }

    std::vector<LocalVar*> closure_locals;
    auto read_expr = [&lazy_ir, &closure_locals](
            const QoreAOTBinaryReader& rdr, const uint8_t*& ptr,
            const uint8_t* end, std::string& expr_error) -> QoreValue {
        LocalVar** locals = closure_locals.empty() ? nullptr : closure_locals.data();
        Var** globals = lazy_ir.globals.empty()
            ? nullptr : const_cast<Var**>(lazy_ir.globals.data());
        return readOneTopLevelIRExpr(rdr, ptr, end, expr_error, lazy_ir.pgm,
            locals, static_cast<int>(closure_locals.size()),
            globals, static_cast<int>(lazy_ir.globals.size()), lazy_ir.local_owner_pgm);
    };

    const uint8_t* ptr = section_data + lazy_ir.ir_offset;
    const uint8_t* end = ptr + lazy_ir.ir_size;
    LocalVar** parent_locals = lazy_ir.parent_locals.empty()
        ? nullptr : const_cast<LocalVar**>(lazy_ir.parent_locals.data());
    auto ir = deserializeIRFunction(reader, ptr, end, lazy_ir.pgm,
        read_expr, &lazy_ir.enclosing_locals, error,
        parent_locals, static_cast<int>(lazy_ir.parent_locals.size()),
        &closure_locals, false, &lazy_ir.body_locals, lazy_ir.local_owner_pgm,
        false, xsink);
    if (!ir) {
        return nullptr;
    }

    UserSignature* sig = uvb->getUserSignature();
    for (unsigned p = 0; p < sig->numParams(); ++p) {
        if (p && !(p % 100)
                && qore_check_cancel(xsink, "AOT lazy closure IR finalization")) {
            error = "lazy closure IR finalization was cancelled";
            return nullptr;
        }
        if (sig->lv[p]) {
            ir->param_local_vars[static_cast<int>(p)] = sig->lv[p];
        }
    }

    makeRuntimeDeserializedClosureIRNameUnique(*ir, closure_variant);
    ir->computeSlotIdsAndEmbed();
    return ir;
}

std::unique_ptr<QoreIRFunction> qore_aot_materialize_lazy_function_ir(
        const QoreAOTLazyFunctionIR& lazy_ir, UserVariantBase* uvb,
        ExceptionSink* xsink, QoreAOTContext*& context, std::string& error) {
    context = nullptr;
    if (!lazy_ir.metadata || !lazy_ir.pgm || !uvb) {
        error = "incomplete lazy function metadata";
        return nullptr;
    }

    QoreAOTBinaryReader reader;
    std::string open_error;
    if (!reader.open(lazy_ir.metadata->metadata.data(),
            static_cast<uint32_t>(lazy_ir.metadata->metadata.size()), open_error)) {
        error = "metadata open failed: " + open_error;
        return nullptr;
    }
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::SLOT_MAPS);
    const uint8_t* section_data = sec ? reader.getSectionData(*sec) : nullptr;
    if (!sec || !section_data) {
        error = "metadata has no valid SLOT_MAPS section";
        return nullptr;
    }
    if (lazy_ir.slot_entry_offset > sec->size
            || sec->size - lazy_ir.slot_entry_offset < sizeof(uint32_t)) {
        error = "serialized function slot entry exceeds SLOT_MAPS section";
        return nullptr;
    }

    const uint8_t* entry_start = section_data + lazy_ir.slot_entry_offset;
    const uint8_t* ptr = entry_start;
    uint32_t entry_size = QoreAOTBinaryReader::readU32(ptr);
    if (entry_size > static_cast<uint32_t>(sec->size - lazy_ir.slot_entry_offset - sizeof(uint32_t))) {
        error = "serialized function slot entry size exceeds SLOT_MAPS section";
        return nullptr;
    }
    const uint8_t* entry_end = ptr + entry_size;
    // name_ref + six slot counts + unsupported/lvalue-path bytes
    if (entry_size < sizeof(uint32_t) + 6 * sizeof(uint16_t) + 2) {
        error = "serialized function slot entry has a truncated header";
        return nullptr;
    }

    const uint8_t* counts = ptr;
    const char* serialized_name = reader.readStringRef(counts);
    uint16_t num_locals = QoreAOTBinaryReader::readU16(counts);
    uint16_t num_globals = QoreAOTBinaryReader::readU16(counts);
    uint16_t num_exprs = QoreAOTBinaryReader::readU16(counts);
    uint16_t num_stmts = QoreAOTBinaryReader::readU16(counts);
    uint16_t num_regex_cases = QoreAOTBinaryReader::readU16(counts);
    (void)QoreAOTBinaryReader::readU16(counts);  // body-local count
    if (!serialized_name || lazy_ir.name != serialized_name) {
        error = "serialized function slot entry name does not match its binding";
        return nullptr;
    }

    QoreAOTFunc aot_func{
        serialized_name, nullptr, num_locals, num_globals, num_exprs, num_stmts,
        num_regex_cases
    };
    std::string build_error;
    QoreAOTContext* ctx = buildContextFromSlotMap(reader, ptr, entry_end,
        uvb, lazy_ir.pgm, aot_func, serialized_name, entry_end, nullptr,
        &build_error, lazy_ir.metadata, section_data, lazy_ir.class_ctx,
        nullptr, nullptr, lazy_ir.pgm);
    if (!ctx) {
        error = build_error.empty() ? "could not reconstruct the AOT slot context" : build_error;
        return nullptr;
    }

    std::unique_ptr<QoreIRFunction> ir = ctx->materializeDebugIR(serialized_name, error);
    if (!ir) {
        delete ctx;
        return nullptr;
    }
    context = ctx;
    return ir;
}

bool QoreAOTBinaryDeserializer::installSourceParseIRFallbacks(std::string& error) {
    if (!has_slot_map_section || slot_variant_bindings.empty()) {
        return true;
    }
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::SLOT_MAPS);
    const uint8_t* section_data = sec ? reader.getSectionData(*sec) : nullptr;
    if (!sec || !section_data || sec->size < sizeof(uint32_t)) {
        error = "invalid SLOT_MAPS section while installing source-parse IR fallbacks";
        return false;
    }
    if (!reader.getInputData() || !reader.getInputSize()) {
        error = "AOT input metadata is unavailable while installing source-parse IR fallbacks";
        return false;
    }

    auto metadata = std::make_shared<QoreAOTDebugMetadata>(
        reader, reader.getInputData(), reader.getInputSize());
    const uint8_t* ptr = section_data;
    const uint8_t* end = section_data + sec->size;
    uint32_t count = QoreAOTBinaryReader::readU32(ptr);
    for (uint32_t i = 0; i < count; ++i) {
        if (i && !(i % 100)
                && qore_check_cancel(nullptr, "AOT source-parse IR fallback installation")) {
            error = "operation cancelled during AOT source-parse IR fallback installation";
            return false;
        }
        if (end - ptr < static_cast<ptrdiff_t>(sizeof(uint32_t))) {
            error = "truncated SLOT_MAPS entry size while installing source-parse IR fallbacks";
            return false;
        }
        const uint8_t* entry_start = ptr;
        uint32_t entry_size = QoreAOTBinaryReader::readU32(ptr);
        if (entry_size > static_cast<uint32_t>(end - ptr)) {
            error = "SLOT_MAPS entry exceeds its section while installing source-parse IR fallbacks";
            return false;
        }
        const uint8_t* entry_end = ptr + entry_size;
        if (entry_size < sizeof(uint32_t)) {
            error = "SLOT_MAPS entry has no function name while installing source-parse IR fallbacks";
            return false;
        }
        const char* name = reader.readStringRef(ptr);
        ptr = entry_end;
        if (!name || !*name || !strcmp(name, "_toplevel")
                || isAOTInitFunctionName(name)
                || !strncmp(name, "__aot_closure::", sizeof("__aot_closure::") - 1)) {
            continue;
        }

        const qore_class_private* class_ctx = nullptr;
        UserVariantBase* uvb = findSlotMapVariant(name, class_ctx);
        if (!uvb || uvb->getStatementBlock() || uvb->getCachedIR() || uvb->hasCachedFunction()) {
            continue;
        }
        auto lazy_ir = std::make_shared<QoreAOTLazyFunctionIR>();
        lazy_ir->metadata = metadata;
        lazy_ir->slot_entry_offset = static_cast<uint32_t>(entry_start - section_data);
        lazy_ir->pgm = pgm;
        lazy_ir->class_ctx = class_ctx;
        lazy_ir->name = name;
        uvb->setLazyAOTFunctionIR(std::move(lazy_ir));
    }
    return true;
}

static void finalizeDeserializedDebugIR(QoreIRFunction& ir, QoreProgram* pgm) {
    ir.computeSlotIdsAndEmbed();
    ir.computeIROnlyLocals();

    for (LocalVar* lv : ir.all_body_locals) {
        ir.pre_instantiated_locals.insert(reinterpret_cast<const void*>(lv));
        ir.pre_instantiated_cache.insert(lv);
    }

    if (pgm && (pgm->getParseOptions() & PO_ALLOW_DEBUGGER)) {
        ir.ir_only_locals.clear();
        ir.ast_visible_body_locals = ir.all_body_locals;
    }

    delete ir.cached_pre_instantiated;
    auto* cached_pre_inst = new std::unordered_set<const LocalVar*>();
    std::unordered_set<const void*> body_local_set;
    for (LocalVar* lv : ir.all_body_locals) {
        body_local_set.insert(reinterpret_cast<const void*>(lv));
    }
    for (const void* lv : ir.pre_instantiated_locals) {
        if (!body_local_set.count(lv)) {
            cached_pre_inst->insert(reinterpret_cast<const LocalVar*>(lv));
        }
    }
    for (LocalVar* lv : ir.ast_visible_body_locals) {
        if (!lv->closureUse()) {
            cached_pre_inst->insert(lv);
        }
    }
    ir.cached_pre_instantiated = cached_pre_inst;
}

//! Re-lower a user function variant to IR and build an AOT context.
/** @param uvb the user variant
    @param name the function name
    @param pgm the QoreProgram
    @param aot_func the AOT function descriptor (for slot counts)
    @return heap-allocated context (caller passes ownership to variant), or nullptr on failure
*/
static QoreAOTContext* buildContextForVariant(UserVariantBase* uvb, const char* name,
        QoreProgram* pgm, const QoreAOTFunc& aot_func) {
    StatementBlock* statements = uvb->getStatementBlock();
    if (!statements) {
        printd(1, "AOT: buildContextForVariant '%s': no statement block\n", name);
        return nullptr;
    }

    // Re-lower to IR (fresh AST from re-parsed source → same IR → same walk order)
    QoreIRFunction* ir_func = new QoreIRFunction(name);
    ir_func->source_variant = uvb->getAbstractFunctionVariant();

    // Record pre-instantiated locals from signature
    UserSignature* sig = uvb->getUserSignature();
    if (sig) {
        for (unsigned i = 0; i < sig->numParams(); ++i) {
            ir_func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(sig->lv[i]));
        }
        if (sig->argvid) {
            ir_func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(sig->argvid));
        }
        if (sig->selfid) {
            ir_func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(sig->selfid));
        }
    }

    QoreIRBuilder builder(ir_func);
    auto* entry = ir_func->createBlock("entry");
    builder.setBlock(entry);

    QoreParseContext parse_context(pgm);
    QoreIRLowering lowering(builder, &parse_context);
    std::string lower_error;
    if (!lowering.lowerStatementBlock(statements, lower_error)) {
        printd(1, "AOT: buildContextForVariant '%s': IR lowering failed: %s\n", name, lower_error.c_str());
        delete ir_func;
        return nullptr;
    }
    // Ensure terminator
    if (ir_func->blocks.back()->instructions.empty() ||
            (ir_func->blocks.back()->instructions.back()->opcode != QoreIROpcode::Return &&
             ir_func->blocks.back()->instructions.back()->opcode != QoreIROpcode::ReturnNothing &&
             ir_func->blocks.back()->instructions.back()->opcode != QoreIROpcode::Br &&
             ir_func->blocks.back()->instructions.back()->opcode != QoreIROpcode::Rethrow)) {
        builder.createReturnNothing();
    }

    std::string verify_error;
    if (!QoreIRVerifier::verify(*ir_func, verify_error)) {
        printd(1, "AOT: buildContextForVariant '%s': verification failed: %s\n", name, verify_error.c_str());
        delete ir_func;
        return nullptr;
    }

    // Collect ALL body locals from the statement tree (includes nested blocks from
    // for/while/if/try/switch statements) so they can be instantiated at runtime.
    collectAllStatementLocals(statements, ir_func->all_body_locals);
    removeSignatureLocalsFromBodyLocals(ir_func->all_body_locals, sig);

    // Build the context from the fresh IR (same walk order → same slot indices)
    QoreAOTContext* ctx = buildAOTContext(*ir_func, aot_func.num_locals,
        aot_func.num_globals, aot_func.num_exprs, aot_func.num_stmts,
        qore_aot_func_num_regex_cases(aot_func));
    if (ctx) {
        ctx->pgm = pgm;
        ctx->uses_argv = qore_aot_func_uses_argv(aot_func);
        ctx->uses_self = qore_aot_func_uses_self(aot_func);
    }
    if (ctx && ctx->num_lv_path_insts > 0) {
        // Keep IR function alive — LValuePath instructions reference path data in it
        ctx->lv_path_ir_func.reset(ir_func);
    } else {
        delete ir_func;
    }

    return ctx;
}

//! Skip a single slot map entry (header + all variable-length fields)
/** Used when an entry doesn't match a target function and must be skipped.
    @param reader the binary reader for string pool lookups
    @param ptr current read position (advanced past the entry)
*/
static void skipSlotMapEntry(const QoreAOTBinaryReader& reader, const uint8_t*& ptr,
        const uint8_t* end) {
    // Read the entry size prefix and jump directly to the next entry.
    // The entry_size encodes the complete byte length of this slot map entry
    // (not including the 4-byte size prefix itself), so we can skip the entry
    // without parsing individual fields.  Previous versions parsed each field
    // in turn, but this is fragile — if any field format changes or new fields
    // are added (e.g. LValuePath instructions appended after statement slots),
    // the skip logic must be kept in sync with buildContextFromSlotMap, and a
    // mismatch causes ptr to drift and eventually SEGV.
    uint32_t entry_size = QoreAOTBinaryReader::readU32(ptr);
    ptr += entry_size;
}

//! Collected init function context for later execution
struct AOTInitFuncExecInfo {
    QoreAOTContext* ctx;
    AotFunctionPtr fn_ptr;
    std::string name;
};

//! Strip root-absolute namespace prefixes (leading ::) from type paths in a signature
/** Compile-time type paths may include leading :: (root-absolute) but runtime paths don't.
    e.g. "(hash<::DataProvider::Foo>,*hash<auto>)" → "(hash<DataProvider::Foo>,*hash<auto>)"
*/
static std::string normalizeTypePaths(const std::string& sig) {
    std::string out;
    out.reserve(sig.size());
    for (size_t i = 0; i < sig.size(); ++i) {
        if (sig[i] == ':' && i + 1 < sig.size() && sig[i + 1] == ':') {
            // Check if this :: is at the start of a type name (after <, (, ,, or *)
            if (i > 0 && (sig[i - 1] == '<' || sig[i - 1] == '(' || sig[i - 1] == ','
                    || sig[i - 1] == '*')) {
                // Skip the leading ::
                i += 1;  // loop will advance past second :
                continue;
            }
        }
        out.push_back(sig[i]);
    }

    // Canonicalize parser/type-info aliases before comparing serialized AOT
    // slot-map signatures to deserialized runtime signatures.  In particular,
    // hash<auto!> is stored internally as a no-narrow marker over auto; nested
    // forms such as reference<hash<auto!>> can be serialized as
    // reference<hash<string, auto>> while the runtime parser reconstructs
    // reference<hash<auto>>.  The no-narrow marker affects lvalue narrowing,
    // not overload identity, so registration matching must compare the
    // canonical type spelling instead of raw QoreTypeInfo::getPath() output.
    auto replaceAll = [&out](const char* from, const char* to) {
        size_t from_len = strlen(from);
        size_t to_len = strlen(to);
        for (size_t pos = out.find(from); pos != std::string::npos;
                pos = out.find(from, pos + to_len)) {
            out.replace(pos, from_len, to);
        }
    };
    replaceAll("auto!", "auto");
    replaceAll("hash<string, auto>", "hash<auto>");

    return out;
}

//! Navigate a namespace path like "Ns1::Ns2" from root to find the target namespace
static qore_ns_private* findNamespaceByPath(qore_ns_private* root, const std::string& path);

//! Return the namespace-qualified key used for AOT free-function entries.
static std::string getAOTQualifiedFunctionName(qore_ns_private* ns, const char* fname) {
    std::string rv;
    ns->getPath(rv);
    if (!rv.empty()) {
        rv += "::";
    }
    rv += fname;
    return rv;
}

//! Register AOT functions using slot maps from deserialized metadata (V2 — no IR re-lowering)
/** Walks the SLOT_MAPS section, finds matching functions in the namespace tree,
    and builds context from slot identities.
    If init_func_contexts is non-null, init functions (names starting with "__const_init::"
    or "__svar_init::") are collected instead of being discarded.
*/
static void registerAOTFunctionsFromSlotMaps(
        const QoreAOTBinaryReader& reader,
        qore_ns_private* root_ns,
        QoreProgram* pgm,
        std::unordered_map<std::string, const QoreAOTFunc*>& func_map,
        int& registered,
        std::vector<AOTInitFuncExecInfo>* init_func_contexts = nullptr,
        QoreAOTTypeResolver* shared_type_resolver = nullptr,
        std::vector<std::string>* registration_errors = nullptr,
        std::shared_ptr<const QoreAOTDebugMetadata> debug_metadata = nullptr,
        bool allow_unlinked_native_inputs = false,
        int* ignored_unlinked_functions = nullptr,
        AOTClosureRuntimeBindingMap* external_closure_bindings = nullptr,
        const QoreAOTBinaryDeserializer* deserialized_variants = nullptr,
        QoreProgram* local_owner_pgm = nullptr) {
    const bool use_resolution_cache = getenv("QORE_DISABLE_AOT_RESOLUTION_CACHE") == nullptr;
    AOTSlotResolutionCache resolution_cache{pgm};
    AOTSlotResolutionCacheScope resolution_cache_scope(use_resolution_cache ? &resolution_cache : nullptr);

    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::SLOT_MAPS);
    if (!sec) {
        printd(0, "AOT v2: no SLOT_MAPS section found\n");
        if (registration_errors) {
            registration_errors->push_back("missing SLOT_MAPS section");
        }
        return;
    }
    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        printd(0, "AOT v2: invalid SLOT_MAPS section data\n");
        if (registration_errors) {
            registration_errors->push_back("invalid SLOT_MAPS section data");
        }
        return;
    }
    const uint8_t* slot_maps_start = ptr;
    const uint8_t* end = ptr + sec->size;

    uint32_t num_funcs = QoreAOTBinaryReader::readU32(ptr);

    // Keep init-function registration diagnostics behind the slot-registration trace switch.
    if (init_func_contexts) {
        int init_count = 0;
        for (auto& kv : func_map) {
            if (isAOTInitFunctionName(kv.first.c_str())) {
                ++init_count;
                if (init_count <= 5) {
                    printd(5, "  init func in func_map: '%s'\n", kv.first.c_str());
                }
            }
        }
        printd(5, "AOT registerFromSlotMaps: num_funcs=%d, func_map.size=%d, init_funcs_in_map=%d\n",
            num_funcs, (int)func_map.size(), init_count);
    }

    // Per-blob cache: slot-map entries are emitted in class-grouped order,
    // so consecutive entries repeatedly hit the same class path.  Cache the
    // last resolved class so ~5 entries per class skip the runtimeFindClass
    // namespace walk.
    std::string last_class_name;
    const QoreClass* last_qc = nullptr;
    const char* trace_slot_reg_env = getenv("QORE_AOT_TRACE_SLOT_REG");
    AOTClosureRuntimeBindingMap local_closure_bindings;
    AOTClosureRuntimeBindingMap& closure_bindings = external_closure_bindings
        ? *external_closure_bindings : local_closure_bindings;
    const bool use_slot_prefix_index = getenv("QORE_DISABLE_AOT_SLOT_PREFIX_INDEX") == nullptr;
    std::unordered_map<std::string, std::vector<const QoreAOTFunc*>> slot_prefix_index;
    if (use_slot_prefix_index) {
        slot_prefix_index.reserve(func_map.size());
        size_t index_count = 0;
        for (const auto& fi : func_map) {
            if (index_count && !(index_count % 100)
                    && qore_check_cancel(nullptr, "AOT slot prefix index build")) {
                return;
            }
            slot_prefix_index[getAOTFunctionKeyPrefix(fi.first)].push_back(fi.second);
            ++index_count;
        }
    }

    for (uint32_t f = 0; f < num_funcs; ++f) {
        const uint8_t* entry_start = ptr;
        // Read entry size prefix to know where next entry should end
        uint32_t entry_size = QoreAOTBinaryReader::readU32(ptr);
        const uint8_t* entry_end = ptr + entry_size;
        if (entry_end < ptr || entry_end > end) {
            std::string msg = "malformed SLOT_MAPS entry ";
            msg += std::to_string(f);
            msg += ": size ";
            msg += std::to_string(entry_size);
            msg += " exceeds section boundary";
            printd(0, "AOT v2: %s\n", msg.c_str());
            if (registration_errors) {
                registration_errors->push_back(std::move(msg));
            }
            return;
        }

        // Peek at function name (comes after size prefix)
        const char* func_name = reader.readStringRef(ptr);
        // Reset to after size field for buildContextFromSlotMap which reads the full entry
        ptr = entry_start + 4;

        if (!func_name || !*func_name) {
            // Skip this entry by reading through it
            printd(2, "AOT v2: skipping unnamed slot map entry\n");
            ptr = entry_start;  // reset: before size prefix for skipSlotMapEntry
            skipSlotMapEntry(reader, ptr, end);
            continue;
        }
        const bool trace_slot_reg = trace_slot_reg_env
            && (!*trace_slot_reg_env || std::strstr(func_name, trace_slot_reg_env));

        // Trace init-function slot-map entries when requested.
        if (init_func_contexts && isAOTInitFunctionName(func_name)) {
            printd(5, "  slot map entry: '%s' (in func_map: %s)\n",
                func_name, func_map.count(func_name) ? "YES" : "NO");
        }

        // Find matching AOT function
        auto it = func_map.find(func_name);
        if (it == func_map.end()) {
            const QoreAOTFunc* compatible_func = nullptr;
            std::string compatible_func_name;
            bool ambiguous_compatible_func = false;
            std::string slot_key(func_name);
            std::string slot_prefix = getAOTFunctionKeyPrefix(slot_key);
            auto check_compatible = [&](const std::string& candidate_name,
                    const QoreAOTFunc* candidate_func) {
                if (!aotSignatureStringsCompatible(slot_key.substr(slot_prefix.size()),
                        candidate_name.substr(slot_prefix.size()))) {
                    return false;
                }
                if (compatible_func && compatible_func != candidate_func) {
                    ambiguous_compatible_func = true;
                    return true;
                }
                compatible_func = candidate_func;
                compatible_func_name = candidate_name;
                return false;
            };
            if (use_slot_prefix_index) {
                auto pi = slot_prefix_index.find(slot_prefix);
                if (pi != slot_prefix_index.end()) {
                    size_t candidate_count = 0;
                    for (const QoreAOTFunc* candidate_func : pi->second) {
                        if (candidate_count && !(candidate_count % 100)
                                && qore_check_cancel(nullptr, "AOT slot compatibility lookup")) {
                            return;
                        }
                        const std::string candidate_name(candidate_func->name);
                        if (func_map.find(candidate_name) != func_map.end()
                                && check_compatible(candidate_name, candidate_func)) {
                            break;
                        }
                        ++candidate_count;
                    }
                }
            } else {
                for (auto fi = func_map.begin(); fi != func_map.end(); ++fi) {
                    if (getAOTFunctionKeyPrefix(fi->first) != slot_prefix) {
                        continue;
                    }
                    if (check_compatible(fi->first, fi->second)) {
                        break;
                    }
                }
            }
            if (compatible_func && !ambiguous_compatible_func) {
                if (trace_slot_reg) {
                    fprintf(stderr, "[aot-slot-reg] slot entry '%s' matched native table entry '%s'\n",
                        func_name, compatible_func_name.c_str());
                }
                it = func_map.find(compatible_func_name);
            }
        }
        if (it == func_map.end()) {
            if (trace_slot_reg) {
                fprintf(stderr, "[aot-slot-reg] slot entry '%s' has no native function table entry\n",
                    func_name);
            }
            // No AOT function for this entry — skip it
            // ptr is already positioned after the size field, so reset to entry_start
            ptr = entry_start;
            skipSlotMapEntry(reader, ptr, end);
            continue;
        }
        const QoreAOTFunc* aot_func = it->second;
        const bool is_native_closure = !strncmp(func_name, "__aot_closure::", 15);

        // Find the UserVariantBase in the namespace tree
        // Function names can be "funcName(types)" or "ClassName::methodName(types)"
        // We need to extract just the function name for lookup
        UserVariantBase* uvb = nullptr;
        const qore_class_private* variant_class_ctx = nullptr;
        if (deserialized_variants) {
            uvb = deserialized_variants->findSlotMapVariant(func_name,
                variant_class_ctx);
        }
        std::string fname_str;
        size_t sep = std::string::npos;
        bool qualified_method_found = false;
        if (!uvb) {
            fname_str = func_name;

            // Strip signature suffix if present (e.g., "add(int,int)" -> "add")
            size_t paren = fname_str.find('(');
            if (paren != std::string::npos) {
                fname_str.resize(paren);
            }
            sep = fname_str.rfind("::");
        }

        if (!uvb && sep != std::string::npos) {
            // Method: Namespace::ClassName::methodName — use last :: as class/method separator
            std::string class_name = fname_str.substr(0, sep);
            std::string method_name = fname_str.substr(sep + 2);

            // Handle the static-method marker used in variant keys for classes
            // with overloaded static+instance methods of the same name. The
            // compile-time key format is "ClassPath::_static_methodName(...)".
            // Strip the prefix and remember whether to search the static map
            // first.
            bool key_is_static = false;
            static const char kStaticPrefix[] = "_static_";
            if (method_name.size() > sizeof(kStaticPrefix) - 1
                    && method_name.compare(0, sizeof(kStaticPrefix) - 1,
                        kStaticPrefix) == 0) {
                key_is_static = true;
                method_name = method_name.substr(sizeof(kStaticPrefix) - 1);
            }

            // All special methods (constructor, destructor, copy) are registered
            // from slot maps.  The deserializer creates proper variant types
            // (UserConstructorVariant, UserDestructorVariant, UserCopyVariant)
            // and each dispatch path (evalConstructor/evalDestructor/evalCopy →
            // evalIntern) correctly invokes the registered AOT function through
            // the tiered cache.  Without registration, evalIntern silently
            // no-ops on source-stripped AOT because statements=nullptr AND
            // cached_aot_fn=nullptr — causing destructors to not run and copy
            // methods to produce empty objects.
            bool skip_special_method = false;

            const QoreClass* qc;
            if (last_qc && class_name == last_class_name) {
                qc = last_qc;
            } else {
                qore_program_private* pp = qore_program_private::get(*pgm);
                const qore_ns_private* found_ns = nullptr;
                qc = qore_root_ns_private::runtimeFindClass(
                    *pp->RootNS, class_name.c_str(), found_ns);
                last_qc = qc;
                last_class_name = class_name;
            }
            printd(5, "AOT slot-reg: method '%s'::'%s' class=%p\n",
                class_name.c_str(), method_name.c_str(), (void*)qc);
            if (trace_slot_reg) {
                fprintf(stderr, "[aot-slot-reg] lookup method func='%s' class='%s' method='%s' "
                    "key_static=%s class_ptr=%p\n", func_name, class_name.c_str(),
                    method_name.c_str(), key_is_static ? "true" : "false",
                    static_cast<const void*>(qc));
            }
            if (qc && !skip_special_method) {
                // Use parse-time lookup instead of findMethod/findStaticMethod;
                // runtime lookup checks committedEmpty(), which returns true for
                // deserialized pending variants.  Slot-map entries can refer to
                // self/private methods, so resolve in the class context instead
                // of only checking the local method map.
                qore_class_private* qcp = nullptr;
                // Collect both normal and static methods — a class can have both
                // a non-static and a static method with the same name (e.g.,
                // getDisplayName() and static getDisplayName(string)).
                // We search both to find the variant matching the target signature.
                // When key_is_static, search static first and fall back to
                // instance only if no static variant is present.
                const QoreMethod* m = nullptr;
                const QoreMethod* m_static = nullptr;
                variant_class_ctx = qore_class_private::get(*const_cast<QoreClass*>(qc));
                if (key_is_static) {
                    qcp = const_cast<qore_class_private*>(variant_class_ctx);
                    m = qcp->parseFindLocalStaticMethod(method_name.c_str());
                    if (!m) {
                        m = findAOTStaticMethod(qc, method_name.c_str());
                    }
                } else {
                    m = resolveAOTSelfMethod(qc, method_name.c_str(), qcp);
                    m_static = qcp->parseFindLocalStaticMethod(method_name.c_str());
                    if (!m_static) {
                        m_static = findAOTStaticMethod(qc, method_name.c_str());
                    }
                }
                if (!m) {
                    m = m_static;
                    m_static = nullptr;
                }
                printd(5, "AOT slot-reg: method lookup '%s' m=%p\n",
                    method_name.c_str(), (void*)m);
                if (trace_slot_reg) {
                    fprintf(stderr, "[aot-slot-reg] method lookup func='%s' primary=%p secondary_static=%p\n",
                        func_name, static_cast<const void*>(m), static_cast<const void*>(m_static));
                }
                if (m) {
                    qualified_method_found = true;
                    // Extract signature from the full func_name to match the correct
                    // overloaded variant.  The func_name format is:
                    //   ClassName::methodName(type1,type2,...)
                    // We need the "(type1,type2,...)" part to match against each variant.
                    std::string target_sig;
                    {
                        size_t p = std::string(func_name).find('(');
                        if (p != std::string::npos) {
                            target_sig = normalizeTypePaths(std::string(func_name).substr(p));
                        } else {
                            target_sig = "()";
                        }
                    }
                    MethodFunctionBase* mfb = qore_method_private::get(*m)->getFunction();
                    QoreFunctionIterator vi(*mfb);
                    int var_count = 0;
                    while (vi.next()) {
                        const AbstractQoreFunctionVariant* v = vi.getVariant();
                        auto* candidate = const_cast<UserVariantBase*>(
                            dynamic_cast<const UserVariantBase*>(v));
                        ++var_count;
                        if (!candidate) {
                            continue;
                        }

                        // Build the signature key for this variant in the same format
                        // as getVariantKey() uses during compilation
                        std::string var_sig("(");
                        AbstractFunctionSignature* sig = v->getSignature();
                        if (sig) {
                            const type_vec_t& types = sig->getTypeList();
                            for (size_t ti = 0; ti < types.size(); ++ti) {
                                if (ti > 0) {
                                    var_sig.append(",");
                                }
                                var_sig.append(qore_get_aot_serializable_type_path(types[ti]));
                            }
                        }
                        var_sig.append(")");

                        if (trace_slot_reg) {
                            fprintf(stderr, "[aot-slot-reg] candidate func='%s' variant[%d] sig='%s' "
                                "normalized='%s' target='%s' candidate=%p\n",
                                func_name, var_count, var_sig.c_str(),
                                normalizeTypePaths(var_sig).c_str(), target_sig.c_str(),
                                static_cast<const void*>(candidate));
                        }
                        if (aotVariantSignatureMatches(v, target_sig, shared_type_resolver, class_name.c_str())) {
                            uvb = candidate;
                            if (trace_slot_reg) {
                                fprintf(stderr, "[aot-slot-reg] matched func='%s' variant[%d]\n",
                                    func_name, var_count);
                            }
                            break;
                        }
                    }
                    // If no match found and there's a same-named static method, try that
                    if (!uvb && m_static) {
                        MethodFunctionBase* mfb_s = qore_method_private::get(*m_static)->getFunction();
                        QoreFunctionIterator vi_s(*mfb_s);
                        while (vi_s.next()) {
                            const AbstractQoreFunctionVariant* v = vi_s.getVariant();
                            auto* candidate = const_cast<UserVariantBase*>(
                                dynamic_cast<const UserVariantBase*>(v));
                            if (!candidate) {
                                continue;
                            }
                            std::string var_sig("(");
                            AbstractFunctionSignature* sig = v->getSignature();
                            if (sig) {
                                const type_vec_t& types = sig->getTypeList();
                                for (size_t ti = 0; ti < types.size(); ++ti) {
                                    if (ti > 0) {
                                        var_sig.append(",");
                                    }
                                    var_sig.append(qore_get_aot_serializable_type_path(types[ti]));
                                }
                            }
                            var_sig.append(")");
                            if (trace_slot_reg) {
                                fprintf(stderr, "[aot-slot-reg] secondary static candidate func='%s' sig='%s' "
                                    "normalized='%s' target='%s' candidate=%p\n",
                                    func_name, var_sig.c_str(), normalizeTypePaths(var_sig).c_str(),
                                    target_sig.c_str(), static_cast<const void*>(candidate));
                            }
                            if (aotVariantSignatureMatches(v, target_sig, shared_type_resolver, class_name.c_str())) {
                                uvb = candidate;
                                if (trace_slot_reg) {
                                    fprintf(stderr, "[aot-slot-reg] matched func='%s' static variant\n",
                                        func_name);
                                }
                                break;
                            }
                        }
                    }
                    if (!uvb) {
                        printd(2, "AOT slot-reg: no matching variant for '%s' "
                            "sig='%s' in %d variants\n",
                            func_name, target_sig.c_str(), var_count);
                        if (trace_slot_reg) {
                            fprintf(stderr, "[aot-slot-reg] no matching variant func='%s' target='%s' "
                                "primary_variants=%d\n", func_name, target_sig.c_str(), var_count);
                        }
                    }
                }
            } else if (trace_slot_reg && !qc) {
                fprintf(stderr, "[aot-slot-reg] class lookup failed for func='%s' class='%s'\n",
                    func_name, class_name.c_str());
            }
        }

        if (!uvb && fname_str != "_toplevel" && (sep == std::string::npos || !qualified_method_found)) {
            // Regular function — search the module's own namespace tree (not system
            // builtins) to find the correct UserFunctionVariant.  We must avoid
            // runtimeFindFunction() because it checks Qore:: namespace first and may
            // return a builtin function that shadows the module's user function.
            //
            // **Module filter**: the slot map was emitted from this module's own
            // functions (see compileNamespaceFunctions' `shouldSkipModuleItem`),
            // so the found uvb must belong to this module.  A dependency module
            // (e.g. PgsqlSqlUtilBase) may define a function of the same simple
            // name (get_table) in a different namespace (PgsqlSqlUtilBase::) and
            // get imported into this module's shadow pgm.  Without the filter,
            // the DFS walks into the imported namespace first, finds the shared
            // uvb (already registered by the dep's own slot-reg), sees
            // hasCachedFunction()==true and skips — leaving the LOCAL
            // get_table's uvb unregistered.  The runtime then dispatches to the
            // local uvb with aot_fn=null and statements=null, producing "block
            // missing return statement" errors.
            const char* current_mod = get_module_context_name();
            std::string search_fname = fname_str;
            qore_ns_private* search_ns = root_ns;
            bool qualified_function = false;
            if (sep != std::string::npos) {
                std::string ns_path = fname_str.substr(0, sep);
                search_fname = fname_str.substr(sep + 2);
                search_ns = findNamespaceByPath(root_ns, ns_path);
                qualified_function = true;
            }

            auto findFuncInNamespace = [&](qore_ns_private* ns) -> UserVariantBase* {
                if (!ns) {
                    return nullptr;
                }
                // Search in this namespace's func_list
                FunctionEntry* fe = ns->func_list.findNode(search_fname.c_str());
                if (fe) {
                    QoreFunction* f = fe->getFunction();
                    if (f && current_mod) {
                        const char* fm = f->getModuleName();
                        if (!fm || strcmp(fm, current_mod) != 0) {
                            // imported from another module — skip this match,
                            // keep walking child namespaces
                            f = nullptr;
                        }
                    }
                    if (f) {
                        // Build the target signature suffix for matching
                        std::string target_sig;
                        {
                            size_t p = std::string(func_name).find('(');
                            if (p != std::string::npos) {
                                target_sig = normalizeTypePaths(std::string(func_name).substr(p));
                            } else {
                                target_sig = "()";
                            }
                        }

                        QoreFunctionIterator vi(*f);
                        while (vi.next()) {
                            const AbstractQoreFunctionVariant* v = vi.getVariant();
                            auto* candidate = const_cast<UserVariantBase*>(
                                dynamic_cast<const UserVariantBase*>(v));
                            if (!candidate) {
                                continue;
                            }
                            // Match signature
                            std::string var_sig("(");
                            AbstractFunctionSignature* sig = v->getSignature();
                            if (sig) {
                                const type_vec_t& types = sig->getTypeList();
                                for (size_t ti = 0; ti < types.size(); ++ti) {
                                    if (ti > 0) {
                                        var_sig.append(",");
                                    }
                                    var_sig.append(qore_get_aot_serializable_type_path(types[ti]));
                                }
                            }
                            var_sig.append(")");
                            if (aotVariantSignatureMatches(v, target_sig, shared_type_resolver, fname_str.c_str())) {
                                return candidate;
                            }
                        }
                        // If no signature match but only one user variant, use it
                        QoreFunctionIterator vi2(*f);
                        while (vi2.next()) {
                            auto* candidate = const_cast<UserVariantBase*>(
                                dynamic_cast<const UserVariantBase*>(vi2.getVariant()));
                            if (candidate) {
                                return candidate;
                            }
                        }
                    }
                }
                return nullptr;
            };

            std::function<UserVariantBase*(qore_ns_private*)> findFuncInTree =
                [&](qore_ns_private* ns) -> UserVariantBase* {
                if (UserVariantBase* result = findFuncInNamespace(ns)) {
                    return result;
                }
                // Search child namespaces recursively
                for (auto ni = ns->nsl.nsmap.begin(), ne = ns->nsl.nsmap.end(); ni != ne; ++ni) {
                    if (ni->second) {
                        UserVariantBase* result = findFuncInTree(qore_ns_private::get(*ni->second));
                        if (result) {
                            return result;
                        }
                    }
                }
                return nullptr;
            };

            uvb = qualified_function ? findFuncInNamespace(search_ns) : findFuncInTree(root_ns);
            printd(5, "AOT slot-reg: function '%s' uvb=%p\n", fname_str.c_str(), (void*)uvb);
            if (trace_slot_reg) {
                fprintf(stderr, "[aot-slot-reg] function lookup func='%s' name='%s' uvb=%p qualified=%s\n",
                    func_name, fname_str.c_str(), static_cast<const void*>(uvb),
                    qualified_function ? "true" : "false");
            }
        }

        const AOTClosureRuntimeBinding* closure_binding = nullptr;
        std::vector<LocalVar*> closure_local_slots;
        if (is_native_closure) {
            auto binding_it = closure_bindings.find(func_name);
            if (binding_it != closure_bindings.end()) {
                closure_binding = &binding_it->second;
                uvb = closure_binding->uvb;
                variant_class_ctx = closure_binding->class_ctx;
                // Context reconstruction can discover nested closures and rehash
                // closure_bindings.  Keep a stable local copy while resolving this body.
                closure_local_slots = closure_binding->local_slots;
            }
            if (std::getenv("QORE_DISABLE_AOT_NATIVE_CLOSURES") != nullptr) {
                ++registered;
                func_map.erase(it);
                ptr = entry_end;
                continue;
            }
            if (!closure_binding || !uvb) {
                std::string msg = "native closure body '";
                msg += func_name;
                msg += "' has no reconstructed closure variant";
                if (registration_errors) {
                    registration_errors->push_back(std::move(msg));
                }
                ptr = entry_end;
                continue;
            }
        }

        // If the variant already has a cached AOT context (e.g., registered during
        // initial module loading and shared via qore_class_private), skip building a
        // duplicate context — the existing one is already correct and the variant is
        // shared between module and target programs.
        bool is_init_func = isAOTInitFunctionName(func_name);

        // Native-only script objects can contain bodies that are not present in
        // the linked aggregate metadata.  This happens when a standalone compile
        // saw a different preprocessor state than the aggregate link, for
        // example a guard file that defines a replacement function before a
        // later source's %ifndef block.  The linked program is authoritative:
        // if there is no matching linked variant, this native body is dead and
        // must not make startup fail.  Keep normal metadata registration strict.
        if (!uvb && allow_unlinked_native_inputs && !is_init_func && fname_str != "_toplevel") {
            ++registered;
            if (ignored_unlinked_functions) {
                ++*ignored_unlinked_functions;
            }
            func_map.erase(it);
            printd(2, "AOT slot-reg: ignored unlinked native body '%s'\n", func_name);
            ptr = entry_end;
            continue;
        }

        if (uvb && uvb->hasCachedFunction() && !is_init_func) {
            ++registered;
            func_map.erase(it);
            printd(2, "AOT slot-reg: '%s' already registered (shared variant), skipping\n", func_name);
            ptr = entry_end;
            continue;
        }

        // Build context from slot map — pass the shared type resolver
        // so body-local slot resolutions hit a cache warmed by
        // deserializeFunctionsAndMethods (same session) instead of
        // cold-resolving each type path per slot.
        std::string build_error;
        QoreAOTContext* ctx = buildContextFromSlotMap(reader, ptr, end, uvb, pgm, *aot_func,
            func_name, entry_end, shared_type_resolver, &build_error, debug_metadata,
            slot_maps_start, variant_class_ctx,
            closure_binding ? &closure_local_slots : nullptr,
            &closure_bindings, local_owner_pgm);
        // Trace init-function context construction at high debug levels.
        if (init_func_contexts && isAOTInitFunctionName(func_name)) {
            printd(5, "  buildContextFromSlotMap('%s'): ctx=%p uvb=%p\n",
                func_name, (void*)ctx, (void*)uvb);
        }
        if (ctx && uvb) {
            uvb->registerPrecompiledAOTFunction(aot_func->fn_ptr, ctx);
            ++registered;
            // Remove from func_map so any registration gap is reported precisely.
            func_map.erase(it);
            printd(2, "AOT slot-reg: registered '%s' from slot map\n", func_name);
            if (is_native_closure && getenv("QORE_AOT_DEBUG_NATIVE_CLOSURES")) {
                fprintf(stderr, "AOT: registered native closure '%s'\n", func_name);
            }
        } else if (ctx) {
            // Check if this is an init function (for constants/static vars)
            bool is_init_func = isAOTInitFunctionName(func_name);
            if (is_init_func && init_func_contexts) {
                AOTInitFuncExecInfo info;
                info.ctx = ctx;
                info.fn_ptr = aot_func->fn_ptr;
                info.name = func_name;
                init_func_contexts->push_back(std::move(info));
                ++registered;
                func_map.erase(it);
                printd(2, "AOT slot-reg: collected init function '%s' for execution\n", func_name);
            } else if (is_init_func) {
                // Init function but no init_func_contexts — deferred to ns_init.
                // Count as registered so the warning doesn't fire for these.
                delete ctx;
                ++registered;
                func_map.erase(it);
                printd(2, "AOT slot-reg: init function '%s' deferred to ns_init\n", func_name);
            } else {
                // Toplevel or unresolved — handled separately
                delete ctx;
                printd(2, "AOT slot-reg: context built for '%s' but no variant (uvb=%p)\n",
                    func_name, (void*)uvb);
                if (trace_slot_reg) {
                    fprintf(stderr, "[aot-slot-reg] context built but no variant func='%s'\n", func_name);
                }
            }
        } else {
            // buildContextFromSlotMap failed or returned null — ensure ptr is at entry boundary
            printd(2, "AOT slot-reg: SKIP '%s' (context build failed) uvb=%p\n",
                func_name, (void*)uvb);
            if (registration_errors) {
                if (build_error.empty()) {
                    build_error = "AOT slot map registration failed for '";
                    build_error += func_name;
                    build_error += "': context build failed";
                }
                registration_errors->push_back(std::move(build_error));
                ptr = entry_end;
                return;
            }
        }

        // Always advance ptr to end of entry (self-healing with entry-size prefix)
        ptr = entry_end;
    }
}

//! Walk a source-parsed namespace tree and register pre-compiled AOT function pointers with context
/** Matches function names from the AOT function table against user function variants
    in the program's namespace tree. For each match, re-lowers to IR to build a
    QoreAOTContext, then registers via registerPrecompiledAOTFunction().

    This is only valid for source-parsed AOT executable/module paths. Source-
    stripped metadata paths must register from SLOT_MAPS only so missing binary
    metadata is a hard error rather than a hidden source fallback.
*/
static void registerAOTFunctionsInNamespace(qore_ns_private* ns, QoreProgram* pgm,
        const std::unordered_map<std::string, const QoreAOTFunc*>& func_map,
        int& registered) {
    // Walk functions in this namespace
    for (auto i = ns->func_list.begin(), e = ns->func_list.end(); i != e; ++i) {
        FunctionEntry* fe = i->second;
        QoreFunction* func = fe->getFunction();
        if (!func) {
            continue;
        }

        // Check all variants
        QoreFunctionIterator vit(*func);
        while (vit.next()) {
            const AbstractQoreFunctionVariant* variant = vit.getVariant();
            UserVariantBase* uvb = const_cast<AbstractQoreFunctionVariant*>(variant)->getUserVariantBase();
            if (!uvb) {
                continue;
            }

            const char* fname = func->getName();
            std::string function_name = getAOTQualifiedFunctionName(ns, fname);
            // Generate unique key including parameter types to match compiled variant
            std::string variant_key = getVariantKey(function_name.c_str(), variant);
            auto it = func_map.find(variant_key);
            if (it != func_map.end()) {
                const QoreAOTFunc* aot_func = it->second;
                QoreAOTContext* ctx = buildContextForVariant(uvb, function_name.c_str(), pgm, *aot_func);
                if (ctx) {
                    uvb->registerPrecompiledAOTFunction(aot_func->fn_ptr, ctx);
                    ++registered;
                    printd(2, "AOT: registered pre-compiled function '%s' (locals=%d, globals=%d, exprs=%d)\n",
                        variant_key.c_str(), aot_func->num_locals, aot_func->num_globals, aot_func->num_exprs);
                } else {
                    printd(1, "AOT: failed to build context for function '%s'\n", variant_key.c_str());
                }
            }
        }
    }

    // Walk classes in this namespace
    ClassListIterator cli(ns->classList);
    while (cli.next()) {
        QoreClass* qc = cli.get();
        if (!qc) {
            continue;
        }
        qore_class_private* qcp = qore_class_private::get(*qc);

        // Use namespace-qualified class path to match compiler's naming convention
        // (QoreAOT.cpp uses qc->getPath() for variant keys)
        const char* class_path = qc->getPath();
        // Strip leading :: (getPath() returns "::Ns::ClassName")
        if (class_path[0] == ':' && class_path[1] == ':') {
            class_path += 2;
        }

        // Skip system classes - they can't be modified and their methods are already set up
        if (qcp->sys) {
            printd(2, "AOT: skipping system class '%s' in method registration\n", class_path);
            continue;
        }

        // Helper lambda for method registration
        auto processMethod = [&](QoreMethod* meth) {
            if (!meth->isUser()) {
                return;
            }
            qore_method_private* mp = qore_method_private::get(*meth);
            MethodFunctionBase* mfb = mp->getFunction();

            QoreFunctionIterator vit(*mfb);
            while (vit.next()) {
                const AbstractQoreFunctionVariant* variant = vit.getVariant();
                UserVariantBase* uvb = const_cast<AbstractQoreFunctionVariant*>(variant)->getUserVariantBase();
                if (!uvb) {
                    continue;
                }

                // Include static/instance marker so overloaded methods
                // (same name + signature, one static one instance) resolve to
                // their correct compiled variant — matches QoreAOT.cpp method
                // compilation key format.
                std::string method_name = std::string(class_path) + "::"
                    + (meth->isStatic() ? "_static_" : "")
                    + meth->getName();
                // Generate unique key including parameter types to match compiled variant
                std::string variant_key = getVariantKey(method_name.c_str(), variant);
                auto it = func_map.find(variant_key);
                if (it != func_map.end()) {
                    const QoreAOTFunc* aot_func = it->second;
                    QoreAOTContext* ctx = buildContextForVariant(uvb, method_name.c_str(), pgm, *aot_func);
                    if (ctx) {
                        uvb->registerPrecompiledAOTFunction(aot_func->fn_ptr, ctx);
                        ++registered;
                        printd(2, "AOT: registered pre-compiled method '%s' (locals=%d, globals=%d, exprs=%d)\n",
                            variant_key.c_str(), aot_func->num_locals, aot_func->num_globals, aot_func->num_exprs);
                    } else {
                        printd(1, "AOT: failed to build context for method '%s'\n", variant_key.c_str());
                    }
                }
            }
        };

        // Instance methods
        for (auto mi = qcp->hm.begin(), me = qcp->hm.end(); mi != me; ++mi) {
            processMethod(mi->second);
        }
        // Static methods
        for (auto mi = qcp->shm.begin(), me = qcp->shm.end(); mi != me; ++mi) {
            processMethod(mi->second);
        }
    }

    // Walk child namespaces recursively
    for (auto ni = ns->nsl.nsmap.begin(), ne = ns->nsl.nsmap.end(); ni != ne; ++ni) {
        QoreNamespace* child_ns = ni->second;
        if (child_ns) {
            registerAOTFunctionsInNamespace(qore_ns_private::get(*child_ns), pgm, func_map, registered);
        }
    }
}

//! Transplant BCAList (base class constructor arguments) from fallback to main constructors.
/** Constructors are registered from slot maps (for their native AOT fn_ptr), but the
    deserialized UserConstructorVariant has bcal=nullptr because the BCAList is not
    serialized in the binary. Without the BCAList, constructorPrelude() cannot pass
    arguments to base class constructors, causing RUNTIME-OVERLOAD-ERROR.
    This function walks all classes in the main namespace and transplants the BCAList
    from the matching fallback constructor variant where bcal is missing.
*/
static void transplantConstructorBCALists(
        qore_ns_private* fallback_ns,
        qore_ns_private* main_ns) {
    // Process classes in this namespace
    ClassListIterator cli(main_ns->classList);
    while (cli.next()) {
        QoreClass* main_qc = cli.get();
        if (!main_qc) {
            continue;
        }
        // Find matching class in fallback namespace
        QoreClass* fb_qc = fallback_ns->classList.find(main_qc->getName());
        if (!fb_qc) {
            continue;
        }
        qore_class_private* main_priv = qore_class_private::get(*main_qc);
        qore_class_private* fb_priv = qore_class_private::get(*fb_qc);

        // Find the constructor method in the main class
        const QoreMethod* main_mc = main_priv->parseFindLocalMethod("constructor");
        const QoreMethod* fb_mc = fb_priv->parseFindLocalMethod("constructor");
        if (!main_mc || !fb_mc) {
            continue;
        }
        MethodFunctionBase* main_mfb = qore_method_private::get(*main_mc)->getFunction();
        MethodFunctionBase* fb_mfb = qore_method_private::get(*fb_mc)->getFunction();
        if (!main_mfb || !fb_mfb) {
            continue;
        }

        // Find UserConstructorVariant in main and fallback
        UserConstructorVariant* main_ucv = nullptr;
        UserConstructorVariant* fb_ucv = nullptr;
        {
            QoreFunctionIterator vi(*main_mfb);
            while (vi.next()) {
                main_ucv = const_cast<UserConstructorVariant*>(
                    dynamic_cast<const UserConstructorVariant*>(vi.getVariant()));
                if (main_ucv) {
                    break;
                }
            }
        }
        {
            QoreFunctionIterator vi(*fb_mfb);
            while (vi.next()) {
                fb_ucv = const_cast<UserConstructorVariant*>(
                    dynamic_cast<const UserConstructorVariant*>(vi.getVariant()));
                if (fb_ucv) {
                    break;
                }
            }
        }
        if (main_ucv && fb_ucv) {
            main_ucv->transplantBCAList(fb_ucv);
            printd(5, "AOT: transplanted BCAList for %s::constructor\n", main_qc->getName());
        }
    }

    // Recurse into sub-namespaces
    for (auto ni = main_ns->nsl.nsmap.begin(), ne = main_ns->nsl.nsmap.end();
            ni != ne; ++ni) {
        QoreNamespace* main_child = ni->second;
        if (!main_child) {
            continue;
        }
        auto fb_ni = fallback_ns->nsl.nsmap.find(ni->first);
        if (fb_ni != fallback_ns->nsl.nsmap.end() && fb_ni->second) {
            transplantConstructorBCALists(
                qore_ns_private::get(*fb_ni->second),
                qore_ns_private::get(*main_child));
        }
    }
}

//! Resolve a fallback-program type pointer to the target AOT program's canonical type pointer.
using hashdecl_retarget_map_t = std::unordered_map<const TypedHashDecl*, const TypedHashDecl*>;

static const QoreTypeInfo* retargetFallbackTypeInfo(QoreAOTTypeResolver& type_resolver, const QoreTypeInfo* ti,
        const hashdecl_retarget_map_t& hashdecl_map) {
    if (!QoreTypeInfo::hasType(ti)) {
        return ti;
    }

    if (const TypedHashDecl* hd = QoreTypeInfo::getUniqueReturnHashDecl(ti)) {
        auto hdi = hashdecl_map.find(hd);
        if (hdi != hashdecl_map.end() && hdi->second) {
            bool or_nothing = QoreTypeInfo::parseReturns(ti, NT_NOTHING) != QTI_NOT_EQUAL;
            return hdi->second->getTypeInfo(or_nothing);
        }
    }

    if (QoreTypeInfo::isReference(ti)) {
        const QoreTypeInfo* target_ti = QoreTypeInfo::getReferenceTarget(ti);
        const QoreTypeInfo* new_target_ti = retargetFallbackTypeInfo(type_resolver, target_ti, hashdecl_map);
        if (new_target_ti != target_ti && QoreTypeInfo::hasType(new_target_ti)) {
            bool or_nothing = QoreTypeInfo::parseReturns(ti, NT_NOTHING) != QTI_NOT_EQUAL;
            return or_nothing
                ? qore_get_complex_reference_or_nothing_type(new_target_ti)
                : qore_get_complex_reference_type(new_target_ti);
        }
    }

    std::string path = qore_get_aot_serializable_type_path(ti);
    if (path.empty() || path == "no type info") {
        return ti;
    }

    std::string error;
    const QoreTypeInfo* resolved = type_resolver.resolve(path.c_str(), error);
    return resolved && error.empty() ? resolved : ti;
}

static void retargetFallbackLocalVarType(LocalVar* lv, QoreAOTTypeResolver& type_resolver,
        const hashdecl_retarget_map_t& hashdecl_map) {
    if (!lv) {
        return;
    }

    const QoreTypeInfo* ti = lv->getTypeInfo();
    const QoreTypeInfo* new_ti = retargetFallbackTypeInfo(type_resolver, ti, hashdecl_map);
    if (new_ti != ti) {
        lv->setTypeInfo(new_ti);
    }
}

static void retargetFallbackValueTypes(
    QoreValue v,
    QoreAOTTypeResolver& type_resolver,
    const hashdecl_retarget_map_t& hashdecl_map,
    std::unordered_set<const AbstractQoreNode*>& seen);

static void retargetFallbackClosureFunctionTypes(
    QoreFunction* func,
    QoreAOTTypeResolver& type_resolver,
    const hashdecl_retarget_map_t& hashdecl_map,
    std::unordered_set<const AbstractQoreNode*>* seen_exprs = nullptr);

static void retargetFallbackCallArgs(
        const FunctionCallBase& call,
        QoreAOTTypeResolver& type_resolver,
        const hashdecl_retarget_map_t& hashdecl_map,
        std::unordered_set<const AbstractQoreNode*>& seen) {
    if (const QoreParseListNode* parse_args = call.getParseArgs()) {
        for (const QoreValue& arg : parse_args->getValues()) {
            retargetFallbackValueTypes(arg, type_resolver, hashdecl_map, seen);
        }
    }

    if (const QoreListNode* args = call.getArgs()) {
        size_t len = args->size();
        for (size_t i = 0; i < len; ++i) {
            retargetFallbackValueTypes(args->retrieveEntry(i), type_resolver, hashdecl_map, seen);
        }
    }
}

template <typename T>
static bool retargetFallbackSingleExpressionOperator(
        const AbstractQoreNode* node,
        QoreAOTTypeResolver& type_resolver,
        const hashdecl_retarget_map_t& hashdecl_map,
        std::unordered_set<const AbstractQoreNode*>& seen) {
    const T* op = dynamic_cast<const T*>(node);
    if (!op) {
        return false;
    }

    retargetFallbackValueTypes(op->getExp(), type_resolver, hashdecl_map, seen);
    return true;
}

template <typename T>
static bool retargetFallbackBinaryOperator(
        const AbstractQoreNode* node,
        QoreAOTTypeResolver& type_resolver,
        const hashdecl_retarget_map_t& hashdecl_map,
        std::unordered_set<const AbstractQoreNode*>& seen) {
    const T* op = dynamic_cast<const T*>(node);
    if (!op) {
        return false;
    }

    retargetFallbackValueTypes(op->getLeft(), type_resolver, hashdecl_map, seen);
    retargetFallbackValueTypes(op->getRight(), type_resolver, hashdecl_map, seen);
    return true;
}

template <typename T, unsigned N>
static bool retargetFallbackNOperator(
        const AbstractQoreNode* node,
        QoreAOTTypeResolver& type_resolver,
        const hashdecl_retarget_map_t& hashdecl_map,
        std::unordered_set<const AbstractQoreNode*>& seen) {
    const T* op = dynamic_cast<const T*>(node);
    if (!op) {
        return false;
    }

    for (unsigned i = 0; i < N; ++i) {
        retargetFallbackValueTypes(op->get(i), type_resolver, hashdecl_map, seen);
    }
    return true;
}

static void retargetFallbackStatementBlockTypes(
        const StatementBlock* block,
        QoreAOTTypeResolver& type_resolver,
        const hashdecl_retarget_map_t& hashdecl_map,
        std::unordered_set<const AbstractQoreNode*>& seen);

static void retargetFallbackStatementTypes(
        const AbstractStatement* stmt,
        QoreAOTTypeResolver& type_resolver,
        const hashdecl_retarget_map_t& hashdecl_map,
        std::unordered_set<const AbstractQoreNode*>& seen) {
    if (!stmt) {
        return;
    }

    if (auto* block = dynamic_cast<const StatementBlock*>(stmt)) {
        retargetFallbackStatementBlockTypes(block, type_resolver, hashdecl_map, seen);
    } else if (auto* expr_stmt = dynamic_cast<const ExpressionStatement*>(stmt)) {
        retargetFallbackValueTypes(expr_stmt->getExpression(), type_resolver, hashdecl_map, seen);
    } else if (auto* return_stmt = dynamic_cast<const ReturnStatement*>(stmt)) {
        retargetFallbackValueTypes(return_stmt->getExpression(), type_resolver, hashdecl_map, seen);
    } else if (auto* throw_stmt = dynamic_cast<const ThrowStatement*>(stmt)) {
        retargetFallbackValueTypes(throw_stmt->getArgs(), type_resolver, hashdecl_map, seen);
    } else if (auto* rethrow_stmt = dynamic_cast<const RethrowStatement*>(stmt)) {
        retargetFallbackValueTypes(rethrow_stmt->getArgs(), type_resolver, hashdecl_map, seen);
    } else if (auto* if_stmt = dynamic_cast<const IfStatement*>(stmt)) {
        retargetFallbackValueTypes(if_stmt->getCond(), type_resolver, hashdecl_map, seen);
        retargetFallbackStatementBlockTypes(if_stmt->getIfCode(), type_resolver, hashdecl_map, seen);
        retargetFallbackStatementBlockTypes(if_stmt->getElseCode(), type_resolver, hashdecl_map, seen);
    } else if (auto* for_stmt = dynamic_cast<const ForStatement*>(stmt)) {
        retargetFallbackValueTypes(for_stmt->getAssignment(), type_resolver, hashdecl_map, seen);
        retargetFallbackValueTypes(for_stmt->getCond(), type_resolver, hashdecl_map, seen);
        retargetFallbackValueTypes(for_stmt->getIterator(), type_resolver, hashdecl_map, seen);
        retargetFallbackStatementBlockTypes(for_stmt->getCode(), type_resolver, hashdecl_map, seen);
    } else if (auto* while_stmt = dynamic_cast<const WhileStatement*>(stmt)) {
        retargetFallbackValueTypes(while_stmt->getCond(), type_resolver, hashdecl_map, seen);
        retargetFallbackStatementBlockTypes(while_stmt->getCode(), type_resolver, hashdecl_map, seen);
    } else if (auto* try_stmt = dynamic_cast<const TryStatement*>(stmt)) {
        retargetFallbackLocalVarType(try_stmt->getCatchVar(), type_resolver, hashdecl_map);
        retargetFallbackStatementBlockTypes(try_stmt->getTryBlock(), type_resolver, hashdecl_map, seen);
        retargetFallbackStatementBlockTypes(try_stmt->getCatchBlock(), type_resolver, hashdecl_map, seen);
    } else if (auto* sw_stmt = dynamic_cast<const SwitchStatement*>(stmt)) {
        retargetFallbackValueTypes(sw_stmt->getSwitchExp(), type_resolver, hashdecl_map, seen);
        const CaseNode* cn = sw_stmt->getCases();
        while (cn) {
            retargetFallbackValueTypes(cn->val, type_resolver, hashdecl_map, seen);
            retargetFallbackStatementBlockTypes(cn->code, type_resolver, hashdecl_map, seen);
            cn = cn->next;
        }
    } else if (auto* foreach_stmt = dynamic_cast<const ForEachStatement*>(stmt)) {
        retargetFallbackValueTypes(foreach_stmt->getVar(), type_resolver, hashdecl_map, seen);
        retargetFallbackValueTypes(foreach_stmt->getList(), type_resolver, hashdecl_map, seen);
        retargetFallbackStatementBlockTypes(foreach_stmt->getCode(), type_resolver, hashdecl_map, seen);
    } else if (auto* debug_stmt = dynamic_cast<const DebugStatement*>(stmt)) {
        retargetFallbackValueTypes(debug_stmt->getExpression(), type_resolver, hashdecl_map, seen);
        retargetFallbackStatementBlockTypes(debug_stmt->getBlock(), type_resolver, hashdecl_map, seen);
    } else if (auto* assert_stmt = dynamic_cast<const AssertStatement*>(stmt)) {
        retargetFallbackValueTypes(assert_stmt->getCondition(), type_resolver, hashdecl_map, seen);
        retargetFallbackValueTypes(assert_stmt->getMessage(), type_resolver, hashdecl_map, seen);
    } else if (auto* obe_stmt = dynamic_cast<const OnBlockExitStatement*>(stmt)) {
        retargetFallbackStatementBlockTypes(obe_stmt->getCode(), type_resolver, hashdecl_map, seen);
    } else if (auto* ctx_stmt = dynamic_cast<const ContextStatement*>(stmt)) {
        retargetFallbackValueTypes(ctx_stmt->exp, type_resolver, hashdecl_map, seen);
        retargetFallbackValueTypes(ctx_stmt->where_exp, type_resolver, hashdecl_map, seen);
        retargetFallbackValueTypes(ctx_stmt->sort_ascending, type_resolver, hashdecl_map, seen);
        retargetFallbackValueTypes(ctx_stmt->sort_descending, type_resolver, hashdecl_map, seen);
        retargetFallbackStatementBlockTypes(ctx_stmt->code, type_resolver, hashdecl_map, seen);
    }
}

static void retargetFallbackStatementBlockTypes(
        const StatementBlock* block,
        QoreAOTTypeResolver& type_resolver,
        const hashdecl_retarget_map_t& hashdecl_map,
        std::unordered_set<const AbstractQoreNode*>& seen) {
    if (!block) {
        return;
    }

    for (auto it = block->getStatements().begin(); it != block->getStatements().end(); ++it) {
        retargetFallbackStatementTypes(*it, type_resolver, hashdecl_map, seen);
    }
}

static void retargetFallbackClosureFunctionTypes(QoreFunction* func, QoreAOTTypeResolver& type_resolver,
        const hashdecl_retarget_map_t& hashdecl_map,
        std::unordered_set<const AbstractQoreNode*>* seen_exprs) {
    if (!func) {
        return;
    }

    UserClosureFunction* ucf = dynamic_cast<UserClosureFunction*>(func);

    if (ucf) {
        const QoreTypeInfo* class_ti = ucf->getClassType();
        const QoreTypeInfo* new_class_ti = retargetFallbackTypeInfo(type_resolver, class_ti, hashdecl_map);
        if (new_class_ti != class_ti) {
            ucf->setClassType(new_class_ti);
        }
    }

    std::unordered_set<const AbstractQoreNode*> local_seen;
    std::unordered_set<const AbstractQoreNode*>& seen = seen_exprs ? *seen_exprs : local_seen;

    QoreFunctionIterator vi(*func);
    while (vi.next()) {
        const UserVariantBase* const_uvb = vi.getVariant()->getUserVariantBase();
        UserVariantBase* uvb = const_cast<UserVariantBase*>(const_uvb);
        if (!uvb) {
            continue;
        }

        UserSignature* sig = uvb->getUserSignature();
        if (!sig) {
            continue;
        }

        const type_vec_t& types = sig->getTypeList();
        std::vector<const QoreTypeInfo*> param_types;
        param_types.reserve(types.size());
        bool changed = false;
        for (const QoreTypeInfo* ti : types) {
            const QoreTypeInfo* new_ti = retargetFallbackTypeInfo(type_resolver, ti, hashdecl_map);
            changed |= new_ti != ti;
            param_types.push_back(new_ti);
        }

        const QoreTypeInfo* ret_ti = sig->getReturnTypeInfo();
        const QoreTypeInfo* new_ret_ti = retargetFallbackTypeInfo(type_resolver, ret_ti, hashdecl_map);
        if (changed || new_ret_ti != ret_ti) {
            sig->replaceResolvedTypes(new_ret_ti, std::move(param_types));
        }

        for (LocalVar* lv : sig->lv) {
            retargetFallbackLocalVarType(lv, type_resolver, hashdecl_map);
        }
        retargetFallbackLocalVarType(sig->argvid, type_resolver, hashdecl_map);
        retargetFallbackLocalVarType(sig->selfid, type_resolver, hashdecl_map);

        if (uvb && uvb->getStatementBlock()) {
            std::vector<LocalVar*> body_locals;
            collectAllStatementLocals(uvb->getStatementBlock(), body_locals);
            for (LocalVar* lv : body_locals) {
                retargetFallbackLocalVarType(lv, type_resolver, hashdecl_map);
            }
        }
        if (uvb && uvb->getStatementBlock()) {
            retargetFallbackStatementBlockTypes(uvb->getStatementBlock(), type_resolver, hashdecl_map, seen);
        }

        if (uvb && uvb->hasCachedAOT()) {
            const std::vector<LocalVar*>& body_locals = uvb->getBodyLocals();
            for (LocalVar* lv : body_locals) {
                retargetFallbackLocalVarType(lv, type_resolver, hashdecl_map);
            }
        }
    }

    if (ucf) {
        LVarSet* vlist = ucf->getVList();
        if (vlist) {
            for (LocalVar* lv : *vlist) {
                retargetFallbackLocalVarType(lv, type_resolver, hashdecl_map);
            }
        }
    }
}

static void retargetFallbackValueTypes(
        QoreValue v,
        QoreAOTTypeResolver& type_resolver,
        const hashdecl_retarget_map_t& hashdecl_map,
        std::unordered_set<const AbstractQoreNode*>& seen) {
    const AbstractQoreNode* node = v.getInternalNode();
    if (!node || !seen.insert(node).second) {
        return;
    }

    switch (v.getType()) {
        case NT_PARSEREFERENCE: {
            const ParseReferenceNode* prn = v.get<const ParseReferenceNode>();
            const QoreTypeInfo* ti = prn->getTypeInfo();
            const QoreTypeInfo* new_ti = retargetFallbackTypeInfo(type_resolver, ti, hashdecl_map);
            if (new_ti != ti) {
                const_cast<ParseReferenceNode*>(prn)->setTypeInfo(new_ti);
            }
            retargetFallbackValueTypes(prn->getLVExp(), type_resolver, hashdecl_map, seen);
            break;
        }

        case NT_PARSE_LIST: {
            const QoreParseListNode* l = v.get<const QoreParseListNode>();
            for (const QoreValue& entry : l->getValues()) {
                retargetFallbackValueTypes(entry, type_resolver, hashdecl_map, seen);
            }
            break;
        }

        case NT_PARSE_HASH: {
            const QoreParseHashNode* h = v.get<const QoreParseHashNode>();
            for (const QoreValue& key : h->getKeys()) {
                retargetFallbackValueTypes(key, type_resolver, hashdecl_map, seen);
            }
            for (const QoreValue& value : h->getValues()) {
                retargetFallbackValueTypes(value, type_resolver, hashdecl_map, seen);
            }
            break;
        }

        case NT_HASH: {
            const QoreHashNode* h = v.get<const QoreHashNode>();
            ConstHashIterator hi(h);
            while (hi.next()) {
                retargetFallbackValueTypes(hi.get(), type_resolver, hashdecl_map, seen);
            }
            break;
        }

        case NT_LIST: {
            const QoreListNode* l = v.get<const QoreListNode>();
            size_t len = l->size();
            for (size_t i = 0; i < len; ++i) {
                retargetFallbackValueTypes(l->retrieveEntry(i), type_resolver, hashdecl_map, seen);
            }
            break;
        }

        case NT_RUNTIME_CLOSURE: {
            const QoreClosureBase* cb = dynamic_cast<const QoreClosureBase*>(node);
            if (cb) {
                retargetFallbackClosureFunctionTypes(cb->getFunction(), type_resolver, hashdecl_map, &seen);
            }
            break;
        }

        case NT_CLOSURE: {
            const QoreClosureParseNode* cn = dynamic_cast<const QoreClosureParseNode*>(node);
            if (cn) {
                retargetFallbackClosureFunctionTypes(cn->getFunction(), type_resolver, hashdecl_map, &seen);
            }
            break;
        }

        case NT_FUNCTION_CALL:
        case NT_PROGRAM_FUNC_CALL:
        case NT_SELF_CALL:
        case NT_METHOD_CALL:
        case NT_STATIC_METHOD_CALL:
        case NT_SCOPE_REF: {
            const AbstractFunctionCallNode* call = dynamic_cast<const AbstractFunctionCallNode*>(node);
            if (call) {
                retargetFallbackCallArgs(*call, type_resolver, hashdecl_map, seen);
            }
            break;
        }

        case NT_NEW_OBJECT: {
            const NewObjectCallNode* call = dynamic_cast<const NewObjectCallNode*>(node);
            if (call) {
                retargetFallbackCallArgs(*call, type_resolver, hashdecl_map, seen);
            }
            break;
        }

        case NT_FUNCREFCALL: {
            const CallReferenceCallNode* call = dynamic_cast<const CallReferenceCallNode*>(node);
            if (call) {
                retargetFallbackValueTypes(call->getExp(), type_resolver, hashdecl_map, seen);
                if (const QoreParseListNode* args = call->getParseArgs()) {
                    for (const QoreValue& arg : args->getValues()) {
                        retargetFallbackValueTypes(arg, type_resolver, hashdecl_map, seen);
                    }
                }
                if (const QoreListNode* args = call->getArgs()) {
                    size_t len = args->size();
                    for (size_t i = 0; i < len; ++i) {
                        retargetFallbackValueTypes(args->retrieveEntry(i), type_resolver, hashdecl_map, seen);
                    }
                }
            }
            break;
        }

        case NT_OPERATOR: {
            if (auto* dot = dynamic_cast<const QoreDotEvalOperatorNode*>(node)) {
                retargetFallbackValueTypes(dot->getExpression(), type_resolver, hashdecl_map, seen);
                if (MethodCallNode* m = dot->getMethodCall()) {
                    retargetFallbackCallArgs(*m, type_resolver, hashdecl_map, seen);
                }
                break;
            }
            if (retargetFallbackBinaryOperator<QoreBinaryOperatorNode<>>(node, type_resolver, hashdecl_map, seen)
                    || retargetFallbackBinaryOperator<QoreBinaryOperatorNode<LValueOperatorNode>>(
                        node, type_resolver, hashdecl_map, seen)
                    || retargetFallbackSingleExpressionOperator<QoreSingleExpressionOperatorNode<>>(
                        node, type_resolver, hashdecl_map, seen)
                    || retargetFallbackSingleExpressionOperator<QoreSingleExpressionOperatorNode<LValueOperatorNode>>(
                        node, type_resolver, hashdecl_map, seen)
                    || retargetFallbackSingleExpressionOperator<QoreSingleValueExpressionOperatorNode<>>(
                        node, type_resolver, hashdecl_map, seen)
                    || retargetFallbackSingleExpressionOperator<QoreSingleValueExpressionOperatorNode<LValueOperatorNode>>(
                        node, type_resolver, hashdecl_map, seen)
                    || retargetFallbackNOperator<QoreNOperatorNodeBase<3>, 3>(
                        node, type_resolver, hashdecl_map, seen)
                    || retargetFallbackNOperator<QoreNOperatorNodeBase<4>, 4>(
                        node, type_resolver, hashdecl_map, seen)) {
                break;
            }
            break;
        }

        default:
            break;
    }
}

static void retargetFallbackValueTypes(QoreValue v, QoreAOTTypeResolver& type_resolver,
        const hashdecl_retarget_map_t& hashdecl_map) {
    std::unordered_set<const AbstractQoreNode*> seen;
    retargetFallbackValueTypes(v, type_resolver, hashdecl_map, seen);
}

static void collectFallbackHashDeclMappings(
        qore_ns_private* fallback_ns,
        qore_ns_private* main_ns,
        hashdecl_retarget_map_t& hashdecl_map) {
    ConstHashDeclListIterator hdi(fallback_ns->hashDeclList);
    while (hdi.next()) {
        const TypedHashDecl* main_hd = main_ns->hashDeclList.find(hdi.getName());
        if (main_hd) {
            hashdecl_map[hdi.get()] = main_hd;
        }
    }

    for (auto ni = main_ns->nsl.nsmap.begin(), ne = main_ns->nsl.nsmap.end(); ni != ne; ++ni) {
        QoreNamespace* main_child = ni->second;
        auto fb_ni = fallback_ns->nsl.nsmap.find(ni->first);
        if (fb_ni != fallback_ns->nsl.nsmap.end() && fb_ni->second) {
            collectFallbackHashDeclMappings(
                qore_ns_private::get(*fb_ni->second),
                qore_ns_private::get(*main_child),
                hashdecl_map);
        }
    }
}

//! Transplant unserialized closure values from fallback-parsed classes to main classes.
/** During AOT serialization, closure/code values are written as VT_NOTHING because
    they can't be serialized. This affects both class constants (ConstantEntry::val)
    and member default value expressions (QoreMemberInfo::exp). After fallback source
    parsing, the fallback program has properly initialized values. This function copies
    those to the main program's classes so they are available at runtime.
*/
static void transplantClassClosureValues(
        qore_ns_private* fallback_ns,
        qore_ns_private* main_ns,
        QoreProgram* main_pgm,
        QoreAOTTypeResolver* shared_type_resolver = nullptr,
        hashdecl_retarget_map_t* shared_hashdecl_map = nullptr) {
    QoreAOTTypeResolver local_type_resolver(main_pgm);
    QoreAOTTypeResolver& type_resolver = shared_type_resolver ? *shared_type_resolver : local_type_resolver;
    hashdecl_retarget_map_t local_hashdecl_map;
    if (!shared_hashdecl_map) {
        collectFallbackHashDeclMappings(fallback_ns, main_ns, local_hashdecl_map);
    }
    hashdecl_retarget_map_t& hashdecl_map = shared_hashdecl_map ? *shared_hashdecl_map : local_hashdecl_map;

    // Process classes in this namespace
    ClassListIterator cli(main_ns->classList);
    while (cli.next()) {
        QoreClass* main_qc = cli.get();
        if (!main_qc) {
            continue;
        }
        qore_class_private* main_priv = qore_class_private::get(*main_qc);

        // Find matching class in fallback namespace
        QoreClass* fb_qc = fallback_ns->classList.find(main_qc->getName());
        if (!fb_qc) {
            continue;
        }
        qore_class_private* fb_priv = qore_class_private::get(*fb_qc);

        // Iterate through main class constants looking for NOTHING values
        ConstConstantListIterator main_ci(main_priv->constlist);
        while (main_ci.next()) {
            const ConstantEntry* main_ce = main_ci.getEntry();
            if (!main_ce->isUser() || !main_ce->getValue().isNothing()) {
                continue;  // skip system constants and non-NOTHING values
            }

            // Find corresponding constant in fallback class
            const ConstantEntry* fb_ce = fb_priv->constlist.findEntry(main_ce->getName());
            if (!fb_ce || fb_ce->getValue().isNothing()) {
                continue;  // no fallback value available
            }

            // Transplant: set the main constant's value from the fallback constant.
            // The fallback program lives until after the main program finishes running,
            // so the referenced values (closures, etc.) remain valid.
            //
            // Use setRuntimeValue() to properly populate both val AND saved_val
            // and clear aot_shell_pending — otherwise any RuntimeConstantRefNode
            // still pointing at this entry (from a sibling constant's expression
            // tree compiled into an AOT-deserialized init function) would read
            // saved_val (still NOTHING) and raise AOT-PENDING-CONSTANT even
            // though the visible val holds the correct transplanted value.
            ConstantEntry* writable_ce = const_cast<ConstantEntry*>(main_ce);
            ExceptionSink txs;
            QoreValue fb_val = fb_ce->getReferencedValue();
            retargetFallbackValueTypes(fb_val, type_resolver, hashdecl_map);
            writable_ce->setRuntimeValue(fb_val, &txs);
            if (txs.isException()) {
                txs.clear();
            }
            writable_ce->init = true;
            printd(5, "AOT: transplanted constant '%s::%s' from fallback\n",
                main_qc->getName(), main_ce->getName());
        }

        // Transplant member default value expressions (closures can't be serialized).
        // The member `exp` field stores the initialization expression evaluated at
        // object construction time. For closures, writeValue() serializes them as
        // VT_NOTHING. Copy the fallback's expression so member initialization works.
        for (auto& mi : main_priv->members.member_list) {
            if (!mi.second->local()) {
                continue;  // only transplant locally-defined members
            }
            if (!mi.second->exp.isNothing()) {
                continue;  // already has a valid default expression
            }

            // Find corresponding member in fallback class
            QoreMemberInfo* fb_mi = fb_priv->members.find(mi.first);
            if (!fb_mi || fb_mi->exp.isNothing()) {
                continue;  // no fallback expression available
            }

            // Transplant: copy the expression reference from the fallback member.
            // The expression is a parse tree node owned by the fallback program,
            // which outlives the main program, so the pointer remains valid.
            // We use refSelf() to get a referenced copy of the expression value.
            retargetFallbackValueTypes(fb_mi->exp, type_resolver, hashdecl_map);
            mi.second->exp = fb_mi->exp.refSelf();
            printd(5, "AOT: transplanted member default '%s::%s' from fallback\n",
                main_qc->getName(), mi.first);
        }

        // Transplant static variable values from the fallback program.
        // The fallback program's parseCommitRuntimeInit() has already evaluated
        // the init expressions (e.g., "Class::forName('NullDataProvider')"), so
        // we copy the evaluated values directly rather than the expressions
        // (which would fail to evaluate in the main program's context).
        for (auto& vi : main_priv->vars.member_list) {
            if (vi.second->eval_init) {
                continue;  // already initialized
            }

            // Find corresponding static var in fallback class
            QoreVarInfo* fb_vi = fb_priv->vars.find(vi.first);
            if (!fb_vi || !fb_vi->eval_init) {
                continue;  // fallback var not evaluated yet
            }

            // Read the evaluated value from the fallback static var
            QoreValue fb_val = fb_vi->val.getValue();
            if (fb_val.isNothing()) {
                // Even if the value is NOTHING, mark as initialized to prevent
                // re-evaluation of a missing init expression
                vi.second->eval_init = true;
                continue;
            }

            // Assign the evaluated value to the main static var
            QoreValue fb_ref = fb_val.refSelf();
            retargetFallbackValueTypes(fb_ref, type_resolver, hashdecl_map);
            discard(vi.second->assignInit(fb_ref), nullptr);
            vi.second->eval_init = true;
            printd(5, "AOT: transplanted static var value '%s::%s' from fallback\n",
                main_qc->getName(), vi.first);
        }
    }

    // Transplant namespace-level constants with NOTHING values (e.g., object constants
    // like "public const EpochIntToDateType = new EpochIntToDateType()" that can't be
    // serialized in the binary format because writeValue() doesn't support NT_OBJECT).
    ConstConstantListIterator ns_ci(main_ns->constant);
    while (ns_ci.next()) {
        const ConstantEntry* main_ce = ns_ci.getEntry();
        if (!main_ce->isUser() || !main_ce->getValue().isNothing()) {
            continue;  // skip system constants and non-NOTHING values
        }

        // Find corresponding constant in fallback namespace
        ConstantEntry* fb_ce = fallback_ns->constant.findEntry(main_ce->getName());
        if (!fb_ce || fb_ce->getValue().isNothing()) {
            continue;  // no fallback value available
        }

        // Transplant through setRuntimeValue() so pending AOT shells have both
        // val and saved_val populated and aot_shell_pending cleared.
        ConstantEntry* writable_ce = const_cast<ConstantEntry*>(main_ce);
        ExceptionSink txs;
        QoreValue fb_val = fb_ce->getReferencedValue();
        retargetFallbackValueTypes(fb_val, type_resolver, hashdecl_map);
        writable_ce->setRuntimeValue(fb_val, &txs);
        if (txs.isException()) {
            txs.clear();
        }
        printd(5, "AOT: transplanted namespace constant '%s::%s' from fallback\n",
            main_ns->name.c_str(), main_ce->getName());
    }

    // Recurse into sub-namespaces
    for (auto ni = main_ns->nsl.nsmap.begin(), ne = main_ns->nsl.nsmap.end();
            ni != ne; ++ni) {
        QoreNamespace* main_child = ni->second;
        if (!main_child) {
            continue;
        }
        auto fb_ni = fallback_ns->nsl.nsmap.find(ni->first);
        if (fb_ni != fallback_ns->nsl.nsmap.end() && fb_ni->second) {
            transplantClassClosureValues(
                qore_ns_private::get(*fb_ni->second),
                qore_ns_private::get(*main_child), main_pgm, &type_resolver, &hashdecl_map);
        }
    }
}

//! Build contexts from fallback AST variants but register on main program's variants.
/** Used in V2 mode when slot map registration fails (e.g., functions with stmt_slots
    that can't be resolved without full AST). Walks the fallback namespace to find
    variants with bodies, builds contexts via IR re-lowering, then registers the
    compiled function pointer + context on the main program's matching variant.
*/
static void registerFallbackFunctionsOnMainVariants(
        qore_ns_private* fallback_ns,
        qore_ns_private* main_ns,
        QoreProgram* main_pgm,
        const std::unordered_map<std::string, const QoreAOTFunc*>& func_map,
        int& registered) {
    // Helper lambda: find UserVariantBase in a namespace's function by name
    auto findMainVariant = [&](const char* fname) -> UserVariantBase* {
        for (auto fi = main_ns->func_list.begin(), fe2 = main_ns->func_list.end();
                fi != fe2; ++fi) {
            FunctionEntry* fe_entry = fi->second;
            QoreFunction* func = fe_entry->getFunction();
            if (func && strcmp(fname, func->getName()) == 0) {
                QoreFunctionIterator vi(*func);
                while (vi.next()) {
                    UserVariantBase* uvb = const_cast<UserVariantBase*>(
                        dynamic_cast<const UserVariantBase*>(vi.getVariant()));
                    if (uvb) {
                        return uvb;
                    }
                }
            }
        }
        return nullptr;
    };
    QoreAOTTypeResolver type_resolver(main_pgm);

    // Walk functions in fallback namespace
    for (auto i = fallback_ns->func_list.begin(), e = fallback_ns->func_list.end(); i != e; ++i) {
        FunctionEntry* fe = i->second;
        QoreFunction* func = fe->getFunction();
        if (!func) {
            continue;
        }

        QoreFunctionIterator vit(*func);
        while (vit.next()) {
            const AbstractQoreFunctionVariant* variant = vit.getVariant();
            UserVariantBase* fb_uvb = const_cast<AbstractQoreFunctionVariant*>(variant)->getUserVariantBase();
            if (!fb_uvb || !fb_uvb->hasBody()) {
                continue;
            }

            const char* fname = func->getName();
            std::string function_name = getAOTQualifiedFunctionName(fallback_ns, fname);
            std::string variant_key = getVariantKey(function_name.c_str(), variant);
            auto it = func_map.find(variant_key);
            if (it == func_map.end()) {
                continue;
            }

            const QoreAOTFunc* aot_func = it->second;
            // Build context from fallback variant's AST (gives consistent LocalVar*)
            QoreAOTContext* ctx = buildContextForVariant(fb_uvb, function_name.c_str(), main_pgm, *aot_func);
            if (!ctx) {
                printd(1, "AOT v2: failed to build fallback context for '%s'\n", variant_key.c_str());
                continue;
            }

            // Find matching variant in MAIN program and register there
            UserVariantBase* main_uvb = findMainVariant(fname);
            if (main_uvb) {
                // Swap main variant's parameter LocalVar* with fallback's so the calling
                // convention instantiates fallback LocalVar* on the thread-local stack.
                // This ensures the compiled AOT code AND on_exit/on_success/on_error handler
                // AST bodies (which reference fallback LocalVar*) both find the same variables.
                UserSignature* main_sig = main_uvb->getUserSignature();
                UserSignature* fb_sig = fb_uvb->getUserSignature();
                if (main_sig && fb_sig && main_sig->lv.size() == fb_sig->lv.size()) {
                    for (size_t pi = 0; pi < main_sig->lv.size(); ++pi) {
                        main_sig->lv[pi] = fb_sig->lv[pi];
                    }
                    main_sig->argvid = fb_sig->argvid;
                    main_sig->selfid = fb_sig->selfid;
                }
                main_uvb->registerPrecompiledAOTFunction(aot_func->fn_ptr, ctx);
                ++registered;
                printd(2, "AOT v2: registered '%s' from fallback (on main variant)\n",
                    variant_key.c_str());
            } else {
                // No main variant found — register on fallback variant
                // This happens when the function was defined only in fallback source
                fb_uvb->registerPrecompiledAOTFunction(aot_func->fn_ptr, ctx);
                ++registered;
                printd(2, "AOT v2: registered '%s' from fallback (on fallback variant)\n",
                    variant_key.c_str());
            }
        }
    }

    // Walk classes in fallback namespace
    ClassListIterator cli(fallback_ns->classList);
    while (cli.next()) {
        QoreClass* qc = cli.get();
        if (!qc) {
            continue;
        }
        qore_class_private* qcp = qore_class_private::get(*qc);

        // Use namespace-qualified class path to match compiler's naming convention
        const char* class_path = qc->getPath();
        if (class_path[0] == ':' && class_path[1] == ':') {
            class_path += 2;
        }

        if (qcp->sys) {
            continue;
        }

        auto processMethod = [&](QoreMethod* meth) {
            if (!meth->isUser()) {
                return;
            }
            qore_method_private* mp = qore_method_private::get(*meth);
            MethodFunctionBase* mfb = mp->getFunction();

            QoreFunctionIterator vit(*mfb);
            while (vit.next()) {
                const AbstractQoreFunctionVariant* variant = vit.getVariant();
                UserVariantBase* fb_uvb =
                    const_cast<AbstractQoreFunctionVariant*>(variant)->getUserVariantBase();
                if (!fb_uvb || !fb_uvb->hasBody()) {
                    continue;
                }

                std::string method_name = std::string(class_path) + "::"
                    + (meth->isStatic() ? "_static_" : "")
                    + meth->getName();
                std::string variant_key = getVariantKey(method_name.c_str(), variant);
                auto it = func_map.find(variant_key);
                if (it == func_map.end()) {
                    continue;
                }

                const QoreAOTFunc* aot_func = it->second;
                QoreAOTContext* ctx = buildContextForVariant(fb_uvb, method_name.c_str(),
                    main_pgm, *aot_func);
                if (!ctx) {
                    printd(1, "AOT v2: failed to build fallback context for '%s'\n",
                        variant_key.c_str());
                    continue;
                }

                // Find matching method variant in MAIN program
                qore_program_private* main_pp = qore_program_private::get(*main_pgm);
                const QoreClass* main_qc = qore_root_ns_private::runtimeFindClass(
                    *main_pp->RootNS, class_path);
                UserVariantBase* main_uvb = nullptr;
                if (main_qc) {
                    qore_class_private* main_qcp = qore_class_private::get(
                        *const_cast<QoreClass*>(main_qc));
                    // Look up on the same side (static/instance) as the
                    // variant being registered — a class with both an instance
                    // and static method of the same name would otherwise
                    // always resolve to the instance one.
                    const QoreMethod* main_m = meth->isStatic()
                        ? main_qcp->parseFindLocalStaticMethod(meth->getName())
                        : main_qcp->parseFindLocalMethod(meth->getName());
                    if (main_m) {
                        // Extract target signature from variant_key to match the
                        // correct overloaded variant in the main program
                        std::string target_sig;
                        {
                            size_t p = variant_key.find('(');
                            if (p != std::string::npos) {
                                target_sig = normalizeTypePaths(variant_key.substr(p));
                            } else {
                                target_sig = "()";
                            }
                        }
                        MethodFunctionBase* main_mfb =
                            qore_method_private::get(*main_m)->getFunction();
                        QoreFunctionIterator mvi(*main_mfb);
                        while (mvi.next()) {
                            const AbstractQoreFunctionVariant* mv = mvi.getVariant();
                            auto* candidate = const_cast<UserVariantBase*>(
                                dynamic_cast<const UserVariantBase*>(mv));
                            if (!candidate) {
                                continue;
                            }
                            // Build signature for this variant
                            std::string var_sig("(");
                            AbstractFunctionSignature* msig = mv->getSignature();
                            if (msig) {
                                const type_vec_t& types = msig->getTypeList();
                                for (size_t ti = 0; ti < types.size(); ++ti) {
                                    if (ti > 0) {
                                        var_sig.append(",");
                                    }
                                    var_sig.append(qore_get_aot_serializable_type_path(types[ti]));
                                }
                            }
                            var_sig.append(")");
                            if (aotVariantSignatureMatches(mv, target_sig, &type_resolver, class_path)) {
                                main_uvb = candidate;
                                break;
                            }
                        }
                    }
                }

                if (main_uvb) {
                    // Swap parameter LocalVar* (same pattern as functions above)
                    UserSignature* main_sig = main_uvb->getUserSignature();
                    UserSignature* fb_sig = fb_uvb->getUserSignature();
                    if (main_sig && fb_sig && main_sig->lv.size() == fb_sig->lv.size()) {
                        for (size_t pi = 0; pi < main_sig->lv.size(); ++pi) {
                            main_sig->lv[pi] = fb_sig->lv[pi];
                        }
                        main_sig->argvid = fb_sig->argvid;
                        main_sig->selfid = fb_sig->selfid;
                    }
                    // For constructors: transplant BCAList from fallback variant
                    // The main variant was deserialized without BCA (base class constructor
                    // arguments). The fallback source parse creates proper BCAList.
                    // constructorPrelude() needs the BCAList to call parent constructors.
                    if (strcmp(meth->getName(), "constructor") == 0 && main_qc) {
                        // Transplant BCAList: find UserConstructorVariant in both
                        // main and fallback constructor methods.
                        UserConstructorVariant* main_ucv = nullptr;
                        UserConstructorVariant* fb_ucv = nullptr;
                        qore_class_private* mqcp = qore_class_private::get(
                            *const_cast<QoreClass*>(main_qc));
                        const QoreMethod* mc = mqcp->parseFindLocalMethod("constructor");
                        if (mc) {
                            MethodFunctionBase* mmfb =
                                qore_method_private::get(*mc)->getFunction();
                            QoreFunctionIterator mvi(*mmfb);
                            while (mvi.next()) {
                                main_ucv = const_cast<UserConstructorVariant*>(
                                    dynamic_cast<const UserConstructorVariant*>(
                                        mvi.getVariant()));
                                if (main_ucv) {
                                    break;
                                }
                            }
                        }
                        {
                            QoreFunctionIterator fvi(*mfb);
                            while (fvi.next()) {
                                fb_ucv = const_cast<UserConstructorVariant*>(
                                    dynamic_cast<const UserConstructorVariant*>(
                                        fvi.getVariant()));
                                if (fb_ucv) {
                                    break;
                                }
                            }
                        }
                        if (main_ucv && fb_ucv) {
                            main_ucv->transplantBCAList(fb_ucv);
                        }
                    }
                    main_uvb->registerPrecompiledAOTFunction(aot_func->fn_ptr, ctx);
                    ++registered;
                    printd(2, "AOT v2: registered '%s' from fallback (on main variant)\n",
                        variant_key.c_str());
                } else {
                    fb_uvb->registerPrecompiledAOTFunction(aot_func->fn_ptr, ctx);
                    ++registered;
                    printd(2, "AOT v2: registered '%s' from fallback (on fallback variant)\n",
                        variant_key.c_str());
                }
            }
        };

        for (auto mi = qcp->hm.begin(), me = qcp->hm.end(); mi != me; ++mi) {
            processMethod(mi->second);
        }
        for (auto mi = qcp->shm.begin(), me = qcp->shm.end(); mi != me; ++mi) {
            processMethod(mi->second);
        }
    }

    // Walk child namespaces recursively
    for (auto ni = fallback_ns->nsl.nsmap.begin(), ne = fallback_ns->nsl.nsmap.end();
            ni != ne; ++ni) {
        QoreNamespace* child_ns = ni->second;
        if (child_ns) {
            // Find matching child namespace in main program
            auto main_ni = main_ns->nsl.nsmap.find(ni->first);
            if (main_ni != main_ns->nsl.nsmap.end() && main_ni->second) {
                registerFallbackFunctionsOnMainVariants(
                    qore_ns_private::get(*child_ns),
                    qore_ns_private::get(*main_ni->second),
                    main_pgm, func_map, registered);
            } else {
                // No matching main namespace — register on fallback variants
                registerAOTFunctionsInNamespace(
                    qore_ns_private::get(*child_ns), main_pgm, func_map, registered);
            }
        }
    }
}

extern "C" DLLEXPORT int qore_aot_run(
    int argc, char** argv,
    const char* source, int source_len,
    const char* label,
    int64_t parse_options,
    const QoreAOTFunc* functions, int num_functions
) {
    // Parse AOT runtime flags before passing remaining args to the program
    bool init_signals = true;
    int first_arg = 1;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-b") == 0) {
            init_signals = false;
            first_arg = i + 1;
        } else if (strcmp(argv[i], "-d") == 0) {
            ++debug;
            first_arg = i + 1;
        } else if (strcmp(argv[i], "--enable-debug") == 0 || strcmp(argv[i], "-G") == 0) {
            parse_options |= PO_ENABLE_DEBUG;
            first_arg = i + 1;
        } else {
            break;
        }
    }

    // Set up ARGV from command-line arguments before qore_init (matches normal qore binary order)
    qore_setup_argv(first_arg, argc, argv);

    // Initialize the Qore runtime with MIT license
    qore_init(QL_MIT, nullptr, false, init_signals ? QLO_NONE : QLO_DISABLE_SIGNAL_HANDLING);

    int rc = 0;
    // Use do-while(false) to ensure ~QoreProgramHelper() runs before qore_cleanup().
    // ~QoreProgramHelper creates a QoreForeignThreadHelper which accesses Qore TLS;
    // if qore_cleanup() is called first, TLS is gone and the destructor crashes.
    do {
        ExceptionSink xsink;
        ExceptionSink wsink;

        QoreProgramHelper qpgm(parse_options, xsink);
        if (xsink.isException()) {
            xsink.handleExceptions();
            rc = 2;
            break;
        }

        // Set JIT execution mode so functions without pre-compiled code will JIT on demand
        qpgm->setExecMode(QEM_JIT);

        // Set script path from the label so %requires with relative paths resolve
        // correctly (label is the absolute path to the original source file)
        if (label) {
            qpgm->setScriptPath(label);
        }

        // Parse the embedded source
        QoreString src_str(source, source_len);
        qpgm->parse(src_str.c_str(), label, &xsink, &wsink, QP_WARN_DEFAULT);

        // Display any warnings
        if (wsink.isException()) {
            wsink.handleWarnings();
        }

        if (xsink.isException()) {
            xsink.handleExceptions();
            rc = 2;
        } else {
            // Register pre-compiled function pointers with AOT context tables
            if (num_functions > 0 && functions) {
                // Build lookup map: name → QoreAOTFunc*
                std::unordered_map<std::string, const QoreAOTFunc*> func_map;
                const QoreAOTFunc* toplevel_func = nullptr;
                for (int i = 0; i < num_functions; ++i) {
                    if (functions[i].name && functions[i].fn_ptr) {
                        if (strcmp(functions[i].name, "_toplevel") == 0) {
                            toplevel_func = &functions[i];
                        } else {
                            func_map[functions[i].name] = &functions[i];
                        }
                    }
                }

                // Walk namespace tree and register with context building
                qore_program_private* pp = qore_program_private::get(**qpgm);
                int registered = 0;
                qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);
                registerAOTFunctionsInNamespace(root_ns, *qpgm, func_map, registered);

                // Register the _toplevel function with context
                if (toplevel_func) {
                    TopLevelStatementBlock& sb = pp->sb;

                    // Re-lower top-level code to IR to build context
                    QoreIRFunction* ir_func = new QoreIRFunction("_toplevel");

                    // Collect nested body locals as pre-instantiated.  Root
                    // top-level locals are owned by QoreProgram.
                    collectAllStatementLocals(&sb, ir_func->all_body_locals);
                    removeBlockLocalsFromBodyLocals(&sb, ir_func->all_body_locals);
                    for (LocalVar* lv : ir_func->all_body_locals) {
                        ir_func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(lv));
                    }
                    if (const LVList* top_lvars = sb.getLVList()) {
                        for (unsigned i = 0; i < top_lvars->size(); ++i) {
                            ir_func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(top_lvars->lv[i]));
                        }
                    }

                    QoreIRBuilder builder(ir_func);
                    auto* entry = ir_func->createBlock("entry");
                    builder.setBlock(entry);

                    QoreParseContext parse_context(*qpgm);
                    QoreIRLowering lowering(builder, &parse_context);
                    std::string lower_error;
                    bool ctx_ok = false;
                    if (lowering.lowerStatementBlock(&sb, lower_error)) {
                        // Ensure terminator
                        auto& last_block = ir_func->blocks.back();
                        if (last_block->instructions.empty() ||
                                (last_block->instructions.back()->opcode != QoreIROpcode::Return &&
                                 last_block->instructions.back()->opcode != QoreIROpcode::ReturnNothing &&
                                 last_block->instructions.back()->opcode != QoreIROpcode::Br &&
                                 last_block->instructions.back()->opcode != QoreIROpcode::Rethrow)) {
                            builder.createReturnNothing();
                        }
                        std::string verify_error;
                        if (QoreIRVerifier::verify(*ir_func, verify_error)) {
                            QoreAOTContext* ctx = buildAOTContext(*ir_func,
                                toplevel_func->num_locals, toplevel_func->num_globals,
                                toplevel_func->num_exprs, toplevel_func->num_stmts,
                                qore_aot_func_num_regex_cases(*toplevel_func));
                            if (ctx) {
                                sb.registerPrecompiledAOTTopLevel(toplevel_func->fn_ptr, ctx);
                                ++registered;
                                ctx_ok = true;
                                if (ctx->num_lv_path_insts > 0) {
                                    ctx->lv_path_ir_func.reset(ir_func);
                                    ir_func = nullptr;
                                }
                                printd(2, "AOT: registered pre-compiled _toplevel with context "
                                    "(locals=%d, globals=%d, exprs=%d, stmts=%d)\n",
                                    toplevel_func->num_locals, toplevel_func->num_globals,
                                    toplevel_func->num_exprs, toplevel_func->num_stmts);
                            }
                        } else {
                            printd(1, "AOT: _toplevel re-verification failed: %s\n", verify_error.c_str());
                        }
                    } else {
                        printd(1, "AOT: _toplevel re-lowering failed: %s\n", lower_error.c_str());
                    }
                    delete ir_func;

                    if (!ctx_ok) {
                        printd(1, "AOT: failed to build context for _toplevel\n");
                    }
                }

                printd(1, "AOT: registered %d/%d pre-compiled functions\n", registered, num_functions);
            }

            // Run the program
            QoreValue rv = qpgm->run(&xsink);
            rc = rv.getAsBigInt();
            rv.discard(&xsink);

            if (xsink.isException()) {
                rc = 3;
            }
        }

        xsink.handleExceptions();
    } while (false);

    qore_cleanup();
    return rc;
}

// ---- Source-Stripped AOT Runtime (V2) ----

// Forward declarations for init function execution
static int executeInitFunctions(QoreProgram* pgm,
    const std::vector<AOTInitFuncExecInfo>& exec_infos,
    const std::vector<AOTInitFuncDescriptor>& descriptors,
    const char* mod_name,
    QoreProgram* shadow_pgm,
    const char* mod_path,
    bool write_shadow,
    ExceptionSink* failure_sink);
static void preInitStaticVarsInProgram(QoreProgram* pgm);

static std::string makeAOTRegistrationFailureMessage(const char* label,
        int registered, int num_functions,
        const std::unordered_map<std::string, const QoreAOTFunc*>* remaining = nullptr,
        const std::vector<std::string>* errors = nullptr) {
    int unregistered = num_functions - registered;
    std::string rv = label ? label : "<aot>";
    rv += ": ";
    rv += std::to_string(unregistered);
    rv += "/";
    rv += std::to_string(num_functions);
    rv += " functions could not be registered; source fallback is disabled";
    if (remaining && !remaining->empty()) {
        std::vector<std::string> names;
        names.reserve(remaining->size());
        size_t checked = 0;
        for (const auto& i : *remaining) {
            if (++checked % 100 == 0
                    && qore_check_cancel(nullptr, "AOT registration diagnostic collection")) {
                rv += "; unregistered diagnostic collection cancelled";
                return rv;
            }
            names.push_back(i.first);
        }
        std::sort(names.begin(), names.end());

        rv += "; unregistered:";
        size_t limit = std::min<size_t>(names.size(), 16);
        for (size_t i = 0; i < limit; ++i) {
            rv += i ? ", " : " ";
            rv += names[i];
        }
        if (names.size() > limit) {
            rv += ", ... +";
            rv += std::to_string(names.size() - limit);
        }
    }
    if (errors && !errors->empty()) {
        rv += "; errors:";
        size_t limit = std::min<size_t>(errors->size(), 8);
        for (size_t i = 0; i < limit; ++i) {
            rv += i ? " | " : " ";
            rv += (*errors)[i];
        }
        if (errors->size() > limit) {
            rv += " | ... +";
            rv += std::to_string(errors->size() - limit);
        }
    }
    return rv;
}

static bool applyAOTModuleCommandsToProgram(QoreProgram* pgm,
        const QoreAOTBinaryReader& reader,
        const char* label, std::string& error) {
    std::vector<AOTModuleCommand> commands;
    std::string read_error;
    if (!readModuleCommands(reader, commands, read_error)) {
        error = "AOT module-command metadata read error";
        if (label && *label) {
            error += " in ";
            error += label;
        }
        if (!read_error.empty()) {
            error += ": " + read_error;
        }
        return false;
    }
    if (commands.empty()) {
        return true;
    }

    for (const AOTModuleCommand& command : commands) {
        if (command.module.empty() || command.command.empty()) {
            error = "AOT module-command metadata contains an empty module or command";
            if (label && *label) {
                error += " in ";
                error += label;
            }
            return false;
        }

        ExceptionSink xsink;
        QoreString qcmd(command.command.c_str());
        int rc = MM.issueRuntimeCmd(command.module.c_str(), pgm, qcmd, &xsink);
        if (rc < 0 || xsink.isException()) {
            error = "AOT module command failed";
            if (label && *label) {
                error += " in ";
                error += label;
            }
            error += ": %module-cmd(";
            error += command.module;
            error += ") ";
            error += command.command;

            QoreValue ex_desc = xsink.getExceptionDesc();
            if (ex_desc.getType() == NT_STRING) {
                QoreStringValueHelper desc_str(ex_desc);
                if (desc_str->c_str() && *desc_str->c_str()) {
                    error += ": ";
                    error += desc_str->c_str();
                }
            }
            xsink.clear();
            return false;
        }
    }

    printd(2, "AOT: replayed %d module command%s for '%s'\n",
        (int)commands.size(), commands.size() == 1 ? "" : "s",
        label ? label : "<aot>");
    return true;
}

static bool applyAOTModuleCommandsToProgram(QoreProgram* pgm,
        const uint8_t* metadata, uint32_t metadata_len,
        const char* label, std::string& error) {
    QoreAOTBinaryReader reader;
    std::string read_error;
    if (!reader.open(metadata, metadata_len, read_error)) {
        error = "AOT module-command metadata read error";
        if (label && *label) {
            error += " in ";
            error += label;
        }
        error += ": " + read_error;
        return false;
    }
    return applyAOTModuleCommandsToProgram(pgm, reader, label, error);
}

//! Decide whether a recorded AOT dependency that failed to load is genuinely unavailable.
/** AOT dependency lists are built from the parsed program's feature lists at compile time, so they
    name exactly the modules that were loaded when the artifact was compiled — including optional
    %try-module modules whose %ifndef-guarded code (e.g. json's make_json) was baked in.  Such a
    module is therefore a hard runtime requirement: if it is genuinely unavailable the compiled
    function/type slots that reference it cannot be registered, which otherwise surfaces as a cryptic
    "unsupported AOT slot metadata" error far from the real cause.

    A failed load is only tolerated for a circular/in-progress dependency, which is still registered
    in the module map.  AOT init runs while binary-module initialization is temporarily outside the
    module-manager mutex, so this helper must use the locking lookup. */
static bool aotRequiredDepUnavailable(const char* dep) {
    return !QMM.findModule(dep);
}

extern "C" DLLEXPORT int qore_aot_run_v2(
    int argc, char** argv,
    const uint8_t* metadata, int metadata_len,
    const char* label,
    int64_t parse_options,
    const QoreAOTFunc* functions, int num_functions
) {
    // Parse AOT runtime flags before passing remaining args to the program
    bool init_signals = true;
    int first_arg = 1;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-b") == 0) {
            init_signals = false;
            first_arg = i + 1;
        } else if (strcmp(argv[i], "-d") == 0) {
            ++debug;
            first_arg = i + 1;
        } else if (strcmp(argv[i], "--enable-debug") == 0 || strcmp(argv[i], "-G") == 0) {
            parse_options |= PO_ENABLE_DEBUG;
            first_arg = i + 1;
        } else {
            break;
        }
    }

    // Set up ARGV from command-line arguments
    qore_setup_argv(first_arg, argc, argv);

    // Initialize the Qore runtime
    qore_init(QL_MIT, nullptr, false, init_signals ? QLO_NONE : QLO_DISABLE_SIGNAL_HANDLING);

    int rc = 0;
    // Use do-while(false) to ensure ~QoreProgramHelper() runs before qore_cleanup().
    // ~QoreProgramHelper creates a QoreForeignThreadHelper which accesses Qore TLS;
    // if qore_cleanup() is called first, TLS is gone and the destructor crashes.
    do {
        ExceptionSink xsink;

        QoreProgramHelper qpgm(parse_options, xsink);
        if (xsink.isException()) {
            xsink.handleExceptions();
            rc = 2;
            break;
        }

        // Set JIT execution mode
        qpgm->setExecMode(QEM_JIT);

        if (label) {
            qpgm->setScriptPath(label);
        }

        printd(2, "AOT v2: parse_options=0x%llx, PO_MODERN=0x%llx, has_modern=%d\n",
            (long long)parse_options, (long long)PO_MODERN,
            (int)((parse_options & PO_MODERN) == PO_MODERN));

        // Apply %prepend-module-path / %append-module-path lists before loading deps
        {
            std::vector<std::string> prepended, appended;
            std::string mp_error;
            if (readModulePathLists(metadata, static_cast<uint32_t>(metadata_len),
                    prepended, appended, mp_error)) {
                applyModulePathListsToProgram(*qpgm, prepended, appended);
            }
        }

        {
            std::string cmd_error;
            if (!applyAOTModuleCommandsToProgram(*qpgm, metadata,
                    static_cast<uint32_t>(metadata_len), label, cmd_error)) {
                printd(0, "AOT v2: %s\n", cmd_error.c_str());
                rc = 2;
                break;
            }
        }

        // Load module dependencies before deserialization so that module classes,
        // functions, etc. are available when resolving base classes and types
        bool dep_unavailable = false;
        {
            std::vector<std::string> deps;
            std::string dep_error;
            if (readDependencies(metadata, static_cast<uint32_t>(metadata_len), deps, dep_error)) {
                printd(2, "AOT v2: loading %d dependencies\n", (int)deps.size());
                for (const std::string& dep : deps) {
                    printd(2, "AOT v2: loading dependency '%s'\n", dep.c_str());
                    int dep_rc = MM.runTimeLoadModule(&xsink, dep.c_str(), *qpgm);
                    if (dep_rc < 0 || xsink.isException()) {
                        printd(2, "AOT v2: dependency '%s' load error (rc=%d)\n",
                            dep.c_str(), dep_rc);
                        xsink.clear();
                        if (aotRequiredDepUnavailable(dep.c_str())) {
                            xsink.raiseException("AOT-ERROR",
                                "the AOT-compiled program (%s) requires module '%s', which could not "
                                "be loaded; the program was AOT-compiled against '%s' (its compiled "
                                "code references that module's symbols) and cannot be loaded without it",
                                (label && *label) ? label : "<unknown path>", dep.c_str(), dep.c_str());
                            xsink.handleExceptions();
                            dep_unavailable = true;
                            break;
                        }
                    }
                }
            }
        }
        if (dep_unavailable) {
            rc = 2;
            break;
        }

        // Deserialize namespace tree from metadata (replaces source parsing)
        // Must set the parse context so UserVariantBase constructor can
        // call parse_get_parse_options() which reads thread-local current_pgm
        QoreAOTBinaryDeserializer deserializer;
        std::string deser_error;
        bool deser_ok = false;
        {
            ProgramRuntimeParseContextHelper pch(&xsink, *qpgm);
            if (xsink.isException()) {
                xsink.handleExceptions();
                rc = 2;
                break;
            }
            if (!deserializer.deserializeIntoProgram(*qpgm,
                    metadata, static_cast<uint32_t>(metadata_len), deser_error)) {
                printd(0, "AOT: metadata deserialization failed: %s\n", deser_error.c_str());
                rc = 2;
                break;
            }
            deser_ok = true;
        }
        if (!deser_ok) {
            break;
        }

        // Register pre-compiled function pointers
        if (num_functions > 0 && functions) {
            std::unordered_map<std::string, const QoreAOTFunc*> func_map;
            const QoreAOTFunc* toplevel_func = nullptr;
            for (int i = 0; i < num_functions; ++i) {
                if (functions[i].name && functions[i].fn_ptr) {
                    if (strcmp(functions[i].name, "_toplevel") == 0) {
                        toplevel_func = &functions[i];
                    } else {
                        func_map[functions[i].name] = &functions[i];
                    }
                }
            }

            // Register non-toplevel functions using slot maps (no IR re-lowering)
            qore_program_private* pp = qore_program_private::get(**qpgm);
            int registered = 0;
            qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);
            bool toplevel_registered = false;
            std::vector<std::string> registration_errors;
            AOTClosureRuntimeBindingMap native_closure_bindings;
            auto debug_metadata = makeAOTDebugMetadata(deserializer.getReader(),
                metadata, metadata_len);

            // Register _toplevel first so setLVarsFromAOTContext() populates
            // the program LVList before method/function contexts deserialize
            // closures that capture top-level locals.
            if (toplevel_func) {
                const QoreAOTSectionHeader* sm_sec = deserializer.getReader().findSection(
                    QoreAOTSectionType::SLOT_MAPS);
                if (sm_sec) {
                    const uint8_t* sm_base = deserializer.getReader().getSectionData(*sm_sec);
                    if (sm_base) {
                        const uint8_t* sm_ptr = sm_base;
                        const uint8_t* sm_end = sm_ptr + sm_sec->size;
                        uint32_t sm_count = QoreAOTBinaryReader::readU32(sm_ptr);
                        for (uint32_t fi = 0; fi < sm_count; ++fi) {
                            const uint8_t* entry_start = sm_ptr;
                            uint32_t entry_size = QoreAOTBinaryReader::readU32(sm_ptr);
                            const char* entry_name = deserializer.getReader().readStringRef(sm_ptr);
                            const uint8_t* entry_end = entry_start + 4 + entry_size;
                            sm_ptr = entry_start + 4;

                            if (entry_name && strcmp(entry_name, "_toplevel") == 0) {
                                std::string build_error;
                                QoreAOTContext* ctx = buildContextFromSlotMap(
                                    deserializer.getReader(), sm_ptr, sm_end,
                                    nullptr, *qpgm, *toplevel_func, "_toplevel", entry_end,
                                    nullptr, &build_error, debug_metadata, sm_base,
                                    nullptr, nullptr, &native_closure_bindings);
                                if (ctx) {
                                    pp->sb.registerPrecompiledAOTTopLevel(
                                        toplevel_func->fn_ptr, ctx);
                                    pp->sb.setLVarsFromAOTContext(ctx);
                                    ++registered;
                                    toplevel_registered = true;
                                    printd(2, "AOT v3: registered _toplevel from slot map\n");
                                } else if (!build_error.empty()) {
                                    registration_errors.push_back(std::move(build_error));
                                }
                                break;
                            }
                            sm_ptr = entry_start;
                            skipSlotMapEntry(deserializer.getReader(), sm_ptr, sm_end);
                        }
                    }
                }
            }

            // Use slot maps from the binary metadata to build contexts
            // Collect init functions for execution after slot map registration
            std::vector<AOTInitFuncExecInfo> init_func_contexts;
            printd(2, "AOT v2: calling registerAOTFunctionsFromSlotMaps with %d func_map entries, "
                "toplevel=%p\n", (int)func_map.size(), (void*)toplevel_func);
            registerAOTFunctionsFromSlotMaps(
                deserializer.getReader(), root_ns, *qpgm, func_map, registered,
                &init_func_contexts, deserializer.getTypeResolver(), &registration_errors,
                debug_metadata, false, nullptr, &native_closure_bindings, &deserializer);
            printd(2, "AOT v2: after slot map registration: %d registered, %d remaining, "
                "%d init functions\n",
                registered, (int)func_map.size(), (int)init_func_contexts.size());

            if (!registration_errors.empty()) {
                std::string msg = makeAOTRegistrationFailureMessage(label, registered, num_functions,
                    &func_map, &registration_errors);
                xsink.raiseException("AOT-ERROR", "%s", msg.c_str());
                xsink.handleExceptions();
                rc = 2;
                break;
            }

            // Execute init functions (constant and static var initialization)
            // Requires program context for closure creation (thread_get_all_closure_vars)
            if (!init_func_contexts.empty()) {
                std::vector<AOTInitFuncDescriptor> init_descriptors;
                std::string init_error;
                readInitFuncs(metadata, static_cast<uint32_t>(metadata_len),
                    init_descriptors, init_error);
                if (!init_descriptors.empty()) {
                    ProgramThreadCountContextHelper tch(&xsink, *qpgm, false);
                    if (!xsink.isException()) {
                        executeInitFunctions(*qpgm, init_func_contexts,
                            init_descriptors, label, nullptr, nullptr,
                            /*write_shadow=*/true, /*failure_sink=*/nullptr);
                    }
                }
            }
            preInitStaticVarsInProgram(*qpgm);

            // Source fallback is intentionally not available. Legacy
            // FUNC_SOURCES records with fallback function names are rejected during
            // metadata deserialization; any registration gap below is a hard error.
            assert(!deserializer.hasLegacyFallbackFunctions());

            // Register the _toplevel function from slot maps
            if (toplevel_func) {
                // Find _toplevel in SLOT_MAPS section
                const QoreAOTSectionHeader* sm_sec = deserializer.getReader().findSection(
                    QoreAOTSectionType::SLOT_MAPS);

                if (!toplevel_registered && sm_sec) {
                    const uint8_t* sm_base = deserializer.getReader().getSectionData(*sm_sec);
                    if (sm_base) {
                        const uint8_t* sm_ptr = sm_base;
                        const uint8_t* sm_end = sm_ptr + sm_sec->size;
                        uint32_t sm_count = QoreAOTBinaryReader::readU32(sm_ptr);
                        for (uint32_t fi = 0; fi < sm_count; ++fi) {
                            const uint8_t* entry_start = sm_ptr;
                            uint32_t entry_size = QoreAOTBinaryReader::readU32(sm_ptr);  // consume size prefix
                            const char* entry_name = deserializer.getReader().readStringRef(sm_ptr);  // peek at name
                            const uint8_t* entry_end = entry_start + 4 + entry_size;
                            sm_ptr = entry_start + 4;  // reset: after size prefix, before name

                            if (entry_name && strcmp(entry_name, "_toplevel") == 0) {
                                std::string build_error;
                                QoreAOTContext* ctx = buildContextFromSlotMap(
                                    deserializer.getReader(), sm_ptr, sm_end,
                                    nullptr, *qpgm, *toplevel_func, "_toplevel", entry_end,
                                    nullptr, &build_error, debug_metadata, sm_base,
                                    nullptr, nullptr, &native_closure_bindings);
                                if (ctx) {
                                    pp->sb.registerPrecompiledAOTTopLevel(
                                        toplevel_func->fn_ptr, ctx);
                                    // Set LVList so doTopLevelInstantiation() can instantiate the locals
                                    pp->sb.setLVarsFromAOTContext(ctx);
                                    ++registered;
                                    toplevel_registered = true;
                                    printd(2, "AOT v2: registered _toplevel from slot map\n");
                                } else if (!build_error.empty()) {
                                    registration_errors.push_back(std::move(build_error));
                                }
                                break;
                            } else {
                                // Skip this entry
                                sm_ptr = entry_start;  // reset: before size prefix for skipSlotMapEntry
                                skipSlotMapEntry(deserializer.getReader(), sm_ptr, sm_end);
                            }
                        }
                    }
                }

                if (!toplevel_registered) {
                    printd(0, "AOT v2: _toplevel not registered from serialized slot maps\n");
                }

            }

            printd(2, "AOT v2: registered %d/%d pre-compiled functions\n", registered, num_functions);

            if (registered < num_functions) {
                std::string msg = makeAOTRegistrationFailureMessage(label, registered, num_functions,
                    &func_map, &registration_errors);
                xsink.raiseException("AOT-ERROR", "%s", msg.c_str());
                xsink.handleExceptions();
                rc = 2;
                break;
            }
        }

        // Run the v2 program
        QoreValue rv = qpgm->run(&xsink);
        rc = rv.getAsBigInt();
        rv.discard(&xsink);

        if (xsink.isException()) {
            rc = 3;
        }

        xsink.handleExceptions();

    } while (false);

    qore_cleanup();
    return rc;
}

// ---- AOT Module Runtime Functions ----

//! Per-module state for AOT-compiled modules
// Forward declaration
static int executeInitFunctions(QoreProgram* pgm,
    const std::vector<AOTInitFuncExecInfo>& exec_infos,
    const std::vector<AOTInitFuncDescriptor>& descriptors,
    const char* mod_name,
    QoreProgram* shadow_pgm,
    const char* mod_path,
    bool write_shadow,
    ExceptionSink* failure_sink);

struct AotModuleState {
    QoreProgram* pgm = nullptr;
    const QoreAOTFunc* funcs = nullptr;
    int num_funcs = 0;
    //! Serializes init-context construction for this module's shared shadow Program.
    /** Context construction appends LocalVars to the shadow Program's arena.  The lock is per module so a builder
        that resolves an API in another Program cannot block unrelated module builders while it waits for that
        Program's parse lock.
    */
    std::shared_ptr<QoreRecursiveThreadLock> shadow_build_lock =
        std::make_shared<QoreRecursiveThreadLock>();
    //! Modules that should be reexported (from %requires(reexport) directives)
    std::vector<std::string> reexport_deps;
    //! Serialized metadata retained only when deferred init cannot reuse an open reader.
    std::shared_ptr<const std::vector<uint8_t>> metadata;
    //! Reader opened during module_init and reused by deferred ns_init work.
    std::shared_ptr<QoreAOTBinaryReader> init_reader;
    //! Debug metadata shared by regular and deferred-init contexts.
    std::shared_ptr<const QoreAOTDebugMetadata> debug_metadata;
    //! Immutable init function descriptors (target type, ns path, item name) read during module_init
    std::shared_ptr<const std::vector<AOTInitFuncDescriptor>> init_descriptors;
    //! Module path/label used for get_module_context_path() during deferred module init
    std::string path;
    //! Progress of the one-time initialization of the shared shadow (module-own) Program's
    //! constant/static-var values.  The shadow Program is shared by ALL imports of this AOT module, so
    //! its ConstantEntries / static vars must be populated exactly once (by the first import that runs
    //! the init functions) AND fully before any concurrent importer reads them.  Guarded by
    //! get_aot_module_state_lock(); waiters block on get_aot_shadow_init_cond().  Makes the previously
    //! idempotent-but-non-atomic "!hasValue()" shadow-write guard explicit and race-free once the
    //! module-load lock no longer serializes init.
    enum ShadowInitState : unsigned char {
        SHADOW_NOT_STARTED = 0,  //!< no import has begun populating the shadow
        SHADOW_IN_PROGRESS,      //!< shadow_init_tid is populating the shadow; other importers wait
        SHADOW_DONE              //!< shadow fully populated; importers read it and write only their target
    };
    ShadowInitState shadow_init_state = SHADOW_NOT_STARTED;
    int shadow_init_tid = 0;     //!< owner thread while SHADOW_IN_PROGRESS
};

//! Map from module name to per-module state
/** Multiple AOT modules can be loaded simultaneously.  Each module's init creates a
    QoreProgram and stores it here keyed by module name.  The ns_init function uses
    get_module_context()->getName() to find the correct program for the module being
    imported.

    Thread safety: Access is serialized by get_aot_module_state_lock().  The binary module loader releases the
    module-manager mutex while running module init code, so this state cannot rely on QoreModuleManager locking.
*/
static std::unordered_map<std::string, AotModuleState> aot_module_map;

static bool useAOTSharedInitMetadata() {
    static const bool enabled = getenv("QORE_DISABLE_AOT_SHARED_INIT_METADATA") == nullptr;
    return enabled;
}

//! Current module being initialized (valid only during qore_aot_module_init / _init_v2)
/** These encode a per-init-context concept: they are written at a module's load start and read only
    as the ns_init fallback during that same module's init, always on the loading thread.  They are
    thread_local so that concurrent cold init of *different* modules (enabled once the process-global
    module-load lock M is retired) cannot clobber each other's "current module" context.  While M
    still serializes init, only one thread is ever in init at a time, so this is behaviorally
    identical to the previous file-global storage; see aot_module_init_context_path below for the
    same pattern. */
static thread_local QoreProgram* aot_module_pgm = nullptr;
static thread_local std::string aot_module_name;
static thread_local std::string aot_module_path;
static thread_local const QoreAOTFunc* aot_module_funcs = nullptr;
static thread_local int aot_module_num_funcs = 0;

//! Runtime .qmod path passed by the module loader to generated API-2 AOT module init adapters.
static thread_local std::string aot_module_init_context_path;

extern "C" DLLEXPORT void qore_aot_set_module_init_context_path(QoreModuleInitContext* ctx) {
    if (ctx && !ctx->path.empty()) {
        aot_module_init_context_path = ctx->path;
    } else {
        aot_module_init_context_path.clear();
    }
}

extern "C" DLLEXPORT void qore_aot_clear_module_init_context_path() {
    aot_module_init_context_path.clear();
}

static void append_unique_module_paths(std::vector<std::string>& target, const std::vector<std::string>& source) {
    for (const std::string& path : source) {
        bool seen = false;
        for (const std::string& existing : target) {
            if (existing == path) {
                seen = true;
                break;
            }
        }
        if (!seen) {
            target.push_back(path);
        }
    }
}

static void inheritAOTModulePathLists(QoreProgram* local_pgm, QoreProgram* parent_pgm,
        const std::vector<std::string>& module_prepended = {},
        const std::vector<std::string>& module_appended = {}) {
    if (!local_pgm) {
        return;
    }

    qore_program_private* local_priv = qore_program_private::get(*local_pgm);
    if (!local_priv) {
        return;
    }

    const qore_program_private* parent_priv = nullptr;
    if (parent_pgm && parent_pgm != local_pgm) {
        parent_priv = qore_program_private::get(*parent_pgm);
    }

    // Source modules inherit the importing Program's path lists, then apply
    // their own directives while parsing. Recreate the equivalent qmod search
    // order: module prepends, inherited prepends, global module path,
    // inherited appends, module appends.
    append_unique_module_paths(local_priv->prepended_module_paths, module_prepended);
    if (parent_priv) {
        append_unique_module_paths(local_priv->prepended_module_paths, parent_priv->prepended_module_paths);
        append_unique_module_paths(local_priv->appended_module_paths, parent_priv->appended_module_paths);
    }
    append_unique_module_paths(local_priv->appended_module_paths, module_appended);
}

static bool aotInitTraceEnabled() {
    static const bool enabled = getenv("QORE_AOT_INIT_TRACE") != nullptr;
    return enabled;
}

QoreRecursiveThreadLock& qore_aot_get_init_execution_lock() {
    static QoreRecursiveThreadLock lock;
    return lock;
}

// Depth of module-load lock acquisitions held by the current thread.  The lock is recursive, so
// this can be > 1 when a module load re-enters module loading (e.g. an AOT module initializer
// loading another module).  Maintained in all builds so that the debug assertion below can tell
// "this thread already holds the module-load lock" (legal) from "this thread is acquiring it for
// the first time while holding an inner lock" (a lock-order inversion).
static thread_local unsigned qore_module_load_lock_depth = 0;

#ifdef DEBUG
// Depth of module-inner locks (locks that must be acquired *after* the module-load lock) held by
// the current thread; see QoreModuleInnerLockHelper.  Only tracked in debug builds; the assertion
// that reads it is the only consumer, so it does not exist at all in non-debug builds.
static thread_local unsigned qore_module_inner_lock_depth = 0;
#endif

QoreModuleLoadLockHelper::QoreModuleLoadLockHelper() {
    // The process-global module-load lock M has been RETIRED: it no longer takes a lock.
    //
    // M conflated two roles and was held across the whole module load, including the cross-thread
    // dependency wait, which reformed a process-wide deadlock (a thread holding M blocked on a
    // cross-thread module load whose counterparty needed M).  Its roles are now covered without a
    // global lock:
    //   - "serialize init" is per-feature single-writer load state (ModuleManager's
    //     ModuleLoadEntry/waiter loop) + the AOT shared shadow-Program init barrier
    //     (AotModuleState::shadow_init_state) + per-Program / per-module state; and
    //   - "lock ordering vs. the target parse lock" is covered by the cross-thread cycle detector
    //     (module_wait_for) — AOT init takes no cross-Program parse-ownership lock, so no parse-lock
    //     ABBA can form.
    //
    // The depth counter and the assertion below are retained as a debug regression net: they detect
    // a module-inner lock (e.g. module-jni's class-map lock, marked via QoreModuleInnerLockHelper)
    // being held across the start of a module load — the inversion that retiring M would otherwise
    // let deadlock silently.  A held inner lock is legal only when this thread is already inside a
    // module load (load_depth > 0), i.e. the inner lock was taken *during* init, not across it.
    assert(!qore_module_inner_lock_depth || qore_module_load_lock_depth);

    ++qore_module_load_lock_depth;
}

QoreModuleLoadLockHelper::~QoreModuleLoadLockHelper() {
    assert(qore_module_load_lock_depth);
    --qore_module_load_lock_depth;
}

// NOTE: the bodies below are compiled out in non-debug builds, but the symbols are always exported
// so that a module built with a different DEBUG setting than libqore still links; in that case the
// tracking is simply inert
QoreModuleInnerLockHelper::QoreModuleInnerLockHelper() {
#ifdef DEBUG
    ++qore_module_inner_lock_depth;
#endif
}

QoreModuleInnerLockHelper::~QoreModuleInnerLockHelper() {
#ifdef DEBUG
    assert(qore_module_inner_lock_depth);
    --qore_module_inner_lock_depth;
#endif
}

static QoreThreadLock& get_aot_module_state_lock() {
    static QoreThreadLock lock;
    return lock;
}

// Condition variable used to let a thread that is applying an already-loaded AOT module to a new
// Program wait until another thread has finished populating the module's shared "shadow" Program
// (its own QoreProgram, shared by every importing Program).  Guarded by get_aot_module_state_lock().
// Once the process-global module-load lock no longer serializes AOT init, concurrent imports of the
// same module race the shared shadow; this barrier makes the shadow populated exactly once and fully
// before any concurrent importer reads it.  The waiting thread holds no other lock and the populating
// thread holds no lock across generated init code, so this wait cannot deadlock (module init reads
// already-loaded dependency constants and does not trigger a circular cross-module shadow apply; real
// module-dependency cycles are detected at load time).
static QoreCondition& get_aot_shadow_init_cond() {
    static QoreCondition cond;
    return cond;
}

//! Extract dependency module names from source \%requires directives
/** Parses the source to find all \%requires directives and extracts the module names.
    Skips "qore" since it's always available.
    NOTE: Properly skips \%requires inside block comments and line comments.
    \param source the source text
    \param source_len length of source
    \param reexport_deps if non-null, receives names of deps with (reexport) flag
    \return vector of dependency module names
*/
static std::vector<std::string> extractDependencies(const char* source, int source_len,
        std::vector<std::string>* reexport_deps = nullptr) {
    std::vector<std::string> deps;
    const char* p = source;
    const char* end = source + source_len;
    bool in_block_comment = false;

    while (p < end) {
        // Check for block comment start/end
        if (!in_block_comment && p + 1 < end && p[0] == '/' && p[1] == '*') {
            in_block_comment = true;
            p += 2;
            continue;
        }
        if (in_block_comment && p + 1 < end && p[0] == '*' && p[1] == '/') {
            in_block_comment = false;
            p += 2;
            continue;
        }
        if (in_block_comment) {
            ++p;
            continue;
        }

        // Skip leading whitespace
        while (p < end && (*p == ' ' || *p == '\t')) {
            ++p;
        }

        // Skip line comments (# ...)
        if (p < end && *p == '#') {
            while (p < end && *p != '\n') {
                ++p;
            }
            if (p < end) {
                ++p;  // skip newline
            }
            continue;
        }

        // Check for %requires directive
        if (p + 9 <= end && strncmp(p, "%requires", 9) == 0) {
            p += 9;
            // Skip whitespace after %requires
            while (p < end && (*p == ' ' || *p == '\t')) {
                ++p;
            }
            // Check for optional (reexport) flag
            bool is_reexport = false;
            if (p + 10 <= end && strncmp(p, "(reexport)", 10) == 0) {
                is_reexport = true;
                p += 10;
                while (p < end && (*p == ' ' || *p == '\t')) {
                    ++p;
                }
            }
            // Extract module name (until whitespace, newline, or version operator)
            const char* name_start = p;
            while (p < end && *p != '\n' && *p != ' ' && *p != '\t' &&
                   *p != '<' && *p != '>' && *p != '=') {
                ++p;
            }
            if (p > name_start) {
                std::string dep_name(name_start, p - name_start);
                // Skip "qore" as it's always available
                if (dep_name != "qore") {
                    deps.push_back(dep_name);
                    if (is_reexport && reexport_deps) {
                        reexport_deps->push_back(dep_name);
                    }
                }
            }
        }

        // Skip to end of line
        while (p < end && *p != '\n') {
            // Also check for block comment start within the line
            if (p + 1 < end && p[0] == '/' && p[1] == '*') {
                in_block_comment = true;
                p += 2;
                break;
            }
            ++p;
        }
        if (p < end && *p == '\n') {
            ++p;  // skip newline
        }
    }

    return deps;
}

struct AOTModuleInitRunResult {
    bool attempted = false;
    bool success = true;
    bool hard_error = false;
    std::string error;
};

static void retryPendingAOTModuleInitsForProgram(QoreProgram* tpgm,
        ExceptionSink& xsink);

static void aotFindInitConstantEntries(const AOTInitFuncDescriptor& desc, QoreProgram* pgm,
        QoreProgram* shadow_pgm, ConstantEntry*& target_ce, ConstantEntry*& shadow_ce);

//! Returns true if an init descriptor still has target or shadow state to populate
static bool aotInitDescriptorNeedsExecution(const AOTInitFuncDescriptor& desc, QoreProgram* pgm,
        QoreProgram* shadow_pgm, bool write_shadow) {
    qore_program_private* pp = qore_program_private::get(*pgm);
    qore_program_private* shadow_pp = shadow_pgm ? qore_program_private::get(*shadow_pgm) : nullptr;

    switch (desc.target_type) {
        case AOTCompiledInitFunc::NS_CONSTANT:
        case AOTCompiledInitFunc::CLASS_CONSTANT: {
            ConstantEntry* target_ce = nullptr;
            ConstantEntry* shadow_ce = nullptr;
            aotFindInitConstantEntries(desc, pgm, shadow_pgm, target_ce, shadow_ce);
            if (!target_ce && !shadow_ce) {
                return true;
            }
            bool target_done = !target_ce || target_ce->hasValue();
            bool shadow_done = !write_shadow || !shadow_ce || shadow_ce == target_ce || shadow_ce->hasValue();
            return !target_done || !shadow_done;
        }

        case AOTCompiledInitFunc::STATIC_VAR: {
            auto find_static_var = [&desc](qore_program_private* p) -> QoreVarInfo* {
                if (!p) {
                    return nullptr;
                }
                const qore_ns_private* found_ns = nullptr;
                const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
                    *p->RootNS, desc.ns_path.c_str(), found_ns);
                return qc ? qore_class_private::get(*const_cast<QoreClass*>(qc))->vars.find(
                    desc.item_name.c_str()) : nullptr;
            };
            auto has_concrete_value = [](QoreVarInfo* vi) -> bool {
                if (!vi || !vi->eval_init) {
                    return false;
                }
                QoreValue value = vi->getRuntimeReferencedValue();
                bool concrete = !value.needsEval();
                value.discard(nullptr);
                return concrete;
            };
            QoreVarInfo* target_vi = find_static_var(pp);
            QoreVarInfo* shadow_vi = find_static_var(shadow_pp);
            if (!target_vi && !shadow_vi) {
                return true;
            }
            bool target_done = !target_vi || has_concrete_value(target_vi);
            bool shadow_done = !write_shadow || !shadow_vi || shadow_vi == target_vi
                || has_concrete_value(shadow_vi);
            return !target_done || !shadow_done;
        }

        case AOTCompiledInitFunc::GLOBAL_VAR:
        case AOTCompiledInitFunc::GLOBAL_VAR_CONSTRUCT: {
            auto find_global_var = [&desc](qore_program_private* p) -> Var* {
                if (!p) {
                    return nullptr;
                }
                qore_ns_private* ns = findNamespaceByPath(qore_ns_private::get(*p->RootNS), desc.ns_path);
                return ns ? ns->var_list.runtimeFindVar(desc.item_name.c_str()) : nullptr;
            };
            Var* target_var = find_global_var(pp);
            Var* shadow_var = find_global_var(shadow_pp);
            if (!target_var && !shadow_var) {
                return true;
            }
            bool same_storage = target_var && shadow_var
                && target_var->parseGetVar() == shadow_var->parseGetVar();
            bool target_done = !target_var || target_var->isAOTInitDone();
            bool shadow_done = !write_shadow || !shadow_var || same_storage || shadow_var->isAOTInitDone();
            return !target_done || !shadow_done;
        }

        case AOTCompiledInitFunc::MODULE_INIT:
            return write_shadow;

        case AOTCompiledInitFunc::OUTLINED_HELPER:
            return false;
    }
    return true;
}

static AOTModuleInitRunResult runAOTModuleInitForProgram(const std::string& mod_name,
        QoreProgram* tpgm, ExceptionSink& xsink) {
    AOTModuleInitRunResult result;
    if (mod_name.empty() || !tpgm) {
        return result;
    }
    qore_program_private* target_pp = qore_program_private::get(*tpgm);

    const QoreAOTFunc* init_funcs = nullptr;
    int init_num_funcs = 0;
    QoreProgram* init_ctx_pgm = nullptr;
    std::shared_ptr<QoreRecursiveThreadLock> shadow_build_lock;
    std::shared_ptr<const std::vector<uint8_t>> init_metadata;
    std::shared_ptr<QoreAOTBinaryReader> cached_init_reader;
    std::shared_ptr<const QoreAOTDebugMetadata> cached_debug_metadata;
    std::shared_ptr<const std::vector<AOTInitFuncDescriptor>> init_descriptor_snapshot;
    std::string mod_path;
    // whether this run is the one that populates the shared shadow (module-own) Program; set
    // exactly once per module under the state lock (see AotModuleState::shadow_init_state)
    bool write_shadow = false;

    {
        AutoLocker aot_state_al(get_aot_module_state_lock());
        auto it = aot_module_map.find(mod_name);
        if (aotInitTraceEnabled()) {
            fprintf(stderr, "[aot-init] ns_init module=%s map_found=%d descriptors=%zu metadata=%zu funcs=%d\n",
                mod_name.c_str(), it != aot_module_map.end(),
                it != aot_module_map.end() && it->second.init_descriptors
                    ? it->second.init_descriptors->size() : 0,
                it != aot_module_map.end() && it->second.metadata
                    ? it->second.metadata->size() : 0,
                it != aot_module_map.end() ? it->second.num_funcs : 0);
        }
        if (it == aot_module_map.end()
                || !it->second.init_descriptors || it->second.init_descriptors->empty()
                || (!it->second.init_reader && !it->second.metadata)
                || target_pp->merged_aot_modules.find(mod_name)
                    == target_pp->merged_aot_modules.end()
                || target_pp->initialized_aot_modules.find(mod_name)
                    != target_pp->initialized_aot_modules.end()
                || target_pp->initializing_aot_modules.find(mod_name)
                    != target_pp->initializing_aot_modules.end()) {
            return result;
        }

        target_pp->initializing_aot_modules.insert(mod_name);
        init_funcs = it->second.funcs;
        init_num_funcs = it->second.num_funcs;
        init_ctx_pgm = it->second.pgm ? it->second.pgm : tpgm;
        shadow_build_lock = it->second.shadow_build_lock;
        init_metadata = it->second.metadata;
        cached_init_reader = it->second.init_reader;
        cached_debug_metadata = it->second.debug_metadata;
        init_descriptor_snapshot = it->second.init_descriptors;
        mod_path = it->second.path;

        // Coordinate the one-time population of the shared shadow Program.  Only the first
        // importer populates it (write_shadow=true); concurrent importers of the same module
        // wait until it is fully populated, then read it (write_shadow=false).  NOTE: the wait
        // releases only the state lock; the populating thread holds no lock across generated
        // init code, so this cannot deadlock.  While the module-load lock still serializes AOT
        // init, no thread ever observes SHADOW_IN_PROGRESS from another thread, so this is inert.
        while (true) {
            // 'it' may be invalidated by the wait below; re-find each iteration
            auto sit = aot_module_map.find(mod_name);
            if (sit == aot_module_map.end()) {
                break;  // module state removed (unloaded) concurrently; nothing to populate
            }
            if (sit->second.shadow_init_state == AotModuleState::SHADOW_IN_PROGRESS
                    && sit->second.shadow_init_tid != q_gettid()) {
                int wait_rc = get_aot_shadow_init_cond().waitWithInterrupt(
                    get_aot_module_state_lock(), &xsink);
                if (wait_rc == QORE_COND_RESULT_INTERRUPTED) {
                    target_pp->initializing_aot_modules.erase(mod_name);
                    result.attempted = true;
                    result.success = false;
                    return result;
                }
                continue;  // re-find and re-check after wake
            }
            if (sit->second.shadow_init_state == AotModuleState::SHADOW_NOT_STARTED) {
                sit->second.shadow_init_state = AotModuleState::SHADOW_IN_PROGRESS;
                sit->second.shadow_init_tid = q_gettid();
                write_shadow = true;
            }
            // else SHADOW_DONE, or SHADOW_IN_PROGRESS by this same thread (nested recursion):
            // read the shadow, do not (re)populate it -> write_shadow stays false
            break;
        }
    }

    assert(init_descriptor_snapshot);
    const std::vector<AOTInitFuncDescriptor>& init_descriptors = *init_descriptor_snapshot;

    // RAII backstop: if this run claimed the one-time shadow population but does not reach
    // finish() below (C++ unwind), release the claim and wake any waiters so a concurrent
    // importer can claim and complete it — waiters must never be stranded.  The normal-path
    // transition (SHADOW_DONE on success, back to SHADOW_NOT_STARTED on a retryable failure)
    // happens in finish(), which disarms this by clearing write_shadow.
    struct ShadowInitFinalizer {
        const std::string& mod_name;
        bool& write_shadow;
        ~ShadowInitFinalizer() {
            if (!write_shadow) {
                return;
            }
            AutoLocker al(get_aot_module_state_lock());
            auto sit = aot_module_map.find(mod_name);
            if (sit != aot_module_map.end()) {
                sit->second.shadow_init_state = AotModuleState::SHADOW_NOT_STARTED;
                sit->second.shadow_init_tid = 0;
            }
            get_aot_shadow_init_cond().broadcast();
        }
    } shadow_finalizer{mod_name, write_shadow};
    bool init_marker_active = true;
    struct InitMarkerFinalizer {
        qore_program_private* target_pp;
        const std::string& mod_name;
        bool& active;
        ~InitMarkerFinalizer() {
            if (!active) {
                return;
            }
            AutoLocker al(get_aot_module_state_lock());
            target_pp->initializing_aot_modules.erase(mod_name);
        }
    } init_marker_finalizer{target_pp, mod_name, init_marker_active};

    if (!useAOTSharedInitMetadata() && init_metadata) {
        init_metadata = std::make_shared<const std::vector<uint8_t>>(*init_metadata);
    }

    result.attempted = true;

    auto finish = [&](bool success, bool hard_error = false, const std::string& error = std::string()) {
        AutoLocker aot_state_al(get_aot_module_state_lock());
        target_pp->initializing_aot_modules.erase(mod_name);
        if (success) {
            target_pp->initialized_aot_modules.insert(mod_name);
        } else {
            target_pp->initialized_aot_modules.erase(mod_name);
        }
        init_marker_active = false;
        auto it = aot_module_map.find(mod_name);
        if (it != aot_module_map.end()) {
            if (write_shadow) {
                // Complete the one-time shared-shadow population claim: on success the shadow is
                // fully populated (SHADOW_DONE); on failure return it to SHADOW_NOT_STARTED so the
                // cross-module retry fixpoint (or a concurrent importer) can claim it again — the
                // shadow writes are guarded by !hasValue()/aot_shell_pending, so a second
                // populating pass is safe and only fills entries the failed pass left pending.
                it->second.shadow_init_state = success ? AotModuleState::SHADOW_DONE
                    : AotModuleState::SHADOW_NOT_STARTED;
                it->second.shadow_init_tid = 0;
            }
        }
        if (write_shadow) {
            // disarm shadow_finalizer: the state transition above is complete
            write_shadow = false;
            get_aot_shadow_init_cond().broadcast();
        }
        result.success = success;
        result.hard_error = hard_error;
        result.error = error;
        return result;
    };

    if (!init_ctx_pgm) {
        return finish(false, true, "missing module program");
    }
    if (init_ctx_pgm != tpgm) {
        retryPendingAOTModuleInitsForProgram(init_ctx_pgm, xsink);
        if (xsink) {
            return finish(false);
        }
    }

    QoreProgram* shadow_pgm = init_ctx_pgm != tpgm ? init_ctx_pgm : nullptr;
    std::vector<AOTInitFuncDescriptor> pending_descriptors;
    std::unordered_set<std::string> pending_func_names;
    for (size_t i = 0; i < init_descriptors.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(&xsink, "AOT pending initializer filtering")) {
            return finish(false);
        }
        const AOTInitFuncDescriptor& desc = init_descriptors[i];
        if (aotInitDescriptorNeedsExecution(desc, tpgm, shadow_pgm, write_shadow)) {
            pending_func_names.insert(desc.name);
            pending_descriptors.push_back(desc);
        }
    }
    if (aotInitTraceEnabled()) {
        fprintf(stderr, "[aot-init] ns_init module=%s pending descriptors=%zu/%zu\n",
            mod_name.c_str(), pending_descriptors.size(), init_descriptors.size());
    }
    if (pending_descriptors.empty()) {
        return finish(true);
    }

    std::unordered_map<std::string, const QoreAOTFunc*> func_map;
    for (int i = 0; init_funcs && i < init_num_funcs; ++i) {
        if (i && !(i % 100) && qore_check_cancel(&xsink, "AOT pending initializer function lookup")) {
            return finish(false);
        }
        const char* fname = init_funcs[i].name;
        if (init_funcs[i].fn_ptr && isAOTInitFunctionName(fname)
                && pending_func_names.find(fname) != pending_func_names.end()) {
            func_map[fname] = &init_funcs[i];
        }
    }
    if (aotInitTraceEnabled()) {
        fprintf(stderr, "[aot-init] ns_init module=%s init func_map=%zu\n",
            mod_name.c_str(), func_map.size());
    }

    QoreAOTBinaryReader fallback_init_reader;
    const QoreAOTBinaryReader* init_reader = cached_init_reader.get();
    if (!init_reader) {
        std::string reader_error;
        if (!init_metadata || !fallback_init_reader.open(init_metadata->data(),
                static_cast<uint32_t>(init_metadata->size()), reader_error)) {
            return finish(false, true, std::string("metadata open failed: ") + reader_error);
        }
        init_reader = &fallback_init_reader;
    }

    RootQoreNamespace* mod_root = init_ctx_pgm->getRootNS();
    if (!mod_root) {
        return finish(false, true, "missing module root namespace");
    }
    qore_ns_private* init_root_priv = qore_ns_private::get(*mod_root);
    std::vector<AOTInitFuncExecInfo> init_func_contexts;
    int registered = 0;
    std::vector<std::string> registration_errors;
    auto debug_metadata = cached_debug_metadata;
    if (!debug_metadata && init_metadata) {
        debug_metadata = makeAOTDebugMetadata(*init_reader,
            init_metadata->data(), static_cast<int>(init_metadata->size()));
    }
    {
        // Lock the thread-current Program before the per-module shadow builder.  Static method references decoded
        // below acquire this parse context.  Taking the shadow lock first can deadlock with the first importer,
        // which already owns the shadow Program's parse context and is waiting to build the same module's contexts.
        CurrentProgramRuntimeParseContextHelper current_pch;
        // Serialize only builders for this module's shared shadow Program.  Context
        // deserialization can resolve APIs in another Program and wait for its parse
        // lock; taking that context above gives every same-module importer one lock order
        // (Program parse context, then shadow builder), while the per-module scope avoids
        // coupling unrelated module loads.
        assert(shadow_build_lock);
        AutoLocker shadow_build_al(*shadow_build_lock);
        registerAOTFunctionsFromSlotMaps(*init_reader, init_root_priv,
            init_ctx_pgm, func_map, registered, &init_func_contexts, nullptr,
            &registration_errors, debug_metadata, false, nullptr, nullptr, nullptr,
            tpgm);
    }

    if (aotInitTraceEnabled()) {
        fprintf(stderr, "[aot-init] ns_init module=%s registered=%d contexts=%zu remaining=%zu\n",
            mod_name.c_str(), registered, init_func_contexts.size(), func_map.size());
    }
    if (!registration_errors.empty()) {
        std::string msg = makeAOTRegistrationFailureMessage(mod_name.c_str(), registered,
            static_cast<int>(func_map.size() + registered), &func_map, &registration_errors);
        return finish(false, true, msg);
    }

    printd(2, "AOT ns_init '%s': got %d init func contexts\n",
        mod_name.c_str(), (int)init_func_contexts.size());
    if (!init_func_contexts.empty()) {
        printd(2, "AOT ns_init '%s': executing %d init functions\n",
            mod_name.c_str(), (int)init_func_contexts.size());
        int failed = executeInitFunctions(tpgm, init_func_contexts,
            pending_descriptors, mod_name.c_str(), shadow_pgm,
            mod_path.empty() ? nullptr : mod_path.c_str(),
            write_shadow, &xsink);
        return finish(failed == 0);
    }

    return finish(true);
}

static void retryPendingAOTModuleInitsForProgram(QoreProgram* tpgm,
        ExceptionSink& xsink) {
    if (!tpgm) {
        return;
    }
    qore_program_private* target_pp = qore_program_private::get(*tpgm);

    for (int round = 0; round < 16; ++round) {
        std::vector<std::string> pending;
        {
            AutoLocker aot_state_al(get_aot_module_state_lock());
            for (const auto& entry : aot_module_map) {
                const AotModuleState& state = entry.second;
                if (state.init_descriptors && !state.init_descriptors->empty()
                        && (state.init_reader || state.metadata)
                        && target_pp->merged_aot_modules.find(entry.first)
                            != target_pp->merged_aot_modules.end()
                        && target_pp->initialized_aot_modules.find(entry.first)
                            == target_pp->initialized_aot_modules.end()
                        && target_pp->initializing_aot_modules.find(entry.first)
                            == target_pp->initializing_aot_modules.end()) {
                    pending.push_back(entry.first);
                }
            }
        }
        if (pending.empty()) {
            return;
        }

        bool completed_any = false;
        bool attempted_any = false;
        for (const std::string& name : pending) {
            AOTModuleInitRunResult r =
                runAOTModuleInitForProgram(name, tpgm, xsink);
            if (xsink) {
                return;
            }
            attempted_any = attempted_any || r.attempted;
            completed_any = completed_any || (r.attempted && r.success);
            if (r.hard_error) {
                printd(0, "AOT ns_init '%s': %s\n", name.c_str(), r.error.c_str());
            }
        }
        if (!attempted_any || !completed_any) {
            return;
        }
    }
}

//! Strip %requires directives from source code
/** AOT modules have already resolved their dependencies at compile time, so we must
    not process %requires directives when parsing the embedded source at runtime.
    Dependencies are loaded explicitly before the embedded source is parsed; replaying
    %requires here would re-enter module loading while the current module is still
    registered as in-progress.

    For %try-module blocks, we need special handling:
    - Strip the %try-module and %endtry directives themselves
    - Try to load the module at runtime
    - If the module loads successfully, strip the fallback code inside the block
    - If the module fails to load, KEEP the fallback code (typically %define directives)
      so that conditional compilation works correctly
*/
static std::string stripRequiresDirectives(const char* source, int source_len,
        QoreProgram* target_pgm = nullptr) {
    std::string result;
    result.reserve(source_len);

    const char* p = source;
    const char* end = source + source_len;

    // Track try-module state: depth and whether current block's module loaded
    struct TryModuleState {
        bool module_loaded;
    };
    std::vector<TryModuleState> try_module_stack;

    while (p < end) {
        // Skip leading whitespace
        while (p < end && (*p == ' ' || *p == '\t')) {
            ++p;
        }

        // Check for %requires directive
        if (p + 9 <= end && strncmp(p, "%requires", 9) == 0) {
            // Skip the entire line (including the newline)
            while (p < end && *p != '\n') {
                ++p;
            }
            if (p < end) {
                ++p;  // skip the newline
            }
            // Replace with a comment to preserve line numbers
            result += "# [AOT: %requires stripped]\n";
        } else if (p + 17 <= end && strncmp(p, "%try-child-module", 17) == 0) {
            // Child module declarations are delivered by the module description function
            // (qore_aot_fill_module_children()), so the directive must not be processed again when the
            // embedded source is parsed; see design/qore-module-structure.md "Child Modules"
            p += 17;
            const char* child_start = p;
            while (p < end && *p != '\n') {
                ++p;
            }
            std::string child(child_start, p - child_start);
            if (p < end) {
                ++p;  // skip the newline
            }
            result += "# [AOT: %try-child-module";
            result += child;
            result += " stripped]\n";
        } else if (p + 11 <= end && strncmp(p, "%try-module", 11) == 0) {
            // Start of %try-module block - extract module name and try to load it
            p += 11;
            while (p < end && (*p == ' ' || *p == '\t')) {
                ++p;
            }
            const char* mod_start = p;
            while (p < end && *p != '\n' && *p != ' ' && *p != '\t') {
                ++p;
            }
            std::string mod_name(mod_start, p - mod_start);

            // Try to load the module (it may already be loaded)
            bool loaded = false;
            if (!mod_name.empty()) {
                ExceptionSink xsink;
                QoreProgram* load_target = target_pgm;
                if (!load_target) {
                    AutoLocker aot_state_al(get_aot_module_state_lock());
                    load_target = aot_module_pgm;
                }
                int rc = MM.runTimeLoadModule(&xsink, mod_name.c_str(), load_target);
                loaded = (rc >= 0 && !xsink);
                xsink.clear();
            }

            try_module_stack.push_back({loaded});

            // Skip to end of line
            while (p < end && *p != '\n') {
                ++p;
            }
            if (p < end) {
                ++p;
            }
            result += "# [AOT: %try-module ";
            result += mod_name;
            result += loaded ? " loaded]\n" : " not loaded - keeping fallback]\n";
        } else if (p + 7 <= end && strncmp(p, "%endtry", 7) == 0) {
            // End of %try-module block
            if (!try_module_stack.empty()) {
                try_module_stack.pop_back();
            }
            while (p < end && *p != '\n') {
                ++p;
            }
            if (p < end) {
                ++p;
            }
            result += "# [AOT: %endtry]\n";
        } else if (!try_module_stack.empty() && try_module_stack.back().module_loaded) {
            // Inside a %try-module block where the module loaded - strip this line
            // (the fallback code is not needed)
            while (p < end && *p != '\n') {
                ++p;
            }
            if (p < end) {
                ++p;
            }
            result += "# [AOT: try-module fallback stripped]\n";
        } else {
            // Either outside try-module, or inside a block where module didn't load
            // Copy the line as-is (keeping %define and other fallback directives)
            while (p < end && *p != '\n') {
                result += *p++;
            }
            if (p < end) {
                result += *p++;  // copy the newline
            }
        }
    }

    return result;
}

//! C ABI entry point for AOT binaries (v3 - full 128-bit parse options)
//! Wrapper around qore_aot_run_v2, accepting parse_options as two int64_t values
extern "C" DLLEXPORT int qore_aot_run_v3(
    int argc, char** argv,
    const uint8_t* metadata, int metadata_len,
    const char* label,
    int64_t parse_options_lo, int64_t parse_options_hi,
    const QoreAOTFunc* functions, int num_functions
) {
    // Construct full 128-bit parse options from lo+hi components
    QoreParseOptions parse_options(parse_options_lo, parse_options_hi);

    printd(2, "AOT v3: entry argc=%d metadata_len=%d num_functions=%d debug=%d\n",
        argc, metadata_len, num_functions, debug);

    // Parse AOT runtime flags before passing remaining args to the program
    bool init_signals = true;
    int first_arg = 1;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-b") == 0) {
            // Disable signal handling (useful for valgrind)
            init_signals = false;
            first_arg = i + 1;
        } else if (strcmp(argv[i], "-d") == 0) {
            // Enable debug output (each -d increases debug level)
            ++debug;
            first_arg = i + 1;
        } else if (strcmp(argv[i], "--enable-debug") == 0 || strcmp(argv[i], "-G") == 0) {
            // Enable @debug and @assert statements (PO_ENABLE_DEBUG parse option)
            parse_options |= PO_ENABLE_DEBUG;
            first_arg = i + 1;
        } else {
            // First non-flag argument — rest goes to ARGV
            break;
        }
    }

    printd(2, "AOT v3: after flag parsing debug=%d first_arg=%d\n", debug, first_arg);

    // Set up ARGV from command-line arguments
    qore_setup_argv(first_arg, argc, argv);

    // Initialize the Qore runtime
    qore_init(QL_MIT, nullptr, false, init_signals ? QLO_NONE : QLO_DISABLE_SIGNAL_HANDLING);

    int rc = 0;
    // Use do-while(false) to ensure ~QoreProgramHelper() runs before qore_cleanup().
    // ~QoreProgramHelper creates a QoreForeignThreadHelper which accesses Qore TLS;
    // if qore_cleanup() is called first, TLS is gone and the destructor crashes.
    do {
        ExceptionSink xsink;

        QoreProgramHelper qpgm(parse_options, xsink);
        if (xsink.isException()) {
            xsink.handleExceptions();
            rc = 2;
            break;
        }

        // Set JIT execution mode
        qpgm->setExecMode(QEM_JIT);

        // Set script path so get_script_path() / get_script_dir() work correctly
        if (label) {
            qpgm->setScriptPath(label);
        }

        // Read and apply program-level metadata (exec-class name)
        {
            std::string exec_class_name;
            std::string meta_error;
            if (readProgramMetadata(metadata, static_cast<uint32_t>(metadata_len),
                    exec_class_name, meta_error)) {
                printd(2, "AOT v3: readProgramMetadata OK, exec_class_name='%s'\n",
                    exec_class_name.c_str());
                if (!exec_class_name.empty()) {
                    qpgm->setExecClass(exec_class_name.c_str());
                    printd(2, "AOT v3: exec-class set to '%s'\n", exec_class_name.c_str());
                }
            } else {
                printd(0, "AOT v3: failed to read program metadata: %s\n", meta_error.c_str());
            }
        }

        // Re-apply %prepend-module-path / %append-module-path lists to the
        // freshly-created Program BEFORE loading dependency modules, so that
        // the blob's declared search paths take effect during runTimeLoadModule
        // below.  See design/qore-module-structure.md "Module Search Path".
        {
            std::vector<std::string> prepended, appended;
            std::string mp_error;
            if (readModulePathLists(metadata, static_cast<uint32_t>(metadata_len),
                    prepended, appended, mp_error)) {
                applyModulePathListsToProgram(*qpgm, prepended, appended);
                if (!prepended.empty() || !appended.empty()) {
                    printd(2, "AOT v3: applied %d prepended + %d appended module paths\n",
                        (int)prepended.size(), (int)appended.size());
                }
            } else {
                printd(0, "AOT v3: failed to read module-path lists: %s\n", mp_error.c_str());
            }
        }

        printd(2, "AOT v3: parse_options=0x%llx|0x%llx, PO_MODERN=0x%llx, has_modern=%d\n",
            (long long)parse_options_lo, (long long)parse_options_hi, (long long)PO_MODERN,
            (int)((parse_options & PO_MODERN) == PO_MODERN));

        {
            std::string cmd_error;
            if (!applyAOTModuleCommandsToProgram(*qpgm, metadata,
                    static_cast<uint32_t>(metadata_len), label, cmd_error)) {
                printd(0, "AOT v3: %s\n", cmd_error.c_str());
                rc = 2;
                break;
            }
        }

        // Load module dependencies before deserialization so that module classes,
        // functions, etc. are available when resolving base classes and types.
        // Scoped ProgramThreadCountContextHelper ensures thread-local program data
        // (tlpd) is initialized so module constant initializers can call constructors
        // that need thread-local variable stacks. Non-AOT scripts get this via
        // QoreProgram::parse(); AOT binaries skip parse() so we set it up explicitly.
        // NOTE: runtime=false to avoid premature doTopLevelInstantiation() before
        // setLVarsFromAOTContext() has populated the top-level LVList.
        bool dep_unavailable_v3 = false;
        {
            ProgramThreadCountContextHelper tch(&xsink, *qpgm, false);
            if (xsink.isException()) {
                xsink.handleExceptions();
                rc = 2;
                break;
            }

            std::vector<std::string> deps;
            std::string dep_error;
            if (readDependencies(metadata, static_cast<uint32_t>(metadata_len), deps, dep_error)) {
                printd(2, "AOT v3: loading %d dependencies\n", (int)deps.size());
                for (const std::string& dep : deps) {
                    printd(2, "AOT v3: loading dependency '%s'\n", dep.c_str());
                    int dep_rc = MM.runTimeLoadModule(&xsink, dep.c_str(), *qpgm);
                    if (dep_rc < 0 || xsink.isException()) {
                        printd(2, "AOT v3: dependency '%s' load error (rc=%d)\n",
                            dep.c_str(), dep_rc);
                        xsink.clear();
                        if (aotRequiredDepUnavailable(dep.c_str())) {
                            xsink.raiseException("AOT-ERROR",
                                "the AOT-compiled program (%s) requires module '%s', which could not "
                                "be loaded; the program was AOT-compiled against '%s' (its compiled "
                                "code references that module's symbols) and cannot be loaded without it",
                                (label && *label) ? label : "<unknown path>", dep.c_str(), dep.c_str());
                            xsink.handleExceptions();
                            dep_unavailable_v3 = true;
                            break;
                        }
                    }
                }
            }
        }
        if (dep_unavailable_v3) {
            rc = 2;
            break;
        }

        // Deserialize namespace tree from metadata (replaces source parsing)
        // Must set the parse context so UserVariantBase constructor can
        // call parse_get_parse_options() which reads thread-local current_pgm
        QoreAOTBinaryDeserializer deserializer;
        std::string deser_error;
        bool deser_ok = false;
        {
            ProgramRuntimeParseContextHelper pch(&xsink, *qpgm);
            if (xsink.isException()) {
                xsink.handleExceptions();
                rc = 2;
                break;
            }
            if (!deserializer.deserializeIntoProgram(*qpgm,
                    metadata, static_cast<uint32_t>(metadata_len), deser_error)) {
                printd(0, "AOT: metadata deserialization failed: %s\n",
                    deser_error.c_str());
                rc = 2;
                break;
            }
            deser_ok = true;
        }
        if (!deser_ok) {
            break;
        }

        // Advisory source staleness check.  Feature compatibility is a hard
        // error in QoreAOTBinaryDeserializer::openAndDeserializeShells()
        // before schema-dependent metadata is read.
        {
            const QoreAOTBinaryHeader& aot_hdr = deserializer.getReader().getHeader();
            if (aot_hdr.source_hash != 0 && label != nullptr) {
                std::ifstream sf(label, std::ios::binary | std::ios::ate);
                if (sf.is_open()) {
                    auto sz = sf.tellg();
                    if (sz > 0) {
                        std::vector<char> src(static_cast<size_t>(sz));
                        sf.seekg(0);
                        sf.read(src.data(), sz);
                        uint64_t live_hash = XXH64(src.data(), static_cast<size_t>(sz), 0);
                        if (live_hash != aot_hdr.source_hash) {
                            printd(0, "AOT WARNING: binary source hash mismatch for '%s' "
                                "(compiled=0x%016llx, current=0x%016llx); source has changed\n",
                                label,
                                (unsigned long long)aot_hdr.source_hash,
                                (unsigned long long)live_hash);
                        }
                    }
                }
            }
        }

        // Register pre-compiled function pointers
        QoreProgram* fallback_pgm = nullptr;
        if (num_functions > 0 && functions) {
            std::unordered_map<std::string, const QoreAOTFunc*> func_map;
            const QoreAOTFunc* toplevel_func = nullptr;
            for (int i = 0; i < num_functions; ++i) {
                if (functions[i].name && functions[i].fn_ptr) {
                    if (strcmp(functions[i].name, "_toplevel") == 0) {
                        toplevel_func = &functions[i];
                    } else {
                        func_map[functions[i].name] = &functions[i];
                    }
                }
            }

            // Register non-toplevel functions using slot maps (no IR re-lowering)
            qore_program_private* pp = qore_program_private::get(**qpgm);
            int registered = 0;
            qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);
            bool toplevel_registered = false;
            std::vector<std::string> registration_errors;
            AOTClosureRuntimeBindingMap native_closure_bindings;
            auto debug_metadata = makeAOTDebugMetadata(deserializer.getReader(),
                metadata, metadata_len);

            // Register _toplevel before other functions so setLVarsFromAOTContext()
            // populates the program LVList for closure deserialization in methods
            // that capture top-level locals.
            if (toplevel_func) {
                const QoreAOTSectionHeader* sm_sec = deserializer.getReader().findSection(
                    QoreAOTSectionType::SLOT_MAPS);
                if (sm_sec) {
                    const uint8_t* sm_base = deserializer.getReader().getSectionData(*sm_sec);
                    if (sm_base) {
                        const uint8_t* sm_ptr = sm_base;
                        const uint8_t* sm_end = sm_ptr + sm_sec->size;
                        uint32_t sm_count = QoreAOTBinaryReader::readU32(sm_ptr);
                        for (uint32_t fi = 0; fi < sm_count; ++fi) {
                            const uint8_t* entry_start = sm_ptr;
                            uint32_t entry_size = QoreAOTBinaryReader::readU32(sm_ptr);
                            const char* entry_name = deserializer.getReader().readStringRef(sm_ptr);
                            const uint8_t* entry_end = entry_start + 4 + entry_size;
                            sm_ptr = entry_start + 4;

                            if (entry_name && strcmp(entry_name, "_toplevel") == 0) {
                                std::string build_error;
                                QoreAOTContext* ctx = buildContextFromSlotMap(
                                    deserializer.getReader(), sm_ptr, sm_end,
                                    nullptr, *qpgm, *toplevel_func, "_toplevel", entry_end,
                                    nullptr, &build_error, debug_metadata, sm_base,
                                    nullptr, nullptr, &native_closure_bindings);
                                if (ctx) {
                                    pp->sb.registerPrecompiledAOTTopLevel(
                                        toplevel_func->fn_ptr, ctx);
                                    pp->sb.setLVarsFromAOTContext(ctx);
                                    ++registered;
                                    toplevel_registered = true;
                                    printd(2, "AOT v3: registered _toplevel from slot map\n");
                                } else if (!build_error.empty()) {
                                    registration_errors.push_back(std::move(build_error));
                                }
                                break;
                            }
                            sm_ptr = entry_start;
                            skipSlotMapEntry(deserializer.getReader(), sm_ptr, sm_end);
                        }
                    }
                }
            }

            // Use slot maps from the binary metadata to build contexts
            // Collect init functions for execution after slot map registration
            std::vector<AOTInitFuncExecInfo> init_func_contexts;
            printd(2, "AOT v3: calling registerAOTFunctionsFromSlotMaps with %d func_map entries, "
                "toplevel=%p, debug=%d\n", (int)func_map.size(), (void*)toplevel_func, debug);
            registerAOTFunctionsFromSlotMaps(
                deserializer.getReader(), root_ns, *qpgm, func_map, registered,
                &init_func_contexts, deserializer.getTypeResolver(), &registration_errors,
                debug_metadata, false, nullptr, &native_closure_bindings, &deserializer);
            printd(2, "AOT v3: after slot map registration: %d registered, %d remaining, "
                "%d init functions\n",
                registered, (int)func_map.size(), (int)init_func_contexts.size());

            if (!registration_errors.empty()) {
                std::string msg = makeAOTRegistrationFailureMessage(label, registered, num_functions,
                    &func_map, &registration_errors);
                xsink.raiseException("AOT-ERROR", "%s", msg.c_str());
                xsink.handleExceptions();
                rc = 2;
                break;
            }

            // Execute init functions (constant and static var initialization)
            // Requires program context for closure creation (thread_get_all_closure_vars)
            if (!init_func_contexts.empty()) {
                std::vector<AOTInitFuncDescriptor> init_descriptors;
                std::string init_error;
                readInitFuncs(metadata, static_cast<uint32_t>(metadata_len),
                    init_descriptors, init_error);
                if (!init_descriptors.empty()) {
                    ProgramThreadCountContextHelper tch(&xsink, *qpgm, false);
                    if (!xsink.isException()) {
                        executeInitFunctions(*qpgm, init_func_contexts,
                            init_descriptors, label, nullptr, nullptr,
                            /*write_shadow=*/true, /*failure_sink=*/nullptr);
                    }
                }
            }
            preInitStaticVarsInProgram(*qpgm);

            // Source fallback is intentionally not available in v3 objects. Legacy
            // FUNC_SOURCES records with fallback function names are rejected during
            // metadata deserialization; any registration gap below is a hard error.
            assert(!deserializer.hasLegacyFallbackFunctions());

            // Register the _toplevel function from slot maps
            if (toplevel_func) {
                // Find _toplevel in SLOT_MAPS section
                const QoreAOTSectionHeader* sm_sec = deserializer.getReader().findSection(
                    QoreAOTSectionType::SLOT_MAPS);

                if (!toplevel_registered && sm_sec) {
                    const uint8_t* sm_base = deserializer.getReader().getSectionData(*sm_sec);
                    if (sm_base) {
                        const uint8_t* sm_ptr = sm_base;
                        const uint8_t* sm_end = sm_ptr + sm_sec->size;
                        uint32_t sm_count = QoreAOTBinaryReader::readU32(sm_ptr);
                        for (uint32_t fi = 0; fi < sm_count; ++fi) {
                            const uint8_t* entry_start = sm_ptr;
                            uint32_t entry_size = QoreAOTBinaryReader::readU32(sm_ptr);  // consume size prefix
                            const char* entry_name = deserializer.getReader().readStringRef(sm_ptr);  // peek at name
                            const uint8_t* entry_end = entry_start + 4 + entry_size;
                            sm_ptr = entry_start + 4;  // reset: after size prefix, before name

                            if (entry_name && strcmp(entry_name, "_toplevel") == 0) {
                                std::string build_error;
                                QoreAOTContext* ctx = buildContextFromSlotMap(
                                    deserializer.getReader(), sm_ptr, sm_end,
                                    nullptr, *qpgm, *toplevel_func, "_toplevel", entry_end,
                                    nullptr, &build_error, debug_metadata, sm_base,
                                    nullptr, nullptr, &native_closure_bindings);
                                if (ctx) {
                                    pp->sb.registerPrecompiledAOTTopLevel(
                                        toplevel_func->fn_ptr, ctx);
                                    // Set LVList so doTopLevelInstantiation() can instantiate the locals
                                    pp->sb.setLVarsFromAOTContext(ctx);
                                    ++registered;
                                    toplevel_registered = true;
                                    printd(2, "AOT v3: registered _toplevel from slot map\n");
                                } else if (!build_error.empty()) {
                                    registration_errors.push_back(std::move(build_error));
                                }
                                break;
                            } else {
                                // Skip this entry
                                sm_ptr = entry_start;  // reset: before size prefix for skipSlotMapEntry
                                skipSlotMapEntry(deserializer.getReader(), sm_ptr, sm_end);
                            }
                        }
                    }
                }

                if (!toplevel_registered) {
                    printd(0, "AOT v3: _toplevel not registered from serialized slot maps\n");
                }

            }

            printd(2, "AOT v3: registered %d/%d pre-compiled functions\n", registered, num_functions);

            if (registered < num_functions) {
                std::string msg = makeAOTRegistrationFailureMessage(label, registered, num_functions,
                    &func_map, &registration_errors);
                xsink.raiseException("AOT-ERROR", "%s", msg.c_str());
                xsink.handleExceptions();
                rc = 2;
                break;
            }
        }

        // Deserialization and function registration are complete, so the inflated
        // AOT metadata pool is dead: string refs that outlive it are interned into
        // the program's string pool as they are read, and the debug metadata is
        // copied by value.  For a large program the pool dominates the startup
        // heap, so return it to the allocator now instead of pinning it for the
        // whole run.
        deserializer.releaseReaderBody();

        // Run the v3 program
        printd(2, "AOT v3: about to call qpgm->run()\n");
        QoreValue rv = qpgm->run(&xsink);
        printd(2, "AOT v3: run() returned rc=%lld exception=%d\n",
            (long long)rv.getAsBigInt(), (int)xsink.isException());
        rc = rv.getAsBigInt();
        rv.discard(&xsink);

        if (xsink.isException()) {
            rc = 3;
        }

        xsink.handleExceptions();

    } while (false);

    qore_cleanup();
    return rc;
}

extern "C" DLLEXPORT QoreStringNode* qore_aot_module_init(
    const char* source, int source_len,
    const char* label,
    int64_t parse_options,
    const char* mod_name,
    const QoreAOTFunc* functions, int num_functions
) {
    ExceptionSink xsink;
    ExceptionSink wsink;

    QoreProgram* parent_pgm = getProgram();

    // Use a local variable for the program being created.  Loading dependencies
    // via runTimeLoadModule() can trigger nested calls to qore_aot_module_init()
    // for other AOT modules, which would overwrite the global aot_module_pgm.
    QoreProgram* local_pgm = new QoreProgram(parse_options);
    if (mod_name && *mod_name && qore_program_private::get(*local_pgm)->addUserFeature(mod_name)) {
        QoreStringNode* err = new QoreStringNodeMaker(
            "AOT module init error: feature '%s' is already loaded in this Program container", mod_name);
        local_pgm->waitForTerminationAndDeref(nullptr);
        return err;
    }

    // Set JIT execution mode so functions without pre-compiled code will JIT on demand
    local_pgm->setExecMode(QEM_JIT);

    // Set script path from the label so get_script_dir() returns the module's source
    // directory during parsing.  This is needed for modules that read files at parse time
    // (e.g., DataProvider loads qore-q-logo.svg via get_script_dir() + filename).
    if (label) {
        local_pgm->setScriptPath(label);
    }

    inheritAOTModulePathLists(local_pgm, parent_pgm);

    // Extract dependencies from source and load/import their namespaces
    // Note: The init function is now called with the module manager lock unlocked
    // (via ModuleLoadMapHelper), so we can safely load dependencies here.
    std::vector<std::string> reexport_deps;
    std::vector<std::string> deps = extractDependencies(source, source_len, &reexport_deps);
    for (const std::string& dep : deps) {
        // Try to load the module (it may already be loaded, which is fine)
        int rc = MM.runTimeLoadModule(&xsink, dep.c_str(), local_pgm);
        if (rc < 0 || xsink) {
            xsink.clear();
            // A genuinely-missing dependency is a hard error (this init path runs with the
            // module-loading mutex unlocked, so use the locking lookup); only a circular/in-progress
            // dependency, still registered in the module map, is tolerated.
            if (aotRequiredDepUnavailable(dep.c_str())) {
                QoreStringNode* err = new QoreStringNodeMaker(
                    "AOT module '%s' (%s) requires module '%s', which could not be loaded; the module "
                    "was AOT-compiled against '%s' (its compiled code references that module's symbols) "
                    "and cannot be loaded without it",
                    mod_name ? mod_name : "<unknown>",
                    (label && *label) ? label : "<unknown path>", dep.c_str(), dep.c_str());
                local_pgm->waitForTerminationAndDeref(nullptr);
                return err;
            }
            // Circular dependency or other in-progress load - tolerate and continue.
            // The types might be resolved later when the requiring script is parsed.
        }
    }

    // Set up module context for the parser (must be QoreUserModuleDefContextHelper
    // because the parser static_casts to it)
    // Note: Do NOT call setNameInit() here - the scanner calls it when it parses
    // the "module Name" declaration, and calling it twice triggers an assertion.
    QoreUserModuleDefContextHelper mod_ctx(mod_name, label, local_pgm, xsink);
    {
        // Strip %requires directives from embedded source to avoid deadlock.
        // The module manager holds a lock when calling this init function, and
        // parsing %requires would try to acquire the same lock.
        // Note: We've already imported the dependency namespaces above.
        std::string stripped_src = stripRequiresDirectives(source, source_len, local_pgm);

        printd(2, "AOT module '%s': source_len=%d stripped_len=%d deps=%d\n",
            mod_name, source_len, (int)stripped_src.size(), (int)deps.size());

        // Split combined source at file boundary markers and parse each segment
        // separately with parsePending().  Split modules (directory-based) embed
        // "# __AOT_FILE_BREAK__\n" between the main .qm source and each .qc file.
        // Each parsePending() call creates a fresh scanner with line numbers starting
        // from 1, which is critical for correct BCANode location tracking.
        const char* marker = "# __AOT_FILE_BREAK__\n";
        const size_t marker_len = strlen(marker);
        std::vector<std::string> segments;
        {
            size_t pos = 0;
            size_t found;
            while ((found = stripped_src.find(marker, pos)) != std::string::npos) {
                segments.push_back(stripped_src.substr(pos, found - pos));
                pos = found + marker_len;
            }
            // Last segment (or entire source if no markers found)
            segments.push_back(stripped_src.substr(pos));
        }

        if (segments.size() > 1) {
            // Split module: parse each segment separately
            printd(2, "AOT module '%s': %d source segments\n", mod_name, (int)segments.size());
            for (size_t i = 0; i < segments.size(); ++i) {
                if (segments[i].empty()) {
                    continue;
                }
                local_pgm->parsePending(segments[i].c_str(), label, &xsink, &wsink, QP_WARN_DEFAULT);
                if (xsink.isException()) {
                    break;
                }
            }
            if (!xsink.isException()) {
                local_pgm->parseCommit(&xsink, &wsink, QP_WARN_DEFAULT);
            }
        } else {
            // Single-file module: parse as one blob
            local_pgm->parse(stripped_src.c_str(), label, &xsink, &wsink, QP_WARN_DEFAULT);
        }

        mod_ctx.close();
    }

    if (wsink.isException()) {
        wsink.handleWarnings();
    }

    if (xsink.isException()) {
        QoreStringNode* err = new QoreStringNode("AOT module parse error: ");
        // Get error description
        QoreValue ex_err = xsink.getExceptionErr();
        QoreValue ex_desc = xsink.getExceptionDesc();
        QoreValue ex_arg = xsink.getExceptionArg();
        if (ex_err.getType() == NT_STRING) {
            QoreStringValueHelper ex_err_str(ex_err);
            err->concat(ex_err_str->c_str());
        } else {
            err->concat("unknown parse error");
        }
        if (ex_desc.getType() == NT_STRING) {
            err->concat(", desc: ");
            QoreStringValueHelper ex_desc_str(ex_desc);
            err->concat(ex_desc_str->c_str());
        }
        if (ex_arg.getType() == NT_STRING) {
            err->concat(", arg: ");
            QoreStringValueHelper ex_arg_str(ex_arg);
            err->concat(ex_arg_str->c_str());
        }
        xsink.clear();
        local_pgm->waitForTerminationAndDeref(nullptr);
        return err;
    }

    // Run module init closure if present (registers factories, etc.)
    if (mod_ctx.hasInit()) {
        if (mod_ctx.init(*local_pgm, xsink)) {
            QoreStringNode* err = new QoreStringNode("AOT module init closure error");
            if (xsink.isException()) {
                QoreValue ex_desc = xsink.getExceptionDesc();
                if (ex_desc.getType() == NT_STRING) {
                    err->concat(": ");
                    QoreStringValueHelper ex_desc_str(ex_desc);
                    err->concat(ex_desc_str->c_str());
                }
                xsink.clear();
            }
            local_pgm->waitForTerminationAndDeref(nullptr);
            return err;
        }
    }

    // Register pre-compiled AOT functions on the module's namespace tree
    if (num_functions > 0 && functions) {
        std::unordered_map<std::string, const QoreAOTFunc*> func_map;
        for (int i = 0; i < num_functions; ++i) {
            if (functions[i].name && functions[i].fn_ptr) {
                func_map[functions[i].name] = &functions[i];
            }
        }

        qore_program_private* pp = qore_program_private::get(*local_pgm);
        int registered = 0;
        qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);
        registerAOTFunctionsInNamespace(root_ns, local_pgm, func_map, registered);

        printd(1, "AOT module '%s': registered %d/%d pre-compiled functions\n",
            mod_name, registered, num_functions);
    }

    // Store per-module state so ns_init can find the correct program.
    // Also update globals for modules that call ns_init during their own init
    // before being stored in the map.
    {
        AutoLocker aot_state_al(get_aot_module_state_lock());
        aot_module_pgm = local_pgm;
        aot_module_name = mod_name;
        aot_module_path = label ? label : "";
        aot_module_funcs = functions;
        aot_module_num_funcs = num_functions;

        AotModuleState state;
        state.pgm = local_pgm;
        state.funcs = functions;
        state.num_funcs = num_functions;
        state.reexport_deps = std::move(reexport_deps);
        state.path = label ? label : "";
        aot_module_map[mod_name] = std::move(state);
    }

    return nullptr;  // success
}

static void qore_aot_module_ns_init_impl(QoreNamespace* root_ns, QoreNamespace* qore_ns,
        ExceptionSink* external_xsink) {
    printd(5, "AOT ns_init called: root_ns=%p qore_ns=%p\n", (void*)root_ns, (void*)qore_ns);

    // Applying an AOT module to a Program walks shared module namespace trees,
    // loads reexported modules, updates per-module init state, and executes
    // generated init functions.  None of that is safe to interleave with the
    // same work for another Program using the same shared AOT module metadata.
    QoreModuleLoadLockHelper aot_module_al;

    ExceptionSink local_xsink;
    ExceptionSink& xsink = external_xsink ? *external_xsink : local_xsink;
    auto raise_ns_init_error = [&](const std::string& msg) {
        if (external_xsink) {
            xsink.raiseException("MODULE-LOAD-ERROR", "%s", msg.c_str());
        } else {
            printd(0, "%s\n", msg.c_str());
        }
    };

    // Look up the correct module program from the per-module map.
    // When QoreBuiltinModule::addToProgramImpl calls module_ns_init, it has set up
    // QoreModuleContextHelper with the module's name. We use get_module_context() to
    // find which module's namespace to merge.
    const char* mod_name = nullptr;
    const char* mod_path = nullptr;
    QoreProgram* mod_pgm = nullptr;
    const std::vector<std::string>* reexport_deps = nullptr;
    std::string mod_name_storage;
    std::string mod_path_storage;
    std::vector<std::string> reexport_deps_storage;

    {
        AutoLocker aot_state_al(get_aot_module_state_lock());
        QoreModuleContext* ctx = get_module_context();
        if (ctx) {
            mod_name = ctx->getName();
            auto it = aot_module_map.find(mod_name);
            if (it != aot_module_map.end()) {
                mod_pgm = it->second.pgm;
                reexport_deps_storage = it->second.reexport_deps;
                reexport_deps = &reexport_deps_storage;
                if (!it->second.path.empty()) {
                    mod_path_storage = it->second.path;
                    mod_path = mod_path_storage.c_str();
                }
            }
        }

        if (aotInitTraceEnabled()) {
            fprintf(stderr, "[aot-init] ns_init entry ctx_mod=%s fallback_mod=%s map_size=%zu mod_pgm=%p path=%s\n",
                mod_name ? mod_name : "<none>", aot_module_name.c_str(),
                aot_module_map.size(), (void*)mod_pgm, mod_path ? mod_path : "<none>");
        }

        // Fall back to the current module being initialized (for the module's own registration)
        if (!mod_pgm) {
            mod_pgm = aot_module_pgm;
            if (!mod_name) {
                mod_name_storage = aot_module_name;
                mod_name = mod_name_storage.c_str();
            }
            if (!aot_module_path.empty()) {
                mod_path_storage = aot_module_path;
                mod_path = mod_path_storage.c_str();
            }
        }
    }

    if (!mod_pgm) {
        printd(5, "AOT module ns_init: no program for '%s'!\n", mod_name ? mod_name : "(unknown)");
        return;
    }

    struct ModuleContextSuspendHelper {
        QoreModuleContext* old;

        ModuleContextSuspendHelper() : old(get_module_context()) {
            set_module_context(nullptr);
        }

        ~ModuleContextSuspendHelper() {
            set_module_context(old);
        }
    } module_context_suspender;

    // Get the module program's root namespace
    RootQoreNamespace* mod_root = mod_pgm->getRootNS();
    if (!mod_root) {
        raise_ns_init_error(std::string("AOT module ns_init '")
            + (mod_name ? mod_name : "(unknown)") + "': no root namespace");
        return;
    }

    // Use the same namespace merge mechanism as user modules to properly handle
    // class hierarchy references. Simple copy() doesn't work because base class
    // pointers would still reference classes from the AOT module's program.
    RootQoreNamespace* target_root = static_cast<RootQoreNamespace*>(root_ns);

    // Set up the target program as the current program context.
    // scanMergeCommittedNamespace() calls parse_check_parse_option() which requires
    // current_pgm to be set. QoreBuiltinModule::addToProgramImpl() only sets
    // call_program_context (via ProgramCallContextHelper), not current_pgm.
    // Use the same approach as QoreUserModule::addToProgramImpl().
    QoreProgram* tpgm = getProgram();
    ProgramThreadCountContextHelper ptcch(&xsink, tpgm, false);
    if (xsink) {
        if (!external_xsink) {
            xsink.handleExceptions();
        }
        return;
    }

    // Load reexported dependencies into the target program BEFORE namespace merge.
    // When a source module has %requires(reexport) logger_bin, the reexport() mechanism
    // loads logger_bin directly into importing programs. For AOT binary modules, we must
    // replicate this behavior because scanMergeCommittedNamespace only copies user-public
    // items — system classes from binary modules (e.g., LoggerLevel from logger_bin)
    // are NOT user-public and would be skipped by the namespace merge.
    if (reexport_deps && !reexport_deps->empty()) {
        for (const std::string& dep : *reexport_deps) {
            if (!qore_program_private::get(*tpgm)->hasFeature(dep.c_str())) {
                printd(5, "AOT module ns_init '%s': loading reexported dep '%s' into target program\n",
                    mod_name, dep.c_str());
                // Use QMM.loadModuleForReexport() (like QoreAbstractModule::reexport()
                // uses QMM.loadModuleIntern()) instead of MM.runTimeLoadModule() which
                // would try to acquire the module manager mutex that is already held by
                // parseLoadModule/runTimeLoadModule
                QMM.loadModuleForReexport(xsink, dep.c_str(), tpgm);
                if (xsink) {
                    printd(0, "AOT module ns_init '%s': WARNING - failed to load reexported dep '%s'\n",
                        mod_name, dep.c_str());
                    xsink.clear();
                }
            }
        }
    }

    printd(5, "AOT module ns_init '%s': starting merge\n", mod_name);

    QoreModuleContext qmc(mod_name, qore_root_ns_private::get(*target_root), xsink);
    {
        // Fence runtime readers of the target namespace for the merge transaction only: until the
        // indexes are rebuilt below, another thread resolving a name here can see merged items that
        // are not yet indexed.  The deferred initialization further down must run outside this
        // scope, because it waits for the per-module shadow builder whose owner needs to read this
        // same namespace.
        RuntimeNamespaceMergeLocker rnml(*target_root);

        printd(5, "AOT module ns_init '%s': calling scanMergeCommittedNamespace\n", mod_name);
        qore_root_ns_private::scanMergeCommittedNamespace(*target_root, *mod_root, qmc);
        printd(5, "AOT module ns_init '%s': scanMergeCommittedNamespace done\n", mod_name);

        if (qmc.hasError()) {
            printd(5, "AOT module ns_init '%s': error during namespace scan/merge\n", mod_name);
            qmc.rollback();
            return;
        }

        printd(5, "AOT module ns_init '%s': calling copyMergeCommittedNamespace\n", mod_name);
        qore_root_ns_private::copyMergeCommittedNamespace(*target_root, *mod_root);
        printd(5, "AOT module ns_init '%s': copyMergeCommittedNamespace done\n", mod_name);
    }

    // Check for exceptions during merge operations
    if (xsink) {
        if (!external_xsink) {
            const QoreValue err = xsink.getExceptionErr();
            QoreStringValueHelper err_str(err);
            printd(0, "AOT module ns_init '%s': exception during namespace merge: %s\n",
                mod_name, err.getType() == NT_STRING ? err_str->c_str() : "(unknown)");
            xsink.clear();
        }
        return;
    }

    printd(5, "AOT module ns_init '%s': merge complete\n", mod_name);

    // Execute deferred init functions for constants/static vars.
    // This must happen AFTER namespace merge so target entries and class
    // hierarchies are committed. Contexts resolve module symbols against the
    // shared shadow Program, while generated locals and deferred-init state
    // belong to the importing Program. Every merge then runs a short
    // cross-module fixpoint for constants that were waiting on another module.
    if (mod_name) {
        qore_program_private* target_pp = qore_program_private::get(*tpgm);
        {
            AutoLocker aot_state_al(get_aot_module_state_lock());
            auto it = aot_module_map.find(mod_name);
            if (it != aot_module_map.end()) {
                target_pp->merged_aot_modules.insert(mod_name);
            }
        }

        AOTModuleInitRunResult init_result =
            runAOTModuleInitForProgram(mod_name, tpgm, xsink);
        if (xsink) {
            if (!external_xsink) {
                xsink.handleExceptions();
            }
            return;
        }
        if (init_result.hard_error) {
            raise_ns_init_error(std::string("AOT ns_init '")
                + (mod_name ? mod_name : "(unknown)") + "': " + init_result.error);
            return;
        }
        retryPendingAOTModuleInitsForProgram(tpgm, xsink);
        if (xsink) {
            if (!external_xsink) {
                xsink.handleExceptions();
            }
            return;
        }
    }
    preInitStaticVarsInProgram(tpgm);
}

extern "C" DLLEXPORT void qore_aot_module_ns_init(QoreNamespace* root_ns, QoreNamespace* qore_ns) {
    qore_aot_module_ns_init_impl(root_ns, qore_ns, nullptr);
}

extern "C" DLLEXPORT void qore_aot_module_ns_init_v2(QoreNamespace* root_ns, QoreNamespace* qore_ns,
        ExceptionSink* xsink) {
    qore_aot_module_ns_init_impl(root_ns, qore_ns, xsink);
}

extern "C" DLLEXPORT void qore_aot_module_delete() {
    // Use the module context name to clean up only the module being unloaded
    // (set by QoreBuiltinModule::~QoreBuiltinModule via QoreModuleNameContextHelper)
    const char* mod_name = get_module_context_name();
    QoreProgram* pgm = nullptr;
    if (mod_name) {
        {
            AutoLocker aot_state_al(get_aot_module_state_lock());
            auto it = aot_module_map.find(mod_name);
            if (it != aot_module_map.end()) {
                pgm = it->second.pgm;
                aot_module_map.erase(it);
            }
            // Clear per-module state if it matches the module being deleted
            if (aot_module_name == mod_name) {
                aot_module_pgm = nullptr;
                aot_module_name.clear();
                aot_module_path.clear();
                aot_module_funcs = nullptr;
                aot_module_num_funcs = 0;
            }
        }
        if (pgm) {
            // Clear namespace data before deref to release cross-program
            // closure references. Without this, the module's ClosureVarValues
            // may reference objects from the main program that have already
            // been freed during global cleanup, causing dangling pointer access.
            ExceptionSink xsink;
            qore_program_private::get(*pgm)->waitForTerminationAndClear(&xsink);
            pgm->waitForTerminationAndDeref(&xsink);
        }
    } else {
        // Fallback: no module context — clean up all (shutdown path)
        std::vector<QoreProgram*> programs;
        {
            AutoLocker aot_state_al(get_aot_module_state_lock());
            for (auto& entry : aot_module_map) {
                if (entry.second.pgm) {
                    programs.push_back(entry.second.pgm);
                }
            }
            aot_module_map.clear();
            aot_module_pgm = nullptr;
            aot_module_name.clear();
            aot_module_path.clear();
            aot_module_funcs = nullptr;
            aot_module_num_funcs = 0;
        }
        for (QoreProgram* p : programs) {
            p->waitForTerminationAndDeref(nullptr);
        }
    }
}

//! Returns true if \a name is registered as an AOT user module in aot_module_map.
/** AOT user modules are source Qore modules compiled to .qmod binary form. They
    export user-public symbols (functions, classes, hashdecls) that must be re-merged
    into each program that %requires them, just like source (.qm) user modules. They
    are therefore tracked in userFeatureList, not the builtin featureList. This helper
    lets the binary-module load path distinguish AOT user modules from true builtin
    C++ modules (e.g. yaml, json, reflection) so that featureList propagation to
    child programs (via qore_program_private::runtimeImportSystemApi and setParent)
    does not hide them from the child's %requires.
*/
DLLLOCAL bool qore_is_aot_user_module(const char* name) {
    if (!name) {
        return false;
    }
    AutoLocker aot_state_al(get_aot_module_state_lock());
    return aot_module_map.find(name) != aot_module_map.end();
}

DLLLOCAL QoreProgram* qore_aot_get_module_pgm(const char* name) {
    if (!name) {
        return nullptr;
    }
    AutoLocker aot_state_al(get_aot_module_state_lock());
    auto it = aot_module_map.find(name);
    if (it == aot_module_map.end()) {
        return nullptr;
    }
    return it->second.pgm;
}

DLLLOCAL int qore_aot_initialize_module(const char* name, ExceptionSink& xsink) {
    if (!name) {
        return 0;
    }

    QoreProgram* pgm = nullptr;
    {
        AutoLocker aot_state_al(get_aot_module_state_lock());
        auto it = aot_module_map.find(name);
        if (it == aot_module_map.end() || !it->second.pgm) {
            return 0;
        }
        pgm = it->second.pgm;
        qore_program_private::get(*pgm)->merged_aot_modules.insert(name);
    }

    AOTModuleInitRunResult result = runAOTModuleInitForProgram(name, pgm, xsink);
    if (xsink) {
        return -1;
    }
    if (result.hard_error) {
        xsink.raiseException("MODULE-LOAD-ERROR", "AOT module '%s' initialization failed: %s", name,
            result.error.c_str());
        return -1;
    }
    retryPendingAOTModuleInitsForProgram(pgm, xsink);
    return xsink ? -1 : 0;
}

DLLLOCAL void qore_aot_clear_all_module_namespace_data(ExceptionSink& xsink) {
    std::vector<QoreProgram*> programs;
    {
        AutoLocker aot_state_al(get_aot_module_state_lock());
        for (auto& entry : aot_module_map) {
            if (entry.second.pgm) {
                programs.push_back(entry.second.pgm);
            }
        }
    }
    for (QoreProgram* pgm : programs) {
        pgm->waitForTermination();
        qore_program_private::get(*pgm)->clearNamespaceData(&xsink);
    }
}

extern "C" DLLEXPORT QoreStringNode* qore_aot_module_init_v2(
    const uint8_t* metadata, int metadata_len,
    const char* label,
    int64_t parse_options,
    const char* mod_name,
    const QoreAOTFunc* functions, int num_functions
) {
    ExceptionSink xsink;

    QoreProgram* parent_pgm = getProgram();

    // Use a local variable for the program being created (see qore_aot_module_init
    // for explanation of nested init overwrite issue)
    QoreProgram* local_pgm = new QoreProgram(parse_options);
    if (mod_name && *mod_name && qore_program_private::get(*local_pgm)->addUserFeature(mod_name)) {
        QoreStringNode* err = new QoreStringNodeMaker(
            "AOT module init error: feature '%s' is already loaded in this Program container", mod_name);
        local_pgm->waitForTerminationAndDeref(nullptr);
        return err;
    }

    // Set JIT execution mode
    local_pgm->setExecMode(QEM_JIT);

    // Set script path from the label so `get_script_dir()` returns the
    // module's .qmod directory when init functions execute.  Mirrors v1's
    // setScriptPath(label) at the equivalent construction site; without
    // this, resource-file constants like
    //     const FooLogo = File::readTextFile(get_script_dir() + "/foo.svg");
    // fail to evaluate and cascade into AOT-PENDING-CONSTANT errors
    // whenever any downstream code references the pending constant.
    if (label) {
        local_pgm->setScriptPath(label);
    }

    // Apply inherited plus compiled-in module path lists BEFORE loading
    // dependencies; source modules inherit the requiring Program's search
    // surface before their own directives are applied.
    {
        std::vector<std::string> prepended, appended;
        std::string mp_error;
        if (readModulePathLists(metadata, static_cast<uint32_t>(metadata_len),
                prepended, appended, mp_error)) {
            inheritAOTModulePathLists(local_pgm, parent_pgm, prepended, appended);
        } else {
            inheritAOTModulePathLists(local_pgm, parent_pgm);
        }
    }

    {
        std::string cmd_error;
        if (!applyAOTModuleCommandsToProgram(local_pgm, metadata,
                static_cast<uint32_t>(metadata_len), label, cmd_error)) {
            QoreStringNode* err = new QoreStringNodeMaker(
                "AOT module-command replay error for module '%s' (%s): %s",
                mod_name ? mod_name : "<unknown>",
                (label && *label) ? label : "<unknown path>",
                cmd_error.c_str());
            local_pgm->waitForTerminationAndDeref(nullptr);
            return err;
        }
    }

    // Load dependencies from serialized metadata BEFORE deserializing namespace tree.
    // Dependencies must be loaded first because deserialization may need to resolve
    // base classes, types, and other references from dependency modules.
    std::vector<std::string> deps;
    std::string dep_error;
    if (!readDependencies(metadata, static_cast<uint32_t>(metadata_len), deps, dep_error)) {
        QoreStringNode* err = new QoreStringNodeMaker(
            "AOT module dependency read error for module '%s' (%s): %s",
            mod_name ? mod_name : "<unknown>",
            (label && *label) ? label : "<unknown path>",
            dep_error.c_str());
        local_pgm->waitForTerminationAndDeref(nullptr);
        return err;
    }

    // Read reexported module names from metadata
    std::vector<std::string> reexport_deps;
    std::string reexport_error;
    if (!readReexportModules(metadata, static_cast<uint32_t>(metadata_len), reexport_deps, reexport_error)) {
        printd(0, "AOT module v2 '%s': WARNING - failed to read reexport modules: %s\n",
            mod_name, reexport_error.c_str());
        // Non-fatal — continue without reexport info
    }

    // Load each dependency module into this program.
    // runTimeLoadModule will call addToProgram which imports the namespace.
    //
    // The dependency list reflects the modules loaded when the .qmod was compiled (including
    // optional %try-module modules whose code was baked into the binary), so a genuinely-missing
    // dependency is a hard error — see the matching loop in qore_aot_module_init_v3 for details.
    for (const std::string& dep : deps) {
        printd(5, "AOT module v2 '%s': loading dependency '%s'\n", mod_name, dep.c_str());
        int rc = MM.runTimeLoadModule(&xsink, dep.c_str(), local_pgm);
        if (rc < 0 || xsink) {
            xsink.clear();
            if (aotRequiredDepUnavailable(dep.c_str())) {
                // Genuinely missing (not a circular/in-progress load) — hard error.
                printd(5, "AOT module v2 '%s': required dependency '%s' is unavailable\n",
                    mod_name, dep.c_str());
                QoreStringNode* err = new QoreStringNodeMaker(
                    "AOT module '%s' (%s) requires module '%s', which could not be loaded; the module "
                    "was AOT-compiled against '%s' (its compiled code references that module's symbols) "
                    "and cannot be loaded without it",
                    mod_name ? mod_name : "<unknown>",
                    (label && *label) ? label : "<unknown path>", dep.c_str(), dep.c_str());
                local_pgm->waitForTerminationAndDeref(nullptr);
                return err;
            }
            // Circular dependency or other in-progress load - tolerate and continue.
            printd(5, "AOT module v2 '%s': dependency '%s' load tolerated (rc=%d, module present)\n",
                mod_name, dep.c_str(), rc);
        }
    }

    printd(5, "AOT module v2 '%s': loaded %d dependencies\n", mod_name, (int)deps.size());

    // Deserialize namespace tree from metadata (replaces source parsing).
    // The metadata contains complete class/namespace structure.  Functions that were
    // successfully compiled to native code will be registered below.  Functions that
    // couldn't be compiled are present as stubs in the metadata.
    // Note: Modules should be compiled with source-first QORE_MODULE_DIR so that all
    // dependency classes/methods are complete during metadata serialization.
    printd(5, "AOT module v2 '%s': starting namespace deserialization\n", mod_name);
    QoreAOTBinaryDeserializer deserializer;
    std::string deser_error;
    {
        // parse_ctx_failed: set parse context threw; deser_failed: metadata read rejected.
        // Both must emit the error OUTSIDE the ProgramRuntimeParseContextHelper scope —
        // waitForTerminationAndDeref blocks until parse_count==0, and pch owns parse_count
        // until it goes out of scope. Calling wait* while pch is live deadlocks.
        bool parse_ctx_failed = false;
        bool deser_failed = false;
        {
            // Set parse context so UserVariantBase constructor can call
            // parse_get_parse_options() which reads thread-local current_pgm
            ProgramRuntimeParseContextHelper pch(&xsink, local_pgm);
            if (xsink.isException()) {
                xsink.clear();
                parse_ctx_failed = true;
            } else if (!deserializer.deserializeIntoProgram(local_pgm,
                    metadata, static_cast<uint32_t>(metadata_len), deser_error)) {
                deser_failed = true;
            }
        }
        if (parse_ctx_failed) {
            local_pgm->waitForTerminationAndDeref(nullptr);
            return new QoreStringNode("AOT module v2: failed to set parse context");
        }
        if (deser_failed) {
            QoreStringNode* err = new QoreStringNode("AOT module metadata deserialization error: ");
            err->concat(deser_error.c_str());
            local_pgm->waitForTerminationAndDeref(nullptr);
            return err;
        }
    }

    // Register pre-compiled AOT functions using slot maps (v2 uses metadata
    // deserialization — no AST available, must use slot map path)
    if (num_functions > 0 && functions) {
        std::unordered_map<std::string, const QoreAOTFunc*> func_map;
        for (int i = 0; i < num_functions; ++i) {
            if (functions[i].name && functions[i].fn_ptr) {
                func_map[functions[i].name] = &functions[i];
            }
        }

        qore_program_private* pp = qore_program_private::get(*local_pgm);
        int registered = 0;
        qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);
        std::vector<std::string> registration_errors;
        auto debug_metadata = makeAOTDebugMetadata(deserializer.getReader(),
            metadata, metadata_len);
        registerAOTFunctionsFromSlotMaps(deserializer.getReader(), root_ns,
            local_pgm, func_map, registered, nullptr, deserializer.getTypeResolver(),
            &registration_errors, debug_metadata, false, nullptr, nullptr, &deserializer);

        if (!registration_errors.empty()) {
            std::string msg = makeAOTRegistrationFailureMessage(mod_name, registered, num_functions,
                &func_map, &registration_errors);
            local_pgm->waitForTerminationAndDeref(nullptr);
            return new QoreStringNode(msg.c_str());
        }

        printd(1, "AOT module v2 '%s': registered %d/%d pre-compiled functions\n",
            mod_name, registered, num_functions);
        if (registered < num_functions) {
            std::string msg = makeAOTRegistrationFailureMessage(mod_name, registered, num_functions,
                &func_map, &registration_errors);
            local_pgm->waitForTerminationAndDeref(nullptr);
            return new QoreStringNode(msg.c_str());
        }
    }

    // Store per-module state so ns_init can find the correct program.
    // Also update globals for early ns_init entry.
    {
        AutoLocker aot_state_al(get_aot_module_state_lock());
        aot_module_pgm = local_pgm;
        aot_module_name = mod_name;
        aot_module_path = label ? label : "";
        aot_module_funcs = functions;
        aot_module_num_funcs = num_functions;

        AotModuleState state;
        state.pgm = local_pgm;
        state.funcs = functions;
        state.num_funcs = num_functions;
        state.reexport_deps = std::move(reexport_deps);
        state.path = label ? label : "";
        aot_module_map[mod_name] = std::move(state);
    }

    return nullptr;  // success
}

//! Navigate a namespace path like "Ns1::Ns2" from root to find the target namespace
static qore_ns_private* findNamespaceByPath(qore_ns_private* root, const std::string& path) {
    if (path.empty()) {
        return root;
    }

    qore_ns_private* current = root;
    size_t start = 0;
    while (start < path.size()) {
        size_t sep = path.find("::", start);
        std::string component;
        if (sep == std::string::npos) {
            component = path.substr(start);
            start = path.size();
        } else {
            component = path.substr(start, sep - start);
            start = sep + 2;
        }

        // Search child namespaces
        bool found = false;
        for (auto ni = current->nsl.nsmap.begin(); ni != current->nsl.nsmap.end(); ++ni) {
            if (ni->first == component && ni->second) {
                current = qore_ns_private::get(*ni->second);
                found = true;
                break;
            }
        }
        if (!found) {
            return nullptr;
        }
    }
    return current;
}

//! Walk every class in the namespace tree and force evalInit() on eligible AOT static vars.
/** Mirrors what source-mode parseCommitRuntimeInit does. AOT-deserialized
    QoreVarInfo entries never carry an `exp` (the init expression is compiled
    separately as a STATIC_VAR init function). For vars that have a STATIC_VAR
    init function, the init function already ran (pass 0) and set eval_init=true.
    For vars with no init expression at all, eval_init is still false here.

    Source entries can coexist in the target Program while an AOT dependency is
    loaded. They are ignored until their normal parse initialization completes.

    If we leave eval_init=false, the FIRST user-script read of the static var
    triggers `getReferencedValue` → `evalInit` → `init()` → `val.set(typeInfo)`
    which silently RESETS any value previously written into the slot (e.g.
    by a MODULE_INIT closure assigning `Storage::obj = new Thing(...)`).

    Calling evalInit here when eval_init=false runs the empty-exp branch
    (`init()`), which sets up container types (empty list/hash for non-nullable
    list/hash) and marks eval_init=true. Subsequent reads return the stored
    value untouched.
*/
static void preInitStaticVarsInNamespace(qore_ns_private* ns, ExceptionSink* xsink) {
    if (!ns) {
        return;
    }
    ClassListIterator cli(ns->classList);
    while (cli.next()) {
        QoreClass* qc = cli.get();
        if (!qc) {
            continue;
        }
        qore_class_private* qcp = qore_class_private::get(*qc);
        if (qcp->sys) {
            continue;
        }
        for (auto& vi : qcp->vars.member_list) {
            if (!vi.second || !vi.second->isParseInitDone() || vi.second->eval_init) {
                continue;
            }
            if (vi.second->exp) {
                // Source-program static var initializers may still be parse-time
                // ASTs while an AOT dependency is being loaded.  They must be
                // evaluated by the normal parse/runtime path, not as a side
                // effect of AOT module namespace init.
                continue;
            }
            // Empty-exp path inside evalInit just calls init() (set typeinfo,
            // create empty container for non-nullable hash/list) and marks
            // eval_init=true. AOT-deserialized vars without init functions do
            // not carry exp; those are the only vars this pass should touch.
            vi.second->evalInit(vi.first, xsink);
            if (xsink && xsink->isException()) {
                xsink->clear();
            }
        }
    }
    for (auto ni = ns->nsl.nsmap.begin(); ni != ns->nsl.nsmap.end(); ++ni) {
        QoreNamespace* child = ni->second;
        if (child) {
            preInitStaticVarsInNamespace(qore_ns_private::get(*child), xsink);
        }
    }
}

static void preInitStaticVarsInProgram(QoreProgram* pgm) {
    if (!pgm) {
        return;
    }
    ExceptionSink local_xs;
    qore_program_private* pp = qore_program_private::get(*pgm);
    preInitStaticVarsInNamespace(qore_ns_private::get(*pp->RootNS), &local_xs);
    local_xs.clear();
}

//! Looks up the target and shadow ConstantEntry for one constant init descriptor
static void aotFindInitConstantEntries(const AOTInitFuncDescriptor& desc, QoreProgram* pgm,
        QoreProgram* shadow_pgm, ConstantEntry*& target_ce, ConstantEntry*& shadow_ce) {
    target_ce = nullptr;
    shadow_ce = nullptr;
    qore_program_private* pp = pgm ? qore_program_private::get(*pgm) : nullptr;
    qore_program_private* shadow_pp = shadow_pgm ? qore_program_private::get(*shadow_pgm) : nullptr;
    auto find_class_constant = [&desc](qore_program_private* p) -> ConstantEntry* {
        if (!p) {
            return nullptr;
        }
        const qore_ns_private* found_ns = nullptr;
        const QoreClass* qc = qore_root_ns_private::runtimeFindClass(*p->RootNS, desc.ns_path.c_str(), found_ns);
        if (!qc) {
            return nullptr;
        }
        return qore_class_private::get(*const_cast<QoreClass*>(qc))->constlist.findEntry(desc.item_name.c_str());
    };
    auto find_ns_constant = [&desc](qore_program_private* p) -> ConstantEntry* {
        if (!p) {
            return nullptr;
        }
        qore_ns_private* ns = findNamespaceByPath(qore_ns_private::get(*p->RootNS), desc.ns_path);
        return ns ? ns->constant.findEntry(desc.item_name.c_str()) : nullptr;
    };

    if (desc.target_type == AOTCompiledInitFunc::CLASS_CONSTANT) {
        target_ce = find_class_constant(pp);
        shadow_ce = find_class_constant(shadow_pp);
        return;
    }
    target_ce = find_ns_constant(pp);
    shadow_ce = find_ns_constant(shadow_pp);
}

//! Stores the result of a constant init function in the target and shadow ConstantEntry
/** Shared by the module-load init loop and by lazy initialization from the first read of a
    still-pending constant, so both paths apply identical store, shadow, and ownership rules.

    @return 0 if the value was stored, -1 if neither the target nor the shadow constant was found
*/
static int aotStoreConstantInitResult(const AOTInitFuncDescriptor& desc, QoreValue result, QoreProgram* pgm,
        QoreProgram* shadow_pgm, bool write_shadow, const char* mod_name,
        const std::function<void(ConstantEntry*)>& remember, ExceptionSink& xsink) {
    ConstantEntry* target_ce = nullptr;
    ConstantEntry* shadow_ce = nullptr;
    aotFindInitConstantEntries(desc, pgm, shadow_pgm, target_ce, shadow_ce);
    if (!target_ce && !shadow_ce) {
        printd(0, "AOT init: constant '%s::%s' not found in target or shadow\n",
            desc.ns_path.c_str(), desc.item_name.c_str());
        return -1;
    }
    if (aotInitTraceEnabled()) {
        fprintf(stderr, "[aot-init] store constant module=%s ns=%s item=%s result=%s "
            "target_ce=%p shadow_ce=%p\n",
            mod_name ? mod_name : "<none>", desc.ns_path.c_str(), desc.item_name.c_str(),
            result.getTypeName(), (void*)target_ce, (void*)shadow_ce);
    }
    // Populate saved_val; pending AOT constant shells keep val as a RuntimeConstantRefNode to preserve
    // source-mode parse semantics.  The shadow module program is shared by all target Programs, so keep its
    // first initialized value canonical: re-importing the same AOT user module into another Program must not
    // rewrite the shared ConstantEntry that compiled module code resolves against.
    // refSelf() each time because setRuntimeValue takes ownership.
    if (target_ce && (!target_ce->hasValue() || target_ce->aot_shell_pending)) {
        target_ce->setRuntimeValue(result.refSelf(), &xsink);
        if (remember) {
            remember(target_ce);
        }
    }
    if (write_shadow && shadow_ce && shadow_ce != target_ce
            && (!shadow_ce->hasValue() || shadow_ce->aot_shell_pending)) {
        shadow_ce->setRuntimeValue(result.refSelf(), &xsink);
        if (remember) {
            remember(shadow_ce);
        }
    }
    if (aotInitTraceEnabled() && xsink.isException()) {
        QoreValue err_val = xsink.getExceptionErr();
        QoreValue desc_val = xsink.getExceptionDesc();
        QoreStringValueHelper err_str(err_val);
        QoreStringValueHelper desc_str(desc_val);
        fprintf(stderr, "[aot-init] store constant exception module=%s ns=%s item=%s err=%s desc=%s\n",
            mod_name ? mod_name : "<none>", desc.ns_path.c_str(), desc.item_name.c_str(),
            err_val.getType() == NT_STRING ? err_str->c_str() : "?",
            desc_val.getType() == NT_STRING ? desc_str->c_str() : "?");
    }
    return 0;
}

//! What one AOT constant needs to recover when module load left it an unpopulated shell
/** AOT constant initializers run in serialization order with a fix-point retry, which resolves ordinary
    declaration-order dependencies.  Two things still leave a shell behind: an initializer no round could run, and
    a Program entry created from the module's entry before that entry held a value.  Before this record existed,
    both were permanent — every read of the constant raised @c AOT-PENDING-CONSTANT for the life of the process,
    and the exception that had actually stopped the initializer was gone.

    The record makes the first read able to recover: adopt the module's value, or run the initializer, whose own
    dependency reads recover the same way one level deeper.  @ref load_err keeps the load-time failure for the
    error message when neither is possible.
*/
struct AOTPendingConstantInit {
    AOTInitFuncDescriptor desc;
    QoreAOTContext* ctx = nullptr;      //!< owned by this record, unless a script load handed it to the Program
    AotFunctionPtr fn_ptr = nullptr;
    bool owns_ctx = true;               //!< false while eager module init borrows the execution context
    //! the module Program the record names, or nullptr for a script load
    /** A module Program lives for the process, so a record can name it and adopt its value.  A script Program
        cannot be named: `qore_aot_script_register()` is a host API that may be given a Program the host later
        destroys, and the record outlives this call.  A script record therefore keeps no Program pointer at all —
        the first read already runs in the Program that owns the entry, which is the only Program its initializer
        may touch — and its retained context is owned by that Program (see AotScriptPendingConstantState).
    */
    QoreProgram* pgm = nullptr;
    //! the module's own Program, or nullptr for a script load; see @ref pgm
    QoreProgram* shadow_pgm = nullptr;
    bool write_shadow = false;
    std::string mod_name;
    std::string mod_path;
    //! the exception the last module-load attempt raised, kept for the error message if the lazy run also fails
    std::string load_err;
    std::string load_desc;
    //! set while this initializer is running so a self-referential read reports a cycle instead of recursing
    bool running = false;

    DLLLOCAL ~AOTPendingConstantInit() {
        if (owns_ctx) {
            delete ctx;
        }
    }
};

static QoreThreadLock& aotPendingConstantInitLock() {
    static QoreThreadLock lck;
    return lck;
}

//! Owns every constant recovery record for the life of the process
/** A record is reachable from any number of ConstantEntry objects — a module imported into several Programs gets a
    copy of each entry, and the copy carries the pointer — so records are never freed individually.  One record
    per initialized constant of a loaded AOT module is kept; only a record whose initializer never ran also holds
    a compiled execution context.
*/
static std::vector<std::unique_ptr<AOTPendingConstantInit>>& aotPendingConstantInitRecords() {
    static std::vector<std::unique_ptr<AOTPendingConstantInit>> records;
    return records;
}

//! Attaches a recovery record to the constant entries one init descriptor targets
/** The record lets a constant that is still an unpopulated shell recover on its first read, either by adopting
    the value the module's own program already holds or by running the initializer that has not run yet.

    @param rec the record to attach; a record already attached to the module's own entry is reused instead
    @param lookup_pgm the Program whose entries the record is attached to.  It is passed separately from
        @ref AOTPendingConstantInit::pgm because a script record deliberately stores no Program pointer, yet its
        entries still have to be found here, while the load that creates it is running.

    @return the record now attached to the entries, or nullptr if the constant was not found
*/
static AOTPendingConstantInit* aotRegisterPendingConstantInit(std::unique_ptr<AOTPendingConstantInit> rec,
        QoreProgram* lookup_pgm) {
    ConstantEntry* target_ce = nullptr;
    ConstantEntry* shadow_ce = nullptr;
    aotFindInitConstantEntries(rec->desc, lookup_pgm, rec->shadow_pgm, target_ce, shadow_ce);
    if (!target_ce && !shadow_ce) {
        return nullptr;
    }
    if (aotInitTraceEnabled()) {
        fprintf(stderr, "[aot-init] recoverable constant module=%s ns=%s item=%s target_ce=%p shadow_ce=%p "
            "initializer=%d err=%s\n", rec->mod_name.c_str(), rec->desc.ns_path.c_str(),
            rec->desc.item_name.c_str(), (void*)target_ce, (void*)shadow_ce, rec->fn_ptr ? 1 : 0,
            rec->load_err.empty() ? "<none>" : rec->load_err.c_str());
    }
    AutoLocker al(aotPendingConstantInitLock());
    // Every Program importing the module registers the same constants; keep one record per constant, held by the
    // module's own entry, and point each Program's entry at it.
    AOTPendingConstantInit* existing = shadow_ce ? shadow_ce->aot_pending_init : nullptr;
    if (existing && !existing->fn_ptr && rec->fn_ptr) {
        // an earlier registration had nothing to run; keep the initializer this one carries
        existing->ctx = rec->ctx;
        existing->fn_ptr = rec->fn_ptr;
        existing->owns_ctx = rec->owns_ctx;
        rec->ctx = nullptr;
    }
    AOTPendingConstantInit* raw = existing;
    if (!raw) {
        raw = rec.get();
        aotPendingConstantInitRecords().push_back(std::move(rec));
    }
    if (target_ce) {
        target_ce->aot_pending_init = raw;
    }
    if (shadow_ce && shadow_ce != target_ce) {
        shadow_ce->aot_pending_init = raw;
    }
    return raw;
}

//! Test hook: leave every AOT constant uninitialized at module load so the first-read path is always taken
static bool aotDeferConstantInitTestHook() {
    static const bool enabled = getenv("QORE_AOT_TEST_DEFER_CONSTANT_INIT") != nullptr;
    return enabled;
}

//! Serializes lazy constant initialization; recursive so an initializer can initialize its own dependencies
static QoreRecursiveThreadLock& aotLazyConstantInitLock() {
    static QoreRecursiveThreadLock lck;
    return lck;
}

//! Target Programs visible only while this thread is eagerly initializing their constants
/** Recovery records cannot retain an importing Program because the Program can be destroyed before the
    process-lifetime record. This thread-local overlay lets recursive eager initialization populate both the
    importing Program and the module shadow without adding a dangling Program pointer to the record.
*/
static thread_local std::unordered_map<AOTPendingConstantInit*, std::vector<QoreProgram*>>
    aotEagerConstantInitPrograms;

namespace {
//! Owns the execution contexts a script Program's unrun constant initializers retain
/** Recovery records live for the process, because a record is reachable from every copy of the entry it is
    attached to and nothing can say when the last copy is gone.  A module's retained context may live that long
    too: its Program does.  A script Program does not — `qore_aot_script_register()` is a host API that may be
    given a Program the host destroys — and a context is bound to the Program whose objects its slots name, so
    running one after that Program is gone would execute against freed state.

    Binding the context to the Program removes the question: when the Program goes, every record it created is
    neutralized, and a copy of a still-pending entry that outlived it reports the constant as unpopulated
    instead of running dead code.  The record itself stays valid and keeps reporting the load-time failure.
*/
class AotScriptPendingConstantState : public AbstractQoreProgramExternalData {
public:
    void add(AOTPendingConstantInit* rec) {
        records.push_back(rec);
    }

    //! a child Program runs its own AOT load; there is nothing to inherit
    AbstractQoreProgramExternalData* copy(QoreProgram*) const override {
        return nullptr;
    }

    void doDeref() override {
        delete this;
    }

    ~AotScriptPendingConstantState() {
        // taken in the same order as qore_aot_run_pending_constant_init, so a concurrent first-read either
        // finishes before the contexts go or never sees them
        AutoLocker lazy_al(aotLazyConstantInitLock());
        AutoLocker al(aotPendingConstantInitLock());
        for (AOTPendingConstantInit* rec : records) {
            rec->fn_ptr = nullptr;
            delete rec->ctx;
            rec->ctx = nullptr;
        }
    }

private:
    std::vector<AOTPendingConstantInit*> records;
};

constexpr const char* kAotScriptPendingConstantKey = "qore_aot_script_pending_constants";
}  // anonymous namespace

//! Hands a script load's retained execution context to the Program that owns the state it runs against
static void aotAdoptScriptPendingConstantContext(QoreProgram* pgm, AOTPendingConstantInit* rec) {
    assert(pgm);
    AbstractQoreProgramExternalData* ext = pgm->getExternalData(kAotScriptPendingConstantKey);
    if (!ext) {
        ext = new AotScriptPendingConstantState;
        pgm->setExternalData(kAotScriptPendingConstantKey, ext);
    }
    static_cast<AotScriptPendingConstantState*>(ext)->add(rec);
}

int qore_aot_run_pending_constant_init(ConstantEntry* ce, ExceptionSink* xsink) {
    // Serialize lazy initialization process-wide: initializers run compiled code that mutates module state, and
    // two threads reading the same pending constant must not run it twice.  The lock is recursive, so an
    // initializer that reads another pending constant initializes it one level deeper on this thread.
    AutoLocker lazy_al(aotLazyConstantInitLock());

    // another thread may have populated the constant while this thread waited for the lock
    if (ce->hasValue()) {
        return 1;
    }

    AOTPendingConstantInit* rec = ce->aot_pending_init;
    if (!rec) {
        return 0;
    }

    // The commonest recovery needs no code at all: this Program's entry was created from the module's entry
    // before that entry had a value.  The module's entry is the canonical one, so adopt its value.
    {
        ConstantEntry* target_ce = nullptr;
        ConstantEntry* shadow_ce = nullptr;
        aotFindInitConstantEntries(rec->desc, rec->pgm, rec->shadow_pgm, target_ce, shadow_ce);
        ConstantEntry* source_ce = (shadow_ce && shadow_ce != ce && shadow_ce->hasValue()) ? shadow_ce
            : ((target_ce && target_ce != ce && target_ce->hasValue()) ? target_ce : nullptr);
        if (source_ce) {
            ExceptionSink adopt_xsink;
            ce->setRuntimeValue(source_ce->getReferencedValue(), &adopt_xsink);
            ce->materializeRuntimeRefs(&adopt_xsink);
            if (adopt_xsink.isException()) {
                xsink->assimilate(adopt_xsink);
                return -1;
            }
            {
                AutoLocker al(aotPendingConstantInitLock());
                ce->aot_pending_init = nullptr;
            }
            if (aotInitTraceEnabled()) {
                fprintf(stderr, "[aot-init] adopted constant value module=%s ns=%s item=%s\n",
                    rec->mod_name.c_str(), rec->desc.ns_path.c_str(), rec->desc.item_name.c_str());
            }
            return 1;
        }
    }

    if (!rec->fn_ptr || !rec->ctx) {
        return 0;
    }
    if (rec->running) {
        // a genuine cycle: the initializer being run reads the constant it initializes
        return 0;
    }
    rec->running = true;
    struct RunningGuard {
        AOTPendingConstantInit* rec;
        ~RunningGuard() {
            rec->running = false;
        }
    } running_guard{rec};

    if (aotInitTraceEnabled()) {
        fprintf(stderr, "[aot-init] lazy constant init module=%s ns=%s item=%s\n",
            rec->mod_name.c_str(), rec->desc.ns_path.c_str(), rec->desc.item_name.c_str());
    }

    // A script record names no Program (see AOTPendingConstantInit::pgm): the read that got here already runs in
    // the Program that owns the entry, which is the only Program the initializer may touch.
    auto eager_pgm = aotEagerConstantInitPrograms.find(rec);
    bool eager_init = eager_pgm != aotEagerConstantInitPrograms.end() && !eager_pgm->second.empty();
    QoreProgram* init_pgm = eager_init ? eager_pgm->second.back() : (rec->pgm ? rec->pgm : ::getProgram());
    if (!init_pgm) {
        return 0;
    }

    ExceptionSink init_xsink;
    QoreValue result;
    {
        std::unique_ptr<ProgramThreadCountContextHelper> program_ctx;
        if (!eager_init && rec->pgm) {
            program_ctx.reset(new ProgramThreadCountContextHelper(&init_xsink, rec->pgm, false));
        }
        if (init_xsink) {
            xsink->assimilate(init_xsink);
            return -1;
        }
        // Initializers can load further modules; mark this thread as being inside a module load so the
        // module-lock-ordering debug net sees the same state it sees during module-load initialization.
        QoreModuleLoadLockHelper aot_init_al;

        std::unique_ptr<ProgramThreadCountContextHelper> shadow_ctx;
        std::unique_ptr<ProgramCallContextHelper> shadow_call_ctx;
        if (rec->shadow_pgm && rec->shadow_pgm != init_pgm) {
            shadow_ctx.reset(new ProgramThreadCountContextHelper(&init_xsink, rec->shadow_pgm, true));
            if (init_xsink.isException()) {
                init_xsink.clear();
                shadow_ctx.reset();
            } else {
                shadow_call_ctx.reset(new ProgramCallContextHelper(rec->shadow_pgm));
            }
        }
        std::unique_ptr<QoreParseClassHelper> parse_ctx;
        if (rec->desc.target_type == AOTCompiledInitFunc::CLASS_CONSTANT) {
            QoreProgram* class_pgm = shadow_ctx && rec->shadow_pgm ? rec->shadow_pgm : init_pgm;
            const qore_ns_private* found_ns = nullptr;
            const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
                *qore_program_private::get(*class_pgm)->RootNS, rec->desc.ns_path.c_str(), found_ns);
            if (!qc && class_pgm != init_pgm) {
                qc = qore_root_ns_private::runtimeFindClass(
                    *qore_program_private::get(*init_pgm)->RootNS, rec->desc.ns_path.c_str(), found_ns);
            }
            if (qc) {
                parse_ctx.reset(new QoreParseClassHelper(const_cast<QoreClass*>(qc)));
            }
        } else {
            QoreProgram* ns_pgm = shadow_ctx && rec->shadow_pgm ? rec->shadow_pgm : init_pgm;
            qore_ns_private* ns = findNamespaceByPath(
                qore_ns_private::get(*qore_program_private::get(*ns_pgm)->RootNS), rec->desc.ns_path);
            if (!ns && ns_pgm != init_pgm) {
                ns = findNamespaceByPath(
                    qore_ns_private::get(*qore_program_private::get(*init_pgm)->RootNS), rec->desc.ns_path);
            }
            if (ns) {
                parse_ctx.reset(new QoreParseClassHelper(nullptr, ns));
            }
        }

        const char* old_name = set_module_context_name(rec->mod_name.empty() ? nullptr : rec->mod_name.c_str());
        const char* old_path = set_module_context_path(rec->mod_path.empty() ? nullptr : rec->mod_path.c_str());
        uint64_t raw_result = 0;
        try {
            raw_result = rec->fn_ptr(rec->ctx, &init_xsink);
        } catch (const QoreJITException&) {
            raw_result = 0;
        }
        set_module_context_path(old_path);
        set_module_context_name(old_name);
        memcpy(&result, &raw_result, sizeof(uint64_t));
    }

    if (init_xsink.isException()) {
        result.discard(nullptr);
        // report the original module-load failure as well when this run failed the same way, so the cause is not
        // replaced by a downstream symptom
        xsink->assimilate(init_xsink);
        return -1;
    }

    std::vector<ConstantEntry*> initialized;
    ExceptionSink store_xsink;
    // Store into the entry being read first: the reader can hold a constant of a Program the record does not
    // name (the same AOT module imported into several Programs each get their own entry).
    if (!ce->hasValue() || ce->aot_shell_pending) {
        ce->setRuntimeValue(result.refSelf(), &store_xsink);
        initialized.push_back(ce);
    }
    // then the module's own entry, so compiled module code resolving against the shadow Program sees it too; a
    // script record names no Program, and the entry read above is the only one there is
    if (init_pgm || rec->shadow_pgm) {
        aotStoreConstantInitResult(rec->desc, result, init_pgm, rec->shadow_pgm, rec->write_shadow,
            rec->mod_name.c_str(), [&initialized](ConstantEntry* entry) { initialized.push_back(entry); },
            store_xsink);
    }
    result.discard(&store_xsink);
    for (ConstantEntry* entry : initialized) {
        entry->materializeRuntimeRefs(&store_xsink);
    }
    if (store_xsink.isException()) {
        xsink->assimilate(store_xsink);
        return -1;
    }
    if (initialized.empty()) {
        return 0;
    }
    AutoLocker al(aotPendingConstantInitLock());
    for (ConstantEntry* entry : initialized) {
        entry->aot_pending_init = nullptr;
    }
    return 1;
}

std::string qore_aot_get_pending_constant_error(ConstantEntry* ce) {
    AOTPendingConstantInit* rec = ce->aot_pending_init;
    if (!rec || rec->load_err.empty()) {
        return std::string();
    }
    return rec->load_err + ": " + rec->load_desc;
}

//! Execute collected init functions and store results in target constants/static vars
/** Called after AOT function registration to initialize constants and static vars
    whose values come from lowered init expressions (delayed_eval constants, object
    constructors, runtime-dependent expressions like now()).

    Init functions are executed in serialization order, which matches the order
    the parser originally processed them — this ensures dependencies between
    constants are satisfied.
*/
static int executeInitFunctions(
        QoreProgram* pgm,
        const std::vector<AOTInitFuncExecInfo>& exec_infos,
        const std::vector<AOTInitFuncDescriptor>& descriptors,
        const char* mod_name,
        QoreProgram* shadow_pgm,
        const char* mod_path,
        bool write_shadow,
        ExceptionSink* failure_sink) {
    if (exec_infos.empty() || descriptors.empty()) {
        return 0;
    }
    assert(pgm);
    if (!pgm) {
        return -1;
    }

    ExceptionSink context_xsink;
    ProgramThreadCountContextHelper program_ctx(&context_xsink, pgm, false);
    if (context_xsink) {
        if (failure_sink) {
            failure_sink->assimilate(context_xsink);
        } else {
            context_xsink.handleExceptions();
        }
        return -1;
    }

    // AOT init functions can load further modules and can run generated code
    // that mutates module/static state. Keep this phase reentrant but serialized
    // across threads, matching the effective source-module initialization model.
    QoreModuleLoadLockHelper aot_init_al;

    if (aotInitTraceEnabled()) {
        fprintf(stderr, "[aot-init] execute module=%s pgm=%p shadow=%p exec_infos=%zu descriptors=%zu path=%s\n",
            mod_name ? mod_name : "<none>", (void*)pgm, (void*)shadow_pgm,
            exec_infos.size(), descriptors.size(), mod_path ? mod_path : "<none>");
    }

    // Build name → exec info map
    std::unordered_map<std::string, const AOTInitFuncExecInfo*> exec_map;
    for (auto& info : exec_infos) {
        exec_map[info.name] = &info;
    }

    struct ModuleInitNamePathContextHelper {
        const char* old_name;
        const char* old_path;

        ModuleInitNamePathContextHelper(const char* name, const char* path)
                : old_name(set_module_context_name(name)),
                  old_path(set_module_context_path(path)) {
        }

        ~ModuleInitNamePathContextHelper() {
            set_module_context_path(old_path);
            set_module_context_name(old_name);
        }
    };

    qore_program_private* pp = qore_program_private::get(*pgm);
    qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);
    // Shadow program (usually the module's own program): when init functions
    // are built against the module's namespace tree, subsequent init functions
    // read cross-constant references through the MODULE program's
    // ConstantEntry. We mirror each init result into the shadow program as
    // well as the target program so later init functions see populated values.
    qore_program_private* shadow_pp = shadow_pgm ? qore_program_private::get(*shadow_pgm) : nullptr;
    qore_ns_private* shadow_root_ns = shadow_pp ? qore_ns_private::get(*shadow_pp->RootNS) : nullptr;
    auto find_class = [](qore_program_private* class_pp, const std::string& class_path) -> QoreClass* {
        if (!class_pp) {
            return nullptr;
        }
        const qore_ns_private* found_ns = nullptr;
        const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
            *class_pp->RootNS, class_path.c_str(), found_ns);
        return const_cast<QoreClass*>(qc);
    };
    auto make_parse_context = [&](const AOTInitFuncDescriptor& desc,
            bool prefer_shadow) -> std::unique_ptr<QoreParseClassHelper> {
        switch (desc.target_type) {
            case AOTCompiledInitFunc::CLASS_CONSTANT:
            case AOTCompiledInitFunc::STATIC_VAR: {
                QoreClass* qc = prefer_shadow ? find_class(shadow_pp, desc.ns_path) : nullptr;
                if (!qc) {
                    qc = find_class(pp, desc.ns_path);
                }
                if (!qc && !prefer_shadow) {
                    qc = find_class(shadow_pp, desc.ns_path);
                }
                return qc ? std::make_unique<QoreParseClassHelper>(qc) : nullptr;
            }

            case AOTCompiledInitFunc::NS_CONSTANT:
            case AOTCompiledInitFunc::GLOBAL_VAR:
            case AOTCompiledInitFunc::GLOBAL_VAR_CONSTRUCT:
            case AOTCompiledInitFunc::MODULE_INIT: {
                qore_ns_private* ns = (prefer_shadow && shadow_root_ns)
                    ? findNamespaceByPath(shadow_root_ns, desc.ns_path) : nullptr;
                if (!ns) {
                    ns = findNamespaceByPath(root_ns, desc.ns_path);
                }
                if (!ns && !prefer_shadow && shadow_root_ns) {
                    ns = findNamespaceByPath(shadow_root_ns, desc.ns_path);
                }
                return ns ? std::make_unique<QoreParseClassHelper>(nullptr, ns) : nullptr;
            }

            case AOTCompiledInitFunc::OUTLINED_HELPER:
                return nullptr;
        }
        return nullptr;
    };

    // ProgramRuntimeParseContextHelper can set td->current_pgm without setting
    // td->tlpd. Init functions that construct objects need td->tlpd for
    // thread_instantiate_lvar() (for example SelfInstantiatorHelper in
    // initMembers), so keep the bridge for nested parse-context callers.
    thread_ensure_local_program_data();

    int executed = 0;
    int failed = 0;

    // Execute in two passes so the module init closure (MODULE_INIT) runs
    // AFTER all constant/static-var inits complete. The closure body may
    // reference constants whose values come from regular init funcs (e.g.
    // DataProvider.qm's init closure references AutoType from Qore::Reflection
    // and assigns to AbstractDataProviderType::anyDataType).
    // Fix-point retry: AOT constant init order is compile-time declaration order,
    // which does not always match dependency order.  A later-declared constant
    // can be referenced by an earlier-declared one (e.g. DataProvider's
    // AbstractDataProviderTypeMap references QoreUnsignedByteDataType which
    // itself depends on QoreIntDataTypeBase::SupportedOptions declared later).
    // Inits that throw AOT-PENDING-CONSTANT are retried in subsequent rounds;
    // we stop when a round executes zero descriptors (no forward progress), or
    // after a small cap to avoid pathological loops.
    std::vector<bool> desc_done(descriptors.size(), false);
    std::vector<std::string> last_error(descriptors.size());
    std::vector<std::string> last_desc(descriptors.size());
    std::vector<bool> last_error_pending(descriptors.size(), false);
    // Attach constant recovery records before executing any initializer. Generated init code reads constants
    // through RuntimeConstantRefNode, so a forward dependency can now run recursively on first read instead of
    // raising AOT-PENDING-CONSTANT and forcing another complete fixpoint round. The contexts stay owned by
    // exec_infos during eager execution; only an initializer left pending at the end transfers ownership.
    std::vector<AOTPendingConstantInit*> active_constant_inits(descriptors.size(), nullptr);
    struct EagerConstantInitProgramGuard {
        std::vector<AOTPendingConstantInit*>& records;
        QoreProgram* pgm;
        ~EagerConstantInitProgramGuard() {
            for (AOTPendingConstantInit* rec : records) {
                auto i = aotEagerConstantInitPrograms.find(rec);
                if (i != aotEagerConstantInitPrograms.end() && !i->second.empty()
                        && i->second.back() == pgm) {
                    i->second.pop_back();
                    if (i->second.empty()) {
                        aotEagerConstantInitPrograms.erase(i);
                    }
                }
            }
        }
    } eager_pgm_guard{active_constant_inits, pgm};
    for (size_t di = 0; di < descriptors.size(); ++di) {
        if (di && !(di % 100) && qore_check_cancel(failure_sink,
                "AOT initializer dependency registration")) {
            std::unordered_set<QoreAOTContext*> deleted;
            for (size_t ci = 0; ci < active_constant_inits.size(); ++ci) {
                AOTPendingConstantInit* rec = active_constant_inits[ci];
                auto ei = exec_map.find(descriptors[ci].name);
                if (rec && ei != exec_map.end() && rec->ctx == ei->second->ctx && !rec->owns_ctx) {
                    rec->ctx = nullptr;
                    rec->fn_ptr = nullptr;
                    rec->owns_ctx = true;
                }
            }
            for (auto& info : exec_infos) {
                if (deleted.insert(info.ctx).second) {
                    delete info.ctx;
                }
            }
            return -1;
        }
        const AOTInitFuncDescriptor& desc = descriptors[di];
        if (desc.target_type != AOTCompiledInitFunc::NS_CONSTANT
                && desc.target_type != AOTCompiledInitFunc::CLASS_CONSTANT) {
            continue;
        }
        auto rec = std::make_unique<AOTPendingConstantInit>();
        rec->desc = desc;
        rec->pgm = shadow_pgm;
        rec->shadow_pgm = shadow_pgm;
        rec->write_shadow = write_shadow;
        rec->mod_name = mod_name ? mod_name : "";
        rec->mod_path = mod_path ? mod_path : "";
        auto ei = exec_map.find(desc.name);
        if (ei != exec_map.end() && ei->second->fn_ptr && ei->second->ctx) {
            rec->ctx = ei->second->ctx;
            rec->fn_ptr = ei->second->fn_ptr;
            rec->owns_ctx = false;
        }
        active_constant_inits[di] = aotRegisterPendingConstantInit(std::move(rec),
            shadow_pgm ? shadow_pgm : pgm);
        if (active_constant_inits[di]) {
            aotEagerConstantInitPrograms[active_constant_inits[di]].push_back(pgm);
        }
    }
    std::vector<ConstantEntry*> initialized_constants;
    auto remember_initialized_constant = [&initialized_constants](ConstantEntry* ce) {
        if (ce) {
            initialized_constants.push_back(ce);
        }
    };
    auto materialize_initialized_constants = [&initialized_constants]() {
        ExceptionSink mxs;
        for (ConstantEntry* ce : initialized_constants) {
            if (!ce) {
                continue;
            }
            ce->materializeRuntimeRefs(&mxs);
            if (mxs) {
                mxs.clear();
            }
        }
    };
    for (int pass = 0; pass < 2; ++pass) {
    const bool run_module_init = (pass == 1);
    // Between pass 0 (STATIC_VAR / NS_CONSTANT / CLASS_CONSTANT) and pass 1
    // (MODULE_INIT closures), force evalInit() on every static var that the
    // STATIC_VAR pass didn't touch. AOT deserialization leaves vars without
    // an init expression at eval_init=false; the first user-script read would
    // otherwise call init() and silently wipe whatever the MODULE_INIT closure
    // wrote into the slot (object writes to *Thing static vars vanish, but
    // simple types initialised in pass 0 work fine because eval_init=true).
    if (run_module_init) {
        ExceptionSink local_xs;
        preInitStaticVarsInNamespace(root_ns, &local_xs);
        local_xs.clear();
    }
    const int max_rounds = run_module_init ? 1 : 32;
    for (int round = 0; round < max_rounds; ++round) {
    int this_round_executed = 0;
    // Execute in descriptor order (matches compilation/parser order)
    for (size_t di = 0; di < descriptors.size(); ++di) {
        if (desc_done[di]) {
            continue;
        }
        const AOTInitFuncDescriptor& desc = descriptors[di];
        bool is_module_init = (desc.target_type == AOTCompiledInitFunc::MODULE_INIT);
        if (is_module_init != run_module_init) {
            continue;
        }
        // A source module's init closure runs once when the module is registered globally, not once for every
        // Program that imports it.  The first AOT initialization pass owns the shared shadow population and is
        // therefore the equivalent global execution.  Later imports still initialize program-local constants and
        // variables in pass 0, but must not repeat module-registration or other external side effects in pass 1.
        if (is_module_init && !write_shadow) {
            desc_done[di] = true;
            continue;
        }
        if ((desc.target_type == AOTCompiledInitFunc::NS_CONSTANT
                || desc.target_type == AOTCompiledInitFunc::CLASS_CONSTANT)
                && !aotInitDescriptorNeedsExecution(desc, pgm, shadow_pgm, write_shadow)) {
            desc_done[di] = true;
            ++executed;
            ++this_round_executed;
            continue;
        }
        if (aotInitTraceEnabled()) {
            fprintf(stderr, "[aot-init] descriptor module=%s pass=%d round=%d name=%s target=%d ns=%s item=%s\n",
                mod_name ? mod_name : "<none>", pass, round, desc.name.c_str(),
                (int)desc.target_type, desc.ns_path.c_str(), desc.item_name.c_str());
        }
        // Test hook: module load almost always satisfies constant initialization order, which leaves the
        // first-read path unreachable from a test.  With this set, no constant is initialized at load, so every
        // read must go through the deferred initializer and produce exactly the same values.
        if (aotDeferConstantInitTestHook() && !run_module_init
                && (desc.target_type == AOTCompiledInitFunc::NS_CONSTANT
                    || desc.target_type == AOTCompiledInitFunc::CLASS_CONSTANT)) {
            continue;
        }
        auto it = exec_map.find(desc.name);
        if (it == exec_map.end()) {
            if (aotInitTraceEnabled()) {
                fprintf(stderr, "[aot-init] missing exec info module=%s name=%s\n",
                    mod_name ? mod_name : "<none>", desc.name.c_str());
            }
            desc_done[di] = true;
            continue;
        }

        const AOTInitFuncExecInfo* info = it->second;

        if (desc.target_type == AOTCompiledInitFunc::GLOBAL_VAR
                || desc.target_type == AOTCompiledInitFunc::GLOBAL_VAR_CONSTRUCT) {
            qore_ns_private* target_ns = findNamespaceByPath(root_ns, desc.ns_path);
            Var* target_var = target_ns
                ? target_ns->var_list.runtimeFindVar(desc.item_name.c_str()) : nullptr;
            Var* shadow_var = nullptr;
            if (shadow_root_ns) {
                qore_ns_private* shadow_ns = findNamespaceByPath(shadow_root_ns, desc.ns_path);
                shadow_var = shadow_ns
                    ? shadow_ns->var_list.runtimeFindVar(desc.item_name.c_str()) : nullptr;
            }
            bool same_storage = target_var && shadow_var
                && target_var->parseGetVar() == shadow_var->parseGetVar();
            bool target_done = !target_var || target_var->isAOTInitDone();
            bool shadow_done = !write_shadow || !shadow_var || same_storage
                || shadow_var->isAOTInitDone();
            if ((target_var || shadow_var) && target_done && shadow_done) {
                desc_done[di] = true;
                ++executed;
                ++this_round_executed;
                continue;
            }
        }

        // Call the init function
        printd(5, "AOT init: calling '%s' fn_ptr=%p ctx=%p\n",
            desc.name.c_str(), (void*)info->fn_ptr, (void*)info->ctx);
        ExceptionSink xsink;
        uint64_t raw_result = 0;
        {
            // Switch current_pgm to the shadow program (the module's own program)
            // during the init call so that reflection APIs (e.g. TypedHash::forName)
            // resolve types in the module's pgm — whose parse is complete — rather
            // than in tpgm, which may still be parsing_in_progress when the module
            // was loaded via %requires inside a user-program parse. Without the
            // switch, a TypedHash constructed inside the init would be bound to
            // tpgm; a subsequent getMembers() call would then attach to tpgm via
            // QoreExternalProgramContextHelper and trip the parse-in-progress
            // rejection in incThreadCount().
            // Source mode avoids the issue because parseCommit clears
            // parsing_in_progress BEFORE ConstantEntry::parseCommitRuntimeInit
            // evaluates the init expression; AOT evaluates later (during module
            // ns_init) with tpgm's parse still in progress.
            std::unique_ptr<ProgramThreadCountContextHelper> init_ctx_helper;
            std::unique_ptr<ProgramCallContextHelper> init_call_ctx_helper;
            if (shadow_pgm && shadow_pgm != pgm) {
                init_ctx_helper.reset(new ProgramThreadCountContextHelper(&xsink, shadow_pgm, true));
                if (xsink.isException()) {
                    xsink.clear();
                    init_ctx_helper.reset();
                } else {
                    // Keep caller-visible lookups consistent with the
                    // shadow execution program for generated init code.
                    init_call_ctx_helper.reset(new ProgramCallContextHelper(shadow_pgm));
                }
            }
            std::unique_ptr<QoreParseClassHelper> parse_ctx_helper = make_parse_context(desc, init_ctx_helper != nullptr);
            const char* init_mod_name = is_module_init && !desc.ns_path.empty()
                ? desc.ns_path.c_str() : mod_name;
            const char* init_mod_path = is_module_init && !desc.item_name.empty()
                ? desc.item_name.c_str() : mod_path;
            ModuleInitNamePathContextHelper module_ctx(init_mod_name, init_mod_path);
            if (is_module_init) {
                // The compiled closure body expects its body locals to be
                // pre-instantiated on the thread's lvstack (mimicking evalTiered's
                // contract with regular functions). Manually instantiate each
                // body local before the call and uninstantiate after.
                int instantiated = 0;
                for (LocalVar* lv : info->ctx->all_body_locals) {
                    if (lv) {
                        lv->instantiate(QoreParseOptions());
                        ++instantiated;
                    }
                }
                // C++ EH prototype: AOT init functions go through the same EH path
                // as regular AOT functions when QORE_AOT_EH=1, so they may throw
                // QoreJITException out of the function body. Catch here at the
                // C++<->AOT boundary; xsink is already populated at the raise site.
                // raw_result stays 0 (NOTHING bits) which is what the check-based
                // path would have written too.
                try {
                    raw_result = info->fn_ptr(info->ctx, &xsink);
                } catch (const QoreJITException&) {
                    raw_result = 0;
                }
                for (int k = instantiated - 1; k >= 0; --k) {
                    LocalVar* lv = info->ctx->all_body_locals[k];
                    if (lv) {
                        lv->uninstantiate(&xsink);
                    }
                }
            } else {
                try {
                    raw_result = info->fn_ptr(info->ctx, &xsink);
                } catch (const QoreJITException&) {
                    raw_result = 0;
                }
            }
        }  // end shadow-context scope (init_ctx_helper destroyed here)
        printd(5, "AOT init: '%s' returned raw=%llu\n", desc.name.c_str(), (unsigned long long)raw_result);

        if (xsink.isException()) {
            QoreValue err_val = xsink.getExceptionErr();
            QoreValue desc_val = xsink.getExceptionDesc();
            QoreStringValueHelper err_str(err_val);
            QoreStringValueHelper desc_str(desc_val);
            const bool is_pending = qore_is_deferred_runtime_init_exception(&xsink);
            const char* err_cstr = err_val.getType() == NT_STRING ? err_str->c_str() : "?";
            const char* desc_cstr = desc_val.getType() == NT_STRING ? desc_str->c_str() : "?";
            last_error[di] = err_cstr;
            last_desc[di] = desc_cstr;
            last_error_pending[di] = is_pending;
            if (aotInitTraceEnabled()) {
                fprintf(stderr, "[aot-init] call exception module=%s name=%s err=%s desc=%s pending=%d\n",
                    mod_name ? mod_name : "<none>", desc.name.c_str(),
                    err_cstr, desc_cstr, static_cast<int>(is_pending));
            }
            printd(5, "AOT init: '%s' raised exception: %s: %s%s\n",
                desc.name.c_str(), err_cstr, desc_cstr,
                is_pending ? " (will retry)" : "");
            if (failure_sink && !*failure_sink && run_module_init) {
                failure_sink->raiseException(err_cstr, "%s", desc_cstr);
            }
            xsink.clear();
            if (!run_module_init) {
                // Leave pass-0 init descriptors retryable until the fixpoint
                // settles.  A lazy static-var read can turn a not-yet-ready
                // dependency into a normal type/overload error by yielding
                // NOTHING, so retry all pass-0 exceptions and report the last
                // one if the descriptor never succeeds.
                continue;
            }
            desc_done[di] = true;
            ++failed;
            continue;
        }

        // Mark completed before store so failed stores still advance.
        desc_done[di] = true;
        last_error[di].clear();
        last_desc[di].clear();
        last_error_pending[di] = false;
        ++this_round_executed;

        // Convert raw result to QoreValue
        QoreValue result;
        memcpy(&result, &raw_result, sizeof(uint64_t));

        // Store the result in the target constant or static var
        switch (desc.target_type) {
            case AOTCompiledInitFunc::NS_CONSTANT:
            case AOTCompiledInitFunc::CLASS_CONSTANT: {
                // Store into the target and the shadow (module) ConstantEntry.  Either or both may be non-null:
                // a constant declared with an init expression that was merged into the target program is
                // findable there, while a constant only the module program holds (a non-public class constant,
                // for example) must still be populated in the shadow so compiled module code reading it through
                // a runtime constant reference sees the value.
                if (aotStoreConstantInitResult(desc, result, pgm, shadow_pgm, write_shadow, mod_name,
                        remember_initialized_constant, xsink)) {
                    result.discard(&xsink);
                    ++failed;
                    break;
                }
                result.discard(&xsink);
                ++executed;
                printd(2, "AOT init: initialized constant '%s::%s' type=%s\n",
                    desc.ns_path.c_str(), desc.item_name.c_str(), result.getTypeName());
                break;
            }

            case AOTCompiledInitFunc::STATIC_VAR: {
                // ns_path is the class path. Look up in both target and shadow: a
                // non-public class only lives in the module program.
                QoreVarInfo* target_vi = nullptr;
                {
                    const qore_ns_private* found_ns = nullptr;
                    const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
                        *pp->RootNS, desc.ns_path.c_str(), found_ns);
                    if (qc) {
                        qore_class_private* qcp = qore_class_private::get(
                            *const_cast<QoreClass*>(qc));
                        target_vi = qcp->vars.find(desc.item_name.c_str());
                    }
                }
                QoreVarInfo* shadow_vi = nullptr;
                if (shadow_pp) {
                    const qore_ns_private* found_ns = nullptr;
                    const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
                        *shadow_pp->RootNS, desc.ns_path.c_str(), found_ns);
                    if (qc) {
                        qore_class_private* qcp = qore_class_private::get(
                            *const_cast<QoreClass*>(qc));
                        shadow_vi = qcp->vars.find(desc.item_name.c_str());
                    }
                }
                if (!target_vi && !shadow_vi) {
                    printd(0, "AOT init: static var '%s::%s' not found in target or shadow\n",
                        desc.ns_path.c_str(), desc.item_name.c_str());
                    result.discard(&xsink);
                    ++failed;
                    break;
                }
                auto has_concrete_static_value = [&xsink](QoreVarInfo* vi) -> bool {
                    if (!vi || !vi->eval_init) {
                        return false;
                    }
                    QoreValue current = vi->getRuntimeReferencedValue();
                    bool concrete = !current.needsEval();
                    current.discard(&xsink);
                    return concrete;
                };
                bool target_has_concrete_value = has_concrete_static_value(target_vi);
                bool shadow_has_concrete_value = has_concrete_static_value(shadow_vi);
                // The target program may have copied the static var from the
                // module program with an already-initialized value.  Clean up
                // the old value before re-assigning so assignInit finds a
                // clean QoreLValue (avoids assert in debug builds / leak in
                // release builds). refSelf() on result each time because
                // assignInit takes ownership.
                if (target_vi && !target_has_concrete_value) {
                    target_vi->val.removeValue(true).discard(&xsink);
                    target_vi->assignInit(result.refSelf());
                    target_vi->eval_init = true;
                }
                // The shadow module program is shared by all imports of this
                // AOT module.  Initialize it once, but do not reset live
                // module static state when the same module is imported into a
                // transient dependency program.
                if (write_shadow && shadow_vi && shadow_vi != target_vi && !shadow_has_concrete_value) {
                    shadow_vi->val.removeValue(true).discard(&xsink);
                    shadow_vi->assignInit(result.refSelf());
                    shadow_vi->eval_init = true;
                }
                result.discard(&xsink);
                ++executed;
                printd(2, "AOT init: initialized static var '%s::%s' type=%s\n",
                    desc.ns_path.c_str(), desc.item_name.c_str(), result.getTypeName());
                break;
            }

            case AOTCompiledInitFunc::GLOBAL_VAR: {
                qore_ns_private* target_ns = findNamespaceByPath(root_ns, desc.ns_path);
                Var* target_var = target_ns
                    ? target_ns->var_list.runtimeFindVar(desc.item_name.c_str()) : nullptr;
                Var* shadow_var = nullptr;
                if (shadow_root_ns) {
                    qore_ns_private* shadow_ns = findNamespaceByPath(shadow_root_ns, desc.ns_path);
                    shadow_var = shadow_ns
                        ? shadow_ns->var_list.runtimeFindVar(desc.item_name.c_str()) : nullptr;
                }
                if (!target_var && !shadow_var) {
                    printd(0, "AOT init: global variable '%s::%s' not found in target or shadow\n",
                        desc.ns_path.c_str(), desc.item_name.c_str());
                    result.discard(&xsink);
                    ++failed;
                    break;
                }

                auto assign_global = [&xsink, &result](Var* var) {
                    if (!var || var->isAOTInitDone()) {
                        return;
                    }
                    LValueHelper lvh(&xsink);
                    if (var->getLValue(lvh, false)) {
                        return;
                    }
                    lvh.assign(result.refSelf(), "<AOT global variable initializer>");
                    if (!xsink) {
                        var->setAOTInitDone();
                    }
                };

                assign_global(target_var);
                if (!xsink && write_shadow && shadow_var
                        && (!target_var
                            || target_var->parseGetVar() != shadow_var->parseGetVar())) {
                    assign_global(shadow_var);
                }
                result.discard(&xsink);
                if (xsink) {
                    ++failed;
                } else {
                    ++executed;
                    printd(2, "AOT init: initialized global variable '%s::%s'\n",
                        desc.ns_path.c_str(), desc.item_name.c_str());
                }
                break;
            }

            case AOTCompiledInitFunc::GLOBAL_VAR_CONSTRUCT: {
                qore_ns_private* target_ns = findNamespaceByPath(root_ns, desc.ns_path);
                Var* target_var = target_ns
                    ? target_ns->var_list.runtimeFindVar(desc.item_name.c_str()) : nullptr;
                Var* shadow_var = nullptr;
                if (shadow_root_ns) {
                    qore_ns_private* shadow_ns = findNamespaceByPath(shadow_root_ns, desc.ns_path);
                    shadow_var = shadow_ns
                        ? shadow_ns->var_list.runtimeFindVar(desc.item_name.c_str()) : nullptr;
                }
                if (!target_var && !shadow_var) {
                    printd(0, "AOT init: constructed global variable '%s::%s' not found\n",
                        desc.ns_path.c_str(), desc.item_name.c_str());
                    result.discard(&xsink);
                    ++failed;
                    break;
                }

                if (target_var) {
                    target_var->setAOTInitDone();
                }
                if (write_shadow && shadow_var && (!target_var
                        || target_var->parseGetVar() != shadow_var->parseGetVar())) {
                    LValueHelper lvh(&xsink);
                    if (!shadow_var->getLValue(lvh, false)) {
                        lvh.assign(result.refSelf(), "<AOT global variable constructor initializer>");
                        if (!xsink) {
                            shadow_var->setAOTInitDone();
                        }
                    }
                }
                result.discard(&xsink);
                if (xsink) {
                    ++failed;
                } else {
                    ++executed;
                    printd(2, "AOT init: constructed global variable '%s::%s'\n",
                        desc.ns_path.c_str(), desc.item_name.c_str());
                }
                break;
            }

            case AOTCompiledInitFunc::MODULE_INIT: {
                // Module init closures run for side effects only — discard the
                // return value. The compiled body already mutated program state
                // (e.g. assigned static members, called registerFactory, etc.).
                result.discard(&xsink);
                ++executed;
                printd(2, "AOT init: executed module init closure for '%s'\n",
                    desc.ns_path.c_str());
                break;
            }

            case AOTCompiledInitFunc::OUTLINED_HELPER: {
                // Outlined helpers are LLVM-lowered and linked into the module
                // but called BY their outer init function — not run at module
                // load.  Reaching this arm means we mis-dispatched (the helper
                // somehow landed in exec_infos).  Skip silently; the outer will
                // drive it.
                result.discard(&xsink);
                printd(5, "AOT init: skipped outlined helper '%s' (called by outer)\n",
                    desc.name.c_str());
                break;
            }
        }
    }
    if (!run_module_init) {
        materialize_initialized_constants();
    }
    // If no descriptor advanced this round, a real dep cycle or unresolvable
    // pending constant exists; stop retrying — the next round would do the
    // same work.  Deferred pending-constant descriptors left as !desc_done
    // surface as the user-visible failure.
    if (this_round_executed == 0) {
        break;
    }
    }  // end round loop
    }  // end two-pass loop

    // Any pass-0 descriptors still !desc_done after all rounds remained
    // stuck on pending-constant deps.  Count them as failures so the
    // caller's tally reflects real problems rather than silently passing.
    std::unordered_set<const QoreAOTContext*> deferred_contexts;
    for (size_t di = 0; di < descriptors.size(); ++di) {
        if (!desc_done[di] && descriptors[di].target_type != AOTCompiledInitFunc::MODULE_INIT) {
            const bool test_deferred = aotDeferConstantInitTestHook()
                && (descriptors[di].target_type == AOTCompiledInitFunc::NS_CONSTANT
                    || descriptors[di].target_type == AOTCompiledInitFunc::CLASS_CONSTANT);
            if (test_deferred) {
                // deferred on purpose by the test hook, not a failure
            } else if (!last_error[di].empty()) {
                printd(0, "AOT init: '%s' remained unresolved after %d rounds; last exception: %s: %s%s\n",
                    descriptors[di].name.c_str(), 32, last_error[di].c_str(), last_desc[di].c_str(),
                    last_error_pending[di] ? " (pending)" : "");
                if (failure_sink && !*failure_sink) {
                    failure_sink->raiseException(last_error[di].c_str(), "%s", last_desc[di].c_str());
                }
            } else {
                printd(0, "AOT init: '%s' remained pending after %d rounds\n",
                    descriptors[di].name.c_str(), 32);
                if (failure_sink && !*failure_sink) {
                    failure_sink->raiseException("AOT-INIT-ERROR",
                        "AOT initializer '%s' remained pending after %d rounds",
                        descriptors[di].name.c_str(), 32);
                }
            }
            ++failed;
        }
    }

    // Finalize the recovery records attached before eager execution. Two things can leave a constant an
    // unpopulated shell that no later load repairs: an initializer this pass could not run at all, and an entry
    // this Program created from the module's entry before that entry held a value.  Both used to be permanent —
    // every read of the constant raised AOT-PENDING-CONSTANT for the life of the process.  With the record, the
    // first read adopts the module's value or runs the initializer.
    //
    // A script load gets a record too.  It used to get none, on the grounds that the record outlives this call
    // while a script Program need not: but that argument only rules out naming the Program in the record, and a
    // script record does not name one (see AOTPendingConstantInit::pgm) — the first read already runs in the
    // Program that owns the entry.  Skipping registration instead made every ordering the load-time fix-point
    // could not satisfy permanent in a compiled executable, where the whole program is one script batch: the
    // constant raised AOT-PENDING-CONSTANT on every read for the life of the process, and only a rebuild that
    // happened to order the initializers differently cured it.
    //
    // This is finalization and is deliberately not cancellable: it hands ownership of retained execution
    // contexts to their records, and the loop below frees exactly the contexts no record took.
    for (size_t di = 0; di < descriptors.size(); ++di) {
        const AOTInitFuncDescriptor& desc = descriptors[di];
        if (desc.target_type != AOTCompiledInitFunc::NS_CONSTANT
                && desc.target_type != AOTCompiledInitFunc::CLASS_CONSTANT) {
            continue;
        }
        auto it = exec_map.find(desc.name);
        AOTPendingConstantInit* rec = active_constant_inits[di];
        if (!rec) {
            continue;
        }
        rec->load_err = last_error[di];
        rec->load_desc = last_desc[di];
        // Only an initializer that did not run is kept executable; keeping every context alive would retain
        // the compiled context of every constant of every loaded module for nothing.
        bool retained_ctx = false;
        if (!desc_done[di] && it != exec_map.end() && rec->ctx == it->second->ctx
                && rec->fn_ptr && deferred_contexts.find(rec->ctx) == deferred_contexts.end()) {
            rec->owns_ctx = true;
            deferred_contexts.insert(it->second->ctx);
            retained_ctx = true;
        } else if (it != exec_map.end() && rec->ctx == it->second->ctx && !rec->owns_ctx) {
            rec->ctx = nullptr;
            rec->fn_ptr = nullptr;
            rec->owns_ctx = true;
        }
        // a context retained for a script load is bound to the Program whose state it runs against, so the
        // Program's teardown neutralizes the record rather than leaving it able to run against freed state
        if (retained_ctx && !shadow_pgm) {
            aotAdoptScriptPendingConstantContext(pgm, rec);
        }
    }

    for (auto& info : exec_infos) {
        // contexts retained for lazy initialization are owned by their AOTPendingConstantInit record
        if (deferred_contexts.find(info.ctx) == deferred_contexts.end()) {
            delete info.ctx;
        }
    }

    printd(5, "AOT module '%s': executed %d/%d init functions (%d failed)\n",
        mod_name, executed, (int)exec_infos.size(), failed);
    return failed;
}

//! C ABI entry point for AOT modules (v3 - full 128-bit parse options)
extern "C" DLLEXPORT QoreStringNode* qore_aot_module_init_v3(
    const uint8_t* metadata, int metadata_len,
    const char* label,
    int64_t parse_options_lo, int64_t parse_options_hi,
    const char* mod_name,
    const QoreAOTFunc* functions, int num_functions
) {
    std::string runtime_module_path = std::move(aot_module_init_context_path);
    aot_module_init_context_path.clear();
    const char* module_context_path = runtime_module_path.empty() ? label : runtime_module_path.c_str();
    std::string module_context_desc =
        (module_context_path && *module_context_path) ? module_context_path : "<unknown module path>";
    if (label && *label && (!module_context_path || strcmp(label, module_context_path))) {
        module_context_desc += "; source label: ";
        module_context_desc += label;
    }

    // Phase 1.5 load-time profiling (temporary): QORE_AOT_LOAD_TRACE=1
    // emits per-phase wall-clock timing so we can see where module
    // load spends its budget — expectation is near-instant
    // deserialize + register, with any 5-second blocks indicating
    // AST fallback re-parse of the embedded source.
    static const bool _aot_trace = getenv("QORE_AOT_LOAD_TRACE") != nullptr;
    struct timespec _aot_t_start;
    struct timespec _aot_t_prev;
    if (_aot_trace) {
        clock_gettime(CLOCK_MONOTONIC, &_aot_t_start);
        _aot_t_prev = _aot_t_start;
    }
#define AOT_TRACE(LABEL) do { \
    if (_aot_trace) { \
        struct timespec _n; clock_gettime(CLOCK_MONOTONIC, &_n); \
        double _d = (_n.tv_sec - _aot_t_prev.tv_sec)*1e3 \
                  + (_n.tv_nsec - _aot_t_prev.tv_nsec)/1e6; \
        double _t = (_n.tv_sec - _aot_t_start.tv_sec)*1e3 \
                  + (_n.tv_nsec - _aot_t_start.tv_nsec)/1e6; \
        fprintf(stderr, "[aot-load] %-40s +%7.1f ms  (total %7.1f ms)\n", \
            LABEL, _d, _t); \
        _aot_t_prev = _n; \
    } \
} while (0)
    AOT_TRACE("entry");
    printd(5, "AOT v3 ENTRY '%s': num_functions=%d functions=%p\n",
        mod_name, num_functions, (const void*)functions);
    // Construct full 128-bit parse options from lo+hi components
    QoreParseOptions parse_options(parse_options_lo, parse_options_hi);

    ExceptionSink xsink;

    QoreProgram* parent_pgm = getProgram();

    // Use a local variable for the program being created (see qore_aot_module_init
    // for explanation of nested init overwrite issue)
    QoreProgram* local_pgm = new QoreProgram(parse_options);
    if (mod_name && *mod_name && qore_program_private::get(*local_pgm)->addUserFeature(mod_name)) {
        QoreStringNode* err = new QoreStringNodeMaker(
            "AOT module init error: feature '%s' is already loaded in this Program container", mod_name);
        local_pgm->waitForTerminationAndDeref(nullptr);
        return err;
    }

    // Set JIT execution mode
    local_pgm->setExecMode(QEM_JIT);

    // Set script path from the runtime module path when available, so
    // `get_script_dir()` returns the module's .qmod directory when init
    // functions execute under a
    // ProgramThreadCountContextHelper(shadow=local_pgm) in ns_init.
    // Constants like
    //     const FooLogo = File::readTextFile(get_script_dir() + "/foo-logo.svg");
    // fire during init and need the path populated; without this, script_dir
    // is empty, concat emits "/foo-logo.svg" (leading slash), and every
    // downstream registerApp(...) call raises AOT-PENDING-CONSTANT when the
    // constant never populates.  Mirrors v1's setScriptPath(label) at the
    // equivalent construction site.  The compile-time label is still used
    // below for diagnostics and source-hash checks.
    if (module_context_path) {
        local_pgm->setScriptPath(module_context_path);
    }

    QoreAOTBinaryReader metadata_reader;
    {
        std::string reader_error;
        if (!metadata_reader.open(metadata, static_cast<uint32_t>(metadata_len), reader_error)) {
            QoreStringNode* err = new QoreStringNodeMaker(
                "AOT module metadata read error for module '%s' (%s): %s",
                mod_name ? mod_name : "<unknown>",
                (module_context_path && *module_context_path) ? module_context_path : "<unknown path>",
                reader_error.c_str());
            local_pgm->waitForTerminationAndDeref(nullptr);
            return err;
        }
    }

    // Apply inherited plus compiled-in module path lists BEFORE loading
    // dependencies; source modules inherit the requiring Program's search
    // surface before their own directives are applied.
    {
        std::vector<std::string> prepended, appended;
        std::string mp_error;
        if (readModulePathLists(metadata_reader, prepended, appended, mp_error)) {
            inheritAOTModulePathLists(local_pgm, parent_pgm, prepended, appended);
        } else {
            inheritAOTModulePathLists(local_pgm, parent_pgm);
        }
    }

    {
        std::string cmd_error;
        if (!applyAOTModuleCommandsToProgram(local_pgm, metadata_reader, module_context_path, cmd_error)) {
            QoreStringNode* err = new QoreStringNodeMaker(
                "AOT module-command replay error for module '%s' (%s): %s",
                mod_name ? mod_name : "<unknown>",
                (module_context_path && *module_context_path) ? module_context_path : "<unknown path>",
                cmd_error.c_str());
            local_pgm->waitForTerminationAndDeref(nullptr);
            return err;
        }
    }

    // Load dependencies from serialized metadata BEFORE deserializing namespace tree.
    // Dependencies must be loaded first because deserialization may need to resolve
    // base classes, types, and other references from dependency modules.
    std::vector<std::string> deps;
    std::string dep_error;
    if (!readDependencies(metadata_reader, deps, dep_error)) {
        QoreStringNode* err = new QoreStringNodeMaker(
            "AOT module dependency read error for module '%s' (%s): %s",
            mod_name ? mod_name : "<unknown>",
            (module_context_path && *module_context_path) ? module_context_path : "<unknown path>",
            dep_error.c_str());
        local_pgm->waitForTerminationAndDeref(nullptr);
        return err;
    }

    // Read reexported module names from metadata
    std::vector<std::string> reexport_deps;
    std::string reexport_error;
    if (!readReexportModules(metadata_reader, reexport_deps, reexport_error)) {
        printd(0, "AOT module v3 '%s': WARNING - failed to read reexport modules: %s\n",
            mod_name, reexport_error.c_str());
        // Non-fatal — continue without reexport info
    }

    // Load each dependency module into this program.
    // runTimeLoadModule will call addToProgram which imports the namespace.
    //
    // The dependency list is built from the parsed program's feature lists at compile time
    // (see serializeProgramFeatureDependencies()), so it lists exactly the modules that were loaded when the
    // .qmod was compiled — including optional %try-module modules whose %ifndef-guarded code
    // (e.g. json's make_json) was baked into the binary.  Such a module is therefore a hard
    // runtime requirement: if it is genuinely unavailable the compiled function/type slots that
    // reference it cannot be registered, which previously surfaced as a cryptic "unsupported AOT
    // slot metadata" error.  Fail here with a clear message instead.
    //
    // A failed load is only tolerated for a circular/in-progress dependency, which is still
    // registered in the module map.  AOT init runs outside the module-manager mutex, so the
    // availability check uses the normal locking lookup.
    for (const std::string& dep : deps) {
        printd(5, "AOT module v3 '%s': loading dependency '%s'\n", mod_name, dep.c_str());
        int rc = MM.runTimeLoadModule(&xsink, dep.c_str(), local_pgm);
        if (rc < 0 || xsink) {
            xsink.clear();
            if (aotRequiredDepUnavailable(dep.c_str())) {
                // Genuinely missing (not a circular/in-progress load) — hard error.
                printd(5, "AOT module v3 '%s': required dependency '%s' is unavailable\n",
                    mod_name, dep.c_str());
                QoreStringNode* err = new QoreStringNodeMaker(
                    "AOT module '%s' (%s) requires module '%s', which could not be loaded; the module "
                    "was AOT-compiled against '%s' (its compiled code references that module's symbols) "
                    "and cannot be loaded without it",
                    mod_name ? mod_name : "<unknown>",
                    module_context_desc.c_str(), dep.c_str(), dep.c_str());
                local_pgm->waitForTerminationAndDeref(nullptr);
                return err;
            }
            // Circular dependency or other in-progress load - tolerate and continue.
            // The types might be resolved later when the requiring script is parsed.
            printd(5, "AOT module v3 '%s': dependency '%s' load tolerated (rc=%d, module present)\n",
                mod_name, dep.c_str(), rc);
        }
    }

    AOT_TRACE("deps loaded");
    printd(5, "AOT module v3 '%s': loaded %d dependencies\n", mod_name, (int)deps.size());

    // Deserialize namespace tree from metadata (replaces source parsing).
    // The metadata contains complete class/namespace structure.  Functions that were
    // successfully compiled to native code will be registered below.  Functions that
    // couldn't be compiled are present as stubs in the metadata.
    // Note: Modules should be compiled with source-first QORE_MODULE_DIR so that all
    // dependency classes/methods are complete during metadata serialization.
    printd(5, "AOT module v3 '%s': starting namespace deserialization\n", mod_name);
    QoreAOTBinaryDeserializer deserializer;
    std::string deser_error;
    {
        // parse_ctx_failed: set parse context threw; deser_failed: metadata read rejected.
        // Both must emit the error OUTSIDE the ProgramRuntimeParseContextHelper scope —
        // waitForTerminationAndDeref blocks until parse_count==0, and pch owns parse_count
        // until it goes out of scope. Calling wait* while pch is live deadlocks.
        bool parse_ctx_failed = false;
        bool deser_failed = false;
        {
            // Set parse context so UserVariantBase constructor can call
            // parse_get_parse_options() which reads thread-local current_pgm
            ProgramRuntimeParseContextHelper pch(&xsink, local_pgm);
            if (xsink.isException()) {
                xsink.clear();
                parse_ctx_failed = true;
            } else if (!deserializer.deserializeIntoProgram(local_pgm,
                    std::move(metadata_reader), deser_error)) {
                deser_failed = true;
            }
        }
        if (parse_ctx_failed) {
            local_pgm->waitForTerminationAndDeref(nullptr);
            return new QoreStringNode("AOT module v3: failed to set parse context");
        }
        if (deser_failed) {
            QoreStringNode* err = new QoreStringNode("AOT module metadata deserialization error: ");
            err->concat(deser_error.c_str());
            local_pgm->waitForTerminationAndDeref(nullptr);
            return err;
        }
    }
    AOT_TRACE("deserializeIntoProgram done");

    std::shared_ptr<const QoreAOTDebugMetadata> debug_metadata;

    // Advisory source staleness check.  Feature compatibility is a hard error
    // in QoreAOTBinaryDeserializer::openAndDeserializeShells() before
    // schema-dependent metadata is read.
    {
        const QoreAOTBinaryHeader& aot_hdr = deserializer.getReader().getHeader();
        if (aot_hdr.source_hash != 0 && label != nullptr) {
            std::ifstream sf(label, std::ios::binary | std::ios::ate);
            if (sf.is_open()) {
                auto sz = sf.tellg();
                if (sz > 0) {
                    std::vector<char> src(static_cast<size_t>(sz));
                    sf.seekg(0);
                    sf.read(src.data(), sz);
                    uint64_t live_hash = XXH64(src.data(), static_cast<size_t>(sz), 0);
                    if (live_hash != aot_hdr.source_hash) {
                        printd(0, "AOT WARNING: binary source hash mismatch for '%s' "
                            "(compiled=0x%016llx, current=0x%016llx); source has changed\n",
                            label,
                            (unsigned long long)aot_hdr.source_hash,
                            (unsigned long long)live_hash);
                    }
                }
            }
        }
    }

    // Register pre-compiled AOT functions using slot maps (v3 uses metadata
    // deserialization — no AST available, must use slot map path)
    if (num_functions > 0 && functions) {
        debug_metadata = makeAOTDebugMetadata(deserializer.getReader(),
            metadata, metadata_len);
        std::unordered_map<std::string, const QoreAOTFunc*> func_map;
        func_map.reserve(static_cast<size_t>(num_functions));
        for (int i = 0; i < num_functions; ++i) {
            if (functions[i].name && functions[i].fn_ptr) {
                func_map[functions[i].name] = &functions[i];
            }
        }

        qore_program_private* pp = qore_program_private::get(*local_pgm);
        int registered = 0;
        qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);
        std::vector<std::string> registration_errors;
        bool registration_parse_ctx_failed = false;
        {
            // Slot-map registration resolves call/constructor variants while
            // rebuilding runtime contexts.  Those lookups consult both the
            // current Program and the runtime parse-options TLS slot, so the
            // module program's context and parse options must be current here;
            // otherwise a requiring program without options such as
            // %allow-debugger can reject valid module-local code.
            ProgramRuntimeParseContextHelper pch(&xsink, local_pgm);
            if (xsink.isException()) {
                xsink.clear();
                registration_parse_ctx_failed = true;
            } else {
                const AbstractStatement* old_stmt = nullptr;
                const QoreProgramLocation* old_loc = nullptr;
                QoreParseOptions old_po;
                if (swap_runtime_statement_location(&xsink, nullptr, nullptr,
                        local_pgm->getParseOptions(), old_stmt, old_loc, old_po)) {
                    xsink.clear();
                    registration_parse_ctx_failed = true;
                } else {
                    registerAOTFunctionsFromSlotMaps(deserializer.getReader(), root_ns,
                        local_pgm, func_map, registered, nullptr, deserializer.getTypeResolver(),
                        &registration_errors, debug_metadata, false, nullptr, nullptr, &deserializer);

                    const AbstractStatement* dummy_stmt = nullptr;
                    const QoreProgramLocation* dummy_loc = nullptr;
                    QoreParseOptions dummy_po;
                    swap_runtime_statement_location(nullptr, old_stmt, old_loc, old_po,
                        dummy_stmt, dummy_loc, dummy_po);
                }
            }
        }

        if (registration_parse_ctx_failed) {
            local_pgm->waitForTerminationAndDeref(nullptr);
            return new QoreStringNode("AOT module v3: failed to set registration parse context");
        }

        if (!registration_errors.empty()) {
            std::string msg = makeAOTRegistrationFailureMessage(mod_name, registered, num_functions,
                &func_map, &registration_errors);
            local_pgm->waitForTerminationAndDeref(nullptr);
            return new QoreStringNode(msg.c_str());
        }

        AOT_TRACE("registerAOTFunctionsFromSlotMaps done");
        printd(1, "AOT module v3 '%s': registered %d/%d pre-compiled functions\n",
            mod_name, registered, num_functions);

        // Source fallback is intentionally not available in v3 objects. Legacy
        // FUNC_SOURCES records with fallback function names are rejected during
        // metadata deserialization; any registration gap below is a hard error.
        assert(!deserializer.hasLegacyFallbackFunctions());
        if (registered < num_functions) {
            std::string msg = makeAOTRegistrationFailureMessage(mod_name, registered, num_functions,
                &func_map, &registration_errors);
            local_pgm->waitForTerminationAndDeref(nullptr);
            return new QoreStringNode(msg.c_str());
        }
    }

    // Read init function descriptors for deferred execution in ns_init.
    // Init functions can't run here because module classes haven't been committed
    // to the target program's namespace tree yet (ns_init does the namespace merge).
    // Object constructors called by init functions need fully resolved class hierarchies.
    // The metadata and function table are stored so ns_init can build contexts using
    // the TARGET program's namespace tree (which has properly committed classes).
    std::vector<AOTInitFuncDescriptor> init_descriptors;
    {
        std::string init_error;
        if (readInitFuncs(deserializer.getReader(), init_descriptors, init_error)) {
            printd(2, "AOT v3 '%s': read %d init descriptors, deferring for ns_init\n",
                mod_name, (int)init_descriptors.size());
            if (aotInitTraceEnabled()) {
                fprintf(stderr, "[aot-init] module_init module=%s descriptors=%zu metadata=%d\n",
                    mod_name, init_descriptors.size(), metadata_len);
            }
        } else {
            // No INIT_FUNCS section or read error — not an error, just no init functions
            printd(5, "AOT v3 '%s': no INIT_FUNCS section: %s\n",
                mod_name, init_error.c_str());
        }
    }

    // Embedded source may be present in explicit --include-source objects for
    // diagnostics, but v3 runtime must not parse it as a recovery path.
    AOT_TRACE("embedded-source check");
    if (_aot_trace && deserializer.hasEmbeddedSource()) {
        fprintf(stderr, "[aot-load] embedded source ignored = %zu bytes\n",
            deserializer.getEmbeddedSourceLen());
    }

    // Store per-module state so ns_init can find the correct program.
    // Also update globals for early ns_init entry.
    {
        AutoLocker aot_state_al(get_aot_module_state_lock());
        aot_module_pgm = local_pgm;
        aot_module_name = mod_name;
        aot_module_path = module_context_path ? module_context_path : "";
        aot_module_funcs = functions;
        aot_module_num_funcs = num_functions;

        AotModuleState state;
        state.pgm = local_pgm;
        state.funcs = functions;
        state.num_funcs = num_functions;
        state.reexport_deps = std::move(reexport_deps);
        if (!init_descriptors.empty()) {
            state.init_descriptors = std::make_shared<const std::vector<AOTInitFuncDescriptor>>(
                std::move(init_descriptors));
        }
        state.path = module_context_path ? module_context_path : "";
        // Keep the open reader and debug payload alive for deferred init instead
        // of copying the complete module metadata again.
        if (state.init_descriptors) {
            static const bool cache_init_reader =
                std::getenv("QORE_DISABLE_AOT_INIT_READER_CACHE") == nullptr;
            if (cache_init_reader) {
                state.init_reader = std::make_shared<QoreAOTBinaryReader>(deserializer.takeReader());
                state.init_reader->wrap_const_ref_in_rcr = false;
                state.init_reader->defer_unresolved_const_refs = false;
            }
            if (useAOTSharedInitMetadata()) {
                state.debug_metadata = debug_metadata;
            }
            if (!cache_init_reader || !useAOTSharedInitMetadata()) {
                state.metadata = std::make_shared<const std::vector<uint8_t>>(
                    metadata, metadata + metadata_len);
            }
        }
        aot_module_map[mod_name] = std::move(state);
    }

    return nullptr;  // success
}

extern "C" DLLEXPORT void qore_aot_fill_module_desc(QoreModuleInfo* mod_info,
        const char* name, const char* version, const char* desc,
        const char* author, const char* url, const char* license_str,
        int api_major, int api_minor, int license,
        void* init_fn, void* ns_init_fn, void* del_fn,
        const char** deps, int num_deps) {
    mod_info->name = name;
    mod_info->version = version;
    mod_info->desc = desc;
    mod_info->author = author;
    if (url) {
        mod_info->url = url;
    }
    mod_info->api_major = api_major;
    unsigned encoded_api_minor = static_cast<unsigned>(api_minor);
    if ((encoded_api_minor & QORE_AOT_MODULE_ABI_API_MINOR_MARKER_MASK)
            == QORE_AOT_MODULE_ABI_API_MINOR_MARKER) {
        mod_info->aot_abi_version =
            (encoded_api_minor & QORE_AOT_MODULE_ABI_VERSION_MASK) >> 8;
        mod_info->api_minor = encoded_api_minor & QORE_AOT_MODULE_API_MINOR_MASK;
    } else {
        mod_info->api_minor = api_minor;
    }
    mod_info->license = static_cast<qore_license_t>(license);
    if (license_str) {
        mod_info->license_str = license_str;
    }
    mod_info->init = reinterpret_cast<qore_module_init_t>(init_fn);
    mod_info->ns_init = reinterpret_cast<qore_module_ns_init_t>(ns_init_fn);
    mod_info->del = reinterpret_cast<qore_module_delete_t>(del_fn);
    mod_info->is_aot = true;
    for (int i = 0; i < num_deps; ++i) {
        if (deps[i]) {
            mod_info->dependencies.push_back(deps[i]);
        }
    }
}

//! Delivers %try-child-module declarations from an AOT-compiled module's description function
/** Called after qore_aot_fill_module_desc() and only when the module declares child modules; artifacts
    compiled before child modules were supported never call this function.

    @see design/qore-module-structure.md "Child Modules"
*/
extern "C" DLLEXPORT void qore_aot_fill_module_children(QoreModuleInfo* mod_info, const char** children,
        int num_children) {
    for (int i = 0; i < num_children; ++i) {
        if (children[i]) {
            mod_info->child_modules.push_back(children[i]);
        }
    }
}

extern "C" DLLEXPORT void qore_aot_raise_init_error(ExceptionSink* xsink, QoreStringNode* err) {
    if (err) {
        xsink->raiseException("MODULE-LOAD-ERROR", err);
    }
}

extern "C" DLLEXPORT void qore_aot_register_into_program(QoreProgram* tpgm,
        void (*desc_fn)(QoreModuleInfo&), const char* path) {
    ExceptionSink xsink;
    MM.registerAOTStaticModule(&xsink, tpgm,
        reinterpret_cast<qore_binary_module_desc_t>(desc_fn),
        path ? path : "<aot-static>");
    if (xsink) {
        xsink.handleExceptions();
    }
}

// Phase 4 slice 10d: script-context register path.  Called by the
// glue object that qcc emits for `qcc -o binary *.qo` (slice 10d).
// Differs from qore_aot_register_into_program / qore_aot_module_init_v3:
//
//   * NO shadow module program created.  Metadata is deserialized
//     straight into the caller's @p tpgm via
//     QoreAOTBinaryDeserializer::deserializeIntoProgram.
//   * NO entry is added to aot_module_map — this is not a module.
//   * NO %requires / user-feature registration — cross-file
//     dependencies within the script are resolved via the metadata
//     blob alone.
//   * Non-public classes stay visible to tpgm (the module-merge
//     public-filter path is bypassed entirely).
//
// Runs the standard sequence exactly once:
//   1. deserializeIntoProgram — rebuilds namespace tree + classes +
//      functions + methods in tpgm.
//   2. registerAOTFunctionsFromSlotMaps — wires pre-compiled function
//      pointers to their variants; missing slot maps are hard errors.
//   3. (no init-funcs handling yet — slice 10d MVP; NS/CLASS_CONSTANT /
//      STATIC_VAR init expressions are deferred to a follow-up).
//
// @param tpgm the target QoreProgram (caller-owned)
// @param metadata serialized binary metadata blob (uncompressed)
// @param metadata_len length of @p metadata in bytes
// Phase 4 slice 10g: per-program batch state for out-of-order
// registration.  Between qore_aot_script_begin_batch and
// qore_aot_script_end_batch, qore_aot_script_register calls only
// stash blobs. end_batch first replays all serialized module commands,
// then runs phase-1 shell creation via QoreAOTBinaryMultiDeserializer,
// phase-2 resolution + function registration + init execution
// atomically for all accumulated blobs, so cross-file inheritance /
// type refs resolve regardless of register call order.
//
// Stored on the target QoreProgram via setExternalData — no global
// mutex needed, lifecycle bound to the program.
namespace {
struct AotScriptDeferredBlob {
    const uint8_t* metadata;
    int metadata_len;
    std::string label;
    const QoreAOTFunc* functions;
    int num_functions;
    bool native_only = false;
};

class AotScriptBatchState : public AbstractQoreProgramExternalData {
public:
    QoreAOTBinaryMultiDeserializer mdes;
    std::vector<AotScriptDeferredBlob> deferred;

    explicit AotScriptBatchState(QoreProgram* pgm) : mdes(pgm) {
    }

    // Child programs start empty — a batch is transient.
    AbstractQoreProgramExternalData* copy(QoreProgram*) const override {
        return nullptr;
    }
    void doDeref() override { delete this; }
};

constexpr const char* kAotScriptBatchKey = "qore_aot_script_batch";
}  // anonymous namespace

static int qore_aot_script_register_native_impl(QoreProgram* tpgm,
        const uint8_t* metadata, int metadata_len,
        const char* label,
        const QoreAOTFunc* functions, int num_functions,
        std::vector<AOTInitFuncExecInfo>* batch_init_contexts = nullptr,
        std::vector<AOTInitFuncDescriptor>* batch_init_descriptors = nullptr,
        bool allow_unlinked_native_inputs = false) {
    if (!tpgm || !metadata || metadata_len <= 0) {
        return 1;
    }
    if (!functions || num_functions <= 0) {
        return 0;
    }

    ExceptionSink xsink;
    ProgramRuntimeParseContextHelper pch(&xsink, tpgm);
    if (xsink.isException()) {
        xsink.handleExceptions();
        return 2;
    }

    QoreAOTBinaryReader reader;
    std::string read_error;
    if (!reader.open(metadata, static_cast<uint32_t>(metadata_len), read_error)) {
        fprintf(stderr, "qore_aot_script_register_native(%s): metadata open failed: %s\n",
            label ? label : "<script>", read_error.c_str());
        return 3;
    }
    uint64_t unsupported = reader.getHeader().feature_flags & ~QORE_AOT_SUPPORTED_FEATURES;
    if (unsupported) {
        fprintf(stderr, "qore_aot_script_register_native(%s): unsupported feature flags 0x%016llx\n",
            label ? label : "<script>", static_cast<unsigned long long>(unsupported));
        return 3;
    }

    std::unordered_map<std::string, const QoreAOTFunc*> func_map;
    for (int i = 0; i < num_functions; ++i) {
        if (i && !(i % 100)
                && qore_check_cancel(&xsink, "AOT script native function map build")) {
            xsink.handleExceptions();
            return 2;
        }
        if (functions[i].name && functions[i].fn_ptr) {
            func_map[functions[i].name] = &functions[i];
        }
    }
    if (func_map.empty()) {
        return 0;
    }

    qore_program_private* pp = qore_program_private::get(*tpgm);
    qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);
    int registered = 0;
    int ignored_unlinked = 0;
    std::vector<AOTInitFuncExecInfo> init_func_contexts;
    std::vector<std::string> registration_errors;
    auto debug_metadata = makeAOTDebugMetadata(reader, metadata, metadata_len);
    registerAOTFunctionsFromSlotMaps(reader, root_ns, tpgm, func_map, registered,
        &init_func_contexts, nullptr, &registration_errors, debug_metadata,
        allow_unlinked_native_inputs, &ignored_unlinked);
    printd(1, "qore_aot_script_register_native(%s): registered %d/%d "
        "pre-compiled functions (%d init funcs, %d unlinked native bodies ignored)\n",
        label ? label : "<script>", registered, num_functions,
        (int)init_func_contexts.size(), ignored_unlinked);
    if (!registration_errors.empty() || registered < num_functions) {
        std::string msg = makeAOTRegistrationFailureMessage(label, registered, num_functions,
            &func_map, &registration_errors);
        fprintf(stderr, "qore_aot_script_register_native: %s\n", msg.c_str());
        return 4;
    }

    if (!init_func_contexts.empty()) {
        std::vector<AOTInitFuncDescriptor> init_descriptors;
        std::string init_err;
        if (!readInitFuncs(reader, init_descriptors, init_err)) {
            fprintf(stderr, "qore_aot_script_register_native(%s): init-func "
                "descriptor read failed: %s\n",
                label ? label : "<script>", init_err.c_str());
            return 5;
        }
        if (!init_descriptors.empty()) {
            if (batch_init_contexts && batch_init_descriptors) {
                batch_init_contexts->insert(batch_init_contexts->end(),
                    init_func_contexts.begin(), init_func_contexts.end());
                batch_init_descriptors->insert(batch_init_descriptors->end(),
                    init_descriptors.begin(), init_descriptors.end());
            } else {
                ExceptionSink tch_xsink;
                ProgramThreadCountContextHelper tch(&tch_xsink, tpgm, false);
                if (tch_xsink.isException()) {
                    tch_xsink.handleExceptions();
                    return 6;
                }
                executeInitFunctions(tpgm, init_func_contexts,
                    init_descriptors, label ? label : "<script>",
                    /*shadow_pgm=*/nullptr, /*mod_path=*/nullptr,
                    /*write_shadow=*/true, /*failure_sink=*/nullptr);
            }
        }
    }

    if (!batch_init_contexts) {
        preInitStaticVarsInProgram(tpgm);
    }
    return 0;
}

// @param label label for diagnostics (source path)
// @param functions array of pre-compiled function descriptors
// @param num_functions entries in @p functions
// @return 0 on success; non-zero on deserialization or registration
//         failure.  Errors are printed to stderr via ExceptionSink.
extern "C" DLLEXPORT int qore_aot_script_register(QoreProgram* tpgm,
        const uint8_t* metadata, int metadata_len,
        const char* label,
        const QoreAOTFunc* functions, int num_functions) {
    if (!tpgm || !metadata || metadata_len <= 0) {
        return 1;
    }

    // Phase 4 slice 10g: if a batch is active on tpgm, defer to
    // end_batch: only load shells now (phase 1), stash
    // (funcs, metadata, label) for later.  Cross-file base-class
    // and type lookups will resolve in end_batch's resolveAll
    // regardless of the order in which register was called.
    {
        AbstractQoreProgramExternalData* ext =
            tpgm->getExternalData(kAotScriptBatchKey);
        if (ext) {
            auto* batch = static_cast<AotScriptBatchState*>(ext);
            AotScriptDeferredBlob d;
            d.metadata = metadata;
            d.metadata_len = metadata_len;
            d.label = label ? label : "<script>";
            d.functions = functions;
            d.num_functions = num_functions;
            d.native_only = false;
            batch->deferred.push_back(std::move(d));
            return 0;
        }
    }

    ExceptionSink xsink;

    {
        std::vector<std::string> prepended, appended;
        std::string mp_error;
        if (readModulePathLists(metadata, static_cast<uint32_t>(metadata_len),
                prepended, appended, mp_error)) {
            applyModulePathListsToProgram(tpgm, prepended, appended);
        }
    }

    {
        std::string cmd_error;
        if (!applyAOTModuleCommandsToProgram(tpgm, metadata,
                static_cast<uint32_t>(metadata_len), label, cmd_error)) {
            fprintf(stderr, "qore_aot_script_register(%s): "
                "module-command replay failed: %s\n",
                label ? label : "<script>", cmd_error.c_str());
            return 5;
        }
    }

    // Load module dependencies before deserialization so that:
    //   1. Module-contributed namespaces, classes, functions, etc. are properly registered
    //      via the standard QoreBuiltinModule::addToProgramImpl path, which calls
    //      addFeature()/commitFeature().  Without this, the AOT metadata-deserialization
    //      path skips feature tracking, leaving runTimeLoadModule()'s lock-free fast path
    //      blind to modules that were %requires'd at AOT compile time - any subsequent
    //      load_module(name) call goes through the slow path, tries to re-load the module,
    //      and fails with "Namespace 'X' already exists" if the AOT metadata had also
    //      deserialized any items into that namespace.
    //   2. Base-class and type references inside the deserialized metadata resolve.
    // Mirrors the AOT v2 / module-init dependency-load step.
    {
        std::vector<std::string> deps;
        std::string dep_error;
        if (readDependencies(metadata, static_cast<uint32_t>(metadata_len), deps, dep_error)) {
            printd(2, "qore_aot_script_register(%s): loading %d dependencies\n",
                label ? label : "<script>", (int)deps.size());
            size_t dep_cancel_i = 0;
            for (const std::string& dep : deps) {
                if (dep_cancel_i && !(dep_cancel_i % 100)
                        && qore_check_cancel(&xsink, "AOT script dependency load")) {
                    xsink.handleExceptions();
                    return 6;
                }
                int dep_rc = MM.runTimeLoadModule(&xsink, dep.c_str(), tpgm);
                if (dep_rc < 0 || xsink.isException()) {
                    printd(2, "qore_aot_script_register(%s): dependency '%s' load error "
                        "(rc=%d)\n", label ? label : "<script>", dep.c_str(), dep_rc);
                    xsink.clear();
                    if (aotRequiredDepUnavailable(dep.c_str())) {
                        xsink.raiseException("AOT-ERROR",
                            "the AOT-compiled script (%s) requires module '%s', which could not be "
                            "loaded; the script was AOT-compiled against '%s' (its compiled code "
                            "references that module's symbols) and cannot be loaded without it",
                            label ? label : "<script>", dep.c_str(), dep.c_str());
                        xsink.handleExceptions();
                        return 7;
                    }
                    // else: circular/in-progress dependency (still registered) - tolerate.
                }
                ++dep_cancel_i;
            }
        }
    }

    {
        // ProgramRuntimeParseContextHelper is required so
        // UserVariantBase::UserVariantBase (called during method
        // deserialization) can read parse_get_parse_options().
        // Same invariant qore_aot_module_init_v3 observes.
        ProgramRuntimeParseContextHelper pch(&xsink, tpgm);
        if (xsink.isException()) {
            xsink.handleExceptions();
            return 2;
        }

        QoreAOTBinaryDeserializer deserializer;
        std::string deser_error;
        if (!deserializer.deserializeIntoProgram(tpgm, metadata,
                static_cast<uint32_t>(metadata_len), deser_error)) {
            fprintf(stderr, "qore_aot_script_register(%s): metadata "
                "deserialization failed: %s\n",
                label ? label : "<script>", deser_error.c_str());
            return 3;
        }

        // Register pre-compiled function pointers from slot maps only.
        // Missing slot maps are hard errors; source fallback must not hide
        // incomplete metadata.
        //
        // Registration also collects init-function execution
        // contexts (for NS_CONSTANT / CLASS_CONSTANT / STATIC_VAR /
        // MODULE_INIT entries) into `init_func_contexts`, to be
        // consumed below by executeInitFunctions — same pattern as
        // qore_aot_module_init_v3.
        std::vector<AOTInitFuncExecInfo> init_func_contexts;
        if (num_functions > 0 && functions) {
            std::unordered_map<std::string, const QoreAOTFunc*> func_map;
            for (int i = 0; i < num_functions; ++i) {
                if (functions[i].name && functions[i].fn_ptr) {
                    func_map[functions[i].name] = &functions[i];
                }
            }

            qore_program_private* pp = qore_program_private::get(*tpgm);
            qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);
            int registered = 0;
            std::vector<std::string> registration_errors;
            auto debug_metadata = makeAOTDebugMetadata(deserializer.getReader(),
                metadata, metadata_len);
            registerAOTFunctionsFromSlotMaps(deserializer.getReader(), root_ns,
                tpgm, func_map, registered, &init_func_contexts, nullptr,
                &registration_errors, debug_metadata, false, nullptr, nullptr, &deserializer);
            printd(1, "qore_aot_script_register(%s): registered %d/%d "
                "pre-compiled functions (%d init funcs)\n",
                label ? label : "<script>", registered, num_functions,
                (int)init_func_contexts.size());
            if (!registration_errors.empty() || registered < num_functions) {
                std::string msg = makeAOTRegistrationFailureMessage(label, registered, num_functions,
                    &func_map, &registration_errors);
                fprintf(stderr, "qore_aot_script_register: %s\n", msg.c_str());
                return 4;
            }
        }

        // Phase 4 slice 10f: execute init functions (NS_CONSTANT /
        // CLASS_CONSTANT / STATIC_VAR / MODULE_INIT) against tpgm.
        // Mirrors what qore_aot_module_init_v3 does at ~line 5497 for
        // module loads — but with shadow_pgm=nullptr since script mode
        // has no shadow program.  Init functions populate constant
        // values + static var values that the user's runtime code
        // depends on (e.g., `const MyConst = compute_value();`).
        //
        // Note (slice 10g): this path runs only when no batch is
        // active — i.e. immediate register.  Batched register calls
        // stash the init descriptors in AotScriptBatchState::deferred
        // and are executed in bulk by qore_aot_script_end_batch after
        // all blobs' shells are in.
        if (!init_func_contexts.empty()) {
            std::vector<AOTInitFuncDescriptor> init_descriptors;
            std::string init_err;
            if (readInitFuncs(metadata,
                    static_cast<uint32_t>(metadata_len),
                    init_descriptors, init_err)) {
                if (!init_descriptors.empty()) {
                    ExceptionSink tch_xsink;
                    ProgramThreadCountContextHelper tch(&tch_xsink,
                        tpgm, false);
                    if (tch_xsink.isException()) {
                        tch_xsink.handleExceptions();
                    } else {
                        executeInitFunctions(tpgm, init_func_contexts,
                            init_descriptors,
                            label ? label : "<script>",
                            /*shadow_pgm=*/nullptr,
                            /*mod_path=*/nullptr,
                            /*write_shadow=*/true,
                            /*failure_sink=*/nullptr);
                    }
                }
            } else {
                fprintf(stderr, "qore_aot_script_register(%s): init-func "
                    "descriptor read failed: %s\n",
                    label ? label : "<script>", init_err.c_str());
            }
        }
        preInitStaticVarsInProgram(tpgm);
    }

    return 0;
}

extern "C" DLLEXPORT int qore_aot_script_register_native(QoreProgram* tpgm,
        const uint8_t* metadata, int metadata_len,
        const char* label,
        const QoreAOTFunc* functions, int num_functions) {
    if (!tpgm || !metadata || metadata_len <= 0) {
        return 1;
    }

    {
        AbstractQoreProgramExternalData* ext =
            tpgm->getExternalData(kAotScriptBatchKey);
        if (ext) {
            auto* batch = static_cast<AotScriptBatchState*>(ext);
            AotScriptDeferredBlob d;
            d.metadata = metadata;
            d.metadata_len = metadata_len;
            d.label = label ? label : "<script>";
            d.functions = functions;
            d.num_functions = num_functions;
            d.native_only = true;
            batch->deferred.push_back(std::move(d));
            return 0;
        }
    }

    return qore_aot_script_register_native_impl(tpgm, metadata, metadata_len,
        label, functions, num_functions);
}

// Phase 4 slice 10g: begin a batch of deferred script registrations.
// Between begin and end, each qore_aot_script_register call only
// stashes (funcs, metadata, label) for the end-batch flush.  Multiple
// begin calls replace any prior
// unflushed state (with a warning) — a host error but non-fatal.
extern "C" DLLEXPORT void qore_aot_script_begin_batch(QoreProgram* tpgm) {
    if (!tpgm) {
        return;
    }
    AbstractQoreProgramExternalData* existing =
        tpgm->getExternalData(kAotScriptBatchKey);
    if (existing) {
        fprintf(stderr, "qore_aot_script_begin_batch: batch already "
            "active on %p, replacing (host error; forgotten end_batch?)\n",
            (void*)tpgm);
        AbstractQoreProgramExternalData* removed =
            tpgm->removeExternalData(kAotScriptBatchKey);
        if (removed) {
            removed->doDeref();
        }
    }
    tpgm->setExternalData(kAotScriptBatchKey, new AotScriptBatchState(tpgm));
}

// Phase 4 slice 10g: flush a batch.  Runs phase-2 cross-blob
// resolution (QoreAOTBinaryMultiDeserializer::resolveAll), then
// registers every blob's functions, then executes all init funcs as a
// single cross-blob fixpoint.  If no batch is active, returns 0 (no-op).
//
// Init-func ordering: descriptors keep compile-time declaration order
// within each blob and batch insertion order across blobs.  The combined
// execution lets executeInitFunctions() retry earlier descriptors after
// later fragments have populated their dependencies, matching source parse
// behavior for split scripts where constants can depend on declarations in
// later files.
extern "C" DLLEXPORT int qore_aot_script_end_batch(QoreProgram* tpgm) {
    if (!tpgm) {
        return -1;
    }
    AbstractQoreProgramExternalData* ext =
        tpgm->removeExternalData(kAotScriptBatchKey);
    if (!ext) {
        return 0;  // no batch; no-op
    }
    std::unique_ptr<AotScriptBatchState> batch(
        static_cast<AotScriptBatchState*>(ext));

    ExceptionSink xsink;
    int rc = 0;

    // Phase-timing instrumentation (post-resolveAll register +
    // init-func execution passes).  MultiDeserializer reports its
    // own resolveAll phase breakdown via QORE_AOT_PHASE_TIMING in
    // its destructor; these counters cover the work that happens
    // AFTER resolveAll but inside end_batch.
    const bool time_on = getenv("QORE_AOT_PHASE_TIMING") != nullptr;
    auto now_us = [] () -> uint64_t {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000ULL;
    };
    uint64_t us_register = 0;
    uint64_t us_init = 0;

    {
        size_t setup_cancel_i = 0;
        for (auto& d : batch->deferred) {
            if (d.native_only) {
                continue;
            }
            if (setup_cancel_i && !(setup_cancel_i % 100)
                    && qore_check_cancel(&xsink, "AOT script batch module setup")) {
                xsink.handleExceptions();
                return 8;
            }

            std::vector<std::string> prepended, appended;
            std::string mp_error;
            if (readModulePathLists(d.metadata, static_cast<uint32_t>(d.metadata_len),
                    prepended, appended, mp_error)) {
                applyModulePathListsToProgram(tpgm, prepended, appended);
            }

            std::string cmd_error;
            if (!applyAOTModuleCommandsToProgram(tpgm, d.metadata,
                    static_cast<uint32_t>(d.metadata_len),
                    d.label.c_str(), cmd_error)) {
                fprintf(stderr, "qore_aot_script_end_batch(%s): "
                    "module-command replay failed: %s\n",
                    d.label.c_str(), cmd_error.c_str());
                return 6;
            }
            ++setup_cancel_i;
        }
    }

    // Load module dependencies for every deferred blob BEFORE entering the
    // parse-context helper.  Each blob's dependency list comes from the AOT
    // compile-time featureList capture; running them through MM.runTimeLoadModule
    // funnels module-contributed namespaces through QoreBuiltinModule::addToProgramImpl,
    // which is what populates featureList/committedFeatureList.  Without this step,
    // a subsequent runtime load_module(name) call (e.g. defensive load_module("json")
    // in a binary that also has %requires json) cannot hit the lock-free fast path,
    // goes through the slow path, and tries to re-register namespaces that the AOT
    // metadata-deserialization step below would otherwise leave invisible to the
    // feature tracker - raising "Namespace 'X' already exists in '::Qore'".
    // Mirrors qore_aot_run_v2 / qore_aot_module_init_v3.
    {
        size_t dep_cancel_i = 0;
        for (auto& d : batch->deferred) {
            if (d.native_only) {
                continue;
            }
            if (dep_cancel_i && !(dep_cancel_i % 100)
                    && qore_check_cancel(&xsink, "AOT script batch dependency load")) {
                xsink.handleExceptions();
                return 11;
            }
            std::vector<std::string> deps;
            std::string dep_error;
            if (readDependencies(d.metadata, static_cast<uint32_t>(d.metadata_len),
                    deps, dep_error)) {
                size_t dep_load_i = 0;
                for (const std::string& dep : deps) {
                    if (dep_load_i && !(dep_load_i % 100)
                            && qore_check_cancel(&xsink, "AOT script batch dependency module load")) {
                        xsink.handleExceptions();
                        return 12;
                    }
                    int dep_rc = MM.runTimeLoadModule(&xsink, dep.c_str(), tpgm);
                    if (dep_rc < 0 || xsink.isException()) {
                        printd(2, "qore_aot_script_end_batch(%s): dependency '%s' load "
                            "error (rc=%d)\n", d.label.c_str(), dep.c_str(), dep_rc);
                        xsink.clear();
                        if (aotRequiredDepUnavailable(dep.c_str())) {
                            xsink.raiseException("AOT-ERROR",
                                "the AOT-compiled script (%s) requires module '%s', which could not be "
                                "loaded; the script was AOT-compiled against '%s' (its compiled code "
                                "references that module's symbols) and cannot be loaded without it",
                                d.label.c_str(), dep.c_str(), dep.c_str());
                            xsink.handleExceptions();
                            return 13;
                        }
                        // else: circular/in-progress dependency (still registered) - tolerate.
                    }
                    ++dep_load_i;
                }
            }
            ++dep_cancel_i;
        }
    }

    {
        ProgramRuntimeParseContextHelper pch(&xsink, tpgm);
        if (xsink.isException()) {
            xsink.handleExceptions();
            return 2;
        }

        size_t cancel_i = 0;
        for (auto& d : batch->deferred) {
            if (d.native_only) {
                continue;
            }
            if (cancel_i && !(cancel_i % 100)
                    && qore_check_cancel(&xsink, "AOT script batch metadata deserialization")) {
                xsink.handleExceptions();
                return 9;
            }
            std::string err;
            if (!batch->mdes.addBlob(d.metadata,
                    static_cast<uint32_t>(d.metadata_len), err)) {
                fprintf(stderr, "qore_aot_script_end_batch(%s): "
                    "addBlob failed: %s\n", d.label.c_str(), err.c_str());
                return 7;
            }
            ++cancel_i;
        }

        // Phase 2: cross-blob resolution.
        std::string resolve_err;
        if (!batch->mdes.resolveAll(resolve_err)) {
            fprintf(stderr, "qore_aot_script_end_batch: resolveAll "
                "failed: %s\n", resolve_err.c_str());
            return 3;
        }

        // Per-blob registration.  Each session owns
        // a reader; registerAOTFunctionsFromSlotMaps needs that
        // reader to enumerate slot-map functions.  Init-function
        // contexts/descriptors are collected and executed only after every
        // fragment has registered its callable bodies so cross-fragment
        // constant/static dependencies can be retried in one fixpoint.
        qore_program_private* pp = qore_program_private::get(*tpgm);
        qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);

        const size_t n = batch->deferred.size();
        size_t metadata_blobs = 0;
        size_t count_cancel_i = 0;
        for (const auto& d : batch->deferred) {
            if (count_cancel_i && !(count_cancel_i % 100)
                    && qore_check_cancel(&xsink, "AOT script batch metadata count")) {
                xsink.handleExceptions();
                return 2;
            }
            ++count_cancel_i;
            if (!d.native_only) {
                ++metadata_blobs;
            }
        }
        if (metadata_blobs != batch->mdes.sessionCount()) {
            fprintf(stderr, "qore_aot_script_end_batch: internal: "
                "metadata=%zu vs sessions=%zu mismatch\n",
                metadata_blobs, batch->mdes.sessionCount());
            return 4;
        }

        std::vector<AOTInitFuncExecInfo> batch_init_contexts;
        std::vector<AOTInitFuncDescriptor> batch_init_descriptors;

        size_t session_idx = 0;
        for (size_t i = 0; i < n; ++i) {
            auto& d = batch->deferred[i];
            if (d.native_only) {
                uint64_t t0 = time_on ? now_us() : 0;
                int native_rc = qore_aot_script_register_native_impl(tpgm,
                    d.metadata, d.metadata_len, d.label.c_str(),
                    d.functions, d.num_functions,
                    &batch_init_contexts, &batch_init_descriptors,
                    true);
                if (time_on) {
                    us_register += now_us() - t0;
                }
                if (native_rc != 0) {
                    fprintf(stderr, "qore_aot_script_end_batch(%s): "
                        "native register failed rc=%d\n",
                        d.label.c_str(), native_rc);
                    return native_rc;
                }
                continue;
            }
            std::unordered_map<std::string, const QoreAOTFunc*> func_map;
            if (d.functions) {
                for (int j = 0; j < d.num_functions; ++j) {
                    if (d.functions[j].name && d.functions[j].fn_ptr) {
                        func_map[d.functions[j].name] = &d.functions[j];
                    }
                }
            }
            std::vector<AOTInitFuncExecInfo> init_func_contexts;
            int registered = 0;
            auto& session = batch->mdes.session(session_idx++);
            std::vector<std::string> registration_errors;
            uint64_t t0 = time_on ? now_us() : 0;
            if (!func_map.empty()) {
                auto debug_metadata = makeAOTDebugMetadata(session.getReader(),
                    d.metadata, d.metadata_len);
                registerAOTFunctionsFromSlotMaps(session.getReader(), root_ns,
                    tpgm, func_map, registered, &init_func_contexts,
                    session.getTypeResolver(), &registration_errors, debug_metadata,
                    false, nullptr, nullptr, &session);
            }
            if (time_on) {
                us_register += now_us() - t0;
            }
            printd(1, "qore_aot_script_end_batch(%s): registered %d/%d "
                "pre-compiled functions (%d init funcs)\n",
                d.label.c_str(), registered, d.num_functions,
                (int)init_func_contexts.size());
            if (!registration_errors.empty() || registered < d.num_functions) {
                std::string msg = makeAOTRegistrationFailureMessage(
                    d.label.c_str(), registered, d.num_functions, &func_map,
                    &registration_errors);
                fprintf(stderr, "qore_aot_script_end_batch: %s\n", msg.c_str());
                return 5;
            }

            if (!init_func_contexts.empty()) {
                std::vector<AOTInitFuncDescriptor> init_descriptors;
                std::string init_err;
                if (readInitFuncs(d.metadata,
                        static_cast<uint32_t>(d.metadata_len),
                        init_descriptors, init_err)) {
                    if (!init_descriptors.empty()) {
                        batch_init_contexts.insert(batch_init_contexts.end(),
                            init_func_contexts.begin(), init_func_contexts.end());
                        batch_init_descriptors.insert(batch_init_descriptors.end(),
                            init_descriptors.begin(), init_descriptors.end());
                    }
                } else {
                    fprintf(stderr, "qore_aot_script_end_batch(%s): "
                        "init-func descriptor read failed: %s\n",
                        d.label.c_str(), init_err.c_str());
                }
            }
        }
        if (!batch_init_contexts.empty() && !batch_init_descriptors.empty()) {
            ExceptionSink tch_xsink;
            ProgramThreadCountContextHelper tch(&tch_xsink, tpgm, false);
            if (tch_xsink.isException()) {
                tch_xsink.handleExceptions();
            } else {
                uint64_t t1 = time_on ? now_us() : 0;
                executeInitFunctions(tpgm, batch_init_contexts,
                    batch_init_descriptors, "<script-batch>",
                    /*shadow_pgm=*/nullptr,
                    /*mod_path=*/nullptr,
                    /*write_shadow=*/true,
                    /*failure_sink=*/nullptr);
                if (time_on) {
                    us_init += now_us() - t1;
                }
            }
        }
        preInitStaticVarsInProgram(tpgm);
    }

    if (time_on) {
        fprintf(stderr, "[aot-timing] post-resolveAll: "
            "registerAOTFunctions=%lu us  executeInitFunctions=%lu us\n",
            (unsigned long)us_register, (unsigned long)us_init);
        fflush(stderr);
    }

    return rc;
}
