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
#include "qore/intern/StatementBlock.h"
#include "qore/intern/Function.h"
#include "qore/intern/QoreIR.h"
#include "qore/intern/QoreIRBuilder.h"
#include "qore/intern/QoreIRLowering.h"
#include "qore/intern/QoreIRVerifier.h"

#include "qore/intern/ModuleInfo.h"
#include "qore/intern/VarRefNode.h"
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
#include "qore/intern/QoreSquareBracketsRangeOperatorNode.h"
#include "qore/intern/QoreRegex.h"
#include "qore/intern/QoreRegexSubst.h"
#include "qore/intern/QoreTransliteration.h"
#include <qore/QoreNumberNode.h>
#include <qore/BinaryNode.h>

#include <cassert>
#include <cstring>
#include <string>
#include <deque>
#include <unordered_map>

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
                printd(0, "AOT v2: cannot resolve class '%s' for self method '%s'\n",
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
                printd(0, "AOT v2: cannot find method '%s::%s'\n", ref1, ref2);
                return 0;
            }
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
            NewObjectCallNode* nocn = new NewObjectCallNode(qc, nullptr);
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

        case AOTExprKind::CONST_NUMBER: {
            if (!ref1 || !*ref1) {
                return 0;
            }
            QoreNumberNode* num = new QoreNumberNode(ref1);
            return toBitsNB(QoreValue(num));
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

        case AOTExprKind::CALL_REF:
        case AOTExprKind::OBJ_METHOD_REF:
        case AOTExprKind::STATIC_VARREF:
        case AOTExprKind::RUNTIME_CONST_REF:
            // These need the full AST context for proper resolution
            printd(1, "AOT v2: expression kind %d requires source fallback\n", (int)kind);
            return 0;

        case AOTExprKind::EXPR_TREE:
            // Handled inline in buildContextFromSlotMap
            return 0;

        case AOTExprKind::GENERIC_EVAL:
        default:
            // Unsupported — function needs source fallback
            return 0;
    }
}

// ---- Expression Tree Deserializer ----

//! Deserializes an AOT EXPR_TREE binary blob back into AST nodes
class ExprTreeDeserializer {
    const uint8_t* ptr;
    const uint8_t* end;
    QoreProgram* pgm;
    QoreAOTContext* ctx;
    bool debug;
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

            // ---- Variable references ----

