/* -*- indent-tabs-mode: nil -*- */
/*
    QoreIRBuilder.h

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

#ifndef _QORE_INTERN_QOREIRBUILDER_H
#define _QORE_INTERN_QOREIRBUILDER_H

#include <qore/intern/QoreIR.h>

class QoreIRBuilder {
public:
    explicit QoreIRBuilder(QoreIRFunction* func = nullptr);

    void setFunction(QoreIRFunction* func);
    void setBlock(QoreIRBasicBlock* block);
    QoreIRFunction* getFunction() const;
    QoreIRBasicBlock* getBlock() const;
    QoreIRBasicBlock* createBlock(const std::string& name);

    QoreIRConstInstruction* createConstInt(int64_t value, const QoreProgramLocation* loc = nullptr);
    QoreIRConstInstruction* createConstFloat(double value, const QoreProgramLocation* loc = nullptr);
    QoreIRConstInstruction* createConstBool(bool value, const QoreProgramLocation* loc = nullptr);
    QoreIRConstInstruction* createConstNothing(const QoreProgramLocation* loc = nullptr);
    QoreIRConstInstruction* createConstNull(const QoreProgramLocation* loc = nullptr);
    QoreIRConstInstruction* createConstString(const std::string& value, const QoreProgramLocation* loc = nullptr);
    QoreIRConstInstruction* createConstDate(int64_t microseconds, bool is_relative,
        const QoreProgramLocation* loc = nullptr);
    QoreIRInstruction* createMakeList(const std::vector<QoreIRValue>& values,
        const QoreProgramLocation* loc = nullptr);
    QoreIRInstruction* createMakeHash(const std::vector<QoreIRValue>& values,
        const QoreProgramLocation* loc = nullptr);
    QoreIRInstruction* createEmptyList(const QoreProgramLocation* loc = nullptr);
    QoreIRInstruction* createListAppend(QoreIRValue list, QoreIRValue value,
        const QoreProgramLocation* loc = nullptr);

    QoreIRInstruction* createBinaryOp(QoreIROpcode op, QoreIRValue lhs, QoreIRValue rhs,
        const QoreProgramLocation* loc = nullptr);
    QoreIRInstruction* createTernaryOp(QoreIROpcode op, QoreIRValue first, QoreIRValue second, QoreIRValue third,
        const QoreProgramLocation* loc = nullptr);
    QoreIRInstruction* createQuaternaryOp(QoreIROpcode op, QoreIRValue first, QoreIRValue second, QoreIRValue third,
        QoreIRValue fourth, const QoreProgramLocation* loc = nullptr);
    QoreIRInstruction* createUnaryOp(QoreIROpcode op, QoreIRValue value, const QoreProgramLocation* loc = nullptr);
    QoreIRLocalInstruction* createLoadLocal(LocalVar* local, const QoreProgramLocation* loc = nullptr);
    QoreIRLocalInstruction* createStoreLocal(LocalVar* local, QoreIRValue value,
        const QoreProgramLocation* loc = nullptr);
    QoreIRLocalInstruction* createUninstantiateLocal(LocalVar* local, const QoreProgramLocation* loc = nullptr);
    QoreIRLocalInstruction* createLoadClosure(LocalVar* local, const QoreProgramLocation* loc = nullptr);
    QoreIRLocalInstruction* createStoreClosure(LocalVar* local, QoreIRValue value,
        const QoreProgramLocation* loc = nullptr);
    QoreIRVarInstruction* createLoadGlobal(Var* var, const QoreProgramLocation* loc = nullptr);
    QoreIRVarInstruction* createStoreGlobal(Var* var, QoreIRValue value, const QoreProgramLocation* loc = nullptr);
    QoreIRVarInstruction* createLoadThreadLocal(Var* var, const QoreProgramLocation* loc = nullptr);
    QoreIRVarInstruction* createStoreThreadLocal(Var* var, QoreIRValue value,
        const QoreProgramLocation* loc = nullptr);
    // Self member access
    QoreIRSelfMemberInstruction* createLoadSelfMember(const char* member_name,
        const QoreProgramLocation* loc = nullptr);
    // Static class variable access
    QoreIRStaticVarInstruction* createLoadStaticVar(QoreVarInfo* vi, const char* var_name,
        const QoreValue& expr, const QoreProgramLocation* loc = nullptr);
    // Object instantiation
    QoreIRNewObjectInstruction* createNewObject(const QoreClass* qc,
        const AbstractQoreFunctionVariant* variant, const QoreListNode* args,
        const QoreValue& expr, const QoreProgramLocation* loc = nullptr);
    // Constant loading
    QoreIRLoadConstantInstruction* createLoadConstant(const RuntimeConstantRefNode* node,
        const QoreValue& expr, const QoreProgramLocation* loc = nullptr);
    // Closure creation
    QoreIRCreateClosureInstruction* createCreateClosure(const QoreClosureParseNode* closure_node,
        const QoreValue& expr, const QoreProgramLocation* loc = nullptr);
    // Call reference creation
    QoreIRCreateCallRefInstruction* createCreateCallRef(const QoreValue& expr,
        const QoreProgramLocation* loc = nullptr);
    // Method reference creation
    QoreIRCreateMethodRefInstruction* createCreateMethodRef(const QoreValue& expr,
        const QoreProgramLocation* loc = nullptr);
    // Parse reference creation
    QoreIRCreateParseRefInstruction* createCreateParseRef(const ParseReferenceNode* node,
        const QoreValue& expr, const QoreProgramLocation* loc = nullptr);
    // Typed container construction
    QoreIRNewHashDeclInstruction* createNewHashDecl(const NewHashDeclNode* node,
        const QoreValue& expr, const QoreProgramLocation* loc = nullptr);
    QoreIRNewComplexHashInstruction* createNewComplexHash(const NewComplexHashNode* node,
        const QoreValue& expr, const QoreProgramLocation* loc = nullptr);
    QoreIRNewComplexListInstruction* createNewComplexList(const NewComplexListNode* node,
        const QoreValue& expr, const QoreProgramLocation* loc = nullptr);
    // VarRefNewObjectNode construction (non-object types)
    QoreIRVrnConstructInstruction* createVrnConstruct(const VarRefNewObjectNode* vrn,
        const QoreValue& expr, const QoreProgramLocation* loc = nullptr);
    // Hash building (for hash map loops)
    QoreIRInstruction* createHashSetKeyValue(QoreIRValue hash, QoreIRValue key, QoreIRValue value,
        const QoreProgramLocation* loc = nullptr);
    // Reverse iterator creation (for foldr)
    QoreIRInstruction* createIteratorCreateReverse(QoreIRValue iterable,
        const QoreProgramLocation* loc = nullptr);
    // Implicit argument opcodes
    QoreIRInstruction* createLoadImplicitArg(int offset, const QoreProgramLocation* loc = nullptr);
    QoreIRInstruction* createLoadImplicitArgv(const QoreProgramLocation* loc = nullptr);
    QoreIRInstruction* createLoadImplicitElement(const QoreProgramLocation* loc = nullptr);
    // Context setup/teardown for functional operators
    QoreIRInstruction* createPushImplicitArg(QoreIRValue value, const QoreProgramLocation* loc = nullptr);
    QoreIRInstruction* createSetImplicitArgv(QoreIRValue argv_list, const QoreProgramLocation* loc = nullptr);
    QoreIRInstruction* createPopImplicitArg(QoreIRValue old_context, const QoreProgramLocation* loc = nullptr);
    QoreIRInstruction* createPushImplicitElement(QoreIRValue index, const QoreProgramLocation* loc = nullptr);
    QoreIRInstruction* createPopImplicitElement(QoreIRValue old_element, const QoreProgramLocation* loc = nullptr);
    QoreIRLValueInstruction* createLoadLValue(const QoreValue& lvalue, const QoreProgramLocation* loc = nullptr);
    QoreIRLValueInstruction* createStoreLValue(const QoreValue& lvalue, QoreIRValue value,
        const QoreProgramLocation* loc = nullptr);
    QoreIRLValueInstruction* createLValueUnaryOp(QoreIROpcode op, const QoreValue& lvalue,
        const QoreProgramLocation* loc = nullptr);
    QoreIRLValueInstruction* createLValueBinaryOp(QoreIROpcode op, const QoreValue& lvalue, QoreIRValue rhs,
        const QoreProgramLocation* loc = nullptr);
    QoreIRLValueInstruction* createLValueTernaryOp(QoreIROpcode op, const QoreValue& lvalue, QoreIRValue first,
        QoreIRValue second, QoreIRValue third, const QoreProgramLocation* loc = nullptr);
    QoreIRExprInstruction* createExprOp(QoreIROpcode op, const QoreValue& expr,
        const std::vector<QoreIRValue>& operands, const QoreProgramLocation* loc = nullptr);
    QoreIRCallDirectInstruction* createCallDirect(const QoreFunction* qf,
        const AbstractQoreFunctionVariant* variant, QoreProgram* pgm, const QoreValue& expr,
        const std::vector<QoreIRValue>& args, const QoreProgramLocation* loc = nullptr);
    QoreIRCallMethodDirectInstruction* createCallMethodDirect(const QoreMethod* method, const QoreClass* qc,
        const std::vector<QoreIRValue>& args, const QoreProgramLocation* loc = nullptr);
    QoreIRInvokeMethodDirectInstruction* createInvokeMethodDirect(const QoreMethod* method, const QoreClass* qc,
        const std::vector<QoreIRValue>& args, QoreIRBasicBlock* normal_target, QoreIRBasicBlock* exception_target,
        const QoreProgramLocation* loc = nullptr);
    QoreIRInvokeInstruction* createInvoke(const QoreValue& expr, const std::vector<QoreIRValue>& operands,
        QoreIRBasicBlock* normal_target, QoreIRBasicBlock* exception_target,
        const QoreProgramLocation* loc = nullptr);
    QoreIRPhiInstruction* createPhi(const std::vector<QoreIRPhiIncoming>& incoming,
        const QoreProgramLocation* loc = nullptr);

    QoreIRBranchInstruction* createBranch(QoreIRBasicBlock* target, const QoreProgramLocation* loc = nullptr);
    QoreIRBranchIfInstruction* createBranchIf(QoreIRValue cond, QoreIRBasicBlock* true_target,
        QoreIRBasicBlock* false_target, const QoreProgramLocation* loc = nullptr);
    QoreIRSwitchIntInstruction* createSwitchInt(QoreIRValue switch_val, QoreIRBasicBlock* default_target,
        const std::vector<QoreIRSwitchCase>& cases, const QoreProgramLocation* loc = nullptr);
    QoreIRSwitchStringInstruction* createSwitchString(QoreIRValue switch_val, QoreIRBasicBlock* default_target,
        const std::vector<QoreIRSwitchStringCase>& cases, const QoreProgramLocation* loc = nullptr);

    QoreIRReturnInstruction* createReturn(QoreIRValue value, const QoreProgramLocation* loc = nullptr);
    QoreIRReturnInstruction* createReturnNothing(const QoreProgramLocation* loc = nullptr);
    QoreIRThrowInstruction* createThrow(QoreIRValue value, QoreIRBasicBlock* exception_target,
        const QoreProgramLocation* loc = nullptr);
    QoreIRInstruction* createRethrow(const QoreProgramLocation* loc = nullptr);
    QoreIRInstruction* createThreadExit(const QoreProgramLocation* loc = nullptr);
    QoreIRInstruction* createLandingPad(const QoreProgramLocation* loc = nullptr);
    QoreIRInstruction* createCatchException(const QoreProgramLocation* loc = nullptr);
    QoreIRGuardInstruction* createGuardInt(QoreIRValue value, QoreIRBasicBlock* exception_target,
        const QoreProgramLocation* loc = nullptr);
    QoreIRGuardInstruction* createGuardFloat(QoreIRValue value, QoreIRBasicBlock* exception_target,
        const QoreProgramLocation* loc = nullptr);
    QoreIRGuardInstruction* createGuardType(QoreIRValue value, const QoreTypeInfo* type,
        QoreIRBasicBlock* exception_target, const QoreProgramLocation* loc = nullptr);
    QoreIRGuardInstruction* createGuardNotNothing(QoreIRValue value, QoreIRBasicBlock* exception_target,
        const QoreProgramLocation* loc = nullptr);
    QoreIRForeachInstruction* createForeach(const ForEachStatement* stmt, const QoreProgramLocation* loc = nullptr);
    QoreIRIteratorCreateInstruction* createIteratorCreate(QoreIRValue iterable,
        FunctionalOperator* iterator_func = nullptr, const QoreProgramLocation* loc = nullptr);
    QoreIRIteratorNextInstruction* createIteratorNext(QoreIRValue iterator, QoreIRBasicBlock* done_target,
        QoreIRBasicBlock* continue_target, const QoreProgramLocation* loc = nullptr);
    QoreIROnBlockExitInstruction* createOnBlockExit(const OnBlockExitStatement* stmt,
        const QoreProgramLocation* loc = nullptr);
    QoreIRScopeEnterInstruction* createScopeEnter(uint32_t scope_id, const QoreProgramLocation* loc = nullptr);
    QoreIRScopeExitInstruction* createScopeExit(uint32_t scope_id, bool is_error = false,
        const QoreProgramLocation* loc = nullptr);
    QoreIRDebugInstruction* createDebug(const DebugStatement* stmt, const QoreProgramLocation* loc = nullptr);
    QoreIRAssertInstruction* createAssert(const AssertStatement* stmt, const QoreProgramLocation* loc = nullptr);
    QoreIRContextInstruction* createContext(const ContextStatement* stmt, const QoreProgramLocation* loc = nullptr);
    QoreIRSummarizeInstruction* createSummarize(const SummarizeStatement* stmt,
        const QoreProgramLocation* loc = nullptr);
    QoreIRSwitchRegexMatchInstruction* createSwitchRegexMatch(const CaseNodeRegex* regex_case,
        QoreIRValue switch_val, const QoreProgramLocation* loc = nullptr);

private:
    QoreIRFunction* func = nullptr;
    QoreIRBasicBlock* block = nullptr;
};

#endif
