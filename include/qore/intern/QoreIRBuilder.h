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
    QoreIRConstInstruction* createConstString(const std::string& value, const QoreProgramLocation* loc = nullptr);
    QoreIRConstInstruction* createConstDate(int64_t microseconds, bool is_relative,
        const QoreProgramLocation* loc = nullptr);
    QoreIRInstruction* createMakeList(const std::vector<QoreIRValue>& values,
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
    QoreIRLocalInstruction* createLoadClosure(LocalVar* local, const QoreProgramLocation* loc = nullptr);
    QoreIRLocalInstruction* createStoreClosure(LocalVar* local, QoreIRValue value,
        const QoreProgramLocation* loc = nullptr);
    QoreIRVarInstruction* createLoadGlobal(Var* var, const QoreProgramLocation* loc = nullptr);
    QoreIRVarInstruction* createStoreGlobal(Var* var, QoreIRValue value, const QoreProgramLocation* loc = nullptr);
    QoreIRVarInstruction* createLoadThreadLocal(Var* var, const QoreProgramLocation* loc = nullptr);
    QoreIRVarInstruction* createStoreThreadLocal(Var* var, QoreIRValue value,
        const QoreProgramLocation* loc = nullptr);
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
    QoreIRInvokeInstruction* createInvoke(const QoreValue& expr, const std::vector<QoreIRValue>& operands,
        QoreIRBasicBlock* normal_target, QoreIRBasicBlock* exception_target,
        const QoreProgramLocation* loc = nullptr);
    QoreIRPhiInstruction* createPhi(const std::vector<QoreIRPhiIncoming>& incoming,
        const QoreProgramLocation* loc = nullptr);

    QoreIRBranchInstruction* createBranch(QoreIRBasicBlock* target, const QoreProgramLocation* loc = nullptr);
    QoreIRBranchIfInstruction* createBranchIf(QoreIRValue cond, QoreIRBasicBlock* true_target,
        QoreIRBasicBlock* false_target, const QoreProgramLocation* loc = nullptr);

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
    QoreIROnBlockExitInstruction* createOnBlockExit(const OnBlockExitStatement* stmt,
        const QoreProgramLocation* loc = nullptr);
    QoreIRDebugInstruction* createDebug(const DebugStatement* stmt, const QoreProgramLocation* loc = nullptr);

private:
    QoreIRFunction* func = nullptr;
    QoreIRBasicBlock* block = nullptr;
};

#endif