            case AOTExprNodeKind::EN_LOCAL_VAR: {
                uint16_t slot = readU16();
                readU16(); // num_children
                if (slot < ctx->num_locals && ctx->locals[slot]) {
                    LocalVar* lv = ctx->locals[slot];
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
                    qore_ns_private* root_ns = qore_ns_private::get(*pp->RootNS);
                    Var* v = root_ns->var_list.runtimeFindVar(name.c_str());
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
                    printd(0, "AOT EXPR_TREE: cannot resolve class '%s' for self call '%s'\n",
                        class_name.c_str(), method_name.c_str());
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
                    printd(0, "AOT EXPR_TREE: cannot find method '%s::%s'\n",
                        class_name.c_str(), method_name.c_str());
                    return fail();
                }
                SelfFunctionCallNode* sfcn = new SelfFunctionCallNode(&loc_builtin,
                    strdup(method_name.c_str()), pln.release(), m,
                    m->getClass(), qore_class_private::get(*m->getClass()));
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
                return QoreValue(new NewObjectCallNode(qc, args_list.release()));
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
                const TypedHashDecl* hd = QoreTypeInfo::getUniqueReturnHashDecl(ti);
                if (hd) {
                    return QoreValue(new QoreHashDeclCastOperatorNode(&loc_builtin, hd, operand,
                        or_nothing != 0));
                }
                // Complex hash or list types
                qore_type_t bt = QoreTypeInfo::getBaseType(ti);
                if (bt == NT_HASH) {
                    return QoreValue(new QoreComplexHashCastOperatorNode(&loc_builtin, ti, operand,
                        or_nothing != 0));
                }
                if (bt == NT_LIST) {
                    return QoreValue(new QoreComplexListCastOperatorNode(&loc_builtin, ti, operand,
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

            case AOTExprNodeKind::EN_CLOSURE:
                // Cannot deserialize closures
                printd(0, "AOT EXPR_TREE: closure deserialization not supported\n");
                return fail();

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

            default:
                printd(0, "AOT EXPR_TREE: unknown node kind %d\n", (int)kind);
                return fail();
        }
    }

public:
    ExprTreeDeserializer(const uint8_t* data, uint32_t size, QoreProgram* p, QoreAOTContext* c)
        : ptr(data), end(data + size), pgm(p), ctx(c),
          debug(getenv("QORE_AOT_DEBUG") != nullptr) {
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
    //         num_stmts(u16), num_body_locals(u16), has_unsupported(u8), padding(u8)
    /*const char* func_name =*/ reader.readStringRef(ptr);
    uint16_t num_locals = QoreAOTBinaryReader::readU16(ptr);
    uint16_t num_globals = QoreAOTBinaryReader::readU16(ptr);
    uint16_t num_exprs = QoreAOTBinaryReader::readU16(ptr);
    uint16_t num_stmts = QoreAOTBinaryReader::readU16(ptr);
    uint16_t num_body_locals = QoreAOTBinaryReader::readU16(ptr);
    uint8_t has_unsupported = QoreAOTBinaryReader::readU8(ptr);
    QoreAOTBinaryReader::readU8(ptr); // padding

    // Validate slot counts match the AOT function descriptor
    if (num_locals != aot_func.num_locals || num_globals != aot_func.num_globals
            || num_exprs != aot_func.num_exprs || num_stmts != aot_func.num_stmts) {
        printd(0, "AOT v2: slot count mismatch for '%s': binary(%d,%d,%d,%d) vs func(%d,%d,%d,%d)\n",
            name, num_locals, num_globals, num_exprs, num_stmts,
            aot_func.num_locals, aot_func.num_globals, aot_func.num_exprs, aot_func.num_stmts);
        return nullptr;
    }

    printd(2, "AOT v2: buildContextFromSlotMap '%s': locals=%d globals=%d exprs=%d stmts=%d body_locals=%d "
        "has_unsupported=%d uvb=%p\n", name, num_locals, num_globals, num_exprs, num_stmts, num_body_locals,
        has_unsupported, (void*)uvb);

    auto* ctx = new QoreAOTContext();
    ctx->num_locals = num_locals;
    ctx->num_globals = num_globals;
    ctx->num_exprs = num_exprs;
    ctx->num_stmts = num_stmts;
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
    for (int i = 0; i < num_exprs; ++i) {
        uint8_t kind_byte = QoreAOTBinaryReader::readU8(ptr);
        AOTExprKind kind = static_cast<AOTExprKind>(kind_byte);
        const char* ref1 = nullptr;
        const char* ref2 = nullptr;

        switch (kind) {
            case AOTExprKind::FUNC_CALL:
            case AOTExprKind::NEW_OBJECT:
            case AOTExprKind::SCOPED_NEW_OBJECT:
            case AOTExprKind::RUNTIME_CONST_REF:
            case AOTExprKind::LOCAL_VARREF:
            case AOTExprKind::CONST_NUMBER:
            case AOTExprKind::CONST_BINARY:
            case AOTExprKind::SELF_VARREF:
                ref1 = reader.readStringRef(ptr);
                break;
            case AOTExprKind::SELF_METHOD_CALL:
            case AOTExprKind::STATIC_METHOD_CALL:
            case AOTExprKind::STATIC_VARREF:
                ref1 = reader.readStringRef(ptr);
                ref2 = reader.readStringRef(ptr);
                break;
            case AOTExprKind::EXPR_TREE: {
                // Read inline blob: u32 length + raw bytes
                uint32_t blob_size = QoreAOTBinaryReader::readU32(ptr);
                const uint8_t* blob_data = ptr;
                ptr += blob_size;
                // Deserialize the expression tree
                ExprTreeDeserializer deser(blob_data, blob_size, pgm, ctx);
                uint64_t bits = deser.deserialize();
                if (bits) {
                    ctx->exprs[i] = bits;
                } else {
                    printd(2, "AOT v2: EXPR_TREE deserialization failed for expr slot %d of '%s'\n",
                        i, name);
                    has_unsupported = true;
                }
                continue;
            }
            case AOTExprKind::CLOSURE_CREATE:
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
                VarRefNode* vrn = new VarRefNode(&loc_builtin, strdup(ctx->locals[local_slot]->getName()),
                    ctx->locals[local_slot], false);
                ctx->exprs[i] = toBitsNB(QoreValue(vrn));
                continue;
            } else {
                printd(0, "AOT v2: invalid local slot %d for LOCAL_VARREF expr slot %d (num_locals=%d)\n",
                    local_slot, i, ctx->num_locals);
            }
        }

        uint64_t bits = resolveExprSlot(kind, ref1, ref2, pgm);
        if (bits) {
            ctx->exprs[i] = bits;
        } else if (kind != AOTExprKind::GENERIC_EVAL) {
            printd(2, "AOT v2: unresolved expr slot %d (kind=%d) for '%s'\n",
                i, (int)kind, name);
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
        std::unordered_map<std::string, LocalVar*> local_name_map;
        for (int i = 0; i < num_locals; ++i) {
            if (ctx->locals[i]) {
                local_name_map[ctx->locals[i]->getName()] = ctx->locals[i];
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
                if (it != local_name_map.end()) {
                    ctx->all_body_locals.push_back(it->second);
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

    // Resolve stmt_slots from the function's AST (on_block_exit handlers + reference foreach)
    if (num_stmts > 0 && uvb) {
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
                    ctx->stmts[i] = unique_stmts[i];
                }
            } else {
                printd(0, "AOT v2: stmt_slots count mismatch for '%s': expected %d, found %d from AST\n",
                    name, num_stmts, (int)unique_stmts.size());
                has_unsupported = true;
            }
        } else {
            // Variant has no statement block (deserialized without body) — cannot resolve
            // stmt_slots. Must fall through to fallback path for consistent LocalVar* pointers.
            printd(2, "AOT v2: '%s' has stmt_slots but no statement block, needs fallback\n", name);
            has_unsupported = true;
        }
    } else if (num_stmts > 0 && !uvb) {
        // Toplevel with on_block_exit/foreach — not supported in slot map path
        has_unsupported = true;
    }

    printd(2, "AOT v2: built context from slot map for '%s' "
        "(locals=%d, globals=%d, exprs=%d, stmts=%d, body_locals=%d, unsupported=%d)\n",
        name, num_locals, num_globals, num_exprs, num_stmts, num_body_locals, has_unsupported);

    // If any expression slots have unsupported types (e.g., closures), skip AOT
    // registration for this function — it will fall through to JIT at runtime
    if (has_unsupported) {
        printd(2, "AOT v2: skipping '%s' due to unsupported expression slots\n", name);
        delete ctx;
        return nullptr;
    }

    return ctx;
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
    QoreAOTContext* ctx = buildAOTContext(*ir_func, aot_func.num_locals, aot_func.num_globals, aot_func.num_exprs, aot_func.num_stmts);
    delete ir_func;

    return ctx;
}

//! Register AOT functions using slot maps from deserialized metadata (V2 — no IR re-lowering)
/** Walks the SLOT_MAPS section, finds matching functions in the namespace tree,
    and builds context from slot identities.
*/
static void registerAOTFunctionsFromSlotMaps(
        const QoreAOTBinaryReader& reader,
        qore_ns_private* root_ns,
        QoreProgram* pgm,
        std::unordered_map<std::string, const QoreAOTFunc*>& func_map,
        int& registered) {
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

    for (uint32_t f = 0; f < num_funcs; ++f) {
        // Peek at function name (first field in slot map entry)
        const uint8_t* entry_start = ptr;
        const char* func_name = reader.readStringRef(ptr);
        // Reset to entry start for buildContextFromSlotMap which reads the full entry
        ptr = entry_start;

        if (!func_name || !*func_name) {
            // Skip this entry by reading through it
            printd(2, "AOT v2: skipping unnamed slot map entry\n");
            // Need to skip the entry — read header + all slots
            reader.readStringRef(ptr); // name
            uint16_t nl = QoreAOTBinaryReader::readU16(ptr);
            uint16_t ng = QoreAOTBinaryReader::readU16(ptr);
            uint16_t ne = QoreAOTBinaryReader::readU16(ptr);
            QoreAOTBinaryReader::readU16(ptr); // num_stmts
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
                    case AOTExprKind::FUNC_CALL:
                    case AOTExprKind::NEW_OBJECT:
                    case AOTExprKind::SCOPED_NEW_OBJECT:
                    case AOTExprKind::RUNTIME_CONST_REF:
                    case AOTExprKind::LOCAL_VARREF:
                    case AOTExprKind::CONST_NUMBER:
                    case AOTExprKind::CONST_BINARY:
                    case AOTExprKind::SELF_VARREF:
                        reader.readStringRef(ptr);
                        break;
                    case AOTExprKind::SELF_METHOD_CALL:
                    case AOTExprKind::STATIC_METHOD_CALL:
                    case AOTExprKind::STATIC_VARREF:
                        reader.readStringRef(ptr);
                        reader.readStringRef(ptr);
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
            continue;
        }

        // Find matching AOT function
        auto it = func_map.find(func_name);
        if (it == func_map.end()) {
            // No AOT function for this entry — skip it
            // Same skip logic as above
            reader.readStringRef(ptr); // name
            uint16_t nl = QoreAOTBinaryReader::readU16(ptr);
            uint16_t ng = QoreAOTBinaryReader::readU16(ptr);
            uint16_t ne = QoreAOTBinaryReader::readU16(ptr);
            QoreAOTBinaryReader::readU16(ptr); // num_stmts
            uint16_t nbl = QoreAOTBinaryReader::readU16(ptr);
            QoreAOTBinaryReader::readU8(ptr);
            QoreAOTBinaryReader::readU8(ptr);
            for (int i = 0; i < nl; ++i) {
                reader.readStringRef(ptr); reader.readStringRef(ptr);
                QoreAOTBinaryReader::readU8(ptr); QoreAOTBinaryReader::readU16(ptr);
            }
            for (int i = 0; i < ng; ++i) {
                reader.readStringRef(ptr); reader.readStringRef(ptr);
                QoreAOTBinaryReader::readU8(ptr);
            }
            for (int i = 0; i < ne; ++i) {
                uint8_t kind = QoreAOTBinaryReader::readU8(ptr);
                switch (static_cast<AOTExprKind>(kind)) {
                    case AOTExprKind::FUNC_CALL:
                    case AOTExprKind::NEW_OBJECT:
                    case AOTExprKind::SCOPED_NEW_OBJECT:
                    case AOTExprKind::RUNTIME_CONST_REF:
                    case AOTExprKind::LOCAL_VARREF:
                    case AOTExprKind::CONST_NUMBER:
                    case AOTExprKind::CONST_BINARY:
                    case AOTExprKind::SELF_VARREF:
                        reader.readStringRef(ptr); break;
                    case AOTExprKind::SELF_METHOD_CALL:
                    case AOTExprKind::STATIC_METHOD_CALL:
                    case AOTExprKind::STATIC_VARREF:
                        reader.readStringRef(ptr); reader.readStringRef(ptr); break;
                    case AOTExprKind::EXPR_TREE: {
                        uint32_t blob_size = QoreAOTBinaryReader::readU32(ptr);
                        ptr += blob_size;
                        break;
                    }
                    default: break;
                }
            }
            for (int i = 0; i < nbl; ++i) {
                reader.readStringRef(ptr); reader.readStringRef(ptr);
                QoreAOTBinaryReader::readU8(ptr);
            }
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

            qore_program_private* pp = qore_program_private::get(*pgm);
            const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
                *pp->RootNS, class_name.c_str());
            if (qc) {
                // Use parseFindLocalMethod/parseFindLocalStaticMethod instead of
                // findMethod/findStaticMethod — the latter checks committedEmpty()
                // which returns true for deserialized (pending) variants
                qore_class_private* qcp = qore_class_private::get(*const_cast<QoreClass*>(qc));
                const QoreMethod* m = qcp->parseFindLocalMethod(method_name.c_str());
                if (!m) {
                    m = qcp->parseFindLocalStaticMethod(method_name.c_str());
                }
                if (m) {
                    MethodFunctionBase* mfb = qore_method_private::get(*m)->getFunction();
                    QoreFunctionIterator vi(*mfb);
                    while (vi.next()) {
                        uvb = const_cast<UserVariantBase*>(
                            dynamic_cast<const UserVariantBase*>(vi.getVariant()));
                        if (uvb) {
                            break;
                        }
                    }
                }
            }
        } else if (fname_str != "_toplevel") {
            // Regular function
            for (auto fi = root_ns->func_list.begin(), fe2 = root_ns->func_list.end(); fi != fe2; ++fi) {
                FunctionEntry* fe_entry = fi->second;
                QoreFunction* func = fe_entry->getFunction();
                if (!func) {
                    continue;
                }
                if (fname_str == func->getName()) {
                    QoreFunctionIterator vi(*func);
                    while (vi.next()) {
                        uvb = const_cast<UserVariantBase*>(
                            dynamic_cast<const UserVariantBase*>(vi.getVariant()));
                        if (uvb) {
                            break;
                        }
                    }
                    break;
                }
            }
        }

        // Build context from slot map
        QoreAOTContext* ctx = buildContextFromSlotMap(reader, ptr, end, uvb, pgm, *aot_func, func_name);
        if (ctx && uvb) {
            uvb->registerPrecompiledAOTFunction(aot_func->fn_ptr, ctx);
            ++registered;
            // Remove from func_map so fallback path won't re-register with wrong LocalVar*
            func_map.erase(func_name);
            printd(2, "AOT v2: registered '%s' from slot map\n", func_name);
        } else if (ctx) {
            // Toplevel or unresolved — handled separately
            delete ctx;
            printd(2, "AOT v2: built context for '%s' but no variant found\n", func_name);
        } else {
            printd(2, "AOT v2: failed to build slot map context for '%s'\n", func_name);
        }
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
            if (!uvb || !uvb->hasBody()) {
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
                if (!uvb || !uvb->hasBody()) {
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
                        MethodFunctionBase* main_mfb =
                            qore_method_private::get(*main_m)->getFunction();
                        QoreFunctionIterator mvi(*main_mfb);
                        while (mvi.next()) {
                            main_uvb = const_cast<UserVariantBase*>(
                                dynamic_cast<const UserVariantBase*>(mvi.getVariant()));
                            if (main_uvb) {
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
    // Check for -b flag (disable signal handling, useful for valgrind)
    bool init_signals = true;
    int first_arg = 1;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-b") == 0) {
            init_signals = false;
            first_arg = i + 1;
            break;
        }
    }

    // Set up ARGV from command-line arguments before qore_init (matches normal qore binary order)
    qore_setup_argv(first_arg, argc, argv);

    // Initialize the Qore runtime with MIT license
    qore_init(QL_MIT, nullptr, false, init_signals ? QLO_NONE : QLO_DISABLE_SIGNAL_HANDLING);

    int rc = 0;
    {
        ExceptionSink xsink;
        ExceptionSink wsink;

        QoreProgramHelper qpgm(parse_options, xsink);
        if (xsink.isException()) {
            xsink.handleExceptions();
            qore_cleanup();
            return 2;
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
                                toplevel_func->num_exprs, toplevel_func->num_stmts);
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
    }

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
    // Check for -b flag (disable signal handling, useful for valgrind)
    bool init_signals = true;
    int first_arg = 1;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-b") == 0) {
            init_signals = false;
            first_arg = i + 1;
            break;
        }
    }

    // Set up ARGV from command-line arguments
    qore_setup_argv(first_arg, argc, argv);

    // Initialize the Qore runtime
    qore_init(QL_MIT, nullptr, false, init_signals ? QLO_NONE : QLO_DISABLE_SIGNAL_HANDLING);

    int rc = 0;
    {
        ExceptionSink xsink;

        QoreProgramHelper qpgm(parse_options, xsink);
        if (xsink.isException()) {
            xsink.handleExceptions();
            qore_cleanup();
            return 2;
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
        {
            ProgramRuntimeParseContextHelper pch(&xsink, *qpgm);
            if (xsink.isException()) {
                xsink.handleExceptions();
                qore_cleanup();
                return 2;
            }
            if (!deserializer.deserializeIntoProgram(*qpgm,
                    metadata, static_cast<uint32_t>(metadata_len), deser_error)) {
                fprintf(stderr, "AOT: metadata deserialization failed: %s\n", deser_error.c_str());
                qore_cleanup();
                return 2;
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
                }
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
                            const char* entry_name = deserializer.getReader().readStringRef(sm_ptr);
                            sm_ptr = entry_start;  // reset for buildContextFromSlotMap

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
                                deserializer.getReader().readStringRef(sm_ptr);
                                uint16_t nl = QoreAOTBinaryReader::readU16(sm_ptr);
                                uint16_t ng = QoreAOTBinaryReader::readU16(sm_ptr);
                                uint16_t ne = QoreAOTBinaryReader::readU16(sm_ptr);
                                QoreAOTBinaryReader::readU16(sm_ptr); // num_stmts
                                uint16_t nbl = QoreAOTBinaryReader::readU16(sm_ptr);
                                QoreAOTBinaryReader::readU8(sm_ptr);
                                QoreAOTBinaryReader::readU8(sm_ptr);
                                for (int i = 0; i < nl; ++i) {
                                    deserializer.getReader().readStringRef(sm_ptr);
                                    deserializer.getReader().readStringRef(sm_ptr);
                                    QoreAOTBinaryReader::readU8(sm_ptr);
                                    QoreAOTBinaryReader::readU16(sm_ptr);
                                }
                                for (int i = 0; i < ng; ++i) {
                                    deserializer.getReader().readStringRef(sm_ptr);
                                    deserializer.getReader().readStringRef(sm_ptr);
                                    QoreAOTBinaryReader::readU8(sm_ptr);
                                }
                                for (int i = 0; i < ne; ++i) {
                                    uint8_t kind = QoreAOTBinaryReader::readU8(sm_ptr);
                                    switch (static_cast<AOTExprKind>(kind)) {
                                        case AOTExprKind::FUNC_CALL:
                                        case AOTExprKind::NEW_OBJECT:
                                        case AOTExprKind::SCOPED_NEW_OBJECT:
                                        case AOTExprKind::RUNTIME_CONST_REF:
                                        case AOTExprKind::LOCAL_VARREF:
                                        case AOTExprKind::CONST_NUMBER:
                                        case AOTExprKind::CONST_BINARY:
                                        case AOTExprKind::SELF_VARREF:
                                            deserializer.getReader().readStringRef(sm_ptr);
                                            break;
                                        case AOTExprKind::SELF_METHOD_CALL:
                                        case AOTExprKind::STATIC_METHOD_CALL:
                                        case AOTExprKind::STATIC_VARREF:
                                            deserializer.getReader().readStringRef(sm_ptr);
                                            deserializer.getReader().readStringRef(sm_ptr);
                                            break;
                                        case AOTExprKind::EXPR_TREE: {
                                            uint32_t blob_size = QoreAOTBinaryReader::readU32(sm_ptr);
                                            sm_ptr += blob_size;
                                            break;
                                        }
                                        default: break;
                                    }
                                }
                                for (int i = 0; i < nbl; ++i) {
                                    deserializer.getReader().readStringRef(sm_ptr);
                                    deserializer.getReader().readStringRef(sm_ptr);
                                    QoreAOTBinaryReader::readU8(sm_ptr);
                                }
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
                                toplevel_func->num_exprs, toplevel_func->num_stmts);
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
        }

        // NOTE: Do NOT clean up fallback_pgm yet - it may contain StatementBlock* pointers
        // that are referenced by AOT contexts for on_exit/on_success/on_error blocks.
        // We must keep it alive until after the program finishes running.

        // Run the program
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
    }

    qore_cleanup();
    return rc;
}

// ---- AOT Module Runtime Functions ----

//! Per-module state for AOT-compiled modules
struct AotModuleState {
    QoreProgram* pgm = nullptr;
    const QoreAOTFunc* funcs = nullptr;
    int num_funcs = 0;
    //! Modules that should be reexported (from %requires(reexport) directives)
    std::vector<std::string> reexport_deps;
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
    if (!deserializer.deserializeIntoProgram(local_pgm,
            metadata, static_cast<uint32_t>(metadata_len), deser_error)) {
        QoreStringNode* err = new QoreStringNode("AOT module metadata deserialization error: ");
        err->concat(deser_error.c_str());
        local_pgm->waitForTerminationAndDeref(nullptr);
        return err;
    }

    // Register pre-compiled AOT functions
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
