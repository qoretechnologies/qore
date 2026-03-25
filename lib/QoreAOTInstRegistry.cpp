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

// ============================================================================
// Group 7: Var - Global variable reference
// ============================================================================

static bool writeVar(AOTInstWriteCtx& ctx) {
    auto* vi = static_cast<const QoreIRVarInstruction*>(ctx.inst);
    ctx.writer.writeStringRef(vi->var ? vi->var->getName() : "");
    ctx.writer.writeU8(vi->weak ? 1 : 0);
    return true;
}

static std::unique_ptr<QoreIRInstruction> readVar(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    const char* vname = ctx.reader.readStringRef(ctx.ptr);
    bool weak = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;
    Var* var = nullptr;
    if (vname && *vname) {
        qore_program_private* pp = qore_program_private::get(*ctx.pgm);
        const qore_ns_private* vns = nullptr;
        var = qore_root_ns_private::runtimeFindGlobalVar(*pp->RootNS, vname, vns);
    }
    auto* vi = new QoreIRVarInstruction(static_cast<QoreIROpcode>(opcode_raw), var);
    vi->weak = weak;
    vi->result = QoreIRValue(result_id);
    vi->operands = operands;
    vi->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(vi);
}

// ============================================================================
// Group 8: LValue - Nested expression with lvalue slot
// ============================================================================

static bool writeLValue(AOTInstWriteCtx& ctx) {
    auto* lvi = static_cast<const QoreIRLValueInstruction*>(ctx.inst);
    if (!ctx.writeExpr(ctx.writer, lvi->lvalue)) {
        return false;
    }
    ctx.writer.writeU8(lvi->weak ? 1 : 0);
    ctx.writer.writeU32(lvi->lvalue_slot_id);
    return true;
}

static std::unique_ptr<QoreIRInstruction> readLValue(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    std::string error;
    QoreValue lvalue = ctx.readExpr(ctx.reader, ctx.ptr, ctx.end, error);
    if (!error.empty()) {
        return nullptr;
    }
    bool weak = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;
    uint32_t lvalue_slot_id = QoreAOTBinaryReader::readU32(ctx.ptr);
    auto* lvi = new QoreIRLValueInstruction(static_cast<QoreIROpcode>(opcode_raw), lvalue, weak);
    lvi->lvalue_slot_id = lvalue_slot_id;
    lvalue.discard(nullptr);
    lvi->result = QoreIRValue(result_id);
    lvi->operands = operands;
    lvi->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(lvi);
}

// ============================================================================
// Group 9: Expr - General expression instruction
// ============================================================================

static bool writeExpr(AOTInstWriteCtx& ctx) {
    auto* ei = static_cast<const QoreIRExprInstruction*>(ctx.inst);
    if (!ctx.writeExpr(ctx.writer, ei->expr)) {
        return false;
    }
    ctx.writer.writeU8(ei->has_ref_args ? 1 : 0);
    return true;
}

static std::unique_ptr<QoreIRInstruction> readExpr(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    std::string error;
    QoreValue expr = ctx.readExpr(ctx.reader, ctx.ptr, ctx.end, error);
    if (!error.empty()) {
        return nullptr;
    }
    bool has_ref_args = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;
    auto* ei = new QoreIRExprInstruction(static_cast<QoreIROpcode>(opcode_raw), expr);
    ei->has_ref_args = has_ref_args;
    expr.discard(nullptr);
    ei->result = QoreIRValue(result_id);
    ei->operands = operands;
    ei->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(ei);
}

// ============================================================================
// Group 10: CallDirect - Direct function call via expression
// ============================================================================

static bool writeCallDirect(AOTInstWriteCtx& ctx) {
    auto* ci = static_cast<const QoreIRCallDirectInstruction*>(ctx.inst);
    if (!ctx.writeExpr(ctx.writer, ci->expr)) {
        return false;
    }
    ctx.writer.writeU8(ci->has_ref_args ? 1 : 0);
    ctx.writer.writeU8(ci->is_self_recursive ? 1 : 0);
    return true;
}

static std::unique_ptr<QoreIRInstruction> readCallDirect(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    std::string error;
    QoreValue expr = ctx.readExpr(ctx.reader, ctx.ptr, ctx.end, error);
    if (!error.empty()) {
        return nullptr;
    }
    bool has_ref_args = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;
    bool is_self_recursive = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;
    const QoreFunction* func = nullptr;
    QoreProgram* func_pgm = ctx.pgm;
    auto* ci = new QoreIRCallDirectInstruction(func, nullptr, func_pgm, expr);
    ci->has_ref_args = has_ref_args;
    ci->is_self_recursive = is_self_recursive;
    expr.discard(nullptr);
    ci->result = QoreIRValue(result_id);
    ci->operands = operands;
    ci->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(ci);
}

// ============================================================================
// Group 11: CallMethodDirect - Direct method call
// ============================================================================

static bool writeCallMethodDirect(AOTInstWriteCtx& ctx) {
    auto* ci = static_cast<const QoreIRCallMethodDirectInstruction*>(ctx.inst);
    if (ci->expr) {
        ctx.writer.writeU8(1);
        if (!ctx.writeExpr(ctx.writer, ci->expr)) {
            return false;
        }
    } else {
        ctx.writer.writeU8(0);
    }
    ctx.writer.writeStringRef(ci->qc ? ci->qc->getPath() : "");
    ctx.writer.writeStringRef(ci->method ? ci->method->getName() : "");
    ctx.writer.writeU8(ci->has_ref_args ? 1 : 0);
    return true;
}

