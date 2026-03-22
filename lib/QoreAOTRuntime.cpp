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

#include <qore/Qore.h>

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
#include "qore/intern/CaseNodeRegex.h"
#include "qore/intern/SwitchStatement.h"

#include "qore/intern/ModuleInfo.h"
#include "qore/intern/VarRefNode.h"
#include "qore/intern/ParseReferenceNode.h"
#include "qore/intern/qore_thread_intern.h"
#include "qore/intern/SelfVarrefNode.h"
#include "qore/intern/ScopedObjectCallNode.h"
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
#include "qore/intern/QoreFoldlOperatorNode.h"
#include "qore/intern/QoreRegex.h"
#include "qore/intern/QoreRegexSubst.h"
#include "qore/intern/QoreTransliteration.h"
#include "qore/intern/QoreClosureParseNode.h"
#include "qore/intern/qore_list_private.h"
#include <qore/QoreNumberNode.h>
#include <qore/BinaryNode.h>

#include <cassert>
#include <cstring>
#include <string>
#include <deque>
#include <unordered_map>
#include <fstream>
#include <vector>

// Defined in Function.cpp - collects all local variables from a StatementBlock and nested blocks
extern void collectAllStatementLocals(const StatementBlock* block, std::vector<LocalVar*>& locals);
// collectStmtSlotStatements() declared in QoreAOT.h, defined in Function.cpp

// Defined in QoreAOT.cpp - generates unique variant key with parameter types
extern std::string getVariantKey(const char* name, const AbstractQoreFunctionVariant* variant);

// ---- Slot Map Context Builder (V2 — no IR re-lowering) ----

//! Helper to convert a QoreValue to NaN-boxed bits
static inline uint64_t toBitsNB(QoreValue v) {
    uint64_t bits;
    memcpy(&bits, &v, sizeof(bits));
    return bits;
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
static uint64_t resolveExprSlot(AOTExprKind kind, const char* ref1, const char* ref2,
        QoreProgram* pgm) {
    qore_program_private* pp = qore_program_private::get(*pgm);
    qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);

    switch (kind) {
        case AOTExprKind::FUNC_CALL: {
            if (!ref1 || !*ref1) {
                return 0;
            }
            // Look up function by name
            const FunctionEntry* fe = qore_root_ns_private::runtimeFindFunctionEntry(
                *pp->RootNS, ref1);
            if (!fe) {
                printd(0, "AOT v2: cannot resolve function '%s' for expr slot\n", ref1);
                return 0;
            }
            // Create a FunctionCallNode with no args (args handled by native code)
            FunctionCallNode* fcn = new FunctionCallNode(&loc_builtin, fe, (QoreListNode*)nullptr, pgm);
            return toBitsNB(QoreValue(fcn));
        }

        case AOTExprKind::STATIC_METHOD_CALL: {
            if (!ref1 || !ref2) {
                return 0;
            }
            // Look up class, then find static method
            const qore_ns_private* found_ns = nullptr;
            const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
                *pp->RootNS, ref1, found_ns);
            if (!qc) {
                printd(0, "AOT v2: cannot resolve class '%s' for static method '%s'\n", ref1, ref2);
                return 0;
            }
            const QoreMethod* m = qc->findStaticMethod(ref2);
            // Fall back to parse-time lookup for not-yet-initialized classes
            if (!m) {
                qore_class_private* qcp = qore_class_private::get(
                    *const_cast<QoreClass*>(qc));
                m = qcp->parseFindLocalStaticMethod(ref2);
            }
            if (!m) {
                printd(0, "AOT v2: cannot find static method '%s::%s'\n", ref1, ref2);
                return 0;
            }
            // Create StaticMethodCallNode
            StaticMethodCallNode* smcn = new StaticMethodCallNode(&loc_builtin, m, (QoreParseListNode*)nullptr);
            return toBitsNB(QoreValue(smcn));
        }

        case AOTExprKind::SELF_METHOD_CALL: {
            if (!ref2 || !*ref2) {
                return 0;
            }
            // Look up class, then find method
            const QoreClass* qc = nullptr;
            if (ref1 && *ref1) {
                const qore_ns_private* found_ns = nullptr;
                qc = qore_root_ns_private::runtimeFindClass(*pp->RootNS, ref1, found_ns);
            }
            if (!qc) {
                printd(1, "AOT SLOT: cannot resolve class '%s' for self method '%s'\n",
                    ref1 ? ref1 : "(null)", ref2);
                return 0;
            }
            const QoreMethod* m = qc->findMethod(ref2);
            if (!m) {
                m = qc->findStaticMethod(ref2);
            }
            // Fall back to parse-time lookup for not-yet-initialized classes
            if (!m) {
                qore_class_private* qcp = qore_class_private::get(
                    *const_cast<QoreClass*>(qc));
                m = qcp->parseFindLocalMethod(ref2);
                if (!m) {
                    m = qcp->parseFindLocalStaticMethod(ref2);
                }
            }
            if (!m) {
                printd(1, "AOT SLOT: cannot find method '%s::%s'\n", ref1, ref2);
                return 0;
            }
            printd(5, "AOT SLOT: resolved self method '%s::%s' -> %p\n", ref1, ref2, m);
            SelfFunctionCallNode* sfcn = new SelfFunctionCallNode(&loc_builtin, strdup(ref2), nullptr, m,
                m->getClass(), qore_class_private::get(*m->getClass()));
            return toBitsNB(QoreValue(sfcn));
        }

        case AOTExprKind::NEW_OBJECT: {
            if (!ref1 || !*ref1) {
                return 0;
            }
            const qore_ns_private* found_ns = nullptr;
            const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
                *pp->RootNS, ref1, found_ns);
            if (!qc) {
                printd(0, "AOT v2: cannot resolve class '%s' for new object\n", ref1);
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
            NewObjectCallNode* nocn = new NewObjectCallNode(qc, nullptr);
            printd(5, "  nocn variant=%p\n", (void*)nocn->getVariant());
            return toBitsNB(QoreValue(nocn));
        }

        case AOTExprKind::SCOPED_NEW_OBJECT: {
            if (!ref1 || !*ref1) {
                return 0;
            }
            const qore_ns_private* found_ns = nullptr;
            const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
                *pp->RootNS, ref1, found_ns);
            if (!qc) {
                printd(0, "AOT v2: cannot resolve class '%s' for scoped new object\n", ref1);
                return 0;
            }
            ScopedObjectCallNode* socn = new ScopedObjectCallNode(&loc_builtin, qc, nullptr);
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
            return toBitsNB(QoreValue(new QoreStringNode(ref1 ? ref1 : "")));
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
            const qore_ns_private* found_ns = nullptr;
            const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
                *pp->RootNS, ref1, found_ns);
            if (!qc) {
                printd(0, "AOT v2: cannot resolve class '%s' for static var '%s'\n", ref1, ref2);
                return 0;
            }
            const QoreExternalStaticMember* m = qc->findLocalStaticMember(ref2);
            if (!m) {
                printd(0, "AOT v2: cannot find static var '%s::%s'\n", ref1, ref2);
                return 0;
            }
            // QoreExternalStaticMember is the public API facade for QoreVarInfo
            QoreVarInfo* vi = const_cast<QoreVarInfo*>(
                reinterpret_cast<const QoreVarInfo*>(m));
            StaticClassVarRefNode* node = new StaticClassVarRefNode(&loc_builtin, strdup(ref2),
                *qc, *vi);
            return toBitsNB(QoreValue(node));
        }

        case AOTExprKind::RUNTIME_CONST_REF: {
            if (!ref1 || !*ref1) {
                return 0;
            }
            const qore_ns_private* cns = nullptr;
            const ConstantEntry* ce = qore_root_ns_private::runtimeFindNamespaceConstant(
                *pp->RootNS, ref1, cns);
            if (!ce) {
                printd(0, "AOT v2: cannot resolve constant '%s'\n", ref1);
                return 0;
            }
            // Return the constant's value directly
            QoreValue cv = ce->getReferencedValue();
            return toBitsNB(cv);
        }

        case AOTExprKind::CALL_REF:
        case AOTExprKind::OBJ_METHOD_REF:
            // These need the full AST context for proper resolution
            printd(1, "AOT v2: expression kind %d requires source fallback\n", (int)kind);
            return 0;

        case AOTExprKind::EXPR_TREE:
            // Handled inline in buildContextFromSlotMap
            return 0;

        case AOTExprKind::HASHDECL_NEW: {
            if (!ref1 || !*ref1) {
                return 0;
            }
            // Look up hashdecl by namespace-qualified path
            const qore_ns_private* found_ns = nullptr;
            const TypedHashDecl* hd = qore_root_ns_private::runtimeFindHashDecl(
                *pp->RootNS, ref1, found_ns);
            if (!hd) {
                printd(0, "AOT v2: cannot resolve hashdecl '%s' for new hashdecl\n", ref1);
                return 0;
            }
            // Create a NewHashDeclNode with no args (args handled by native code)
            NewHashDeclNode* nhd = new NewHashDeclNode(&loc_builtin, hd, (QoreParseListNode*)nullptr, false);
            return toBitsNB(QoreValue(nhd));
        }

        case AOTExprKind::COMPLEX_HASH_NEW: {
            if (!ref1 || !*ref1) {
                return 0;
            }
            std::string type_error;
            QoreAOTTypeResolver type_resolver(pgm);
            const QoreTypeInfo* ti = type_resolver.resolve(ref1, type_error);
            if (!ti) {
                printd(0, "AOT v2: cannot resolve type '%s' for complex hash: %s\n",
                    ref1, type_error.c_str());
                return 0;
            }
            NewComplexHashNode* nch = new NewComplexHashNode(&loc_builtin, ti, nullptr);
            return toBitsNB(QoreValue(nch));
        }

        case AOTExprKind::COMPLEX_LIST_NEW: {
            if (!ref1 || !*ref1) {
                return 0;
            }
            std::string type_error;
            QoreAOTTypeResolver type_resolver(pgm);
            const QoreTypeInfo* ti = type_resolver.resolve(ref1, type_error);
            if (!ti) {
                printd(0, "AOT v2: cannot resolve type '%s' for complex list: %s\n",
                    ref1, type_error.c_str());
                return 0;
            }
            NewComplexListNode* ncl = new NewComplexListNode(&loc_builtin, ti, QoreValue());
            return toBitsNB(QoreValue(ncl));
        }

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
            // Unsupported — function needs source fallback
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
            const qore_ns_private* found_ns = nullptr;
            const TypedHashDecl* hd = qore_root_ns_private::runtimeFindHashDecl(
                *pp->RootNS, ref1, found_ns);
            if (!hd) {
                printd(0, "AOT v2: cannot resolve hashdecl '%s' for cast\n", ref1);
                return 0;
            }
            auto* node = new QoreHashDeclCastOperatorNode(&loc_builtin, hd, QoreValue(), or_nothing);
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
            const qore_ns_private* found_ns = nullptr;
            const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
                *pp->RootNS, ref1, found_ns);
            if (!qc) {
                printd(0, "AOT v2: cannot resolve class '%s' for cast\n", ref1);
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
        default:
            return 0;
    }
}

// ---- Inline IR Expression Reader ----

