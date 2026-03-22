/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreAOTInstRegistry.cpp

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

#include "qore/intern/QoreAOTInstRegistry.h"
#include "qore/intern/QoreAOTBinary.h"
#include "qore/QoreValue.h"

// Forward declarations for recursive serialization functions
bool serializeIRFunction(QoreAOTBinaryWriter& writer, const QoreIRFunction& func,
        const AOTExprWriteFunc& writeExpr);
std::unique_ptr<QoreIRFunction> deserializeIRFunction(
        const QoreAOTBinaryReader& reader,
        const uint8_t*& ptr, const uint8_t* end,
        QoreProgram* pgm,
        const AOTExprReadFunc& readExpr,
        const std::unordered_map<std::string, class LocalVar*>* enclosing_locals,
        std::string& error);

// Forward decl for getLocalTypePath
const char* getLocalTypePath(const LocalVar* lv);

// ============================================================================
// Group 0: Base - No extra fields
// ============================================================================

static bool writeBase(AOTInstWriteCtx& ctx) {
    return true;
}

static std::unique_ptr<QoreIRInstruction> readBase(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    auto inst = std::make_unique<QoreIRInstruction>(static_cast<QoreIROpcode>(opcode_raw));
    inst->result = QoreIRValue(result_id);
    inst->operands = std::move(operands);
    inst->exception_target = exc_target;
    return inst;
}

// ============================================================================
// Group 1: Const - Constant value with kind dispatch
// ============================================================================

static bool writeConst(AOTInstWriteCtx& ctx) {
    auto* ci = static_cast<const QoreIRConstInstruction*>(ctx.inst);
    ctx.writer.writeU8(static_cast<uint8_t>(ci->constant.kind));
    switch (ci->constant.kind) {
        case QoreIRConstant::Kind::Int:
            ctx.writer.writeI64(ci->constant.int_value);
            break;
        case QoreIRConstant::Kind::Float:
            ctx.writer.writeF64(ci->constant.float_value);
            break;
        case QoreIRConstant::Kind::Bool:
            ctx.writer.writeU8(ci->constant.bool_value ? 1 : 0);
            break;
        case QoreIRConstant::Kind::Nothing:
        case QoreIRConstant::Kind::Null:
            break;
        case QoreIRConstant::Kind::String:
            ctx.writer.writeStringRef(ci->constant.string_value.c_str());
            break;
        case QoreIRConstant::Kind::Date:
            ctx.writer.writeI64(ci->constant.date_microseconds);
            ctx.writer.writeU8(ci->constant.date_is_relative ? 1 : 0);
            break;
        case QoreIRConstant::Kind::Enum:
            if (ci->constant.enum_member) {
                std::string ns_path = ci->constant.enum_member->getEnumDecl()->getNamespacePath();
                ctx.writer.writeStringRef(ns_path.c_str());
                ctx.writer.writeStringRef(ci->constant.enum_member->getName());
            } else {
                ctx.writer.writeStringRef("");
                ctx.writer.writeStringRef("");
            }
            break;
    }
    return true;
}