static std::unique_ptr<QoreIRInstruction> readCallMethodDirect(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    bool has_expr = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;
    QoreValue expr;
    std::string error;
    if (has_expr) {
        expr = ctx.readExpr(ctx.reader, ctx.ptr, ctx.end, error);
        if (!error.empty()) {
            return nullptr;
        }
    }
    const char* class_path = ctx.reader.readStringRef(ctx.ptr);
    const char* method_name = ctx.reader.readStringRef(ctx.ptr);
    bool has_ref_args = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;

    const QoreMethod* method = nullptr;
    const QoreClass* qc = nullptr;
    if (class_path && *class_path) {
        qore_program_private* pp = qore_program_private::get(*ctx.pgm);
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
    ci->result = QoreIRValue(result_id);
    ci->operands = operands;
    ci->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(ci);
}

// ============================================================================
// Group 12: InvokeMethodDirect - Direct method call with EH targets
// ============================================================================

static bool writeInvokeMethodDirect(AOTInstWriteCtx& ctx) {
    auto* ci = static_cast<const QoreIRInvokeMethodDirectInstruction*>(ctx.inst);
    if (ci->expr) {
        ctx.writer.writeU8(1);
        if (!ctx.writeExpr(ctx.writer, ci->expr)) {
            return false;
        }
    } else {
        ctx.writer.writeU8(0);
    }
    ctx.writer.writeStringRef(ci->qc ? ci->qc->getPath() : "");
    ctx.writer.writeStringRef(ci->method ? ci->method->getName() : "");
    ctx.writer.writeU8(ci->has_ref_args ? 1 : 0);
    auto it_n = ctx.block_idx.find(ci->normal_target);
    ctx.writer.writeU16(it_n != ctx.block_idx.end() ? it_n->second : 0xFFFF);
    auto it_e = ctx.block_idx.find(ci->exception_target);
    ctx.writer.writeU16(it_e != ctx.block_idx.end() ? it_e->second : 0xFFFF);
    return true;
}

static std::unique_ptr<QoreIRInstruction> readInvokeMethodDirect(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    bool has_expr = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;
    QoreValue expr;
    std::string error;
    if (has_expr) {
        expr = ctx.readExpr(ctx.reader, ctx.ptr, ctx.end, error);
        if (!error.empty()) {
            return nullptr;
        }
    }
    const char* class_path = ctx.reader.readStringRef(ctx.ptr);
    const char* method_name = ctx.reader.readStringRef(ctx.ptr);
    bool has_ref_args = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;
    uint16_t normal_idx = QoreAOTBinaryReader::readU16(ctx.ptr);
    uint16_t exception_idx = QoreAOTBinaryReader::readU16(ctx.ptr);

    const QoreMethod* method = nullptr;
    const QoreClass* qc = nullptr;
    if (class_path && *class_path) {
        qore_program_private* pp = qore_program_private::get(*ctx.pgm);
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
        ctx.resolveBlock(normal_idx), ctx.resolveBlock(exception_idx), expr);
    ci->has_ref_args = has_ref_args;
    if (has_expr) {
        expr.discard(nullptr);
    }
    ci->result = QoreIRValue(result_id);
    ci->operands = operands;
    ci->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(ci);
}

// ============================================================================
// Group 13: CallStaticDirect - Static method call
// ============================================================================

static bool writeCallStaticDirect(AOTInstWriteCtx& ctx) {
    auto* ci = static_cast<const QoreIRCallStaticDirectInstruction*>(ctx.inst);
    if (!ctx.writeExpr(ctx.writer, ci->expr)) {
        return false;
    }
    ctx.writer.writeStringRef(ci->method ? ci->method->getClass()->getPath() : "");
    ctx.writer.writeStringRef(ci->method ? ci->method->getName() : "");
    ctx.writer.writeU8(ci->has_ref_args ? 1 : 0);
    return true;
}

static std::unique_ptr<QoreIRInstruction> readCallStaticDirect(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    std::string error;
    QoreValue expr = ctx.readExpr(ctx.reader, ctx.ptr, ctx.end, error);
    if (!error.empty()) {
        return nullptr;
    }
    const char* class_path = ctx.reader.readStringRef(ctx.ptr);
    const char* method_name = ctx.reader.readStringRef(ctx.ptr);
    bool has_ref_args = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;

    const QoreMethod* method = nullptr;
    if (class_path && *class_path) {
        qore_program_private* pp = qore_program_private::get(*ctx.pgm);
        const qore_ns_private* found_ns = nullptr;
        const QoreClass* qc = qore_root_ns_private::runtimeFindClass(*pp->RootNS, class_path, found_ns);
        if (qc && method_name && *method_name) {
            method = qc->findStaticMethod(method_name);
        }
    }
    auto* ci = new QoreIRCallStaticDirectInstruction(method, nullptr, expr);
    ci->has_ref_args = has_ref_args;
    expr.discard(nullptr);
    ci->result = QoreIRValue(result_id);
    ci->operands = operands;
    ci->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(ci);
}

// ============================================================================
// Group 14: DotEvalMethodDirect - Dot-notation method evaluation
// ============================================================================

static bool writeDotEvalMethodDirect(AOTInstWriteCtx& ctx) {
    auto* ci = static_cast<const QoreIRDotEvalMethodDirectInstruction*>(ctx.inst);
    if (!ctx.writeExpr(ctx.writer, ci->expr)) {
        return false;
    }
    ctx.writer.writeStringRef(ci->qc ? ci->qc->getPath() : "");
    ctx.writer.writeStringRef(ci->method ? ci->method->getName() : "");
    ctx.writer.writeU8(ci->pseudo ? 1 : 0);
    ctx.writer.writeU8(ci->has_ref_args ? 1 : 0);
    return true;
}

static std::unique_ptr<QoreIRInstruction> readDotEvalMethodDirect(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    std::string error;
    QoreValue expr = ctx.readExpr(ctx.reader, ctx.ptr, ctx.end, error);
    if (!error.empty()) {
        return nullptr;
    }
    const char* class_path = ctx.reader.readStringRef(ctx.ptr);
    const char* method_name = ctx.reader.readStringRef(ctx.ptr);
    bool pseudo = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;
    bool has_ref_args = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;

    const QoreMethod* method = nullptr;
    const QoreClass* qc = nullptr;
    if (class_path && *class_path) {
        qore_program_private* pp = qore_program_private::get(*ctx.pgm);
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
    ci->result = QoreIRValue(result_id);
    ci->operands = operands;
    ci->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(ci);
}

// ============================================================================
// Group 15: InvokeDotEvalMethodDirect - Dot-notation method with EH targets
// ============================================================================

static bool writeInvokeDotEvalMethodDirect(AOTInstWriteCtx& ctx) {
    auto* ci = static_cast<const QoreIRInvokeDotEvalMethodDirectInstruction*>(ctx.inst);
    if (!ctx.writeExpr(ctx.writer, ci->expr)) {
        return false;
    }
    ctx.writer.writeStringRef(ci->qc ? ci->qc->getPath() : "");
    ctx.writer.writeStringRef(ci->method ? ci->method->getName() : "");
    ctx.writer.writeU8(ci->pseudo ? 1 : 0);
    ctx.writer.writeU8(ci->has_ref_args ? 1 : 0);
    auto it_n = ctx.block_idx.find(ci->normal_target);
    ctx.writer.writeU16(it_n != ctx.block_idx.end() ? it_n->second : 0xFFFF);
    auto it_e = ctx.block_idx.find(ci->exception_target);
    ctx.writer.writeU16(it_e != ctx.block_idx.end() ? it_e->second : 0xFFFF);
    return true;
}

static std::unique_ptr<QoreIRInstruction> readInvokeDotEvalMethodDirect(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    std::string error;
    QoreValue expr = ctx.readExpr(ctx.reader, ctx.ptr, ctx.end, error);
    if (!error.empty()) {
        return nullptr;
    }
    const char* class_path = ctx.reader.readStringRef(ctx.ptr);
    const char* method_name = ctx.reader.readStringRef(ctx.ptr);
    bool pseudo = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;
    bool has_ref_args = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;
    uint16_t normal_idx = QoreAOTBinaryReader::readU16(ctx.ptr);
    uint16_t exception_idx = QoreAOTBinaryReader::readU16(ctx.ptr);

    const QoreMethod* method = nullptr;
    const QoreClass* qc = nullptr;
    if (class_path && *class_path) {
        qore_program_private* pp = qore_program_private::get(*ctx.pgm);
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
        pseudo, ctx.resolveBlock(normal_idx), ctx.resolveBlock(exception_idx));
    ci->has_ref_args = has_ref_args;
    expr.discard(nullptr);
    ci->result = QoreIRValue(result_id);
    ci->operands = operands;
    ci->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(ci);
}

// ============================================================================
// Group 16: Invoke - Generic invoke with opcode and key name
// ============================================================================

static bool writeInvoke(AOTInstWriteCtx& ctx) {
    auto* ii = static_cast<const QoreIRInvokeInstruction*>(ctx.inst);
    if (!ctx.writeExpr(ctx.writer, ii->expr)) {
        return false;
    }
    ctx.writer.writeU16(static_cast<uint16_t>(ii->invoke_opcode));
    ctx.writer.writeStringRef(ii->invoke_key_name.c_str());
    ctx.writer.writeU8(ii->weak ? 1 : 0);
    auto it_n = ctx.block_idx.find(ii->normal_target);
    ctx.writer.writeU16(it_n != ctx.block_idx.end() ? it_n->second : 0xFFFF);
    auto it_e = ctx.block_idx.find(ii->exception_target);
    ctx.writer.writeU16(it_e != ctx.block_idx.end() ? it_e->second : 0xFFFF);
    return true;
}

static std::unique_ptr<QoreIRInstruction> readInvoke(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    std::string error;
    QoreValue expr = ctx.readExpr(ctx.reader, ctx.ptr, ctx.end, error);
    if (!error.empty()) {
        return nullptr;
    }
    uint16_t invoke_opcode_raw = QoreAOTBinaryReader::readU16(ctx.ptr);
    const char* invoke_key_name = ctx.reader.readStringRef(ctx.ptr);
    bool weak = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;
    uint16_t normal_idx = QoreAOTBinaryReader::readU16(ctx.ptr);
    uint16_t exception_idx = QoreAOTBinaryReader::readU16(ctx.ptr);
    auto* ii = new QoreIRInvokeInstruction(expr,
        ctx.resolveBlock(normal_idx), ctx.resolveBlock(exception_idx));
    ii->invoke_opcode = static_cast<QoreIROpcode>(invoke_opcode_raw);
    ii->invoke_key_name = invoke_key_name ? invoke_key_name : "";
    ii->weak = weak;
    expr.discard(nullptr);
    ii->result = QoreIRValue(result_id);
    ii->operands = operands;
    ii->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(ii);
}

// ============================================================================
// Group 17: ScopeEnter - Scope entry tracking
// ============================================================================

static bool writeScopeEnter(AOTInstWriteCtx& ctx) {
    auto* si = static_cast<const QoreIRScopeEnterInstruction*>(ctx.inst);
    ctx.writer.writeU32(si->scope_id);
    return true;
}

static std::unique_ptr<QoreIRInstruction> readScopeEnter(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    uint32_t scope_id = QoreAOTBinaryReader::readU32(ctx.ptr);
    auto inst = std::make_unique<QoreIRScopeEnterInstruction>(scope_id);
    inst->result = QoreIRValue(result_id);
    inst->operands = operands;
    inst->exception_target = exc_target;
    return inst;
}

// ============================================================================
// Group 18: ScopeExit - Scope exit tracking
// ============================================================================

static bool writeScopeExit(AOTInstWriteCtx& ctx) {
    auto* si = static_cast<const QoreIRScopeExitInstruction*>(ctx.inst);
    ctx.writer.writeU32(si->scope_id);
    ctx.writer.writeU8(si->is_error ? 1 : 0);
    ctx.writer.writeU8(si->inline_lowered ? 1 : 0);
    return true;
}

static std::unique_ptr<QoreIRInstruction> readScopeExit(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    uint32_t scope_id = QoreAOTBinaryReader::readU32(ctx.ptr);
    bool is_error = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;
    bool inline_lowered = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;
    auto inst = std::make_unique<QoreIRScopeExitInstruction>(scope_id, is_error, inline_lowered);
    inst->result = QoreIRValue(result_id);
    inst->operands = operands;
    inst->exception_target = exc_target;
    return inst;
}

// ============================================================================
// Group 19: LandingPad - Exception handler landing pad
// ============================================================================

static bool writeLandingPad(AOTInstWriteCtx& ctx) {
    auto* li = static_cast<const QoreIRLandingPadInstruction*>(ctx.inst);
    ctx.writer.writeU32(static_cast<uint32_t>(li->scope_depth));
    ctx.writer.writeU32(li->try_scope_id);
    return true;
}

static std::unique_ptr<QoreIRInstruction> readLandingPad(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    uint32_t scope_depth = QoreAOTBinaryReader::readU32(ctx.ptr);
    uint32_t try_scope_id = QoreAOTBinaryReader::readU32(ctx.ptr);
    auto inst = std::make_unique<QoreIRLandingPadInstruction>(
        static_cast<size_t>(scope_depth), try_scope_id);
    inst->result = QoreIRValue(result_id);
    inst->operands = operands;
    inst->exception_target = exc_target;
    return inst;
}

// ============================================================================
// Group 20: SwitchInt - Variable-length integer switch table
// ============================================================================

static bool writeSwitchInt(AOTInstWriteCtx& ctx) {
    auto* si = static_cast<const QoreIRSwitchIntInstruction*>(ctx.inst);
    ctx.writer.writeU32(si->switch_val.id);
    auto it_d = ctx.block_idx.find(si->default_target);
    ctx.writer.writeU16(it_d != ctx.block_idx.end() ? it_d->second : 0xFFFF);
    ctx.writer.writeU16(static_cast<uint16_t>(si->cases.size()));
    for (auto& c : si->cases) {
        ctx.writer.writeI64(c.value);
        auto it = ctx.block_idx.find(c.target);
        ctx.writer.writeU16(it != ctx.block_idx.end() ? it->second : 0xFFFF);
    }
    return true;
}

static std::unique_ptr<QoreIRInstruction> readSwitchInt(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    auto* si = new QoreIRSwitchIntInstruction();
    si->opcode = static_cast<QoreIROpcode>(opcode_raw);
    si->switch_val = QoreIRValue(QoreAOTBinaryReader::readU32(ctx.ptr));
    uint16_t default_idx = QoreAOTBinaryReader::readU16(ctx.ptr);
    si->default_target = ctx.resolveBlock(default_idx);
    uint16_t num_cases = QoreAOTBinaryReader::readU16(ctx.ptr);
    si->cases.reserve(num_cases);
    for (int j = 0; j < num_cases; ++j) {
        int64_t value = QoreAOTBinaryReader::readI64(ctx.ptr);
        uint16_t target_idx = QoreAOTBinaryReader::readU16(ctx.ptr);
        si->cases.push_back({value, ctx.resolveBlock(target_idx)});
    }
    si->result = QoreIRValue(result_id);
    si->operands = operands;
    si->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(si);
}

// ============================================================================
// Group 21: SwitchString - Variable-length string switch table
// ============================================================================

static bool writeSwitchString(AOTInstWriteCtx& ctx) {
    auto* si = static_cast<const QoreIRSwitchStringInstruction*>(ctx.inst);
    ctx.writer.writeU32(si->switch_val.id);
    auto it_d = ctx.block_idx.find(si->default_target);
    ctx.writer.writeU16(it_d != ctx.block_idx.end() ? it_d->second : 0xFFFF);
    ctx.writer.writeU16(static_cast<uint16_t>(si->cases.size()));
    for (auto& c : si->cases) {
        ctx.writer.writeStringRef(c.value.c_str());
        auto it = ctx.block_idx.find(c.target);
        ctx.writer.writeU16(it != ctx.block_idx.end() ? it->second : 0xFFFF);
    }
    return true;
}

static std::unique_ptr<QoreIRInstruction> readSwitchString(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    auto* si = new QoreIRSwitchStringInstruction();
    si->opcode = static_cast<QoreIROpcode>(opcode_raw);
    si->switch_val = QoreIRValue(QoreAOTBinaryReader::readU32(ctx.ptr));
    uint16_t default_idx = QoreAOTBinaryReader::readU16(ctx.ptr);
    si->default_target = ctx.resolveBlock(default_idx);
    uint16_t num_cases = QoreAOTBinaryReader::readU16(ctx.ptr);
    si->cases.reserve(num_cases);
    for (int j = 0; j < num_cases; ++j) {
        const char* value = ctx.reader.readStringRef(ctx.ptr);
        uint16_t target_idx = QoreAOTBinaryReader::readU16(ctx.ptr);
        si->cases.push_back({value ? value : "", ctx.resolveBlock(target_idx)});
    }
    si->result = QoreIRValue(result_id);
    si->operands = operands;
    si->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(si);
}

// ============================================================================
// Group 22: Phi - Phi node with variable-length incoming list
// ============================================================================

static bool writePhi(AOTInstWriteCtx& ctx) {
    auto* pi = static_cast<const QoreIRPhiInstruction*>(ctx.inst);
    ctx.writer.writeU16(static_cast<uint16_t>(pi->incoming.size()));
    for (auto& inc : pi->incoming) {
        ctx.writer.writeU32(inc.value.id);
        auto it = ctx.block_idx.find(inc.block);
        ctx.writer.writeU16(it != ctx.block_idx.end() ? it->second : 0xFFFF);
    }
    return true;
}

static std::unique_ptr<QoreIRInstruction> readPhi(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    auto* pi = new QoreIRPhiInstruction();
    pi->opcode = static_cast<QoreIROpcode>(opcode_raw);
    uint16_t num_incoming = QoreAOTBinaryReader::readU16(ctx.ptr);
    pi->incoming.reserve(num_incoming);
    for (int j = 0; j < num_incoming; ++j) {
        uint32_t val_id = QoreAOTBinaryReader::readU32(ctx.ptr);
        uint16_t block_idx = QoreAOTBinaryReader::readU16(ctx.ptr);
        pi->incoming.push_back({QoreIRValue(val_id), ctx.resolveBlock(block_idx)});
    }
    pi->result = QoreIRValue(result_id);
    pi->operands = operands;
    pi->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(pi);
}

// ============================================================================
// Group 23: Guard - Speculative guard with deopt target
// ============================================================================

static bool writeGuard(AOTInstWriteCtx& ctx) {
    auto* gi = static_cast<const QoreIRGuardInstruction*>(ctx.inst);
    auto it = ctx.block_idx.find(gi->deopt_target);
    ctx.writer.writeU16(it != ctx.block_idx.end() ? it->second : 0xFFFF);
    ctx.writer.writeStringRef(gi->type_info ? QoreTypeInfo::getPath(gi->type_info) : "");
    ctx.writer.writeU32(gi->guard_id);
    return true;
}

static std::unique_ptr<QoreIRInstruction> readGuard(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    auto* gi = new QoreIRGuardInstruction(static_cast<QoreIROpcode>(opcode_raw));
    uint16_t deopt_idx = QoreAOTBinaryReader::readU16(ctx.ptr);
    gi->deopt_target = ctx.resolveBlock(deopt_idx);
    const char* type_path = ctx.reader.readStringRef(ctx.ptr);
    gi->guard_id = QoreAOTBinaryReader::readU32(ctx.ptr);
    if (type_path && *type_path) {
        std::string type_error;
        QoreAOTTypeResolver type_resolver(ctx.pgm);
        gi->type_info = type_resolver.resolve(type_path, type_error);
    }
    gi->result = QoreIRValue(result_id);
    gi->operands = operands;
    gi->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(gi);
}

// ============================================================================
// Group 24: ImplicitArg - Implicit argument access
// ============================================================================

static bool writeImplicitArg(AOTInstWriteCtx& ctx) {
    auto* ii = static_cast<const QoreIRImplicitArgInstruction*>(ctx.inst);
    ctx.writer.writeU16(static_cast<uint16_t>(ii->offset));
    return true;
}

static std::unique_ptr<QoreIRInstruction> readImplicitArg(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    uint16_t offset = QoreAOTBinaryReader::readU16(ctx.ptr);
    auto inst = std::make_unique<QoreIRImplicitArgInstruction>(static_cast<int>(offset));
    inst->result = QoreIRValue(result_id);
    inst->operands = operands;
    inst->exception_target = exc_target;
    return inst;
}

// ============================================================================
// Group 25: HashKeyAccess - Hash key access by name
// ============================================================================

static bool writeHashKeyAccess(AOTInstWriteCtx& ctx) {
    auto* hi = static_cast<const QoreIRHashKeyAccessInstruction*>(ctx.inst);
    ctx.writer.writeStringRef(hi->key_name.c_str());
    return true;
}

static std::unique_ptr<QoreIRInstruction> readHashKeyAccess(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    const char* key_name = ctx.reader.readStringRef(ctx.ptr);
    auto inst = std::make_unique<QoreIRHashKeyAccessInstruction>(key_name ? key_name : "", static_cast<QoreIROpcode>(opcode_raw));
    inst->result = QoreIRValue(result_id);
    inst->operands = operands;
    inst->exception_target = exc_target;
    return inst;
}

// ============================================================================
// Group 26: SelfMember - Member access on self
// ============================================================================

static bool writeSelfMember(AOTInstWriteCtx& ctx) {
    auto* si = static_cast<const QoreIRSelfMemberInstruction*>(ctx.inst);
    ctx.writer.writeStringRef(si->member_name.c_str());
    return true;
}

static std::unique_ptr<QoreIRInstruction> readSelfMember(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    const char* member_name = ctx.reader.readStringRef(ctx.ptr);
    auto* si = new QoreIRSelfMemberInstruction(member_name ? member_name : "");
    si->opcode = static_cast<QoreIROpcode>(opcode_raw);
    si->result = QoreIRValue(result_id);
    si->operands = operands;
    si->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(si);
}

// ============================================================================
// Group 27: StaticVar - Static variable access with expression
// ============================================================================

static bool writeStaticVar(AOTInstWriteCtx& ctx) {
    auto* si = static_cast<const QoreIRStaticVarInstruction*>(ctx.inst);
    ctx.writer.writeStringRef(si->var_name.c_str());
    if (!ctx.writeExpr(ctx.writer, si->expr)) {
        return false;
    }
    return true;
}

static std::unique_ptr<QoreIRInstruction> readStaticVar(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    const char* var_name = ctx.reader.readStringRef(ctx.ptr);
    std::string error;
    QoreValue expr = ctx.readExpr(ctx.reader, ctx.ptr, ctx.end, error);
    if (!error.empty()) {
        return nullptr;
    }
    auto* si = new QoreIRStaticVarInstruction(nullptr, var_name ? var_name : "", expr);
    si->opcode = static_cast<QoreIROpcode>(opcode_raw);
    expr.discard(nullptr);
    si->result = QoreIRValue(result_id);
    si->operands = operands;
    si->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(si);
}

// ============================================================================
// Group 28: NewObject - Object construction
// ============================================================================

static bool writeNewObject(AOTInstWriteCtx& ctx) {
    auto* ni = static_cast<const QoreIRNewObjectInstruction*>(ctx.inst);
    if (!ctx.writeExpr(ctx.writer, ni->expr)) {
        return false;
    }
    return true;
}

static std::unique_ptr<QoreIRInstruction> readNewObject(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    std::string error;
    QoreValue expr = ctx.readExpr(ctx.reader, ctx.ptr, ctx.end, error);
    if (!error.empty()) {
        return nullptr;
    }
    auto* ni = new QoreIRNewObjectInstruction(nullptr, nullptr, nullptr, expr);
    ni->opcode = static_cast<QoreIROpcode>(opcode_raw);
    expr.discard(nullptr);
    ni->result = QoreIRValue(result_id);
    ni->operands = operands;
    ni->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(ni);
}

// ============================================================================
// Group 29: LoadConst - Constant loading
// ============================================================================

static bool writeLoadConst(AOTInstWriteCtx& ctx) {
    auto* lci = static_cast<const QoreIRLoadConstantInstruction*>(ctx.inst);
    if (!ctx.writeExpr(ctx.writer, lci->expr)) {
        return false;
    }
    return true;
}

static std::unique_ptr<QoreIRInstruction> readLoadConst(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    std::string error;
    QoreValue expr = ctx.readExpr(ctx.reader, ctx.ptr, ctx.end, error);
    if (!error.empty()) {
        return nullptr;
    }
    auto* lci = new QoreIRLoadConstantInstruction(nullptr, expr);
    lci->opcode = static_cast<QoreIROpcode>(opcode_raw);
    expr.discard(nullptr);
    lci->result = QoreIRValue(result_id);
    lci->operands = operands;
    lci->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(lci);
}

// ============================================================================
// Group 30: CreateClosure - Closure creation
// ============================================================================

static bool writeCreateClosure(AOTInstWriteCtx& ctx) {
    auto* cci = static_cast<const QoreIRCreateClosureInstruction*>(ctx.inst);
    if (!ctx.writeExpr(ctx.writer, cci->expr)) {
        return false;
    }
    return true;
}

static std::unique_ptr<QoreIRInstruction> readCreateClosure(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    std::string error;
    QoreValue expr = ctx.readExpr(ctx.reader, ctx.ptr, ctx.end, error);
    if (!error.empty()) {
        return nullptr;
    }
    auto* cci = new QoreIRCreateClosureInstruction(nullptr, expr);
    cci->opcode = static_cast<QoreIROpcode>(opcode_raw);
    expr.discard(nullptr);
    cci->result = QoreIRValue(result_id);
    cci->operands = operands;
    cci->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(cci);
}

// ============================================================================
// Group 31: CreateCallRef - Call reference creation
// ============================================================================

static bool writeCreateCallRef(AOTInstWriteCtx& ctx) {
    auto* cri = static_cast<const QoreIRCreateCallRefInstruction*>(ctx.inst);
    if (!ctx.writeExpr(ctx.writer, cri->expr)) {
        return false;
    }
    return true;
}

static std::unique_ptr<QoreIRInstruction> readCreateCallRef(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    std::string error;
    QoreValue expr = ctx.readExpr(ctx.reader, ctx.ptr, ctx.end, error);
    if (!error.empty()) {
        return nullptr;
    }
    auto* cri = new QoreIRCreateCallRefInstruction(expr);
    cri->opcode = static_cast<QoreIROpcode>(opcode_raw);
    expr.discard(nullptr);
    cri->result = QoreIRValue(result_id);
    cri->operands = operands;
    cri->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(cri);
}

// ============================================================================
// Group 32: CreateMethodRef - Method reference creation
// ============================================================================

static bool writeCreateMethodRef(AOTInstWriteCtx& ctx) {
    auto* cri = static_cast<const QoreIRCreateMethodRefInstruction*>(ctx.inst);
    if (!ctx.writeExpr(ctx.writer, cri->expr)) {
        return false;
    }
    return true;
}

static std::unique_ptr<QoreIRInstruction> readCreateMethodRef(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    std::string error;
    QoreValue expr = ctx.readExpr(ctx.reader, ctx.ptr, ctx.end, error);
    if (!error.empty()) {
        return nullptr;
    }
    auto* cri = new QoreIRCreateMethodRefInstruction(expr);
    cri->opcode = static_cast<QoreIROpcode>(opcode_raw);
    expr.discard(nullptr);
    cri->result = QoreIRValue(result_id);
    cri->operands = operands;
    cri->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(cri);
}

// ============================================================================
// Group 33: CreateParseRef - Parse reference creation
// ============================================================================

static bool writeCreateParseRef(AOTInstWriteCtx& ctx) {
    auto* cri = static_cast<const QoreIRCreateParseRefInstruction*>(ctx.inst);
    if (!ctx.writeExpr(ctx.writer, cri->expr)) {
        return false;
    }
    return true;
}

static std::unique_ptr<QoreIRInstruction> readCreateParseRef(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    std::string error;
    QoreValue expr = ctx.readExpr(ctx.reader, ctx.ptr, ctx.end, error);
    if (!error.empty()) {
        return nullptr;
    }
    auto* cri = new QoreIRCreateParseRefInstruction(nullptr, expr);
    cri->opcode = static_cast<QoreIROpcode>(opcode_raw);
    expr.discard(nullptr);
    cri->result = QoreIRValue(result_id);
    cri->operands = operands;
    cri->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(cri);
}

// ============================================================================
// Group 34: NewHashDecl - Hash decl instantiation
// ============================================================================

static bool writeNewHashDecl(AOTInstWriteCtx& ctx) {
    auto* ni = static_cast<const QoreIRNewHashDeclInstruction*>(ctx.inst);
    if (!ctx.writeExpr(ctx.writer, ni->expr)) {
        return false;
    }
    return true;
}

static std::unique_ptr<QoreIRInstruction> readNewHashDecl(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    std::string error;
    QoreValue expr = ctx.readExpr(ctx.reader, ctx.ptr, ctx.end, error);
    if (!error.empty()) {
        return nullptr;
    }
    auto* ni = new QoreIRNewHashDeclInstruction(nullptr, expr);
    ni->opcode = static_cast<QoreIROpcode>(opcode_raw);
    expr.discard(nullptr);
    ni->result = QoreIRValue(result_id);
    ni->operands = operands;
    ni->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(ni);
}

// ============================================================================
// Group 35: NewComplexHash - Complex hash creation
// ============================================================================

static bool writeNewComplexHash(AOTInstWriteCtx& ctx) {
    auto* ni = static_cast<const QoreIRNewComplexHashInstruction*>(ctx.inst);
    if (!ctx.writeExpr(ctx.writer, ni->expr)) {
        return false;
    }
    return true;
}

static std::unique_ptr<QoreIRInstruction> readNewComplexHash(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    std::string error;
    QoreValue expr = ctx.readExpr(ctx.reader, ctx.ptr, ctx.end, error);
    if (!error.empty()) {
        return nullptr;
    }
    auto* ni = new QoreIRNewComplexHashInstruction(nullptr, expr);
    ni->opcode = static_cast<QoreIROpcode>(opcode_raw);
    expr.discard(nullptr);
    ni->result = QoreIRValue(result_id);
    ni->operands = operands;
    ni->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(ni);
}

// ============================================================================
// Group 36: NewComplexList - Complex list creation
// ============================================================================

static bool writeNewComplexList(AOTInstWriteCtx& ctx) {
    auto* ni = static_cast<const QoreIRNewComplexListInstruction*>(ctx.inst);
    if (!ctx.writeExpr(ctx.writer, ni->expr)) {
        return false;
    }
    return true;
}

static std::unique_ptr<QoreIRInstruction> readNewComplexList(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    std::string error;
    QoreValue expr = ctx.readExpr(ctx.reader, ctx.ptr, ctx.end, error);
    if (!error.empty()) {
        return nullptr;
    }
    auto* ni = new QoreIRNewComplexListInstruction(nullptr, expr);
    ni->opcode = static_cast<QoreIROpcode>(opcode_raw);
    expr.discard(nullptr);
    ni->result = QoreIRValue(result_id);
    ni->operands = operands;
    ni->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(ni);
}

// ============================================================================
// Group 37: VrnConstruct - Variant value construction
// ============================================================================

static bool writeVrnConstruct(AOTInstWriteCtx& ctx) {
    auto* vi = static_cast<const QoreIRVrnConstructInstruction*>(ctx.inst);
    if (!ctx.writeExpr(ctx.writer, vi->expr)) {
        return false;
    }
    return true;
}

static std::unique_ptr<QoreIRInstruction> readVrnConstruct(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    std::string error;
    QoreValue expr = ctx.readExpr(ctx.reader, ctx.ptr, ctx.end, error);
    if (!error.empty()) {
        return nullptr;
    }
    auto* vi = new QoreIRVrnConstructInstruction(nullptr, expr);
    vi->opcode = static_cast<QoreIROpcode>(opcode_raw);
    expr.discard(nullptr);
    vi->result = QoreIRValue(result_id);
    vi->operands = operands;
    vi->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(vi);
}

// ============================================================================
// Group 38: HashKeyStore - Hash key storage
// ============================================================================

static bool writeHashKeyStore(AOTInstWriteCtx& ctx) {
    auto* hi = static_cast<const QoreIRHashKeyStoreInstruction*>(ctx.inst);
    ctx.writer.writeStringRef(hi->key_name.c_str());
    ctx.writer.writeU32(hi->container_slot_id);
    return true;
}

static std::unique_ptr<QoreIRInstruction> readHashKeyStore(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    const char* key_name = ctx.reader.readStringRef(ctx.ptr);
    uint32_t container_slot_id = QoreAOTBinaryReader::readU32(ctx.ptr);
    auto* hi = new QoreIRHashKeyStoreInstruction(nullptr, key_name ? key_name : "");
    hi->opcode = static_cast<QoreIROpcode>(opcode_raw);
    hi->container_slot_id = container_slot_id;
    hi->result = QoreIRValue(result_id);
    hi->operands = operands;
    hi->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(hi);
}

// ============================================================================
// Group 39: ListIndexStore - List index storage
// ============================================================================

static bool writeListIndexStore(AOTInstWriteCtx& ctx) {
    auto* li = static_cast<const QoreIRListIndexStoreInstruction*>(ctx.inst);
    ctx.writer.writeU32(li->container_slot_id);
    return true;
}

static std::unique_ptr<QoreIRInstruction> readListIndexStore(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    uint32_t container_slot_id = QoreAOTBinaryReader::readU32(ctx.ptr);
    auto* li = new QoreIRListIndexStoreInstruction(nullptr);
    li->opcode = static_cast<QoreIROpcode>(opcode_raw);
    li->container_slot_id = container_slot_id;
    li->result = QoreIRValue(result_id);
    li->operands = operands;
    li->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(li);
}

// ============================================================================
// Group 40: FusedAddLocal - Fused += operation on local int
// ============================================================================

static bool writeFusedAddLocal(AOTInstWriteCtx& ctx) {
    auto* fi = static_cast<const QoreIRAddAssignLocalIntInstruction*>(ctx.inst);
    ctx.writer.writeStringRef(fi->target ? fi->target->getName() : "");
    ctx.writer.writeStringRef(fi->source ? fi->source->getName() : "");
    ctx.writer.writeU32(fi->target_slot_id);
    ctx.writer.writeU32(fi->source_slot_id);
    ctx.writer.writeU8(fi->target_ir_only ? 1 : 0);
    return true;
}

static std::unique_ptr<QoreIRInstruction> readFusedAddLocal(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    const char* target_name = ctx.reader.readStringRef(ctx.ptr);
    const char* source_name = ctx.reader.readStringRef(ctx.ptr);
    uint32_t target_slot_id = QoreAOTBinaryReader::readU32(ctx.ptr);
    uint32_t source_slot_id = QoreAOTBinaryReader::readU32(ctx.ptr);
    bool target_ir_only = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;
    LocalVar* target_lv = ctx.resolveLocal(target_name);
    LocalVar* source_lv = ctx.resolveLocal(source_name);
    auto* fi = new QoreIRAddAssignLocalIntInstruction(target_lv, source_lv);
    fi->target_slot_id = target_slot_id;
    fi->source_slot_id = source_slot_id;
    fi->target_ir_only = target_ir_only;
    fi->result = QoreIRValue(result_id);
    fi->operands = operands;
    fi->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(fi);
}

// ============================================================================
// Group 41: FusedIncLocal - Fused ++ or += on local int
// ============================================================================

static bool writeFusedIncLocal(AOTInstWriteCtx& ctx) {
    auto* fi = static_cast<const QoreIRIncrementLocalIntInstruction*>(ctx.inst);
    ctx.writer.writeStringRef(fi->local ? fi->local->getName() : "");
    ctx.writer.writeI64(fi->delta);
    ctx.writer.writeU32(fi->slot_id);
    ctx.writer.writeU8(fi->ir_only ? 1 : 0);
    return true;
}

static std::unique_ptr<QoreIRInstruction> readFusedIncLocal(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    const char* local_name = ctx.reader.readStringRef(ctx.ptr);
    int64_t delta = QoreAOTBinaryReader::readI64(ctx.ptr);
    uint32_t slot_id = QoreAOTBinaryReader::readU32(ctx.ptr);
    bool ir_only = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;
    LocalVar* lv = ctx.resolveLocal(local_name);
    auto* fi = new QoreIRIncrementLocalIntInstruction(lv, delta);
    fi->slot_id = slot_id;
    fi->ir_only = ir_only;
    fi->result = QoreIRValue(result_id);
    fi->operands = operands;
    fi->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(fi);
}

// ============================================================================
// Group 42: FusedBrLtLocal - Fused branch-if-less-than on local ints
// ============================================================================

static bool writeFusedBrLtLocal(AOTInstWriteCtx& ctx) {
    auto* fi = static_cast<const QoreIRBranchIfLtLocalIntInstruction*>(ctx.inst);
    ctx.writer.writeStringRef(fi->lhs ? fi->lhs->getName() : "");
    ctx.writer.writeStringRef(fi->rhs ? fi->rhs->getName() : "");
    ctx.writer.writeU32(fi->lhs_slot_id);
    ctx.writer.writeU32(fi->rhs_slot_id);
    auto it_t = ctx.block_idx.find(fi->true_target);
    ctx.writer.writeU16(it_t != ctx.block_idx.end() ? it_t->second : 0xFFFF);
    auto it_f = ctx.block_idx.find(fi->false_target);
    ctx.writer.writeU16(it_f != ctx.block_idx.end() ? it_f->second : 0xFFFF);
    return true;
}

static std::unique_ptr<QoreIRInstruction> readFusedBrLtLocal(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    const char* lhs_name = ctx.reader.readStringRef(ctx.ptr);
    const char* rhs_name = ctx.reader.readStringRef(ctx.ptr);
    uint32_t lhs_slot_id = QoreAOTBinaryReader::readU32(ctx.ptr);
    uint32_t rhs_slot_id = QoreAOTBinaryReader::readU32(ctx.ptr);
    uint16_t true_idx = QoreAOTBinaryReader::readU16(ctx.ptr);
    uint16_t false_idx = QoreAOTBinaryReader::readU16(ctx.ptr);
    LocalVar* lhs_lv = ctx.resolveLocal(lhs_name);
    LocalVar* rhs_lv = ctx.resolveLocal(rhs_name);
    auto* fi = new QoreIRBranchIfLtLocalIntInstruction(
        lhs_lv, rhs_lv, ctx.resolveBlock(true_idx), ctx.resolveBlock(false_idx));
    fi->lhs_slot_id = lhs_slot_id;
    fi->rhs_slot_id = rhs_slot_id;
    fi->result = QoreIRValue(result_id);
    fi->operands = operands;
    fi->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(fi);
}

// ============================================================================
// Group 43: MapHashKey - Hash key mapping operation
// ============================================================================

static bool writeMapHashKey(AOTInstWriteCtx& ctx) {
    auto* mi = static_cast<const QoreIRMapHashKeyInstruction*>(ctx.inst);
    ctx.writer.writeStringRef(mi->key1.c_str());
    ctx.writer.writeStringRef(mi->key2.c_str());
    return true;
}

static std::unique_ptr<QoreIRInstruction> readMapHashKey(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    const char* key1 = ctx.reader.readStringRef(ctx.ptr);
    const char* key2 = ctx.reader.readStringRef(ctx.ptr);
    auto inst = std::make_unique<QoreIRMapHashKeyInstruction>(static_cast<QoreIROpcode>(opcode_raw),
        key1 ? key1 : "", key2 ? key2 : "");
    inst->result = QoreIRValue(result_id);
    inst->operands = operands;
    inst->exception_target = exc_target;
    return inst;
}

// ============================================================================
// Group 44: IteratorCreate - Iterator creation from iterable
// ============================================================================

static bool writeIteratorCreate(AOTInstWriteCtx& ctx) {
    auto* ii = static_cast<const QoreIRIteratorCreateInstruction*>(ctx.inst);
    ctx.writer.writeU32(ii->iterable.id);
    return true;
}

static std::unique_ptr<QoreIRInstruction> readIteratorCreate(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    uint32_t iterable_id = QoreAOTBinaryReader::readU32(ctx.ptr);
    auto* ii = new QoreIRIteratorCreateInstruction(QoreIRValue(iterable_id));
    ii->opcode = static_cast<QoreIROpcode>(opcode_raw);
    ii->result = QoreIRValue(result_id);
    ii->operands = operands;
    ii->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(ii);
}

// ============================================================================
// Group 45: IteratorNext - Iterator advance with two targets
// ============================================================================

static bool writeIteratorNext(AOTInstWriteCtx& ctx) {
    auto* ii = static_cast<const QoreIRIteratorNextInstruction*>(ctx.inst);
    ctx.writer.writeU32(ii->iterator.id);
    auto it_d = ctx.block_idx.find(ii->done_target);
    ctx.writer.writeU16(it_d != ctx.block_idx.end() ? it_d->second : 0xFFFF);
    auto it_c = ctx.block_idx.find(ii->continue_target);
    ctx.writer.writeU16(it_c != ctx.block_idx.end() ? it_c->second : 0xFFFF);
    return true;
}

static std::unique_ptr<QoreIRInstruction> readIteratorNext(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    uint32_t iterator_id = QoreAOTBinaryReader::readU32(ctx.ptr);
    uint16_t done_idx = QoreAOTBinaryReader::readU16(ctx.ptr);
    uint16_t continue_idx = QoreAOTBinaryReader::readU16(ctx.ptr);
    auto inst = std::make_unique<QoreIRIteratorNextInstruction>(
        QoreIRValue(iterator_id), ctx.resolveBlock(done_idx), ctx.resolveBlock(continue_idx));
    inst->result = QoreIRValue(result_id);
    inst->operands = operands;
    inst->exception_target = exc_target;
    return inst;
}

// ============================================================================
// Group 46: RefForeachInit - Reference foreach initialization
// ============================================================================

static bool writeRefForeachInit(AOTInstWriteCtx& ctx) {
    auto* ri = static_cast<const QoreIRRefForeachInitInstruction*>(ctx.inst);
    if (!ctx.writeExpr(ctx.writer, ri->expr)) {
        return false;
    }
    return true;
}

static std::unique_ptr<QoreIRInstruction> readRefForeachInit(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    std::string error;
    QoreValue expr = ctx.readExpr(ctx.reader, ctx.ptr, ctx.end, error);
    if (!error.empty()) {
        return nullptr;
    }
    auto* ri = new QoreIRRefForeachInitInstruction(expr);
    ri->opcode = static_cast<QoreIROpcode>(opcode_raw);
    expr.discard(nullptr);
    ri->result = QoreIRValue(result_id);
    ri->operands = operands;
    ri->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(ri);
}

// ============================================================================
// Group 47: SwitchRegexMatch - Regex match case
// ============================================================================

static bool writeSwitchRegexMatch(AOTInstWriteCtx& ctx) {
    auto* sri = static_cast<const QoreIRSwitchRegexMatchInstruction*>(ctx.inst);
    if (!sri->regex_case) {
        return false;
    }
    QoreRegex* re = sri->regex_case->getRegex();
    if (!re || !re->getPatternCStr()) {
        return false;
    }
    ctx.writer.writeStringRef(re->getPatternCStr());
    ctx.writer.writeI64(re->getOptions());
    ctx.writer.writeU8(dynamic_cast<const CaseNodeNegRegex*>(sri->regex_case) ? 1 : 0);
    return true;
}

static std::unique_ptr<QoreIRInstruction> readSwitchRegexMatch(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    const char* pattern = ctx.reader.readStringRef(ctx.ptr);
    int64_t options = QoreAOTBinaryReader::readI64(ctx.ptr);
    bool is_negated = QoreAOTBinaryReader::readU8(ctx.ptr) != 0;
    ExceptionSink xsink;
    QoreRegex* re = new QoreRegex(pattern ? pattern : "", options, &xsink);
    if (xsink) {
        delete re;
        return nullptr;
    }
    const CaseNodeRegex* cnode = is_negated
        ? new CaseNodeNegRegex(&loc_builtin, re, nullptr)
        : new CaseNodeRegex(&loc_builtin, re, nullptr);
    auto* sri = new QoreIRSwitchRegexMatchInstruction(cnode);
    sri->opcode = static_cast<QoreIROpcode>(opcode_raw);
    sri->owns_regex_case = true;
    sri->result = QoreIRValue(result_id);
    sri->operands = operands;
    sri->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(sri);
}

// ============================================================================
// Group 49: MakeHashConstKeys - Variable-length const key list
// ============================================================================

static bool writeMakeHashConstKeys(AOTInstWriteCtx& ctx) {
    auto* mhck = static_cast<const QoreIRMakeHashConstKeysInstruction*>(ctx.inst);
    ctx.writer.writeU16(static_cast<uint16_t>(mhck->keys.size()));
    for (const auto& key : mhck->keys) {
        ctx.writer.writeStringRef(key.c_str());
    }
    return true;
}

static std::unique_ptr<QoreIRInstruction> readMakeHashConstKeys(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    uint16_t key_count = QoreAOTBinaryReader::readU16(ctx.ptr);
    std::vector<std::string> keys;
    keys.reserve(key_count);
    for (uint16_t k = 0; k < key_count; ++k) {
        const char* key = ctx.reader.readStringRef(ctx.ptr);
        keys.push_back(key ? key : "");
    }
    auto inst = std::make_unique<QoreIRMakeHashConstKeysInstruction>(std::move(keys));
    inst->result = QoreIRValue(result_id);
    inst->operands = operands;
    inst->exception_target = exc_target;
    return inst;
}

// ============================================================================
// Group 50: SwitchCaseMatch - Switch case matching with optional value
// ============================================================================

static bool writeSwitchCaseMatch(AOTInstWriteCtx& ctx) {
    auto* scm = static_cast<const QoreIRSwitchCaseMatchInstruction*>(ctx.inst);
    if (scm->case_node && scm->case_node->val) {
        ctx.writer.writeU8(1);  // has_val
        if (!ctx.writeExpr(ctx.writer, scm->case_node->val)) {
            return false;
        }
    } else {
        ctx.writer.writeU8(0);  // no val (default case)
    }
    return true;
}

static std::unique_ptr<QoreIRInstruction> readSwitchCaseMatch(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    uint8_t has_val = QoreAOTBinaryReader::readU8(ctx.ptr);
    QoreValue case_val;
    std::string error;
    if (has_val) {
        case_val = ctx.readExpr(ctx.reader, ctx.ptr, ctx.end, error);
        if (!error.empty()) {
            return nullptr;
        }
    }
    auto* cnode = new CaseNode(&loc_builtin, case_val, nullptr);
    auto* scm = new QoreIRSwitchCaseMatchInstruction(cnode);
    scm->owns_case_node = true;
    scm->result = QoreIRValue(result_id);
    scm->operands = operands;
    scm->exception_target = exc_target;
    return std::unique_ptr<QoreIRInstruction>(scm);
}

// ============================================================================
// Group 51: ListIndexAccess - List index access (base operands only)
// ============================================================================

static bool writeListIndexAccess(AOTInstWriteCtx& ctx) {
    // No extra fields beyond base operands
    return true;
}

static std::unique_ptr<QoreIRInstruction> readListIndexAccess(
        uint16_t opcode_raw, QoreIRBasicBlock* exc_target,
        const std::vector<QoreIRValue>& operands, uint32_t result_id,
        AOTInstReadCtx& ctx) {
    auto inst = std::make_unique<QoreIRListIndexAccessInstruction>();
    inst->result = QoreIRValue(result_id);
    inst->operands = operands;
    inst->exception_target = exc_target;
    return inst;
}

// ============================================================================
// Instruction Group Registry Table
// ============================================================================

const QoreIRInstGroupInfo AOT_INST_GROUP_REGISTRY[AOT_INST_GROUP_TABLE_SIZE] = {
    // Index 0: Base
    { "Base", 0, true, false, writeBase, readBase, "Base instruction (no group data)" },

    // Index 1: Const
    { "Const", 1, true, false, writeConst, readConst, "Constant value" },

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
    { "Var", 7, true, false, writeVar, readVar, "Global variable reference" },

    // Index 8: LValue
    { "LValue", 8, true, false, writeLValue, readLValue, "LValue (assignable) reference" },

    // Index 9: Expr
    { "Expr", 9, true, false, writeExpr, readExpr, "Generic expression" },

    // Index 10: CallDirect
    { "CallDirect", 10, true, false, writeCallDirect, readCallDirect, "Direct function call" },

    // Index 11: CallMethodDirect
    { "CallMethodDirect", 11, true, false, writeCallMethodDirect, readCallMethodDirect, "Direct method call" },

    // Index 12: InvokeMethodDirect
    { "InvokeMethodDirect", 12, true, false, writeInvokeMethodDirect, readInvokeMethodDirect, "Direct method call with exception handling" },

    // Index 13: CallStaticDirect
    { "CallStaticDirect", 13, true, false, writeCallStaticDirect, readCallStaticDirect, "Static method call" },

    // Index 14: DotEvalMethodDirect
    { "DotEvalMethodDirect", 14, true, false, writeDotEvalMethodDirect, readDotEvalMethodDirect, "Dot-notation method evaluation" },

    // Index 15: InvokeDotEvalMethodDirect
    { "InvokeDotEvalMethodDirect", 15, true, false, writeInvokeDotEvalMethodDirect, readInvokeDotEvalMethodDirect, "Dot-notation method evaluation with exception handling" },

    // Index 16: Invoke
    { "Invoke", 16, true, false, writeInvoke, readInvoke, "Generic method invocation" },

    // Index 17: ScopeEnter
    { "ScopeEnter", 17, true, false, writeScopeEnter, readScopeEnter, "Scope entry tracking" },

    // Index 18: ScopeExit
    { "ScopeExit", 18, true, false, writeScopeExit, readScopeExit, "Scope exit tracking" },

    // Index 19: LandingPad
    { "LandingPad", 19, true, false, writeLandingPad, readLandingPad, "Exception handling landing pad" },

    // Index 20: SwitchInt
    { "SwitchInt", 20, true, false, writeSwitchInt, readSwitchInt, "Switch on integer value" },

    // Index 21: SwitchString
    { "SwitchString", 21, true, false, writeSwitchString, readSwitchString, "Switch on string value" },

    // Index 22: Phi
    { "Phi", 22, true, false, writePhi, readPhi, "Phi node for value merging" },

    // Index 23: Guard
    { "Guard", 23, true, false, writeGuard, readGuard, "Type guard with deoptimization" },

    // Index 24: ImplicitArg
    { "ImplicitArg", 24, true, false, writeImplicitArg, readImplicitArg, "Implicit argument access" },

    // Index 25: HashKeyAccess
    { "HashKeyAccess", 25, true, false, writeHashKeyAccess, readHashKeyAccess, "Hash key access by name" },

    // Index 26: SelfMember
    { "SelfMember", 26, true, false, writeSelfMember, readSelfMember, "Member access on self" },

    // Index 27: StaticVar
    { "StaticVar", 27, true, false, writeStaticVar, readStaticVar, "Static variable access" },

    // Index 28: NewObject
    { "NewObject", 28, true, false, writeNewObject, readNewObject, "Object instantiation" },

    // Index 29: LoadConst
    { "LoadConst", 29, true, false, writeLoadConst, readLoadConst, "Constant loading" },

    // Index 30: CreateClosure
    { "CreateClosure", 30, true, false, writeCreateClosure, readCreateClosure, "Closure creation" },

    // Index 31: CreateCallRef
    { "CreateCallRef", 31, true, false, writeCreateCallRef, readCreateCallRef, "Call reference creation" },

    // Index 32: CreateMethodRef
    { "CreateMethodRef", 32, true, false, writeCreateMethodRef, readCreateMethodRef, "Method reference creation" },

    // Index 33: CreateParseRef
    { "CreateParseRef", 33, true, false, writeCreateParseRef, readCreateParseRef, "Parse reference creation" },

    // Index 34: NewHashDecl
    { "NewHashDecl", 34, true, false, writeNewHashDecl, readNewHashDecl, "Hash declaration instantiation" },

    // Index 35: NewComplexHash
    { "NewComplexHash", 35, true, false, writeNewComplexHash, readNewComplexHash, "Complex hash creation" },

    // Index 36: NewComplexList
    { "NewComplexList", 36, true, false, writeNewComplexList, readNewComplexList, "Complex list creation" },

    // Index 37: VrnConstruct
    { "VrnConstruct", 37, true, false, writeVrnConstruct, readVrnConstruct, "Variant value construction" },

    // Index 38: HashKeyStore
    { "HashKeyStore", 38, true, false, writeHashKeyStore, readHashKeyStore, "Hash key storage" },

    // Index 39: ListIndexStore
    { "ListIndexStore", 39, true, false, writeListIndexStore, readListIndexStore, "List index storage" },

    // Index 40: FusedAddLocal
    { "FusedAddLocal", 40, true, false, writeFusedAddLocal, readFusedAddLocal, "Fused += operation on local" },

    // Index 41: FusedIncLocal
    { "FusedIncLocal", 41, true, false, writeFusedIncLocal, readFusedIncLocal, "Fused ++ operation on local" },

    // Index 42: FusedBrLtLocal
    { "FusedBrLtLocal", 42, true, false, writeFusedBrLtLocal, readFusedBrLtLocal, "Fused branch-if-less-than on locals" },

    // Index 43: MapHashKey
    { "MapHashKey", 43, true, false, writeMapHashKey, readMapHashKey, "Hash key mapping operation" },

    // Index 44: Foreach was removed - preserved as gap for binary compatibility
    #define UNUSED_ENTRY(idx) { nullptr, idx, false, false, nullptr, nullptr, nullptr }
    UNUSED_ENTRY(44),

    // Index 45: OnBlockExit
    { "OnBlockExit", 45, true, true, writeOnBlockExit, readOnBlockExit, "Exception handler with nested IR function" },

    // Index 46: IteratorCreate
    { "IteratorCreate", 46, true, false, writeIteratorCreate, readIteratorCreate, "Iterator creation from iterable" },

    // Index 47: IteratorNext
    { "IteratorNext", 47, true, false, writeIteratorNext, readIteratorNext, "Iterator advance with targets" },

    // Index 48: SwitchRegexMatch
    { "SwitchRegexMatch", 48, true, false, writeSwitchRegexMatch, readSwitchRegexMatch, "Regex pattern matching" },

    // Index 49: RefForeachInit
    { "RefForeachInit", 49, true, false, writeRefForeachInit, readRefForeachInit, "Reference foreach initialization" },

    // Index 50: Debug was removed - preserved as gap for binary compatibility
    UNUSED_ENTRY(50),

    // Index 51: Assert was removed - preserved as gap for binary compatibility
    UNUSED_ENTRY(51),

    // Index 52: MakeHashConstKeys
    { "MakeHashConstKeys", 52, true, false, writeMakeHashConstKeys, readMakeHashConstKeys, "Create hash with constant keys" },

    // Index 53: SwitchCaseMatch
    { "SwitchCaseMatch", 53, true, false, writeSwitchCaseMatch, readSwitchCaseMatch, "Switch case matching" },

    // Index 54: Context holds raw ContextStatement* pointer - no serialization
    UNUSED_ENTRY(54),

    // Index 55: Summarize holds raw SummarizeStatement* pointer - no serialization
    UNUSED_ENTRY(55),

    // Index 56: ListIndexAccess
    { "ListIndexAccess", 56, true, false, writeListIndexAccess, readListIndexAccess, "List index access" },

    // Remaining 57-255: Unsupported/undefined
    UNUSED_ENTRY(57), UNUSED_ENTRY(58), UNUSED_ENTRY(59),
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
