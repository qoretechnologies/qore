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

#include "qore/intern/QoreJITIncludes.h"
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

QoreIRConstInstruction* QoreIRBuilder::createConstEnum(const QoreEnumMember* member,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRConstInstruction>();
    inst->opcode = QoreIROpcode::ConstEnum;
    inst->loc = loc;
    inst->result = func->createValue();
    inst->constant.kind = QoreIRConstant::Kind::Enum;
    inst->constant.enum_member = member;
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createMakeList(const std::vector<QoreIRValue>& values,
        const QoreProgramLocation* loc, const QoreTypeInfo* typeInfo) {
    auto inst = block->appendInstruction<QoreIRMakeListInstruction>();
    inst->loc = loc;
    inst->typeInfo = typeInfo;
    inst->result = func->createValue();
    inst->operands = values;
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createMakeHash(const std::vector<QoreIRValue>& values,
        const QoreProgramLocation* loc, const QoreTypeInfo* typeInfo) {
    auto inst = block->appendInstruction<QoreIRMakeHashInstruction>();
    inst->loc = loc;
    inst->typeInfo = typeInfo;
    inst->result = func->createValue();
    inst->operands = values;
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createMakeHashConstKeys(std::vector<std::string>&& keys,
        const std::vector<QoreIRValue>& values, const QoreProgramLocation* loc,
        const QoreTypeInfo* typeInfo) {
    auto inst = block->appendInstruction<QoreIRMakeHashConstKeysInstruction>(std::move(keys));
    inst->loc = loc;
    inst->typeInfo = typeInfo;
    inst->result = func->createValue();
    inst->operands = values;
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createEmptyList(const QoreProgramLocation* loc,
        const QoreTypeInfo* element_type) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::CreateEmptyList);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->element_type = element_type;
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createSizedList(QoreIRValue capacity, const QoreProgramLocation* loc,
        const QoreTypeInfo* element_type) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::CreateSizedList);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->operands.push_back(capacity);
    inst->element_type = element_type;
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createListAppend(QoreIRValue list, QoreIRValue value,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::ListAppend);
    inst->loc = loc;
    inst->operands.push_back(list);
    inst->operands.push_back(value);
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createListSize(QoreIRValue list, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::ListSize);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->operands.push_back(list);
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createListGetInt(QoreIRValue list, QoreIRValue index,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::ListGetInt);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->operands.push_back(list);
    inst->operands.push_back(index);
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createListGetFloat(QoreIRValue list, QoreIRValue index,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::ListGetFloat);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->operands.push_back(list);
    inst->operands.push_back(index);
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createListGetValue(QoreIRValue list, QoreIRValue index,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::ListGetValue);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->operands.push_back(list);
    inst->operands.push_back(index);
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createListGetValueNoRef(QoreIRValue list, QoreIRValue index,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::ListGetValueNoRef);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->operands.push_back(list);
    inst->operands.push_back(index);
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createListSetInt(QoreIRValue list, QoreIRValue index, QoreIRValue value,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::ListSetInt);
    inst->loc = loc;
    inst->operands.push_back(list);
    inst->operands.push_back(index);
    inst->operands.push_back(value);
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createListSetFloat(QoreIRValue list, QoreIRValue index, QoreIRValue value,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::ListSetFloat);
    inst->loc = loc;
    inst->operands.push_back(list);
    inst->operands.push_back(index);
    inst->operands.push_back(value);
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createListSetValue(QoreIRValue list, QoreIRValue index, QoreIRValue value,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::ListSetValue);
    inst->loc = loc;
    inst->operands.push_back(list);
    inst->operands.push_back(index);
    inst->operands.push_back(value);
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createGetObjectClass(QoreIRValue obj, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::GetObjectClass);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->operands.push_back(obj);
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createListPush(QoreIRValue list, QoreIRValue val,
        const QoreProgramLocation* loc) {
    return createBinaryOp(QoreIROpcode::ListPush, list, val, loc);
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

QoreIRLocalInstruction* QoreIRBuilder::createLoadLocal(LocalVar* local, const QoreProgramLocation* loc, bool auto_ref) {
    auto inst = block->appendInstruction<QoreIRLocalInstruction>(QoreIROpcode::LoadLocal, local, auto_ref);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRLocalInstruction* QoreIRBuilder::createStoreLocal(LocalVar* local, QoreIRValue value,
        const QoreProgramLocation* loc, bool weak) {
    auto inst = block->appendInstruction<QoreIRLocalInstruction>(QoreIROpcode::StoreLocal, local);
    inst->loc = loc;
    inst->operands.push_back(value);
    inst->weak = weak;
    return inst;
}

QoreIRLocalInstruction* QoreIRBuilder::createUninstantiateLocal(LocalVar* local, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRLocalInstruction>(QoreIROpcode::UninstantiateLocal, local);
    inst->loc = loc;
    return inst;
}

QoreIRLocalInstruction* QoreIRBuilder::createInstantiateLocal(LocalVar* local, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRLocalInstruction>(QoreIROpcode::InstantiateLocal, local);
    inst->loc = loc;
    return inst;
}

QoreIRLocalInstruction* QoreIRBuilder::createLoadClosure(LocalVar* local, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRLocalInstruction>(QoreIROpcode::LoadClosure, local);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRLocalInstruction* QoreIRBuilder::createStoreClosure(LocalVar* local, QoreIRValue value,
        const QoreProgramLocation* loc, bool weak) {
    auto inst = block->appendInstruction<QoreIRLocalInstruction>(QoreIROpcode::StoreClosure, local);
    inst->loc = loc;
    inst->operands.push_back(value);
    inst->weak = weak;
    return inst;
}

QoreIRVarInstruction* QoreIRBuilder::createLoadGlobal(Var* var, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRVarInstruction>(QoreIROpcode::LoadGlobal, var);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRVarInstruction* QoreIRBuilder::createStoreGlobal(Var* var, QoreIRValue value,
        const QoreProgramLocation* loc, bool weak) {
    auto inst = block->appendInstruction<QoreIRVarInstruction>(QoreIROpcode::StoreGlobal, var);
    inst->loc = loc;
    inst->operands.push_back(value);
    inst->weak = weak;
    return inst;
}

QoreIRVarInstruction* QoreIRBuilder::createLoadThreadLocal(Var* var, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRVarInstruction>(QoreIROpcode::LoadThreadLocal, var);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRVarInstruction* QoreIRBuilder::createStoreThreadLocal(Var* var, QoreIRValue value,
        const QoreProgramLocation* loc, bool weak) {
    auto inst = block->appendInstruction<QoreIRVarInstruction>(QoreIROpcode::StoreThreadLocal, var);
    inst->loc = loc;
    inst->operands.push_back(value);
    inst->weak = weak;
    return inst;
}

QoreIRHashKeyAccessInstruction* QoreIRBuilder::createHashKeyAccess(const char* key_name,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRHashKeyAccessInstruction>(key_name);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRHashKeyAccessInstruction* QoreIRBuilder::createHashKeyAccessInt(const char* key_name,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRHashKeyAccessInstruction>(key_name,
        QoreIROpcode::HashKeyAccessInt);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRInvokeInstruction* QoreIRBuilder::createInvokeHashKeyAccess(const char* key_name,
        const QoreValue& expr, const std::vector<QoreIRValue>& operands,
        QoreIRBasicBlock* normal_target, QoreIRBasicBlock* exception_target,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInvokeInstruction>(expr, normal_target, exception_target);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->operands = operands;
    inst->invoke_opcode = QoreIROpcode::HashKeyAccess;
    inst->invoke_key_name = key_name;
    return inst;
}

QoreIRMapHashKeyInstruction* QoreIRBuilder::createMapHashKey(QoreIROpcode op, const char* key1,
        const char* key2, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRMapHashKeyInstruction>(op, key1, key2);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRSelfMemberInstruction* QoreIRBuilder::createLoadSelfMember(const char* member_name,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRSelfMemberInstruction>(member_name);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRStaticVarInstruction* QoreIRBuilder::createLoadStaticVar(QoreVarInfo* vi, const char* var_name,
        const QoreValue& expr, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRStaticVarInstruction>(vi, var_name, expr);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRNewObjectInstruction* QoreIRBuilder::createNewObject(const QoreClass* qc,
        const AbstractQoreFunctionVariant* variant, const QoreListNode* args,
        const QoreValue& expr, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRNewObjectInstruction>(qc, variant, args, expr);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRLoadConstantInstruction* QoreIRBuilder::createLoadConstant(const RuntimeConstantRefNode* node,
        const QoreValue& expr, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRLoadConstantInstruction>(node, expr);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRCreateClosureInstruction* QoreIRBuilder::createCreateClosure(
        const QoreClosureParseNode* closure_node,
        const QoreValue& expr, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRCreateClosureInstruction>(closure_node, expr);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRCreateCallRefInstruction* QoreIRBuilder::createCreateCallRef(const QoreValue& expr,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRCreateCallRefInstruction>(expr);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRCreateMethodRefInstruction* QoreIRBuilder::createCreateMethodRef(const QoreValue& expr,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRCreateMethodRefInstruction>(expr);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRCreateParseRefInstruction* QoreIRBuilder::createCreateParseRef(const ParseReferenceNode* node,
        const QoreValue& expr, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRCreateParseRefInstruction>(node, expr);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRNewHashDeclInstruction* QoreIRBuilder::createNewHashDecl(const NewHashDeclNode* node,
        const QoreValue& expr, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRNewHashDeclInstruction>(node, expr);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRNewComplexHashInstruction* QoreIRBuilder::createNewComplexHash(const NewComplexHashNode* node,
        const QoreValue& expr, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRNewComplexHashInstruction>(node, expr);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRNewComplexListInstruction* QoreIRBuilder::createNewComplexList(const NewComplexListNode* node,
        const QoreValue& expr, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRNewComplexListInstruction>(node, expr);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRVrnConstructInstruction* QoreIRBuilder::createVrnConstruct(const VarRefNewObjectNode* vrn,
        const QoreValue& expr, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRVrnConstructInstruction>(vrn, expr);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRNewHashDeclFromHashInstruction* QoreIRBuilder::createNewHashDeclFromHash(const TypedHashDecl* hd,
        bool runtime_check, QoreIRValue hash_val, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRNewHashDeclFromHashInstruction>(hd, runtime_check);
    inst->loc = loc;
    inst->operands.push_back(hash_val);
    inst->result = func->createValue();
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createHashSetKeyValue(QoreIRValue hash, QoreIRValue key,
        QoreIRValue value, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::HashSetKeyValue);
    inst->loc = loc;
    inst->operands.push_back(hash);
    inst->operands.push_back(key);
    inst->operands.push_back(value);
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createIteratorCreateReverse(QoreIRValue iterable,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::IteratorCreateReverse);
    inst->loc = loc;
    inst->operands.push_back(iterable);
    inst->result = func->createValue();
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createLoadImplicitArg(int offset, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRImplicitArgInstruction>(offset);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createLoadImplicitArgv(const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::LoadImplicitArgv);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createLoadImplicitElement(const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::LoadImplicitElement);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createPushImplicitArg(QoreIRValue value, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::PushImplicitArg);
    inst->loc = loc;
    inst->operands.push_back(value);
    inst->result = func->createValue();  // Result is old context for later restoration
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createSetImplicitArgv(QoreIRValue argv_list, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::SetImplicitArgv);
    inst->loc = loc;
    inst->operands.push_back(argv_list);
    inst->result = func->createValue();  // Result is old context for later restoration
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createPopImplicitArg(QoreIRValue old_context, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::PopImplicitArg);
    inst->loc = loc;
    inst->operands.push_back(old_context);
    // No result - this is just a context restoration
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createPushImplicitElement(QoreIRValue index, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::PushImplicitElement);
    inst->loc = loc;
    inst->operands.push_back(index);
    inst->result = func->createValue();  // Result is old element for later restoration
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createPopImplicitElement(QoreIRValue old_element, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::PopImplicitElement);
    inst->loc = loc;
    inst->operands.push_back(old_element);
    // No result - this is just a context restoration
    return inst;
}

QoreIRLValueInstruction* QoreIRBuilder::createLoadLValue(const QoreValue& lvalue, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRLValueInstruction>(QoreIROpcode::LoadLValue, lvalue);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRLValueInstruction* QoreIRBuilder::createStoreLValue(const QoreValue& lvalue, QoreIRValue value,
        const QoreProgramLocation* loc, bool weak) {
    auto inst = block->appendInstruction<QoreIRLValueInstruction>(QoreIROpcode::StoreLValue, lvalue, weak);
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
    // For generic Call/CallMethod/CallStatic/CallIndirect, conservatively assume
    // ref args are possible since we don't have resolved variant information
    if (op == QoreIROpcode::Call || op == QoreIROpcode::CallMethod ||
        op == QoreIROpcode::CallStatic || op == QoreIROpcode::CallIndirect) {
        inst->has_ref_args = true;
    }
    return inst;
}

static bool checkRefArgs(const AbstractQoreFunctionVariant* variant) {
    if (!variant) return false;
    // Get the UserVariantBase to access parameter type information
    auto* uvb = variant->getUserVariantBase();
    if (!uvb) return false;
    auto* sig = uvb->getUserSignature();
    if (!sig) return false;
    // Check if any declared parameter is a reference type.
    // Only declared reference<T> params create callee→caller bindings that
    // can modify the caller's local variables.  The implicit argv parameter
    // contains copies/values of extra arguments (not lvalue bindings), so it
    // does not need full locals cache invalidation.
    for (size_t i = 0; i < sig->numParams(); ++i) {
        auto pinfo = sig->getParamTypeInfo(i);
        if (pinfo && QoreTypeInfo::isReference(pinfo)) {
            return true;
        }
    }
    return false;
}

QoreIRCallDirectInstruction* QoreIRBuilder::createCallDirect(const QoreFunction* qf,
        const AbstractQoreFunctionVariant* variant, QoreProgram* pgm, const QoreValue& expr,
        const std::vector<QoreIRValue>& args, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRCallDirectInstruction>(qf, variant, pgm, expr);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->operands = args;
    // Check if any argument is a reference type (may be modified by callee)
    inst->has_ref_args = checkRefArgs(variant);
    // Check if this is a self-recursive call (callee has same name as current function)
    if (qf && func && func->name == qf->getName()) {
        inst->is_self_recursive = true;
    }
    return inst;
}

QoreIRCallMethodDirectInstruction* QoreIRBuilder::createCallMethodDirect(const QoreMethod* method,
        const QoreClass* qc, const AbstractQoreFunctionVariant* variant,
        const std::vector<QoreIRValue>& args, const QoreValue& expr, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRCallMethodDirectInstruction>(method, qc, variant, expr);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->operands = args;
    // Check if any argument is a reference type (may be modified by callee)
    inst->has_ref_args = checkRefArgs(variant);
    return inst;
}

QoreIRInvokeMethodDirectInstruction* QoreIRBuilder::createInvokeMethodDirect(const QoreMethod* method,
        const QoreClass* qc, const AbstractQoreFunctionVariant* variant,
        const std::vector<QoreIRValue>& args, QoreIRBasicBlock* normal_target,
        QoreIRBasicBlock* exception_target, const QoreValue& expr, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInvokeMethodDirectInstruction>(
            method, qc, variant, normal_target, exception_target, expr);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->operands = args;
    // Check if any argument is a reference type (may be modified by callee)
    inst->has_ref_args = checkRefArgs(variant);
    return inst;
}

QoreIRCallStaticDirectInstruction* QoreIRBuilder::createCallStaticDirect(const QoreMethod* method,
        const AbstractQoreFunctionVariant* variant, const QoreValue& expr,
        const std::vector<QoreIRValue>& args, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRCallStaticDirectInstruction>(method, variant, expr);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->operands = args;
    // Check if any argument is a reference type (may be modified by callee)
    inst->has_ref_args = checkRefArgs(variant);
    return inst;
}

QoreIRDotEvalMethodDirectInstruction* QoreIRBuilder::createDotEvalMethodDirect(const QoreMethod* method,
        const QoreClass* qc, const AbstractQoreFunctionVariant* variant, const QoreValue& expr,
        bool pseudo, const std::vector<QoreIRValue>& operands, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRDotEvalMethodDirectInstruction>(method, qc, variant, expr, pseudo);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->operands = operands;
    // Check if any argument is a reference type (may be modified by callee)
    inst->has_ref_args = checkRefArgs(variant);
    return inst;
}

QoreIRInvokeDotEvalMethodDirectInstruction* QoreIRBuilder::createInvokeDotEvalMethodDirect(
        const QoreMethod* method, const QoreClass* qc, const AbstractQoreFunctionVariant* variant,
        const QoreValue& expr, bool pseudo, const std::vector<QoreIRValue>& operands,
        QoreIRBasicBlock* normal_target, QoreIRBasicBlock* exception_target,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInvokeDotEvalMethodDirectInstruction>(
            method, qc, variant, expr, pseudo, normal_target, exception_target);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->operands = operands;
    // Check if any argument is a reference type (may be modified by callee)
    inst->has_ref_args = checkRefArgs(variant);
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

QoreIRAddAssignLocalIntInstruction* QoreIRBuilder::createAddAssignLocalInt(LocalVar* target, LocalVar* source,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRAddAssignLocalIntInstruction>(target, source);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRIncrementLocalIntInstruction* QoreIRBuilder::createIncrementLocalInt(LocalVar* local, int64_t delta,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRIncrementLocalIntInstruction>(local, delta);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRBranchIfLtLocalIntInstruction* QoreIRBuilder::createBranchIfLtLocalInt(LocalVar* lhs, LocalVar* rhs,
        QoreIRBasicBlock* true_target, QoreIRBasicBlock* false_target, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRBranchIfLtLocalIntInstruction>(lhs, rhs, true_target, false_target);
    inst->loc = loc;
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

QoreIRThrowInstruction* QoreIRBuilder::createRethrow(QoreIRBasicBlock* exception_target,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRThrowInstruction>(QoreIROpcode::Rethrow);
    inst->exception_target = exception_target;
    inst->loc = loc;
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createThreadExit(const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::ThreadExit);
    inst->loc = loc;
    return inst;
}

QoreIRLandingPadInstruction* QoreIRBuilder::createLandingPad(size_t scope_depth, uint32_t try_scope_id,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRLandingPadInstruction>(scope_depth, try_scope_id);
    inst->loc = loc;
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createCatchException(const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::CatchException);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createCatchCleanup(const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::CatchCleanup);
    inst->loc = loc;
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

QoreIRIteratorCreateInstruction* QoreIRBuilder::createIteratorCreate(QoreIRValue iterable,
        FunctionalOperator* iterator_func, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRIteratorCreateInstruction>(iterable, iterator_func);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRIteratorNextInstruction* QoreIRBuilder::createIteratorNext(QoreIRValue iterator, QoreIRBasicBlock* done_target,
        QoreIRBasicBlock* continue_target, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRIteratorNextInstruction>(iterator, done_target, continue_target);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRRefForeachInitInstruction* QoreIRBuilder::createRefForeachInit(const QoreValue& parse_ref_expr,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRRefForeachInitInstruction>(parse_ref_expr);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createRefForeachSize(QoreIRValue state, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::RefForeachSize);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->operands.push_back(state);
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createRefForeachGetEntry(QoreIRValue state, QoreIRValue index,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::RefForeachGetEntry);
    inst->loc = loc;
    inst->result = func->createValue();
    inst->operands.push_back(state);
    inst->operands.push_back(index);
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createRefForeachRecord(QoreIRValue state, QoreIRValue value,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::RefForeachRecord);
    inst->loc = loc;
    inst->operands.push_back(state);
    inst->operands.push_back(value);
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createRefForeachFinalize(QoreIRValue state, QoreIRValue fill_remaining,
        const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::RefForeachFinalize);
    inst->loc = loc;
    inst->operands.push_back(state);
    inst->operands.push_back(fill_remaining);
    return inst;
}

QoreIRInstruction* QoreIRBuilder::createRefForeachCleanup(QoreIRValue state, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRInstruction>(QoreIROpcode::RefForeachCleanup);
    inst->loc = loc;
    inst->operands.push_back(state);
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
        const QoreProgramLocation* loc, bool inline_lowered) {
    auto inst = block->appendInstruction<QoreIRScopeExitInstruction>(scope_id, is_error, inline_lowered);
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

QoreIRSwitchRegexMatchInstruction* QoreIRBuilder::createSwitchRegexMatch(const CaseNodeRegex* regex_case,
        QoreIRValue switch_val, const QoreProgramLocation* loc) {
    // Clone the regex case node so the IR instruction doesn't depend on the AST node's lifetime
    // (important when serializing cached IR after the AST has been freed)
    QoreRegex* re = regex_case->getRegex();
    QoreRegex* cloned_re = nullptr;
    if (re) {
        ExceptionSink xsink;
        cloned_re = new QoreRegex(re->getPatternCStr(), re->getOptions(), &xsink);
        if (xsink) {
            delete cloned_re;
            return nullptr;
        }
    }

    const CaseNodeRegex* cloned_case = dynamic_cast<const CaseNodeNegRegex*>(regex_case)
        ? new CaseNodeNegRegex(loc ? loc : &loc_builtin, cloned_re, nullptr)
        : new CaseNodeRegex(loc ? loc : &loc_builtin, cloned_re, nullptr);

    auto inst = block->appendInstruction<QoreIRSwitchRegexMatchInstruction>(cloned_case);
    inst->owns_regex_case = true;
    inst->operands.push_back(switch_val);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}

QoreIRSwitchCaseMatchInstruction* QoreIRBuilder::createSwitchCaseMatch(const CaseNode* case_node,
        QoreIRValue switch_val, const QoreProgramLocation* loc) {
    auto inst = block->appendInstruction<QoreIRSwitchCaseMatchInstruction>(case_node);
    inst->operands.push_back(switch_val);
    inst->loc = loc;
    inst->result = func->createValue();
    return inst;
}