//! Read one expression from inline closure/handler IR binary data.
/** Used by readExprCb lambdas in buildContextFromSlotMap to deserialize expressions
    stored inline in closure and handler IR bodies. The format is produced by
    classifyAndWriteExpr(): kind(u8) + kind-specific data (stringrefs, recursive exprs).

    For NEW_OBJECT/SCOPED_NEW_OBJECT: class_path(stringref) + num_args(u8) + N×readOneExpr().

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
    if (ek == AOTExprKind::PARSE_REF) {
        skipOneExpr(rdr, p, e);  // inner lvalue expression
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
    if (ek == AOTExprKind::LIST_LITERAL) {
        uint8_t n = QoreAOTBinaryReader::readU8(p);
        for (uint8_t i = 0; i < n; ++i) {
            skipOneExpr(rdr, p, e);  // each element (recursive)
        }
        return;
    }
    if (ek == AOTExprKind::NEW_OBJECT || ek == AOTExprKind::SCOPED_NEW_OBJECT) {
        rdr.readStringRef(p);  // class path
        uint8_t na = QoreAOTBinaryReader::readU8(p);
        for (uint8_t i = 0; i < na; ++i) {
            skipOneExpr(rdr, p, e);  // each arg
        }
        return;
    }
    // Two-stringref kinds
    if (ek == AOTExprKind::SELF_METHOD_CALL || ek == AOTExprKind::STATIC_METHOD_CALL
            || ek == AOTExprKind::STATIC_VARREF || ek == AOTExprKind::CONST_ENUM) {
        rdr.readStringRef(p);
        rdr.readStringRef(p);
        return;
    }
    // One-stringref kinds
    if (ek == AOTExprKind::FUNC_CALL || ek == AOTExprKind::RUNTIME_CONST_REF
            || ek == AOTExprKind::CONST_NUMBER || ek == AOTExprKind::CONST_BINARY
            || ek == AOTExprKind::CONST_STRING || ek == AOTExprKind::SELF_VARREF
            || ek == AOTExprKind::LOCAL_VARREF || ek == AOTExprKind::GLOBAL_VARREF
            || ek == AOTExprKind::HASHDECL_NEW || ek == AOTExprKind::COMPLEX_HASH_NEW
            || ek == AOTExprKind::COMPLEX_LIST_NEW) {
        rdr.readStringRef(p);
        return;
    }
    // Cast kinds: stringref + u8
    if (ek == AOTExprKind::CAST_HASHDECL || ek == AOTExprKind::CAST_COMPLEX_HASH
            || ek == AOTExprKind::CAST_COMPLEX_LIST || ek == AOTExprKind::CAST_CLASS
            || ek == AOTExprKind::CAST_ENUM) {
        rdr.readStringRef(p);
        QoreAOTBinaryReader::readU8(p);
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
    // CONST_NOTHING, GENERIC_EVAL or unknown: no bytes to skip
}

// Forward declaration for use by expression registry handlers (Phase 3.2)
QoreValue readOneExpr(
        const QoreAOTBinaryReader& rdr, const uint8_t*& p, const uint8_t* e,
        std::string& err, QoreProgram* pgm,
        LocalVar** locals, int num_locals,
        Var** globals, int num_globals) {
    uint8_t ekind = QoreAOTBinaryReader::readU8(p);
    AOTExprKind ek = static_cast<AOTExprKind>(ekind);

    // HASH_LITERAL: num_pairs(u8) + [key_str(stringref) + value(readOneExpr)] * N
    // HASH_DEREF: left(base) + right(key) — reconstructs QoreHashObjectDereferenceOperatorNode
    if (ek == AOTExprKind::HASH_DEREF) {
        std::string left_err;
        QoreValue left = readOneExpr(rdr, p, e, left_err, pgm, locals, num_locals, globals, num_globals);
        if (!left_err.empty()) {
            err = left_err;
            return QoreValue();
        }
        std::string right_err;
        QoreValue right = readOneExpr(rdr, p, e, right_err, pgm, locals, num_locals, globals, num_globals);
        if (!right_err.empty()) {
            err = right_err;
            left.discard(nullptr);
            return QoreValue();
        }
        return QoreValue(new QoreHashObjectDereferenceOperatorNode(&loc_builtin, left, right));
    }

    // PARSE_REF: \var lvalue reference — contains one inner lvalue expression
    if (ek == AOTExprKind::PARSE_REF) {
        std::string inner_err;
        QoreValue inner = readOneExpr(rdr, p, e, inner_err, pgm, locals, num_locals, globals, num_globals);
        if (!inner_err.empty()) {
            err = inner_err;
            return QoreValue();
        }
        return QoreValue(new ParseReferenceNode(&loc_builtin, inner));
    }

    if (ek == AOTExprKind::HASH_LITERAL) {
        uint8_t num_pairs = QoreAOTBinaryReader::readU8(p);
        QoreParseHashNode* phn = new QoreParseHashNode(&loc_builtin);
        for (uint8_t j = 0; j < num_pairs; ++j) {
            const char* key_str = rdr.readStringRef(p);
            std::string val_err;
            QoreValue val = readOneExpr(rdr, p, e, val_err, pgm,
                locals, num_locals, globals, num_globals);
            if (!val_err.empty()) {
                err = val_err;
                val.discard(nullptr);
                phn->deref(nullptr);
                return QoreValue();
            }
            phn->add(new QoreStringNode(key_str ? key_str : ""), val, &loc_builtin);
        }
        return QoreValue(phn);
    }

    // LIST_LITERAL: count(u8) + [value(readOneExpr)] * N
    if (ek == AOTExprKind::LIST_LITERAL) {
        uint8_t count = QoreAOTBinaryReader::readU8(p);
        QoreParseListNode* pln = new QoreParseListNode(&loc_builtin);
        for (uint8_t j = 0; j < count; ++j) {
            std::string val_err;
            QoreValue val = readOneExpr(rdr, p, e, val_err, pgm,
                locals, num_locals, globals, num_globals);
            if (!val_err.empty()) {
                err = val_err;
                val.discard(nullptr);
                pln->deref();
                return QoreValue();
            }
            pln->add(val, &loc_builtin);
        }
        return QoreValue(pln);
    }

    // NEW_OBJECT / SCOPED_NEW_OBJECT: class_path + num_args + N×readOneExpr()
    if (ek == AOTExprKind::NEW_OBJECT || ek == AOTExprKind::SCOPED_NEW_OBJECT) {
        const char* class_path = rdr.readStringRef(p);
        uint8_t num_args = QoreAOTBinaryReader::readU8(p);
        QoreListNode* args_list = nullptr;
        if (num_args > 0) {
            // Create with needs_eval=true so evalList() evaluates VarRefNode args at call time.
            // new QoreListNode(autoTypeInfo) defaults to value=true (no evaluation), but args
            // may contain VarRefNode or other AST nodes that require runtime evaluation.
            args_list = qore_list_private::newList(true);
            for (uint8_t j = 0; j < num_args; ++j) {
                std::string arg_err;
                QoreValue arg = readOneExpr(rdr, p, e, arg_err, pgm,
                    locals, num_locals, globals, num_globals);
                if (!arg_err.empty()) {
                    err = arg_err;
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
        qore_program_private* pp = qore_program_private::get(*pgm);
        const qore_ns_private* found_ns = nullptr;
        const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
            *pp->RootNS, class_path, found_ns);
        if (!qc) {
            if (args_list) {
                args_list->deref(nullptr);
            }
            return QoreValue();
        }
        NewObjectCallNode* nocn = new NewObjectCallNode(qc, args_list);
        return QoreValue(nocn);
    }

    const char* r1 = nullptr;
    const char* r2 = nullptr;
    switch (ek) {
        case AOTExprKind::FUNC_CALL:
        case AOTExprKind::RUNTIME_CONST_REF:
        case AOTExprKind::CONST_NUMBER:
        case AOTExprKind::CONST_BINARY:
        case AOTExprKind::CONST_STRING:
        case AOTExprKind::SELF_VARREF:
        case AOTExprKind::LOCAL_VARREF:
        case AOTExprKind::GLOBAL_VARREF:
        case AOTExprKind::HASHDECL_NEW:
        case AOTExprKind::COMPLEX_HASH_NEW:
        case AOTExprKind::COMPLEX_LIST_NEW:
            r1 = rdr.readStringRef(p);
            break;
        case AOTExprKind::CONST_INT: {
            return QoreValue(QoreAOTBinaryReader::readI64(p));
        }
        case AOTExprKind::CONST_FLOAT: {
            return QoreValue(QoreAOTBinaryReader::readF64(p));
        }
        case AOTExprKind::CONST_BOOL: {
            return QoreValue((bool)QoreAOTBinaryReader::readU8(p));
        }
        case AOTExprKind::CONST_NOTHING:
            return QoreValue();
        case AOTExprKind::SELF_METHOD_CALL:
        case AOTExprKind::STATIC_METHOD_CALL:
        case AOTExprKind::STATIC_VARREF:
        case AOTExprKind::CONST_ENUM:
            r1 = rdr.readStringRef(p);
            r2 = rdr.readStringRef(p);
            break;
        case AOTExprKind::CAST_HASHDECL:
        case AOTExprKind::CAST_COMPLEX_HASH:
        case AOTExprKind::CAST_COMPLEX_LIST:
        case AOTExprKind::CAST_CLASS:
        case AOTExprKind::CAST_ENUM: {
            // Cast operators: ref1 = type/hashdecl/class path, u8 = or_nothing
            r1 = rdr.readStringRef(p);
            uint8_t or_nothing = QoreAOTBinaryReader::readU8(p);
            uint64_t bits = resolveCastExprSlot(ek, r1, or_nothing != 0, pgm);
            if (bits) {
                QoreValue v;
                memcpy(&v, &bits, sizeof(v));
                return v;
            }
            return QoreValue();
        }
        default:
            // GENERIC_EVAL or unknown — no additional bytes to read
            return QoreValue();
    }

    // Handle LOCAL_VARREF using the passed-in locals array
    if (ek == AOTExprKind::LOCAL_VARREF && r1) {
        int local_slot = std::atoi(r1);
        if (local_slot >= 0 && local_slot < num_locals && locals && locals[local_slot]) {
            LocalVar* lv = locals[local_slot];
            // NOTE: always use false for in_closure — VT_LOCAL type calls ref.id->eval() which
            // internally checks closure_use and uses the correct lookup (local stack vs closure
            // stack). VT_CLOSURE uses thread_get_runtime_closure_var() (pointer-based runtime
            // closure env lookup) which returns null outside a closure execution context.
            VarRefNode* vrn = new VarRefNode(&loc_builtin,
                strdup(lv->getName()),
                lv, false);
            return QoreValue(vrn);
        }
        return QoreValue();
    }

    // Handle GLOBAL_VARREF using the passed-in globals array
    if (ek == AOTExprKind::GLOBAL_VARREF && r1) {
        int global_slot = std::atoi(r1);
        if (global_slot >= 0 && global_slot < num_globals && globals && globals[global_slot]) {
            Var* gvar = globals[global_slot];
            GlobalVarRefNode* vrn = new GlobalVarRefNode(&loc_builtin, strdup(gvar->getName()), gvar);
            return QoreValue(vrn);
        }
        return QoreValue();
    }

    uint64_t bits = resolveExprSlot(ek, r1, r2, pgm);
    if (bits) {
        QoreValue v;
        memcpy(&v, &bits, sizeof(v));
        return v;
    }
    return QoreValue();
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
        qore_program_private* pp = qore_program_private::get(*pgm);
        const qore_ns_private* found_ns = nullptr;
        return qore_root_ns_private::runtimeFindClass(*pp->RootNS, name.c_str(), found_ns);
    }

    //! Deserialize a single QoreValue from the blob
    /** Returns QoreValue(). On error (corrupted data), returns nothing.
    */
    QoreValue deserializeValue() {
        if (ptr >= end) {
            return QoreValue();
        }

        AOTExprNodeKind kind = static_cast<AOTExprNodeKind>(readU8());

        switch (kind) {
            // ---- Leaf constants ----

            case AOTExprNodeKind::EN_NOTHING: {
                readU16(); // num_children (0)
                return QoreValue();
            }

            case AOTExprNodeKind::EN_NULL: {
                readU16();
                return QoreValue(null());
            }

            case AOTExprNodeKind::EN_INT: {
                int64_t v = readI64();
                readU16();
                return QoreValue(v);
            }

            case AOTExprNodeKind::EN_FLOAT: {
                double v = readF64();
                readU16();
                return QoreValue(v);
            }

            case AOTExprNodeKind::EN_BOOL: {
                uint8_t v = readU8();
                readU16();
                return QoreValue((bool)v);
            }

            case AOTExprNodeKind::EN_STRING: {
                std::string s = readStr();
                readU16();
                return QoreValue(new QoreStringNode(s));
            }

            case AOTExprNodeKind::EN_NUMBER: {
                std::string s = readStr();
                readU16();
                return QoreValue(new QoreNumberNode(s.c_str()));
            }

            case AOTExprNodeKind::EN_BINARY: {
                uint32_t len = readU32();
                SimpleRefHolder<BinaryNode> bin(new BinaryNode);
                if (len > 0 && ptr + len <= end) {
                    bin->append(ptr, len);
                    ptr += len;
                }
                readU16();
                return QoreValue(bin.release());
            }

            case AOTExprNodeKind::EN_ENUM: {
                std::string enum_path = readStr();
                std::string member_name = readStr();
                readU16(); // num_children (0)
                const QoreNamespace* pns = nullptr;
                const QoreEnumDecl* ed = pgm->findEnum(enum_path.c_str(), pns);
                if (!ed) {
                    printd(0, "AOT expr tree: cannot resolve enum '%s'\n", enum_path.c_str());
                    return QoreValue();
                }
                const QoreEnumMember* member = ed->findMember(member_name.c_str());
                if (!member) {
                    printd(0, "AOT expr tree: cannot find enum member '%s::%s'\n",
                        enum_path.c_str(), member_name.c_str());
                    return QoreValue();
                }
                return QoreValue::makeEnum(member);
            }

            case AOTExprNodeKind::EN_DATE: {
                uint8_t is_relative = readU8();
                if (is_relative) {
                    int32_t year = readI32();
                    int32_t month = readI32();
                    int32_t day = readI32();
                    int32_t hour = readI32();
                    int32_t minute = readI32();
                    int32_t second = readI32();
                    int32_t us = readI32();
                    readU16();
                    return QoreValue(DateTimeNode::makeRelative(
                        year, month, day, hour, minute, second, us));
                } else {
                    int64_t epoch = readI64();
                    int32_t us = readI32();
                    std::string zname = readStr();
                    readU16();
                    ExceptionSink tz_xsink;
                    const AbstractQoreZoneInfo* zone = QTZM.findLoadRegion(
                        zname.c_str(), &tz_xsink);
                    return QoreValue(DateTimeNode::makeAbsolute(
                        zone, epoch, us));
                }
            }

            // ---- Variable references ----

            case AOTExprNodeKind::EN_LOCAL_VAR: {
                uint16_t slot = readU16();
                readU16(); // num_children
                if (slot < ctx->num_locals && ctx->locals[slot]) {
                    LocalVar* lv = ctx->locals[slot];
                    // NOTE: always use false for in_closure — VT_LOCAL type calls
                    // ref.id->eval() which internally checks closure_use and uses
                    // the correct lookup (local stack vs closure stack).
                    // VT_CLOSURE uses thread_get_runtime_closure_var() (pointer-based
                    // runtime closure env lookup) which doesn't work for non-closure
                    // variables that just happen to have closure_use=true.
                    return QoreValue(new VarRefNode(&loc_builtin, strdup(lv->getName()), lv, false));
                }
                printd(0, "AOT EXPR_TREE: invalid local slot %d\n", slot);
                return fail();
            }

            case AOTExprNodeKind::EN_GLOBAL_VAR: {
                std::string name = readStr();
                readU16();
                if (!name.empty()) {
                    qore_program_private* pp = qore_program_private::get(*pgm);
                    // Use runtimeFindGlobalVar which searches the varmap index across
                    // all namespaces (needed for Qore::ENV, Qore::ARGV, etc.)
                    const qore_ns_private* found_ns = nullptr;
                    Var* v = qore_root_ns_private::runtimeFindGlobalVar(
                        *pp->RootNS, name.c_str(), found_ns);
                    if (!v) {
                        // Fall back to local namespace search
                        qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);
                        v = root_ns->var_list.runtimeFindVar(name.c_str());
                    }
                    if (v) {
                        return QoreValue(new GlobalVarRefNode(&loc_builtin, strdup(name.c_str()), v));
                    }
                    printd(0, "AOT EXPR_TREE: cannot resolve global var '%s'\n", name.c_str());
                }
                return fail();
            }

            case AOTExprNodeKind::EN_SELF_REF: {
                std::string name = readStr();
                readU16();
                return QoreValue(new SelfVarrefNode(&loc_builtin, strdup(name.c_str())));
            }

            case AOTExprNodeKind::EN_STATIC_VAR: {
                std::string class_name = readStr();
                std::string var_name = readStr();
                readU16();
                const QoreClass* qc = resolveClass(class_name);
                if (qc) {
                    const QoreExternalStaticMember* m = qc->findLocalStaticMember(var_name.c_str());
                    if (m) {
                        // QoreExternalStaticMember is the public API facade for QoreVarInfo
                        // (same cast pattern used in QoreReflection.cpp)
                        QoreVarInfo* vi = const_cast<QoreVarInfo*>(
                            reinterpret_cast<const QoreVarInfo*>(m));
                        return QoreValue(new StaticClassVarRefNode(&loc_builtin, strdup(var_name.c_str()),
                            *qc, *vi));
                    }
                }
                printd(0, "AOT EXPR_TREE: cannot resolve static var %s::%s\n",
                    class_name.c_str(), var_name.c_str());
                return fail();
            }

            case AOTExprNodeKind::EN_CONST_REF: {
                std::string name = readStr();
                readU16();
                if (!name.empty()) {
                    qore_program_private* pp = qore_program_private::get(*pgm);
                    const qore_ns_private* cns = nullptr;
                    const ConstantEntry* ce = qore_root_ns_private::runtimeFindNamespaceConstant(
                        *pp->RootNS, name.c_str(), cns);
                    if (ce) {
                        // Return the constant's value directly; RuntimeConstantRefNode
                        // requires saved_val which is only set during parse-time
                        // delayed evaluation, not in AOT v2 metadata deserialization
                        return ce->getReferencedValue();
                    }
                    printd(0, "AOT EXPR_TREE: cannot resolve constant '%s'\n", name.c_str());
                }
                return fail();
            }

            // ---- Call nodes ----

            case AOTExprNodeKind::EN_FUNC_CALL: {
                std::string name = readStr();
                uint16_t num_children = readU16();
                // Deserialize args
                SimpleRefHolder<QoreParseListNode> pln;
                if (num_children > 0) {
                    pln = new QoreParseListNode(&loc_builtin);
                    for (uint16_t i = 0; i < num_children && !failed; ++i) {
                        pln->add(deserializeValue(), &loc_builtin);
                    }
                }
                if (failed) {
                    return QoreValue();
                }
                // Look up function
                qore_program_private* pp = qore_program_private::get(*pgm);
                const FunctionEntry* fe = qore_root_ns_private::runtimeFindFunctionEntry(
                    *pp->RootNS, name.c_str());
                if (!fe) {
                    printd(0, "AOT EXPR_TREE: cannot resolve function '%s'\n", name.c_str());
                    return fail();
                }
                return QoreValue(new FunctionCallNode(&loc_builtin, fe, pln.release()));
            }

            case AOTExprNodeKind::EN_SELF_CALL: {
                std::string class_name = readStr();
                std::string method_name = readStr();
                uint16_t num_children = readU16();
                // Deserialize args into a QoreListNode (not QoreParseListNode)
                // IMPORTANT: SelfFunctionCallNode stores args in parse_args when given
                // QoreParseListNode, but crlr_selfcall_copy (used by background operator)
                // only copies getArgs() = FunctionCallBase::args field. Using QoreListNode
                // ensures args are preserved for background call copying.
                QoreListNode* ql = nullptr;
                if (num_children > 0) {
                    ql = qore_list_private::newList(true);  // needs_eval = true
                    for (uint16_t i = 0; i < num_children && !failed; ++i) {
                        QoreValue v = deserializeValue();
                        if (failed) {
                            if (v.hasNode()) {
                                v.discard(nullptr);
                            }
                            break;
                        }
                        ql->push(v, nullptr);
                    }
                }
                if (failed) {
                    if (ql) {
                        ql->deref(nullptr);
                    }
                    return QoreValue();
                }
                const QoreClass* qc = resolveClass(class_name);
                if (!qc) {
                    printd(0, "AOT EXPR_TREE: cannot resolve class '%s' for self call '%s'\n",
                        class_name.c_str(), method_name.c_str());
                    if (ql) {
                        ql->deref(nullptr);
                    }
                    return fail();
                }
                // Try runtime findMethod first (works for initialized classes)
                const QoreMethod* m = qc->findMethod(method_name.c_str());
                if (!m) {
                    m = qc->findStaticMethod(method_name.c_str());
                }
                // Fall back to parse-time lookup for not-yet-initialized classes
                if (!m) {
                    qore_class_private* qcp = qore_class_private::get(
                        *const_cast<QoreClass*>(qc));
                    m = qcp->parseFindLocalMethod(method_name.c_str());
                    if (!m) {
                        m = qcp->parseFindLocalStaticMethod(method_name.c_str());
                    }
                }
                if (!m) {
                    printd(1, "AOT EXPR_TREE: cannot find method '%s::%s'\n",
                        class_name.c_str(), method_name.c_str());
                    if (ql) {
                        ql->deref(nullptr);
                    }
                    return fail();
                }
                printd(5, "AOT EXPR_TREE: resolved self call '%s::%s' args=%d -> %p\n",
                    class_name.c_str(), method_name.c_str(), (int)num_children, m);
                // Create base node (null parse args) then use copy constructor to set args field
                // This ensures SelfFunctionCallNode::args (not parse_args) is set, which is
                // what crlr_selfcall_copy uses when copying background operator expressions.
                SelfFunctionCallNode* base = new SelfFunctionCallNode(&loc_builtin,
                    strdup(method_name.c_str()), nullptr, m,
                    m->getClass(), qore_class_private::get(*m->getClass()));
                SelfFunctionCallNode* sfcn = new SelfFunctionCallNode(*base, ql);
                base->deref(nullptr);
                return QoreValue(sfcn);
            }

            case AOTExprNodeKind::EN_STATIC_CALL: {
                std::string class_name = readStr();
                std::string method_name = readStr();
                uint16_t num_children = readU16();
                SimpleRefHolder<QoreParseListNode> pln;
                if (num_children > 0) {
                    pln = new QoreParseListNode(&loc_builtin);
                    for (uint16_t i = 0; i < num_children && !failed; ++i) {
                        pln->add(deserializeValue(), &loc_builtin);
                    }
                }
                if (failed) {
                    return QoreValue();
                }
                const QoreClass* qc = resolveClass(class_name);
                if (!qc) {
                    printd(0, "AOT EXPR_TREE: cannot resolve class '%s' for static call '%s'\n",
                        class_name.c_str(), method_name.c_str());
                    return fail();
                }
                const QoreMethod* m = qc->findStaticMethod(method_name.c_str());
                // Fall back to parse-time lookup for not-yet-initialized classes
                if (!m) {
                    qore_class_private* qcp = qore_class_private::get(
                        *const_cast<QoreClass*>(qc));
                    m = qcp->parseFindLocalStaticMethod(method_name.c_str());
                }
                if (!m) {
                    printd(0, "AOT EXPR_TREE: cannot find static method '%s::%s'\n",
                        class_name.c_str(), method_name.c_str());
                    return fail();
                }
                StaticMethodCallNode* smcn = new StaticMethodCallNode(&loc_builtin, m, pln.release());
                return QoreValue(smcn);
            }

            case AOTExprNodeKind::EN_DOT_EVAL: {
                std::string method_name = readStr();
                std::string class_path = readStr();
                uint8_t is_pseudo = readU8();
                uint16_t num_children = readU16();
                // First child = target expression
                QoreValue target;
                SimpleRefHolder<QoreParseListNode> pln;
                if (num_children > 0) {
                    target = deserializeValue();
                    // Remaining children = method args
                    if (num_children > 1) {
                        pln = new QoreParseListNode(&loc_builtin);
                        for (uint16_t i = 1; i < num_children && !failed; ++i) {
                            pln->add(deserializeValue(), &loc_builtin);
                        }
                    }
                }
                if (failed) {
                    target.discard(nullptr);
                    return QoreValue();
                }
                MethodCallNode* mc = new MethodCallNode(&loc_builtin,
                    strdup(method_name.c_str()), pln.release());
                // Resolve class and method from serialized class path
                if (!class_path.empty()) {
                    const QoreClass* dot_qc = resolveClass(class_path);
                    if (dot_qc) {
                        const QoreMethod* dot_method = is_pseudo
                            ? dot_qc->findMethod(method_name.c_str())
                            : dot_qc->findMethod(method_name.c_str());
                        if (dot_method) {
                            mc->parseSetClassAndMethod(dot_qc, dot_method);
                        }
                    }
                }
                return QoreValue(new QoreDotEvalOperatorNode(&loc_builtin, target, mc));
            }

            case AOTExprNodeKind::EN_NEW: {
                std::string class_name = readStr();
                uint16_t num_children = readU16();
                ReferenceHolder<QoreListNode> args_list(nullptr, nullptr);
                if (num_children > 0) {
                    args_list = new QoreListNode(autoTypeInfo);
                    for (uint16_t i = 0; i < num_children && !failed; ++i) {
                        QoreValue v = deserializeValue();
                        args_list->push(v, nullptr);
                    }
                }
                if (failed) {
                    return QoreValue();
                }
                const QoreClass* qc = resolveClass(class_name);
                if (!qc) {
                    printd(0, "AOT EXPR_TREE: cannot resolve class '%s' for new\n",
                        class_name.c_str());
                    return fail();
                }
                {
                    const QoreMethod* cons = qc->getConstructor();
                    printd(5, "AOT EXPR_TREE EN_NEW: class='%s' id=%d nargs=%d constructor=%p\n",
                        qc->getName(), qc->getID(), (int)num_children, (void*)cons);
                    if (cons) {
                        const QoreFunction* cf = qore_method_private::get(*cons)->getFunction();
                        printd(5, "  constructor vlist=%d\n", (int)cf->numVariants());
                    }
                }
                NewObjectCallNode* nocn = new NewObjectCallNode(qc, args_list.release());
                printd(5, "  nocn variant=%p\n", (void*)nocn->getVariant());
                return QoreValue(nocn);
            }

            case AOTExprNodeKind::EN_SCOPED_NEW: {
                std::string class_name = readStr();
                uint16_t num_children = readU16();
                SimpleRefHolder<QoreParseListNode> pln;
                if (num_children > 0) {
                    pln = new QoreParseListNode(&loc_builtin);
                    for (uint16_t i = 0; i < num_children && !failed; ++i) {
                        pln->add(deserializeValue(), &loc_builtin);
                    }
                }
                if (failed) {
                    return QoreValue();
                }
                const QoreClass* qc = resolveClass(class_name);
                if (!qc) {
                    printd(0, "AOT EXPR_TREE: cannot resolve class '%s' for scoped new\n",
                        class_name.c_str());
                    return fail();
                }
                return QoreValue(new ScopedObjectCallNode(&loc_builtin, qc, pln.release()));
            }

            case AOTExprNodeKind::EN_CALLREF_CALL: {
                uint16_t num_children = readU16();
                QoreValue callref_expr;
                SimpleRefHolder<QoreParseListNode> pln;
                if (num_children > 0) {
                    callref_expr = deserializeValue();
                    if (num_children > 1) {
                        pln = new QoreParseListNode(&loc_builtin);
                        for (uint16_t i = 1; i < num_children && !failed; ++i) {
                            pln->add(deserializeValue(), &loc_builtin);
                        }
                    }
                }
                if (failed) {
                    callref_expr.discard(nullptr);
                    return QoreValue();
                }
                return QoreValue(new CallReferenceCallNode(&loc_builtin, callref_expr, pln.release()));
            }

            // ---- Access operators ----

            case AOTExprNodeKind::EN_HASH_DEREF: {
                uint16_t num_children = readU16();
                QoreValue left, right;
                if (num_children >= 1) {
                    left = deserializeValue();
                }
                if (num_children >= 2) {
                    right = deserializeValue();
                }
                return QoreValue(new QoreHashObjectDereferenceOperatorNode(&loc_builtin, left, right));
            }

            case AOTExprNodeKind::EN_SQUARE_BRKT: {
                uint16_t num_children = readU16();
                QoreValue left, right;
                if (num_children >= 1) {
                    left = deserializeValue();
                }
                if (num_children >= 2) {
                    right = deserializeValue();
                }
                return QoreValue(new QoreSquareBracketsOperatorNode(&loc_builtin, left, right));
            }

            // ---- Unary operators ----

            case AOTExprNodeKind::EN_KEYS:
            case AOTExprNodeKind::EN_ELEMENTS:
            case AOTExprNodeKind::EN_EXISTS:
            case AOTExprNodeKind::EN_DELETE:
            case AOTExprNodeKind::EN_REMOVE:
            case AOTExprNodeKind::EN_BACKGROUND:
            case AOTExprNodeKind::EN_TRIM:
            case AOTExprNodeKind::EN_CHOMP:
            case AOTExprNodeKind::EN_POP:
            case AOTExprNodeKind::EN_SHIFT:
            case AOTExprNodeKind::EN_UNARY_MINUS:
            case AOTExprNodeKind::EN_UNARY_PLUS:
            case AOTExprNodeKind::EN_LOG_NOT:
            case AOTExprNodeKind::EN_BIT_NOT: {
                uint16_t num_children = readU16();
                QoreValue operand;
                if (num_children >= 1) {
                    operand = deserializeValue();
                }
                // Skip any extra children
                for (uint16_t i = 1; i < num_children; ++i) {
                    deserializeValue().discard(nullptr);
                }
                switch (kind) {
                    case AOTExprNodeKind::EN_KEYS:
                        return QoreValue(new QoreKeysOperatorNode(&loc_builtin, operand));
                    case AOTExprNodeKind::EN_ELEMENTS:
                        return QoreValue(new QoreElementsOperatorNode(&loc_builtin, operand));
                    case AOTExprNodeKind::EN_EXISTS:
                        return QoreValue(new QoreExistsOperatorNode(&loc_builtin, operand));
                    case AOTExprNodeKind::EN_DELETE:
                        return QoreValue(new QoreDeleteOperatorNode(&loc_builtin, operand));
                    case AOTExprNodeKind::EN_REMOVE:
                        return QoreValue(new QoreRemoveOperatorNode(&loc_builtin, operand));
                    case AOTExprNodeKind::EN_BACKGROUND:
                        return QoreValue(new QoreBackgroundOperatorNode(&loc_builtin, operand));
                    case AOTExprNodeKind::EN_TRIM:
                        return QoreValue(new QoreTrimOperatorNode(&loc_builtin, operand));
                    case AOTExprNodeKind::EN_CHOMP:
                        return QoreValue(new QoreChompOperatorNode(&loc_builtin, operand));
                    case AOTExprNodeKind::EN_POP:
                        return QoreValue(new QorePopOperatorNode(&loc_builtin, operand));
                    case AOTExprNodeKind::EN_SHIFT:
                        return QoreValue(new QoreShiftOperatorNode(&loc_builtin, operand));
                    case AOTExprNodeKind::EN_UNARY_MINUS:
                        return QoreValue(new QoreUnaryMinusOperatorNode(&loc_builtin, operand));
                    case AOTExprNodeKind::EN_UNARY_PLUS:
                        return QoreValue(new QoreUnaryPlusOperatorNode(&loc_builtin, operand));
                    case AOTExprNodeKind::EN_LOG_NOT:
                        return QoreValue(new QoreLogicalNotOperatorNode(&loc_builtin, operand));
                    case AOTExprNodeKind::EN_BIT_NOT:
                        return QoreValue(new QoreBinaryNotOperatorNode(&loc_builtin, operand));
                    default:
                        break;
                }
                return QoreValue();
            }

            case AOTExprNodeKind::EN_INSTANCEOF: {
                std::string type_path = readStr();
                uint16_t num_children = readU16();
                QoreValue operand;
                if (num_children >= 1) {
                    operand = deserializeValue();
                }
                // Resolve type
                if (!type_path.empty()) {
                    std::string type_error;
                    QoreAOTTypeResolver type_resolver(pgm);
                    const QoreTypeInfo* ti = type_resolver.resolve(type_path.c_str(), type_error);
                    if (ti) {
                        return QoreValue(new QoreInstanceOfOperatorNode(&loc_builtin, operand, ti));
                    }
                    printd(0, "AOT EXPR_TREE: cannot resolve type '%s' for instanceof\n",
                        type_path.c_str());
                }
                operand.discard(nullptr);
                return fail();
            }

            case AOTExprNodeKind::EN_CAST: {
                std::string type_path = readStr();
                uint8_t or_nothing = readU8();
                uint16_t num_children = readU16();
                QoreValue operand;
                if (num_children >= 1) {
                    operand = deserializeValue();
                }
                // Resolve the target type from the type path
                std::string type_error;
                QoreAOTTypeResolver type_resolver(pgm);
                const QoreTypeInfo* ti = type_resolver.resolve(type_path.c_str(), type_error);
                if (!ti) {
                    printd(0, "AOT EXPR_TREE: cannot resolve cast type '%s': %s\n",
                        type_path.c_str(), type_error.c_str());
                    operand.discard(nullptr);
                    return fail();
                }
                // Determine which concrete cast subclass to create
                const QoreClass* qc = QoreTypeInfo::getUniqueReturnClass(ti);
                if (qc) {
                    return QoreValue(new QoreClassCastOperatorNode(&loc_builtin, qc, operand,
                        or_nothing != 0));
                }
                // Check complex hash/list types BEFORE hashdecl, since hash<HashdeclType> has both
                qore_type_t bt = QoreTypeInfo::getBaseType(ti);
                if (bt == NT_HASH) {
                    const QoreTypeInfo* ch = QoreTypeInfo::getUniqueReturnComplexHash(ti);
                    if (ch) {
                        return QoreValue(new QoreComplexHashCastOperatorNode(&loc_builtin, ti, operand,
                            or_nothing != 0));
                    }
                }
                if (bt == NT_LIST) {
                    const QoreTypeInfo* cl = QoreTypeInfo::getUniqueReturnComplexList(ti);
                    if (cl) {
                        return QoreValue(new QoreComplexListCastOperatorNode(&loc_builtin, ti, operand,
                            or_nothing != 0));
                    }
                }
                // Try hashdecl
                const TypedHashDecl* hd = QoreTypeInfo::getUniqueReturnHashDecl(ti);
                if (hd) {
                    return QoreValue(new QoreHashDeclCastOperatorNode(&loc_builtin, hd, operand,
                        or_nothing != 0));
                }
                // Fallback — try as class cast with null class (for basic types)
                return QoreValue(new QoreClassCastOperatorNode(&loc_builtin, nullptr, operand,
                    or_nothing != 0));
            }

            // Pre/post increment/decrement
            case AOTExprNodeKind::EN_PRE_INC: {
                uint16_t n = readU16();
                QoreValue op;
                if (n >= 1) {
                    op = deserializeValue();
                }
                return QoreValue(new QorePreIncrementOperatorNode(&loc_builtin, op));
            }
            case AOTExprNodeKind::EN_PRE_DEC: {
                uint16_t n = readU16();
                QoreValue op;
                if (n >= 1) {
                    op = deserializeValue();
                }
                return QoreValue(new QorePreDecrementOperatorNode(&loc_builtin, op));
            }
            case AOTExprNodeKind::EN_POST_INC: {
                uint16_t n = readU16();
                QoreValue op;
                if (n >= 1) {
                    op = deserializeValue();
                }
                return QoreValue(new QorePostIncrementOperatorNode(&loc_builtin, op));
            }
            case AOTExprNodeKind::EN_POST_DEC: {
                uint16_t n = readU16();
                QoreValue op;
                if (n >= 1) {
                    op = deserializeValue();
                }
                return QoreValue(new QorePostDecrementOperatorNode(&loc_builtin, op));
            }

            // ---- Binary operators ----

            case AOTExprNodeKind::EN_PUSH:
            case AOTExprNodeKind::EN_UNSHIFT:
            case AOTExprNodeKind::EN_LIST_ASSIGN:
            case AOTExprNodeKind::EN_PLUS:
            case AOTExprNodeKind::EN_MINUS:
            case AOTExprNodeKind::EN_MULTIPLY:
            case AOTExprNodeKind::EN_DIVIDE:
            case AOTExprNodeKind::EN_MODULO:
            case AOTExprNodeKind::EN_SHIFT_LEFT:
            case AOTExprNodeKind::EN_SHIFT_RIGHT:
            case AOTExprNodeKind::EN_BIT_AND:
            case AOTExprNodeKind::EN_BIT_OR:
            case AOTExprNodeKind::EN_BIT_XOR:
            case AOTExprNodeKind::EN_LOG_CMP:
            case AOTExprNodeKind::EN_LOG_AND:
            case AOTExprNodeKind::EN_LOG_OR:
            case AOTExprNodeKind::EN_LOG_EQ:
            case AOTExprNodeKind::EN_LOG_NE:
            case AOTExprNodeKind::EN_LOG_AEQ:
            case AOTExprNodeKind::EN_LOG_ANE:
            case AOTExprNodeKind::EN_LOG_LT:
            case AOTExprNodeKind::EN_LOG_GT:
            case AOTExprNodeKind::EN_LOG_LE:
            case AOTExprNodeKind::EN_LOG_GE:
            case AOTExprNodeKind::EN_NULL_COAL:
            case AOTExprNodeKind::EN_VAL_COAL:
            case AOTExprNodeKind::EN_ASSIGN:
            case AOTExprNodeKind::EN_PLUS_EQ:
            case AOTExprNodeKind::EN_MINUS_EQ:
            case AOTExprNodeKind::EN_MULTIPLY_EQ:
            case AOTExprNodeKind::EN_DIVIDE_EQ:
            case AOTExprNodeKind::EN_MODULO_EQ:
            case AOTExprNodeKind::EN_AND_EQ:
            case AOTExprNodeKind::EN_OR_EQ:
            case AOTExprNodeKind::EN_XOR_EQ:
            case AOTExprNodeKind::EN_SHL_EQ:
            case AOTExprNodeKind::EN_SHR_EQ: {
                uint16_t num_children = readU16();
                QoreValue left, right;
                if (num_children >= 1) {
                    left = deserializeValue();
                }
                if (num_children >= 2) {
                    right = deserializeValue();
                }
                switch (kind) {
                    case AOTExprNodeKind::EN_PUSH:
                        return QoreValue(new QorePushOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_UNSHIFT:
                        return QoreValue(new QoreUnshiftOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_LIST_ASSIGN:
                        return QoreValue(new QoreListAssignmentOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_PLUS:
                        return QoreValue(new QorePlusOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_MINUS:
                        return QoreValue(new QoreMinusOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_MULTIPLY:
                        return QoreValue(new QoreMultiplicationOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_DIVIDE:
                        return QoreValue(new QoreDivisionOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_MODULO:
                        return QoreValue(new QoreModuloOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_SHIFT_LEFT:
                        return QoreValue(new QoreShiftLeftOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_SHIFT_RIGHT:
                        return QoreValue(new QoreShiftRightOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_BIT_AND:
                        return QoreValue(new QoreBinaryAndOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_BIT_OR:
                        return QoreValue(new QoreBinaryOrOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_BIT_XOR:
                        return QoreValue(new QoreBinaryXorOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_LOG_CMP:
                        return QoreValue(new QoreLogicalComparisonOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_LOG_AND:
                        return QoreValue(new QoreLogicalAndOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_LOG_OR:
                        return QoreValue(new QoreLogicalOrOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_LOG_EQ:
                        return QoreValue(new QoreLogicalEqualsOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_LOG_NE:
                        return QoreValue(new QoreLogicalNotEqualsOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_LOG_AEQ:
                        return QoreValue(new QoreLogicalAbsoluteEqualsOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_LOG_ANE:
                        return QoreValue(new QoreLogicalAbsoluteNotEqualsOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_LOG_LT:
                        return QoreValue(new QoreLogicalLessThanOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_LOG_GT:
                        return QoreValue(new QoreLogicalGreaterThanOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_LOG_LE:
                        return QoreValue(new QoreLogicalLessThanOrEqualsOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_LOG_GE:
                        return QoreValue(new QoreLogicalGreaterThanOrEqualsOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_NULL_COAL:
                        return QoreValue(new QoreNullCoalescingOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_VAL_COAL:
                        return QoreValue(new QoreValueCoalescingOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_ASSIGN:
                        return QoreValue(new QoreAssignmentOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_PLUS_EQ:
                        return QoreValue(new QorePlusEqualsOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_MINUS_EQ:
                        return QoreValue(new QoreMinusEqualsOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_MULTIPLY_EQ:
                        return QoreValue(new QoreMultiplyEqualsOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_DIVIDE_EQ:
                        return QoreValue(new QoreDivideEqualsOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_MODULO_EQ:
                        return QoreValue(new QoreModuloEqualsOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_AND_EQ:
                        return QoreValue(new QoreAndEqualsOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_OR_EQ:
                        return QoreValue(new QoreOrEqualsOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_XOR_EQ:
                        return QoreValue(new QoreXorEqualsOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_SHL_EQ:
                        return QoreValue(new QoreShiftLeftEqualsOperatorNode(&loc_builtin, left, right));
                    case AOTExprNodeKind::EN_SHR_EQ:
                        return QoreValue(new QoreShiftRightEqualsOperatorNode(&loc_builtin, left, right));
                    default:
                        break;
                }
                left.discard(nullptr);
                right.discard(nullptr);
                return QoreValue();
            }

            // Ternary: question mark (? :)
            case AOTExprNodeKind::EN_QUESTION: {
                uint16_t num_children = readU16();
                QoreValue cond, true_expr, false_expr;
                if (num_children >= 1) {
                    cond = deserializeValue();
                }
                if (num_children >= 2) {
                    true_expr = deserializeValue();
                }
                if (num_children >= 3) {
                    false_expr = deserializeValue();
                }
                return QoreValue(new QoreQuestionMarkOperatorNode(&loc_builtin,
                    cond, true_expr, false_expr));
            }

            // Range operator (binary: start..stop)
            case AOTExprNodeKind::EN_RANGE: {
                uint16_t num_children = readU16();
                QoreValue left, right;
                if (num_children >= 1) {
                    left = deserializeValue();
                }
                if (num_children >= 2) {
                    right = deserializeValue();
                }
                return QoreValue(new QoreRangeOperatorNode(&loc_builtin, left, right));
            }

            // ---- Regex operators ----

            case AOTExprNodeKind::EN_REGEX_MATCH:
            case AOTExprNodeKind::EN_REGEX_EXTRACT: {
                std::string pattern = readStr();
                int64_t options = readI64();
                uint16_t num_children = readU16();
                QoreValue operand;
                if (num_children >= 1) {
                    operand = deserializeValue();
                }
                if (failed) {
                    operand.discard(nullptr);
                    return QoreValue();
                }
                ExceptionSink xsink;
                QoreRegex* re = new QoreRegex(pattern.c_str(), options, &xsink);
                if (xsink) {
                    printd(0, "AOT EXPR_TREE: regex compile error for pattern '%s'\n",
                        pattern.c_str());
                    operand.discard(nullptr);
                    return fail();
                }
                if (kind == AOTExprNodeKind::EN_REGEX_EXTRACT) {
                    return QoreValue(new QoreRegexExtractOperatorNode(&loc_builtin, operand, re));
                }
                return QoreValue(new QoreRegexMatchOperatorNode(&loc_builtin, operand, re));
            }

            case AOTExprNodeKind::EN_REGEX_NMATCH: {
                std::string pattern = readStr();
                int64_t options = readI64();
                uint16_t num_children = readU16();
                QoreValue operand;
                if (num_children >= 1) {
                    operand = deserializeValue();
                }
                if (failed) {
                    operand.discard(nullptr);
                    return QoreValue();
                }
                ExceptionSink xsink;
                QoreRegex* re = new QoreRegex(pattern.c_str(), options, &xsink);
                if (xsink) {
                    operand.discard(nullptr);
                    return fail();
                }
                return QoreValue(new QoreRegexNMatchOperatorNode(&loc_builtin, operand, re));
            }

            case AOTExprNodeKind::EN_REGEX_SUBST: {
                std::string pattern = readStr();
                std::string replacement = readStr();
                int64_t options = readI64();
                uint8_t global = readU8();
                uint16_t num_children = readU16();
                QoreValue operand;
                if (num_children >= 1) {
                    operand = deserializeValue();
                }
                if (failed) {
                    operand.discard(nullptr);
                    return QoreValue();
                }
                // Use the default constructor which creates both str and newstr
                // (the runtime constructor doesn't create newstr, causing null deref
                // in concatTarget)
                QoreRegexSubst* rs = new QoreRegexSubst();
                // Set regex options from serialized data before compiling
                if (options & PCRE2_CASELESS) {
                    rs->setCaseInsensitive();
                }
                if (options & PCRE2_DOTALL) {
                    rs->setDotAll();
                }
                if (options & PCRE2_EXTENDED) {
                    rs->setExtended();
                }
                if (options & PCRE2_MULTILINE) {
                    rs->setMultiline();
                }
                // Compile pattern
                ExceptionSink xsink;
                if (rs->parseRT(pattern.c_str(), &xsink)) {
                    printd(0, "AOT EXPR_TREE: regex subst compile error for pattern '%s'\n",
                        pattern.c_str());
                    delete rs;
                    operand.discard(nullptr);
                    return fail();
                }
                if (global) {
                    rs->setGlobal();
                }
                // Set replacement string via concatTarget (newstr created by default ctor)
                for (char c : replacement) {
                    rs->concatTarget(c);
                }
                return QoreValue(new QoreRegexSubstOperatorNode(&loc_builtin, operand, rs));
            }

            case AOTExprNodeKind::EN_TRANSLIT: {
                std::string source = readStr();
                std::string target = readStr();
                uint16_t num_children = readU16();
                QoreValue operand;
                if (num_children >= 1) {
                    operand = deserializeValue();
                }
                QoreTransliteration* tr = new QoreTransliteration(&loc_builtin);
                for (char c : source) {
                    tr->concatSource(c);
                }
                tr->finishSource();
                for (char c : target) {
                    tr->concatTarget(c);
                }
                tr->finishTarget();
                return QoreValue(new QoreTransliterationOperatorNode(&loc_builtin, operand, tr));
            }

            // ---- Special nodes ----

            case AOTExprNodeKind::EN_OBJ_METH_REF: {
                std::string method_name = readStr();
                uint16_t num_children = readU16();
                QoreValue target;
                if (num_children >= 1) {
                    target = deserializeValue();
                }
                return QoreValue(new ParseObjectMethodReferenceNode(&loc_builtin,
                    target, strdup(method_name.c_str())));
            }

            case AOTExprNodeKind::EN_SELF_METH_REF: {
                std::string method_name = readStr();
                readU16(); // 0 children
                return QoreValue(new ParseSelfMethodReferenceNode(&loc_builtin,
                    strdup(method_name.c_str())));
            }

            case AOTExprNodeKind::EN_EXTRACT: {
                uint16_t num_children = readU16();
                QoreValue lvalue, offset, length, new_val;
                if (num_children >= 1) {
                    lvalue = deserializeValue();
                }
                if (num_children >= 2) {
                    offset = deserializeValue();
                }
                if (num_children >= 3) {
                    length = deserializeValue();
                }
                if (num_children >= 4) {
                    new_val = deserializeValue();
                }
                return QoreValue(new QoreExtractOperatorNode(&loc_builtin,
                    lvalue, offset, length, new_val));
            }

            case AOTExprNodeKind::EN_SPLICE: {
                uint16_t num_children = readU16();
                QoreValue lvalue, offset, length, new_val;
                if (num_children >= 1) {
                    lvalue = deserializeValue();
                }
                if (num_children >= 2) {
                    offset = deserializeValue();
                }
                if (num_children >= 3) {
                    length = deserializeValue();
                }
                if (num_children >= 4) {
                    new_val = deserializeValue();
                }
                return QoreValue(new QoreSpliceOperatorNode(&loc_builtin,
                    lvalue, offset, length, new_val));
            }

            case AOTExprNodeKind::EN_PARSE_LIST: {
                uint16_t count = readU16();
                QoreParseListNode* pln = new QoreParseListNode(&loc_builtin);
                for (uint16_t i = 0; i < count; ++i) {
                    pln->add(deserializeValue(), &loc_builtin);
                }
                return QoreValue(pln);
            }

            case AOTExprNodeKind::EN_FUNC_REF: {
                std::string name = readStr();
                readU16(); // 0 children
                // Look up function and create a reference
                qore_program_private* pp = qore_program_private::get(*pgm);
                const FunctionEntry* fe = qore_root_ns_private::runtimeFindFunctionEntry(
                    *pp->RootNS, name.c_str());
                if (fe) {
                    // FunctionCallNode without args acts as reference when resolved
                    return QoreValue(new FunctionCallNode(&loc_builtin, fe, (QoreListNode*)nullptr, pgm));
                }
                printd(0, "AOT EXPR_TREE: cannot resolve function ref '%s'\n", name.c_str());
                return fail();
            }

            case AOTExprNodeKind::EN_STATIC_METH_REF: {
                std::string class_path = readStr();
                std::string method_name = readStr();
                readU16(); // 0 children
                ExceptionSink xsink;
                const QoreClass* qc = pgm->findClass(class_path.c_str(), &xsink);
                if (xsink.isException()) {
                    xsink.clear();
                }
                if (qc) {
                    const QoreMethod* m = qc->findStaticMethod(method_name.c_str());
                    if (!m) {
                        m = qc->findMethod(method_name.c_str());
                    }
                    if (m) {
                        return QoreValue(new LocalStaticMethodCallReferenceNode(&loc_builtin, m));
                    }
                }
                printd(0, "AOT EXPR_TREE: cannot resolve static method ref '%s::%s'\n",
                    class_path.c_str(), method_name.c_str());
                return fail();
            }

            case AOTExprNodeKind::EN_BOUND_METH_REF: {
                std::string class_path = readStr();
                std::string method_name = readStr();
                readU16(); // 0 children
                ExceptionSink xsink;
                const QoreClass* qc = pgm->findClass(class_path.c_str(), &xsink);
                if (xsink.isException()) {
                    xsink.clear();
                }
                if (qc) {
                    const QoreMethod* m = qc->findMethod(method_name.c_str());
                    if (!m) {
                        m = qc->findStaticMethod(method_name.c_str());
                    }
                    if (m) {
                        return QoreValue(new LocalMethodCallReferenceNode(&loc_builtin, m));
                    }
                }
                printd(0, "AOT EXPR_TREE: cannot resolve bound method ref '%s::%s'\n",
                    class_path.c_str(), method_name.c_str());
                return fail();
            }

            case AOTExprNodeKind::EN_CLOSURE: {
                uint32_t slot = readU32();
                readU16(); // 0 children
                if (ctx && slot < static_cast<uint32_t>(ctx->num_exprs) && ctx->exprs[slot]) {
                    QoreValue v;
                    memcpy(&v, &ctx->exprs[slot], sizeof(v));
                    return v.refSelf();
                }
                printd(0, "AOT EXPR_TREE: cannot resolve closure expr slot %u\n", slot);
                return fail();
            }

            case AOTExprNodeKind::EN_LIST: {
                uint16_t count = readU16();
                ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), nullptr);
                for (uint16_t i = 0; i < count; ++i) {
                    list->push(deserializeValue(), nullptr);
                }
                return QoreValue(list.release());
            }

            case AOTExprNodeKind::EN_HASH: {
                uint16_t count = readU16();
                ReferenceHolder<QoreHashNode> hash(new QoreHashNode(autoTypeInfo), nullptr);
                for (uint16_t i = 0; i < count; ++i) {
                    std::string key = readStr();
                    QoreValue val = deserializeValue();
                    hash->setKeyValue(key.c_str(), val, nullptr);
                }
                return QoreValue(hash.release());
            }

            case AOTExprNodeKind::EN_PARSE_HASH: {
                uint16_t count = readU16();
                QoreParseHashNode* phn = new QoreParseHashNode(&loc_builtin);
                for (uint16_t i = 0; i < count; ++i) {
                    QoreValue key = deserializeValue();
                    QoreValue val = deserializeValue();
                    phn->add(key, val, &loc_builtin);
                }
                return QoreValue(phn);
            }

            case AOTExprNodeKind::EN_IMPLICIT_ARG: {
                int16_t offset;
                uint16_t raw = readU16();
                memcpy(&offset, &raw, sizeof(offset));
                readU16(); // 0 children
                // getOffset() returns internal offset (0 for $1, 1 for $2, -1 for $argv)
                // Constructor expects public offset (1 for $1, 2 for $2, -1 for $argv)
                // so add 1 to non-negative offsets
                int ctor_offset = (offset >= 0) ? (offset + 1) : offset;
                return QoreValue(new QoreImplicitArgumentNode(&loc_builtin, ctor_offset));
            }

            case AOTExprNodeKind::EN_IMPLICIT_ELEM: {
                readU16(); // 0 children
                return QoreValue(new QoreImplicitElementNode(&loc_builtin));
            }

            case AOTExprNodeKind::EN_REF_TO_LVALUE: {
                uint16_t num_children = readU16();
                QoreValue lv_exp;
                if (num_children >= 1) {
                    lv_exp = deserializeValue();
                }
                return QoreValue(new ParseReferenceNode(&loc_builtin, lv_exp));
            }

            case AOTExprNodeKind::EN_SQ_BRKT_RANGE: {
                uint16_t num_children = readU16();
                QoreValue target, start, end_val;
                if (num_children >= 1) {
                    target = deserializeValue();
                }
                if (num_children >= 2) {
                    start = deserializeValue();
                }
                if (num_children >= 3) {
                    end_val = deserializeValue();
                }
                return QoreValue(new QoreSquareBracketsRangeOperatorNode(&loc_builtin,
                    target, start, end_val));
            }

            case AOTExprNodeKind::EN_MAP: {
                uint16_t num_children = readU16();
                QoreValue map_expr, source;
                if (num_children >= 1) {
                    map_expr = deserializeValue();
                }
                if (num_children >= 2) {
                    source = deserializeValue();
                }
                return QoreValue(new QoreMapOperatorNode(&loc_builtin, map_expr, source));
            }

            case AOTExprNodeKind::EN_MAP_SELECT: {
                uint16_t num_children = readU16();
                QoreValue map_expr, source, where_expr;
                if (num_children >= 1) {
                    map_expr = deserializeValue();
                }
                if (num_children >= 2) {
                    source = deserializeValue();
                }
                if (num_children >= 3) {
                    where_expr = deserializeValue();
                }
                return QoreValue(new QoreMapSelectOperatorNode(&loc_builtin,
                    map_expr, source, where_expr));
            }

            case AOTExprNodeKind::EN_HASH_MAP: {
                uint16_t num_children = readU16();
                QoreValue key_expr, val_expr, source;
                if (num_children >= 1) {
                    key_expr = deserializeValue();
                }
                if (num_children >= 2) {
                    val_expr = deserializeValue();
                }
                if (num_children >= 3) {
                    source = deserializeValue();
                }
                return QoreValue(new QoreHashMapOperatorNode(&loc_builtin,
                    key_expr, val_expr, source));
            }

            case AOTExprNodeKind::EN_HASH_MAP_SELECT: {
                uint16_t num_children = readU16();
                QoreValue key_expr, val_expr, source, where_expr;
                if (num_children >= 1) {
                    key_expr = deserializeValue();
                }
                if (num_children >= 2) {
                    val_expr = deserializeValue();
                }
                if (num_children >= 3) {
                    source = deserializeValue();
                }
                if (num_children >= 4) {
                    where_expr = deserializeValue();
                }
                return QoreValue(new QoreHashMapSelectOperatorNode(&loc_builtin,
                    key_expr, val_expr, source, where_expr));
            }

            case AOTExprNodeKind::EN_FOLDL: {
                uint16_t num_children = readU16();
                QoreValue left, right;
                if (num_children >= 1) {
                    left = deserializeValue();
                }
                if (num_children >= 2) {
                    right = deserializeValue();
                }
                return QoreValue(new QoreFoldlOperatorNode(&loc_builtin, left, right));
            }

            case AOTExprNodeKind::EN_FOLDR: {
                uint16_t num_children = readU16();
                QoreValue left, right;
                if (num_children >= 1) {
                    left = deserializeValue();
                }
                if (num_children >= 2) {
                    right = deserializeValue();
                }
                return QoreValue(new QoreFoldrOperatorNode(&loc_builtin, left, right));
            }

            default:
                printd(0, "AOT EXPR_TREE: unknown node kind %d\n", (int)kind);
                return fail();
        }
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
};

QoreValue deserializeExprTreeFromBlob(const uint8_t* data, uint32_t size, QoreProgram* pgm,
        LocalVar** locals, int num_locals) {
    // Build a minimal QoreAOTContext for the deserializer with just local var slots.
    // We borrow the locals pointer; must null it before ctx destructor runs (which frees it).
    QoreAOTContext ctx;
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
static QoreAOTContext* buildContextFromSlotMap(
        const QoreAOTBinaryReader& reader,
        const uint8_t*& ptr, const uint8_t* end,
        UserVariantBase* uvb, QoreProgram* pgm,
        const QoreAOTFunc& aot_func, const char* name) {
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
    QoreAOTBinaryReader::readU8(ptr); // padding

    if (debug > 1 && has_unsupported) {
        printd(5, "AOT buildCtx: '%s' has_unsupported=1 FROM BINARY (pre-flagged)\n", name);
    }

    // Validate slot counts match the AOT function descriptor
    if (num_locals != aot_func.num_locals || num_globals != aot_func.num_globals
            || num_exprs != aot_func.num_exprs || num_stmts != aot_func.num_stmts
            || num_regex_cases != aot_func.num_regex_cases) {
        printd(0, "AOT v2: slot count mismatch for '%s': binary(%d,%d,%d,%d,%d) vs func(%d,%d,%d,%d,%d)\n",
            name, num_locals, num_globals, num_exprs, num_stmts, num_regex_cases,
            aot_func.num_locals, aot_func.num_globals, aot_func.num_exprs, aot_func.num_stmts, aot_func.num_regex_cases);
        return nullptr;
    }

    printd(2, "AOT v2: buildContextFromSlotMap '%s': locals=%d globals=%d exprs=%d stmts=%d regex_cases=%d "
        "body_locals=%d has_unsupported=%d uvb=%p\n", name, num_locals, num_globals, num_exprs, num_stmts,
        num_regex_cases, num_body_locals, has_unsupported, (void*)uvb);

    auto* ctx = new QoreAOTContext();
    ctx->num_locals = num_locals;
    ctx->num_globals = num_globals;
    ctx->num_exprs = num_exprs;
    ctx->num_stmts = num_stmts;
    ctx->num_regex_cases = num_regex_cases;
    ctx->allocate();

    qore_program_private* pp = qore_program_private::get(*pgm);

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

    // Read and resolve local slot identities
    for (int i = 0; i < num_locals; ++i) {
        const char* lname = reader.readStringRef(ptr);
        const char* ltype = reader.readStringRef(ptr);
        uint8_t lflags = QoreAOTBinaryReader::readU8(ptr);
        uint16_t param_idx = QoreAOTBinaryReader::readU16(ptr);

        LocalVar* lv = nullptr;
        if (lflags & 0x04) {
            // is_self
            if (sig) {
                lv = sig->selfid;
            }
        } else if (lflags & 0x08) {
            // is_argv
            if (sig) {
                lv = sig->argvid;
            }
        } else if (lflags & 0x01) {
            // is_param
            if (sig && param_idx < sig->lv.size()) {
                lv = sig->lv[param_idx];
            }
        } else {
            // Body local — try to find the actual LocalVar* from the function's AST
            // first, then fall back to creating a new one (toplevel case).
            // Use pop_front() to consume in walk order, handling duplicate names
            // from nested scopes correctly.
            if (lname && *lname && !stmt_local_deque.empty()) {
                auto it = stmt_local_deque.find(lname);
                if (it != stmt_local_deque.end() && !it->second.empty()) {
                    lv = it->second.front();
                    it->second.pop_front();
                }
            }
            if (!lv) {
                // Toplevel or not found in AST — create a new LocalVar
                std::string type_error;
                QoreAOTTypeResolver type_resolver(pgm);
                const QoreTypeInfo* ti = nullptr;
                if (ltype && *ltype) {
                    ti = type_resolver.resolve(ltype, type_error);
                    if (!type_error.empty()) {
                        type_error.clear();
                    }
                }
                lv = pp->createLocalVar(lname ? lname : "", ti);
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
            ctx->locals[i] = lv;
            printd(3, "AOT v2: '%s' local[%d] = '%s' (flags=0x%x param_idx=%d) -> %p\n",
                name, i, lname ? lname : "", lflags, param_idx, (void*)lv);
        } else {
            printd(0, "AOT v2: '%s' unresolved local slot %d ('%s' flags=0x%x param_idx=%d)\n",
                name, i, lname ? lname : "", lflags, param_idx);
        }
    }

    // Read and resolve global slot identities
    // Use qore_root_ns_private::runtimeFindGlobalVar() which searches via the varmap index
    // across all namespaces — not just the root namespace's local vmap.
    // This is needed for builtin globals like Qore::ARGV, Qore::QORE_ARGV, Qore::ENV
    // which live in the Qore sub-namespace.
    for (int i = 0; i < num_globals; ++i) {
        const char* gname = reader.readStringRef(ptr);
        const char* gtype = reader.readStringRef(ptr);
        uint8_t is_tl = QoreAOTBinaryReader::readU8(ptr);

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
    for (int i = 0; i < num_exprs; ++i) {
        const uint8_t* before_expr = ptr;  // Track ptr position for validation
        uint8_t kind_byte = QoreAOTBinaryReader::readU8(ptr);
        AOTExprKind kind = static_cast<AOTExprKind>(kind_byte);
        const char* ref1 = nullptr;
        const char* ref2 = nullptr;

        // Validate expression kind is known (Phase 1 validation)
        bool kind_is_valid = (kind_byte >= 1 && kind_byte <= 33) || kind_byte == 0xFE || kind_byte == 0xFF;
        if (!kind_is_valid) {
            printd(0, "AOT buildCtx: '%s' expr slot %d: invalid kind byte 0x%02x\n",
                name, i, (int)kind_byte);
            has_unsupported = true;
            break;
        }

        switch (kind) {
            case AOTExprKind::NEW_OBJECT:
            case AOTExprKind::SCOPED_NEW_OBJECT: {
                // ref1 = class path, followed by serialized constructor args.
                // Args are in classifyAndWriteExpr format (AOTExprKind-tagged), NOT writeValue format.
                // Use readOneExpr so VarRefNode args are reconstructed from ctx->locals[] and
                // hash literal args (QoreParseHashNode) are reconstructed with proper VarRefNode values.
                // The list must have value=false (needs_eval=true) so evalList() evaluates VarRefNodes.
                ref1 = reader.readStringRef(ptr);
                uint8_t num_args = QoreAOTBinaryReader::readU8(ptr);
                QoreListNode* constructor_args = nullptr;
                if (num_args > 0) {
                    constructor_args = qore_list_private::newList(true);
                    for (uint8_t j = 0; j < num_args; ++j) {
                        std::string arg_err;
                        QoreValue arg = readOneExpr(reader, ptr, end, arg_err, pgm,
                            ctx->locals, ctx->num_locals, ctx->globals, ctx->num_globals);
                        if (!arg_err.empty()) {
                            printd(0, "AOT v2: error reading constructor arg %d for '%s': %s\n",
                                j, ref1 ? ref1 : "", arg_err.c_str());
                            arg.discard(nullptr);
                            constructor_args->push(QoreValue(), nullptr);
                        } else {
                            constructor_args->push(arg, nullptr);
                        }
                    }
                }
                // Resolve the class and create the node with args
                if (ref1 && *ref1) {
                    const qore_ns_private* found_ns = nullptr;
                    const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
                        *pp->RootNS, ref1, found_ns);
                    if (qc) {
                        // Use NewObjectCallNode for both kinds — evalImpl is identical
                        // (both call qore_class_private::execConstructor with the same args)
                        {
                            const QoreMethod* cons = qc->getConstructor();
                            printd(5, "AOT buildCtx NEW_OBJECT: class='%s' id=%d nargs=%d constructor=%p\n",
                                qc->getName(), qc->getID(), (int)num_args, (void*)cons);
                            if (cons) {
                                const QoreFunction* cf = qore_method_private::get(*cons)->getFunction();
                                printd(5, "  constructor vlist=%d\n", (int)cf->numVariants());
                                if (cf->numVariants() > 0) {
                                    auto* sig2 = cf->first()->getSignature();
                                    printd(5, "  first variant sig='%s' np=%d minp=%d\n",
                                        sig2->getSignatureText(), sig2->numParams(), sig2->getMinParamTypes());
                                }
                            }
                        }
                        NewObjectCallNode* nocn = new NewObjectCallNode(qc, constructor_args);
                        printd(5, "  nocn->variant=%p\n", (void*)nocn->getVariant());
                        ctx->exprs[i] = toBitsNB(QoreValue(nocn));
                    } else {
                        printd(0, "AOT v2: cannot resolve class '%s' for new object\n", ref1);
                        if (constructor_args) {
                            constructor_args->deref(nullptr);
                        }
                        has_unsupported = true;
                    }
                } else {
                    if (constructor_args) {
                        constructor_args->deref(nullptr);
                    }
                    has_unsupported = true;
                }
                continue;
            }
            case AOTExprKind::FUNC_CALL:
            case AOTExprKind::RUNTIME_CONST_REF:
            case AOTExprKind::LOCAL_VARREF:
            case AOTExprKind::GLOBAL_VARREF:
            case AOTExprKind::CONST_NUMBER:
            case AOTExprKind::CONST_BINARY:
            case AOTExprKind::CONST_STRING:
            case AOTExprKind::SELF_VARREF:
            case AOTExprKind::HASHDECL_NEW:
            case AOTExprKind::COMPLEX_HASH_NEW:
            case AOTExprKind::COMPLEX_LIST_NEW:
                ref1 = reader.readStringRef(ptr);
                break;
            case AOTExprKind::SELF_METHOD_CALL:
            case AOTExprKind::STATIC_METHOD_CALL:
            case AOTExprKind::STATIC_VARREF:
            case AOTExprKind::CONST_ENUM:
                ref1 = reader.readStringRef(ptr);
                ref2 = reader.readStringRef(ptr);
                break;
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
                        ctx->locals, num_locals, ctx->globals, num_globals);
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
            case AOTExprKind::LIST_LITERAL: {
                // count(u8) + [value(readOneExpr)] * N
                uint8_t count = QoreAOTBinaryReader::readU8(ptr);
                QoreParseListNode* pln = new QoreParseListNode(&loc_builtin);
                bool list_ok = true;
                for (uint8_t j = 0; j < count; ++j) {
                    std::string val_err;
                    QoreValue val = readOneExpr(reader, ptr, end, val_err, pgm,
                        ctx->locals, num_locals, ctx->globals, num_globals);
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
                // \var lvalue reference: inner lvalue expression (AOTExprKind-encoded)
                std::string inner_err;
                QoreValue inner = readOneExpr(reader, ptr, end, inner_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals);
                if (!inner_err.empty()) {
                    printd(2, "AOT v2: PARSE_REF inner error for expr slot %d of '%s': %s\n",
                        i, name, inner_err.c_str());
                    inner.discard(nullptr);
                    has_unsupported = true;
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(new ParseReferenceNode(&loc_builtin, inner)));
                }
                continue;
            }
            case AOTExprKind::HASH_DEREF: {
                // left (base expr) + right (key expr) — uses readOneExpr to consume both sub-expressions
                std::string left_err;
                QoreValue left = readOneExpr(reader, ptr, end, left_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals);
                std::string right_err;
                QoreValue right = readOneExpr(reader, ptr, end, right_err, pgm,
                    ctx->locals, num_locals, ctx->globals, num_globals);
                if (!left_err.empty() || !right_err.empty()) {
                    left.discard(nullptr);
                    right.discard(nullptr);
                    has_unsupported = true;
                } else {
                    ctx->exprs[i] = toBitsNB(QoreValue(
                        new QoreHashObjectDereferenceOperatorNode(&loc_builtin, left, right)));
                }
                continue;
            }
            case AOTExprKind::CLOSURE_CREATE: {
                // Read flags
                const char* flags_str = reader.readStringRef(ptr);
                const char* class_type_path = reader.readStringRef(ptr);

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
                bool closure_has_varargs = QoreAOTBinaryReader::readU8(ptr) != 0;

                // Read captured variable names
                uint16_t num_captured = QoreAOTBinaryReader::readU16(ptr);
                std::vector<std::string> captured_names(num_captured);
                for (uint16_t c = 0; c < num_captured; ++c) {
                    const char* cname = reader.readStringRef(ptr);
                    captured_names[c] = cname ? cname : "";
                }

                // Read closure body IR
                uint8_t has_ir = QoreAOTBinaryReader::readU8(ptr);
                if (!has_ir) {
                    // No IR — need source fallback
                    closure_ir_missing = true;
                    continue;
                }

                uint32_t ir_size = QoreAOTBinaryReader::readU32(ptr);
                const uint8_t* ir_end_ptr = ptr + ir_size;

                // Resolve class for method context
                const QoreClass* closure_class = nullptr;
                if (class_type_path && *class_type_path) {
                    const qore_ns_private* found_ns = nullptr;
                    closure_class = qore_root_ns_private::runtimeFindClass(
                        *pp->RootNS, class_type_path, found_ns);
                }

                // Construct UserClosureFunction + UserClosureVariant FIRST
                // so signature locals are available for IR deserialization.
                // Need a parse context so UserVariantBase ctor can call
                // parse_get_parse_options() and getProgram()
                ExceptionSink closure_xsink;
                ProgramRuntimeParseContextHelper closure_pch(&closure_xsink, pgm);
                if (closure_xsink.isException()) {
                    closure_xsink.clear();
                    ptr = ir_end_ptr;
                    closure_ir_missing = true;
                    continue;
                }
                auto* ucf = new UserClosureFunction(nullptr, 0, 0, QoreValue(), nullptr);
                auto* closure_variant = static_cast<UserClosureVariant*>(
                    const_cast<AbstractQoreFunctionVariant*>(ucf->first()));
                UserSignature* closure_sig = closure_variant->getUserSignature();
                closure_sig->setupFromAOTMetadata(
                    pgm, ret_type, param_names, param_types, defaults, closure_has_varargs,
                    closure_class);

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
                if (closure_sig->selfid) {
                    enclosing_locals["self"] = closure_sig->selfid;
                }

                // Deserialize closure body IR
                std::string ir_error;
                auto readExprCb = [pgm, ctx, num_locals, num_globals]
                        (const QoreAOTBinaryReader& rdr, const uint8_t*& p,
                        const uint8_t* e, std::string& err) -> QoreValue {
                    return readOneExpr(rdr, p, e, err, pgm,
                        ctx->locals, num_locals, ctx->globals, num_globals);
                };
                auto closure_ir = deserializeIRFunction(reader, ptr, ir_end_ptr, pgm,
                    readExprCb, &enclosing_locals, ir_error);
                ptr = ir_end_ptr;  // Ensure we advance past IR data

                if (!closure_ir) {
                    printd(2, "AOT: closure IR deser failed for expr slot %d: %s\n",
                        i, ir_error.c_str());
                    delete ucf;
                    closure_ir_missing = true;
                    continue;
                }

                // Set up captured variables in LVarSet and ensure closureUse
                // is set on parent-scope LocalVars.  This is critical: setupCall()
                // uses closureUse to decide whether to instantiate a parameter on
                // the cvstack (closure variable stack) vs lvstack.  Without this,
                // thread_find_closure_var() returns null at closure creation time.
                LVarSet* closure_vlist = ucf->getVList();
                for (auto& cap_name : captured_names) {
                    auto it = enclosing_locals.find(cap_name);
                    if (it != enclosing_locals.end()) {
                        closure_vlist->add(it->second);
                        if (!it->second->closureUse()) {
                            it->second->setClosureUse();
                        }
                    }
                }

                // Populate pre_instantiated_locals so the IR interpreter knows
                // which locals belong to this closure (vs outer-scope variables).
                // Without this, ensureLocalInstantiated() skips body locals.
                // This set includes params + argvid + selfid + body locals.
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
                if (closure_sig->selfid) {
                    closure_ir->pre_instantiated_locals.insert(
                        reinterpret_cast<const void*>(closure_sig->selfid));
                }
                for (LocalVar* lv : closure_ir->all_body_locals) {
                    closure_ir->pre_instantiated_locals.insert(
                        reinterpret_cast<const void*>(lv));
                }

                // Build cached_pre_instantiated for the IR interpreter: only params
                // + argvid + selfid are pre-instantiated by CodeEvaluationHelper.
                // Body locals are NOT pre-instantiated — the IR interpreter lazily
                // instantiates them via ensureLocalInstantiated().
                auto* cached_pre_inst = new std::unordered_set<const LocalVar*>();
                for (unsigned p = 0; p < closure_sig->numParams(); ++p) {
                    if (closure_sig->lv[p]) {
                        cached_pre_inst->insert(closure_sig->lv[p]);
                    }
                }
                if (closure_sig->argvid) {
                    cached_pre_inst->insert(closure_sig->argvid);
                }
                if (closure_sig->selfid) {
                    cached_pre_inst->insert(closure_sig->selfid);
                }
                closure_ir->cached_pre_instantiated = cached_pre_inst;

                // Set cached IR on variant and promote to TIER_IR
                closure_ir->computeSlotIdsAndEmbed();
                closure_variant->setCachedIR(closure_ir.release());
                closure_variant->pgm = pgm;

                // Set class type if in a method context
                if (class_type_path && *class_type_path) {
                    ucf->setClassType(type_resolver.resolve(class_type_path, type_error));
                }

                // Create QoreClosureParseNode
                auto* closure_node = new QoreClosureParseNode(nullptr, ucf, is_lambda, is_in_method);
                ctx->exprs[i] = toBitsNB(QoreValue(closure_node));
                continue;
            }
            case AOTExprKind::CAST_HASHDECL: {
                ref1 = reader.readStringRef(ptr);
                uint8_t or_nothing = QoreAOTBinaryReader::readU8(ptr);
                if (ref1 && *ref1) {
                    const qore_ns_private* found_ns = nullptr;
                    const TypedHashDecl* hd = qore_root_ns_private::runtimeFindHashDecl(
                        *pp->RootNS, ref1, found_ns);
                    if (hd) {
                        auto* node = new QoreHashDeclCastOperatorNode(&loc_builtin, hd, QoreValue(), or_nothing != 0);
                        ctx->exprs[i] = toBitsNB(QoreValue(node));
                    } else {
                        printd(0, "AOT v2: cannot resolve hashdecl '%s' for cast\n", ref1);
                        has_unsupported = true;
                    }
                } else {
                    has_unsupported = true;
                }
                continue;
            }
            case AOTExprKind::CAST_COMPLEX_HASH: {
                ref1 = reader.readStringRef(ptr);
                uint8_t or_nothing = QoreAOTBinaryReader::readU8(ptr);
                if (ref1 && *ref1) {
                    std::string type_error;
                    QoreAOTTypeResolver type_resolver(pgm);
                    const QoreTypeInfo* ti = type_resolver.resolve(ref1, type_error);
                    if (ti) {
                        auto* node = new QoreComplexHashCastOperatorNode(&loc_builtin, ti, QoreValue(),
                            or_nothing != 0);
                        ctx->exprs[i] = toBitsNB(QoreValue(node));
                    } else {
                        printd(0, "AOT v2: cannot resolve type '%s' for complex hash cast: %s\n",
                            ref1, type_error.c_str());
                        has_unsupported = true;
                    }
                } else {
                    has_unsupported = true;
                }
                continue;
            }
            case AOTExprKind::CAST_COMPLEX_LIST: {
                ref1 = reader.readStringRef(ptr);
                uint8_t or_nothing = QoreAOTBinaryReader::readU8(ptr);
                if (ref1 && *ref1) {
                    std::string type_error;
                    QoreAOTTypeResolver type_resolver(pgm);
                    const QoreTypeInfo* ti = type_resolver.resolve(ref1, type_error);
                    if (ti) {
                        auto* node = new QoreComplexListCastOperatorNode(&loc_builtin, ti, QoreValue(),
                            or_nothing != 0);
                        ctx->exprs[i] = toBitsNB(QoreValue(node));
                    } else {
                        printd(0, "AOT v2: cannot resolve type '%s' for complex list cast: %s\n",
                            ref1, type_error.c_str());
                        has_unsupported = true;
                    }
                } else {
                    has_unsupported = true;
                }
                continue;
            }
            case AOTExprKind::CAST_CLASS: {
                ref1 = reader.readStringRef(ptr);
                uint8_t or_nothing = QoreAOTBinaryReader::readU8(ptr);
                if (ref1 && *ref1) {
                    const qore_ns_private* found_ns = nullptr;
                    const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
                        *pp->RootNS, ref1, found_ns);
                    if (qc) {
                        auto* node = new QoreClassCastOperatorNode(&loc_builtin, qc, QoreValue(),
                            or_nothing != 0);
                        ctx->exprs[i] = toBitsNB(QoreValue(node));
                    } else {
                        printd(0, "AOT v2: cannot resolve class '%s' for cast\n", ref1);
                        has_unsupported = true;
                    }
                } else {
                    has_unsupported = true;
                }
                continue;
            }
            case AOTExprKind::CAST_ENUM: {
                ref1 = reader.readStringRef(ptr);
                uint8_t or_nothing = QoreAOTBinaryReader::readU8(ptr);
                if (ref1 && *ref1) {
                    std::string type_error;
                    QoreAOTTypeResolver type_resolver(pgm);
                    const QoreTypeInfo* ti = type_resolver.resolve(ref1, type_error);
                    if (ti) {
                        const QoreEnumDecl* ed = QoreTypeInfo::getUniqueReturnEnum(ti);
                        if (ed) {
                            auto* node = new QoreEnumCastOperatorNode(&loc_builtin, ed, ti, QoreValue(),
                                or_nothing != 0);
                            ctx->exprs[i] = toBitsNB(QoreValue(node));
                        } else {
                            printd(0, "AOT v2: cannot extract enum from type '%s' for cast\n", ref1);
                            has_unsupported = true;
                        }
                    } else {
                        printd(0, "AOT v2: cannot resolve type '%s' for enum cast: %s\n",
                            ref1, type_error.c_str());
                        has_unsupported = true;
                    }
                } else {
                    has_unsupported = true;
                }
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
            case AOTExprKind::CONST_NOTHING:
                ctx->exprs[i] = toBitsNB(QoreValue());
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
                VarRefNode* vrn = new VarRefNode(&loc_builtin, strdup(lv->getName()),
                    lv, false);
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
            int global_slot = std::atoi(ref1);
            if (global_slot >= 0 && global_slot < ctx->num_globals && ctx->globals[global_slot]) {
                // Create a GlobalVarRefNode pointing to the global variable
                Var* gvar = ctx->globals[global_slot];
                GlobalVarRefNode* vrn = new GlobalVarRefNode(&loc_builtin, strdup(gvar->getName()), gvar);
                ctx->exprs[i] = toBitsNB(QoreValue(vrn));
                continue;
            } else {
                printd(0, "AOT v2: invalid global slot %d for GLOBAL_VARREF expr slot %d (num_globals=%d)\n",
                    global_slot, i, ctx->num_globals);
                has_unsupported = true;
            }
        }

        uint64_t bits = resolveExprSlot(kind, ref1, ref2, pgm);
        if (bits) {
            ctx->exprs[i] = bits;
        } else if (kind != AOTExprKind::GENERIC_EVAL) {
            printd(2, "AOT buildCtx: '%s' unresolved expr[%d] kind=%d ref1='%s' ref2='%s'\n",
                name, i, (int)kind, ref1 ? ref1 : "", ref2 ? ref2 : "");
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
    for (auto& dt : deferred_expr_trees) {
        ExprTreeDeserializer deser(dt.blob_data, dt.blob_size, pgm, ctx);
        uint64_t bits = deser.deserialize();
        if (bits) {
            ctx->exprs[dt.slot] = bits;
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
            continue;
        }
        // Dot-eval method call (obj.method())
        const auto* dot_eval = dynamic_cast<const QoreDotEvalOperatorNode*>(node);
        if (dot_eval) {
            auto* mc = dot_eval->getMethodCall();
            if (mc) {
                ctx->call_targets[i].method = mc->getMethod();
                ctx->call_targets[i].qc = mc->getClass();
                ctx->call_targets[i].variant = mc->getVariant();
                ctx->call_targets[i].is_pseudo = mc->isPseudo();
                ctx->call_targets[i].method_name = mc->getName();
                // Resolve variant from method if not set (needed for fast dispatch)
                if (ctx->call_targets[i].method && !ctx->call_targets[i].variant) {
                    MethodFunctionBase* mfb = qore_method_private::get(
                        *ctx->call_targets[i].method)->getFunction();
                    if (mfb && mfb->numVariants() > 0) {
                        ctx->call_targets[i].variant = mfb->first();
                    }
                }
            }
            continue;
        }
    }

    // For user function variants, use the statement locals we already collected from the AST.
    // These are the same LocalVar* that the compiled code expects, preserving pointer identity.
    // For toplevel functions (no uvb), create new LocalVars and build a name map for reuse.
    if (uvb && !stmt_locals.empty()) {
        // Use the pre-collected statement locals directly as body locals
        // Skip the serialized body local entries (just advance the pointer)
        for (int i = 0; i < num_body_locals; ++i) {
            reader.readStringRef(ptr);  // name
            reader.readStringRef(ptr);  // type
            QoreAOTBinaryReader::readU8(ptr);  // closure flag
        }
        ctx->all_body_locals = stmt_locals;
    } else {
        // Toplevel path — create new LocalVars, reuse from ctx->locals[] where possible
        // Use a deque map because nested scopes can have variables with the same name
        // (e.g., 'h' in nested foreach loops). Consuming front-to-back gives correct matches.
        std::unordered_map<std::string, std::deque<LocalVar*>> local_name_map;
        for (int i = 0; i < num_locals; ++i) {
            if (ctx->locals[i]) {
                local_name_map[ctx->locals[i]->getName()].push_back(ctx->locals[i]);
            }
        }

        // Read body locals
        for (int i = 0; i < num_body_locals; ++i) {
            const char* blname = reader.readStringRef(ptr);
            const char* bltype = reader.readStringRef(ptr);
            uint8_t bl_closure = QoreAOTBinaryReader::readU8(ptr);

            // Reuse LocalVar* from ctx->locals[] if same name exists
            if (blname) {
                auto it = local_name_map.find(blname);
                if (it != local_name_map.end() && !it->second.empty()) {
                    ctx->all_body_locals.push_back(it->second.front());
                    it->second.pop_front();
                    (void)bl_closure;
                    continue;
                }
            }

            std::string type_error;
            QoreAOTTypeResolver type_resolver(pgm);
            const QoreTypeInfo* ti = nullptr;
            if (bltype && *bltype) {
                ti = type_resolver.resolve(bltype, type_error);
                if (!type_error.empty()) {
                    type_error.clear();
                }
            }

            LocalVar* lv = pp->createLocalVar(blname ? blname : "", ti);
            ctx->all_body_locals.push_back(lv);
            (void)bl_closure;
        }
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

    // Read handler IR for statement slots
    // For each stmt slot, read the handler IR flag and optionally deserialize the IR function
    bool all_stmt_slots_have_ir = true;
    if (num_stmts > 0) {
        ctx->handler_irs.resize(num_stmts);
        // Build local name map for handler IR deserialization
        std::unordered_map<std::string, LocalVar*> handler_local_map;
        for (int i = 0; i < num_locals; ++i) {
            if (ctx->locals[i] && ctx->locals[i]->getName()) {
                handler_local_map[ctx->locals[i]->getName()] = ctx->locals[i];
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
                auto readExprCb = [pgm, ctx](const QoreAOTBinaryReader& rdr, const uint8_t*& p,
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
                        ctx->locals, ctx->num_locals, ctx->globals, ctx->num_globals);
                };
                auto handler = deserializeIRFunction(reader, ptr, end, pgm, readExprCb,
                    &handler_local_map, ir_error);
                if (handler) {
                    // Compute slot IDs for the deserialized handler
                    handler->computeSlotIdsAndEmbed();
                    ctx->handler_irs[i] = std::move(handler);
                    printd(3, "AOT buildCtx: '%s' stmt[%d] handler IR deserialized OK\n", name, i);
                } else {
                    printd(2, "AOT buildCtx: '%s' stmt[%d] handler IR deser failed: %s\n",
                        name, i, ir_error.c_str());
                    all_stmt_slots_have_ir = false;
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

    printd(2, "AOT v2: built context from slot map for '%s' "
        "(locals=%d, globals=%d, exprs=%d, stmts=%d, regex_cases=%d, body_locals=%d, unsupported=%d, closure_ir_missing=%d)\n",
        name, num_locals, num_globals, num_exprs, num_stmts, num_regex_cases, num_body_locals, has_unsupported,
        closure_ir_missing);

    // If any expression slots have unsupported types, skip AOT
    // registration for this function — it will fall through to JIT at runtime
    if (has_unsupported) {
        printd(2, "AOT buildCtx: SKIP '%s' (unsupported) locals=%d globals=%d "
            "exprs=%d stmts=%d body_locals=%d\n",
            name, num_locals, num_globals, num_exprs, num_stmts, num_body_locals);
        delete ctx;
        return nullptr;
    }

    // Closure IR errors are hard failures (Phase 2: no source fallback)
    if (closure_ir_missing) {
        printd(2, "AOT buildCtx: '%s' failed to register (closure IR missing/invalid, no fallback available)\n", name);
        delete ctx;
        return nullptr;
    }

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
        const AOTExprReadFunc& readExpr,
        QoreProgram* pgm,
        std::string& error) {
    if (ptr + 4 > end) {
        error = "truncated instruction header";
        return nullptr;
    }

    // Read opcode and group tag
    uint16_t opcode_raw = QoreAOTBinaryReader::readU16(ptr);
    auto opcode = static_cast<QoreIROpcode>(opcode_raw);
    uint8_t group_byte = QoreAOTBinaryReader::readU8(ptr);
    auto group = static_cast<QoreIRInstGroup>(group_byte);

    // Read base fields: result, operands, exception_target
    uint32_t result_id = QoreAOTBinaryReader::readU32(ptr);
    uint8_t num_operands = QoreAOTBinaryReader::readU8(ptr);
    std::vector<QoreIRValue> operands;
    operands.reserve(num_operands);
    for (int j = 0; j < num_operands; ++j) {
        uint32_t op_id = QoreAOTBinaryReader::readU32(ptr);
        operands.push_back(QoreIRValue(op_id));
    }
    uint16_t exc_target_idx = QoreAOTBinaryReader::readU16(ptr);
    QoreIRBasicBlock* exc_target = (exc_target_idx != 0xFFFF && exc_target_idx < blocks.size())
        ? blocks[exc_target_idx].get() : nullptr;

    // Helper to resolve a block index
    auto resolveBlock = [&](uint16_t idx) -> QoreIRBasicBlock* {
        return (idx != 0xFFFF && idx < blocks.size()) ? blocks[idx].get() : nullptr;
    };

    // Helper to resolve a local variable by name
    auto resolveLocal = [&](const char* name) -> LocalVar* {
        if (!name || !*name) {
            return nullptr;
        }
        auto it = local_map.find(name);
        return it != local_map.end() ? it->second : nullptr;
    };

    std::unique_ptr<QoreIRInstruction> inst;

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
            LocalVar* lv = resolveLocal(lname);
            if (!lv && lname && *lname) {
                // Create a new local variable for handler/closure locals
                std::string type_error;
                QoreAOTTypeResolver type_resolver(pgm);
                const QoreTypeInfo* ti = (ltype && *ltype)
                    ? type_resolver.resolve(ltype, type_error) : nullptr;
                qore_program_private* pp = qore_program_private::get(*pgm);
                lv = pp->createLocalVar(lname, ti);
            }
            auto* li = new QoreIRLocalInstruction(opcode, lv, auto_ref);
            li->weak = weak;
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
                qore_program_private* pp = qore_program_private::get(*pgm);
                const qore_ns_private* found_ns = nullptr;
                qc = qore_root_ns_private::runtimeFindClass(*pp->RootNS, class_path, found_ns);
                if (qc && method_name && *method_name) {
                    method = qc->findMethod(method_name);
                    if (!method) {
                        method = qc->findStaticMethod(method_name);
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
                qore_program_private* pp = qore_program_private::get(*pgm);
                const qore_ns_private* found_ns = nullptr;
                qc = qore_root_ns_private::runtimeFindClass(*pp->RootNS, class_path, found_ns);
                if (qc && method_name && *method_name) {
                    method = qc->findMethod(method_name);
                    if (!method) {
                        method = qc->findStaticMethod(method_name);
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
                qore_program_private* pp = qore_program_private::get(*pgm);
                const qore_ns_private* found_ns = nullptr;
                const QoreClass* qc = qore_root_ns_private::runtimeFindClass(*pp->RootNS, class_path, found_ns);
                if (qc && method_name && *method_name) {
                    method = qc->findStaticMethod(method_name);
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
            const char* method_name = reader.readStringRef(ptr);
            bool pseudo = QoreAOTBinaryReader::readU8(ptr) != 0;
            bool has_ref_args = QoreAOTBinaryReader::readU8(ptr) != 0;

            const QoreMethod* method = nullptr;
            const QoreClass* qc = nullptr;
            if (class_path && *class_path) {
                qore_program_private* pp = qore_program_private::get(*pgm);
                const qore_ns_private* found_ns = nullptr;
                qc = qore_root_ns_private::runtimeFindClass(*pp->RootNS, class_path, found_ns);
                if (qc && method_name && *method_name) {
                    method = qc->findMethod(method_name);
                    if (!method) {
                        method = qc->findStaticMethod(method_name);
                    }
                }
            }
            auto* ci = new QoreIRDotEvalMethodDirectInstruction(method, qc, nullptr, expr, pseudo);
            ci->has_ref_args = has_ref_args;
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
            const char* method_name = reader.readStringRef(ptr);
            bool pseudo = QoreAOTBinaryReader::readU8(ptr) != 0;
            bool has_ref_args = QoreAOTBinaryReader::readU8(ptr) != 0;
            uint16_t normal_idx = QoreAOTBinaryReader::readU16(ptr);
            uint16_t exception_idx = QoreAOTBinaryReader::readU16(ptr);

            const QoreMethod* method = nullptr;
            const QoreClass* qc = nullptr;
            if (class_path && *class_path) {
                qore_program_private* pp = qore_program_private::get(*pgm);
                const qore_ns_private* found_ns = nullptr;
                qc = qore_root_ns_private::runtimeFindClass(*pp->RootNS, class_path, found_ns);
                if (qc && method_name && *method_name) {
                    method = qc->findMethod(method_name);
                    if (!method) {
                        method = qc->findStaticMethod(method_name);
                    }
                }
            }
            auto* ci = new QoreIRInvokeDotEvalMethodDirectInstruction(method, qc, nullptr, expr,
                pseudo, resolveBlock(normal_idx), resolveBlock(exception_idx));
            ci->has_ref_args = has_ref_args;
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
            QoreValue expr = readExpr(reader, ptr, end, error);
            if (!error.empty()) {
                return nullptr;
            }
            auto* ni = new QoreIRNewObjectInstruction(nullptr, nullptr, nullptr, expr);
            ni->opcode = opcode;
            expr.discard(nullptr);
            inst.reset(ni);
            break;
        }

        case QoreIRInstGroup::LoadConst: {
            QoreValue expr = readExpr(reader, ptr, end, error);
            if (!error.empty()) {
                return nullptr;
            }
            auto* lci = new QoreIRLoadConstantInstruction(nullptr, expr);
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
            inst.reset(hi);
            break;
        }

        case QoreIRInstGroup::ListIndexStore: {
            uint32_t container_slot_id = QoreAOTBinaryReader::readU32(ptr);
            auto* li = new QoreIRListIndexStoreInstruction(nullptr);
            li->opcode = opcode;
            li->container_slot_id = container_slot_id;
            inst.reset(li);
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
            inst = std::make_unique<QoreIRIteratorNextInstruction>(
                QoreIRValue(iterator_id), resolveBlock(done_idx), resolveBlock(continue_idx));
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
                    &local_map, error);
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

        // These groups hold AST pointers; encountering them is a serialization error
        case static_cast<QoreIRInstGroup>(44):  // Foreach (removed)
        case static_cast<QoreIRInstGroup>(50):  // Debug (removed)
        case static_cast<QoreIRInstGroup>(51):  // Assert (removed)
        case QoreIRInstGroup::Context:
        case QoreIRInstGroup::Summarize:
            error = "unsupported AST-delegation instruction group " + std::to_string(group_byte);
            return nullptr;

        default:
            error = "unsupported IR instruction group " + std::to_string(group_byte);
            return nullptr;
    }

    // Set base fields
    inst->result = QoreIRValue(result_id);
    inst->operands = std::move(operands);
    inst->exception_target = exc_target;

    return inst;
}

std::unique_ptr<QoreIRFunction> deserializeIRFunction(
        const QoreAOTBinaryReader& reader,
        const uint8_t*& ptr,
        const uint8_t* end,
        QoreProgram* pgm,
        const AOTExprReadFunc& readExpr,
        const std::unordered_map<std::string, LocalVar*>* enclosing_locals,
        std::string& error) {
    // 1. Function header
    const char* func_name = reader.readStringRef(ptr);
    uint32_t max_value_id = QoreAOTBinaryReader::readU32(ptr);
    uint32_t max_local_slot_id = QoreAOTBinaryReader::readU32(ptr);
    uint32_t num_guards = QoreAOTBinaryReader::readU32(ptr);
    const char* return_type_path = reader.readStringRef(ptr);
    uint16_t num_blocks = QoreAOTBinaryReader::readU16(ptr);
    uint16_t num_local_slots = QoreAOTBinaryReader::readU16(ptr);
    uint16_t num_body_locals = QoreAOTBinaryReader::readU16(ptr);

    auto func = std::make_unique<QoreIRFunction>(func_name ? func_name : "");
    func->max_value_id = max_value_id;
    func->max_local_slot_id = max_local_slot_id;
    func->num_guards = num_guards;

    // Resolve return type
    if (return_type_path && *return_type_path) {
        std::string type_error;
        QoreAOTTypeResolver type_resolver(pgm);
        func->return_type_info = type_resolver.resolve(return_type_path, type_error);
    }

    // 2. Read local variable slot table and build name→LocalVar* map
    std::unordered_map<std::string, LocalVar*> local_map;
    if (enclosing_locals) {
        local_map = *enclosing_locals;
    }
    for (int i = 0; i < num_local_slots; ++i) {
        const char* lname = reader.readStringRef(ptr);
        const char* ltype = reader.readStringRef(ptr);
        uint32_t slot_id = QoreAOTBinaryReader::readU32(ptr);

        if (!lname || !*lname) {
            continue;
        }

        // Check if local is already provided by enclosing scope
        auto it = local_map.find(lname);
        if (it != local_map.end()) {
            func->local_var_slots[it->second] = slot_id;
            continue;
        }

        // Create a new local variable (handler-specific local not in enclosing scope)
        if (pgm) {
            std::string type_error;
            QoreAOTTypeResolver type_resolver(pgm);
            const QoreTypeInfo* ti = (ltype && *ltype)
                ? type_resolver.resolve(ltype, type_error) : nullptr;
            qore_program_private* pp = qore_program_private::get(*pgm);
            LocalVar* lv = pp->createLocalVar(lname, ti);
            local_map[lname] = lv;
            func->local_var_slots[lv] = slot_id;
        } else {
            printd(5, "AOT IR deser: local '%s' not found in enclosing scope and no pgm\n", lname);
        }
    }

    // 3. Read body locals
    for (int i = 0; i < num_body_locals; ++i) {
        const char* blname = reader.readStringRef(ptr);
        const char* bltype = reader.readStringRef(ptr);

        if (blname && *blname) {
            auto it = local_map.find(blname);
            if (it != local_map.end()) {
                func->all_body_locals.push_back(it->second);
            }
        }
    }

    // 4. Pre-create all blocks (needed for forward references)
    func->blocks.reserve(num_blocks);
    for (int i = 0; i < num_blocks; ++i) {
        // Peek at block name - we'll set it below
        func->blocks.push_back(std::make_unique<QoreIRBasicBlock>(""));
    }

    // 5. Read blocks and instructions
    for (int i = 0; i < num_blocks; ++i) {
        const char* block_name = reader.readStringRef(ptr);
        bool is_loop_header = QoreAOTBinaryReader::readU8(ptr) != 0;
        uint16_t num_insts = QoreAOTBinaryReader::readU16(ptr);

        func->blocks[i]->name = block_name ? block_name : "";
        func->blocks[i]->is_loop_header = is_loop_header;

        for (int j = 0; j < num_insts; ++j) {
            auto inst = deserializeIRInstruction(reader, ptr, end, func->blocks, local_map,
                readExpr, pgm, error);
            if (!inst) {
                error = "failed to deserialize instruction " + std::to_string(j)
                    + " in block " + std::to_string(i) + ": " + error;
                return nullptr;
            }
            func->blocks[i]->instructions.push_back(std::move(inst));
        }
    }

    return func;
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

    // Build the context from the fresh IR (same walk order → same slot indices)
    QoreAOTContext* ctx = buildAOTContext(*ir_func, aot_func.num_locals, aot_func.num_globals, aot_func.num_exprs, aot_func.num_stmts, aot_func.num_regex_cases);
    delete ir_func;

    return ctx;
}

//! Skip a single slot map entry (header + all variable-length fields)
/** Used when an entry doesn't match a target function and must be skipped.
    @param reader the binary reader for string pool lookups
    @param ptr current read position (advanced past the entry)
*/
static void skipSlotMapEntry(const QoreAOTBinaryReader& reader, const uint8_t*& ptr,
        const uint8_t* end) {
    const uint8_t* entry_start = ptr;
    // Read entry size prefix to know where next entry starts
    uint32_t entry_size = QoreAOTBinaryReader::readU32(ptr);
    const uint8_t* entry_end = ptr + entry_size;

    const char* skip_name = reader.readStringRef(ptr); // name
    uint16_t nl = QoreAOTBinaryReader::readU16(ptr);
    uint16_t ng = QoreAOTBinaryReader::readU16(ptr);
    uint16_t ne = QoreAOTBinaryReader::readU16(ptr);
    uint16_t ns = QoreAOTBinaryReader::readU16(ptr); // num_stmts
    uint16_t nrc = QoreAOTBinaryReader::readU16(ptr); // num_regex_cases
    uint16_t nbl = QoreAOTBinaryReader::readU16(ptr);
    QoreAOTBinaryReader::readU8(ptr); // has_unsupported
    QoreAOTBinaryReader::readU8(ptr); // padding
    // Skip local entries
    for (int i = 0; i < nl; ++i) {
        reader.readStringRef(ptr);
        reader.readStringRef(ptr);
        QoreAOTBinaryReader::readU8(ptr);
        QoreAOTBinaryReader::readU16(ptr);
    }
    // Skip global entries
    for (int i = 0; i < ng; ++i) {
        reader.readStringRef(ptr);
        reader.readStringRef(ptr);
        QoreAOTBinaryReader::readU8(ptr);
    }
    // Skip expression entries
    for (int i = 0; i < ne; ++i) {
        uint8_t kind = QoreAOTBinaryReader::readU8(ptr);
        switch (static_cast<AOTExprKind>(kind)) {
            case AOTExprKind::NEW_OBJECT:
            case AOTExprKind::SCOPED_NEW_OBJECT: {
                // ref1 = class path + u8 num_args + N×classifyAndWriteExpr-encoded args
                reader.readStringRef(ptr);  // class path
                uint8_t num_args = QoreAOTBinaryReader::readU8(ptr);
                for (uint8_t j = 0; j < num_args; ++j) {
                    skipOneExpr(reader, ptr, end);
                }
                break;
            }
            case AOTExprKind::FUNC_CALL:
            case AOTExprKind::RUNTIME_CONST_REF:
            case AOTExprKind::LOCAL_VARREF:
            case AOTExprKind::GLOBAL_VARREF:
            case AOTExprKind::CONST_NUMBER:
            case AOTExprKind::CONST_BINARY:
            case AOTExprKind::CONST_STRING:
            case AOTExprKind::SELF_VARREF:
            case AOTExprKind::HASHDECL_NEW:
            case AOTExprKind::COMPLEX_HASH_NEW:
            case AOTExprKind::COMPLEX_LIST_NEW:
                reader.readStringRef(ptr);
                break;
            case AOTExprKind::SELF_METHOD_CALL:
            case AOTExprKind::STATIC_METHOD_CALL:
            case AOTExprKind::STATIC_VARREF:
            case AOTExprKind::CONST_ENUM:
                reader.readStringRef(ptr);
                reader.readStringRef(ptr);
                break;
            case AOTExprKind::CAST_HASHDECL:
            case AOTExprKind::CAST_COMPLEX_HASH:
            case AOTExprKind::CAST_COMPLEX_LIST:
            case AOTExprKind::CAST_CLASS:
            case AOTExprKind::CAST_ENUM:
                reader.readStringRef(ptr);  // type/class/hashdecl path
                QoreAOTBinaryReader::readU8(ptr);  // or_nothing
                break;
            case AOTExprKind::EXPR_TREE: {
                uint32_t blob_size = QoreAOTBinaryReader::readU32(ptr);
                ptr += blob_size;
                break;
            }
            case AOTExprKind::CLOSURE_CREATE: {
                // Skip flags, class_type_path
                reader.readStringRef(ptr);  // flags
                reader.readStringRef(ptr);  // class_type_path
                // Skip return type
                reader.readStringRef(ptr);
                // Skip params
                uint16_t skip_nparams = QoreAOTBinaryReader::readU16(ptr);
                for (uint16_t p = 0; p < skip_nparams; ++p) {
                    reader.readStringRef(ptr);  // param name
                    reader.readStringRef(ptr);  // param type
                    uint8_t skip_has_default = QoreAOTBinaryReader::readU8(ptr);
                    if (skip_has_default) {
                        std::string skip_error;
                        reader.readValue(ptr, end, skip_error);  // skip by reading & discarding
                    }
                }
                QoreAOTBinaryReader::readU8(ptr);  // varargs
                // Skip captured var names
                uint16_t skip_ncap = QoreAOTBinaryReader::readU16(ptr);
                for (uint16_t c = 0; c < skip_ncap; ++c) {
                    reader.readStringRef(ptr);
                }
                // Skip closure IR
                uint8_t skip_has_ir = QoreAOTBinaryReader::readU8(ptr);
                if (skip_has_ir) {
                    uint32_t skip_ir_size = QoreAOTBinaryReader::readU32(ptr);
                    ptr += skip_ir_size;
                }
                break;
            }
            case AOTExprKind::HASH_LITERAL: {
                // num_pairs(u8) + [key_str(stringref) + value(AOTExprKind)] * N
                // Use skipOneExpr to advance past each value without allocating objects
                uint8_t skip_npairs = QoreAOTBinaryReader::readU8(ptr);
                for (uint8_t sp = 0; sp < skip_npairs; ++sp) {
                    reader.readStringRef(ptr);  // key
                    skipOneExpr(reader, ptr, end ? end : ptr + 65536);  // value
                }
                break;
            }
            case AOTExprKind::LIST_LITERAL: {
                // count(u8) + [value(AOTExprKind)] * N
                uint8_t skip_count = QoreAOTBinaryReader::readU8(ptr);
                for (uint8_t sp = 0; sp < skip_count; ++sp) {
                    skipOneExpr(reader, ptr, end ? end : ptr + 65536);
                }
                break;
            }
            case AOTExprKind::CONST_INT:
            case AOTExprKind::CONST_FLOAT:
            case AOTExprKind::CONST_BOOL:
                reader.readStringRef(ptr);
                break;
            case AOTExprKind::CONST_NOTHING:
                break;
            default:
                break;
        }
    }
    // Skip body locals
    for (int i = 0; i < nbl; ++i) {
        reader.readStringRef(ptr);
        reader.readStringRef(ptr);
        QoreAOTBinaryReader::readU8(ptr);
    }
    // Skip regex cases: pattern_ref(u32) options(i64) is_negated(u8)
    for (int i = 0; i < nrc; ++i) {
        reader.readStringRef(ptr);
        QoreAOTBinaryReader::readI64(ptr);
        QoreAOTBinaryReader::readU8(ptr);
    }
    // Skip handler IR entries: u8 flag, if 1 then u32 size + data
    for (int i = 0; i < ns; ++i) {
        uint8_t has_ir = QoreAOTBinaryReader::readU8(ptr);
        if (has_ir) {
            uint32_t ir_size = QoreAOTBinaryReader::readU32(ptr);
            ptr += ir_size;
        }
    }

    // Always jump to correct entry boundary regardless of individual read success/failure
    ptr = entry_end;
}

//! Collected init function context for later execution
struct AOTInitFuncExecInfo {
    QoreAOTContext* ctx;
    AotFunctionPtr fn_ptr;
    std::string name;
};

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
        std::vector<AOTInitFuncExecInfo>* init_func_contexts = nullptr) {
    const QoreAOTSectionHeader* sec = reader.findSection(QoreAOTSectionType::SLOT_MAPS);
    if (!sec) {
        printd(0, "AOT v2: no SLOT_MAPS section found\n");
        return;
    }
    const uint8_t* ptr = reader.getSectionData(*sec);
    if (!ptr) {
        printd(0, "AOT v2: invalid SLOT_MAPS section data\n");
        return;
    }
    const uint8_t* end = ptr + sec->size;

    uint32_t num_funcs = QoreAOTBinaryReader::readU32(ptr);

    // Debug: count init functions in func_map
    if (init_func_contexts) {
        int init_count = 0;
        for (auto& kv : func_map) {
            if (kv.first.substr(0, 14) == "__const_init::" || kv.first.substr(0, 13) == "__svar_init::") {
                ++init_count;
                if (init_count <= 5) {
                    printd(5, "  init func in func_map: '%s'\n", kv.first.c_str());
                }
            }
        }
        printd(5, "AOT registerFromSlotMaps: num_funcs=%d, func_map.size=%d, init_funcs_in_map=%d\n",
            num_funcs, (int)func_map.size(), init_count);
    }

    for (uint32_t f = 0; f < num_funcs; ++f) {
        const uint8_t* entry_start = ptr;
        // Read entry size prefix to know where next entry should end
        uint32_t entry_size = QoreAOTBinaryReader::readU32(ptr);
        const uint8_t* entry_end = ptr + entry_size;

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

        // Debug: print init function slot map entries
        if (init_func_contexts && (strncmp(func_name, "__const_init::", 14) == 0
                || strncmp(func_name, "__svar_init::", 13) == 0)) {
            printd(5, "  slot map entry: '%s' (in func_map: %s)\n",
                func_name, func_map.count(func_name) ? "YES" : "NO");
        }

        // Find matching AOT function
        auto it = func_map.find(func_name);
        if (it == func_map.end()) {
            // No AOT function for this entry — skip it
            // ptr is already positioned after the size field, so reset to entry_start
            ptr = entry_start;
            skipSlotMapEntry(reader, ptr, end);
            continue;
        }

        const QoreAOTFunc* aot_func = it->second;

        // Find the UserVariantBase in the namespace tree
        // Function names can be "funcName(types)" or "ClassName::methodName(types)"
        // We need to extract just the function name for lookup
        UserVariantBase* uvb = nullptr;
        std::string fname_str(func_name);

        // Strip signature suffix if present (e.g., "add(int,int)" -> "add")
        size_t paren = fname_str.find('(');
        if (paren != std::string::npos) {
            fname_str = fname_str.substr(0, paren);
        }

        size_t sep = fname_str.rfind("::");

        if (sep != std::string::npos) {
            // Method: Namespace::ClassName::methodName — use last :: as class/method separator
            std::string class_name = fname_str.substr(0, sep);
            std::string method_name = fname_str.substr(sep + 2);

            // Destructors and copy methods need their specific variant types with
            // full AST context. Skip slot map registration for these; leave
            // uvb=nullptr so buildContextFromSlotMap runs (to advance ptr) but
            // registration is skipped.
            // Note: constructors ARE registered from slot maps — the AOT-compiled
            // constructor body includes all logic (including base constructor calls
            // if any). The BCAList is not needed at runtime.
            bool skip_special_method = (method_name == "destructor"
                || method_name == "copy");

            qore_program_private* pp = qore_program_private::get(*pgm);
            const qore_ns_private* found_ns = nullptr;
            const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
                *pp->RootNS, class_name.c_str(), found_ns);
            printd(5, "AOT slot-reg: method '%s'::'%s' class=%p\n",
                class_name.c_str(), method_name.c_str(), (void*)qc);
            if (qc && !skip_special_method) {
                // Use parseFindLocalMethod/parseFindLocalStaticMethod instead of
                // findMethod/findStaticMethod — the latter checks committedEmpty()
                // which returns true for deserialized (pending) variants
                qore_class_private* qcp = qore_class_private::get(*const_cast<QoreClass*>(qc));
                const QoreMethod* m = qcp->parseFindLocalMethod(method_name.c_str());
                if (!m) {
                    m = qcp->parseFindLocalStaticMethod(method_name.c_str());
                }
                printd(5, "AOT slot-reg: method lookup '%s' m=%p\n",
                    method_name.c_str(), (void*)m);
                if (m) {
                    // Extract signature from the full func_name to match the correct
                    // overloaded variant.  The func_name format is:
                    //   ClassName::methodName(type1,type2,...)
                    // We need the "(type1,type2,...)" part to match against each variant.
                    std::string target_sig;
                    {
                        size_t p = std::string(func_name).find('(');
                        if (p != std::string::npos) {
                            target_sig = std::string(func_name).substr(p);
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
                                var_sig.append(QoreTypeInfo::getPath(types[ti]));
                            }
                        }
                        var_sig.append(")");

                        if (var_sig == target_sig) {
                            uvb = candidate;
                            break;
                        }
                    }
                    if (!uvb) {
                        printd(2, "AOT slot-reg: no matching variant for '%s' "
                            "sig='%s' in %d variants\n",
                            func_name, target_sig.c_str(), var_count);
                    }
                }
            }
        } else if (fname_str != "_toplevel") {
            // Regular function — search the module's own namespace tree (not system
            // builtins) to find the correct UserFunctionVariant.  We must avoid
            // runtimeFindFunction() because it checks Qore:: namespace first and may
            // return a builtin function that shadows the module's user function.
            std::function<UserVariantBase*(qore_ns_private*)> findFuncInTree =
                [&](qore_ns_private* ns) -> UserVariantBase* {
                // Search in this namespace's func_list
                FunctionEntry* fe = ns->func_list.findNode(fname_str.c_str());
                if (fe) {
                    QoreFunction* f = fe->getFunction();
                    if (f) {
                        // Build the target signature suffix for matching
                        std::string target_sig;
                        {
                            size_t p = std::string(func_name).find('(');
                            if (p != std::string::npos) {
                                target_sig = std::string(func_name).substr(p);
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
                                    var_sig.append(QoreTypeInfo::getPath(types[ti]));
                                }
                            }
                            var_sig.append(")");
                            if (var_sig == target_sig) {
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

            uvb = findFuncInTree(root_ns);
            printd(5, "AOT slot-reg: function '%s' uvb=%p\n", fname_str.c_str(), (void*)uvb);
        }

        // Build context from slot map
        QoreAOTContext* ctx = buildContextFromSlotMap(reader, ptr, end, uvb, pgm, *aot_func, func_name);
        // Debug: trace init function context building
        if (init_func_contexts && (strncmp(func_name, "__const_init::", 14) == 0
                || strncmp(func_name, "__svar_init::", 13) == 0)) {
            printd(5, "  buildContextFromSlotMap('%s'): ctx=%p uvb=%p\n",
                func_name, (void*)ctx, (void*)uvb);
        }
        if (ctx && uvb) {
            uvb->registerPrecompiledAOTFunction(aot_func->fn_ptr, ctx);
            ++registered;
            // Remove from func_map so fallback path won't re-register with wrong LocalVar*
            func_map.erase(func_name);
            printd(2, "AOT slot-reg: registered '%s' from slot map\n", func_name);
        } else if (ctx) {
            // Check if this is an init function (for constants/static vars)
            bool is_init_func = (strncmp(func_name, "__const_init::", 14) == 0
                || strncmp(func_name, "__svar_init::", 13) == 0);
            if (is_init_func && init_func_contexts) {
                AOTInitFuncExecInfo info;
                info.ctx = ctx;
                info.fn_ptr = aot_func->fn_ptr;
                info.name = func_name;
                init_func_contexts->push_back(std::move(info));
                func_map.erase(func_name);
                printd(2, "AOT slot-reg: collected init function '%s' for execution\n", func_name);
            } else {
                // Toplevel or unresolved — handled separately
                delete ctx;
                printd(2, "AOT slot-reg: context built for '%s' but no variant (uvb=%p)\n",
                    func_name, (void*)uvb);
            }
        } else {
            // buildContextFromSlotMap failed or returned null — ensure ptr is at entry boundary
            printd(2, "AOT slot-reg: SKIP '%s' (context build failed) uvb=%p\n",
                func_name, (void*)uvb);
        }

        // Always advance ptr to end of entry (self-healing with entry-size prefix)
        ptr = entry_end;
    }
}

//! Walk a namespace tree and register pre-compiled AOT function pointers with context
/** Matches function names from the AOT function table against user function variants
    in the program's namespace tree. For each match, re-lowers to IR to build a
    QoreAOTContext, then registers via registerPrecompiledAOTFunction().
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
            // Generate unique key including parameter types to match compiled variant
            std::string variant_key = getVariantKey(fname, variant);
            auto it = func_map.find(variant_key);
            if (it != func_map.end()) {
                const QoreAOTFunc* aot_func = it->second;
                QoreAOTContext* ctx = buildContextForVariant(uvb, fname, pgm, *aot_func);
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
        const char* class_name = qc->getName();

        // Skip system classes - they can't be modified and their methods are already set up
        if (qcp->sys) {
            printd(2, "AOT: skipping system class '%s' in method registration\n", class_name);
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

                std::string method_name = std::string(class_name) + "::" + meth->getName();
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
        QoreProgram* main_pgm) {
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
            ConstantEntry* writable_ce = const_cast<ConstantEntry*>(main_ce);
            writable_ce->val = fb_ce->getReferencedValue();
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
            discard(vi.second->assignInit(fb_val.refSelf()), nullptr);
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

        // Transplant: set the main constant's value from the fallback constant.
        ConstantEntry* writable_ce = const_cast<ConstantEntry*>(main_ce);
        writable_ce->val = fb_ce->getReferencedValue();
        writable_ce->init = true;
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
                qore_ns_private::get(*main_child), main_pgm);
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
            std::string variant_key = getVariantKey(fname, variant);
            auto it = func_map.find(variant_key);
            if (it == func_map.end()) {
                continue;
            }

            const QoreAOTFunc* aot_func = it->second;
            // Build context from fallback variant's AST (gives consistent LocalVar*)
            QoreAOTContext* ctx = buildContextForVariant(fb_uvb, fname, main_pgm, *aot_func);
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
        const char* class_name = qc->getName();

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

                std::string method_name = std::string(class_name) + "::" + meth->getName();
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
                    *main_pp->RootNS, class_name);
                UserVariantBase* main_uvb = nullptr;
                if (main_qc) {
                    qore_class_private* main_qcp = qore_class_private::get(
                        *const_cast<QoreClass*>(main_qc));
                    const QoreMethod* main_m = main_qcp->parseFindLocalMethod(meth->getName());
                    if (!main_m) {
                        main_m = main_qcp->parseFindLocalStaticMethod(meth->getName());
                    }
                    if (main_m) {
                        // Extract target signature from variant_key to match the
                        // correct overloaded variant in the main program
                        std::string target_sig;
                        {
                            size_t p = variant_key.find('(');
                            if (p != std::string::npos) {
                                target_sig = variant_key.substr(p);
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
                                    var_sig.append(QoreTypeInfo::getPath(types[ti]));
                                }
                            }
                            var_sig.append(")");
                            if (var_sig == target_sig) {
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

                    // Collect ALL body locals (top-level + nested blocks) as pre-instantiated
                    collectAllStatementLocals(&sb, ir_func->all_body_locals);
                    for (LocalVar* lv : ir_func->all_body_locals) {
                        ir_func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(lv));
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
                                toplevel_func->num_exprs, toplevel_func->num_stmts, toplevel_func->num_regex_cases);
                            if (ctx) {
                                sb.registerPrecompiledAOTTopLevel(toplevel_func->fn_ptr, ctx);
                                ++registered;
                                ctx_ok = true;
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

        printd(2, "AOT v2: parse_options=0x%llx, PO_MODERN=0x%llx, has_modern=%d\n",
            (long long)parse_options, (long long)PO_MODERN,
            (int)((parse_options & PO_MODERN) == PO_MODERN));

        // Load module dependencies before deserialization so that module classes,
        // functions, etc. are available when resolving base classes and types
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
                    }
                }
            }
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

            // Use slot maps from the binary metadata to build contexts
            printd(2, "AOT v2: calling registerAOTFunctionsFromSlotMaps with %d func_map entries, "
                "toplevel=%p\n", (int)func_map.size(), (void*)toplevel_func);
            registerAOTFunctionsFromSlotMaps(
                deserializer.getReader(), root_ns, *qpgm, func_map, registered);
            printd(2, "AOT v2: after slot map registration: %d registered, %d remaining\n",
                registered, (int)func_map.size());

            // Parse fallback source if available.
            // Needed for: (1) functions with stmt_slots that couldn't resolve from slot map,
            // (2) _toplevel fallback if slot map registration failed,
            // (3) any functions that failed slot map registration.
            if (deserializer.hasFallbackSource()) {
                ExceptionSink wsink;
                // Strip PO_NO_TOP_LEVEL_STATEMENTS from fallback parse options because the
                // full source may have top-level statements before %exec-class directive;
                // the original parser processes directives sequentially but AOT bakes all
                // parse options (including %exec-class) upfront
                fallback_pgm = new QoreProgram(parse_options & ~PO_NO_TOP_LEVEL_STATEMENTS);
                // Set script path so %requires with relative paths resolve correctly
                fallback_pgm->setScriptPath(label);
                fallback_pgm->parse(deserializer.getFallbackSource(), label, &xsink, &wsink,
                    QP_WARN_DEFAULT);
                if (wsink.isException()) {
                    wsink.handleWarnings();
                }
                if (xsink.isException()) {
                    // Fallback source parsing failed (e.g., module class conflicts).
                    // This is non-fatal: module functions are available from runtime module
                    // loading, only test-local functions that failed compilation are lost.
                    printd(2, "AOT v2: fallback source parse failed (non-fatal), "
                        "module functions available from runtime loading\n");
                    xsink.clear();
                    fallback_pgm->waitForTerminationAndDeref(nullptr);
                    fallback_pgm = nullptr;
                } else {
                    // Register remaining functions from fallback via IR re-lowering.
                    // Build contexts from fallback AST but register on main program's variants
                    // for consistent LocalVar* pointer identity with the thread-local stack.
                    if (!func_map.empty()) {
                        int fallback_registered = 0;
                        qore_ns_private* fallback_root_ns = qore_ns_private::get(
                            *qore_program_private::get(*fallback_pgm)->RootNS);
                        qore_ns_private* main_root_ns = qore_ns_private::get(*pp->RootNS);
                        registerFallbackFunctionsOnMainVariants(
                            fallback_root_ns, main_root_ns, *qpgm, func_map,
                            fallback_registered);
                        registered += fallback_registered;
                    }

                    // Transplant closure/code constants from fallback classes to main classes
                    {
                        qore_ns_private* fb_root = qore_ns_private::get(
                            *qore_program_private::get(*fallback_pgm)->RootNS);
                        qore_ns_private* main_root = qore_ns_private::get(*pp->RootNS);
                        transplantClassClosureValues(fb_root, main_root, *qpgm);
                    }
                }
            }

            // Transplant BCAList for all constructors — needed even when registered from
            // slot maps, because slot map registration only sets the native fn_ptr but
            // doesn't provide the BCAList (base class constructor arguments).
            if (fallback_pgm) {
                qore_ns_private* fb_root = qore_ns_private::get(
                    *qore_program_private::get(*fallback_pgm)->RootNS);
                qore_ns_private* main_root = qore_ns_private::get(*pp->RootNS);
                transplantConstructorBCALists(fb_root, main_root);
            }

            // Register the _toplevel function from slot maps
            if (toplevel_func) {
                // Find _toplevel in SLOT_MAPS section
                const QoreAOTSectionHeader* sm_sec = deserializer.getReader().findSection(
                    QoreAOTSectionType::SLOT_MAPS);
                bool toplevel_registered = false;

                if (sm_sec) {
                    const uint8_t* sm_ptr = deserializer.getReader().getSectionData(*sm_sec);
                    if (sm_ptr) {
                        const uint8_t* sm_end = sm_ptr + sm_sec->size;
                        uint32_t sm_count = QoreAOTBinaryReader::readU32(sm_ptr);
                        for (uint32_t fi = 0; fi < sm_count; ++fi) {
                            const uint8_t* entry_start = sm_ptr;
                            uint32_t entry_size = QoreAOTBinaryReader::readU32(sm_ptr);  // consume size prefix
                            const char* entry_name = deserializer.getReader().readStringRef(sm_ptr);  // peek at name
                            const uint8_t* entry_end = entry_start + 4 + entry_size;
                            sm_ptr = entry_start + 4;  // reset: after size prefix, before name

                            if (entry_name && strcmp(entry_name, "_toplevel") == 0) {
                                QoreAOTContext* ctx = buildContextFromSlotMap(
                                    deserializer.getReader(), sm_ptr, sm_end,
                                    nullptr, *qpgm, *toplevel_func, "_toplevel");
                                if (ctx) {
                                    pp->sb.registerPrecompiledAOTTopLevel(
                                        toplevel_func->fn_ptr, ctx);
                                    // Set LVList so doTopLevelInstantiation() can instantiate the locals
                                    pp->sb.setLVarsFromAOTContext(ctx);
                                    ++registered;
                                    toplevel_registered = true;
                                    printd(2, "AOT v2: registered _toplevel from slot map\n");
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

                if (!toplevel_registered && fallback_pgm) {
                    // Fall back to IR re-lowering path for toplevel using fallback source
                    qore_program_private* fb_pp = qore_program_private::get(*fallback_pgm);
                    TopLevelStatementBlock& fb_sb = fb_pp->sb;

                    QoreIRFunction* ir_func = new QoreIRFunction("_toplevel");

                    // Collect ALL body locals (top-level + nested blocks) as pre-instantiated
                    collectAllStatementLocals(&fb_sb, ir_func->all_body_locals);
                    for (LocalVar* lv : ir_func->all_body_locals) {
                        ir_func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(lv));
                    }

                    QoreIRBuilder builder(ir_func);
                    auto* entry = ir_func->createBlock("entry");
                    builder.setBlock(entry);

                    QoreParseContext parse_context(fallback_pgm);
                    QoreIRLowering lowering(builder, &parse_context);
                    std::string lower_error;
                    bool ctx_ok = false;
                    if (lowering.lowerStatementBlock(&fb_sb, lower_error)) {
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
                                toplevel_func->num_exprs, toplevel_func->num_stmts, toplevel_func->num_regex_cases);
                            if (ctx) {
                                pp->sb.registerPrecompiledAOTTopLevel(toplevel_func->fn_ptr, ctx);
                                // Set LVList so doTopLevelInstantiation() can instantiate the locals
                                pp->sb.setLVarsFromAOTContext(ctx);
                                ++registered;
                                ctx_ok = true;
                                printd(2, "AOT v2: registered _toplevel via fallback IR\n");
                            }
                        } else {
                            printd(0, "AOT v2: _toplevel re-verification failed: %s\n",
                                verify_error.c_str());
                        }
                    } else {
                        printd(0, "AOT v2: _toplevel re-lowering failed: %s\n", lower_error.c_str());
                    }
                    delete ir_func;

                    if (!ctx_ok) {
                        printd(0, "AOT v2: failed to build context for _toplevel\n");
                    }
                } else if (!toplevel_registered) {
                    printd(0, "AOT v2: _toplevel not registered (no slot map or fallback)\n");
                }

            }

            printd(2, "AOT v2: registered %d/%d pre-compiled functions\n", registered, num_functions);

            // Safety check: if functions weren't registered and no fallback source is available,
            // they will exist as empty shells that crash when called. Warn early.
            if (registered < num_functions && !deserializer.hasFallbackSource() && !fallback_pgm) {
                int unregistered = num_functions - registered;
                printd(0, "AOT ERROR: %d/%d functions could not be registered and no fallback "
                    "source is available.\nRecompile with --include-source or update the AOT compiler "
                    "to embed fallback sources automatically.\n", unregistered, num_functions);
            }
        }

        // NOTE: Do NOT clean up fallback_pgm yet - it may contain StatementBlock* pointers
        // that are referenced by AOT contexts for on_exit/on_success/on_error blocks.
        // We must keep it alive until after the program finishes running.

        // Run the v2 program
        QoreValue rv = qpgm->run(&xsink);
        rc = rv.getAsBigInt();
        rv.discard(&xsink);

        if (xsink.isException()) {
            rc = 3;
        }

        xsink.handleExceptions();

        // Clean up fallback program AFTER program execution - it contains StatementBlock*
        // pointers used by AOT contexts for on_exit/on_success/on_error blocks
        if (fallback_pgm) {
            fallback_pgm->waitForTerminationAndDeref(nullptr);
            fallback_pgm = nullptr;
        }
    } while (false);

    qore_cleanup();
    return rc;
}

// ---- AOT Module Runtime Functions ----

//! Per-module state for AOT-compiled modules
// Forward declaration
static void executeInitFunctions(QoreProgram* pgm,
    const std::vector<AOTInitFuncExecInfo>& exec_infos,
    const std::vector<AOTInitFuncDescriptor>& descriptors,
    const char* mod_name);

struct AotModuleState {
    QoreProgram* pgm = nullptr;
    const QoreAOTFunc* funcs = nullptr;
    int num_funcs = 0;
    //! Modules that should be reexported (from %requires(reexport) directives)
    std::vector<std::string> reexport_deps;
    //! Serialized metadata for deferred init function processing in ns_init
    std::vector<uint8_t> metadata;
    //! Init function descriptors (target type, ns path, item name) read during module_init
    std::vector<AOTInitFuncDescriptor> init_descriptors;
};

//! Map from module name to per-module state
/** Multiple AOT modules can be loaded simultaneously.  Each module's init creates a
    QoreProgram and stores it here keyed by module name.  The ns_init function uses
    get_module_context()->getName() to find the correct program for the module being
    imported.

    Thread safety: Access is serialized by QoreModuleManager's module loading mutex.
    All accesses (qore_aot_module_init, qore_aot_module_ns_init, qore_aot_module_delete)
    occur under the module manager lock, so no additional synchronization is needed.
*/
static std::unordered_map<std::string, AotModuleState> aot_module_map;

//! Current module being initialized (valid only during qore_aot_module_init / _init_v2)
static QoreProgram* aot_module_pgm = nullptr;
static std::string aot_module_name;
static const QoreAOTFunc* aot_module_funcs = nullptr;
static int aot_module_num_funcs = 0;

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

//! Strip %requires directives from source code
/** AOT modules have already resolved their dependencies at compile time, so we must
    not process %requires directives when parsing the embedded source at runtime.
    Processing %requires would cause a deadlock because module loading holds the
    module manager lock, and parsing %requires tries to acquire the same lock.

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
                QoreProgram* load_target = target_pgm ? target_pgm : aot_module_pgm;
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

        printd(2, "AOT v3: parse_options=0x%llx|0x%llx, PO_MODERN=0x%llx, has_modern=%d\n",
            (long long)parse_options_lo, (long long)parse_options_hi, (long long)PO_MODERN,
            (int)((parse_options & PO_MODERN) == PO_MODERN));

        // Load module dependencies before deserialization so that module classes,
        // functions, etc. are available when resolving base classes and types
        {
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
                    }
                }
            }
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

        // Advisory checks for source staleness and feature compatibility
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
            // Feature compatibility check
            if (aot_hdr.feature_flags != 0) {
                uint64_t unsupported = aot_hdr.feature_flags & ~QORE_AOT_SUPPORTED_FEATURES;
                if (unsupported) {
                    printd(0, "AOT WARNING: binary '%s' requires unsupported features 0x%016llx; "
                        "affected functions will fall back to JIT\n",
                        label ? label : "<unknown>",
                        (unsigned long long)unsupported);
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

            // Use slot maps from the binary metadata to build contexts
            printd(2, "AOT v3: calling registerAOTFunctionsFromSlotMaps with %d func_map entries, "
                "toplevel=%p, debug=%d\n", (int)func_map.size(), (void*)toplevel_func, debug);
            registerAOTFunctionsFromSlotMaps(
                deserializer.getReader(), root_ns, *qpgm, func_map, registered);
            printd(2, "AOT v3: after slot map registration: %d registered, %d remaining\n",
                registered, (int)func_map.size());

            // Parse fallback source if available.
            // Needed for: (1) functions with stmt_slots that couldn't resolve from slot map,
            // (2) _toplevel fallback if slot map registration failed,
            // (3) any functions that failed slot map registration.
            if (deserializer.hasFallbackSource()) {
                ExceptionSink wsink;
                // Strip PO_NO_TOP_LEVEL_STATEMENTS from fallback parse options because the
                // full source may have top-level statements before %exec-class directive;
                // the original parser processes directives sequentially but AOT bakes all
                // parse options (including %exec-class) upfront
                fallback_pgm = new QoreProgram(parse_options & ~PO_NO_TOP_LEVEL_STATEMENTS);
                // Set script path so %requires with relative paths resolve correctly
                fallback_pgm->setScriptPath(label);
                fallback_pgm->parse(deserializer.getFallbackSource(), label, &xsink, &wsink,
                    QP_WARN_DEFAULT);
                if (wsink.isException()) {
                    wsink.handleWarnings();
                }
                if (xsink.isException()) {
                    // Fallback source parsing failed (e.g., module class conflicts).
                    // This is non-fatal: module functions are available from runtime module
                    // loading, only test-local functions that failed compilation are lost.
                    printd(0, "AOT v2 '%s': fallback source parse failed\n", label);
                    xsink.clear();
                    fallback_pgm->waitForTerminationAndDeref(nullptr);
                    fallback_pgm = nullptr;
                } else {
                    printd(5, "AOT v2 '%s': fallback source parsed OK, remaining funcs=%d\n",
                        label, (int)func_map.size());
                    // Register remaining functions from fallback via IR re-lowering.
                    // Build contexts from fallback AST but register on main program's variants
                    // for consistent LocalVar* pointer identity with the thread-local stack.
                    if (!func_map.empty()) {
                        int fallback_registered = 0;
                        qore_ns_private* fallback_root_ns = qore_ns_private::get(
                            *qore_program_private::get(*fallback_pgm)->RootNS);
                        qore_ns_private* main_root_ns = qore_ns_private::get(*pp->RootNS);
                        registerFallbackFunctionsOnMainVariants(
                            fallback_root_ns, main_root_ns, *qpgm, func_map,
                            fallback_registered);
                        printd(2, "AOT v3: fallback registered %d, func_map.size=%d\n",
                            fallback_registered, (int)func_map.size());
                        registered += fallback_registered;
                    }

                    // Transplant closure/code constants from fallback classes to main classes.
                    // These can't be serialized in the binary (written as VT_NOTHING).
                    {
                        qore_ns_private* fb_root = qore_ns_private::get(
                            *qore_program_private::get(*fallback_pgm)->RootNS);
                        qore_ns_private* main_root = qore_ns_private::get(*pp->RootNS);
                        transplantClassClosureValues(fb_root, main_root, *qpgm);
                    }
                }
            }

            // Transplant BCAList for all constructors — needed even when registered from
            // slot maps, because slot map registration only sets the native fn_ptr but
            // doesn't provide the BCAList (base class constructor arguments).
            if (fallback_pgm) {
                qore_ns_private* fb_root = qore_ns_private::get(
                    *qore_program_private::get(*fallback_pgm)->RootNS);
                qore_ns_private* main_root = qore_ns_private::get(*pp->RootNS);
                transplantConstructorBCALists(fb_root, main_root);
            }

            // Register the _toplevel function from slot maps
            if (toplevel_func) {
                // Find _toplevel in SLOT_MAPS section
                const QoreAOTSectionHeader* sm_sec = deserializer.getReader().findSection(
                    QoreAOTSectionType::SLOT_MAPS);
                bool toplevel_registered = false;

                if (sm_sec) {
                    const uint8_t* sm_ptr = deserializer.getReader().getSectionData(*sm_sec);
                    if (sm_ptr) {
                        const uint8_t* sm_end = sm_ptr + sm_sec->size;
                        uint32_t sm_count = QoreAOTBinaryReader::readU32(sm_ptr);
                        for (uint32_t fi = 0; fi < sm_count; ++fi) {
                            const uint8_t* entry_start = sm_ptr;
                            uint32_t entry_size = QoreAOTBinaryReader::readU32(sm_ptr);  // consume size prefix
                            const char* entry_name = deserializer.getReader().readStringRef(sm_ptr);  // peek at name
                            const uint8_t* entry_end = entry_start + 4 + entry_size;
                            sm_ptr = entry_start + 4;  // reset: after size prefix, before name

                            if (entry_name && strcmp(entry_name, "_toplevel") == 0) {
                                QoreAOTContext* ctx = buildContextFromSlotMap(
                                    deserializer.getReader(), sm_ptr, sm_end,
                                    nullptr, *qpgm, *toplevel_func, "_toplevel");
                                if (ctx) {
                                    pp->sb.registerPrecompiledAOTTopLevel(
                                        toplevel_func->fn_ptr, ctx);
                                    // Set LVList so doTopLevelInstantiation() can instantiate the locals
                                    pp->sb.setLVarsFromAOTContext(ctx);
                                    ++registered;
                                    toplevel_registered = true;
                                    printd(2, "AOT v3: registered _toplevel from slot map\n");
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

                if (!toplevel_registered && fallback_pgm) {
                    // Fall back to IR re-lowering path for toplevel using fallback source
                    qore_program_private* fb_pp = qore_program_private::get(*fallback_pgm);
                    TopLevelStatementBlock& fb_sb = fb_pp->sb;

                    QoreIRFunction* ir_func = new QoreIRFunction("_toplevel");

                    // Collect ALL body locals (top-level + nested blocks) as pre-instantiated
                    collectAllStatementLocals(&fb_sb, ir_func->all_body_locals);
                    for (LocalVar* lv : ir_func->all_body_locals) {
                        ir_func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(lv));
                    }

                    QoreIRBuilder builder(ir_func);
                    auto* entry = ir_func->createBlock("entry");
                    builder.setBlock(entry);

                    QoreParseContext parse_context(fallback_pgm);
                    QoreIRLowering lowering(builder, &parse_context);
                    std::string lower_error;
                    bool ctx_ok = false;
                    if (lowering.lowerStatementBlock(&fb_sb, lower_error)) {
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
                                toplevel_func->num_exprs, toplevel_func->num_stmts, toplevel_func->num_regex_cases);
                            if (ctx) {
                                pp->sb.registerPrecompiledAOTTopLevel(toplevel_func->fn_ptr, ctx);
                                // Set LVList so doTopLevelInstantiation() can instantiate the locals
                                pp->sb.setLVarsFromAOTContext(ctx);
                                ++registered;
                                ctx_ok = true;
                                printd(2, "AOT v3: registered _toplevel via fallback IR\n");
                            }
                        } else {
                            printd(0, "AOT v3: _toplevel re-verification failed: %s\n",
                                verify_error.c_str());
                        }
                    } else {
                        printd(0, "AOT v3: _toplevel re-lowering failed: %s\n", lower_error.c_str());
                    }
                    delete ir_func;

                    if (!ctx_ok) {
                        printd(0, "AOT v3: failed to build context for _toplevel\n");
                    }
                } else if (!toplevel_registered) {
                    printd(0, "AOT v3: _toplevel not registered (no slot map or fallback)\n");
                }

            }

            printd(2, "AOT v3: registered %d/%d pre-compiled functions\n", registered, num_functions);

            // Safety check: if functions weren't registered and no fallback source is available,
            // they will exist as empty shells that crash when called. Warn early.
            if (registered < num_functions && !deserializer.hasFallbackSource() && !fallback_pgm) {
                int unregistered = num_functions - registered;
                printd(0, "AOT ERROR: %d/%d functions could not be registered and no fallback "
                    "source is available.\nRecompile with --include-source or update the AOT compiler "
                    "to embed fallback sources automatically.\n", unregistered, num_functions);
            }
        }

        // NOTE: Do NOT clean up fallback_pgm yet - it may contain StatementBlock* pointers
        // that are referenced by AOT contexts for on_exit/on_success/on_error blocks.
        // We must keep it alive until after the program finishes running.

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

        // Clean up fallback program AFTER program execution - it contains StatementBlock*
        // pointers used by AOT contexts for on_exit/on_success/on_error blocks
        if (fallback_pgm) {
            fallback_pgm->waitForTerminationAndDeref(nullptr);
            fallback_pgm = nullptr;
        }
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

    // Use a local variable for the program being created.  Loading dependencies
    // via runTimeLoadModule() can trigger nested calls to qore_aot_module_init()
    // for other AOT modules, which would overwrite the global aot_module_pgm.
    QoreProgram* local_pgm = new QoreProgram(parse_options);

    // Set JIT execution mode so functions without pre-compiled code will JIT on demand
    local_pgm->setExecMode(QEM_JIT);

    // Set script path from the label so get_script_dir() returns the module's source
    // directory during parsing.  This is needed for modules that read files at parse time
    // (e.g., DataProvider loads qore-q-logo.svg via get_script_dir() + filename).
    if (label) {
        local_pgm->setScriptPath(label);
    }

    // Extract dependencies from source and load/import their namespaces
    // Note: The init function is now called with the module manager lock unlocked
    // (via ModuleLoadMapHelper), so we can safely load dependencies here.
    std::vector<std::string> reexport_deps;
    std::vector<std::string> deps = extractDependencies(source, source_len, &reexport_deps);
    for (const std::string& dep : deps) {
        // Try to load the module (it may already be loaded, which is fine)
        int rc = MM.runTimeLoadModule(&xsink, dep.c_str(), local_pgm);
        if (rc < 0 || xsink) {
            // Circular dependency or other issue - clear error and continue
            // The types might be resolved later when the requiring script is parsed
            xsink.clear();
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
            err->concat(ex_err.get<const QoreStringNode>()->c_str());
        } else {
            err->concat("unknown parse error");
        }
        if (ex_desc.getType() == NT_STRING) {
            err->concat(", desc: ");
            err->concat(ex_desc.get<const QoreStringNode>()->c_str());
        }
        if (ex_arg.getType() == NT_STRING) {
            err->concat(", arg: ");
            err->concat(ex_arg.get<const QoreStringNode>()->c_str());
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
                    err->concat(ex_desc.get<const QoreStringNode>()->c_str());
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
    // Also update the global for the fallback path in ns_init (for modules
    // that call ns_init during their own init before being stored in the map).
    aot_module_pgm = local_pgm;
    aot_module_name = mod_name;
    aot_module_funcs = functions;
    aot_module_num_funcs = num_functions;
    aot_module_map[mod_name] = {local_pgm, functions, num_functions, std::move(reexport_deps)};

    return nullptr;  // success
}

extern "C" DLLEXPORT void qore_aot_module_ns_init(QoreNamespace* root_ns, QoreNamespace* qore_ns) {
    printd(5, "AOT ns_init called: root_ns=%p qore_ns=%p\n", (void*)root_ns, (void*)qore_ns);
    // Look up the correct module program from the per-module map.
    // When QoreBuiltinModule::addToProgramImpl calls module_ns_init, it has set up
    // QoreModuleContextHelper with the module's name. We use get_module_context() to
    // find which module's namespace to merge.
    const char* mod_name = nullptr;
    QoreProgram* mod_pgm = nullptr;
    const std::vector<std::string>* reexport_deps = nullptr;

    QoreModuleContext* ctx = get_module_context();
    if (ctx) {
        mod_name = ctx->getName();
        auto it = aot_module_map.find(mod_name);
        if (it != aot_module_map.end()) {
            mod_pgm = it->second.pgm;
            reexport_deps = &it->second.reexport_deps;
        }
    }

    // Fall back to the current module being initialized (for the module's own registration)
    if (!mod_pgm) {
        mod_pgm = aot_module_pgm;
        if (!mod_name) {
            mod_name = aot_module_name.c_str();
        }
    }

    if (!mod_pgm) {
        printd(5, "AOT module ns_init: no program for '%s'!\n", mod_name ? mod_name : "(unknown)");
        return;
    }

    // Get the module program's root namespace
    RootQoreNamespace* mod_root = mod_pgm->getRootNS();
    if (!mod_root) {
        printd(5, "AOT module ns_init '%s': no root namespace!\n", mod_name ? mod_name : "(unknown)");
        return;
    }

    // Use the same namespace merge mechanism as user modules to properly handle
    // class hierarchy references. Simple copy() doesn't work because base class
    // pointers would still reference classes from the AOT module's program.
    ExceptionSink xsink;
    RootQoreNamespace* target_root = static_cast<RootQoreNamespace*>(root_ns);

    // Set up the target program as the current program context.
    // scanMergeCommittedNamespace() calls parse_check_parse_option() which requires
    // current_pgm to be set. QoreBuiltinModule::addToProgramImpl() only sets
    // call_program_context (via ProgramCallContextHelper), not current_pgm.
    // Use the same approach as QoreUserModule::addToProgramImpl().
    QoreProgram* tpgm = getProgram();
    ProgramThreadCountContextHelper ptcch(&xsink, tpgm, false);
    if (xsink) {
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

    // Rebuild indexes so the merged items can be found during name resolution
    // This is needed because copyMergeCommittedNamespace adds items directly without
    // going through the module commit mechanism that normally rebuilds indexes
    printd(5, "AOT module ns_init '%s': calling rebuildAllIndexes\n", mod_name);
    qore_root_ns_private::get(*target_root)->rebuildAllIndexes();

    // Check for exceptions during merge operations
    if (xsink) {
        const QoreValue err = xsink.getExceptionErr();
        const char* err_str = (err.getType() == NT_STRING) ? err.get<const QoreStringNode>()->c_str() : "(unknown)";
        printd(0, "AOT module ns_init '%s': WARNING - exception during namespace merge: %s\n",
            mod_name, err_str);
    }

    printd(5, "AOT module ns_init '%s': merge complete\n", mod_name);

    // Execute deferred init functions for constants/static vars.
    // This must happen AFTER namespace merge so that class hierarchies are fully
    // committed and object constructors can find base class private data slots.
    // We build init function contexts HERE using the TARGET program's namespace tree
    // (which has properly committed classes) rather than the module program's tree.
    if (mod_name) {
        auto it = aot_module_map.find(mod_name);
        if (it != aot_module_map.end() && !it->second.init_descriptors.empty()
                && !it->second.metadata.empty()) {
            // Build function table from the stored function pointers
            std::unordered_map<std::string, const QoreAOTFunc*> func_map;
            for (int i = 0; i < it->second.num_funcs; ++i) {
                if (it->second.funcs[i].name && it->second.funcs[i].fn_ptr) {
                    func_map[it->second.funcs[i].name] = &it->second.funcs[i];
                }
            }

            // Build init function contexts from slot maps using the TARGET program
            // (tpgm) which has properly committed class hierarchies after merge
            QoreAOTBinaryReader init_reader;
            std::string reader_error;
            if (init_reader.open(it->second.metadata.data(),
                    static_cast<uint32_t>(it->second.metadata.size()), reader_error)) {
                qore_ns_private* target_root_priv = qore_ns_private::get(
                    *static_cast<RootQoreNamespace*>(root_ns));
                std::vector<AOTInitFuncExecInfo> init_func_contexts;
                int registered = 0;
                registerAOTFunctionsFromSlotMaps(init_reader, target_root_priv,
                    tpgm, func_map, registered, &init_func_contexts);

                printd(2, "AOT ns_init '%s': got %d init func contexts\n",
                    mod_name, (int)init_func_contexts.size());
                if (!init_func_contexts.empty()) {
                    printd(2, "AOT ns_init '%s': executing %d init functions\n",
                        mod_name, (int)init_func_contexts.size());
                    executeInitFunctions(tpgm, init_func_contexts,
                        it->second.init_descriptors, mod_name);
                }
            }
            // Clear stored metadata
            it->second.metadata.clear();
            it->second.init_descriptors.clear();
        }
    }
}

extern "C" DLLEXPORT void qore_aot_module_delete() {
    // Use the module context name to clean up only the module being unloaded
    // (set by QoreBuiltinModule::~QoreBuiltinModule via QoreModuleNameContextHelper)
    const char* mod_name = get_module_context_name();
    if (mod_name) {
        auto it = aot_module_map.find(mod_name);
        if (it != aot_module_map.end()) {
            if (it->second.pgm) {
                it->second.pgm->waitForTerminationAndDeref(nullptr);
            }
            aot_module_map.erase(it);
        }
        // Clear per-module state if it matches the module being deleted
        if (aot_module_name == mod_name) {
            aot_module_pgm = nullptr;
            aot_module_name.clear();
            aot_module_funcs = nullptr;
            aot_module_num_funcs = 0;
        }
    } else {
        // Fallback: no module context — clean up all (shutdown path)
        for (auto& entry : aot_module_map) {
            if (entry.second.pgm) {
                entry.second.pgm->waitForTerminationAndDeref(nullptr);
            }
        }
        aot_module_map.clear();
        aot_module_pgm = nullptr;
        aot_module_name.clear();
        aot_module_funcs = nullptr;
        aot_module_num_funcs = 0;
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

    // Use a local variable for the program being created (see qore_aot_module_init
    // for explanation of nested init overwrite issue)
    QoreProgram* local_pgm = new QoreProgram(parse_options);

    // Set JIT execution mode
    local_pgm->setExecMode(QEM_JIT);

    // Load dependencies from serialized metadata BEFORE deserializing namespace tree.
    // Dependencies must be loaded first because deserialization may need to resolve
    // base classes, types, and other references from dependency modules.
    std::vector<std::string> deps;
    std::string dep_error;
    if (!readDependencies(metadata, static_cast<uint32_t>(metadata_len), deps, dep_error)) {
        QoreStringNode* err = new QoreStringNode("AOT module dependency read error: ");
        err->concat(dep_error.c_str());
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
    for (const std::string& dep : deps) {
        printd(5, "AOT module v2 '%s': loading dependency '%s'\n", mod_name, dep.c_str());
        int rc = MM.runTimeLoadModule(&xsink, dep.c_str(), local_pgm);
        if (rc < 0 || xsink) {
            // Circular dependency or other issue - clear error and continue
            // The types might be resolved later when the requiring script is parsed
            printd(5, "AOT module v2 '%s': dependency '%s' load error (rc=%d)\n", mod_name, dep.c_str(), rc);
            xsink.clear();
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
        // Set parse context so UserVariantBase constructor can call
        // parse_get_parse_options() which reads thread-local current_pgm
        ProgramRuntimeParseContextHelper pch(&xsink, local_pgm);
        if (xsink.isException()) {
            xsink.clear();
            local_pgm->waitForTerminationAndDeref(nullptr);
            return new QoreStringNode("AOT module v2: failed to set parse context");
        }
        if (!deserializer.deserializeIntoProgram(local_pgm,
                metadata, static_cast<uint32_t>(metadata_len), deser_error)) {
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
        registerAOTFunctionsFromSlotMaps(deserializer.getReader(), root_ns,
            local_pgm, func_map, registered);

        // Fall back to namespace walk for any functions not registered from slot maps
        // (e.g., functions that had no slot map entry)
        if (registered < num_functions) {
            registerAOTFunctionsInNamespace(root_ns, local_pgm, func_map, registered);
        }

        printd(1, "AOT module v2 '%s': registered %d/%d pre-compiled functions\n",
            mod_name, registered, num_functions);
    }

    // Store per-module state so ns_init can find the correct program.
    // Also update the global for the fallback path in ns_init.
    aot_module_pgm = local_pgm;
    aot_module_name = mod_name;
    aot_module_funcs = functions;
    aot_module_num_funcs = num_functions;
    aot_module_map[mod_name] = {local_pgm, functions, num_functions, std::move(reexport_deps)};

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

//! Execute collected init functions and store results in target constants/static vars
/** Called after AOT function registration to initialize constants and static vars
    whose values come from lowered init expressions (delayed_eval constants, object
    constructors, runtime-dependent expressions like now()).

    Init functions are executed in serialization order, which matches the order
    the parser originally processed them — this ensures dependencies between
    constants are satisfied.
*/
static void executeInitFunctions(
        QoreProgram* pgm,
        const std::vector<AOTInitFuncExecInfo>& exec_infos,
        const std::vector<AOTInitFuncDescriptor>& descriptors,
        const char* mod_name) {
    if (exec_infos.empty() || descriptors.empty()) {
        return;
    }

    // Build name → exec info map
    std::unordered_map<std::string, const AOTInitFuncExecInfo*> exec_map;
    for (auto& info : exec_infos) {
        exec_map[info.name] = &info;
    }

    qore_program_private* pp = qore_program_private::get(*pgm);
    qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);

    int executed = 0;
    int failed = 0;

    // Execute in descriptor order (matches compilation/parser order)
    for (auto& desc : descriptors) {
        auto it = exec_map.find(desc.name);
        if (it == exec_map.end()) {
            printd(2, "AOT init: no context for '%s' — skipping\n", desc.name.c_str());
            continue;
        }

        const AOTInitFuncExecInfo* info = it->second;

        // Call the init function
        printd(5, "AOT init: calling '%s' fn_ptr=%p ctx=%p\n",
            desc.name.c_str(), (void*)info->fn_ptr, (void*)info->ctx);
        ExceptionSink xsink;
        uint64_t raw_result = info->fn_ptr(info->ctx, &xsink);
        printd(5, "AOT init: '%s' returned raw=%llu\n", desc.name.c_str(), (unsigned long long)raw_result);

        if (xsink.isException()) {
            QoreValue err_val = xsink.getExceptionErr();
            QoreValue desc_val = xsink.getExceptionDesc();
            printd(5, "AOT init: '%s' raised exception: %s: %s\n",
                desc.name.c_str(),
                err_val.getType() == NT_STRING ? err_val.get<const QoreStringNode>()->c_str() : "?",
                desc_val.getType() == NT_STRING ? desc_val.get<const QoreStringNode>()->c_str() : "?");
            xsink.clear();
            ++failed;
            continue;
        }

        // Convert raw result to QoreValue
        QoreValue result;
        memcpy(&result, &raw_result, sizeof(uint64_t));

        // Store the result in the target constant or static var
        switch (desc.target_type) {
            case AOTCompiledInitFunc::NS_CONSTANT: {
                qore_ns_private* ns = findNamespaceByPath(root_ns, desc.ns_path);
                if (!ns) {
                    printd(0, "AOT init: namespace '%s' not found for constant '%s'\n",
                        desc.ns_path.c_str(), desc.item_name.c_str());
                    result.discard(&xsink);
                    ++failed;
                    break;
                }
                ConstantEntry* ce = ns->constant.findEntry(desc.item_name.c_str());
                if (!ce) {
                    printd(0, "AOT init: constant '%s' not found in namespace '%s'\n",
                        desc.item_name.c_str(), desc.ns_path.c_str());
                    result.discard(&xsink);
                    ++failed;
                    break;
                }
                // Discard old value and store new one
                ce->val.discard(&xsink);
                ce->val = result;
                ce->init = true;
                ++executed;
                printd(2, "AOT init: initialized namespace constant '%s::%s' type=%s\n",
                    desc.ns_path.c_str(), desc.item_name.c_str(), result.getTypeName());
                break;
            }

            case AOTCompiledInitFunc::CLASS_CONSTANT: {
                // ns_path is the class path like "DataProvider::AbstractDataProvider"
                const qore_ns_private* found_ns = nullptr;
                const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
                    *pp->RootNS, desc.ns_path.c_str(), found_ns);
                if (!qc) {
                    printd(0, "AOT init: class '%s' not found for constant '%s'\n",
                        desc.ns_path.c_str(), desc.item_name.c_str());
                    result.discard(&xsink);
                    ++failed;
                    break;
                }
                qore_class_private* qcp = qore_class_private::get(
                    *const_cast<QoreClass*>(qc));
                ConstantEntry* ce = qcp->constlist.findEntry(desc.item_name.c_str());
                if (!ce) {
                    printd(0, "AOT init: constant '%s' not found in class '%s'\n",
                        desc.item_name.c_str(), desc.ns_path.c_str());
                    result.discard(&xsink);
                    ++failed;
                    break;
                }
                ce->val.discard(&xsink);
                ce->val = result;
                ce->init = true;
                ++executed;
                printd(2, "AOT init: initialized class constant '%s::%s' type=%s\n",
                    desc.ns_path.c_str(), desc.item_name.c_str(), result.getTypeName());
                break;
            }

            case AOTCompiledInitFunc::STATIC_VAR: {
                // ns_path is the class path
                const qore_ns_private* found_ns = nullptr;
                const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
                    *pp->RootNS, desc.ns_path.c_str(), found_ns);
                if (!qc) {
                    printd(0, "AOT init: class '%s' not found for static var '%s'\n",
                        desc.ns_path.c_str(), desc.item_name.c_str());
                    result.discard(&xsink);
                    ++failed;
                    break;
                }
                qore_class_private* qcp = qore_class_private::get(
                    *const_cast<QoreClass*>(qc));
                // Find and set the static var value
                QoreVarInfo* vi = qcp->vars.find(desc.item_name.c_str());
                if (!vi) {
                    printd(0, "AOT init: static var '%s' not found in class '%s'\n",
                        desc.item_name.c_str(), desc.ns_path.c_str());
                    result.discard(&xsink);
                    ++failed;
                    break;
                }
                vi->assignInit(result);
                vi->eval_init = true;
                ++executed;
                printd(2, "AOT init: initialized static var '%s::%s' type=%s\n",
                    desc.ns_path.c_str(), desc.item_name.c_str(), result.getTypeName());
                break;
            }
        }
    }

    // Clean up init function contexts
    for (auto& info : exec_infos) {
        delete info.ctx;
    }

    printd(5, "AOT module '%s': executed %d/%d init functions (%d failed)\n",
        mod_name, executed, (int)exec_infos.size(), failed);
}

//! C ABI entry point for AOT modules (v3 - full 128-bit parse options)
extern "C" DLLEXPORT QoreStringNode* qore_aot_module_init_v3(
    const uint8_t* metadata, int metadata_len,
    const char* label,
    int64_t parse_options_lo, int64_t parse_options_hi,
    const char* mod_name,
    const QoreAOTFunc* functions, int num_functions
) {
    printd(5, "AOT v3 ENTRY '%s': num_functions=%d functions=%p\n",
        mod_name, num_functions, (const void*)functions);
    // Construct full 128-bit parse options from lo+hi components
    QoreParseOptions parse_options(parse_options_lo, parse_options_hi);

    ExceptionSink xsink;

    // Use a local variable for the program being created (see qore_aot_module_init
    // for explanation of nested init overwrite issue)
    QoreProgram* local_pgm = new QoreProgram(parse_options);

    // Set JIT execution mode
    local_pgm->setExecMode(QEM_JIT);

    // Load dependencies from serialized metadata BEFORE deserializing namespace tree.
    // Dependencies must be loaded first because deserialization may need to resolve
    // base classes, types, and other references from dependency modules.
    std::vector<std::string> deps;
    std::string dep_error;
    if (!readDependencies(metadata, static_cast<uint32_t>(metadata_len), deps, dep_error)) {
        QoreStringNode* err = new QoreStringNode("AOT module dependency read error: ");
        err->concat(dep_error.c_str());
        local_pgm->waitForTerminationAndDeref(nullptr);
        return err;
    }

    // Read reexported module names from metadata
    std::vector<std::string> reexport_deps;
    std::string reexport_error;
    if (!readReexportModules(metadata, static_cast<uint32_t>(metadata_len), reexport_deps, reexport_error)) {
        printd(0, "AOT module v3 '%s': WARNING - failed to read reexport modules: %s\n",
            mod_name, reexport_error.c_str());
        // Non-fatal — continue without reexport info
    }

    // Load each dependency module into this program.
    // runTimeLoadModule will call addToProgram which imports the namespace.
    for (const std::string& dep : deps) {
        printd(5, "AOT module v3 '%s': loading dependency '%s'\n", mod_name, dep.c_str());
        int rc = MM.runTimeLoadModule(&xsink, dep.c_str(), local_pgm);
        if (rc < 0 || xsink) {
            // Circular dependency or other issue - clear error and continue
            // The types might be resolved later when the requiring script is parsed
            printd(5, "AOT module v3 '%s': dependency '%s' load error (rc=%d)\n", mod_name, dep.c_str(), rc);
            xsink.clear();
        }
    }

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
        // Set parse context so UserVariantBase constructor can call
        // parse_get_parse_options() which reads thread-local current_pgm
        ProgramRuntimeParseContextHelper pch(&xsink, local_pgm);
        if (xsink.isException()) {
            xsink.clear();
            local_pgm->waitForTerminationAndDeref(nullptr);
            return new QoreStringNode("AOT module v3: failed to set parse context");
        }
        if (!deserializer.deserializeIntoProgram(local_pgm,
                metadata, static_cast<uint32_t>(metadata_len), deser_error)) {
            QoreStringNode* err = new QoreStringNode("AOT module metadata deserialization error: ");
            err->concat(deser_error.c_str());
            local_pgm->waitForTerminationAndDeref(nullptr);
            return err;
        }
    }

    // Advisory checks for source staleness and feature compatibility
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
        // Feature compatibility check
        if (aot_hdr.feature_flags != 0) {
            uint64_t unsupported = aot_hdr.feature_flags & ~QORE_AOT_SUPPORTED_FEATURES;
            if (unsupported) {
                printd(0, "AOT WARNING: binary '%s' requires unsupported features 0x%016llx; "
                    "affected functions will fall back to JIT\n",
                    label ? label : "<unknown>",
                    (unsigned long long)unsupported);
            }
        }
    }

    // Register pre-compiled AOT functions using slot maps (v3 uses metadata
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
        registerAOTFunctionsFromSlotMaps(deserializer.getReader(), root_ns,
            local_pgm, func_map, registered);

        // Fall back to namespace walk for any functions not registered from slot maps
        // (e.g., functions that had no slot map entry)
        if (registered < num_functions) {
            registerAOTFunctionsInNamespace(root_ns, local_pgm, func_map, registered);
        }

        printd(1, "AOT module v3 '%s': registered %d/%d pre-compiled functions\n",
            mod_name, registered, num_functions);
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
        if (readInitFuncs(metadata, static_cast<uint32_t>(metadata_len),
                init_descriptors, init_error)) {
            printd(2, "AOT v3 '%s': read %d init descriptors, deferring for ns_init\n",
                mod_name, (int)init_descriptors.size());
        } else {
            // No INIT_FUNCS section or read error — not an error, just no init functions
            printd(5, "AOT v3 '%s': no INIT_FUNCS section: %s\n",
                mod_name, init_error.c_str());
        }
    }

    // Parse fallback source if available to recover values that can't be serialized
    // (e.g., object constants, closure constants, static member initializers).
    if (deserializer.hasFallbackSource()) {
        ExceptionSink wsink;
        QoreProgram* fallback_pgm = new QoreProgram(parse_options & ~PO_NO_TOP_LEVEL_STATEMENTS);
        fallback_pgm->setScriptPath(label);
        fallback_pgm->parse(deserializer.getFallbackSource(), label, &xsink, &wsink,
            QP_WARN_DEFAULT);
        if (wsink.isException()) {
            wsink.handleWarnings();
        }
        if (xsink.isException()) {
            printd(0, "AOT v3 '%s': fallback source parse failed\n", mod_name);
            xsink.clear();
            fallback_pgm->waitForTerminationAndDeref(nullptr);
        } else {
            // Transplant object constants and closure values from fallback to main program.
            qore_program_private* pp = qore_program_private::get(*local_pgm);
            qore_ns_private* fb_root = qore_ns_private::get(
                *qore_program_private::get(*fallback_pgm)->RootNS);
            qore_ns_private* main_root = qore_ns_private::get(*pp->RootNS);
            transplantClassClosureValues(fb_root, main_root, local_pgm);

            // Transplant BCAList for constructors.
            transplantConstructorBCALists(fb_root, main_root);

            // Note: fallback_pgm is NOT cleaned up here — it may contain values
            // (closures, objects) referenced by the main program's constants.
        }
    }

    // Store per-module state so ns_init can find the correct program.
    // Also update the global for the fallback path in ns_init.
    aot_module_pgm = local_pgm;
    aot_module_name = mod_name;
    aot_module_funcs = functions;
    aot_module_num_funcs = num_functions;
    {
        AotModuleState state;
        state.pgm = local_pgm;
        state.funcs = functions;
        state.num_funcs = num_functions;
        state.reexport_deps = std::move(reexport_deps);
        state.init_descriptors = std::move(init_descriptors);
        // Store metadata copy for ns_init to build init function contexts
        if (!state.init_descriptors.empty()) {
            state.metadata.assign(metadata, metadata + metadata_len);
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
    mod_info->api_minor = api_minor;
    mod_info->license = static_cast<qore_license_t>(license);
    if (license_str) {
        mod_info->license_str = license_str;
    }
    mod_info->init = reinterpret_cast<qore_module_init_t>(init_fn);
    mod_info->ns_init = reinterpret_cast<qore_module_ns_init_t>(ns_init_fn);
    mod_info->del = reinterpret_cast<qore_module_delete_t>(del_fn);
    for (int i = 0; i < num_deps; ++i) {
        if (deps[i]) {
            mod_info->dependencies.push_back(deps[i]);
        }
    }
}

extern "C" DLLEXPORT void qore_aot_raise_init_error(ExceptionSink* xsink, QoreStringNode* err) {
    if (err) {
        xsink->raiseException("MODULE-LOAD-ERROR", err);
    }
}