static std::unique_ptr<QoreIRInstruction> readConst(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    auto* ci = new QoreIRConstInstruction();
    ci->opcode = static_cast<QoreIROpcode>(opcode_raw);
    uint8_t kind_byte = QoreAOTBinaryReader::readU8(ctx.ptr);
    ci->constant.kind = static_cast<QoreIRConstant::Kind>(kind_byte);
    switch (ci->constant.kind) {
        case QoreIRConstant::Kind::Int:
            ci->constant.int_value = QoreAOTBinaryReader::readI64(ctx.ptr);
            break;
        case QoreIRConstant::Kind::Float:
            ci->constant.float_value = QoreAOTBinaryReader::readF64(ctx.ptr);
            break;
        case QoreIRConstant::Kind::Bool:
            ci->constant.bool_value = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;
            break;
        case QoreIRConstant::Kind::Nothing:
        case QoreIRConstant::Kind::Null:
            break;
        case QoreIRConstant::Kind::String:
            ci->constant.string_value = ctx.reader.readStringRef(ctx.ptr);
            break;
        case QoreIRConstant::Kind::Date:
            ci->constant.date_microseconds = QoreAOTBinaryReader::readI64(ctx.ptr);
            ci->constant.date_is_relative = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;
            break;
        case QoreIRConstant::Kind::Enum: {
            const char* enum_path = ctx.reader.readStringRef(ctx.ptr);
            const char* member_name = ctx.reader.readStringRef(ctx.ptr);
            if (enum_path && *enum_path && member_name && *member_name) {
                const QoreNamespace* pns = nullptr;
                const QoreEnumDecl* ed = ctx.pgm->findEnum(enum_path, pns);
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
    ci->result = QoreIRValue(result_id);
    ci->operands = std::move(operands);
    ci->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(ci);
}

// ============================================================================
// Group 2: Branch - Single block target
// ============================================================================

static bool writeBranch(AOTInstWriteCtx& ctx) {
    auto* bi = static_cast<const QoreIRBranchInstruction*>(ctx.inst);
    auto it = ctx.block_idx.find(bi->target);
    ctx.writer.writeU16(it != ctx.block_idx.end() ? it->second : 0xFFFF);
    return true;
}

static std::unique_ptr<QoreIRInstruction> readBranch(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    auto* bi = new QoreIRBranchInstruction();
    bi->opcode = static_cast<QoreIROpcode>(opcode_raw);
    uint16_t target_idx = QoreAOTBinaryReader::readU16(ctx.ptr);
    bi->target = ctx.resolveBlock(target_idx);
    bi->result = QoreIRValue(result_id);
    bi->operands = std::move(operands);
    bi->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(bi);
}

// ============================================================================
// Group 3: BranchIf - Conditional branch
// ============================================================================

static bool writeBranchIf(AOTInstWriteCtx& ctx) {
    auto* bi = static_cast<const QoreIRBranchIfInstruction*>(ctx.inst);
    ctx.writer.writeU32(bi->condition.id);
    auto it_t = ctx.block_idx.find(bi->true_target);
    ctx.writer.writeU16(it_t != ctx.block_idx.end() ? it_t->second : 0xFFFF);
    auto it_f = ctx.block_idx.find(bi->false_target);
    ctx.writer.writeU16(it_f != ctx.block_idx.end() ? it_f->second : 0xFFFF);
    return true;
}

static std::unique_ptr<QoreIRInstruction> readBranchIf(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    auto* bi = new QoreIRBranchIfInstruction();
    bi->opcode = static_cast<QoreIROpcode>(opcode_raw);
    bi->condition = QoreIRValue(QoreAOTBinaryReader::readU32(ctx.ptr));
    uint16_t true_idx = QoreAOTBinaryReader::readU16(ctx.ptr);
    uint16_t false_idx = QoreAOTBinaryReader::readU16(ctx.ptr);
    bi->true_target = ctx.resolveBlock(true_idx);
    bi->false_target = ctx.resolveBlock(false_idx);
    bi->result = QoreIRValue(result_id);
    bi->operands = std::move(operands);
    bi->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(bi);
}

// ============================================================================
// Group 4: Return - Optional value return
// ============================================================================

static bool writeReturn(AOTInstWriteCtx& ctx) {
    auto* ri = static_cast<const QoreIRReturnInstruction*>(ctx.inst);
    ctx.writer.writeU8(ri->has_value ? 1 : 0);
    if (ri->has_value) {
        ctx.writer.writeU32(ri->value.id);
    }
    return true;
}

static std::unique_ptr<QoreIRInstruction> readReturn(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    auto* ri = new QoreIRReturnInstruction();
    ri->opcode = static_cast<QoreIROpcode>(opcode_raw);
    ri->has_value = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;
    if (ri->has_value) {
        ri->value = QoreIRValue(QoreAOTBinaryReader::readU32(ctx.ptr));
    }
    ri->result = QoreIRValue(result_id);
    ri->operands = std::move(operands);
    ri->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(ri);
}

// ============================================================================
// Group 5: Throw - Exception with catch depth
// ============================================================================

static bool writeThrow(AOTInstWriteCtx& ctx) {
    auto* ti = static_cast<const QoreIRThrowInstruction*>(ctx.inst);
    if (ti->exception_target) {
        auto it = ctx.block_idx.find(ti->exception_target);
        ctx.writer.writeU16(it != ctx.block_idx.end() ? it->second : 0xFFFF);
    } else {
        ctx.writer.writeU16(0xFFFF);
    }
    ctx.writer.writeU16(static_cast<uint16_t>(ti->catch_depth));
    ctx.writer.writeU8(ti->synthetic ? 1 : 0);
    return true;
}

static std::unique_ptr<QoreIRInstruction> readThrow(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    auto* ti = new QoreIRThrowInstruction(static_cast<QoreIROpcode>(opcode_raw));
    uint16_t throw_exc_idx = QoreAOTBinaryReader::readU16(ctx.ptr);
    ti->exception_target = ctx.resolveBlock(throw_exc_idx);
    ti->catch_depth = static_cast<int>(QoreAOTBinaryReader::readU16(ctx.ptr));
    ti->synthetic = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;
    ti->result = QoreIRValue(result_id);
    ti->operands = std::move(operands);
    ti->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(ti);
}

// ============================================================================
// Group 6: Local - Local variable declaration
// ============================================================================

static bool writeLocal(AOTInstWriteCtx& ctx) {
    auto* li = static_cast<const QoreIRLocalInstruction*>(ctx.inst);
    ctx.writer.writeStringRef(li->local ? li->local->getName() : "");
    ctx.writer.writeStringRef(li->local ? getLocalTypePath(li->local) : "");
    ctx.writer.writeU32(li->slot_id);
    ctx.writer.writeU8(li->auto_ref ? 1 : 0);
    ctx.writer.writeU8(li->weak ? 1 : 0);
    ctx.writer.writeU8(li->is_closure ? 1 : 0);
    ctx.writer.writeU8(li->is_ref ? 1 : 0);
    return true;
}

static std::unique_ptr<QoreIRInstruction> readLocal(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    const char* lname = ctx.reader.readStringRef(ctx.ptr);
    const char* ltype = ctx.reader.readStringRef(ctx.ptr);
    uint32_t slot_id = QoreAOTBinaryReader::readU32(ctx.ptr);
    bool auto_ref = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;
    bool weak = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;
    bool is_closure = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;
    bool is_ref = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;

    LocalVar* lv = ctx.resolveLocal(lname);
    if (!lv && lname && *lname) {
        std::string type_error;
        QoreAOTTypeResolver type_resolver(ctx.pgm);
        const QoreTypeInfo* ti = (ltype && *ltype)
            ? type_resolver.resolve(ltype, type_error) : nullptr;
        qore_program_private* pp = qore_program_private::get(*ctx.pgm);
        lv = pp->createLocalVar(lname, ti);
    }
    auto* li = new QoreIRLocalInstruction(static_cast<QoreIROpcode>(opcode_raw), lv, auto_ref);
    li->weak = weak;
    li->is_closure = is_closure;
    li->is_ref = is_ref;
    li->slot_id = slot_id;
    li->result = QoreIRValue(result_id);
    li->operands = std::move(operands);
    li->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(li);
}

// ============================================================================
// STUB IMPLEMENTATIONS FOR REMAINING GROUPS 7-51
// These are placeholders - full implementations will be added in next iteration
// ============================================================================

#define MAKE_STUB_PAIR(N, CLASS) \
static bool write_group_##N(AOTInstWriteCtx& ctx) { return true; } \
static std::unique_ptr<QoreIRInstruction> read_group_##N(uint16_t opcode_raw, QoreIRBasicBlock* exc_target, \
        const std::vector<QoreIRValue>& operands, uint32_t result_id, AOTInstReadCtx& ctx) { \
    return nullptr; \
}

MAKE_STUB_PAIR(7, QoreIRVarInstruction)
MAKE_STUB_PAIR(8, QoreIRLValueInstruction)
MAKE_STUB_PAIR(9, QoreIRExprInstruction)
MAKE_STUB_PAIR(10, QoreIRCallDirectInstruction)
MAKE_STUB_PAIR(11, QoreIRCallMethodDirectInstruction)
MAKE_STUB_PAIR(12, QoreIRInvokeMethodDirectInstruction)
MAKE_STUB_PAIR(13, QoreIRCallStaticDirectInstruction)
MAKE_STUB_PAIR(14, QoreIRDotEvalMethodDirectInstruction)
MAKE_STUB_PAIR(15, QoreIRInvokeDotEvalMethodDirectInstruction)
MAKE_STUB_PAIR(16, QoreIRInvokeInstruction)
MAKE_STUB_PAIR(17, QoreIRScopeEnterInstruction)
MAKE_STUB_PAIR(18, QoreIRScopeExitInstruction)
MAKE_STUB_PAIR(19, QoreIRLandingPadInstruction)
MAKE_STUB_PAIR(20, QoreIRSwitchIntInstruction)
MAKE_STUB_PAIR(21, QoreIRSwitchStringInstruction)
MAKE_STUB_PAIR(22, QoreIRPhiInstruction)
MAKE_STUB_PAIR(23, QoreIRGuardInstruction)
MAKE_STUB_PAIR(24, QoreIRImplicitArgInstruction)
MAKE_STUB_PAIR(25, QoreIRHashKeyAccessInstruction)
MAKE_STUB_PAIR(26, QoreIRSelfMemberInstruction)
MAKE_STUB_PAIR(27, QoreIRStaticVarInstruction)
MAKE_STUB_PAIR(28, QoreIRNewObjectInstruction)
MAKE_STUB_PAIR(29, QoreIRLoadConstantInstruction)
MAKE_STUB_PAIR(30, QoreIRCreateClosureInstruction)
MAKE_STUB_PAIR(31, QoreIRCreateCallRefInstruction)
MAKE_STUB_PAIR(32, QoreIRCreateMethodRefInstruction)
MAKE_STUB_PAIR(33, QoreIRCreateParseRefInstruction)
MAKE_STUB_PAIR(34, QoreIRNewHashDeclInstruction)
MAKE_STUB_PAIR(35, QoreIRNewComplexHashInstruction)
MAKE_STUB_PAIR(36, QoreIRNewComplexListInstruction)
MAKE_STUB_PAIR(37, QoreIRVrnConstructInstruction)
MAKE_STUB_PAIR(38, QoreIRHashKeyStoreInstruction)
MAKE_STUB_PAIR(39, QoreIRListIndexStoreInstruction)
MAKE_STUB_PAIR(40, QoreIRAddAssignLocalIntInstruction)
MAKE_STUB_PAIR(41, QoreIRIncrementLocalIntInstruction)
MAKE_STUB_PAIR(42, QoreIRBranchIfLtLocalIntInstruction)
MAKE_STUB_PAIR(43, QoreIRMapHashKeyInstruction)
MAKE_STUB_PAIR(44, QoreIRIteratorCreateInstruction)
MAKE_STUB_PAIR(45, QoreIRIteratorNextInstruction)
MAKE_STUB_PAIR(46, QoreIRRefForeachInitInstruction)
MAKE_STUB_PAIR(47, QoreIRSwitchRegexMatchInstruction)
MAKE_STUB_PAIR(49, QoreIRMakeHashConstKeysInstruction)
MAKE_STUB_PAIR(50, QoreIRSwitchCaseMatchInstruction)
MAKE_STUB_PAIR(51, QoreIRListIndexAccessInstruction)

#undef MAKE_STUB_PAIR

// ============================================================================
// Group 48: OnBlockExit - Handler with nested IR
// ============================================================================

static bool writeOnBlockExit(AOTInstWriteCtx& ctx) {
    auto* obe_inst = static_cast<const QoreIROnBlockExitInstruction*>(ctx.inst);
    obe_type_e type = obe_inst->stmt ? obe_inst->stmt->getType() : obe_inst->obe_type;
    ctx.writer.writeU8(static_cast<uint8_t>(type));
    if (obe_inst->handler_ir) {
        ctx.writer.writeU8(1);
        if (!serializeIRFunction(ctx.writer, *obe_inst->handler_ir, ctx.writeExpr)) {
            return false;
        }
    } else {
        return false;
    }
    return true;
}

static std::unique_ptr<QoreIRInstruction> readOnBlockExit(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    obe_type_e obe_type = static_cast<obe_type_e>(QoreAOTBinaryReader::readU8(ctx.ptr));
    uint8_t has_handler_ir = QoreAOTBinaryReader::readU8(ctx.ptr);
    std::unique_ptr<QoreIRFunction> nested_handler;
    if (has_handler_ir) {
        nested_handler = deserializeIRFunction(ctx.reader, ctx.ptr, ctx.end, ctx.pgm, ctx.readExpr,
            nullptr, ctx.error);
        if (!nested_handler) {
            ctx.error = "failed to deserialize nested OnBlockExit handler IR: " + ctx.error;
            return nullptr;
        }
        nested_handler->computeSlotIdsAndEmbed();
    } else {
        ctx.error = "OnBlockExit without handler IR in deserialized context";
        return nullptr;
    }
    auto* obe_inst = new QoreIROnBlockExitInstruction(obe_type, std::move(nested_handler));
    obe_inst->opcode = static_cast<QoreIROpcode>(opcode_raw);
    obe_inst->result = QoreIRValue(result_id);
    obe_inst->operands = std::move(operands);
    obe_inst->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(obe_inst);
}

// ============================================================================
// Instruction Group Registry Table
// ============================================================================

const QoreIRInstGroupInfo AOT_INST_GROUP_REGISTRY[AOT_INST_GROUP_TABLE_SIZE] = {
    // Index 0: Base
    { "Base", 0, true, false, writeBase, readBase, "Base instruction with no group-specific fields" },

    // Index 1: Const
    { "Const", 1, true, false, writeConst, readConst, "Constant value (int/float/bool/string/date/enum)" },

    // Index 2: Branch
    { "Branch", 2, true, false, writeBranch, readBranch, "Unconditional branch to target block" },

    // Index 3: BranchIf
    { "BranchIf", 3, true, false, writeBranchIf, readBranchIf, "Conditional branch to two target blocks" },

    // Index 4: Return
    { "Return", 4, true, false, writeReturn, readReturn, "Function return with optional value" },

    // Index 5: Throw
    { "Throw", 5, true, false, writeThrow, readThrow, "Exception throw with catch depth tracking" },

    // Index 6: Local
    { "Local", 6, true, false, writeLocal, readLocal, "Local variable slot definition" },

    // Index 7: Var
    { "Var", 7, true, false, write_group_7, read_group_7, "Global variable reference" },

    // Index 8: LValue
    { "LValue", 8, true, false, write_group_8, read_group_8, "LValue (assignable) reference" },

    // Index 9: Expr
    { "Expr", 9, true, false, write_group_9, read_group_9, "Generic expression" },

    // Index 10: CallDirect
    { "CallDirect", 10, true, false, write_group_10, read_group_10, "Direct function call" },

    // Index 11: CallMethodDirect
    { "CallMethodDirect", 11, true, false, write_group_11, read_group_11, "Direct method call" },

    // Index 12: InvokeMethodDirect
    { "InvokeMethodDirect", 12, true, false, write_group_12, read_group_12, "Direct method call with exception handling" },

    // Index 13: CallStaticDirect
    { "CallStaticDirect", 13, true, false, write_group_13, read_group_13, "Static method call" },

    // Index 14: DotEvalMethodDirect
    { "DotEvalMethodDirect", 14, true, false, write_group_14, read_group_14, "Dot-notation method evaluation" },

    // Index 15: InvokeDotEvalMethodDirect
    { "InvokeDotEvalMethodDirect", 15, true, false, write_group_15, read_group_15, "Dot-notation method evaluation with exception handling" },

    // Index 16: Invoke
    { "Invoke", 16, true, false, write_group_16, read_group_16, "Generic method invocation" },

    // Index 17: ScopeEnter
    { "ScopeEnter", 17, true, false, write_group_17, read_group_17, "Scope entry tracking" },

    // Index 18: ScopeExit
    { "ScopeExit", 18, true, false, write_group_18, read_group_18, "Scope exit tracking" },

    // Index 19: LandingPad
    { "LandingPad", 19, true, false, write_group_19, read_group_19, "Exception handling landing pad" },

    // Index 20: SwitchInt
    { "SwitchInt", 20, true, false, write_group_20, read_group_20, "Switch on integer value" },

    // Index 21: SwitchString
    { "SwitchString", 21, true, false, write_group_21, read_group_21, "Switch on string value" },

    // Index 22: Phi
    { "Phi", 22, true, false, write_group_22, read_group_22, "Phi node for value merging" },

    // Index 23: Guard
    { "Guard", 23, true, false, write_group_23, read_group_23, "Type guard with deoptimization" },

    // Index 24: ImplicitArg
    { "ImplicitArg", 24, true, false, write_group_24, read_group_24, "Implicit argument access" },

    // Index 25: HashKeyAccess
    { "HashKeyAccess", 25, true, false, write_group_25, read_group_25, "Hash key access by name" },

    // Index 26: SelfMember
    { "SelfMember", 26, true, false, write_group_26, read_group_26, "Member access on self" },

    // Index 27: StaticVar
    { "StaticVar", 27, true, false, write_group_27, read_group_27, "Static variable access" },

    // Index 28: NewObject
    { "NewObject", 28, true, false, write_group_28, read_group_28, "Object instantiation" },

    // Index 29: LoadConst
    { "LoadConst", 29, true, false, write_group_29, read_group_29, "Constant loading" },

    // Index 30: CreateClosure
    { "CreateClosure", 30, true, false, write_group_30, read_group_30, "Closure creation" },

    // Index 31: CreateCallRef
    { "CreateCallRef", 31, true, false, write_group_31, read_group_31, "Call reference creation" },

    // Index 32: CreateMethodRef
    { "CreateMethodRef", 32, true, false, write_group_32, read_group_32, "Method reference creation" },

    // Index 33: CreateParseRef
    { "CreateParseRef", 33, true, false, write_group_33, read_group_33, "Parse reference creation" },

    // Index 34: NewHashDecl
    { "NewHashDecl", 34, true, false, write_group_34, read_group_34, "Hash declaration instantiation" },

    // Index 35: NewComplexHash
    { "NewComplexHash", 35, true, false, write_group_35, read_group_35, "Complex hash creation" },

    // Index 36: NewComplexList
    { "NewComplexList", 36, true, false, write_group_36, read_group_36, "Complex list creation" },

    // Index 37: VrnConstruct
    { "VrnConstruct", 37, true, false, write_group_37, read_group_37, "Variant value construction" },

    // Index 38: HashKeyStore
    { "HashKeyStore", 38, true, false, write_group_38, read_group_38, "Hash key storage" },

    // Index 39: ListIndexStore
    { "ListIndexStore", 39, true, false, write_group_39, read_group_39, "List index storage" },

    // Index 40: FusedAddLocal
    { "FusedAddLocal", 40, true, false, write_group_40, read_group_40, "Fused += operation on local" },

    // Index 41: FusedIncLocal
    { "FusedIncLocal", 41, true, false, write_group_41, read_group_41, "Fused ++ operation on local" },

    // Index 42: FusedBrLtLocal
    { "FusedBrLtLocal", 42, true, false, write_group_42, read_group_42, "Fused branch-if-less-than on locals" },

    // Index 43: MapHashKey
    { "MapHashKey", 43, true, false, write_group_43, read_group_43, "Hash key mapping operation" },

    // Index 44: IteratorCreate
    { "IteratorCreate", 44, true, false, write_group_44, read_group_44, "Iterator creation from iterable" },

    // Index 45: IteratorNext
    { "IteratorNext", 45, true, false, write_group_45, read_group_45, "Iterator advance with targets" },

    // Index 46: RefForeachInit
    { "RefForeachInit", 46, true, false, write_group_46, read_group_46, "Reference foreach initialization" },

    // Index 47: SwitchRegexMatch
    { "SwitchRegexMatch", 47, true, false, write_group_47, read_group_47, "Regex pattern matching" },

    // Index 48: OnBlockExit
    { "OnBlockExit", 48, true, true, writeOnBlockExit, readOnBlockExit, "Exception handler with nested IR function" },

    // Index 49: MakeHashConstKeys
    { "MakeHashConstKeys", 49, true, false, write_group_49, read_group_49, "Create hash with constant keys" },

    // Index 50: SwitchCaseMatch
    { "SwitchCaseMatch", 50, true, false, write_group_50, read_group_50, "Switch case matching" },

    // Index 51: ListIndexAccess
    { "ListIndexAccess", 51, true, false, write_group_51, read_group_51, "List index access" },

    // Remaining 52-255: Unsupported/undefined
    #define UNUSED_ENTRY(idx) { nullptr, idx, false, false, nullptr, nullptr, nullptr }

    UNUSED_ENTRY(52), UNUSED_ENTRY(53), UNUSED_ENTRY(54), UNUSED_ENTRY(55),
    UNUSED_ENTRY(56), UNUSED_ENTRY(57), UNUSED_ENTRY(58), UNUSED_ENTRY(59),
    UNUSED_ENTRY(60), UNUSED_ENTRY(61), UNUSED_ENTRY(62), UNUSED_ENTRY(63),
    UNUSED_ENTRY(64), UNUSED_ENTRY(65), UNUSED_ENTRY(66), UNUSED_ENTRY(67),
    UNUSED_ENTRY(68), UNUSED_ENTRY(69), UNUSED_ENTRY(70), UNUSED_ENTRY(71),
    UNUSED_ENTRY(72), UNUSED_ENTRY(73), UNUSED_ENTRY(74), UNUSED_ENTRY(75),
    UNUSED_ENTRY(76), UNUSED_ENTRY(77), UNUSED_ENTRY(78), UNUSED_ENTRY(79),
    UNUSED_ENTRY(80), UNUSED_ENTRY(81), UNUSED_ENTRY(82), UNUSED_ENTRY(83),
    UNUSED_ENTRY(84), UNUSED_ENTRY(85), UNUSED_ENTRY(86), UNUSED_ENTRY(87),
    UNUSED_ENTRY(88), UNUSED_ENTRY(89), UNUSED_ENTRY(90), UNUSED_ENTRY(91),
    UNUSED_ENTRY(92), UNUSED_ENTRY(93), UNUSED_ENTRY(94), UNUSED_ENTRY(95),
    UNUSED_ENTRY(96), UNUSED_ENTRY(97), UNUSED_ENTRY(98), UNUSED_ENTRY(99),
    UNUSED_ENTRY(100), UNUSED_ENTRY(101), UNUSED_ENTRY(102), UNUSED_ENTRY(103),
    UNUSED_ENTRY(104), UNUSED_ENTRY(105), UNUSED_ENTRY(106), UNUSED_ENTRY(107),
    UNUSED_ENTRY(108), UNUSED_ENTRY(109), UNUSED_ENTRY(110), UNUSED_ENTRY(111),
    UNUSED_ENTRY(112), UNUSED_ENTRY(113), UNUSED_ENTRY(114), UNUSED_ENTRY(115),
    UNUSED_ENTRY(116), UNUSED_ENTRY(117), UNUSED_ENTRY(118), UNUSED_ENTRY(119),
    UNUSED_ENTRY(120), UNUSED_ENTRY(121), UNUSED_ENTRY(122), UNUSED_ENTRY(123),
    UNUSED_ENTRY(124), UNUSED_ENTRY(125), UNUSED_ENTRY(126), UNUSED_ENTRY(127),
    UNUSED_ENTRY(128), UNUSED_ENTRY(129), UNUSED_ENTRY(130), UNUSED_ENTRY(131),
    UNUSED_ENTRY(132), UNUSED_ENTRY(133), UNUSED_ENTRY(134), UNUSED_ENTRY(135),
    UNUSED_ENTRY(136), UNUSED_ENTRY(137), UNUSED_ENTRY(138), UNUSED_ENTRY(139),
    UNUSED_ENTRY(140), UNUSED_ENTRY(141), UNUSED_ENTRY(142), UNUSED_ENTRY(143),
    UNUSED_ENTRY(144), UNUSED_ENTRY(145), UNUSED_ENTRY(146), UNUSED_ENTRY(147),
    UNUSED_ENTRY(148), UNUSED_ENTRY(149), UNUSED_ENTRY(150), UNUSED_ENTRY(151),
    UNUSED_ENTRY(152), UNUSED_ENTRY(153), UNUSED_ENTRY(154), UNUSED_ENTRY(155),
    UNUSED_ENTRY(156), UNUSED_ENTRY(157), UNUSED_ENTRY(158), UNUSED_ENTRY(159),
    UNUSED_ENTRY(160), UNUSED_ENTRY(161), UNUSED_ENTRY(162), UNUSED_ENTRY(163),
    UNUSED_ENTRY(164), UNUSED_ENTRY(165), UNUSED_ENTRY(166), UNUSED_ENTRY(167),
    UNUSED_ENTRY(168), UNUSED_ENTRY(169), UNUSED_ENTRY(170), UNUSED_ENTRY(171),
    UNUSED_ENTRY(172), UNUSED_ENTRY(173), UNUSED_ENTRY(174), UNUSED_ENTRY(175),
    UNUSED_ENTRY(176), UNUSED_ENTRY(177), UNUSED_ENTRY(178), UNUSED_ENTRY(179),
    UNUSED_ENTRY(180), UNUSED_ENTRY(181), UNUSED_ENTRY(182), UNUSED_ENTRY(183),
    UNUSED_ENTRY(184), UNUSED_ENTRY(185), UNUSED_ENTRY(186), UNUSED_ENTRY(187),
    UNUSED_ENTRY(188), UNUSED_ENTRY(189), UNUSED_ENTRY(190), UNUSED_ENTRY(191),
    UNUSED_ENTRY(192), UNUSED_ENTRY(193), UNUSED_ENTRY(194), UNUSED_ENTRY(195),
    UNUSED_ENTRY(196), UNUSED_ENTRY(197), UNUSED_ENTRY(198), UNUSED_ENTRY(199),
    UNUSED_ENTRY(200), UNUSED_ENTRY(201), UNUSED_ENTRY(202), UNUSED_ENTRY(203),
    UNUSED_ENTRY(204), UNUSED_ENTRY(205), UNUSED_ENTRY(206), UNUSED_ENTRY(207),
    UNUSED_ENTRY(208), UNUSED_ENTRY(209), UNUSED_ENTRY(210), UNUSED_ENTRY(211),
    UNUSED_ENTRY(212), UNUSED_ENTRY(213), UNUSED_ENTRY(214), UNUSED_ENTRY(215),
    UNUSED_ENTRY(216), UNUSED_ENTRY(217), UNUSED_ENTRY(218), UNUSED_ENTRY(219),
    UNUSED_ENTRY(220), UNUSED_ENTRY(221), UNUSED_ENTRY(222), UNUSED_ENTRY(223),
    UNUSED_ENTRY(224), UNUSED_ENTRY(225), UNUSED_ENTRY(226), UNUSED_ENTRY(227),
    UNUSED_ENTRY(228), UNUSED_ENTRY(229), UNUSED_ENTRY(230), UNUSED_ENTRY(231),
    UNUSED_ENTRY(232), UNUSED_ENTRY(233), UNUSED_ENTRY(234), UNUSED_ENTRY(235),
    UNUSED_ENTRY(236), UNUSED_ENTRY(237), UNUSED_ENTRY(238), UNUSED_ENTRY(239),
    UNUSED_ENTRY(240), UNUSED_ENTRY(241), UNUSED_ENTRY(242), UNUSED_ENTRY(243),
    UNUSED_ENTRY(244), UNUSED_ENTRY(245), UNUSED_ENTRY(246), UNUSED_ENTRY(247),
    UNUSED_ENTRY(248), UNUSED_ENTRY(249), UNUSED_ENTRY(250), UNUSED_ENTRY(251),
    UNUSED_ENTRY(252), UNUSED_ENTRY(253), UNUSED_ENTRY(254), UNUSED_ENTRY(255),

    #undef UNUSED_ENTRY
};
