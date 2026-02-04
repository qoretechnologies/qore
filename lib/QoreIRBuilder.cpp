/* -*- indent-tabs-mode: nil -*- */
/*
    QoreIRBuilder.cpp

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

#include <qore/intern/QoreIRBuilder.h>

QoreIRBuilder::QoreIRBuilder(QoreIRFunction* n_func) : func(n_func) {
}

void QoreIRBuilder::setFunction(QoreIRFunction* n_func) {
    func = n_func;
    block = nullptr;
}

void QoreIRBuilder::setBlock(QoreIRBasicBlock* n_block) {
    block = n_block;
}

QoreIRFunction* QoreIRBuilder::getFunction() const {
    return func;
}

QoreIRBasicBlock* QoreIRBuilder::getBlock() const {
    return block;
}

QoreIRBasicBlock* QoreIRBuilder::createBlock(const std::string& name) {
    if (!func) {
        return nullptr;
    }
    return func->createBlock(name);
}

QoreIRConstInstruction* QoreIRBuilder::createConstInt(int64_t value, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRConstInstruction>();
    inst->opcode = QoreIROpcode::ConstInt;
    inst->loc = loc;
    inst->result = func->createValue();
    inst->constant.kind = QoreIRConstant::Kind::Int;
    inst->constant.int_value = value;
    return inst;
}

QoreIRConstInstruction* QoreIRBuilder::createConstFloat(double value, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRConstInstruction>();
    inst->opcode = QoreIROpcode::ConstFloat;
    inst->loc = loc;
    inst->result = func->createValue();
    inst->constant.kind = QoreIRConstant::Kind::Float;
    inst->constant.float_value = value;
    return inst;
}

QoreIRConstInstruction* QoreIRBuilder::createConstBool(bool value, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRConstInstruction>();
    inst->opcode = QoreIROpcode::ConstBool;
    inst->loc = loc;
    inst->result = func->createValue();
    inst->constant.kind = QoreIRConstant::Kind::Bool;
    inst->constant.bool_value = value;
    return inst;
}

QoreIRConstInstruction* QoreIRBuilder::createConstNothing(const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRConstInstruction>();
    inst->opcode = QoreIROpcode::ConstNothing;
    inst->loc = loc;
    inst->result = func->createValue();
    inst->constant.kind = QoreIRConstant::Kind::Nothing;
    return inst;
}

QoreIRConstInstruction* QoreIRBuilder::createConstNull(const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRConstInstruction>();
    inst->opcode = QoreIROpcode::ConstNull;
    inst->loc = loc;
    inst->result = func->createValue();
    inst->constant.kind = QoreIRConstant::Kind::Null;
    return inst;
}

QoreIRConstInstruction* QoreIRBuilder::createConstString(const std::string& value,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRConstInstruction>();
    inst->opcode = QoreIROpcode::ConstString;
    inst->loc = loc;
    inst->result = func->createValue();
    inst->constant.kind = QoreIRConstant::Kind::String;
    inst->constant.string_value = value;
    return inst;
}

QoreIRConstInstruction* QoreIRBuilder::createConstDate(int64_t microseconds, bool is_relative,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRConstInstruction>();
    inst->opcode = QoreIROpcode::ConstDate;
    inst->loc = loc;
    inst->result = func->createValue();
    inst->constant.kind = QoreIRConstant::Kind::Date;
    inst->constant.date_microseconds = microseconds;
    inst->constant.date_is_relative = is_relative;
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createMakeList(const std::vector<QoreIRValue>& values,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::MakeList);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->operands = values;
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createMakeHash(const std::vector<QoreIRValue>& values,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::MakeHash);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->operands = values;
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createBinaryOp(QoreIROpcode op, QoreIRValue lhs, QoreIRValue rhs,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(op);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->operands.push_back(lhs);
    inst->operands.push_back(rhs);
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createTernaryOp(QoreIROpcode op, QoreIRValue first, QoreIRValue second,
        QoreIRValue third, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(op);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->operands.push_back(first);
    inst->operands.push_back(second);
    inst->operands.push_back(third);
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createQuaternaryOp(QoreIROpcode op, QoreIRValue first, QoreIRValue second,
        QoreIRValue third, QoreIRValue fourth, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(op);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->operands.push_back(first);
    inst->operands.push_back(second);
    inst->operands.push_back(third);
    inst->operands.push_back(fourth);
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createUnaryOp(QoreIROpcode op, QoreIRValue value,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(op);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->operands.push_back(value);
    return inst;
}

QoreIRLocalInstruction* QoreIRBuilder::createLoadLocal(LocalVar* local, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRLocalInstruction>(QoreIROpcode::LoadLocal, local);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRLocalInstruction* QoreIRBuilder::createStoreLocal(LocalVar* local, QoreIRValue value,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRLocalInstruction>(QoreIROpcode::StoreLocal, local);
    inst->loc = loc;
    inst->operands.push_back(value);
    return inst;
}

QoreIRLocalInstruction* QoreIRBuilder::createLoadClosure(LocalVar* local, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRLocalInstruction>(QoreIROpcode::LoadClosure, local);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRLocalInstruction* QoreIRBuilder::createStoreClosure(LocalVar* local, QoreIRValue value,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRLocalInstruction>(QoreIROpcode::StoreClosure, local);
    inst->loc = loc;
    inst->operands.push_back(value);
    return inst;
}

QoreIRVarInstruction* QoreIRBuilder::createLoadGlobal(Var* var, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRVarInstruction>(QoreIROpcode::LoadGlobal, var);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRVarInstruction* QoreIRBuilder::createStoreGlobal(Var* var, QoreIRValue value,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRVarInstruction>(QoreIROpcode::StoreGlobal, var);
    inst->loc = loc;
    inst->operands.push_back(value);
    return inst;
}

QoreIRVarInstruction* QoreIRBuilder::createLoadThreadLocal(Var* var, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRVarInstruction>(QoreIROpcode::LoadThreadLocal, var);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRVarInstruction* QoreIRBuilder::createStoreThreadLocal(Var* var, QoreIRValue value,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRVarInstruction>(QoreIROpcode::StoreThreadLocal, var);
    inst->loc = loc;
    inst->operands.push_back(value);
    return inst;
}

QoreIRLValueInstruction* QoreIRBuilder::createLoadLValue(const QoreValue& lvalue, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRLValueInstruction>(QoreIROpcode::LoadLValue, lvalue);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRLValueInstruction* QoreIRBuilder::createStoreLValue(const QoreValue& lvalue, QoreIRValue value,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRLValueInstruction>(QoreIROpcode::StoreLValue, lvalue);
    inst->loc = loc;
    inst->operands.push_back(value);
    return inst;
}

QoreIRLValueInstruction* QoreIRBuilder::createLValueUnaryOp(QoreIROpcode op, const QoreValue& lvalue,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRLValueInstruction>(op, lvalue);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRLValueInstruction* QoreIRBuilder::createLValueBinaryOp(QoreIROpcode op, const QoreValue& lvalue,
        QoreIRValue rhs, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRLValueInstruction>(op, lvalue);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->operands.push_back(rhs);
    return inst;
}

QoreIRLValueInstruction* QoreIRBuilder::createLValueTernaryOp(QoreIROpcode op, const QoreValue& lvalue,
        QoreIRValue first, QoreIRValue second, QoreIRValue third, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRLValueInstruction>(op, lvalue);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->operands.push_back(first);
    inst->operands.push_back(second);
    inst->operands.push_back(third);
    return inst;
}

QoreIRExprInstruction* QoreIRBuilder::createExprOp(QoreIROpcode op, const QoreValue& expr,
        const std::vector<QoreIRValue>& operands, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRExprInstruction>(op, expr);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->operands = operands;
    return inst;
}

QoreIRInvokeInstruction* QoreIRBuilder::createInvoke(const QoreValue& expr, const std::vector<QoreIRValue>& operands,
        QoreIRBasicBlock* normal_target, QoreIRBasicBlock* exception_target, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInvokeInstruction>(expr, normal_target, exception_target);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->operands = operands;
    return inst;
}

QoreIRPhiInstruction* QoreIRBuilder::createPhi(const std::vector<QoreIRPhiIncoming>& incoming,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRPhiInstruction>();
    inst->loc = loc;
    inst->result = func->createValue();
    inst->incoming = incoming;
    for (const auto& inc : incoming) {
        inst->operands.push_back(inc.value);
    }
    return inst;
}

QoreIRBranchInstruction* QoreIRBuilder::createBranch(QoreIRBasicBlock* target,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRBranchInstruction>();
    inst->loc = loc;
    inst->target = target;
    return inst;
}

QoreIRBranchIfInstruction* QoreIRBuilder::createBranchIf(QoreIRValue cond, QoreIRBasicBlock* true_target,
        QoreIRBasicBlock* false_target, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRBranchIfInstruction>();
    inst->loc = loc;
    inst->condition = cond;
    inst->true_target = true_target;
    inst->false_target = false_target;
    return inst;
}

QoreIRSwitchIntInstruction* QoreIRBuilder::createSwitchInt(QoreIRValue switch_val, QoreIRBasicBlock* default_target,
        const std::vector<QoreIRSwitchCase>& cases, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRSwitchIntInstruction>();
    inst->loc = loc;
    inst->switch_val = switch_val;
    inst->default_target = default_target;
    inst->cases = cases;
    return inst;
}

QoreIRSwitchStringInstruction* QoreIRBuilder::createSwitchString(QoreIRValue switch_val,
        QoreIRBasicBlock* default_target, const std::vector<QoreIRSwitchStringCase>& cases,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRSwitchStringInstruction>();
    inst->loc = loc;
    inst->switch_val = switch_val;
    inst->default_target = default_target;
    inst->cases = cases;
    return inst;
}

QoreIRReturnInstruction* QoreIRBuilder::createReturn(QoreIRValue value, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRReturnInstruction>();
    inst->opcode = QoreIROpcode::Return;
    inst->loc = loc;
    inst->has_value = true;
    inst->value = value;
    return inst;
}

QoreIRReturnInstruction* QoreIRBuilder::createReturnNothing(const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRReturnInstruction>();
    inst->opcode = QoreIROpcode::ReturnNothing;
    inst->loc = loc;
    return inst;
}

QoreIRThrowInstruction* QoreIRBuilder::createThrow(QoreIRValue value, QoreIRBasicBlock* exception_target,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRThrowInstruction>();
    inst->loc = loc;
    inst->operands.push_back(value);
    inst->exception_target = exception_target;
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createRethrow(const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::Rethrow);
    inst->loc = loc;
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createThreadExit(const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::ThreadExit);
    inst->loc = loc;
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createLandingPad(const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::LandingPad);
    inst->loc = loc;
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createCatchException(const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::CatchException);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRGuardInstruction* QoreIRBuilder::createGuardInt(QoreIRValue value, QoreIRBasicBlock* exception_target,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRGuardInstruction>(QoreIROpcode::GuardInt);
    inst->loc = loc;
    inst->operands.push_back(value);
    inst->deopt_target = exception_target;
    inst->guard_id = func->num_guards++;
    return inst;
}

QoreIRGuardInstruction* QoreIRBuilder::createGuardFloat(QoreIRValue value, QoreIRBasicBlock* exception_target,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRGuardInstruction>(QoreIROpcode::GuardFloat);
    inst->loc = loc;
    inst->operands.push_back(value);
    inst->deopt_target = exception_target;
    inst->guard_id = func->num_guards++;
    return inst;
}

QoreIRGuardInstruction* QoreIRBuilder::createGuardType(QoreIRValue value, const QoreTypeInfo* type,
        QoreIRBasicBlock* exception_target, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRGuardInstruction>(QoreIROpcode::GuardType);
    inst->loc = loc;
    inst->operands.push_back(value);
    inst->type_info = type;
    inst->deopt_target = exception_target;
    inst->guard_id = func->num_guards++;
    return inst;
}

QoreIRGuardInstruction* QoreIRBuilder::createGuardNotNothing(QoreIRValue value, QoreIRBasicBlock* exception_target,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRGuardInstruction>(QoreIROpcode::GuardNotNothing);
    inst->loc = loc;
    inst->operands.push_back(value);
    inst->deopt_target = exception_target;
    inst->guard_id = func->num_guards++;
    return inst;
}

QoreIRForeachInstruction* QoreIRBuilder::createForeach(const ForEachStatement* stmt, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRForeachInstruction>(stmt);
    inst->loc = loc;
    return inst;
}

QoreIRIteratorCreateInstruction* QoreIRBuilder::createIteratorCreate(QoreIRValue iterable,
        FunctionalOperator* iterator_func, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRIteratorCreateInstruction>(iterable, iterator_func);
    inst->loc = loc;
    return inst;
}

QoreIRIteratorNextInstruction* QoreIRBuilder::createIteratorNext(QoreIRValue iterator, QoreIRBasicBlock* done_target,
        QoreIRBasicBlock* continue_target, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRIteratorNextInstruction>(iterator, done_target, continue_target);
    inst->loc = loc;
    return inst;
}

QoreIROnBlockExitInstruction* QoreIRBuilder::createOnBlockExit(const OnBlockExitStatement* stmt,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIROnBlockExitInstruction>(stmt);
    inst->loc = loc;
    return inst;
}

QoreIRScopeEnterInstruction* QoreIRBuilder::createScopeEnter(uint32_t scope_id, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRScopeEnterInstruction>(scope_id);
    inst->loc = loc;
    return inst;
}

QoreIRScopeExitInstruction* QoreIRBuilder::createScopeExit(uint32_t scope_id, bool is_error,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRScopeExitInstruction>(scope_id, is_error);
    inst->loc = loc;
    return inst;
}

QoreIRDebugInstruction* QoreIRBuilder::createDebug(const DebugStatement* stmt, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRDebugInstruction>(stmt);
    inst->loc = loc;
    return inst;
}

QoreIRAssertInstruction* QoreIRBuilder::createAssert(const AssertStatement* stmt, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRAssertInstruction>(stmt);
    inst->loc = loc;
    return inst;
}

QoreIRContextInstruction* QoreIRBuilder::createContext(const ContextStatement* stmt,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRContextInstruction>(stmt);
    inst->loc = loc;
    return inst;
}

QoreIRSummarizeInstruction* QoreIRBuilder::createSummarize(const SummarizeStatement* stmt,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRSummarizeInstruction>(stmt);
    inst->loc = loc;
    return inst;
}
