/* -*- indent-tabs-mode: nil -*- */
/*
    QoreIRAnalysis.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    This source is released under the MIT license; see README-LICENSE.
*/

#include <qore/intern/QoreIRAnalysis.h>
#include <qore/intern/LocalVar.h>
#include <qore/intern/QoreJITIncludes.h>
#include <qore/intern/QoreSquareBracketsOperatorNode.h>
#include <qore/intern/typed_hash_decl_private.h>

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <unordered_set>

static bool qore_ir_analysis_cancelled(size_t& count, const char* operation) {
    return ++count % 100 == 0 && qore_check_cancel(nullptr, operation);
}

extern const VarRefNode* extractLValueBaseVarRef(const QoreValue& lvalue);

static bool qore_ir_is_ast_lvalue_mutation(QoreIROpcode opcode) {
    switch (opcode) {
        case QoreIROpcode::PopAny:
        case QoreIROpcode::PushAny:
        case QoreIROpcode::ExtractAny:
        case QoreIROpcode::ExtractList:
        case QoreIROpcode::ExtractString:
        case QoreIROpcode::ExtractBinary:
        case QoreIROpcode::RemoveAny:
        case QoreIROpcode::RemoveList:
        case QoreIROpcode::RemoveHash:
        case QoreIROpcode::RemoveObject:
        case QoreIROpcode::RemoveString:
        case QoreIROpcode::RemoveBinary:
        case QoreIROpcode::RegexSubstAny:
        case QoreIROpcode::RegexSubstString:
        case QoreIROpcode::TrimAny:
        case QoreIROpcode::TrimString:
        case QoreIROpcode::ChompAny:
        case QoreIROpcode::ChompString:
        case QoreIROpcode::TransliterateAny:
        case QoreIROpcode::TransliterateString:
        case QoreIROpcode::ListAssignAny:
            return true;
        default:
            return false;
    }
}

bool qore_ir_get_readonly_scalar_closure_captures(
        const QoreIRFunction& func, const LVarSet* captures,
        std::vector<const LocalVar*>& result) {
    result.clear();
    if (!captures || captures->empty() || captures->size() > 4
            || func.has_opaque_ast_local_access) {
        return false;
    }

    std::unordered_set<const LocalVar*> capture_set;
    capture_set.reserve(captures->size());
    size_t capture_count = 0;
    for (const LocalVar* local : *captures) {
        if (capture_count++ && !(capture_count % 100)
                && qore_check_cancel(nullptr,
                    "IR read-only closure capture classification")) {
            result.clear();
            return false;
        }
        const QoreTypeInfo* type = local ? local->getTypeInfo() : nullptr;
        bool exact_scalar = type
            && !QoreTypeInfo::isReference(type)
            && !QoreTypeInfo::parseAcceptsReturns(type, NT_NOTHING)
            && ((QoreTypeInfo::isType(type, NT_INT)
                    && !QoreTypeInfo::getReturnEnum(type))
                || QoreTypeInfo::isType(type, NT_FLOAT)
                || QoreTypeInfo::isType(type, NT_BOOLEAN));
        if (!exact_scalar) {
            result.clear();
            return false;
        }
        capture_set.insert(local);
        result.push_back(local);
    }

    std::unordered_set<const LocalVar*> loaded;
    size_t instruction_count = 0;
    for (const auto& block : func.blocks) {
        for (const auto& inst_ptr : block->instructions) {
            if (instruction_count++ && !(instruction_count % 100)
                    && qore_check_cancel(nullptr,
                        "IR read-only closure capture use analysis")) {
                result.clear();
                return false;
            }
            const QoreIRInstruction* inst = inst_ptr.get();
            if (!inst) {
                continue;
            }
            QoreIROpcode effective_opcode = inst->opcode;
            if (inst->opcode == QoreIROpcode::Invoke) {
                effective_opcode = static_cast<const QoreIRInvokeInstruction*>(inst)
                    ->invoke_opcode;
            }
            if (qore_ir_is_ast_lvalue_mutation(effective_opcode)) {
                result.clear();
                return false;
            }
            if (inst->opcode == QoreIROpcode::CreateClosure
                    || inst->opcode == QoreIROpcode::CreateParseRef
                    || (inst->opcode == QoreIROpcode::Invoke
                        && static_cast<const QoreIRInvokeInstruction*>(inst)
                            ->invoke_opcode == QoreIROpcode::CreateParseRef)) {
                result.clear();
                return false;
            }
            if (inst->opcode == QoreIROpcode::LoadClosure
                    || inst->opcode == QoreIROpcode::StoreClosure
                    || inst->opcode == QoreIROpcode::LoadLocal
                    || inst->opcode == QoreIROpcode::StoreLocal
                    || inst->opcode == QoreIROpcode::InstantiateLocal
                    || inst->opcode == QoreIROpcode::UninstantiateLocal) {
                const auto* local_inst = static_cast<const QoreIRLocalInstruction*>(inst);
                if (!capture_set.count(local_inst->local)) {
                    continue;
                }
                if (inst->opcode != QoreIROpcode::LoadClosure
                        || local_inst->is_ref || local_inst->weak) {
                    result.clear();
                    return false;
                }
                loaded.insert(local_inst->local);
                continue;
            }
            const LocalVar* mutated = nullptr;
            if (inst->opcode == QoreIROpcode::AddAssignLocalInt) {
                mutated = static_cast<const QoreIRAddAssignLocalIntInstruction*>(inst)->target;
            } else if (inst->opcode == QoreIROpcode::IncrementLocalInt) {
                mutated = static_cast<const QoreIRIncrementLocalIntInstruction*>(inst)->local;
            } else if (inst->opcode == QoreIROpcode::LValuePathAssign
                    || inst->opcode == QoreIROpcode::LValuePathCompound
                    || inst->opcode == QoreIROpcode::LValuePathUnary
                    || inst->opcode == QoreIROpcode::LValuePathBinaryMut
                    || inst->opcode == QoreIROpcode::LValuePathTernary) {
                const auto* path = static_cast<const QoreIRLValuePathInstruction*>(inst);
                if (!path->path.empty()
                        && (path->path.front().kind == LVPathStepKind::LocalVar
                            || path->path.front().kind == LVPathStepKind::ClosureVar)) {
                    mutated = reinterpret_cast<const LocalVar*>(path->path.front().ref_ptr);
                }
            } else if (inst->opcode == QoreIROpcode::StoreLValue
                    || inst->opcode == QoreIROpcode::PreIncLValue
                    || inst->opcode == QoreIROpcode::PreDecLValue
                    || inst->opcode == QoreIROpcode::PostIncLValue
                    || inst->opcode == QoreIROpcode::PostDecLValue
                    || inst->opcode == QoreIROpcode::AddAssignLValue
                    || inst->opcode == QoreIROpcode::SubAssignLValue
                    || inst->opcode == QoreIROpcode::MulAssignLValue
                    || inst->opcode == QoreIROpcode::DivAssignLValue
                    || inst->opcode == QoreIROpcode::ModAssignLValue
                    || inst->opcode == QoreIROpcode::AndAssignLValue
                    || inst->opcode == QoreIROpcode::OrAssignLValue
                    || inst->opcode == QoreIROpcode::XorAssignLValue
                    || inst->opcode == QoreIROpcode::ShlAssignLValue
                    || inst->opcode == QoreIROpcode::ShrAssignLValue
                    || inst->opcode == QoreIROpcode::ShiftLValue
                    || inst->opcode == QoreIROpcode::UnshiftLValue
                    || inst->opcode == QoreIROpcode::SpliceLValue) {
                const auto* lvalue = static_cast<const QoreIRLValueInstruction*>(inst);
                const VarRefNode* base = extractLValueBaseVarRef(lvalue->lvalue);
                mutated = base && base->getType() == VT_LOCAL ? base->ref.id : nullptr;
            }
            if (mutated && capture_set.count(mutated)) {
                result.clear();
                return false;
            }
        }
    }
    if (loaded.size() != capture_set.size()) {
        result.clear();
        return false;
    }
    return true;
}

static bool qore_ir_local_write_may_invalidate_caller_caches(
        const QoreIRFunction& func, const LocalVar* local) {
    if (!local || !func.ir_only_locals.count(reinterpret_cast<const void*>(local))) {
        return true;
    }
    return local->closureUse() || QoreTypeInfo::isReference(local->getTypeInfo());
}

static bool qore_ir_var_ref_write_may_invalidate_caller_caches(
        const QoreIRFunction& func, const VarRefNode* var) {
    if (!var) {
        return true;
    }
    switch (var->getType()) {
        case VT_LOCAL:
            return qore_ir_local_write_may_invalidate_caller_caches(func, var->ref.id);
        case VT_GLOBAL:
        case VT_LOCAL_TS:
        case VT_CLOSURE:
        case VT_IMMEDIATE:
        default:
            return true;
    }
}

static bool qore_ir_container_write_may_invalidate_caller_caches(
        const QoreIRFunction& func, const VarRefNode* container,
        const LocalVar* container_local) {
    return container_local
        ? qore_ir_local_write_may_invalidate_caller_caches(func, container_local)
        : qore_ir_var_ref_write_may_invalidate_caller_caches(func, container);
}

static bool qore_ir_lvalue_path_may_invalidate_caller_caches(
        const QoreIRFunction& func, const QoreIRLValuePathInstruction* inst) {
    if (!inst || inst->path.empty()) {
        return true;
    }
    const LVPathStep& root = inst->path.front();
    switch (root.kind) {
        case LVPathStepKind::LocalVar: {
            auto* local = reinterpret_cast<const LocalVar*>(root.ref_ptr);
            return qore_ir_local_write_may_invalidate_caller_caches(func, local)
                || (root.type_info && QoreTypeInfo::isReference(root.type_info));
        }
        case LVPathStepKind::SelfMember:
            return false;
        case LVPathStepKind::ClosureVar:
        case LVPathStepKind::GlobalVar:
        case LVPathStepKind::ThreadLocalVar:
        case LVPathStepKind::StaticVar:
        default:
            return true;
    }
}

static bool qore_ir_var_ref_write_may_modify_caller_runtime_locals(
        const QoreIRFunction& func, const VarRefNode* var) {
    if (!var) {
        return true;
    }
    switch (var->getType()) {
        case VT_LOCAL:
            return qore_ir_local_write_may_invalidate_caller_caches(
                func, var->ref.id);
        case VT_GLOBAL:
        case VT_THREAD_LOCAL:
            return !var->getTypeInfo()
                || QoreTypeInfo::isReference(var->getTypeInfo());
        case VT_LOCAL_TS:
        case VT_CLOSURE:
        case VT_IMMEDIATE:
        default:
            return true;
    }
}

static bool qore_ir_container_write_may_modify_caller_runtime_locals(
        const QoreIRFunction& func, const VarRefNode* container,
        const LocalVar* container_local) {
    return container_local
        ? qore_ir_local_write_may_invalidate_caller_caches(
            func, container_local)
        : qore_ir_var_ref_write_may_modify_caller_runtime_locals(
            func, container);
}

static bool qore_ir_lvalue_path_may_modify_caller_runtime_locals(
        const QoreIRFunction& func,
        const QoreIRLValuePathInstruction* inst) {
    if (!inst || inst->path.empty()) {
        return true;
    }
    const LVPathStep& root = inst->path.front();
    switch (root.kind) {
        case LVPathStepKind::LocalVar: {
            auto* local =
                reinterpret_cast<const LocalVar*>(root.ref_ptr);
            return qore_ir_local_write_may_invalidate_caller_caches(
                    func, local)
                || !root.type_info
                || QoreTypeInfo::isReference(root.type_info);
        }
        case LVPathStepKind::SelfMember:
            return false;
        case LVPathStepKind::GlobalVar:
        case LVPathStepKind::ThreadLocalVar:
            return !root.type_info
                || QoreTypeInfo::isReference(root.type_info);
        case LVPathStepKind::ClosureVar:
        case LVPathStepKind::StaticVar:
        default:
            return true;
    }
}

static bool qore_ir_instruction_may_modify_caller_runtime_locals(
        const QoreIRFunction& func, const QoreIRInstruction* inst) {
    if (!inst) {
        return true;
    }
    switch (inst->opcode) {
        case QoreIROpcode::StoreGlobal:
        case QoreIROpcode::StoreThreadLocal: {
            const auto* var_inst =
                static_cast<const QoreIRVarInstruction*>(inst);
            return !var_inst->var || !var_inst->var->getTypeInfo()
                || QoreTypeInfo::isReference(
                    var_inst->var->getTypeInfo());
        }
        case QoreIROpcode::Backquote:
            return false;
        case QoreIROpcode::StoreLocal: {
            const auto* local_inst =
                static_cast<const QoreIRLocalInstruction*>(inst);
            return qore_ir_local_write_may_invalidate_caller_caches(
                func, local_inst->local);
        }
        case QoreIROpcode::AddAssignLocalInt: {
            const auto* add_inst =
                static_cast<const QoreIRAddAssignLocalIntInstruction*>(inst);
            return !add_inst->target_ir_only
                || qore_ir_local_write_may_invalidate_caller_caches(
                    func, add_inst->target);
        }
        case QoreIROpcode::IncrementLocalInt: {
            const auto* inc_inst =
                static_cast<const QoreIRIncrementLocalIntInstruction*>(inst);
            return !inc_inst->ir_only
                || qore_ir_local_write_may_invalidate_caller_caches(
                    func, inc_inst->local);
        }
        case QoreIROpcode::HashKeyStore: {
            const auto* store =
                static_cast<const QoreIRHashKeyStoreInstruction*>(inst);
            return qore_ir_container_write_may_modify_caller_runtime_locals(
                func, store->container, store->container_lv);
        }
        case QoreIROpcode::HashKeyStoreDynamic: {
            const auto* store =
                static_cast<const QoreIRHashKeyStoreDynamicInstruction*>(
                    inst);
            return qore_ir_container_write_may_modify_caller_runtime_locals(
                func, store->container, store->container_lv);
        }
        case QoreIROpcode::ListIndexStore: {
            const auto* store =
                static_cast<const QoreIRListIndexStoreInstruction*>(inst);
            return qore_ir_container_write_may_modify_caller_runtime_locals(
                func, store->container, store->container_lv);
        }
        case QoreIROpcode::StoreLValue:
        case QoreIROpcode::PreIncLValue:
        case QoreIROpcode::PreDecLValue:
        case QoreIROpcode::PostIncLValue:
        case QoreIROpcode::PostDecLValue:
        case QoreIROpcode::AddAssignLValue:
        case QoreIROpcode::SubAssignLValue:
        case QoreIROpcode::MulAssignLValue:
        case QoreIROpcode::DivAssignLValue:
        case QoreIROpcode::ModAssignLValue:
        case QoreIROpcode::AndAssignLValue:
        case QoreIROpcode::OrAssignLValue:
        case QoreIROpcode::XorAssignLValue:
        case QoreIROpcode::ShlAssignLValue:
        case QoreIROpcode::ShrAssignLValue:
        case QoreIROpcode::ShiftLValue:
        case QoreIROpcode::UnshiftLValue:
        case QoreIROpcode::PopAny:
        case QoreIROpcode::PushAny:
        case QoreIROpcode::SpliceLValue: {
            const auto* lvalue_inst =
                static_cast<const QoreIRLValueInstruction*>(inst);
            return qore_ir_var_ref_write_may_modify_caller_runtime_locals(
                func, extractLValueBaseVarRef(lvalue_inst->lvalue));
        }
        case QoreIROpcode::LValuePathAssign:
        case QoreIROpcode::LValuePathCompound:
        case QoreIROpcode::LValuePathUnary:
        case QoreIROpcode::LValuePathBinaryMut:
        case QoreIROpcode::LValuePathTernary:
            return qore_ir_lvalue_path_may_modify_caller_runtime_locals(
                func,
                static_cast<const QoreIRLValuePathInstruction*>(inst));
        default:
            return qore_ir_instruction_may_invalidate_caller_caches(
                func, inst);
    }
}

static const LocalVar* qore_ir_get_written_local(
        const QoreIRInstruction* inst) {
    if (!inst) {
        return nullptr;
    }
    switch (inst->opcode) {
        case QoreIROpcode::StoreLocal:
        case QoreIROpcode::StoreClosure:
        case QoreIROpcode::InstantiateLocal:
        case QoreIROpcode::UninstantiateLocal:
            return static_cast<const QoreIRLocalInstruction*>(inst)->local;
        case QoreIROpcode::AddAssignLocalInt:
            return static_cast<const QoreIRAddAssignLocalIntInstruction*>(
                inst)->target;
        case QoreIROpcode::IncrementLocalInt:
            return static_cast<const QoreIRIncrementLocalIntInstruction*>(
                inst)->local;
        case QoreIROpcode::HashKeyStore: {
            const auto* store =
                static_cast<const QoreIRHashKeyStoreInstruction*>(inst);
            return store->container_lv
                ? store->container_lv
                : store->container && store->container->getType() == VT_LOCAL
                    ? store->container->ref.id : nullptr;
        }
        case QoreIROpcode::HashKeyStoreDynamic: {
            const auto* store =
                static_cast<const QoreIRHashKeyStoreDynamicInstruction*>(inst);
            return store->container_lv
                ? store->container_lv
                : store->container && store->container->getType() == VT_LOCAL
                    ? store->container->ref.id : nullptr;
        }
        case QoreIROpcode::ListIndexStore: {
            const auto* store =
                static_cast<const QoreIRListIndexStoreInstruction*>(inst);
            return store->container_lv
                ? store->container_lv
                : store->container && store->container->getType() == VT_LOCAL
                    ? store->container->ref.id : nullptr;
        }
        case QoreIROpcode::StoreLValue:
        case QoreIROpcode::PreIncLValue:
        case QoreIROpcode::PreDecLValue:
        case QoreIROpcode::PostIncLValue:
        case QoreIROpcode::PostDecLValue:
        case QoreIROpcode::AddAssignLValue:
        case QoreIROpcode::SubAssignLValue:
        case QoreIROpcode::MulAssignLValue:
        case QoreIROpcode::DivAssignLValue:
        case QoreIROpcode::ModAssignLValue:
        case QoreIROpcode::AndAssignLValue:
        case QoreIROpcode::OrAssignLValue:
        case QoreIROpcode::XorAssignLValue:
        case QoreIROpcode::ShlAssignLValue:
        case QoreIROpcode::ShrAssignLValue:
        case QoreIROpcode::ShiftLValue:
        case QoreIROpcode::UnshiftLValue:
        case QoreIROpcode::PopAny:
        case QoreIROpcode::PushAny:
        case QoreIROpcode::SpliceLValue: {
            const auto* lvalue =
                static_cast<const QoreIRLValueInstruction*>(inst);
            const VarRefNode* base = extractLValueBaseVarRef(lvalue->lvalue);
            return base && base->getType() == VT_LOCAL ? base->ref.id : nullptr;
        }
        case QoreIROpcode::LValuePathAssign:
        case QoreIROpcode::LValuePathCompound:
        case QoreIROpcode::LValuePathUnary:
        case QoreIROpcode::LValuePathBinaryMut:
        case QoreIROpcode::LValuePathTernary: {
            const auto* path =
                static_cast<const QoreIRLValuePathInstruction*>(inst);
            return !path->path.empty()
                    && path->path.front().kind == LVPathStepKind::LocalVar
                ? reinterpret_cast<const LocalVar*>(path->path.front().ref_ptr)
                : nullptr;
        }
        default:
            return nullptr;
    }
}

bool qore_ir_instruction_may_invalidate_caller_caches(
        const QoreIRFunction& func, const QoreIRInstruction* inst) {
    if (!inst) {
        return true;
    }
    switch (inst->opcode) {
        case QoreIROpcode::StoreClosure:
        case QoreIROpcode::StoreGlobal:
        case QoreIROpcode::StoreThreadLocal:
        case QoreIROpcode::NewObject:
        case QoreIROpcode::CreateParseRef:
        case QoreIROpcode::Call:
        case QoreIROpcode::CallIndirect:
        case QoreIROpcode::CallMethod:
        case QoreIROpcode::CallStatic:
        case QoreIROpcode::CallStaticDirect:
        case QoreIROpcode::CallMethodDirect:
        case QoreIROpcode::InvokeMethodDirect:
        case QoreIROpcode::DotEvalMethodDirect:
        case QoreIROpcode::InvokeDotEvalMethodDirect:
        case QoreIROpcode::CallClosureDirect:
        case QoreIROpcode::DotEvalAny:
        case QoreIROpcode::DotEvalInt:
        case QoreIROpcode::DotEvalFloat:
        case QoreIROpcode::DotEvalString:
        case QoreIROpcode::DotEvalDate:
        case QoreIROpcode::DotEvalList:
        case QoreIROpcode::DotEvalHash:
        case QoreIROpcode::DotEvalObject:
        case QoreIROpcode::BackgroundInt:
        case QoreIROpcode::Invoke:
        case QoreIROpcode::InvokeSimError:
        case QoreIROpcode::OnBlockExit:
        case QoreIROpcode::ScopeExit:
        case QoreIROpcode::Backquote:
            return true;

        case QoreIROpcode::StoreLocal: {
            auto* local_inst = static_cast<const QoreIRLocalInstruction*>(inst);
            return qore_ir_local_write_may_invalidate_caller_caches(func, local_inst->local);
        }
        case QoreIROpcode::AddAssignLocalInt: {
            auto* add_inst = static_cast<const QoreIRAddAssignLocalIntInstruction*>(inst);
            return !add_inst->target_ir_only
                || qore_ir_local_write_may_invalidate_caller_caches(func, add_inst->target);
        }
        case QoreIROpcode::IncrementLocalInt: {
            auto* inc_inst = static_cast<const QoreIRIncrementLocalIntInstruction*>(inst);
            return !inc_inst->ir_only
                || qore_ir_local_write_may_invalidate_caller_caches(func, inc_inst->local);
        }
        case QoreIROpcode::HashKeyStore: {
            auto* store_inst = static_cast<const QoreIRHashKeyStoreInstruction*>(inst);
            return qore_ir_container_write_may_invalidate_caller_caches(
                func, store_inst->container, store_inst->container_lv);
        }
        case QoreIROpcode::HashKeyStoreDynamic: {
            auto* store_inst = static_cast<const QoreIRHashKeyStoreDynamicInstruction*>(inst);
            return qore_ir_container_write_may_invalidate_caller_caches(
                func, store_inst->container, store_inst->container_lv);
        }
        case QoreIROpcode::ListIndexStore: {
            auto* store_inst = static_cast<const QoreIRListIndexStoreInstruction*>(inst);
            return qore_ir_var_ref_write_may_invalidate_caller_caches(func, store_inst->container);
        }
        case QoreIROpcode::StoreLValue:
        case QoreIROpcode::PreIncLValue:
        case QoreIROpcode::PreDecLValue:
        case QoreIROpcode::PostIncLValue:
        case QoreIROpcode::PostDecLValue:
        case QoreIROpcode::AddAssignLValue:
        case QoreIROpcode::SubAssignLValue:
        case QoreIROpcode::MulAssignLValue:
        case QoreIROpcode::DivAssignLValue:
        case QoreIROpcode::ModAssignLValue:
        case QoreIROpcode::AndAssignLValue:
        case QoreIROpcode::OrAssignLValue:
        case QoreIROpcode::XorAssignLValue:
        case QoreIROpcode::ShlAssignLValue:
        case QoreIROpcode::ShrAssignLValue:
        case QoreIROpcode::ShiftLValue:
        case QoreIROpcode::UnshiftLValue:
        case QoreIROpcode::PopAny:
        case QoreIROpcode::PushAny:
        case QoreIROpcode::SpliceLValue: {
            auto* lvalue_inst = static_cast<const QoreIRLValueInstruction*>(inst);
            return qore_ir_var_ref_write_may_invalidate_caller_caches(
                func, extractLValueBaseVarRef(lvalue_inst->lvalue));
        }
        case QoreIROpcode::LValuePathAssign:
        case QoreIROpcode::LValuePathCompound:
        case QoreIROpcode::LValuePathUnary:
        case QoreIROpcode::LValuePathBinaryMut:
        case QoreIROpcode::LValuePathTernary:
            return qore_ir_lvalue_path_may_invalidate_caller_caches(
                func, static_cast<const QoreIRLValuePathInstruction*>(inst));
        default:
            return false;
    }
}

static bool qore_ir_variant_has_reference_params(
        const AbstractQoreFunctionVariant* variant) {
    const UserVariantBase* uvb = variant ? variant->getUserVariantBase() : nullptr;
    const UserSignature* sig = uvb ? uvb->getUserSignature() : nullptr;
    if (!sig) {
        return true;
    }
    for (size_t i = 0; i < sig->numParams(); ++i) {
        if (i && !(i % 100)
                && qore_check_cancel(nullptr,
                    "IR closure reference parameter analysis")) {
            return true;
        }
        const QoreTypeInfo* type = sig->getParamTypeInfo(i);
        if (type && QoreTypeInfo::isReference(type)) {
            return true;
        }
    }
    return false;
}

static const AbstractQoreFunctionVariant* qore_ir_get_resolved_effect_callee(
        const QoreIRInstruction* inst, bool& has_ref_args,
        const std::unordered_map<uint32_t,
            const AbstractQoreFunctionVariant*>* closure_values = nullptr) {
    has_ref_args = true;
    if (!inst) {
        return nullptr;
    }
    switch (inst->opcode) {
        case QoreIROpcode::CallDirect: {
            const auto* call = static_cast<const QoreIRCallDirectInstruction*>(inst);
            has_ref_args = call->has_ref_args;
            if (call->variant) {
                return call->variant;
            }
            if (call->expr.hasNode()) {
                const auto* expr = dynamic_cast<const FunctionCallNode*>(
                    call->expr.getInternalNode());
                if (expr && expr->getVariant()) {
                    return expr->getVariant();
                }
            }
            return call->func && call->func->numVariants() == 1
                ? call->func->first() : nullptr;
        }
        case QoreIROpcode::CallStaticDirect: {
            const auto* call = static_cast<const QoreIRCallStaticDirectInstruction*>(inst);
            has_ref_args = call->has_ref_args;
            return call->variant;
        }
        case QoreIROpcode::CallMethodDirect: {
            const auto* call = static_cast<const QoreIRCallMethodDirectInstruction*>(inst);
            has_ref_args = call->has_ref_args;
            return call->variant;
        }
        case QoreIROpcode::InvokeMethodDirect: {
            const auto* call = static_cast<const QoreIRInvokeMethodDirectInstruction*>(inst);
            has_ref_args = call->has_ref_args;
            return call->variant;
        }
        case QoreIROpcode::CallClosureDirect: {
            const auto* call = static_cast<const QoreIRExprInstruction*>(inst);
            has_ref_args = call->has_ref_args;
            if (!closure_values || inst->operands.empty()) {
                return nullptr;
            }
            auto closure = closure_values->find(inst->operands[0].id);
            if (closure == closure_values->end()) {
                return nullptr;
            }
            has_ref_args = qore_ir_variant_has_reference_params(closure->second);
            return closure->second;
        }
        case QoreIROpcode::Invoke: {
            const auto* call = static_cast<const QoreIRInvokeInstruction*>(inst);
            has_ref_args = call->has_ref_args;
            if (call->invoke_opcode == QoreIROpcode::CallDirect) {
                if (call->expr.hasNode()) {
                    const auto* expr = dynamic_cast<const FunctionCallNode*>(
                        call->expr.getInternalNode());
                    if (expr && expr->getVariant()) {
                        return expr->getVariant();
                    }
                }
                return call->func && call->func->numVariants() == 1
                    ? call->func->first() : nullptr;
            }
            if (call->invoke_opcode == QoreIROpcode::CallStaticDirect) {
                const auto* expr = dynamic_cast<const StaticMethodCallNode*>(
                    call->expr.getInternalNode());
                return expr ? expr->getVariant() : nullptr;
            }
            if (call->invoke_opcode != QoreIROpcode::CallClosureDirect) {
                return nullptr;
            }
            if (!closure_values || inst->operands.empty()) {
                return nullptr;
            }
            auto closure = closure_values->find(inst->operands[0].id);
            if (closure == closure_values->end()) {
                return nullptr;
            }
            has_ref_args = qore_ir_variant_has_reference_params(closure->second);
            return closure->second;
        }
        default:
            return nullptr;
    }
}

static size_t qore_ir_get_effect_callee_arg_offset(
        const QoreIRInstruction* inst) {
    if (!inst) {
        return 0;
    }
    if (inst->opcode == QoreIROpcode::CallClosureDirect) {
        return 1;
    }
    if (inst->opcode == QoreIROpcode::Invoke) {
        const auto* invoke =
            static_cast<const QoreIRInvokeInstruction*>(inst);
        return invoke->invoke_opcode == QoreIROpcode::CallClosureDirect ? 1 : 0;
    }
    return 0;
}

static const AbstractQoreFunctionVariant* qore_ir_get_created_closure_variant(
        const QoreIRInstruction* inst) {
    if (!inst || inst->opcode != QoreIROpcode::CreateClosure) {
        return nullptr;
    }
    const auto* create = static_cast<const QoreIRCreateClosureInstruction*>(inst);
    const QoreClosureParseNode* closure = create->closure_node;
    if (!closure) {
        closure = dynamic_cast<const QoreClosureParseNode*>(
            create->expr.getInternalNode());
    }
    const UserClosureFunction* ucf = closure ? closure->getFunction() : nullptr;
    return ucf ? ucf->first() : nullptr;
}

static bool qore_ir_is_non_overridable_method_call(const QoreIRInstruction& inst) {
    const QoreMethod* method = nullptr;
    const QoreClass* qc = nullptr;
    const AbstractQoreFunctionVariant* variant = nullptr;
    if (inst.opcode == QoreIROpcode::CallMethodDirect) {
        const auto& call = static_cast<const QoreIRCallMethodDirectInstruction&>(inst);
        method = call.method;
        qc = call.qc;
        variant = call.variant;
    } else if (inst.opcode == QoreIROpcode::InvokeMethodDirect) {
        const auto& call = static_cast<const QoreIRInvokeMethodDirectInstruction&>(inst);
        method = call.method;
        qc = call.qc;
        variant = call.variant;
    } else if (inst.opcode == QoreIROpcode::DotEvalMethodDirect) {
        const auto& call =
            static_cast<const QoreIRDotEvalMethodDirectInstruction&>(inst);
        if (call.pseudo) {
            return false;
        }
        method = call.method;
        qc = call.qc;
        variant = call.variant;
    } else {
        return false;
    }
    if (!method || !qc || method->getClass() != qc) {
        return false;
    }
    if (qc->isFinal()) {
        return true;
    }
    if (std::getenv("QORE_DISABLE_IR_FINAL_METHOD_DEVIRTUALIZATION")) {
        return false;
    }
    const auto* method_variant = dynamic_cast<const MethodVariantBase*>(variant);
    return method_variant && method_variant->isFinal();
}

static bool qore_ir_is_read_only_list_use(const QoreIRInstruction& inst,
        QoreIRValue value) {
    if (inst.operands.empty() || inst.operands[0].id != value.id) {
        return false;
    }
    switch (inst.opcode) {
        case QoreIROpcode::ListSize:
        case QoreIROpcode::ListGetInt:
        case QoreIROpcode::ListGetFloat:
        case QoreIROpcode::ListGetValue:
        case QoreIROpcode::ListGetValueNoRef:
        case QoreIROpcode::ListGetValueNoRefUnchecked:
        case QoreIROpcode::ListIndexDynamic:
            return true;
        default:
            return false;
    }
}

static bool qore_ir_is_read_only_hash_use(const QoreIRInstruction& inst,
        QoreIRValue value) {
    if (inst.operands.empty() || inst.operands[0].id != value.id) {
        return false;
    }
    switch (inst.opcode) {
        case QoreIROpcode::HashKeyAccess:
        case QoreIROpcode::HashKeyAccessInt:
        case QoreIROpcode::HashKeyAccessHash:
        case QoreIROpcode::HashKeyAccessHashGuarded:
            return true;
        case QoreIROpcode::Invoke: {
            const auto& invoke = static_cast<const QoreIRInvokeInstruction&>(inst);
            return invoke.invoke_opcode == QoreIROpcode::HashKeyAccess
                || invoke.invoke_opcode == QoreIROpcode::HashKeyAccessInt
                || invoke.invoke_opcode == QoreIROpcode::HashKeyAccessHash
                || invoke.invoke_opcode == QoreIROpcode::HashKeyAccessHashGuarded;
        }
        default:
            return false;
    }
}

static bool qore_ir_is_read_only_string_intrinsic(QoreIRIntrinsic intrinsic) {
    switch (intrinsic) {
        case QoreIRIntrinsic::Size:
        case QoreIRIntrinsic::Empty:
        case QoreIRIntrinsic::StringStrlen:
        case QoreIRIntrinsic::StringLength:
        case QoreIRIntrinsic::StringSizeP:
        case QoreIRIntrinsic::StringStrP:
        case QoreIRIntrinsic::StringIntP:
        case QoreIRIntrinsic::StringLower:
        case QoreIRIntrinsic::StringUpper:
        case QoreIRIntrinsic::StringToInt:
        case QoreIRIntrinsic::StringStartsWith:
        case QoreIRIntrinsic::StringEndsWith:
        case QoreIRIntrinsic::StringContains:
        case QoreIRIntrinsic::StringFind:
        case QoreIRIntrinsic::StringRFind:
        case QoreIRIntrinsic::StringSubstr:
            return true;
        default:
            return false;
    }
}

static bool qore_ir_is_read_only_string_arg_intrinsic(QoreIRIntrinsic intrinsic) {
    switch (intrinsic) {
        case QoreIRIntrinsic::StringStartsWith:
        case QoreIRIntrinsic::StringEndsWith:
        case QoreIRIntrinsic::StringContains:
        case QoreIRIntrinsic::StringFind:
        case QoreIRIntrinsic::StringRFind:
            return true;
        default:
            return false;
    }
}

static bool qore_ir_is_read_only_string_use(const QoreIRInstruction& inst,
        QoreIRValue value) {
    if (inst.operands.empty()) {
        return false;
    }
    if (inst.opcode == QoreIROpcode::DotEvalMethodDirect) {
        const auto& direct = static_cast<const QoreIRDotEvalMethodDirectInstruction&>(inst);
        return direct.pseudo && ((inst.operands[0].id == value.id
                && direct.pseudo_base_known_string
                && qore_ir_is_read_only_string_intrinsic(direct.intrinsic))
            || (inst.operands.size() > 1 && inst.operands[1].id == value.id
                && direct.pseudo_arg0_known_string
                && qore_ir_is_read_only_string_arg_intrinsic(direct.intrinsic)));
    }
    if (inst.opcode == QoreIROpcode::InvokeDotEvalMethodDirect) {
        const auto& invoke = static_cast<const QoreIRInvokeDotEvalMethodDirectInstruction&>(inst);
        return invoke.pseudo && ((inst.operands[0].id == value.id
                && invoke.pseudo_base_known_string
                && qore_ir_is_read_only_string_intrinsic(invoke.intrinsic))
            || (inst.operands.size() > 1 && inst.operands[1].id == value.id
                && invoke.pseudo_arg0_known_string
                && qore_ir_is_read_only_string_arg_intrinsic(invoke.intrinsic)));
    }
    return false;
}

static bool qore_ir_is_read_only_aggregate_use(const QoreIRInstruction& inst,
        QoreIRValue value) {
    return qore_ir_is_read_only_list_use(inst, value)
        || qore_ir_is_read_only_hash_use(inst, value)
        || qore_ir_is_read_only_string_use(inst, value);
}

bool qore_ir_compute_function_effect_summaries(
        const std::vector<std::pair<const AbstractQoreFunctionVariant*, const QoreIRFunction*>>& functions,
        std::unordered_map<const AbstractQoreFunctionVariant*, QoreIRFunctionEffectSummary>& summaries) {
    constexpr size_t MAX_EXACT_RUNTIME_LOCAL_EFFECTS = 16;

    struct FunctionEffects {
        const AbstractQoreFunctionVariant* variant = nullptr;
        bool local_may_invalidate = false;
        bool local_may_modify_runtime_locals = false;
        std::unordered_set<const void*> local_modified_runtime_locals;
        bool local_never_returns_nothing = true;
        bool saw_return = false;
        std::vector<const AbstractQoreFunctionVariant*> callees;
        std::vector<uint8_t> param_noescape;
        std::vector<uint8_t> param_may_modify;
        std::vector<std::vector<std::pair<const AbstractQoreFunctionVariant*, size_t>>> param_callees;
    };

    summaries.clear();
    std::unordered_map<const AbstractQoreFunctionVariant*, size_t> function_ids;
    std::vector<FunctionEffects> effects;
    effects.reserve(functions.size());
    size_t check_count = 0;
    for (const auto& [variant, func] : functions) {
        if (qore_ir_analysis_cancelled(check_count, "IR function effect analysis")) {
            return false;
        }
        if (!variant || !func || function_ids.count(variant)) {
            continue;
        }
        size_t function_id = effects.size();
        function_ids.emplace(variant, function_id);
        effects.push_back({variant});
        FunctionEffects& effect = effects.back();
        size_t param_count = 0;
        for (const auto& [index, local] : func->param_local_vars) {
            if (index >= 0 && local) {
                param_count = std::max(param_count, static_cast<size_t>(index) + 1);
            }
        }
        effect.param_noescape.assign(param_count, true);
        effect.param_may_modify.assign(param_count, false);
        effect.param_callees.resize(param_count);
        std::unordered_map<const LocalVar*, size_t> param_indexes;
        for (const auto& [index, local] : func->param_local_vars) {
            if (index < 0 || !local || static_cast<size_t>(index) >= param_count) {
                continue;
            }
            size_t param_index = static_cast<size_t>(index);
            param_indexes[local] = param_index;
            if (local->closureUse() || QoreTypeInfo::isReference(local->getTypeInfo())) {
                effect.param_noescape[param_index] = false;
            }
        }
        std::unordered_set<const void*> initially_assigned;
        for (const auto& [index, local] : func->param_local_vars) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR function assigned parameter analysis")) {
                return false;
            }
            (void)index;
            if (local) {
                initially_assigned.insert(reinterpret_cast<const void*>(local));
            }
        }
        if (!qore_ir_get_native_unsafe_locals(*func, initially_assigned).empty()) {
            effect.local_never_returns_nothing = false;
        }
        std::unordered_map<uint32_t, const AbstractQoreFunctionVariant*>
            closure_values;
        for (const auto& block : func->blocks) {
            for (const auto& inst_ptr : block->instructions) {
                if (qore_ir_analysis_cancelled(check_count,
                        "IR function closure value analysis")) {
                    return false;
                }
                const AbstractQoreFunctionVariant* closure =
                    qore_ir_get_created_closure_variant(inst_ptr.get());
                if (closure && inst_ptr->result.isValid()) {
                    closure_values.emplace(inst_ptr->result.id, closure);
                }
            }
        }
        std::unordered_map<uint32_t, size_t> loaded_params;
        for (const auto& block : func->blocks) {
            if (qore_ir_analysis_cancelled(check_count, "IR function effect analysis")) {
                return false;
            }
            for (const auto& inst_ptr : block->instructions) {
                if (qore_ir_analysis_cancelled(check_count, "IR function effect analysis")) {
                    return false;
                }
                const QoreIRInstruction* inst = inst_ptr.get();
                if (inst && inst->opcode == QoreIROpcode::LoadLocal) {
                    const auto* load = static_cast<const QoreIRLocalInstruction*>(inst);
                    auto param_it = param_indexes.find(load->local);
                    if (param_it != param_indexes.end() && inst->result.isValid()) {
                        loaded_params[inst->result.id] = param_it->second;
                    }
                }
                if (inst && inst->opcode == QoreIROpcode::Return) {
                    auto* ret = static_cast<const QoreIRReturnInstruction*>(inst);
                    effect.saw_return = true;
                    const QoreIRValueFacts* facts = ret->has_value
                        ? func->getValueFacts(ret->value) : nullptr;
                    if (!facts || facts->assigned_state != QoreIRAssignedState::Assigned
                            || !facts->never_nothing) {
                        effect.local_never_returns_nothing = false;
                    }
                } else if (inst && inst->opcode == QoreIROpcode::ReturnNothing) {
                    effect.saw_return = true;
                    effect.local_never_returns_nothing = false;
                }
                bool has_ref_args = true;
                const AbstractQoreFunctionVariant* callee =
                    qore_ir_get_resolved_effect_callee(inst, has_ref_args,
                        &closure_values);
                if (callee || (inst && (inst->opcode == QoreIROpcode::CallDirect
                        || inst->opcode == QoreIROpcode::CallStaticDirect
                        || inst->opcode == QoreIROpcode::CallMethodDirect
                        || inst->opcode == QoreIROpcode::InvokeMethodDirect
                        || inst->opcode == QoreIROpcode::CallClosureDirect
                        || (inst->opcode == QoreIROpcode::Invoke
                            && static_cast<const QoreIRInvokeInstruction*>(inst)
                                ->invoke_opcode == QoreIROpcode::CallClosureDirect)))) {
                    if (has_ref_args || !callee) {
                        effect.local_may_invalidate = true;
                        effect.local_may_modify_runtime_locals = true;
                        effect.local_modified_runtime_locals.clear();
                    } else {
                        effect.callees.push_back(callee);
                    }
                } else if (qore_ir_instruction_may_invalidate_caller_caches(*func, inst)) {
                    const LocalVar* written_local =
                        qore_ir_get_written_local(inst);
                    auto param_it = param_indexes.find(written_local);
                    if (param_it != param_indexes.end()
                            && written_local && !written_local->closureUse()
                            && !QoreTypeInfo::isReference(
                                written_local->getTypeInfo())) {
                        effect.param_may_modify[param_it->second] = true;
                    } else {
                        effect.local_may_invalidate = true;
                        if (qore_ir_instruction_may_modify_caller_runtime_locals(
                                *func, inst)) {
                            if (written_local && !written_local->closureUse()
                                    && !QoreTypeInfo::isReference(
                                        written_local->getTypeInfo())) {
                                if (!effect.local_may_modify_runtime_locals) {
                                    effect.local_modified_runtime_locals.insert(
                                        reinterpret_cast<const void*>(
                                            written_local));
                                    if (effect.local_modified_runtime_locals.size()
                                            > MAX_EXACT_RUNTIME_LOCAL_EFFECTS) {
                                        effect.local_may_modify_runtime_locals = true;
                                        effect.local_modified_runtime_locals.clear();
                                    }
                                }
                            } else {
                                effect.local_may_modify_runtime_locals = true;
                                effect.local_modified_runtime_locals.clear();
                            }
                        }
                    }
                }
            }
        }
        for (const auto& block : func->blocks) {
            for (const auto& inst_ptr : block->instructions) {
                if (qore_ir_analysis_cancelled(check_count,
                        "IR function parameter noescape analysis")) {
                    return false;
                }
                const QoreIRInstruction* inst = inst_ptr.get();
                if (!inst) {
                    continue;
                }
                const LocalVar* written_local =
                    qore_ir_get_written_local(inst);
                if (written_local) {
                    auto param_it = param_indexes.find(written_local);
                    if (param_it != param_indexes.end()) {
                        effect.param_noescape[param_it->second] = false;
                        effect.param_may_modify[param_it->second] = true;
                    }
                }
                bool callee_arg_analysis_cancelled = false;
                qore_ir_visit_value_operands(*inst, [&](QoreIRValue operand) {
                    auto loaded_it = loaded_params.find(operand.id);
                    if (loaded_it == loaded_params.end()) {
                        return;
                    }
                    size_t param_index = loaded_it->second;
                    if (inst->opcode == QoreIROpcode::Return) {
                        return;
                    }
                    bool has_ref_args = true;
                    const AbstractQoreFunctionVariant* callee =
                        qore_ir_get_resolved_effect_callee(inst, has_ref_args,
                            &closure_values);
                    if (callee || inst->opcode == QoreIROpcode::CallDirect
                            || inst->opcode == QoreIROpcode::CallStaticDirect
                            || inst->opcode == QoreIROpcode::CallMethodDirect
                            || inst->opcode == QoreIROpcode::InvokeMethodDirect
                            || inst->opcode == QoreIROpcode::CallClosureDirect
                            || (inst->opcode == QoreIROpcode::Invoke
                                && static_cast<const QoreIRInvokeInstruction*>(inst)
                                    ->invoke_opcode == QoreIROpcode::CallClosureDirect)) {
                        if (!callee || has_ref_args) {
                            effect.param_noescape[param_index] = false;
                            effect.param_may_modify[param_index] = true;
                            return;
                        }
                        size_t arg_offset =
                            qore_ir_get_effect_callee_arg_offset(inst);
                        for (size_t arg = arg_offset;
                                arg < inst->operands.size(); ++arg) {
                            if (qore_ir_analysis_cancelled(check_count,
                                    "IR function parameter callee argument analysis")) {
                                callee_arg_analysis_cancelled = true;
                                return;
                            }
                            if (inst->operands[arg].id == operand.id) {
                                effect.param_callees[param_index].emplace_back(
                                    callee, arg - arg_offset);
                            }
                        }
                        return;
                    }
                    if (!effect.param_noescape[param_index]) {
                        return;
                    }
                    if (qore_ir_is_read_only_aggregate_use(*inst, operand)) {
                        return;
                    }
                    effect.param_noescape[param_index] = false;
                });
                if (callee_arg_analysis_cancelled) {
                    return false;
                }
            }
        }
    }

    std::vector<std::vector<size_t>> callers(effects.size());
    std::vector<size_t> worklist;
    for (size_t function_id = 0; function_id < effects.size(); ++function_id) {
        if (qore_ir_analysis_cancelled(check_count, "IR function effect graph construction")) {
            return false;
        }
        const FunctionEffects& effect = effects[function_id];
        bool may_invalidate = effect.local_may_invalidate;
        for (const AbstractQoreFunctionVariant* callee : effect.callees) {
            if (qore_ir_analysis_cancelled(check_count, "IR function effect graph construction")) {
                return false;
            }
            auto callee_it = function_ids.find(callee);
            if (callee_it == function_ids.end()) {
                may_invalidate = true;
            } else {
                callers[callee_it->second].push_back(function_id);
            }
        }
        summaries[effect.variant].may_invalidate_external_caches = may_invalidate;
        summaries[effect.variant].may_modify_runtime_locals =
            effect.local_may_modify_runtime_locals;
        summaries[effect.variant].modified_runtime_locals.assign(
            effect.local_modified_runtime_locals.begin(),
            effect.local_modified_runtime_locals.end());
        summaries[effect.variant].never_returns_nothing =
            effect.saw_return && effect.local_never_returns_nothing;
        summaries[effect.variant].param_noescape = effect.param_noescape;
        summaries[effect.variant].param_may_modify = effect.param_may_modify;
        if (may_invalidate) {
            worklist.push_back(function_id);
        }
    }
    while (!worklist.empty()) {
        if (qore_ir_analysis_cancelled(check_count, "IR function effect propagation")) {
            return false;
        }
        size_t callee_id = worklist.back();
        worklist.pop_back();
        for (size_t caller_id : callers[callee_id]) {
            if (qore_ir_analysis_cancelled(check_count, "IR function effect propagation")) {
                return false;
            }
            QoreIRFunctionEffectSummary& caller_summary = summaries[effects[caller_id].variant];
            if (!caller_summary.may_invalidate_external_caches) {
                caller_summary.may_invalidate_external_caches = true;
                worklist.push_back(caller_id);
            }
        }
    }
    worklist.clear();
    for (size_t function_id = 0; function_id < effects.size();
            ++function_id) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR runtime-local effect worklist construction")) {
            return false;
        }
        const auto& summary = summaries[effects[function_id].variant];
        if (summary.may_modify_runtime_locals
                || !summary.modified_runtime_locals.empty()) {
            worklist.push_back(function_id);
        }
    }
    while (!worklist.empty()) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR runtime-local effect propagation")) {
            return false;
        }
        size_t callee_id = worklist.back();
        worklist.pop_back();
        for (size_t caller_id : callers[callee_id]) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR runtime-local effect propagation")) {
                return false;
            }
            auto& caller_summary =
                summaries[effects[caller_id].variant];
            bool changed = false;
            const auto& callee_summary =
                summaries[effects[callee_id].variant];
            if (callee_summary.may_modify_runtime_locals
                    && !caller_summary.may_modify_runtime_locals) {
                caller_summary.may_modify_runtime_locals = true;
                caller_summary.modified_runtime_locals.clear();
                changed = true;
            }
            if (!caller_summary.may_modify_runtime_locals) {
                for (const void* local
                        : callee_summary.modified_runtime_locals) {
                    if (std::find(
                            caller_summary.modified_runtime_locals.begin(),
                            caller_summary.modified_runtime_locals.end(), local)
                            == caller_summary.modified_runtime_locals.end()) {
                        caller_summary.modified_runtime_locals.push_back(local);
                        changed = true;
                        if (caller_summary.modified_runtime_locals.size()
                                > MAX_EXACT_RUNTIME_LOCAL_EFFECTS) {
                            caller_summary.may_modify_runtime_locals = true;
                            caller_summary.modified_runtime_locals.clear();
                            break;
                        }
                    }
                }
            }
            if (changed) {
                worklist.push_back(caller_id);
            }
        }
    }
    bool noescape_changed = true;
    while (noescape_changed) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR function parameter noescape propagation")) {
            return false;
        }
        noescape_changed = false;
        for (const FunctionEffects& effect : effects) {
            auto& summary = summaries[effect.variant];
            for (size_t param = 0; param < summary.param_noescape.size(); ++param) {
                if (!summary.param_noescape[param]) {
                    continue;
                }
                for (const auto& [callee, callee_param] : effect.param_callees[param]) {
                    auto callee_it = summaries.find(callee);
                    if (callee_it == summaries.end()
                            || callee_param >= callee_it->second.param_noescape.size()
                            || !callee_it->second.param_noescape[callee_param]) {
                        summary.param_noescape[param] = false;
                        noescape_changed = true;
                        break;
                    }
                }
            }
        }
    }
    bool modify_changed = true;
    while (modify_changed) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR function parameter modification propagation")) {
            return false;
        }
        modify_changed = false;
        for (const FunctionEffects& effect : effects) {
            auto& summary = summaries[effect.variant];
            for (size_t param = 0; param < summary.param_may_modify.size();
                    ++param) {
                if (summary.param_may_modify[param]) {
                    continue;
                }
                for (const auto& [callee, callee_param]
                        : effect.param_callees[param]) {
                    auto callee_it = summaries.find(callee);
                    if (callee_it == summaries.end()
                            || callee_param
                                >= callee_it->second.param_may_modify.size()
                            || callee_it->second.param_may_modify[callee_param]) {
                        summary.param_may_modify[param] = true;
                        modify_changed = true;
                        break;
                    }
                }
            }
        }
    }
    return true;
}

bool qore_ir_visit_value_operands(const QoreIRInstruction& inst, const QoreIRValueVisitor& visitor,
        size_t* check_count, const char* operation) {
    auto visit = [&](QoreIRValue operand) {
        if (check_count && qore_ir_analysis_cancelled(*check_count, operation)) {
            return false;
        }
        visitor(operand);
        return true;
    };
    for (QoreIRValue operand : inst.operands) {
        if (!visit(operand)) {
            return false;
        }
    }

    switch (inst.opcode) {
        case QoreIROpcode::BrIf:
            return visit(static_cast<const QoreIRBranchIfInstruction&>(inst).condition);
        case QoreIROpcode::SwitchInt:
            return visit(static_cast<const QoreIRSwitchIntInstruction&>(inst).switch_val);
        case QoreIROpcode::SwitchString:
            return visit(static_cast<const QoreIRSwitchStringInstruction&>(inst).switch_val);
        case QoreIROpcode::IteratorCreate:
        case QoreIROpcode::IteratorCreateIterate:
            return visit(static_cast<const QoreIRIteratorCreateInstruction&>(inst).iterable);
        case QoreIROpcode::IteratorNext:
            return visit(static_cast<const QoreIRIteratorNextInstruction&>(inst).iterator);
        case QoreIROpcode::TypedForeachNextInt:
        case QoreIROpcode::TypedForeachNextFloat:
        case QoreIROpcode::TypedForeachNextBool:
        case QoreIROpcode::TypedForeachNextString:
            // The list, index, and entry limit are in the base operand vector.
            break;
        case QoreIROpcode::Return: {
            const auto& ret = static_cast<const QoreIRReturnInstruction&>(inst);
            if (ret.has_value) {
                return visit(ret.value);
            }
            break;
        }
        default:
            break;
    }
    return true;
}

static void qore_ir_visit_unique_successor(QoreIRBasicBlock* block,
        std::unordered_set<QoreIRBasicBlock*>& visited, const QoreIRBlockVisitor& visitor) {
    if (block && visited.insert(block).second) {
        visitor(block);
    }
}

void qore_ir_visit_successors(const QoreIRInstruction& inst, const QoreIRBlockVisitor& visitor) {
    std::unordered_set<QoreIRBasicBlock*> visited;
    auto visit = [&](QoreIRBasicBlock* block) {
        qore_ir_visit_unique_successor(block, visited, visitor);
    };

    switch (inst.opcode) {
        case QoreIROpcode::Br:
            visit(static_cast<const QoreIRBranchInstruction&>(inst).target);
            break;
        case QoreIROpcode::BrIf: {
            const auto& branch = static_cast<const QoreIRBranchIfInstruction&>(inst);
            visit(branch.true_target);
            visit(branch.false_target);
            break;
        }
        case QoreIROpcode::BranchIfLtLocalInt: {
            const auto& branch = static_cast<const QoreIRBranchIfLtLocalIntInstruction&>(inst);
            visit(branch.true_target);
            visit(branch.false_target);
            break;
        }
        case QoreIROpcode::SwitchInt: {
            const auto& switch_inst = static_cast<const QoreIRSwitchIntInstruction&>(inst);
            visit(switch_inst.default_target);
            for (const QoreIRSwitchCase& c : switch_inst.cases) {
                visit(c.target);
            }
            break;
        }
        case QoreIROpcode::SwitchString: {
            const auto& switch_inst = static_cast<const QoreIRSwitchStringInstruction&>(inst);
            visit(switch_inst.default_target);
            for (const QoreIRSwitchStringCase& c : switch_inst.cases) {
                visit(c.target);
            }
            break;
        }
        case QoreIROpcode::IteratorNext:
        case QoreIROpcode::TypedForeachNextInt:
        case QoreIROpcode::TypedForeachNextFloat:
        case QoreIROpcode::TypedForeachNextBool:
        case QoreIROpcode::TypedForeachNextString: {
            const auto& next = static_cast<const QoreIRIteratorNextInstruction&>(inst);
            visit(next.continue_target);
            visit(next.done_target);
            break;
        }
        case QoreIROpcode::InvokeMethodDirect: {
            const auto& invoke = static_cast<const QoreIRInvokeMethodDirectInstruction&>(inst);
            visit(invoke.normal_target);
            break;
        }
        case QoreIROpcode::InvokeDotEvalMethodDirect: {
            const auto& invoke = static_cast<const QoreIRInvokeDotEvalMethodDirectInstruction&>(inst);
            visit(invoke.normal_target);
            break;
        }
        case QoreIROpcode::Invoke:
            visit(static_cast<const QoreIRInvokeInstruction&>(inst).normal_target);
            break;
        default:
            break;
    }
}

QoreIRControlFlowGraph::QoreIRControlFlowGraph(const QoreIRFunction& func) {
    blocks.reserve(func.blocks.size());
    size_t check_count = 0;
    for (const auto& block : func.blocks) {
        if (qore_ir_analysis_cancelled(check_count, "IR control-flow analysis")) {
            cancelled = true;
            return;
        }
        block_ids.emplace(block.get(), blocks.size());
        blocks.push_back(block.get());
    }

    successors.resize(blocks.size());
    predecessors.resize(blocks.size());
    for (size_t block_id = 0; block_id < blocks.size(); ++block_id) {
        if (qore_ir_analysis_cancelled(check_count, "IR control-flow analysis")) {
            cancelled = true;
            return;
        }
        const QoreIRBasicBlock* block = blocks[block_id];
        if (!block || block->instructions.empty()) {
            continue;
        }
        qore_ir_visit_successors(*block->instructions.back(), [&](QoreIRBasicBlock* successor) {
            auto it = block_ids.find(successor);
            if (it == block_ids.end()) {
                return;
            }
            successors[block_id].push_back(it->second);
            predecessors[it->second].push_back(block_id);
        });
    }

    reachable.assign(blocks.size(), 0);
    if (!blocks.empty()) {
        std::vector<size_t> worklist{0};
        reachable[0] = 1;
        while (!worklist.empty()) {
            if (qore_ir_analysis_cancelled(check_count, "IR reachability analysis")) {
                cancelled = true;
                return;
            }
            size_t block_id = worklist.back();
            worklist.pop_back();
            for (size_t successor : successors[block_id]) {
                if (!reachable[successor]) {
                    reachable[successor] = 1;
                    worklist.push_back(successor);
                }
            }
        }
    }

    const size_t word_count = (blocks.size() + 63) / 64;
    dominators.assign(blocks.size(), std::vector<uint64_t>(word_count, 0));
    for (size_t block_id = 0; block_id < blocks.size(); ++block_id) {
        if (qore_ir_analysis_cancelled(check_count, "IR dominator analysis")) {
            cancelled = true;
            return;
        }
        if (!reachable[block_id]) {
            if (word_count) {
                dominators[block_id][block_id / 64] |= uint64_t(1) << (block_id % 64);
            }
            continue;
        }
        if (block_id == 0) {
            dominators[block_id][0] = 1;
            continue;
        }
        for (size_t candidate = 0; candidate < blocks.size(); ++candidate) {
            if (reachable[candidate]) {
                dominators[block_id][candidate / 64] |= uint64_t(1) << (candidate % 64);
            }
        }
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t block_id = 1; block_id < blocks.size(); ++block_id) {
            if (qore_ir_analysis_cancelled(check_count, "IR dominator analysis")) {
                cancelled = true;
                return;
            }
            if (!reachable[block_id]) {
                continue;
            }
            std::vector<uint64_t> next(word_count, std::numeric_limits<uint64_t>::max());
            bool saw_predecessor = false;
            for (size_t predecessor : predecessors[block_id]) {
                if (!reachable[predecessor]) {
                    continue;
                }
                saw_predecessor = true;
                for (size_t word = 0; word < word_count; ++word) {
                    next[word] &= dominators[predecessor][word];
                }
            }
            if (!saw_predecessor) {
                std::fill(next.begin(), next.end(), 0);
            }
            next[block_id / 64] |= uint64_t(1) << (block_id % 64);
            if (next != dominators[block_id]) {
                dominators[block_id] = std::move(next);
                changed = true;
            }
        }
    }
}

bool QoreIRControlFlowGraph::dominates(size_t dominator, size_t block) const {
    return dominator < blocks.size() && block < blocks.size() && reachable[block]
        && (dominators[block][dominator / 64] & (uint64_t(1) << (dominator % 64)));
}

std::vector<QoreIRNaturalLoop> qore_ir_find_natural_loops(const QoreIRControlFlowGraph& cfg) {
    if (cfg.cancelled) {
        return {};
    }
    std::unordered_map<size_t, std::unordered_set<size_t>> loops_by_header;
    size_t check_count = 0;
    for (size_t tail = 0; tail < cfg.blocks.size(); ++tail) {
        if (qore_ir_analysis_cancelled(check_count, "IR natural-loop analysis")) {
            return {};
        }
        if (!cfg.reachable[tail]) {
            continue;
        }
        for (size_t header : cfg.successors[tail]) {
            if (!cfg.dominates(header, tail)) {
                continue;
            }
            auto& loop_blocks = loops_by_header[header];
            loop_blocks.insert(header);
            if (loop_blocks.insert(tail).second && tail != header) {
                std::vector<size_t> worklist{tail};
                while (!worklist.empty()) {
                    if (qore_ir_analysis_cancelled(check_count, "IR natural-loop analysis")) {
                        return {};
                    }
                    size_t block = worklist.back();
                    worklist.pop_back();
                    for (size_t predecessor : cfg.predecessors[block]) {
                        if (loop_blocks.insert(predecessor).second && predecessor != header) {
                            worklist.push_back(predecessor);
                        }
                    }
                }
            }
        }
    }

    std::vector<QoreIRNaturalLoop> loops;
    for (auto& [header, loop_blocks] : loops_by_header) {
        if (qore_ir_analysis_cancelled(check_count, "IR natural-loop analysis")) {
            return {};
        }
        std::vector<size_t> outside_predecessors;
        for (size_t predecessor : cfg.predecessors[header]) {
            if (!loop_blocks.count(predecessor)) {
                outside_predecessors.push_back(predecessor);
            }
        }
        if (outside_predecessors.size() != 1) {
            continue;
        }
        size_t preheader = outside_predecessors.front();
        if (cfg.successors[preheader].size() != 1 || cfg.successors[preheader].front() != header) {
            continue;
        }
        QoreIRBasicBlock* preheader_block = cfg.blocks[preheader];
        if (!preheader_block || preheader_block->instructions.empty()
                || preheader_block->instructions.back()->opcode != QoreIROpcode::Br) {
            continue;
        }
        // The LLVM emitter currently resolves ordinary SSA operands in block-list
        // order.  Keep the preheader before every loop block until that backend
        // contract is replaced by dominance-order lowering.
        if (std::any_of(loop_blocks.begin(), loop_blocks.end(), [preheader](size_t block) {
                return block < preheader;
            })) {
            continue;
        }
        QoreIRNaturalLoop loop;
        loop.header = header;
        loop.preheader = preheader;
        loop.blocks.assign(loop_blocks.begin(), loop_blocks.end());
        std::sort(loop.blocks.begin(), loop.blocks.end());
        loops.push_back(std::move(loop));
    }
    std::sort(loops.begin(), loops.end(), [](const QoreIRNaturalLoop& left, const QoreIRNaturalLoop& right) {
        return left.blocks.size() < right.blocks.size();
    });
    return loops;
}

static bool qore_ir_is_native_scalar_pure_opcode(QoreIROpcode opcode) {
    switch (opcode) {
        case QoreIROpcode::AddInt:
        case QoreIROpcode::AddFloat:
        case QoreIROpcode::SubInt:
        case QoreIROpcode::SubFloat:
        case QoreIROpcode::MulInt:
        case QoreIROpcode::MulFloat:
        case QoreIROpcode::AndInt:
        case QoreIROpcode::OrInt:
        case QoreIROpcode::XorInt:
        case QoreIROpcode::ShlInt:
        case QoreIROpcode::ShrInt:
        case QoreIROpcode::EqInt:
        case QoreIROpcode::EqFloat:
        case QoreIROpcode::NeInt:
        case QoreIROpcode::NeFloat:
        case QoreIROpcode::LtInt:
        case QoreIROpcode::LtFloat:
        case QoreIROpcode::LeInt:
        case QoreIROpcode::LeFloat:
        case QoreIROpcode::GtInt:
        case QoreIROpcode::GtFloat:
        case QoreIROpcode::GeInt:
        case QoreIROpcode::GeFloat:
        case QoreIROpcode::CmpInt:
        case QoreIROpcode::CmpFloat:
        case QoreIROpcode::ToBool:
        case QoreIROpcode::Not:
        case QoreIROpcode::UnaryMinusInt:
        case QoreIROpcode::UnaryMinusFloat:
            return true;
        default:
            return false;
    }
}

static bool qore_ir_is_native_scalar_constant(QoreIROpcode opcode) {
    return opcode == QoreIROpcode::ConstInt || opcode == QoreIROpcode::ConstFloat
        || opcode == QoreIROpcode::ConstBool;
}

struct QoreIRScalarUse {
    const QoreIRInstruction* inst = nullptr;
    size_t block_id = 0;
};

using QoreIRScalarUses = std::unordered_map<uint32_t, std::vector<QoreIRScalarUse>>;

static bool qore_ir_collect_scalar_uses(const QoreIRFunction& func, QoreIRScalarUses& uses,
        size_t& check_count) {
    uses.clear();
    for (size_t block_id = 0; block_id < func.blocks.size(); ++block_id) {
        for (const auto& inst : func.blocks[block_id]->instructions) {
            if (qore_ir_analysis_cancelled(check_count, "IR scalar use analysis")) {
                return false;
            }
            if (!qore_ir_visit_value_operands(*inst, [&](QoreIRValue operand) {
                if (operand.isValid()) {
                    uses[operand.id].push_back({inst.get(), block_id});
                }
            }, &check_count, "IR scalar use analysis")) {
                return false;
            }
        }
    }
    return true;
}

static bool qore_ir_is_nonconsuming_scalar_use(const QoreIRFunction& func,
        const QoreIRInstruction& inst, bool allow_ir_only_store) {
    if (qore_ir_is_native_scalar_pure_opcode(inst.opcode)) {
        return true;
    }
    switch (inst.opcode) {
        case QoreIROpcode::BrIf:
        case QoreIROpcode::SwitchInt:
        case QoreIROpcode::AddAssignLocalInt:
            return true;
        case QoreIROpcode::StoreLocal: {
            if (!allow_ir_only_store) {
                return false;
            }
            const auto& store = static_cast<const QoreIRLocalInstruction&>(inst);
            return store.local
                && func.ir_only_locals.count(reinterpret_cast<const void*>(store.local));
        }
        default:
            return false;
    }
}

static bool qore_ir_has_only_nonconsuming_scalar_uses(const QoreIRFunction& func,
        QoreIRValue result, const QoreIRScalarUses& uses, bool allow_ir_only_store,
        size_t& check_count, bool& cancelled,
        const std::unordered_set<size_t>* required_blocks = nullptr) {
    auto use_it = uses.find(result.id);
    if (use_it == uses.end() || use_it->second.empty()) {
        return false;
    }
    for (const QoreIRScalarUse& use : use_it->second) {
        if (qore_ir_analysis_cancelled(check_count, "IR scalar use validation")) {
            cancelled = true;
            return false;
        }
        if (!use.inst || (required_blocks && !required_blocks->count(use.block_id))
                || !qore_ir_is_nonconsuming_scalar_use(func, *use.inst, allow_ir_only_store)) {
            return false;
        }
    }
    return true;
}

static void qore_ir_rewrite_value(QoreIRValue& value,
        const std::unordered_map<uint32_t, QoreIRValue>& replacements) {
    auto it = replacements.find(value.id);
    if (it != replacements.end()) {
        value = it->second;
    }
}

static bool qore_ir_rewrite_value_operands(QoreIRInstruction& inst,
        const std::unordered_map<uint32_t, QoreIRValue>& replacements,
        size_t& check_count, bool honor_cancellation) {
    for (QoreIRValue& operand : inst.operands) {
        if (qore_ir_analysis_cancelled(check_count, "IR scalar common-expression elimination")
                && honor_cancellation) {
            return false;
        }
        qore_ir_rewrite_value(operand, replacements);
    }

    switch (inst.opcode) {
        case QoreIROpcode::BrIf:
            qore_ir_rewrite_value(static_cast<QoreIRBranchIfInstruction&>(inst).condition, replacements);
            break;
        case QoreIROpcode::SwitchInt:
            qore_ir_rewrite_value(static_cast<QoreIRSwitchIntInstruction&>(inst).switch_val, replacements);
            break;
        case QoreIROpcode::SwitchString:
            qore_ir_rewrite_value(static_cast<QoreIRSwitchStringInstruction&>(inst).switch_val, replacements);
            break;
        case QoreIROpcode::IteratorCreate:
        case QoreIROpcode::IteratorCreateIterate:
            qore_ir_rewrite_value(static_cast<QoreIRIteratorCreateInstruction&>(inst).iterable, replacements);
            break;
        case QoreIROpcode::IteratorNext:
            qore_ir_rewrite_value(static_cast<QoreIRIteratorNextInstruction&>(inst).iterator, replacements);
            break;
        case QoreIROpcode::TypedForeachNextInt:
        case QoreIROpcode::TypedForeachNextFloat:
        case QoreIROpcode::TypedForeachNextBool:
        case QoreIROpcode::TypedForeachNextString: {
            auto& next = static_cast<QoreIRIteratorNextInstruction&>(inst);
            if (next.operands.size() == 3) {
                next.iterator = next.operands[0];
                next.index = next.operands[1];
                next.limit = next.operands[2];
            }
            break;
        }
        case QoreIROpcode::Phi: {
            auto& phi = static_cast<QoreIRPhiInstruction&>(inst);
            for (QoreIRPhiIncoming& incoming : phi.incoming) {
                if (qore_ir_analysis_cancelled(check_count, "IR scalar common-expression elimination")
                        && honor_cancellation) {
                    return false;
                }
                qore_ir_rewrite_value(incoming.value, replacements);
            }
            break;
        }
        case QoreIROpcode::LValuePathAssign:
        case QoreIROpcode::LValuePathCompound:
        case QoreIROpcode::LValuePathUnary:
        case QoreIROpcode::LValuePathBinaryMut:
        case QoreIROpcode::LValuePathTernary: {
            auto& path = static_cast<QoreIRLValuePathInstruction&>(inst);
            for (LVPathStep& step : path.path) {
                if (qore_ir_analysis_cancelled(check_count, "IR scalar common-expression elimination")
                        && honor_cancellation) {
                    return false;
                }
                if (step.operand_idx != UINT32_MAX) {
                    QoreIRValue operand(step.operand_idx);
                    qore_ir_rewrite_value(operand, replacements);
                    step.operand_idx = operand.id;
                }
                for (uint32_t& id : step.slice_operand_ids) {
                    if (qore_ir_analysis_cancelled(check_count, "IR scalar common-expression elimination")
                            && honor_cancellation) {
                        return false;
                    }
                    QoreIRValue operand(id);
                    qore_ir_rewrite_value(operand, replacements);
                    id = operand.id;
                }
            }
            break;
        }
        case QoreIROpcode::Return: {
            auto& ret = static_cast<QoreIRReturnInstruction&>(inst);
            if (ret.has_value) {
                qore_ir_rewrite_value(ret.value, replacements);
            }
            break;
        }
        default:
            break;
    }
    return true;
}

struct QoreIRFixedAggregateScalarizationStats {
    size_t lists = 0;
    size_t hashes = 0;
};

static QoreIRFixedAggregateScalarizationStats qore_ir_scalar_replace_fixed_aggregates(
        QoreIRFunction& func, const QoreIRControlFlowGraph& cfg, size_t& check_count) {
    QoreIRFixedAggregateScalarizationStats stats;
    const bool enable_lists =
        std::getenv("QORE_DISABLE_IR_FIXED_LIST_SCALAR_REPLACEMENT") == nullptr;
    const bool enable_hashes =
        std::getenv("QORE_DISABLE_IR_FIXED_HASH_SCALAR_REPLACEMENT") == nullptr;
    if (!enable_lists && !enable_hashes) {
        return stats;
    }

    struct InstructionPosition {
        QoreIRInstruction* inst = nullptr;
        size_t block = 0;
        size_t offset = 0;
    };
    auto dominates = [&](const InstructionPosition& definition,
            const InstructionPosition& use, bool cross_block) {
        return definition.block == use.block
            ? definition.offset < use.offset
            : cross_block && cfg.dominates(definition.block, use.block);
    };
    std::unordered_map<uint32_t, InstructionPosition> definitions;
    std::unordered_map<const LocalVar*, std::vector<InstructionPosition>> local_accesses;
    std::unordered_set<const LocalVar*> path_locals;
    for (size_t block_id = 0; block_id < func.blocks.size(); ++block_id) {
        for (size_t offset = 0; offset < func.blocks[block_id]->instructions.size(); ++offset) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR fixed-aggregate scalar replacement analysis")) {
                return {};
            }
            QoreIRInstruction* inst = func.blocks[block_id]->instructions[offset].get();
            if (inst->result.isValid()) {
                definitions.emplace(inst->result.id,
                    InstructionPosition{inst, block_id, offset});
            }
            if (inst->opcode == QoreIROpcode::LoadLocal
                    || inst->opcode == QoreIROpcode::StoreLocal
                    || inst->opcode == QoreIROpcode::InstantiateLocal
                    || inst->opcode == QoreIROpcode::UninstantiateLocal) {
                auto* local_inst = static_cast<QoreIRLocalInstruction*>(inst);
                if (local_inst->local) {
                    local_accesses[local_inst->local].push_back(
                        {inst, block_id, offset});
                }
            }
            switch (inst->opcode) {
                case QoreIROpcode::LValuePathAssign:
                case QoreIROpcode::LValuePathCompound:
                case QoreIROpcode::LValuePathUnary:
                case QoreIROpcode::LValuePathBinaryMut:
                case QoreIROpcode::LValuePathTernary: {
                    const auto* path = static_cast<const QoreIRLValuePathInstruction*>(inst);
                    if (!path->path.empty()
                            && path->path.front().kind == LVPathStepKind::LocalVar) {
                        auto* local = reinterpret_cast<const LocalVar*>(
                            path->path.front().ref_ptr);
                        if (local) {
                            path_locals.insert(local);
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    QoreIRScalarUses uses;
    if (!qore_ir_collect_scalar_uses(func, uses, check_count)) {
        return {};
    }
    std::unordered_set<const QoreIRInstruction*> eliminated;
    std::unordered_map<uint32_t, QoreIRValue> replacements;
    size_t scalarized = 0;

    for (const auto& [local, accesses] : local_accesses) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR fixed-aggregate scalar replacement candidate analysis")) {
            return {};
        }
        if (!local || path_locals.count(local) || local->closureUse()
                || QoreTypeInfo::isReference(local->getTypeInfo())
                || !func.ir_only_locals.count(reinterpret_cast<const void*>(local))) {
            continue;
        }
        const QoreTypeInfo* element_type = nullptr;
        bool hash_candidate = false;
        if (enable_lists) {
            element_type =
                QoreTypeInfo::getUniqueReturnComplexList(local->getTypeInfo());
        }
        if (!element_type && enable_hashes) {
            element_type =
                QoreTypeInfo::getUniqueReturnComplexHash(local->getTypeInfo());
            hash_candidate = element_type != nullptr;
        }
        if (!element_type) {
            continue;
        }
        const bool cross_block = hash_candidate
            ? std::getenv(
                "QORE_DISABLE_IR_CROSS_BLOCK_FIXED_HASH_SCALAR_REPLACEMENT")
                == nullptr
            : std::getenv(
                "QORE_DISABLE_IR_CROSS_BLOCK_FIXED_LIST_SCALAR_REPLACEMENT")
                == nullptr;
        QoreIRValueRepresentation expected_representation;
        if (element_type == bigIntTypeInfo) {
            expected_representation = QoreIRValueRepresentation::NativeInt;
        } else if (element_type == floatTypeInfo) {
            expected_representation = QoreIRValueRepresentation::NativeFloat;
        } else if (element_type == boolTypeInfo) {
            expected_representation = QoreIRValueRepresentation::NativeBool;
        } else {
            continue;
        }

        const InstructionPosition* store_pos = nullptr;
        std::vector<const InstructionPosition*> load_positions;
        std::vector<const QoreIRInstruction*> local_cleanup;
        bool invalid = false;
        for (const InstructionPosition& position : accesses) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR fixed-aggregate scalar replacement local analysis")) {
                return {};
            }
            if (position.inst->opcode == QoreIROpcode::StoreLocal) {
                if (store_pos) {
                    invalid = true;
                    break;
                }
                store_pos = &position;
            } else if (position.inst->opcode == QoreIROpcode::LoadLocal) {
                load_positions.push_back(&position);
            } else if (position.inst->opcode == QoreIROpcode::UninstantiateLocal) {
                local_cleanup.push_back(position.inst);
            } else {
                invalid = true;
                break;
            }
        }
        if (invalid || !store_pos || load_positions.empty()
                || store_pos->inst->operands.size() != 1
                || store_pos->inst->result.isValid()
                || static_cast<const QoreIRLocalInstruction*>(
                    store_pos->inst)->weak) {
            continue;
        }
        auto make_it = definitions.find(store_pos->inst->operands[0].id);
        if (make_it == definitions.end()
                || make_it->second.inst->opcode
                    != (hash_candidate ? QoreIROpcode::MakeHashConstKeys
                        : QoreIROpcode::MakeList)
                || make_it->second.inst->exception_target
                || make_it->second.block != store_pos->block
                || make_it->second.offset >= store_pos->offset
                || make_it->second.inst->operands.empty()) {
            continue;
        }
        QoreIRInstruction* make = make_it->second.inst;
        const QoreIRMakeHashConstKeysInstruction* make_hash = hash_candidate
            ? static_cast<const QoreIRMakeHashConstKeysInstruction*>(make)
            : nullptr;
        if (make_hash && make_hash->keys.size() != make->operands.size()) {
            continue;
        }
        auto make_uses = uses.find(make->result.id);
        if (make_uses == uses.end() || make_uses->second.size() != 1
                || make_uses->second.front().inst != store_pos->inst) {
            continue;
        }
        for (QoreIRValue operand : make->operands) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR fixed-aggregate scalar operand analysis")) {
                return {};
            }
            const QoreIRValueFacts* facts = func.getValueFacts(operand);
            if (!facts || facts->representation != expected_representation
                    || facts->assigned_state != QoreIRAssignedState::Assigned
                    || !facts->never_nothing) {
                invalid = true;
                break;
            }
        }
        if (invalid) {
            continue;
        }

        std::vector<const QoreIRInstruction*> reads;
        std::unordered_map<uint32_t, QoreIRValue> candidate_replacements;
        std::unordered_map<std::string, QoreIRValue> hash_values;
        if (make_hash) {
            for (size_t i = 0; i < make_hash->keys.size(); ++i) {
                if (qore_ir_analysis_cancelled(check_count,
                        "IR fixed-hash scalar key analysis")) {
                    return {};
                }
                hash_values[make_hash->keys[i]] = make->operands[i];
            }
        }
        for (const InstructionPosition* load_pos : load_positions) {
            if (!dominates(*store_pos, *load_pos, cross_block)
                    || load_pos->inst->exception_target) {
                invalid = true;
                break;
            }
            auto load_uses = uses.find(load_pos->inst->result.id);
            if (load_uses == uses.end() || load_uses->second.empty()) {
                invalid = true;
                break;
            }
            for (const QoreIRScalarUse& use : load_uses->second) {
                if (qore_ir_analysis_cancelled(check_count,
                        "IR fixed-aggregate scalar read analysis")) {
                    return {};
                }
                bool matching_read = use.inst && !use.inst->exception_target
                    && use.inst->result.isValid()
                    && !use.inst->operands.empty()
                    && use.inst->operands[0].id
                        == load_pos->inst->result.id;
                if (matching_read && hash_candidate) {
                    matching_read = use.inst->operands.size() == 1
                        && (use.inst->opcode == QoreIROpcode::HashKeyAccess
                            || use.inst->opcode
                                == QoreIROpcode::HashKeyAccessInt
                            || use.inst->opcode
                                == QoreIROpcode::HashKeyAccessHash
                            || use.inst->opcode
                                == QoreIROpcode::HashKeyAccessHashGuarded);
                } else if (matching_read) {
                    matching_read = use.inst->operands.size() == 2
                        && use.inst->opcode
                            == QoreIROpcode::ListIndexDynamic;
                }
                if (!matching_read) {
                    invalid = true;
                    break;
                }
                auto read_position = definitions.find(use.inst->result.id);
                if (read_position == definitions.end()
                        || !dominates(
                            *load_pos, read_position->second, cross_block)) {
                    invalid = true;
                    break;
                }
                QoreIRValue replacement;
                if (hash_candidate) {
                    const auto* access =
                        static_cast<const QoreIRHashKeyAccessInstruction*>(
                            use.inst);
                    auto value_it = hash_values.find(access->key_name);
                    if (value_it == hash_values.end()) {
                        invalid = true;
                        break;
                    }
                    replacement = value_it->second;
                } else {
                    const auto* index_inst =
                        static_cast<const QoreIRExprInstruction*>(use.inst);
                    if (!index_inst->list_selector_kinds.empty()) {
                        invalid = true;
                        break;
                    }
                    auto index_it = definitions.find(use.inst->operands[1].id);
                    if (index_it == definitions.end()
                            || index_it->second.inst->opcode
                                != QoreIROpcode::ConstInt) {
                        invalid = true;
                        break;
                    }
                    int64_t index = static_cast<const QoreIRConstInstruction*>(
                        index_it->second.inst)->constant.int_value;
                    if (index < 0
                            || static_cast<size_t>(index)
                                >= make->operands.size()) {
                        invalid = true;
                        break;
                    }
                    replacement =
                        make->operands[static_cast<size_t>(index)];
                }
                candidate_replacements.emplace(use.inst->result.id,
                    replacement);
                reads.push_back(use.inst);
            }
            if (invalid) {
                break;
            }
        }
        if (invalid || candidate_replacements.empty()) {
            continue;
        }
        eliminated.insert(make);
        eliminated.insert(store_pos->inst);
        for (const InstructionPosition* load_pos : load_positions) {
            eliminated.insert(load_pos->inst);
        }
        eliminated.insert(reads.begin(), reads.end());
        eliminated.insert(local_cleanup.begin(), local_cleanup.end());
        replacements.insert(candidate_replacements.begin(), candidate_replacements.end());
        ++scalarized;
        if (hash_candidate) {
            ++stats.hashes;
        } else {
            ++stats.lists;
        }
    }

    if (!scalarized) {
        return stats;
    }
    for (auto& [result_id, replacement] : replacements) {
        std::unordered_set<uint32_t> seen{result_id};
        while (true) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR fixed-aggregate scalar replacement normalization")) {
                return {};
            }
            auto next = replacements.find(replacement.id);
            if (next == replacements.end()) {
                break;
            }
            if (!seen.insert(replacement.id).second) {
                return {};
            }
            replacement = next->second;
        }
    }
    for (const auto& block : func.blocks) {
        auto& instructions = block->instructions;
        for (auto it = instructions.begin(); it != instructions.end();) {
            // Complete the committed rewrite even if cancellation is requested.
            if (++check_count % 100 == 0) {
                (void)qore_check_cancel(
                    nullptr, "IR fixed-aggregate scalar replacement");
            }
            if (eliminated.count(it->get())) {
                it = instructions.erase(it);
            } else {
                (void)qore_ir_rewrite_value_operands(
                    **it, replacements, check_count, false);
                ++it;
            }
        }
    }
    return stats;
}

static size_t qore_ir_fold_scalar_list_queries(QoreIRFunction& func,
        size_t& check_count) {
    if (std::getenv("QORE_DISABLE_IR_SCALAR_LIST_QUERY_FOLDING")) {
        return 0;
    }

    std::unordered_map<uint32_t, QoreIRInstruction*> definitions;
    for (const auto& block : func.blocks) {
        for (const auto& inst : block->instructions) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR scalar-list query definition analysis")) {
                return 0;
            }
            if (inst->result.isValid()) {
                definitions.emplace(inst->result.id, inst.get());
            }
        }
    }

    QoreIRScalarUses uses;
    if (!qore_ir_collect_scalar_uses(func, uses, check_count)) {
        return 0;
    }

    enum class FoldedScalarListQueryKind {
        Size,
        BoxedBool,
    };
    struct FoldedScalarListQuery {
        FoldedScalarListQueryKind kind = FoldedScalarListQueryKind::Size;
        int64_t size = 0;
        bool bool_value = false;
    };

    std::unordered_set<const QoreIRInstruction*> eliminated;
    std::unordered_map<const QoreIRInstruction*, FoldedScalarListQuery> folded;
    for (const auto& [result_id, definition] : definitions) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR scalar-list query candidate analysis")) {
            return 0;
        }
        if (!definition || definition->opcode != QoreIROpcode::MakeList
                || definition->exception_target || definition->operands.size() > 100) {
            continue;
        }
        const auto* make = static_cast<const QoreIRMakeListInstruction*>(definition);
        QoreIRValueRepresentation expected = QoreIRValueRepresentation::Unknown;
        if (make->typeInfo) {
            const QoreTypeInfo* element_type =
                QoreTypeInfo::getUniqueReturnComplexList(make->typeInfo);
            if (element_type == bigIntTypeInfo) {
                expected = QoreIRValueRepresentation::NativeInt;
            } else if (element_type == floatTypeInfo) {
                expected = QoreIRValueRepresentation::NativeFloat;
            } else if (element_type == boolTypeInfo) {
                expected = QoreIRValueRepresentation::NativeBool;
            } else if (element_type && element_type != autoTypeInfo
                    && element_type != anyTypeInfo) {
                continue;
            }
        }
        bool safe_operands = true;
        for (QoreIRValue operand : definition->operands) {
            const QoreIRValueFacts* facts = func.getValueFacts(operand);
            if (!facts || facts->assigned_state != QoreIRAssignedState::Assigned
                    || !facts->never_nothing
                    || (facts->representation != QoreIRValueRepresentation::NativeInt
                        && facts->representation != QoreIRValueRepresentation::NativeFloat
                        && facts->representation != QoreIRValueRepresentation::NativeBool)
                    || (expected != QoreIRValueRepresentation::Unknown
                        && facts->representation != expected)) {
                safe_operands = false;
                break;
            }
        }
        if (!safe_operands) {
            continue;
        }

        auto use_it = uses.find(result_id);
        if (use_it == uses.end() || use_it->second.size() != 1
                || !use_it->second.front().inst) {
            continue;
        }
        const QoreIRInstruction* query = use_it->second.front().inst;
        if (query->exception_target || !query->result.isValid()
                || query->operands.size() != 1
                || query->operands[0].id != result_id) {
            continue;
        }

        FoldedScalarListQuery replacement;
        if (query->opcode == QoreIROpcode::ListSize) {
            replacement.size = static_cast<int64_t>(definition->operands.size());
        } else if (query->opcode == QoreIROpcode::DotEvalMethodDirect) {
            const auto* direct = static_cast<const QoreIRDotEvalMethodDirectInstruction*>(query);
            const char* method_name = direct->method
                ? direct->method->getName() : direct->fallback_method_name;
            if (!direct->pseudo || !method_name
                    || (strcmp(method_name, "empty") && strcmp(method_name, "val"))) {
                continue;
            }
            replacement.kind = FoldedScalarListQueryKind::BoxedBool;
            bool nonempty = !definition->operands.empty();
            replacement.bool_value = !strcmp(method_name, "empty") ? !nonempty : nonempty;
        } else {
            continue;
        }
        eliminated.insert(definition);
        folded.emplace(query, replacement);
    }

    if (folded.empty()) {
        return 0;
    }
    for (const auto& block : func.blocks) {
        auto& instructions = block->instructions;
        for (auto it = instructions.begin(); it != instructions.end();) {
            // Complete the committed rewrite even if cancellation is requested.
            if (++check_count % 100 == 0) {
                (void)qore_check_cancel(nullptr, "IR scalar-list query folding");
            }
            if (eliminated.count(it->get())) {
                it = instructions.erase(it);
                continue;
            }
            auto fold_it = folded.find(it->get());
            if (fold_it == folded.end()) {
                ++it;
                continue;
            }
            auto replacement = std::make_unique<QoreIRConstInstruction>();
            replacement->loc = (*it)->loc;
            replacement->result = (*it)->result;
            QoreIRValueFacts facts;
            facts.assigned_state = QoreIRAssignedState::Assigned;
            facts.never_nothing = true;
            if (fold_it->second.kind == FoldedScalarListQueryKind::Size) {
                replacement->opcode = QoreIROpcode::ConstInt;
                replacement->constant.kind = QoreIRConstant::Kind::Int;
                replacement->constant.int_value = fold_it->second.size;
                facts.type_info = bigIntTypeInfo;
                facts.representation = QoreIRValueRepresentation::NativeInt;
            } else {
                replacement->opcode = QoreIROpcode::ConstBoolBoxed;
                replacement->constant.kind = QoreIRConstant::Kind::Bool;
                replacement->constant.bool_value = fold_it->second.bool_value;
                facts.type_info = boolTypeInfo;
                facts.representation = QoreIRValueRepresentation::Boxed;
            }
            func.setValueFacts(replacement->result, facts);
            *it = std::move(replacement);
            ++it;
        }
    }
    return folded.size();
}

struct QoreIRScalarExpressionKey {
    QoreIROpcode opcode = QoreIROpcode::ConstNothing;
    uint64_t constant_bits = 0;
    std::vector<uint32_t> operands;

    bool operator==(const QoreIRScalarExpressionKey& other) const {
        return opcode == other.opcode && constant_bits == other.constant_bits
            && operands == other.operands;
    }
};

struct QoreIRScalarExpressionKeyHash {
    size_t operator()(const QoreIRScalarExpressionKey& key) const {
        size_t hash = static_cast<size_t>(key.opcode) * 0x9e3779b1U;
        hash ^= static_cast<size_t>(key.constant_bits ^ (key.constant_bits >> 32));
        for (uint32_t operand : key.operands) {
            hash ^= static_cast<size_t>(operand) + 0x9e3779b9U + (hash << 6) + (hash >> 2);
        }
        return hash;
    }
};

static QoreIRScalarExpressionKey qore_ir_get_scalar_expression_key(const QoreIRInstruction& inst) {
    QoreIRScalarExpressionKey key;
    key.opcode = inst.opcode;
    key.operands.reserve(inst.operands.size());
    for (QoreIRValue operand : inst.operands) {
        key.operands.push_back(operand.id);
    }
    if (qore_ir_is_native_scalar_constant(inst.opcode)) {
        const auto& constant = static_cast<const QoreIRConstInstruction&>(inst).constant;
        if (inst.opcode == QoreIROpcode::ConstInt) {
            key.constant_bits = static_cast<uint64_t>(constant.int_value);
        } else if (inst.opcode == QoreIROpcode::ConstFloat) {
            key.constant_bits = std::bit_cast<uint64_t>(constant.float_value);
        } else {
            key.constant_bits = constant.bool_value ? 1 : 0;
        }
    }
    return key;
}

static bool qore_ir_is_forwardable_scalar_load(const QoreIRFunction& func,
        const QoreIRInstruction& inst) {
    if (inst.opcode != QoreIROpcode::LoadLocal) {
        return false;
    }
    const auto& load = static_cast<const QoreIRLocalInstruction&>(inst);
    if (!load.local || load.is_closure || load.is_ref
            || !func.ir_only_locals.count(reinterpret_cast<const void*>(load.local))) {
        return false;
    }
    const QoreIRValueFacts* facts = func.getValueFacts(inst.result);
    if (!facts || facts->assigned_state != QoreIRAssignedState::Assigned || !facts->never_nothing) {
        return false;
    }
    return facts->representation == QoreIRValueRepresentation::NativeInt
        || facts->representation == QoreIRValueRepresentation::NativeFloat
        || facts->representation == QoreIRValueRepresentation::NativeBool;
}

static bool qore_ir_may_mutate_unknown_local(QoreIROpcode opcode) {
    switch (opcode) {
        case QoreIROpcode::NewObject:
        case QoreIROpcode::CreateParseRef:
        case QoreIROpcode::Call:
        case QoreIROpcode::CallIndirect:
        case QoreIROpcode::CallDirect:
        case QoreIROpcode::CallMethod:
        case QoreIROpcode::CallMethodDirect:
        case QoreIROpcode::InvokeMethodDirect:
        case QoreIROpcode::CallStatic:
        case QoreIROpcode::CallStaticDirect:
        case QoreIROpcode::DotEvalAny:
        case QoreIROpcode::DotEvalInt:
        case QoreIROpcode::DotEvalFloat:
        case QoreIROpcode::DotEvalString:
        case QoreIROpcode::DotEvalDate:
        case QoreIROpcode::DotEvalList:
        case QoreIROpcode::DotEvalHash:
        case QoreIROpcode::DotEvalObject:
        case QoreIROpcode::DotEvalMethodDirect:
        case QoreIROpcode::InvokeDotEvalMethodDirect:
        case QoreIROpcode::CallClosureDirect:
        case QoreIROpcode::BackgroundInt:
        case QoreIROpcode::Invoke:
        case QoreIROpcode::InvokeSimError:
        case QoreIROpcode::OnBlockExit:
        case QoreIROpcode::ScopeExit:
        case QoreIROpcode::Backquote:
        case QoreIROpcode::StoreLValue:
        case QoreIROpcode::PreIncLValue:
        case QoreIROpcode::PreDecLValue:
        case QoreIROpcode::PostIncLValue:
        case QoreIROpcode::PostDecLValue:
        case QoreIROpcode::AddAssignLValue:
        case QoreIROpcode::SubAssignLValue:
        case QoreIROpcode::MulAssignLValue:
        case QoreIROpcode::DivAssignLValue:
        case QoreIROpcode::ModAssignLValue:
        case QoreIROpcode::AndAssignLValue:
        case QoreIROpcode::OrAssignLValue:
        case QoreIROpcode::XorAssignLValue:
        case QoreIROpcode::ShlAssignLValue:
        case QoreIROpcode::ShrAssignLValue:
        case QoreIROpcode::ShiftLValue:
        case QoreIROpcode::UnshiftLValue:
        case QoreIROpcode::PopAny:
        case QoreIROpcode::PushAny:
        case QoreIROpcode::SpliceLValue:
        case QoreIROpcode::LValuePathAssign:
        case QoreIROpcode::LValuePathCompound:
        case QoreIROpcode::LValuePathUnary:
        case QoreIROpcode::LValuePathBinaryMut:
        case QoreIROpcode::LValuePathTernary:
            return true;
        default:
            return false;
    }
}

struct QoreIRScalarCSEStats {
    size_t loads_forwarded = 0;
    size_t expressions_eliminated = 0;
};

static size_t qore_ir_fold_constant_branches(QoreIRFunction& func) {
    size_t check_count = 0;
    std::unordered_map<uint32_t, bool> constants;
    for (const auto& block : func.blocks) {
        if (qore_ir_analysis_cancelled(check_count, "IR constant branch folding")) {
            return 0;
        }
        for (const auto& inst : block->instructions) {
            if (qore_ir_analysis_cancelled(check_count, "IR constant branch folding")) {
                return 0;
            }
            if (inst->opcode == QoreIROpcode::ConstBool && inst->result.isValid()) {
                constants.emplace(inst->result.id,
                    static_cast<const QoreIRConstInstruction&>(*inst).constant.bool_value);
            }
        }
    }

    struct Replacement {
        std::unique_ptr<QoreIRInstruction>* slot = nullptr;
        QoreIRBasicBlock* source = nullptr;
        QoreIRBasicBlock* target = nullptr;
        QoreIRBasicBlock* removed_target = nullptr;
    };
    std::vector<Replacement> replacements;
    for (const auto& block : func.blocks) {
        if (qore_ir_analysis_cancelled(check_count, "IR constant branch folding")) {
            return 0;
        }
        if (block->instructions.empty()) {
            continue;
        }
        std::unique_ptr<QoreIRInstruction>& terminator = block->instructions.back();
        if (terminator->opcode != QoreIROpcode::BrIf || terminator->exception_target) {
            continue;
        }
        const auto& branch = static_cast<const QoreIRBranchIfInstruction&>(*terminator);
        auto constant = constants.find(branch.condition.id);
        if (constant == constants.end()) {
            continue;
        }
        QoreIRBasicBlock* target = constant->second ? branch.true_target : branch.false_target;
        QoreIRBasicBlock* removed_target = constant->second ? branch.false_target : branch.true_target;
        replacements.push_back({&terminator, block.get(), target,
            target == removed_target ? nullptr : removed_target});
    }

    size_t folded = 0;
    std::unordered_map<QoreIRBasicBlock*, std::unordered_set<QoreIRBasicBlock*>> removed_predecessors;
    for (const Replacement& replacement : replacements) {
        if (qore_ir_analysis_cancelled(check_count, "IR constant branch folding")) {
            break;
        }
        const QoreIRInstruction& original = **replacement.slot;
        auto branch = std::make_unique<QoreIRBranchInstruction>();
        branch->target = replacement.target;
        branch->cached_start_line = original.cached_start_line;
        branch->intrinsic = original.intrinsic;
        branch->loc = original.loc;
        branch->exception_target = original.exception_target;
        branch->element_type = original.element_type;
        branch->temp_scope_id = original.temp_scope_id;
        *replacement.slot = std::move(branch);
        if (replacement.removed_target) {
            removed_predecessors[replacement.removed_target].insert(replacement.source);
        }
        ++folded;
    }
    // Branch replacement is already committed; finish all matching phi updates
    // even after cancellation so the IR always has one incoming value per edge.
    for (const auto& [target, predecessors] : removed_predecessors) {
        (void)qore_ir_analysis_cancelled(check_count, "IR constant branch folding");
        for (const auto& inst : target->instructions) {
            (void)qore_ir_analysis_cancelled(check_count, "IR constant branch folding");
            if (inst->opcode != QoreIROpcode::Phi) {
                continue;
            }
            auto& phi = static_cast<QoreIRPhiInstruction&>(*inst);
            phi.incoming.erase(std::remove_if(phi.incoming.begin(), phi.incoming.end(),
                [&](const QoreIRPhiIncoming& incoming) {
                    (void)qore_ir_analysis_cancelled(check_count, "IR constant branch folding");
                    return predecessors.count(incoming.block);
                }), phi.incoming.end());
            phi.operands.clear();
            phi.operands.reserve(phi.incoming.size());
            for (const QoreIRPhiIncoming& incoming : phi.incoming) {
                (void)qore_ir_analysis_cancelled(check_count, "IR constant branch folding");
                phi.operands.push_back(incoming.value);
            }
        }
    }
    return folded;
}

static QoreIRScalarCSEStats qore_ir_eliminate_common_scalar_expressions(
        QoreIRFunction& func, const QoreIRControlFlowGraph& cfg) {
    size_t check_count = 0;
    QoreIRScalarUses uses;
    if (!qore_ir_collect_scalar_uses(func, uses, check_count)) {
        return {};
    }
    bool cancelled = false;
    std::unordered_map<uint32_t, QoreIRValue> replacements;
    std::unordered_set<const QoreIRInstruction*> eliminated;
    std::unordered_set<const QoreIRInstruction*> forwarded_loads;
    using ExpressionMap = std::unordered_map<QoreIRScalarExpressionKey, QoreIRValue,
        QoreIRScalarExpressionKeyHash>;
    using LoadMap = std::unordered_map<const LocalVar*, QoreIRValue>;
    struct AvailableState {
        ExpressionMap expressions;
        LoadMap loads;
    };
    std::vector<AvailableState> block_outputs(func.blocks.size());
    std::vector<uint8_t> processed(func.blocks.size(), 0);
    std::vector<uint8_t> discovered(func.blocks.size(), 0);
    std::vector<size_t> order;
    if (!func.blocks.empty()) {
        std::vector<size_t> worklist{0};
        discovered[0] = 1;
        while (!worklist.empty()) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR cross-block scalar traversal")) {
                return {};
            }
            size_t block_id = worklist.back();
            worklist.pop_back();
            order.push_back(block_id);
            for (size_t successor : cfg.successors[block_id]) {
                if (qore_ir_analysis_cancelled(check_count,
                        "IR cross-block scalar traversal")) {
                    return {};
                }
                if (!discovered[successor]) {
                    discovered[successor] = 1;
                    worklist.push_back(successor);
                }
            }
        }
    }
    for (size_t block_id = 0; block_id < func.blocks.size(); ++block_id) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR cross-block scalar traversal")) {
            return {};
        }
        if (!discovered[block_id]) {
            order.push_back(block_id);
        }
    }

    const bool cross_block = std::getenv("QORE_DISABLE_IR_CROSS_BLOCK_CSE") == nullptr;
    for (size_t block_id : order) {
        if (qore_ir_analysis_cancelled(check_count, "IR scalar common-expression elimination")) {
            return {};
        }
        ExpressionMap available;
        LoadMap available_loads;
        if (cross_block && cfg.reachable[block_id]
                && cfg.predecessors[block_id].size() == 1) {
            size_t predecessor = cfg.predecessors[block_id][0];
            if (predecessor != block_id && processed[predecessor]
                    && cfg.dominates(predecessor, block_id)) {
                if (cfg.successors[predecessor].size() == 1) {
                    available = std::move(block_outputs[predecessor].expressions);
                    available_loads = std::move(block_outputs[predecessor].loads);
                } else {
                    available = block_outputs[predecessor].expressions;
                    available_loads = block_outputs[predecessor].loads;
                }
            }
        }
        const auto& block = func.blocks[block_id];
        for (const auto& inst_ptr : block->instructions) {
            if (qore_ir_analysis_cancelled(check_count, "IR scalar common-expression elimination")) {
                return {};
            }
            QoreIRInstruction& inst = *inst_ptr;
            if (!qore_ir_rewrite_value_operands(inst, replacements, check_count, true)) {
                return {};
            }

            const LocalVar* written_local = nullptr;
            if (inst.opcode == QoreIROpcode::StoreLocal
                    || inst.opcode == QoreIROpcode::StoreClosure
                    || inst.opcode == QoreIROpcode::InstantiateLocal
                    || inst.opcode == QoreIROpcode::UninstantiateLocal) {
                written_local = static_cast<const QoreIRLocalInstruction&>(inst).local;
            } else if (inst.opcode == QoreIROpcode::AddAssignLocalInt) {
                written_local = static_cast<const QoreIRAddAssignLocalIntInstruction&>(inst).target;
            } else if (inst.opcode == QoreIROpcode::IncrementLocalInt) {
                written_local = static_cast<const QoreIRIncrementLocalIntInstruction&>(inst).local;
            }
            if (written_local) {
                available_loads.erase(written_local);
            } else if (qore_ir_may_mutate_unknown_local(inst.opcode)) {
                available_loads.clear();
            }

            if (qore_ir_is_forwardable_scalar_load(func, inst)
                    && qore_ir_has_only_nonconsuming_scalar_uses(func, inst.result, uses, true,
                        check_count, cancelled)) {
                const auto& load = static_cast<const QoreIRLocalInstruction&>(inst);
                auto [load_it, inserted] = available_loads.emplace(load.local, inst.result);
                if (!inserted) {
                    replacements.emplace(inst.result.id, load_it->second);
                    eliminated.insert(&inst);
                    forwarded_loads.insert(&inst);
                }
                continue;
            }
            if (cancelled) {
                return {};
            }

            bool candidate = inst.result.isValid() && !inst.exception_target
                && (qore_ir_is_native_scalar_constant(inst.opcode)
                    || qore_ir_is_native_scalar_pure_opcode(inst.opcode));
            if (!candidate
                    || !qore_ir_has_only_nonconsuming_scalar_uses(func, inst.result, uses, true,
                        check_count, cancelled)) {
                if (cancelled) {
                    return {};
                }
                continue;
            }
            QoreIRScalarExpressionKey key = qore_ir_get_scalar_expression_key(inst);
            auto [available_it, inserted] = available.emplace(std::move(key), inst.result);
            if (inserted) {
                continue;
            }
            replacements.emplace(inst.result.id, available_it->second);
            eliminated.insert(&inst);
        }
        block_outputs[block_id].expressions = std::move(available);
        block_outputs[block_id].loads = std::move(available_loads);
        processed[block_id] = 1;
    }

    if (eliminated.empty()) {
        return {};
    }

    for (const auto& block : func.blocks) {
        (void)qore_ir_analysis_cancelled(check_count, "IR scalar common-expression elimination");
        auto& instructions = block->instructions;
        for (auto it = instructions.begin(); it != instructions.end();) {
            // Once commit starts, finish all rewrites even if cancellation is
            // requested so no use can reference an erased definition.
            if (++check_count % 100 == 0) {
                (void)qore_check_cancel(nullptr, "IR scalar common-expression elimination");
            }
            if (eliminated.count(it->get())) {
                it = instructions.erase(it);
            } else {
                (void)qore_ir_rewrite_value_operands(**it, replacements, check_count, false);
                ++it;
            }
        }
    }
    return {
        forwarded_loads.size(),
        eliminated.size() - forwarded_loads.size(),
    };
}

static bool qore_ir_collect_mutated_locals(const QoreIRNaturalLoop& loop, const QoreIRControlFlowGraph& cfg,
        std::unordered_set<const LocalVar*>& mutated, size_t& check_count) {
    for (size_t block_id : loop.blocks) {
        for (const auto& inst_ptr : cfg.blocks[block_id]->instructions) {
            if (qore_ir_analysis_cancelled(check_count, "IR loop mutation analysis")) {
                return false;
            }
            const QoreIRInstruction* inst = inst_ptr.get();
            switch (inst->opcode) {
                case QoreIROpcode::StoreLocal:
                case QoreIROpcode::StoreClosure:
                case QoreIROpcode::UninstantiateLocal:
                case QoreIROpcode::InstantiateLocal:
                    mutated.insert(static_cast<const QoreIRLocalInstruction*>(inst)->local);
                    break;
                case QoreIROpcode::AddAssignLocalInt: {
                    const auto* local_inst = static_cast<const QoreIRAddAssignLocalIntInstruction*>(inst);
                    mutated.insert(local_inst->target);
                    break;
                }
                case QoreIROpcode::IncrementLocalInt:
                    mutated.insert(static_cast<const QoreIRIncrementLocalIntInstruction*>(inst)->local);
                    break;
                default:
                    break;
            }
        }
    }
    return true;
}

static bool qore_ir_is_hoistable_load(const QoreIRFunction& func, const QoreIRInstruction& inst,
        const std::unordered_set<const LocalVar*>& mutated) {
    if (inst.opcode != QoreIROpcode::LoadLocal) {
        return false;
    }
    const auto& load = static_cast<const QoreIRLocalInstruction&>(inst);
    if (!load.local || load.is_closure || load.is_ref || mutated.count(load.local)
            || !func.ir_only_locals.count(reinterpret_cast<const void*>(load.local))) {
        return false;
    }
    const QoreIRValueFacts* facts = func.getValueFacts(inst.result);
    if (!facts || facts->assigned_state != QoreIRAssignedState::Assigned || !facts->never_nothing) {
        return false;
    }
    return facts->representation == QoreIRValueRepresentation::NativeInt
        || facts->representation == QoreIRValueRepresentation::NativeFloat
        || facts->representation == QoreIRValueRepresentation::NativeBool;
}

static bool qore_ir_is_borrowed_list_element_consumer(const QoreIRInstruction& inst,
        uint32_t value_id) {
    switch (inst.opcode) {
        case QoreIROpcode::HashKeyAccess:
        case QoreIROpcode::HashKeyAccessInt:
        case QoreIROpcode::HashKeyAccessHash:
        case QoreIROpcode::HashKeyAccessHashGuarded:
        case QoreIROpcode::EqHard:
        case QoreIROpcode::NeHard:
        case QoreIROpcode::ToBool:
            return true;
        case QoreIROpcode::ListAppend:
            return inst.operands.size() == 2 && inst.operands[1].id == value_id;
        case QoreIROpcode::ListSetValue:
            return inst.operands.size() == 3 && inst.operands[2].id == value_id;
        case QoreIROpcode::HashSetKeyValue:
            return inst.operands.size() == 3
                && (inst.operands[1].id == value_id || inst.operands[2].id == value_id);
        default:
            return false;
    }
}

struct QoreIRBorrowedLoopSafety {
    bool may_mutate_unknown = false;
    std::unordered_set<uint32_t> mutated_lists;
};

static bool qore_ir_collect_borrowed_loop_safety(const QoreIRNaturalLoop& loop,
        const QoreIRControlFlowGraph& cfg, QoreIRBorrowedLoopSafety& safety,
        size_t& check_count) {
    for (size_t block_id : loop.blocks) {
        if (qore_ir_analysis_cancelled(check_count, "IR borrowed list source analysis")) {
            return false;
        }
        for (const auto& inst : cfg.blocks[block_id]->instructions) {
            if (qore_ir_analysis_cancelled(check_count, "IR borrowed list source analysis")) {
                return false;
            }
            if (qore_ir_may_mutate_unknown_local(inst->opcode)) {
                safety.may_mutate_unknown = true;
            }
            switch (inst->opcode) {
                case QoreIROpcode::ListAppend:
                case QoreIROpcode::ListSetInt:
                case QoreIROpcode::ListSetFloat:
                case QoreIROpcode::ListSetValue:
                    if (!inst->operands.empty()) {
                        safety.mutated_lists.insert(inst->operands[0].id);
                    }
                    break;
                case QoreIROpcode::ListPush:
                    // A separately loaded local can alias the source list.
                    safety.may_mutate_unknown = true;
                    break;
                default:
                    break;
            }
        }
    }
    return true;
}

static size_t qore_ir_mark_borrowed_list_reads(const QoreIRControlFlowGraph& cfg,
        const std::vector<QoreIRNaturalLoop>& loops,
        const QoreIRScalarUses& uses, size_t& check_count) {
    std::unordered_map<uint32_t, size_t> definition_blocks;
    for (size_t block_id = 0; block_id < cfg.blocks.size(); ++block_id) {
        if (qore_ir_analysis_cancelled(check_count, "IR borrowed list definition analysis")) {
            return 0;
        }
        for (const auto& inst : cfg.blocks[block_id]->instructions) {
            if (qore_ir_analysis_cancelled(check_count, "IR borrowed list definition analysis")) {
                return 0;
            }
            if (inst->result.isValid()) {
                definition_blocks[inst->result.id] = block_id;
            }
        }
    }

    size_t changed = 0;
    for (const QoreIRNaturalLoop& loop : loops) {
        if (qore_ir_analysis_cancelled(check_count, "IR borrowed list read analysis")) {
            return changed;
        }
        QoreIRBorrowedLoopSafety safety;
        if (!qore_ir_collect_borrowed_loop_safety(loop, cfg, safety, check_count)) {
            return changed;
        }
        if (safety.may_mutate_unknown) {
            continue;
        }
        std::unordered_set<size_t> loop_blocks(loop.blocks.begin(), loop.blocks.end());
        for (size_t block_id : loop.blocks) {
            if (qore_ir_analysis_cancelled(check_count, "IR borrowed list read analysis")) {
                return changed;
            }
            for (const auto& inst_ptr : cfg.blocks[block_id]->instructions) {
                if (qore_ir_analysis_cancelled(check_count, "IR borrowed list read analysis")) {
                    return changed;
                }
                QoreIRInstruction& inst = *inst_ptr;
                if (inst.opcode != QoreIROpcode::ListGetValue || inst.operands.size() != 2
                        || !inst.result.isValid()) {
                    continue;
                }
                uint32_t source_id = inst.operands[0].id;
                auto source_def = definition_blocks.find(source_id);
                if (source_def != definition_blocks.end() && loop_blocks.count(source_def->second)) {
                    continue;
                }
                auto use_it = uses.find(inst.result.id);
                if (use_it == uses.end() || use_it->second.empty()) {
                    continue;
                }
                bool safe_uses = true;
                for (const QoreIRScalarUse& use : use_it->second) {
                    if (qore_ir_analysis_cancelled(check_count, "IR borrowed list use analysis")) {
                        return changed;
                    }
                    if (!use.inst || !loop_blocks.count(use.block_id)
                            || !qore_ir_is_borrowed_list_element_consumer(*use.inst, inst.result.id)) {
                        safe_uses = false;
                        break;
                    }
                }
                if (!safe_uses || safety.mutated_lists.count(source_id)) {
                    continue;
                }
                inst.opcode = QoreIROpcode::ListGetValueNoRef;
                ++changed;
            }
        }
    }
    return changed;
}

static bool qore_ir_is_assigned_value(const QoreIRFunction& func, QoreIRValue value) {
    const QoreIRValueFacts* facts = func.getValueFacts(value);
    return facts && facts->assigned_state == QoreIRAssignedState::Assigned && facts->never_nothing;
}

static bool qore_ir_may_mutate_list(QoreIROpcode opcode) {
    switch (opcode) {
        case QoreIROpcode::ListAppend:
        case QoreIROpcode::ListSetInt:
        case QoreIROpcode::ListSetFloat:
        case QoreIROpcode::ListSetValue:
        case QoreIROpcode::ListPush:
        case QoreIROpcode::ListIndexStore:
            return true;
        default:
            return false;
    }
}

static size_t qore_ir_specialize_bounded_typed_list_reads(QoreIRFunction& func,
        const QoreIRControlFlowGraph& cfg, const std::vector<QoreIRNaturalLoop>& loops,
        const QoreIRScalarUses& uses, size_t& direct_boxed_reads, size_t& check_count) {
    std::unordered_map<uint32_t, QoreIRInstruction*> definitions;
    for (const auto& block : cfg.blocks) {
        for (const auto& inst : block->instructions) {
            if (qore_ir_analysis_cancelled(check_count, "IR bounded list definition analysis")) {
                return 0;
            }
            if (inst->result.isValid()) {
                definitions[inst->result.id] = inst.get();
            }
        }
    }

    auto get_definition = [&](QoreIRValue value) -> QoreIRInstruction* {
        auto i = definitions.find(value.id);
        return i == definitions.end() ? nullptr : i->second;
    };
    auto get_loaded_local = [&](QoreIRValue value) -> LocalVar* {
        QoreIRInstruction* def = get_definition(value);
        if (!def || def->opcode != QoreIROpcode::LoadLocal || !qore_ir_is_assigned_value(func, value)) {
            return nullptr;
        }
        auto* load = static_cast<QoreIRLocalInstruction*>(def);
        return load->is_ref || load->is_closure ? nullptr : load->local;
    };
    auto get_raw_loaded_local = [&](QoreIRValue value) -> LocalVar* {
        QoreIRInstruction* def = get_definition(value);
        if (!def || def->opcode != QoreIROpcode::LoadLocal) {
            return nullptr;
        }
        auto* load = static_cast<QoreIRLocalInstruction*>(def);
        return load->is_ref || load->is_closure ? nullptr : load->local;
    };
    auto get_const_int = [&](QoreIRValue value, int64_t& result) {
        QoreIRInstruction* def = get_definition(value);
        if (!def || def->opcode != QoreIROpcode::ConstInt) {
            return false;
        }
        result = static_cast<QoreIRConstInstruction*>(def)->constant.int_value;
        return true;
    };

    size_t changed = 0;
    for (const QoreIRNaturalLoop& loop : loops) {
        if (qore_ir_analysis_cancelled(check_count, "IR bounded list loop analysis")) {
            return changed;
        }
        QoreIRBasicBlock* header = cfg.blocks[loop.header];
        if (header->instructions.empty()) {
            continue;
        }
        QoreIRInstruction* terminator = header->instructions.back().get();
        QoreIRBasicBlock* true_target = nullptr;
        QoreIRBasicBlock* false_target = nullptr;
        LocalVar* index_local = nullptr;
        LocalVar* list_local = nullptr;
        LocalVar* bound_local = nullptr;
        QoreIRInstruction* list_size_inst = nullptr;
        QoreIRInstruction* list_size_load_inst = nullptr;
        QoreIRInstruction* reverse_index_store = nullptr;
        bool reverse_loop = false;
        int64_t forward_bound_offset = 0;
        size_t bound_assignment_block = cfg.blocks.size();
        size_t bound_assignment_offset = 0;

        if (terminator->opcode == QoreIROpcode::BrIf) {
            auto* branch = static_cast<QoreIRBranchIfInstruction*>(terminator);
            true_target = branch->true_target;
            false_target = branch->false_target;
            QoreIRInstruction* condition = get_definition(branch->condition);
            if (!condition || condition->operands.size() != 2) {
                continue;
            }

            QoreIRValue index_value;
            QoreIRValue bound_value;
            bool inclusive = false;
            switch (condition->opcode) {
                case QoreIROpcode::LtInt:
                    index_value = condition->operands[0];
                    bound_value = condition->operands[1];
                    break;
                case QoreIROpcode::LeInt:
                    index_value = condition->operands[0];
                    bound_value = condition->operands[1];
                    inclusive = true;
                    break;
                case QoreIROpcode::GtInt:
                    index_value = condition->operands[1];
                    bound_value = condition->operands[0];
                    break;
                case QoreIROpcode::GeInt:
                    index_value = condition->operands[1];
                    bound_value = condition->operands[0];
                    inclusive = true;
                    break;
                default:
                    continue;
            }

            index_local = get_loaded_local(index_value);
            QoreIRValue size_value = bound_value;
            QoreIRInstruction* subtract = get_definition(size_value);
            int64_t bound_offset = 0;
            if (subtract && subtract->opcode == QoreIROpcode::SubInt
                    && subtract->operands.size() == 2
                    && get_const_int(subtract->operands[1], bound_offset)
                    && bound_offset >= 0) {
                size_value = subtract->operands[0];
            } else {
                bound_offset = 0;
            }
            QoreIRInstruction* size = get_definition(size_value);
            if (index_local && size && size->opcode == QoreIROpcode::ListSize
                    && size->operands.size() == 1
                    && (!inclusive || bound_offset > 0)) {
                forward_bound_offset = inclusive ? bound_offset - 1 : bound_offset;
                list_local = get_raw_loaded_local(size->operands[0]);
                list_size_inst = size;
                list_size_load_inst = get_definition(size->operands[0]);
            } else {
                // Reverse loops use `i >= 0` (or equivalently `0 <= i`) and
                // establish the upper bound with `i = list.size() - 1`.
                QoreIRValue reverse_index;
                QoreIRValue zero_value;
                if (condition->opcode == QoreIROpcode::GeInt) {
                    reverse_index = condition->operands[0];
                    zero_value = condition->operands[1];
                } else if (condition->opcode == QoreIROpcode::LeInt) {
                    reverse_index = condition->operands[1];
                    zero_value = condition->operands[0];
                } else {
                    continue;
                }
                int64_t zero = -1;
                index_local = get_loaded_local(reverse_index);
                if (!index_local || !get_const_int(zero_value, zero) || zero != 0) {
                    continue;
                }
                reverse_loop = true;
                for (const auto& inst_ptr : cfg.blocks[loop.preheader]->instructions) {
                    if (qore_ir_analysis_cancelled(
                            check_count, "IR reverse bounded list initialization analysis")) {
                        return changed;
                    }
                    QoreIRInstruction* inst = inst_ptr.get();
                    if (inst->opcode != QoreIROpcode::StoreLocal
                            || static_cast<QoreIRLocalInstruction*>(inst)->local
                                != index_local
                            || inst->operands.size() != 1) {
                        continue;
                    }
                    QoreIRInstruction* init = get_definition(inst->operands[0]);
                    int64_t adjustment = 0;
                    if (!init || init->opcode != QoreIROpcode::SubInt
                            || init->operands.size() != 2
                            || !get_const_int(init->operands[1], adjustment)
                            || adjustment != 1) {
                        continue;
                    }
                    QoreIRInstruction* reverse_size =
                        get_definition(init->operands[0]);
                    if (!reverse_size
                            || reverse_size->opcode != QoreIROpcode::ListSize
                            || reverse_size->operands.size() != 1) {
                        continue;
                    }
                    list_local =
                        get_raw_loaded_local(reverse_size->operands[0]);
                    list_size_inst = reverse_size;
                    list_size_load_inst =
                        get_definition(reverse_size->operands[0]);
                    reverse_index_store = inst;
                }
            }
            if (!index_local || !list_local) {
                continue;
            }
        } else if (terminator->opcode == QoreIROpcode::BranchIfLtLocalInt) {
            auto* branch = static_cast<QoreIRBranchIfLtLocalIntInstruction*>(terminator);
            true_target = branch->true_target;
            false_target = branch->false_target;
            index_local = branch->lhs;
            bound_local = branch->rhs;
            if (!index_local || !bound_local || index_local == bound_local
                    || index_local->closureUse() || bound_local->closureUse()
                    || QoreTypeInfo::isReference(index_local->getTypeInfo())
                    || QoreTypeInfo::isReference(bound_local->getTypeInfo())) {
                continue;
            }
            size_t bound_assignments = 0;
            for (size_t block_id = 0; block_id < cfg.blocks.size(); ++block_id) {
                for (size_t offset = 0; offset < cfg.blocks[block_id]->instructions.size(); ++offset) {
                    if (qore_ir_analysis_cancelled(check_count,
                            "IR cached bounded list assignment analysis")) {
                        return changed;
                    }
                    QoreIRInstruction* inst = cfg.blocks[block_id]->instructions[offset].get();
                    if (inst->opcode != QoreIROpcode::StoreLocal
                            || static_cast<QoreIRLocalInstruction*>(inst)->local != bound_local) {
                        continue;
                    }
                    ++bound_assignments;
                    if (inst->operands.size() != 1) {
                        continue;
                    }
                    QoreIRInstruction* size = get_definition(inst->operands[0]);
                    if (!size || size->opcode != QoreIROpcode::ListSize
                            || size->operands.size() != 1) {
                        continue;
                    }
                    list_local = get_raw_loaded_local(size->operands[0]);
                    bound_assignment_block = block_id;
                    bound_assignment_offset = offset;
                }
            }
            if (bound_assignments != 1 || !list_local
                    || !cfg.dominates(bound_assignment_block, loop.header)) {
                continue;
            }
        } else {
            continue;
        }

        auto true_id = cfg.block_ids.find(true_target);
        auto false_id = cfg.block_ids.find(false_target);
        if (true_id == cfg.block_ids.end() || false_id == cfg.block_ids.end()) {
            continue;
        }
        std::unordered_set<size_t> loop_blocks(loop.blocks.begin(), loop.blocks.end());
        if (!loop_blocks.count(true_id->second) || loop_blocks.count(false_id->second)) {
            continue;
        }

        const QoreTypeInfo* element_type = list_local
            ? QoreTypeInfo::getUniqueReturnComplexList(list_local->getTypeInfo()) : nullptr;
        QoreIROpcode specialized_opcode;
        QoreIRValueRepresentation representation;
        if (element_type == bigIntTypeInfo) {
            specialized_opcode = QoreIROpcode::ListGetInt;
            representation = QoreIRValueRepresentation::NativeInt;
        } else if (element_type == floatTypeInfo) {
            specialized_opcode = QoreIROpcode::ListGetFloat;
            representation = QoreIRValueRepresentation::NativeFloat;
        } else if (element_type == boolTypeInfo
                || QoreTypeInfo::parseReturns(element_type, NT_STRING) == QTI_IDENT) {
            specialized_opcode = QoreIROpcode::ListGetValue;
            representation = QoreIRValueRepresentation::Boxed;
        } else {
            continue;
        }
        if (!list_local || list_local == index_local || list_local == bound_local) {
            continue;
        }

        size_t assignment_block = cfg.blocks.size();
        size_t assignment_offset = 0;
        size_t assignments = 0;
        bool assigned_non_nothing = false;
        int64_t exact_list_size = -1;
        bool list_parameter = false;
        size_t parameter_count = 0;
        for (const auto& [index, parameter] : func.param_local_vars) {
            if (parameter_count++ && !(parameter_count % 100)
                    && qore_check_cancel(nullptr,
                        "IR bounded list parameter analysis")) {
                return changed;
            }
            if (index >= 0 && parameter == list_local) {
                list_parameter = QoreTypeInfo::parseReturns(
                    list_local->getTypeInfo(), NT_NOTHING) == QTI_NOT_EQUAL;
                break;
            }
        }
        for (size_t block_id = 0; block_id < cfg.blocks.size(); ++block_id) {
            for (size_t offset = 0; offset < cfg.blocks[block_id]->instructions.size(); ++offset) {
                if (qore_ir_analysis_cancelled(
                        check_count, "IR bounded list assignment analysis")) {
                    return changed;
                }
                QoreIRInstruction* inst = cfg.blocks[block_id]->instructions[offset].get();
                if (inst->opcode != QoreIROpcode::StoreLocal
                        || static_cast<QoreIRLocalInstruction*>(inst)->local != list_local) {
                    continue;
                }
                ++assignments;
                assignment_block = block_id;
                assignment_offset = offset;
                if (inst->operands.size() == 1) {
                    QoreIRInstruction* value_def = get_definition(inst->operands[0]);
                    assigned_non_nothing = qore_ir_is_assigned_value(func, inst->operands[0])
                        || (value_def && (value_def->opcode == QoreIROpcode::MakeList
                            || value_def->opcode == QoreIROpcode::CreateEmptyList
                            || value_def->opcode == QoreIROpcode::CreateSizedList));
                    if (value_def
                            && value_def->opcode == QoreIROpcode::MakeList
                            && value_def->operands.size()
                                <= static_cast<size_t>(
                                    std::numeric_limits<int64_t>::max())) {
                        exact_list_size = static_cast<int64_t>(
                            value_def->operands.size());
                    } else if (value_def
                            && value_def->opcode
                                == QoreIROpcode::CreateEmptyList) {
                        exact_list_size = 0;
                    }
                }
            }
        }
        bool stable_initial_list = assignments == 1 && assigned_non_nothing
            && cfg.dominates(assignment_block, loop.header);
        if (!stable_initial_list && !(assignments == 0 && list_parameter)) {
            continue;
        }
        bool assignment_invalidated = false;
        for (size_t block_id = 0; block_id < cfg.blocks.size(); ++block_id) {
            for (size_t offset = 0; offset < cfg.blocks[block_id]->instructions.size(); ++offset) {
                if (qore_ir_analysis_cancelled(
                        check_count, "IR bounded list invalidation analysis")) {
                    return changed;
                }
                QoreIRInstruction* inst = cfg.blocks[block_id]->instructions[offset].get();
                bool after_assignment = list_parameter
                    || (block_id == assignment_block
                        ? offset > assignment_offset : cfg.dominates(assignment_block, block_id));
                if (!after_assignment || !cfg.dominates(block_id, loop.header)) {
                    continue;
                }
                if (qore_ir_may_mutate_unknown_local(inst->opcode)) {
                    assignment_invalidated = true;
                    break;
                }
                if ((inst->opcode == QoreIROpcode::InstantiateLocal
                        || inst->opcode == QoreIROpcode::UninstantiateLocal)
                        && static_cast<QoreIRLocalInstruction*>(inst)->local == list_local) {
                    assignment_invalidated = true;
                    break;
                }
            }
            if (assignment_invalidated) {
                break;
            }
        }
        if (assignment_invalidated) {
            continue;
        }

        if (bound_local) {
            bool bound_invalidated = false;
            for (size_t block_id = 0; block_id < cfg.blocks.size(); ++block_id) {
                for (size_t offset = 0; offset < cfg.blocks[block_id]->instructions.size(); ++offset) {
                    if (qore_ir_analysis_cancelled(check_count,
                            "IR cached bounded list invalidation analysis")) {
                        return changed;
                    }
                    QoreIRInstruction* inst = cfg.blocks[block_id]->instructions[offset].get();
                    bool after_bound_assignment = block_id == bound_assignment_block
                        ? offset > bound_assignment_offset
                        : cfg.dominates(bound_assignment_block, block_id);
                    if (!after_bound_assignment || !cfg.dominates(block_id, loop.header)) {
                        continue;
                    }
                    if (qore_ir_may_mutate_unknown_local(inst->opcode)
                            || qore_ir_may_mutate_list(inst->opcode)
                            || (inst->opcode == QoreIROpcode::IncrementLocalInt
                                && static_cast<QoreIRIncrementLocalIntInstruction*>(inst)->local
                                    == bound_local)
                            || (inst->opcode == QoreIROpcode::AddAssignLocalInt
                                && static_cast<QoreIRAddAssignLocalIntInstruction*>(inst)->target
                                    == bound_local)
                            || ((inst->opcode == QoreIROpcode::StoreLocal
                                    || inst->opcode == QoreIROpcode::StoreClosure
                                    || inst->opcode == QoreIROpcode::InstantiateLocal
                                    || inst->opcode == QoreIROpcode::UninstantiateLocal)
                                && static_cast<QoreIRLocalInstruction*>(inst)->local == bound_local)) {
                        bound_invalidated = true;
                        break;
                    }
                }
                if (bound_invalidated) {
                    break;
                }
            }
            if (bound_invalidated) {
                continue;
            }
        }

        bool valid_initial_value = reverse_loop && reverse_index_store;
        int64_t initial_index = 0;
        for (const auto& inst_ptr : cfg.blocks[loop.preheader]->instructions) {
            if (qore_ir_analysis_cancelled(
                    check_count, "IR bounded list initial-value analysis")) {
                return changed;
            }
            QoreIRInstruction* inst = inst_ptr.get();
            if (inst->opcode == QoreIROpcode::StoreLocal) {
                auto* store = static_cast<QoreIRLocalInstruction*>(inst);
                if (store->local == index_local) {
                    if (reverse_loop) {
                        valid_initial_value = inst == reverse_index_store;
                    } else {
                        initial_index = -1;
                        valid_initial_value = inst->operands.size() == 1
                            && get_const_int(inst->operands[0], initial_index)
                            && initial_index >= 0;
                    }
                }
            } else if (inst->opcode == QoreIROpcode::IncrementLocalInt
                    && static_cast<QoreIRIncrementLocalIntInstruction*>(inst)->local == index_local) {
                valid_initial_value = false;
            } else if (inst->opcode == QoreIROpcode::AddAssignLocalInt
                    && static_cast<QoreIRAddAssignLocalIntInstruction*>(inst)->target == index_local) {
                valid_initial_value = false;
            }
        }
        if (!valid_initial_value) {
            continue;
        }

        size_t increments = 0;
        bool invalidated = false;
        auto is_unrelated_local_lvalue_path = [&](const QoreIRInstruction* inst) {
            switch (inst->opcode) {
                case QoreIROpcode::LValuePathAssign:
                case QoreIROpcode::LValuePathCompound:
                case QoreIROpcode::LValuePathUnary:
                case QoreIROpcode::LValuePathBinaryMut:
                case QoreIROpcode::LValuePathTernary:
                    break;
                default:
                    return false;
            }
            const auto* path = static_cast<const QoreIRLValuePathInstruction*>(inst);
            if (path->path.empty() || path->path.front().kind != LVPathStepKind::LocalVar) {
                return false;
            }
            auto* local = reinterpret_cast<const LocalVar*>(path->path.front().ref_ptr);
            return local && local != index_local && local != list_local
                && !local->closureUse() && !QoreTypeInfo::isReference(local->getTypeInfo());
        };
        size_t loop_specializations = 0;
        size_t induction_block = cfg.blocks.size();
        size_t induction_offset = 0;
        for (size_t block_id : loop.blocks) {
            const auto& instructions = cfg.blocks[block_id]->instructions;
            for (size_t offset = 0; offset < instructions.size(); ++offset) {
                if (qore_ir_analysis_cancelled(check_count, "IR bounded list mutation analysis")) {
                    return changed;
                }
                QoreIRInstruction* inst = instructions[offset].get();
                if ((qore_ir_may_mutate_unknown_local(inst->opcode)
                        && !is_unrelated_local_lvalue_path(inst))
                        || qore_ir_may_mutate_list(inst->opcode)) {
                    invalidated = true;
                    break;
                }
                if (inst->opcode == QoreIROpcode::IncrementLocalInt) {
                    auto* increment = static_cast<QoreIRIncrementLocalIntInstruction*>(inst);
                    if (increment->local == index_local) {
                        bool valid_delta = reverse_loop
                            ? increment->delta < 0
                            : increment->delta == 1;
                        if (!reverse_loop && increment->delta > 1
                                && exact_list_size >= 0) {
                            __int128 largest_index =
                                exact_list_size
                                ? static_cast<__int128>(exact_list_size) - 1
                                : 0;
                            valid_delta = largest_index
                                    + static_cast<__int128>(
                                        increment->delta)
                                <= std::numeric_limits<int64_t>::max();
                        }
                        if (!valid_delta) {
                            invalidated = true;
                            break;
                        }
                        ++increments;
                        induction_block = block_id;
                        induction_offset = offset;
                    }
                } else if (inst->opcode == QoreIROpcode::AddAssignLocalInt) {
                    auto* add = static_cast<QoreIRAddAssignLocalIntInstruction*>(inst);
                    if (add->target == index_local || add->target == list_local) {
                        invalidated = true;
                        break;
                    }
                } else if (inst->opcode == QoreIROpcode::StoreLocal
                        || inst->opcode == QoreIROpcode::StoreClosure
                        || inst->opcode == QoreIROpcode::InstantiateLocal
                        || inst->opcode == QoreIROpcode::UninstantiateLocal) {
                    auto* local_inst = static_cast<QoreIRLocalInstruction*>(inst);
                    if (local_inst->local == index_local || local_inst->local == list_local
                            || local_inst->local == bound_local) {
                        invalidated = true;
                        break;
                    }
                }
            }
            if (invalidated) {
                break;
            }
        }
        if (invalidated || increments != 1
                || cfg.successors[induction_block].size() != 1
                || cfg.successors[induction_block].front() != loop.header) {
            continue;
        }

        for (size_t block_id : loop.blocks) {
            if (!cfg.dominates(true_id->second, block_id)) {
                continue;
            }
            const auto& instructions = cfg.blocks[block_id]->instructions;
            for (size_t offset = 0; offset < instructions.size(); ++offset) {
                if (qore_ir_analysis_cancelled(
                        check_count, "IR bounded list candidate analysis")) {
                    return changed;
                }
                QoreIRInstruction& inst = *instructions[offset];
                if (inst.opcode != QoreIROpcode::ListIndexDynamic || inst.operands.size() != 2) {
                    continue;
                }
                if (block_id == induction_block && offset > induction_offset) {
                    continue;
                }
                auto* expr_inst = static_cast<QoreIRExprInstruction*>(&inst);
                LocalVar* read_index_local = get_loaded_local(inst.operands[1]);
                int64_t read_index_offset = 0;
                if (!read_index_local && !reverse_loop) {
                    QoreIRInstruction* index_expr = get_definition(inst.operands[1]);
                    int64_t adjustment = 0;
                    if (index_expr && index_expr->operands.size() == 2
                            && index_expr->opcode == QoreIROpcode::AddInt) {
                        LocalVar* lhs_local =
                            get_loaded_local(index_expr->operands[0]);
                        LocalVar* rhs_local =
                            get_loaded_local(index_expr->operands[1]);
                        if (lhs_local
                                && get_const_int(index_expr->operands[1],
                                    adjustment)) {
                            read_index_local = lhs_local;
                            read_index_offset = adjustment;
                        } else if (rhs_local
                                && get_const_int(index_expr->operands[0],
                                    adjustment)) {
                            read_index_local = rhs_local;
                            read_index_offset = adjustment;
                        }
                    } else if (index_expr && index_expr->operands.size() == 2
                            && index_expr->opcode == QoreIROpcode::SubInt
                            && get_const_int(index_expr->operands[1],
                                adjustment)
                            && adjustment != std::numeric_limits<int64_t>::min()) {
                        read_index_local =
                            get_loaded_local(index_expr->operands[0]);
                        read_index_offset = -adjustment;
                    }
                }
                bool proven_index = read_index_local == index_local;
                if (proven_index && reverse_loop) {
                    proven_index = read_index_offset == 0;
                } else if (proven_index) {
                    __int128 initial_read =
                        static_cast<__int128>(initial_index)
                        + static_cast<__int128>(read_index_offset);
                    proven_index = initial_read >= 0
                        && read_index_offset <= forward_bound_offset;
                }
                if (!expr_inst->list_selector_kinds.empty()
                        || get_raw_loaded_local(inst.operands[0]) != list_local
                        || !proven_index) {
                    continue;
                }
                QoreIROpcode candidate_opcode = specialized_opcode;
                if (representation == QoreIRValueRepresentation::Boxed
                        && !getenv("QORE_DISABLE_IR_BOUNDED_BOXED_DIRECT_READS")) {
                    auto use_it = uses.find(inst.result.id);
                    bool borrowed_uses = use_it != uses.end() && !use_it->second.empty();
                    if (borrowed_uses) {
                        for (const QoreIRScalarUse& use : use_it->second) {
                            if (qore_ir_analysis_cancelled(
                                    check_count, "IR bounded boxed list use analysis")) {
                                return changed;
                            }
                            if (!use.inst || !loop_blocks.count(use.block_id)
                                    || !qore_ir_is_borrowed_list_element_consumer(
                                        *use.inst, inst.result.id)) {
                                borrowed_uses = false;
                                break;
                            }
                        }
                    }
                    if (borrowed_uses) {
                        candidate_opcode = QoreIROpcode::ListGetValueNoRefUnchecked;
                        ++direct_boxed_reads;
                    }
                }
                inst.opcode = candidate_opcode;
                QoreIRValueFacts result_facts;
                result_facts.type_info = element_type;
                result_facts.representation = representation;
                // Exact typed lists can contain unassigned sparse slots.  Native
                // numeric reads preserve their established conversion semantics;
                // boxed bool/string reads must continue to expose NOTHING.
                bool native_numeric = representation == QoreIRValueRepresentation::NativeInt
                    || representation == QoreIRValueRepresentation::NativeFloat;
                result_facts.assigned_state = native_numeric
                    ? QoreIRAssignedState::Assigned : QoreIRAssignedState::MaybeAssigned;
                result_facts.never_nothing = native_numeric;
                func.setValueFacts(inst.result, result_facts);
                ++changed;
                ++loop_specializations;
            }
        }
        if (loop_specializations && list_size_inst && list_size_load_inst
                && !getenv("QORE_DISABLE_IR_BOUNDED_LIST_SIZE_HOIST")) {
            auto move_to_preheader = [&](QoreIRInstruction* target) {
                std::unique_ptr<QoreIRInstruction> moved;
                for (size_t block_id : loop.blocks) {
                    if (qore_ir_analysis_cancelled(check_count,
                            "IR bounded list size hoist")) {
                        return false;
                    }
                    auto& instructions = cfg.blocks[block_id]->instructions;
                    for (auto found = instructions.begin(); found != instructions.end(); ++found) {
                        if (qore_ir_analysis_cancelled(check_count,
                                "IR bounded list size hoist")) {
                            return false;
                        }
                        if (found->get() == target) {
                            moved = std::move(*found);
                            instructions.erase(found);
                            break;
                        }
                    }
                    if (moved) {
                        break;
                    }
                }
                if (!moved) {
                    return true;
                }
                auto& preheader = cfg.blocks[loop.preheader]->instructions;
                preheader.insert(preheader.end() - 1, std::move(moved));
                return true;
            };
            if (!move_to_preheader(list_size_load_inst)
                    || !move_to_preheader(list_size_inst)) {
                return changed;
            }
        }
    }
    return changed;
}

static size_t qore_ir_mark_in_place_list_pushes(QoreIRFunction& func,
        const QoreIRControlFlowGraph& cfg, const QoreIRScalarUses& uses,
        size_t& check_count,
        const QoreIRParamNoEscapeQuery* param_noescape = nullptr) {
    std::unordered_map<uint32_t, QoreIRInstruction*> definitions;
    std::unordered_map<uint32_t, const AbstractQoreFunctionVariant*> closure_values;
    std::unordered_set<LocalVar*> candidate_locals;
    for (const auto& block : cfg.blocks) {
        if (qore_ir_analysis_cancelled(check_count, "IR in-place list push definition analysis")) {
            return 0;
        }
        for (const auto& inst : block->instructions) {
            if (qore_ir_analysis_cancelled(check_count, "IR in-place list push definition analysis")) {
                return 0;
            }
            if (inst->result.isValid()) {
                definitions[inst->result.id] = inst.get();
                const AbstractQoreFunctionVariant* closure =
                    qore_ir_get_created_closure_variant(inst.get());
                if (closure) {
                    closure_values.emplace(inst->result.id, closure);
                }
            }
            if (inst->opcode == QoreIROpcode::LoadLocal
                    || inst->opcode == QoreIROpcode::StoreLocal) {
                auto* local_inst = static_cast<QoreIRLocalInstruction*>(inst.get());
                if (local_inst->local && !local_inst->is_ref && !local_inst->is_closure
                        && func.ir_only_locals.count(
                            reinterpret_cast<const void*>(local_inst->local))) {
                    candidate_locals.insert(local_inst->local);
                }
            }
        }
    }

    auto get_loaded_local = [&](QoreIRValue value) -> LocalVar* {
        auto def_it = definitions.find(value.id);
        if (def_it == definitions.end() || def_it->second->opcode != QoreIROpcode::LoadLocal) {
            return nullptr;
        }
        auto* load = static_cast<QoreIRLocalInstruction*>(def_it->second);
        return load->is_ref || load->is_closure ? nullptr : load->local;
    };
    auto get_paired_push_store = [&](const QoreIRInstruction& push,
            LocalVar* local) -> const QoreIRLocalInstruction* {
        if (push.opcode != QoreIROpcode::ListPush || push.operands.size() != 2
                || !push.result.isValid() || get_loaded_local(push.operands[0]) != local) {
            return nullptr;
        }
        auto use_it = uses.find(push.result.id);
        if (use_it == uses.end() || use_it->second.size() != 1 || !use_it->second[0].inst
                || use_it->second[0].inst->opcode != QoreIROpcode::StoreLocal) {
            return nullptr;
        }
        auto* store = static_cast<const QoreIRLocalInstruction*>(use_it->second[0].inst);
        return store->local == local && !store->weak && store->operands.size() == 1
                && store->operands[0].id == push.result.id ? store : nullptr;
    };
    auto is_exclusive_fresh_store = [&](const QoreIRLocalInstruction& store) {
        if (store.weak || store.operands.size() != 1) {
            return false;
        }
        auto def_it = definitions.find(store.operands[0].id);
        if (def_it == definitions.end()
                || (def_it->second->opcode != QoreIROpcode::MakeList
                    && def_it->second->opcode != QoreIROpcode::CreateEmptyList
                    && def_it->second->opcode != QoreIROpcode::CreateSizedList)) {
            return false;
        }
        auto use_it = uses.find(def_it->second->result.id);
        return use_it != uses.end() && use_it->second.size() == 1
            && use_it->second[0].inst == &store;
    };
    auto is_read_only_list_use = [&](const QoreIRInstruction& inst, QoreIRValue value) {
        if (qore_ir_is_read_only_list_use(inst, value)) {
            return true;
        }
        if (!param_noescape) {
            return false;
        }
        size_t arg_offset = 0;
        bool supported_call = inst.opcode == QoreIROpcode::CallDirect
            || inst.opcode == QoreIROpcode::CallStaticDirect;
        if (inst.opcode == QoreIROpcode::CallMethodDirect
                || inst.opcode == QoreIROpcode::InvokeMethodDirect) {
            supported_call = qore_ir_is_non_overridable_method_call(inst);
        } else if (inst.opcode == QoreIROpcode::CallClosureDirect
                || (inst.opcode == QoreIROpcode::Invoke
                    && static_cast<const QoreIRInvokeInstruction&>(inst).invoke_opcode
                        == QoreIROpcode::CallClosureDirect)) {
            supported_call = true;
            arg_offset = 1;
        }
        if (!supported_call) {
            return false;
        }
        bool has_ref_args = true;
        const AbstractQoreFunctionVariant* callee =
            qore_ir_get_resolved_effect_callee(&inst, has_ref_args,
                &closure_values);
        if (!callee || has_ref_args) {
            return false;
        }
        qore_type_t return_type = QoreTypeInfo::getSingleType(callee->getReturnTypeInfo());
        if (return_type != NT_INT && return_type != NT_FLOAT && return_type != NT_BOOLEAN) {
            return false;
        }
        bool found = false;
        for (size_t arg = arg_offset; arg < inst.operands.size(); ++arg) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR noescape call argument analysis")) {
                return false;
            }
            if (inst.operands[arg].id != value.id) {
                continue;
            }
            if (!(*param_noescape)(callee, arg - arg_offset)) {
                return false;
            }
            found = true;
        }
        return found;
    };

    using FreshLocalSet = std::unordered_set<LocalVar*>;
    bool cancelled = false;
    auto transfer_block = [&](size_t block_id, FreshLocalSet state, bool mark,
            size_t& changed) -> FreshLocalSet {
        for (const auto& inst_ptr : cfg.blocks[block_id]->instructions) {
            if (qore_ir_analysis_cancelled(check_count, "IR in-place list push escape analysis")) {
                cancelled = true;
                return state;
            }
            QoreIRInstruction& inst = *inst_ptr;
            if (!qore_ir_visit_value_operands(inst, [&](QoreIRValue operand) {
                LocalVar* local = get_loaded_local(operand);
                if (!local || !candidate_locals.count(local)) {
                    return;
                }
                bool preserves_freshness = is_read_only_list_use(inst, operand)
                    || (inst.opcode == QoreIROpcode::ListPush
                        && !inst.operands.empty() && inst.operands[0].id == operand.id
                        && get_paired_push_store(inst, local));
                if (!preserves_freshness) {
                    state.erase(local);
                }
            }, &check_count, "IR in-place list push escape analysis")) {
                cancelled = true;
                return state;
            }
            if (inst.opcode == QoreIROpcode::ListPush && inst.operands.size() == 2) {
                LocalVar* local = get_loaded_local(inst.operands[0]);
                const QoreIRLocalInstruction* store = local
                    ? get_paired_push_store(inst, local) : nullptr;
                const QoreIRValueFacts* facts = func.getValueFacts(inst.operands[0]);
                if (mark && !inst.list_push_in_place && store && state.count(local) && facts
                        && facts->assigned_state == QoreIRAssignedState::Assigned
                        && facts->never_nothing && QoreTypeInfo::isListType(facts->type_info)) {
                    inst.list_push_in_place = true;
                    const_cast<QoreIRLocalInstruction*>(store)->redundant_store = true;
                    ++changed;
                }
            }
            if (inst.opcode == QoreIROpcode::StoreLocal) {
                auto& store = static_cast<QoreIRLocalInstruction&>(inst);
                if (store.local && candidate_locals.count(store.local)) {
                    if (is_exclusive_fresh_store(store)) {
                        state.insert(store.local);
                    } else {
                        auto def_it = store.operands.empty()
                            ? definitions.end() : definitions.find(store.operands[0].id);
                        if (def_it == definitions.end()
                                || !get_paired_push_store(*def_it->second, store.local)) {
                            state.erase(store.local);
                        }
                    }
                }
            }
        }
        return state;
    };

    std::vector<FreshLocalSet> fresh_in(cfg.blocks.size(), candidate_locals);
    std::vector<FreshLocalSet> fresh_out(cfg.blocks.size(), candidate_locals);
    std::vector<size_t> worklist;
    std::vector<uint8_t> queued(cfg.blocks.size(), 0);
    for (size_t block_id = 0; block_id < cfg.blocks.size(); ++block_id) {
        if (qore_ir_analysis_cancelled(check_count, "IR in-place list push dataflow")) {
            return 0;
        }
        if (cfg.reachable[block_id]) {
            worklist.push_back(block_id);
            queued[block_id] = 1;
        }
    }
    size_t ignored_changed = 0;
    while (!worklist.empty()) {
        if (qore_ir_analysis_cancelled(check_count, "IR in-place list push dataflow")) {
            return 0;
        }
        size_t block_id = worklist.back();
        worklist.pop_back();
        queued[block_id] = 0;
        FreshLocalSet input;
        if (block_id) {
            bool first = true;
            for (size_t predecessor : cfg.predecessors[block_id]) {
                if (qore_ir_analysis_cancelled(check_count, "IR in-place list push dataflow")) {
                    return 0;
                }
                if (!cfg.reachable[predecessor]) {
                    continue;
                }
                if (first) {
                    input = fresh_out[predecessor];
                    first = false;
                    continue;
                }
                for (auto it = input.begin(); it != input.end();) {
                    if (qore_ir_analysis_cancelled(check_count, "IR in-place list push dataflow")) {
                        return 0;
                    }
                    if (!fresh_out[predecessor].count(*it)) {
                        it = input.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
        fresh_in[block_id] = input;
        FreshLocalSet output = transfer_block(block_id, input, false, ignored_changed);
        if (cancelled) {
            return 0;
        }
        if (output == fresh_out[block_id]) {
            continue;
        }
        fresh_out[block_id] = std::move(output);
        for (size_t successor : cfg.successors[block_id]) {
            if (qore_ir_analysis_cancelled(check_count, "IR in-place list push dataflow")) {
                return 0;
            }
            if (!queued[successor]) {
                queued[successor] = 1;
                worklist.push_back(successor);
            }
        }
    }

    size_t changed = 0;
    for (size_t block_id = 0; block_id < cfg.blocks.size(); ++block_id) {
        if (qore_ir_analysis_cancelled(check_count, "IR in-place list push escape analysis")) {
            return changed;
        }
        if (cfg.reachable[block_id]) {
            transfer_block(block_id, fresh_in[block_id], true, changed);
            if (cancelled) {
                return changed;
            }
        }
    }
    return changed;
}

static size_t qore_ir_mark_in_place_string_appends(QoreIRFunction& func,
        const QoreIRControlFlowGraph& cfg, const QoreIRScalarUses& uses,
        size_t& check_count) {
    // test/debug knob: leave paired loads owned so the interpreter exercises
    // its runtime-uniqueness CoW fallback instead of the borrowed fast path
    bool enable_borrowed_load =
        !getenv("QORE_DISABLE_IR_BORROWED_STRING_LOAD");
    std::unordered_map<uint32_t, QoreIRInstruction*> definitions;
    std::unordered_set<LocalVar*> candidate_locals;
    for (const auto& block : cfg.blocks) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR in-place string append definition analysis")) {
            return 0;
        }
        for (const auto& inst : block->instructions) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR in-place string append definition analysis")) {
                return 0;
            }
            if (inst->result.isValid()) {
                definitions[inst->result.id] = inst.get();
            }
            if (inst->opcode == QoreIROpcode::LoadLocal
                    || inst->opcode == QoreIROpcode::StoreLocal) {
                auto* local_inst = static_cast<QoreIRLocalInstruction*>(inst.get());
                if (local_inst->local && !local_inst->is_ref && !local_inst->is_closure
                        && local_inst->local->getTypeInfo() == stringTypeInfo
                        && func.ir_only_locals.count(
                            reinterpret_cast<const void*>(local_inst->local))) {
                    candidate_locals.insert(local_inst->local);
                }
            }
        }
    }

    auto get_loaded_local = [&](QoreIRValue value) -> LocalVar* {
        auto def_it = definitions.find(value.id);
        if (def_it == definitions.end()
                || def_it->second->opcode != QoreIROpcode::LoadLocal) {
            return nullptr;
        }
        auto* load = static_cast<QoreIRLocalInstruction*>(def_it->second);
        return load->is_ref || load->is_closure ? nullptr : load->local;
    };
    auto get_paired_append_store = [&](const QoreIRInstruction& append,
            LocalVar* local) -> const QoreIRLocalInstruction* {
        if (append.opcode != QoreIROpcode::AppendStringCow
                || append.operands.size() != 2 || !append.result.isValid()
                || get_loaded_local(append.operands[0]) != local) {
            return nullptr;
        }
        auto use_it = uses.find(append.result.id);
        if (use_it == uses.end() || use_it->second.size() != 1
                || !use_it->second[0].inst
                || use_it->second[0].inst->opcode != QoreIROpcode::StoreLocal) {
            return nullptr;
        }
        auto* store =
            static_cast<const QoreIRLocalInstruction*>(use_it->second[0].inst);
        return store->local == local && !store->weak
                && store->operands.size() == 1
                && store->operands[0].id == append.result.id
            ? store : nullptr;
    };
    auto is_exclusive_fresh_store = [&](const QoreIRLocalInstruction& store) {
        if (store.weak || store.operands.size() != 1) {
            return false;
        }
        auto def_it = definitions.find(store.operands[0].id);
        if (def_it == definitions.end()
                || (def_it->second->opcode != QoreIROpcode::ConstString
                    && def_it->second->opcode != QoreIROpcode::StringConcat)) {
            return false;
        }
        auto use_it = uses.find(def_it->second->result.id);
        return use_it != uses.end() && use_it->second.size() == 1
            && use_it->second[0].inst == &store;
    };

    using FreshLocalSet = std::unordered_set<LocalVar*>;
    bool cancelled = false;
    auto transfer_block = [&](size_t block_id, FreshLocalSet state, bool mark,
            size_t& changed) -> FreshLocalSet {
        for (const auto& inst_ptr : cfg.blocks[block_id]->instructions) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR in-place string append escape analysis")) {
                cancelled = true;
                return state;
            }
            QoreIRInstruction& inst = *inst_ptr;
            if (!qore_ir_visit_value_operands(inst, [&](QoreIRValue operand) {
                LocalVar* local = get_loaded_local(operand);
                if (!local || !candidate_locals.count(local)) {
                    return;
                }
                bool preserves_freshness =
                    qore_ir_is_read_only_string_use(inst, operand)
                    || (inst.opcode == QoreIROpcode::AppendStringCow
                        && !inst.operands.empty()
                        && inst.operands[0].id == operand.id
                        && get_paired_append_store(inst, local));
                if (!preserves_freshness) {
                    state.erase(local);
                }
            }, &check_count, "IR in-place string append escape analysis")) {
                cancelled = true;
                return state;
            }
            if (inst.opcode == QoreIROpcode::AppendStringCow
                    && inst.operands.size() == 2) {
                LocalVar* local = get_loaded_local(inst.operands[0]);
                const QoreIRLocalInstruction* store = local
                    ? get_paired_append_store(inst, local) : nullptr;
                const QoreIRValueFacts* facts =
                    func.getValueFacts(inst.operands[0]);
                if (mark && !inst.string_append_in_place && store
                        && state.count(local) && facts
                        && facts->assigned_state == QoreIRAssignedState::Assigned
                        && facts->never_nothing
                        && QoreTypeInfo::parseReturns(
                            facts->type_info, NT_STRING) == QTI_IDENT) {
                    inst.string_append_in_place = true;
                    const_cast<QoreIRLocalInstruction*>(store)->redundant_store =
                        true;
                    // when the append is the load's only consumer, the
                    // interpreter can borrow the local's slot-cache reference
                    // instead of taking its own, keeping the builder node
                    // unique at the mutation site; with other consumers the
                    // load stays owned and the interpreter's runtime
                    // uniqueness check falls back to the CoW append
                    auto load_def = definitions.find(inst.operands[0].id);
                    auto load_uses = uses.find(inst.operands[0].id);
                    if (enable_borrowed_load
                            && load_def != definitions.end()
                            && load_uses != uses.end()
                            && load_uses->second.size() == 1
                            && load_uses->second[0].inst == &inst) {
                        load_def->second->borrowed_local_load = true;
                    }
                    ++changed;
                }
            }
            if (inst.opcode == QoreIROpcode::StoreLocal) {
                auto& store = static_cast<QoreIRLocalInstruction&>(inst);
                if (store.local && candidate_locals.count(store.local)) {
                    if (is_exclusive_fresh_store(store)) {
                        state.insert(store.local);
                    } else {
                        auto def_it = store.operands.empty()
                            ? definitions.end()
                            : definitions.find(store.operands[0].id);
                        if (def_it == definitions.end()
                                || !get_paired_append_store(
                                    *def_it->second, store.local)) {
                            state.erase(store.local);
                        }
                    }
                }
            }
        }
        return state;
    };

    std::vector<FreshLocalSet> fresh_in(cfg.blocks.size(), candidate_locals);
    std::vector<FreshLocalSet> fresh_out(cfg.blocks.size(), candidate_locals);
    std::vector<size_t> worklist;
    std::vector<uint8_t> queued(cfg.blocks.size(), 0);
    for (size_t block_id = 0; block_id < cfg.blocks.size(); ++block_id) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR in-place string append dataflow")) {
            return 0;
        }
        if (cfg.reachable[block_id]) {
            worklist.push_back(block_id);
            queued[block_id] = 1;
        }
    }
    size_t ignored_changed = 0;
    while (!worklist.empty()) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR in-place string append dataflow")) {
            return 0;
        }
        size_t block_id = worklist.back();
        worklist.pop_back();
        queued[block_id] = 0;
        FreshLocalSet input;
        if (block_id) {
            bool first = true;
            for (size_t predecessor : cfg.predecessors[block_id]) {
                if (qore_ir_analysis_cancelled(check_count,
                        "IR in-place string append dataflow")) {
                    return 0;
                }
                if (!cfg.reachable[predecessor]) {
                    continue;
                }
                if (first) {
                    input = fresh_out[predecessor];
                    first = false;
                    continue;
                }
                for (auto it = input.begin(); it != input.end();) {
                    if (qore_ir_analysis_cancelled(check_count,
                            "IR in-place string append dataflow")) {
                        return 0;
                    }
                    if (!fresh_out[predecessor].count(*it)) {
                        it = input.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
        fresh_in[block_id] = input;
        FreshLocalSet output =
            transfer_block(block_id, input, false, ignored_changed);
        if (cancelled) {
            return 0;
        }
        if (output == fresh_out[block_id]) {
            continue;
        }
        fresh_out[block_id] = std::move(output);
        for (size_t successor : cfg.successors[block_id]) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR in-place string append dataflow")) {
                return 0;
            }
            if (!queued[successor]) {
                queued[successor] = 1;
                worklist.push_back(successor);
            }
        }
    }

    size_t changed = 0;
    for (size_t block_id = 0; block_id < cfg.blocks.size(); ++block_id) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR in-place string append escape analysis")) {
            return changed;
        }
        if (cfg.reachable[block_id]) {
            transfer_block(block_id, fresh_in[block_id], true, changed);
            if (cancelled) {
                return changed;
            }
        }
    }
    return changed;
}

struct QoreIRNativeSSAValue {
    QoreIRValue value;
    size_t phi_block = std::numeric_limits<size_t>::max();

    bool isValid() const {
        return value.isValid() || phi_block != std::numeric_limits<size_t>::max();
    }

    bool operator==(const QoreIRNativeSSAValue& other) const {
        return value.id == other.value.id && phi_block == other.phi_block;
    }

    bool operator!=(const QoreIRNativeSSAValue& other) const {
        return !(*this == other);
    }
};

struct QoreIRNativeLocalPromotionStats {
    size_t loads = 0;
    size_t stores = 0;
};

static QoreIRNativeLocalPromotionStats qore_ir_promote_native_local_loads(QoreIRFunction& func,
        const QoreIRControlFlowGraph& cfg, size_t& check_count) {
    if (std::getenv("QORE_DISABLE_IR_NATIVE_LOCAL_SSA") || func.blocks.empty()
            || func.ir_only_locals.empty() || func.has_opaque_ast_local_access) {
        return {};
    }
    const bool eliminate_stores_enabled =
        !std::getenv("QORE_DISABLE_IR_NATIVE_LOCAL_STORE_ELIMINATION");

    // Exception handlers are not represented by normal CFG predecessors. Keep
    // promotion on normal control flow so every PHI edge is exact.
    for (const auto& block : func.blocks) {
        for (const auto& inst : block->instructions) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR native local SSA exception-flow analysis")) {
                return {};
            }
            if (inst->exception_target) {
                return {};
            }
        }
    }

    std::unordered_set<const LocalVar*> parameter_locals;
    for (const auto& [index, local] : func.param_local_vars) {
        (void)index;
        if (qore_ir_analysis_cancelled(check_count,
                "IR native local SSA parameter analysis")) {
            return {};
        }
        if (local) {
            parameter_locals.insert(local);
        }
    }

    struct LocalAccess {
        QoreIRInstruction* inst = nullptr;
        size_t block = 0;
    };
    std::unordered_map<const LocalVar*, std::vector<LocalAccess>> accesses;
    std::unordered_set<const LocalVar*> dedicated_accesses;
    for (size_t block_id = 0; block_id < func.blocks.size(); ++block_id) {
        for (const auto& inst_ptr : func.blocks[block_id]->instructions) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR native local SSA access analysis")) {
                return {};
            }
            QoreIRInstruction* inst = inst_ptr.get();
            if (inst->opcode == QoreIROpcode::LoadLocal
                    || inst->opcode == QoreIROpcode::StoreLocal
                    || inst->opcode == QoreIROpcode::InstantiateLocal
                    || inst->opcode == QoreIROpcode::UninstantiateLocal) {
                auto* local_inst = static_cast<QoreIRLocalInstruction*>(inst);
                if (local_inst->local) {
                    accesses[local_inst->local].push_back({inst, block_id});
                }
            } else if (inst->opcode == QoreIROpcode::AddAssignLocalInt) {
                auto* local_inst =
                    static_cast<QoreIRAddAssignLocalIntInstruction*>(inst);
                dedicated_accesses.insert(local_inst->target);
                dedicated_accesses.insert(local_inst->source);
            } else if (inst->opcode == QoreIROpcode::IncrementLocalInt) {
                dedicated_accesses.insert(
                    static_cast<QoreIRIncrementLocalIntInstruction*>(inst)->local);
            } else if (inst->opcode == QoreIROpcode::BranchIfLtLocalInt) {
                auto* local_inst =
                    static_cast<QoreIRBranchIfLtLocalIntInstruction*>(inst);
                dedicated_accesses.insert(local_inst->lhs);
                dedicated_accesses.insert(local_inst->rhs);
            }
        }
    }
    dedicated_accesses.erase(nullptr);

    const std::unordered_set<const void*> unsafe =
        qore_ir_get_native_unsafe_locals(func, {});
    struct Promotion {
        const LocalVar* local = nullptr;
        QoreIRValueRepresentation representation =
            QoreIRValueRepresentation::Unknown;
        QoreIRPhiValueKind phi_kind = QoreIRPhiValueKind::QoreValue;
        std::vector<size_t> phi_blocks;
        std::vector<QoreIRNativeSSAValue> output;
        bool eliminate_stores = true;
    };
    std::vector<Promotion> promotions;

    for (const auto& [local, local_accesses] : accesses) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR native local SSA candidate analysis")) {
            return {};
        }
        const void* key = reinterpret_cast<const void*>(local);
        const QoreTypeInfo* type = local ? local->getTypeInfo() : nullptr;
        QoreIRValueRepresentation representation =
            QoreIRValueRepresentation::Unknown;
        QoreIRPhiValueKind phi_kind = QoreIRPhiValueKind::QoreValue;
        if (type && !QoreTypeInfo::isReference(type)
                && !QoreTypeInfo::parseAcceptsReturns(type, NT_NOTHING)) {
            if (QoreTypeInfo::isType(type, NT_INT)
                    && !QoreTypeInfo::getReturnEnum(type)) {
                representation = QoreIRValueRepresentation::NativeInt;
                phi_kind = QoreIRPhiValueKind::NativeInt;
            } else if (QoreTypeInfo::isType(type, NT_FLOAT)) {
                representation = QoreIRValueRepresentation::NativeFloat;
                phi_kind = QoreIRPhiValueKind::NativeFloat;
            } else if (QoreTypeInfo::isType(type, NT_BOOLEAN)) {
                representation = QoreIRValueRepresentation::NativeBool;
                phi_kind = QoreIRPhiValueKind::NativeBool;
            }
        }
        if (representation == QoreIRValueRepresentation::Unknown
                || parameter_locals.count(local) || dedicated_accesses.count(local)
                || unsafe.count(key) || !func.ir_only_locals.count(key)
                || local->closureUse()) {
            continue;
        }

        bool valid = true;
        bool entry_store = false;
        bool eliminate_stores = eliminate_stores_enabled;
        size_t load_count = 0;
        for (const LocalAccess& access : local_accesses) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR native local SSA candidate validation")) {
                return {};
            }
            if (!cfg.reachable[access.block]) {
                valid = false;
                break;
            }
            if (access.inst->opcode == QoreIROpcode::StoreLocal) {
                auto* store =
                    static_cast<QoreIRLocalInstruction*>(access.inst);
                if (store->weak || store->is_ref || store->is_closure
                        || store->operands.size() != 1) {
                    valid = false;
                    break;
                }
                if (store->result.isValid()) {
                    eliminate_stores = false;
                }
                const QoreIRValueFacts* facts =
                    func.getValueFacts(store->operands[0]);
                if (!facts
                        || facts->assigned_state != QoreIRAssignedState::Assigned
                        || !facts->never_nothing
                        || facts->representation != representation) {
                    valid = false;
                    break;
                }
                if (access.block == 0) {
                    entry_store = true;
                }
            } else if (access.inst->opcode == QoreIROpcode::LoadLocal) {
                auto* load = static_cast<QoreIRLocalInstruction*>(access.inst);
                const QoreIRValueFacts* facts =
                    func.getValueFacts(load->result);
                if (load->is_ref || load->is_closure || !facts
                        || facts->assigned_state != QoreIRAssignedState::Assigned
                        || !facts->never_nothing
                        || facts->representation != representation) {
                    valid = false;
                    break;
                }
                ++load_count;
            }
        }
        if (!valid || !entry_store || !load_count) {
            continue;
        }

        // Compute pruned local liveness so PHIs are inserted only at joins where
        // the pre-join value can actually reach a load.
        std::vector<uint8_t> use_before_def(func.blocks.size(), 0);
        std::vector<uint8_t> has_def(func.blocks.size(), 0);
        for (size_t block_id = 0; block_id < func.blocks.size(); ++block_id) {
            bool defined = false;
            for (const auto& inst : func.blocks[block_id]->instructions) {
                if (qore_ir_analysis_cancelled(check_count,
                        "IR native local SSA liveness setup")) {
                    return {};
                }
                if ((inst->opcode == QoreIROpcode::StoreLocal
                            || inst->opcode == QoreIROpcode::UninstantiateLocal)
                        && static_cast<const QoreIRLocalInstruction*>(inst.get())
                            ->local == local) {
                    defined = true;
                    has_def[block_id] = 1;
                } else if (inst->opcode == QoreIROpcode::LoadLocal
                        && static_cast<const QoreIRLocalInstruction*>(inst.get())
                            ->local == local
                        && !defined) {
                    use_before_def[block_id] = 1;
                }
            }
        }
        std::vector<uint8_t> live_in(func.blocks.size(), 0);
        std::vector<uint8_t> live_out(func.blocks.size(), 0);
        std::vector<size_t> liveness_worklist;
        std::vector<uint8_t> liveness_queued(func.blocks.size(), 1);
        liveness_worklist.reserve(func.blocks.size());
        for (size_t block_id = func.blocks.size(); block_id-- > 0;) {
            liveness_worklist.push_back(block_id);
        }
        while (!liveness_worklist.empty()) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR native local SSA liveness analysis")) {
                return {};
            }
            size_t block_id = liveness_worklist.back();
            liveness_worklist.pop_back();
            liveness_queued[block_id] = 0;
            bool next_out = false;
            for (size_t successor : cfg.successors[block_id]) {
                if (qore_ir_analysis_cancelled(check_count,
                        "IR native local SSA liveness analysis")) {
                    return {};
                }
                if (live_in[successor]) {
                    next_out = true;
                    break;
                }
            }
            bool next_in =
                use_before_def[block_id] || (next_out && !has_def[block_id]);
            if (live_out[block_id] == next_out
                    && live_in[block_id] == next_in) {
                continue;
            }
            live_out[block_id] = next_out;
            live_in[block_id] = next_in;
            for (size_t predecessor : cfg.predecessors[block_id]) {
                if (!liveness_queued[predecessor]) {
                    liveness_queued[predecessor] = 1;
                    liveness_worklist.push_back(predecessor);
                }
            }
        }

        Promotion promotion;
        promotion.local = local;
        promotion.representation = representation;
        promotion.phi_kind = phi_kind;
        promotion.eliminate_stores = eliminate_stores;
        for (size_t block_id = 1; block_id < func.blocks.size(); ++block_id) {
            if (live_in[block_id] && cfg.predecessors[block_id].size() > 1) {
                bool all_reachable = true;
                for (size_t predecessor : cfg.predecessors[block_id]) {
                    if (qore_ir_analysis_cancelled(check_count,
                            "IR native local SSA predecessor analysis")) {
                        return {};
                    }
                    if (!cfg.reachable[predecessor]) {
                        all_reachable = false;
                        break;
                    }
                }
                if (!all_reachable) {
                    valid = false;
                    break;
                }
                promotion.phi_blocks.push_back(block_id);
            }
        }
        if (!valid) {
            continue;
        }

        std::unordered_set<size_t> phi_blocks(
            promotion.phi_blocks.begin(), promotion.phi_blocks.end());
        promotion.output.resize(func.blocks.size());
        std::vector<size_t> value_worklist;
        std::vector<uint8_t> value_queued(func.blocks.size(), 1);
        value_worklist.reserve(func.blocks.size());
        for (size_t block_id = func.blocks.size(); block_id-- > 0;) {
            value_worklist.push_back(block_id);
        }
        while (!value_worklist.empty()) {
            size_t block_id = value_worklist.back();
            value_worklist.pop_back();
            value_queued[block_id] = 0;
            if (qore_ir_analysis_cancelled(check_count,
                    "IR native local SSA value analysis")) {
                return {};
            }
            if (!cfg.reachable[block_id]) {
                continue;
            }
            QoreIRNativeSSAValue current;
            if (phi_blocks.count(block_id)) {
                current.phi_block = block_id;
            } else if (block_id && cfg.predecessors[block_id].size() == 1) {
                current = promotion.output[
                    cfg.predecessors[block_id].front()];
            }
            for (const auto& inst : func.blocks[block_id]->instructions) {
                if (qore_ir_analysis_cancelled(check_count,
                        "IR native local SSA value analysis")) {
                    return {};
                }
                if ((inst->opcode == QoreIROpcode::StoreLocal
                            || inst->opcode == QoreIROpcode::UninstantiateLocal)
                        && static_cast<const QoreIRLocalInstruction*>(inst.get())
                            ->local == local) {
                    if (inst->opcode == QoreIROpcode::StoreLocal) {
                        current.value = inst->operands[0];
                        current.phi_block =
                            std::numeric_limits<size_t>::max();
                    } else {
                        current = QoreIRNativeSSAValue();
                    }
                }
            }
            if (promotion.output[block_id] == current) {
                continue;
            }
            promotion.output[block_id] = current;
            for (size_t successor : cfg.successors[block_id]) {
                if (!value_queued[successor]) {
                    value_queued[successor] = 1;
                    value_worklist.push_back(successor);
                }
            }
        }
        for (size_t phi_block : promotion.phi_blocks) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR native local SSA incoming validation")) {
                return {};
            }
            for (size_t predecessor : cfg.predecessors[phi_block]) {
                if (qore_ir_analysis_cancelled(check_count,
                        "IR native local SSA incoming validation")) {
                    return {};
                }
                if (!promotion.output[predecessor].isValid()) {
                    valid = false;
                    break;
                }
            }
            if (!valid) {
                break;
            }
        }
        // Validate every load before committing. This is required for no-PHI
        // candidates where a multi-predecessor block is legal only when a
        // dominating store in that block establishes the local first.
        for (size_t block_id = 0; valid && block_id < func.blocks.size(); ++block_id) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR native local SSA load validation")) {
                return {};
            }
            if (!cfg.reachable[block_id]) {
                continue;
            }
            QoreIRNativeSSAValue current;
            if (phi_blocks.count(block_id)) {
                current.phi_block = block_id;
            } else if (block_id && cfg.predecessors[block_id].size() == 1) {
                current = promotion.output[cfg.predecessors[block_id].front()];
            }
            for (const auto& inst : func.blocks[block_id]->instructions) {
                if (qore_ir_analysis_cancelled(check_count,
                        "IR native local SSA load validation")) {
                    return {};
                }
                if (inst->opcode == QoreIROpcode::StoreLocal
                        && static_cast<const QoreIRLocalInstruction*>(inst.get())
                            ->local == local) {
                    current.value = inst->operands[0];
                    current.phi_block = std::numeric_limits<size_t>::max();
                } else if (inst->opcode == QoreIROpcode::UninstantiateLocal
                        && static_cast<const QoreIRLocalInstruction*>(inst.get())
                            ->local == local) {
                    current = QoreIRNativeSSAValue();
                } else if (inst->opcode == QoreIROpcode::LoadLocal
                        && static_cast<const QoreIRLocalInstruction*>(inst.get())
                            ->local == local
                        && !current.isValid()) {
                    valid = false;
                    break;
                }
            }
        }
        if (valid) {
            promotions.push_back(std::move(promotion));
        }
    }
    if (promotions.empty()) {
        return {};
    }

    std::unordered_map<uint32_t, QoreIRValue> replacements;
    std::unordered_set<const QoreIRInstruction*> eliminated_loads;
    std::unordered_set<const QoreIRInstruction*> eliminated_stores;
    for (Promotion& promotion : promotions) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR native local SSA commit preparation")) {
            return {};
        }
        std::unordered_map<size_t, QoreIRPhiInstruction*> phis;
        for (size_t block_id : promotion.phi_blocks) {
            (void)qore_ir_analysis_cancelled(check_count,
                "IR native local SSA commit");
            auto phi = std::make_unique<QoreIRPhiInstruction>();
            QoreIRPhiInstruction* raw_phi = phi.get();
            raw_phi->result = func.createValue();
            func.max_value_id = std::max(
                func.max_value_id, raw_phi->result.id);
            raw_phi->value_kind = promotion.phi_kind;
            QoreIRValueFacts facts;
            facts.type_info = promotion.local->getTypeInfo();
            facts.assigned_state = QoreIRAssignedState::Assigned;
            facts.representation = promotion.representation;
            facts.never_nothing = true;
            func.setValueFacts(raw_phi->result, facts);
            auto& instructions = func.blocks[block_id]->instructions;
            auto insert_at = instructions.begin();
            while (insert_at != instructions.end()
                    && (*insert_at)->opcode == QoreIROpcode::Phi) {
                (void)qore_ir_analysis_cancelled(check_count,
                    "IR native local SSA commit");
                ++insert_at;
            }
            instructions.insert(insert_at, std::move(phi));
            func.blocks[block_id]->has_phi_nodes = true;
            phis.emplace(block_id, raw_phi);
        }
        auto resolve = [&](const QoreIRNativeSSAValue& value) {
            if (value.value.isValid()) {
                return value.value;
            }
            auto phi = phis.find(value.phi_block);
            return phi == phis.end() ? QoreIRValue() : phi->second->result;
        };
        for (size_t block_id : promotion.phi_blocks) {
            (void)qore_ir_analysis_cancelled(check_count,
                "IR native local SSA commit");
            QoreIRPhiInstruction* phi = phis.at(block_id);
            for (size_t predecessor : cfg.predecessors[block_id]) {
                (void)qore_ir_analysis_cancelled(check_count,
                    "IR native local SSA commit");
                QoreIRValue value = resolve(promotion.output[predecessor]);
                phi->incoming.push_back(
                    {value, func.blocks[predecessor].get()});
                phi->operands.push_back(value);
            }
        }

        for (size_t block_id = 0; block_id < func.blocks.size(); ++block_id) {
            (void)qore_ir_analysis_cancelled(check_count,
                "IR native local SSA commit");
            QoreIRNativeSSAValue current;
            auto phi = phis.find(block_id);
            if (phi != phis.end()) {
                current.value = phi->second->result;
            } else if (block_id && cfg.predecessors[block_id].size() == 1) {
                current = promotion.output[
                    cfg.predecessors[block_id].front()];
                current.value = resolve(current);
                current.phi_block = std::numeric_limits<size_t>::max();
            }
            for (const auto& inst : func.blocks[block_id]->instructions) {
                (void)qore_ir_analysis_cancelled(check_count,
                    "IR native local SSA commit");
                if (inst->opcode == QoreIROpcode::StoreLocal
                        && static_cast<const QoreIRLocalInstruction*>(inst.get())
                            ->local == promotion.local) {
                    current.value = inst->operands[0];
                    current.phi_block = std::numeric_limits<size_t>::max();
                    if (promotion.eliminate_stores) {
                        eliminated_stores.insert(inst.get());
                    }
                } else if (inst->opcode == QoreIROpcode::UninstantiateLocal
                        && static_cast<const QoreIRLocalInstruction*>(inst.get())
                            ->local == promotion.local) {
                    current = QoreIRNativeSSAValue();
                } else if (inst->opcode == QoreIROpcode::LoadLocal
                        && static_cast<const QoreIRLocalInstruction*>(inst.get())
                            ->local == promotion.local) {
                    QoreIRValue replacement = resolve(current);
                    if (!replacement.isValid()) {
                        return {};
                    }
                    replacements.emplace(inst->result.id, replacement);
                    eliminated_loads.insert(inst.get());
                }
            }
        }
    }

    // Finish the committed rewrite even when cancellation is requested so no
    // operand can retain a reference to an erased load definition.
    for (auto& [result, replacement] : replacements) {
        (void)result;
        std::unordered_set<uint32_t> seen;
        while (seen.insert(replacement.id).second) {
            (void)qore_ir_analysis_cancelled(check_count,
                "IR native local SSA commit");
            auto next = replacements.find(replacement.id);
            if (next == replacements.end()) {
                break;
            }
            replacement = next->second;
        }
    }
    for (const auto& block : func.blocks) {
        auto& instructions = block->instructions;
        for (auto it = instructions.begin(); it != instructions.end();) {
            if (++check_count % 100 == 0) {
                (void)qore_check_cancel(nullptr, "IR native local SSA commit");
            }
            if (eliminated_loads.count(it->get())
                    || eliminated_stores.count(it->get())) {
                it = instructions.erase(it);
            } else {
                (void)qore_ir_rewrite_value_operands(
                    **it, replacements, check_count, false);
                ++it;
            }
        }
    }
    return {eliminated_loads.size(), eliminated_stores.size()};
}

static QoreIRValueRepresentation qore_ir_conditional_native_representation(
        QoreIROpcode opcode) {
    switch (opcode) {
        case QoreIROpcode::AddInt:
        case QoreIROpcode::SubInt:
        case QoreIROpcode::MulInt:
        case QoreIROpcode::DivInt:
        case QoreIROpcode::ModInt:
        case QoreIROpcode::ShlInt:
        case QoreIROpcode::ShrInt:
        case QoreIROpcode::AndInt:
        case QoreIROpcode::OrInt:
        case QoreIROpcode::XorInt:
        case QoreIROpcode::AddAssignInt:
        case QoreIROpcode::SubAssignInt:
        case QoreIROpcode::MulAssignInt:
        case QoreIROpcode::DivAssignInt:
        case QoreIROpcode::ModAssignInt:
        case QoreIROpcode::ShlAssignInt:
        case QoreIROpcode::ShrAssignInt:
        case QoreIROpcode::AndAssignInt:
        case QoreIROpcode::OrAssignInt:
        case QoreIROpcode::XorAssignInt:
            return QoreIRValueRepresentation::NativeInt;
        case QoreIROpcode::AddFloat:
        case QoreIROpcode::SubFloat:
        case QoreIROpcode::MulFloat:
        case QoreIROpcode::DivFloat:
        case QoreIROpcode::AddAssignFloat:
        case QoreIROpcode::SubAssignFloat:
        case QoreIROpcode::MulAssignFloat:
        case QoreIROpcode::DivAssignFloat:
            return QoreIRValueRepresentation::NativeFloat;
        default:
            return QoreIRValueRepresentation::Unknown;
    }
}

static bool qore_ir_value_is_proven_assigned(
        const QoreIRFunction& func, QoreIRValue value,
        const std::unordered_map<uint32_t, const QoreIRInstruction*>& definitions,
        const std::unordered_set<const LocalVar*>* known_locals,
        std::unordered_set<uint32_t>& visiting, size_t& check_count) {
    if (!value.isValid()
            || qore_ir_analysis_cancelled(check_count,
                "IR local assigned-value proof")
            || !visiting.insert(value.id).second) {
        return false;
    }
    auto finish = [&visiting, value](bool result) {
        visiting.erase(value.id);
        return result;
    };
    auto definition = definitions.find(value.id);
    if (definition != definitions.end() && definition->second) {
        const QoreIRInstruction* inst = definition->second;
        if (inst->opcode == QoreIROpcode::LoadLocal && known_locals) {
            const auto* load =
                static_cast<const QoreIRLocalInstruction*>(inst);
            return finish(load->local
                && known_locals->count(load->local));
        }
        if (qore_ir_conditional_native_representation(inst->opcode)
                != QoreIRValueRepresentation::Unknown) {
            if (inst->operands.empty()) {
                return finish(false);
            }
            for (QoreIRValue operand : inst->operands) {
                if (!qore_ir_value_is_proven_assigned(func, operand,
                        definitions, known_locals, visiting, check_count)) {
                    return finish(false);
                }
            }
            return finish(true);
        }
    }
    const QoreIRValueFacts* facts = func.getValueFacts(value);
    return finish(facts
        && facts->assigned_state == QoreIRAssignedState::Assigned
        && facts->never_nothing);
}

static size_t qore_ir_refine_local_value_facts(QoreIRFunction& func,
        const QoreIRControlFlowGraph& cfg, size_t& check_count) {
    if (std::getenv("QORE_DISABLE_IR_LOCAL_VALUE_FACTS")
            || func.blocks.empty() || func.has_opaque_ast_local_access) {
        return 0;
    }

    std::unordered_set<const LocalVar*> universe;
    for (const auto& block : func.blocks) {
        for (const auto& inst : block->instructions) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR local value fact candidate analysis")) {
                return 0;
            }
            if (inst->opcode == QoreIROpcode::LoadLocal
                    || inst->opcode == QoreIROpcode::StoreLocal
                    || inst->opcode == QoreIROpcode::UninstantiateLocal) {
                const auto* local =
                    static_cast<const QoreIRLocalInstruction*>(inst.get())
                        ->local;
                const QoreTypeInfo* type =
                    local ? local->getTypeInfo() : nullptr;
                const QoreTypeInfo* value_type =
                    type ? qore_get_value_type(type) : nullptr;
                bool native_scalar = value_type
                    && ((QoreTypeInfo::isType(value_type, NT_INT)
                            && !QoreTypeInfo::getReturnEnum(value_type))
                        || QoreTypeInfo::isType(value_type, NT_FLOAT)
                        || QoreTypeInfo::isType(value_type, NT_BOOLEAN));
                if (local && !local->closureUse()
                        && !QoreTypeInfo::isReference(type)
                        && native_scalar) {
                    universe.insert(local);
                }
            }
        }
    }
    if (universe.empty()) {
        return 0;
    }

    std::unordered_set<const LocalVar*> initially_known;
    for (const auto& [index, local] : func.param_local_vars) {
        (void)index;
        if (qore_ir_analysis_cancelled(check_count,
                "IR local value fact parameter analysis")) {
            return 0;
        }
        if (local && universe.count(local)
                && !QoreTypeInfo::parseAcceptsReturns(
                    local->getTypeInfo(), NT_NOTHING)) {
            initially_known.insert(local);
        }
    }

    auto transfer_instruction = [&](const QoreIRInstruction* inst,
            std::unordered_set<const LocalVar*>& known) -> bool {
        if (qore_ir_analysis_cancelled(check_count,
                "IR local value fact transfer")) {
            return false;
        }
        if (inst->opcode == QoreIROpcode::StoreLocal) {
            const auto* store =
                static_cast<const QoreIRLocalInstruction*>(inst);
            if (!store->local || !universe.count(store->local)
                    || store->operands.size() != 1 || store->weak
                    || store->is_ref || store->is_closure) {
                if (store->local) {
                    known.erase(store->local);
                }
                return true;
            }
            const QoreIRValueFacts* facts =
                func.getValueFacts(store->operands[0]);
            if (facts
                    && facts->assigned_state
                        == QoreIRAssignedState::Assigned
                    && facts->never_nothing) {
                known.insert(store->local);
            } else {
                known.erase(store->local);
            }
            return true;
        }
        if (inst->opcode == QoreIROpcode::UninstantiateLocal) {
            const auto* local =
                static_cast<const QoreIRLocalInstruction*>(inst);
            if (local->local) {
                known.erase(local->local);
            }
            return true;
        }
        if (inst->opcode == QoreIROpcode::LoadLocal) {
            return true;
        }

        bool has_ref_args = true;
        const AbstractQoreFunctionVariant* callee =
            qore_ir_get_resolved_effect_callee(inst, has_ref_args);
        bool direct_call = inst->opcode == QoreIROpcode::CallDirect
            || inst->opcode == QoreIROpcode::CallStaticDirect
            || inst->opcode == QoreIROpcode::CallMethodDirect
            || inst->opcode == QoreIROpcode::InvokeMethodDirect
            || inst->opcode == QoreIROpcode::CallClosureDirect;
        if (direct_call && (!callee || has_ref_args)) {
            known.clear();
            return true;
        }
        if (!direct_call
                && qore_ir_instruction_may_invalidate_caller_caches(
                    func, inst)) {
            const LocalVar* written = qore_ir_get_written_local(inst);
            if (written) {
                known.erase(written);
            } else {
                known.clear();
            }
        }
        return true;
    };
    auto transfer = [&](size_t block_id,
            std::unordered_set<const LocalVar*>& known) -> bool {
        for (const auto& inst : func.blocks[block_id]->instructions) {
            if (!transfer_instruction(inst.get(), known)) {
                return false;
            }
        }
        return true;
    };

    std::vector<std::unordered_set<const LocalVar*>> in(func.blocks.size());
    std::vector<std::unordered_set<const LocalVar*>> out(
        func.blocks.size(), universe);
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t block_id = 0; block_id < func.blocks.size(); ++block_id) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR local value fact fixed-point analysis")) {
                return 0;
            }
            std::unordered_set<const LocalVar*> next = block_id == 0
                ? initially_known : std::unordered_set<const LocalVar*>();
            if (block_id && !cfg.predecessors[block_id].empty()) {
                next = out[cfg.predecessors[block_id].front()];
                for (size_t pred_index = 1;
                        pred_index < cfg.predecessors[block_id].size();
                        ++pred_index) {
                    if (!(pred_index % 100)
                            && qore_ir_analysis_cancelled(check_count,
                                "IR local value fact predecessor intersection")) {
                        return 0;
                    }
                    const auto& predecessor =
                        out[cfg.predecessors[block_id][pred_index]];
                    size_t local_count = 0;
                    for (auto it = next.begin(); it != next.end();) {
                        if (++local_count % 100 == 0
                                && qore_ir_analysis_cancelled(check_count,
                                    "IR local value fact set intersection")) {
                            return 0;
                        }
                        if (!predecessor.count(*it)) {
                            it = next.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
            }
            in[block_id] = next;
            if (!transfer(block_id, next)) {
                return 0;
            }
            if (out[block_id] != next) {
                out[block_id] = std::move(next);
                changed = true;
            }
        }
    }

    size_t refined = 0;
    for (size_t block_id = 0; block_id < func.blocks.size(); ++block_id) {
        std::unordered_set<const LocalVar*> known = in[block_id];
        for (const auto& inst_ptr : func.blocks[block_id]->instructions) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR local value fact annotation")) {
                return refined;
            }
            const QoreIRInstruction* inst = inst_ptr.get();
            if (inst->opcode == QoreIROpcode::LoadLocal) {
                const auto* load =
                    static_cast<const QoreIRLocalInstruction*>(inst);
                if (load->local && universe.count(load->local)
                        && inst->result.isValid()) {
                    QoreIRValueFacts facts;
                    if (const QoreIRValueFacts* current =
                            func.getValueFacts(inst->result)) {
                        facts = *current;
                    }
                    QoreIRValueFacts refined_facts = facts;
                    refined_facts.type_info = load->local->getTypeInfo();
                    bool assigned = known.count(load->local);
                    refined_facts.assigned_state = assigned
                        ? QoreIRAssignedState::Assigned
                        : QoreIRAssignedState::MaybeAssigned;
                    refined_facts.never_nothing = assigned;
                    const QoreTypeInfo* value_type =
                        qore_get_value_type(refined_facts.type_info);
                    refined_facts.representation = !assigned
                        ? QoreIRValueRepresentation::Boxed
                        : QoreTypeInfo::isType(value_type, NT_INT)
                            && !QoreTypeInfo::getReturnEnum(value_type)
                            ? QoreIRValueRepresentation::NativeInt
                            : QoreTypeInfo::isType(value_type, NT_FLOAT)
                                ? QoreIRValueRepresentation::NativeFloat
                                : QoreIRValueRepresentation::NativeBool;
                    if (facts.assigned_state != refined_facts.assigned_state
                            || facts.never_nothing
                                != refined_facts.never_nothing
                            || facts.type_info != refined_facts.type_info
                            || facts.representation
                                != refined_facts.representation) {
                        func.setValueFacts(inst->result, refined_facts);
                        ++refined;
                    }
                }
            }
            if (!transfer_instruction(inst, known)) {
                return refined;
            }
        }
    }
    return refined;
}

bool qore_ir_values_proven_assigned_at(const QoreIRFunction& func,
        const QoreIRInstruction* point,
        const std::vector<QoreIRValue>& values) {
    if (!point || values.empty() || func.blocks.empty()
            || func.has_opaque_ast_local_access) {
        return false;
    }
    size_t check_count = 0;
    QoreIRControlFlowGraph cfg(func);
    if (cfg.cancelled) {
        return false;
    }

    size_t point_block = SIZE_MAX;
    std::unordered_map<uint32_t, const QoreIRInstruction*> definitions;
    std::unordered_set<const LocalVar*> universe;
    for (size_t block_id = 0; block_id < func.blocks.size(); ++block_id) {
        for (const auto& inst : func.blocks[block_id]->instructions) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR point assigned-value setup")) {
                return false;
            }
            if (inst.get() == point) {
                point_block = block_id;
            }
            if (inst->result.isValid()) {
                definitions.emplace(inst->result.id, inst.get());
            }
            if (inst->opcode == QoreIROpcode::LoadLocal
                    || inst->opcode == QoreIROpcode::StoreLocal
                    || inst->opcode == QoreIROpcode::UninstantiateLocal) {
                const LocalVar* local =
                    static_cast<const QoreIRLocalInstruction*>(inst.get())
                        ->local;
                const QoreTypeInfo* type = local
                    ? local->getTypeInfo() : nullptr;
                if (local && !local->closureUse()
                        && !QoreTypeInfo::isReference(type)) {
                    universe.insert(local);
                }
            }
        }
    }
    if (point_block == SIZE_MAX || !cfg.reachable[point_block]) {
        return false;
    }

    std::unordered_set<const LocalVar*> initially_known;
    for (const auto& [index, local] : func.param_local_vars) {
        (void)index;
        if (qore_ir_analysis_cancelled(check_count,
                "IR point assigned-value parameter analysis")) {
            return false;
        }
        if (local && universe.count(local)
                && !QoreTypeInfo::parseAcceptsReturns(
                    local->getTypeInfo(), NT_NOTHING)) {
            initially_known.insert(local);
        }
    }

    auto transfer_instruction = [&](const QoreIRInstruction* inst,
            std::unordered_set<const LocalVar*>& known) -> bool {
        if (qore_ir_analysis_cancelled(check_count,
                "IR point assigned-value transfer")) {
            return false;
        }
        if (inst->opcode == QoreIROpcode::StoreLocal) {
            const auto* store =
                static_cast<const QoreIRLocalInstruction*>(inst);
            if (!store->local || !universe.count(store->local)
                    || store->operands.size() != 1 || store->weak
                    || store->is_ref || store->is_closure) {
                if (store->local) {
                    known.erase(store->local);
                }
                return true;
            }
            std::unordered_set<uint32_t> visiting;
            if (qore_ir_value_is_proven_assigned(func,
                    store->operands[0], definitions, &known, visiting,
                    check_count)) {
                known.insert(store->local);
            } else {
                known.erase(store->local);
            }
            return true;
        }
        if (inst->opcode == QoreIROpcode::UninstantiateLocal) {
            const auto* local =
                static_cast<const QoreIRLocalInstruction*>(inst);
            if (local->local) {
                known.erase(local->local);
            }
            return true;
        }
        if (inst->opcode == QoreIROpcode::LoadLocal) {
            return true;
        }

        bool has_ref_args = true;
        const AbstractQoreFunctionVariant* callee =
            qore_ir_get_resolved_effect_callee(inst, has_ref_args);
        bool direct_call = inst->opcode == QoreIROpcode::CallDirect
            || inst->opcode == QoreIROpcode::CallStaticDirect
            || inst->opcode == QoreIROpcode::CallMethodDirect
            || inst->opcode == QoreIROpcode::InvokeMethodDirect
            || inst->opcode == QoreIROpcode::CallClosureDirect;
        if (direct_call && (!callee || has_ref_args)) {
            known.clear();
            return true;
        }
        if (!direct_call
                && qore_ir_instruction_may_invalidate_caller_caches(
                    func, inst)) {
            const LocalVar* written = qore_ir_get_written_local(inst);
            if (written) {
                known.erase(written);
            } else {
                known.clear();
            }
        }
        return true;
    };
    auto transfer_block = [&](size_t block_id,
            std::unordered_set<const LocalVar*>& known) -> bool {
        for (const auto& inst : func.blocks[block_id]->instructions) {
            if (!transfer_instruction(inst.get(), known)) {
                return false;
            }
        }
        return true;
    };

    std::vector<std::unordered_set<const LocalVar*>> in(func.blocks.size());
    std::vector<std::unordered_set<const LocalVar*>> out(
        func.blocks.size(), universe);
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t block_id = 0; block_id < func.blocks.size(); ++block_id) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR point assigned-value fixed-point analysis")) {
                return false;
            }
            if (!cfg.reachable[block_id]) {
                continue;
            }
            std::unordered_set<const LocalVar*> next = block_id == 0
                ? initially_known : universe;
            bool have_predecessor = block_id == 0;
            if (block_id) {
                size_t predecessor_count = 0;
                for (size_t predecessor : cfg.predecessors[block_id]) {
                    if (++predecessor_count % 100 == 0
                            && qore_ir_analysis_cancelled(check_count,
                                "IR point assigned-value predecessor intersection")) {
                        return false;
                    }
                    if (!cfg.reachable[predecessor]) {
                        continue;
                    }
                    if (!have_predecessor) {
                        next = out[predecessor];
                        have_predecessor = true;
                        continue;
                    }
                    size_t local_count = 0;
                    for (auto it = next.begin(); it != next.end();) {
                        if (++local_count % 100 == 0
                                && qore_ir_analysis_cancelled(check_count,
                                    "IR point assigned-value set intersection")) {
                            return false;
                        }
                        if (!out[predecessor].count(*it)) {
                            it = next.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
            }
            if (!have_predecessor) {
                next.clear();
            }
            in[block_id] = next;
            if (!transfer_block(block_id, next)) {
                return false;
            }
            if (out[block_id] != next) {
                out[block_id] = std::move(next);
                changed = true;
            }
        }
    }

    std::unordered_set<const LocalVar*> known = in[point_block];
    for (const auto& inst : func.blocks[point_block]->instructions) {
        if (inst.get() == point) {
            size_t value_count = 0;
            for (QoreIRValue value : values) {
                if (++value_count % 100 == 0
                        && qore_ir_analysis_cancelled(check_count,
                            "IR point assigned-value result validation")) {
                    return false;
                }
                std::unordered_set<uint32_t> visiting;
                if (!qore_ir_value_is_proven_assigned(func, value,
                        definitions, &known, visiting, check_count)) {
                    return false;
                }
            }
            return true;
        }
        if (!transfer_instruction(inst.get(), known)) {
            return false;
        }
    }
    return false;
}

bool qore_ir_complex_hash_initializer_prechecked(const QoreIRFunction& func,
        const QoreIRInstruction* definition, const QoreTypeInfo* target_type) {
    if (!definition
            || std::getenv("QORE_DISABLE_IR_COMPLEX_HASH_PRECHECK")) {
        return false;
    }
    target_type = func.specializeType(target_type);
    const QoreTypeInfo* target_value_type =
        QoreTypeInfo::getUniqueReturnComplexHash(target_type);
    if (!target_value_type) {
        target_value_type =
            QoreTypeInfo::getReturnComplexHashOrNothing(target_type);
    }
    if (!QoreTypeInfo::hasType(target_value_type)
            || target_value_type == autoTypeInfo
            || target_value_type == anyTypeInfo) {
        return false;
    }

    size_t check_count = 0;
    if (definition->opcode != QoreIROpcode::MakeHash
            && definition->opcode != QoreIROpcode::MakeHashConstKeys) {
        return false;
    }

    const QoreTypeInfo* source_type = definition->opcode
        == QoreIROpcode::MakeHash
        ? static_cast<const QoreIRMakeHashInstruction*>(definition)->typeInfo
        : static_cast<const QoreIRMakeHashConstKeysInstruction*>(definition)->typeInfo;
    source_type = func.specializeType(source_type);
    const QoreTypeInfo* source_value_type =
        QoreTypeInfo::getUniqueReturnComplexHash(source_type);
    if (!source_value_type) {
        source_value_type =
            QoreTypeInfo::getReturnComplexHashOrNothing(source_type);
    }
    if (QoreTypeInfo::hasType(source_value_type)
            && source_value_type != autoTypeInfo
            && source_value_type != anyTypeInfo
            && QoreTypeInfo::isInputIdentical(
                target_value_type, source_value_type)) {
        return true;
    }

    std::vector<QoreIRValue> values;
    if (definition->opcode == QoreIROpcode::MakeHashConstKeys) {
        const auto* make =
            static_cast<const QoreIRMakeHashConstKeysInstruction*>(definition);
        if (make->keys.size() != definition->operands.size()) {
            return false;
        }
        values = definition->operands;
    } else {
        size_t operand_count = definition->operands.size();
        if (operand_count % 2) {
            --operand_count;
        }
        values.reserve(operand_count / 2);
        for (size_t i = 1; i < operand_count; i += 2) {
            values.push_back(definition->operands[i]);
        }
    }

    for (QoreIRValue value : values) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR typed-hash initializer value analysis")) {
            return false;
        }
        const QoreIRValueFacts* facts = func.getValueFacts(value);
        if (!facts || facts->assigned_state != QoreIRAssignedState::Assigned
                || !facts->never_nothing || !facts->type_info) {
            return false;
        }
        const QoreTypeInfo* value_type =
            qore_get_value_type(func.specializeType(facts->type_info));
        if (!QoreTypeInfo::hasType(value_type)
                || !QoreTypeInfo::isInputIdentical(
                    target_value_type, value_type)) {
            return false;
        }
    }
    return true;
}

bool qore_ir_hashdecl_literal_keys_prechecked(
        const QoreIRInstruction* initializer, const TypedHashDecl* target) {
    if (!initializer || !target
            || std::getenv("QORE_DISABLE_IR_HASHDECL_KEY_PROOF")
            || initializer->opcode != QoreIROpcode::MakeHashConstKeys) {
        return false;
    }
    const auto* make =
        static_cast<const QoreIRMakeHashConstKeysInstruction*>(initializer);
    if (make->keys.size() != initializer->operands.size()) {
        return false;
    }
    const typed_hash_decl_private* target_private =
        typed_hash_decl_private::get(*target);
    size_t check_count = 0;
    for (const std::string& key : make->keys) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR hashdecl initializer key analysis")
                || !target_private->findMember(key.c_str())) {
            return false;
        }
    }
    return true;
}

bool qore_ir_hashdecl_literal_values_prechecked(const QoreIRFunction& func,
        const QoreIRInstruction* initializer, const TypedHashDecl* target,
        bool operands_native_and_assigned) {
    if (!initializer || !target
            || std::getenv("QORE_DISABLE_IR_HASHDECL_VALUE_PROOF")
            || initializer->opcode != QoreIROpcode::MakeHashConstKeys) {
        return false;
    }
    const auto* make =
        static_cast<const QoreIRMakeHashConstKeysInstruction*>(initializer);
    if (make->keys.size() != initializer->operands.size()) {
        return false;
    }
    if (!operands_native_and_assigned
            && !qore_ir_values_proven_assigned_at(
                func, initializer, initializer->operands)) {
        return false;
    }
    const typed_hash_decl_private* target_private =
        typed_hash_decl_private::get(*target);
    size_t check_count = 0;
    for (size_t i = 0; i < make->keys.size(); ++i) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR hashdecl initializer value analysis")) {
            return false;
        }
        const HashDeclMemberInfo* member =
            target_private->findMember(make->keys[i].c_str());
        const QoreIRValueFacts* facts =
            func.getValueFacts(initializer->operands[i]);
        if (!member || !facts || !facts->type_info) {
            return false;
        }
        const QoreTypeInfo* member_type =
            func.specializeType(member->getTypeInfo());
        const QoreTypeInfo* value_type =
            qore_get_value_type(func.specializeType(facts->type_info));
        if (!QoreTypeInfo::hasType(member_type)
                || !QoreTypeInfo::hasType(value_type)
                || !QoreTypeInfo::isInputIdentical(member_type, value_type)) {
            return false;
        }
    }
    return true;
}

bool qore_ir_hashdecl_literal_layout_prechecked(
        const QoreIRInstruction* initializer, const TypedHashDecl* target) {
    if (!initializer || !target
            || std::getenv("QORE_DISABLE_IR_HASHDECL_LAYOUT_PROOF")
            || initializer->opcode != QoreIROpcode::MakeHashConstKeys) {
        return false;
    }
    const auto* make =
        static_cast<const QoreIRMakeHashConstKeysInstruction*>(initializer);
    return make->keys.size() == initializer->operands.size()
        && typed_hash_decl_private::get(*target)
            ->matchesLiteralMemberOrder(make->keys);
}

struct QoreIRDenseListStats {
    size_t loads = 0;
    size_t joins = 0;
};

static QoreIRDenseListStats qore_ir_refine_dense_list_facts(
        QoreIRFunction& func, const QoreIRControlFlowGraph& cfg,
        size_t& check_count) {
    QoreIRDenseListStats stats;
    if (std::getenv("QORE_DISABLE_IR_LIST_DENSITY")
            || func.blocks.empty() || func.has_opaque_ast_local_access) {
        return stats;
    }
    for (const auto& block : func.blocks) {
        for (const auto& inst : block->instructions) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR dense list exception-flow analysis")) {
                return {};
            }
            if (inst->exception_target) {
                return stats;
            }
        }
    }

    std::unordered_set<const LocalVar*> universe;
    for (const auto& block : func.blocks) {
        for (const auto& inst : block->instructions) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR dense list candidate analysis")) {
                return {};
            }
            if (inst->opcode != QoreIROpcode::StoreLocal) {
                continue;
            }
            const auto* local_inst =
                static_cast<const QoreIRLocalInstruction*>(inst.get());
            const LocalVar* local = local_inst->local;
            const QoreTypeInfo* type = local ? local->getTypeInfo() : nullptr;
            if (local && !local->closureUse()
                    && !QoreTypeInfo::isReference(type)
                    && local_inst->operands.size() == 1) {
                const QoreIRValueFacts* facts =
                    func.getValueFacts(local_inst->operands[0]);
                if (!facts
                        || facts->list_density != QoreIRListDensity::Dense) {
                    continue;
                }
                universe.insert(local);
            }
        }
    }
    if (universe.empty()) {
        return stats;
    }

    auto transfer_instruction = [&](const QoreIRInstruction* inst,
            std::unordered_set<const LocalVar*>& known) -> bool {
        if (qore_ir_analysis_cancelled(check_count,
                "IR dense list transfer")) {
            return false;
        }
        if (inst->opcode == QoreIROpcode::StoreLocal) {
            const auto* store =
                static_cast<const QoreIRLocalInstruction*>(inst);
            if (!store->local || !universe.count(store->local)) {
                return true;
            }
            if (store->operands.size() != 1 || store->weak
                    || store->is_ref || store->is_closure) {
                known.erase(store->local);
                return true;
            }
            const QoreIRValueFacts* facts =
                func.getValueFacts(store->operands[0]);
            if (facts
                    && facts->list_density == QoreIRListDensity::Dense) {
                known.insert(store->local);
            } else {
                known.erase(store->local);
            }
            return true;
        }
        if (inst->opcode == QoreIROpcode::UninstantiateLocal) {
            const auto* local =
                static_cast<const QoreIRLocalInstruction*>(inst);
            if (local->local) {
                known.erase(local->local);
            }
            return true;
        }
        if (inst->opcode == QoreIROpcode::LoadLocal) {
            return true;
        }

        bool has_ref_args = true;
        const AbstractQoreFunctionVariant* callee =
            qore_ir_get_resolved_effect_callee(inst, has_ref_args);
        bool direct_call = inst->opcode == QoreIROpcode::CallDirect
            || inst->opcode == QoreIROpcode::CallStaticDirect
            || inst->opcode == QoreIROpcode::CallMethodDirect
            || inst->opcode == QoreIROpcode::InvokeMethodDirect
            || inst->opcode == QoreIROpcode::CallClosureDirect;
        if (direct_call && (!callee || has_ref_args)) {
            known.clear();
            return true;
        }
        if (!direct_call
                && qore_ir_instruction_may_invalidate_caller_caches(
                    func, inst)) {
            const LocalVar* written = qore_ir_get_written_local(inst);
            if (written) {
                known.erase(written);
            } else {
                known.clear();
            }
        }
        return true;
    };
    auto transfer = [&](size_t block_id,
            std::unordered_set<const LocalVar*>& known) -> bool {
        for (const auto& inst : func.blocks[block_id]->instructions) {
            if (!transfer_instruction(inst.get(), known)) {
                return false;
            }
        }
        return true;
    };

    std::vector<std::unordered_set<const LocalVar*>> in(func.blocks.size());
    std::vector<std::unordered_set<const LocalVar*>> out(
        func.blocks.size(), universe);
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t block_id = 0; block_id < func.blocks.size(); ++block_id) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR dense list fixed-point analysis")) {
                return {};
            }
            std::unordered_set<const LocalVar*> next;
            if (block_id && !cfg.predecessors[block_id].empty()) {
                next = out[cfg.predecessors[block_id].front()];
                for (size_t pred_index = 1;
                        pred_index < cfg.predecessors[block_id].size();
                        ++pred_index) {
                    if (!(pred_index % 100)
                            && qore_ir_analysis_cancelled(check_count,
                                "IR dense list predecessor intersection")) {
                        return {};
                    }
                    const auto& predecessor =
                        out[cfg.predecessors[block_id][pred_index]];
                    size_t local_count = 0;
                    for (auto it = next.begin(); it != next.end();) {
                        if (++local_count % 100 == 0
                                && qore_ir_analysis_cancelled(check_count,
                                    "IR dense list set intersection")) {
                            return {};
                        }
                        if (!predecessor.count(*it)) {
                            it = next.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
            }
            in[block_id] = next;
            if (!transfer(block_id, next)) {
                return {};
            }
            if (out[block_id] != next) {
                out[block_id] = std::move(next);
                changed = true;
            }
        }
    }

    for (size_t block_id = 0; block_id < func.blocks.size(); ++block_id) {
        std::unordered_set<const LocalVar*> known = in[block_id];
        for (const auto& inst_ptr : func.blocks[block_id]->instructions) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR dense list annotation")) {
                return stats;
            }
            const QoreIRInstruction* inst = inst_ptr.get();
            if (inst->opcode == QoreIROpcode::LoadLocal) {
                const auto* load =
                    static_cast<const QoreIRLocalInstruction*>(inst);
                if (load->local && known.count(load->local)
                        && inst->result.isValid()) {
                    QoreIRValueFacts facts;
                    if (const QoreIRValueFacts* current =
                            func.getValueFacts(inst->result)) {
                        facts = *current;
                    }
                    if (facts.list_density != QoreIRListDensity::Dense) {
                        facts.list_density = QoreIRListDensity::Dense;
                        func.setValueFacts(inst->result, facts);
                        ++stats.loads;
                    }
                }
            }
            if (!transfer_instruction(inst, known)) {
                return stats;
            }
        }
    }

    for (const auto& block : func.blocks) {
        for (const auto& inst_ptr : block->instructions) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR dense identity-map join elision")) {
                return stats;
            }
            QoreIRInstruction* inst = inst_ptr.get();
            QoreIRValue source;
            if (inst->opcode == QoreIROpcode::ListStringJoinIdentityMap
                    && inst->operands.size() >= 2) {
                source = inst->operands[1];
            } else if (inst->opcode == QoreIROpcode::Invoke) {
                auto* invoke = static_cast<QoreIRInvokeInstruction*>(inst);
                if (invoke->invoke_opcode
                            != QoreIROpcode::ListStringJoinIdentityMap
                        || invoke->operands.size() < 2) {
                    continue;
                }
                source = invoke->operands[1];
            } else {
                continue;
            }
            const QoreIRValueFacts* facts = func.getValueFacts(source);
            if (!facts
                    || facts->list_density != QoreIRListDensity::Dense) {
                continue;
            }
            if (inst->opcode == QoreIROpcode::Invoke) {
                static_cast<QoreIRInvokeInstruction*>(inst)->invoke_opcode =
                    QoreIROpcode::ListStringJoin;
            } else {
                inst->opcode = QoreIROpcode::ListStringJoin;
            }
            ++stats.joins;
        }
    }
    return stats;
}

void qore_ir_optimize(QoreIRFunction& func, QoreIROptimizationStats* stats,
        bool enable_licm) {
    QoreIROptimizationStats local_stats;
    if (getenv("QORE_DISABLE_IR_OPT")) {
        if (stats) {
            *stats = local_stats;
        }
        return;
    }
    size_t check_count = 0;
    local_stats.scalar_list_queries_folded =
        qore_ir_fold_scalar_list_queries(func, check_count);
    QoreIRControlFlowGraph cfg(func);
    if (cfg.cancelled) {
        if (stats) {
            *stats = local_stats;
        }
        return;
    }
    QoreIRFixedAggregateScalarizationStats aggregate_stats =
        qore_ir_scalar_replace_fixed_aggregates(func, cfg, check_count);
    local_stats.fixed_lists_scalarized = aggregate_stats.lists;
    local_stats.fixed_hashes_scalarized = aggregate_stats.hashes;
    local_stats.local_value_facts_refined =
        qore_ir_refine_local_value_facts(func, cfg, check_count);
    QoreIRDenseListStats dense_list_stats =
        qore_ir_refine_dense_list_facts(func, cfg, check_count);
    local_stats.dense_list_facts_refined = dense_list_stats.loads;
    local_stats.dense_identity_map_joins_elided = dense_list_stats.joins;
    QoreIRNativeLocalPromotionStats native_local_stats =
        qore_ir_promote_native_local_loads(func, cfg, check_count);
    local_stats.native_local_loads_promoted = native_local_stats.loads;
    local_stats.native_local_stores_eliminated = native_local_stats.stores;
    for (const auto& block : func.blocks) {
        if (qore_ir_analysis_cancelled(check_count, "IR typed foreach statistics")) {
            if (stats) {
                *stats = local_stats;
            }
            return;
        }
        if (block->name.rfind("foreach.typed.header.", 0) == 0) {
            ++local_stats.typed_foreach_loops;
        }
    }
    std::vector<QoreIRNaturalLoop> loops = qore_ir_find_natural_loops(cfg);
    bool enable_in_place_list_push =
        !getenv("QORE_DISABLE_IR_IN_PLACE_LIST_PUSH");
    bool enable_in_place_string_append =
        !getenv("QORE_DISABLE_IR_IN_PLACE_STRING_APPEND");
    bool has_list_push = false;
    bool has_string_append = false;
    if (enable_in_place_list_push || enable_in_place_string_append) {
        for (const QoreIRBasicBlock* block : cfg.blocks) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR in-place mutation discovery")) {
                if (stats) {
                    *stats = local_stats;
                }
                return;
            }
            for (const auto& inst : block->instructions) {
                if (qore_ir_analysis_cancelled(check_count,
                        "IR in-place mutation discovery")) {
                    if (stats) {
                        *stats = local_stats;
                    }
                    return;
                }
                if (enable_in_place_list_push
                        && inst->opcode == QoreIROpcode::ListPush) {
                    has_list_push = true;
                } else if (enable_in_place_string_append
                        && inst->opcode == QoreIROpcode::AppendStringCow) {
                    has_string_append = true;
                }
                if ((!enable_in_place_list_push || has_list_push)
                        && (!enable_in_place_string_append
                            || has_string_append)) {
                    break;
                }
            }
            if ((!enable_in_place_list_push || has_list_push)
                    && (!enable_in_place_string_append
                        || has_string_append)) {
                break;
            }
        }
    }
    QoreIRScalarUses uses;
    if ((!loops.empty() || has_list_push || has_string_append)
            && !qore_ir_collect_scalar_uses(func, uses, check_count)) {
        if (stats) {
            *stats = local_stats;
        }
        return;
    }
    if (!getenv("QORE_DISABLE_IR_BOUNDED_TYPED_LIST_READS")) {
        local_stats.bounded_typed_list_reads = qore_ir_specialize_bounded_typed_list_reads(
            func, cfg, loops, uses, local_stats.bounded_boxed_direct_reads, check_count);
    }
    if (!getenv("QORE_DISABLE_IR_BORROWED_LIST_READS")) {
        local_stats.borrowed_list_reads = qore_ir_mark_borrowed_list_reads(
            cfg, loops, uses, check_count);
    }
    if (enable_in_place_list_push) {
        local_stats.in_place_list_pushes = qore_ir_mark_in_place_list_pushes(
            func, cfg, uses, check_count, nullptr);
    }
    if (enable_in_place_string_append) {
        local_stats.in_place_string_appends =
            qore_ir_mark_in_place_string_appends(
                func, cfg, uses, check_count);
    }
    if (!enable_licm || getenv("QORE_DISABLE_IR_LICM")) {
        loops.clear();
    }
    local_stats.loops_analyzed = loops.size();

    for (const QoreIRNaturalLoop& loop : loops) {
        if (qore_ir_analysis_cancelled(check_count, "IR loop-invariant code motion")) {
            break;
        }
        std::unordered_set<size_t> loop_blocks(loop.blocks.begin(), loop.blocks.end());
        std::unordered_map<uint32_t, size_t> definition_blocks;
        std::unordered_map<uint32_t, QoreIRInstruction*> definitions;
        for (size_t block_id = 0; block_id < cfg.blocks.size(); ++block_id) {
            for (const auto& inst : cfg.blocks[block_id]->instructions) {
                if (qore_ir_analysis_cancelled(check_count, "IR loop-invariant code motion")) {
                    if (stats) {
                        *stats = local_stats;
                    }
                    return;
                }
                if (inst->result.isValid()) {
                    definition_blocks[inst->result.id] = block_id;
                    definitions[inst->result.id] = inst.get();
                }
            }
        }

        std::unordered_set<const LocalVar*> mutated;
        if (!qore_ir_collect_mutated_locals(loop, cfg, mutated, check_count)) {
            break;
        }
        bool loop_may_invalidate_loads = false;
        if (getenv("QORE_DISABLE_IR_ONLY_LICM_ACROSS_CALLS")) {
            for (size_t block_id : loop.blocks) {
                for (const auto& inst : cfg.blocks[block_id]->instructions) {
                    if (qore_ir_analysis_cancelled(check_count,
                            "IR loop invalidation analysis")) {
                        if (stats) {
                            *stats = local_stats;
                        }
                        return;
                    }
                    if (qore_ir_may_mutate_unknown_local(inst->opcode)) {
                        loop_may_invalidate_loads = true;
                        break;
                    }
                }
                if (loop_may_invalidate_loads) {
                    break;
                }
            }
        }
        std::unordered_set<uint32_t> safe_repeated_values;
        bool cancelled = false;
        for (size_t block_id : loop.blocks) {
            for (const auto& inst : cfg.blocks[block_id]->instructions) {
                if (qore_ir_analysis_cancelled(check_count, "IR loop use validation")) {
                    if (stats) {
                        *stats = local_stats;
                    }
                    return;
                }
                if (inst->result.isValid()
                        && qore_ir_has_only_nonconsuming_scalar_uses(func, inst->result, uses, false,
                            check_count, cancelled, &loop_blocks)) {
                    safe_repeated_values.insert(inst->result.id);
                }
                if (cancelled) {
                    if (stats) {
                        *stats = local_stats;
                    }
                    return;
                }
            }
        }
        std::unordered_set<const QoreIRInstruction*> selected;
        std::vector<QoreIRInstruction*> order;
        bool changed = true;
        while (changed) {
            changed = false;
            for (size_t block_id : loop.blocks) {
                for (const auto& inst_ptr : cfg.blocks[block_id]->instructions) {
                    if (qore_ir_analysis_cancelled(check_count, "IR loop-invariant code motion")) {
                        if (stats) {
                            *stats = local_stats;
                        }
                        return;
                    }
                    QoreIRInstruction* inst = inst_ptr.get();
                    if (selected.count(inst) || !inst->result.isValid() || inst->exception_target) {
                        continue;
                    }
                    bool candidate = qore_ir_is_native_scalar_constant(inst->opcode)
                        || qore_ir_is_hoistable_load(func, *inst, mutated)
                        || qore_ir_is_native_scalar_pure_opcode(inst->opcode);
                    if (!candidate
                            || (inst->opcode == QoreIROpcode::LoadLocal && loop_may_invalidate_loads)
                            || !safe_repeated_values.count(inst->result.id)) {
                        continue;
                    }
                    bool operands_invariant = true;
                    if (!qore_ir_visit_value_operands(*inst, [&](QoreIRValue operand) {
                        auto def = definition_blocks.find(operand.id);
                        if (def == definition_blocks.end() || !loop_blocks.count(def->second)) {
                            return;
                        }
                        auto defining = definitions.find(operand.id);
                        QoreIRInstruction* defining_inst = defining == definitions.end() ? nullptr : defining->second;
                        if (!defining_inst || !selected.count(defining_inst)) {
                            operands_invariant = false;
                        }
                    }, &check_count, "IR loop-invariant operand analysis")) {
                        if (stats) {
                            *stats = local_stats;
                        }
                        return;
                    }
                    if (!operands_invariant) {
                        continue;
                    }
                    selected.insert(inst);
                    order.push_back(inst);
                    changed = true;
                }
            }
        }
        if (order.empty()) {
            continue;
        }

        std::unordered_map<QoreIRInstruction*, std::unique_ptr<QoreIRInstruction>> moved;
        for (size_t block_id : loop.blocks) {
            auto& instructions = cfg.blocks[block_id]->instructions;
            for (auto it = instructions.begin(); it != instructions.end();) {
                if (selected.count(it->get())) {
                    QoreIRInstruction* raw = it->get();
                    moved.emplace(raw, std::move(*it));
                    it = instructions.erase(it);
                } else {
                    ++it;
                }
            }
        }
        auto& preheader_instructions = cfg.blocks[loop.preheader]->instructions;
        auto insert_at = preheader_instructions.end() - 1;
        for (QoreIRInstruction* inst : order) {
            auto moved_it = moved.find(inst);
            if (moved_it == moved.end()) {
                continue;
            }
            insert_at = preheader_instructions.insert(insert_at, std::move(moved_it->second));
            ++insert_at;
            ++local_stats.instructions_hoisted;
        }
    }

    if (!getenv("QORE_DISABLE_IR_CSE")) {
        QoreIRScalarCSEStats cse_stats = qore_ir_eliminate_common_scalar_expressions(func, cfg);
        local_stats.scalar_loads_forwarded = cse_stats.loads_forwarded;
        local_stats.scalar_expressions_eliminated = cse_stats.expressions_eliminated;
    }

    if (!getenv("QORE_DISABLE_IR_CONST_FOLD")) {
        local_stats.constant_branches_folded = qore_ir_fold_constant_branches(func);
    }

    if (stats) {
        *stats = local_stats;
    }
}

size_t qore_ir_optimize_fresh_list_calls(QoreIRFunction& func,
        const QoreIRParamNoEscapeQuery& param_noescape) {
    size_t check_count = 0;
    QoreIRControlFlowGraph cfg(func);
    if (cfg.cancelled) {
        return 0;
    }
    QoreIRScalarUses uses;
    if (!qore_ir_collect_scalar_uses(func, uses, check_count)) {
        return 0;
    }
    return qore_ir_mark_in_place_list_pushes(func, cfg, uses, check_count, &param_noescape);
}

size_t qore_ir_fold_fresh_list_size_calls(QoreIRFunction& func,
        const QoreIRFreshListSizeQuery& is_list_size) {
    if (!is_list_size) {
        return 0;
    }
    size_t check_count = 0;
    std::unordered_map<uint32_t, QoreIRInstruction*> definitions;
    for (const auto& block : func.blocks) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR fresh list-size call definition analysis")) {
            return 0;
        }
        for (const auto& inst : block->instructions) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR fresh list-size call definition analysis")) {
                return 0;
            }
            if (inst->result.isValid()) {
                definitions.emplace(inst->result.id, inst.get());
            }
        }
    }

    QoreIRScalarUses uses;
    if (!qore_ir_collect_scalar_uses(func, uses, check_count)) {
        return 0;
    }
    struct Fold {
        QoreIRInstruction* make = nullptr;
        QoreIRInstruction* call = nullptr;
        int64_t size = 0;
    };
    std::vector<Fold> folds;
    for (const auto& [result_id, definition] : definitions) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR fresh list-size call candidate analysis")) {
            return 0;
        }
        if (!definition || definition->opcode != QoreIROpcode::MakeList
                || definition->exception_target
                || definition->operands.size() > 100) {
            continue;
        }
        const auto* make =
            static_cast<const QoreIRMakeListInstruction*>(definition);
        QoreIRValueRepresentation expected =
            QoreIRValueRepresentation::Unknown;
        if (make->typeInfo) {
            const QoreTypeInfo* element_type =
                QoreTypeInfo::getUniqueReturnComplexList(make->typeInfo);
            if (element_type == bigIntTypeInfo) {
                expected = QoreIRValueRepresentation::NativeInt;
            } else if (element_type == floatTypeInfo) {
                expected = QoreIRValueRepresentation::NativeFloat;
            } else if (element_type == boolTypeInfo) {
                expected = QoreIRValueRepresentation::NativeBool;
            } else if (element_type && element_type != autoTypeInfo
                    && element_type != anyTypeInfo) {
                continue;
            }
        }
        bool safe_operands = true;
        for (QoreIRValue operand : definition->operands) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR fresh list-size call operand analysis")) {
                return 0;
            }
            const QoreIRValueFacts* facts = func.getValueFacts(operand);
            if (!facts
                    || facts->assigned_state
                        != QoreIRAssignedState::Assigned
                    || !facts->never_nothing
                    || (facts->representation
                            != QoreIRValueRepresentation::NativeInt
                        && facts->representation
                            != QoreIRValueRepresentation::NativeFloat
                        && facts->representation
                            != QoreIRValueRepresentation::NativeBool)
                    || (expected != QoreIRValueRepresentation::Unknown
                        && facts->representation != expected)) {
                safe_operands = false;
                break;
            }
        }
        if (!safe_operands) {
            continue;
        }

        auto use = uses.find(result_id);
        if (use == uses.end() || use->second.size() != 1
                || !use->second.front().inst) {
            continue;
        }
        QoreIRInstruction* call =
            const_cast<QoreIRInstruction*>(use->second.front().inst);
        if (call->opcode != QoreIROpcode::CallDirect
                && call->opcode != QoreIROpcode::CallStaticDirect) {
            continue;
        }
        if (call->exception_target || !call->result.isValid()
                || call->operands.size() != 1
                || call->operands[0].id != result_id) {
            continue;
        }
        bool has_ref_args = true;
        const AbstractQoreFunctionVariant* callee =
            qore_ir_get_resolved_effect_callee(call, has_ref_args);
        if (!callee || has_ref_args || !is_list_size(callee, 0)) {
            continue;
        }
        folds.push_back({definition, call,
            static_cast<int64_t>(definition->operands.size())});
    }
    if (folds.empty()) {
        return 0;
    }

    std::unordered_map<QoreIRInstruction*, int64_t> replacements;
    std::unordered_set<QoreIRInstruction*> eliminated;
    for (const Fold& fold : folds) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR fresh list-size call rewrite preparation")) {
            return 0;
        }
        replacements.emplace(fold.call, fold.size);
        eliminated.insert(fold.make);
    }
    for (const auto& block : func.blocks) {
        auto& instructions = block->instructions;
        for (auto inst = instructions.begin(); inst != instructions.end();) {
            if (++check_count % 100 == 0) {
                (void)qore_check_cancel(nullptr,
                    "IR fresh list-size call folding");
            }
            if (eliminated.count(inst->get())) {
                inst = instructions.erase(inst);
                continue;
            }
            auto replacement = replacements.find(inst->get());
            if (replacement == replacements.end()) {
                ++inst;
                continue;
            }
            auto constant = std::make_unique<QoreIRConstInstruction>();
            constant->opcode = QoreIROpcode::ConstInt;
            constant->loc = (*inst)->loc;
            constant->result = (*inst)->result;
            constant->constant.kind = QoreIRConstant::Kind::Int;
            constant->constant.int_value = replacement->second;
            QoreIRValueFacts facts;
            facts.type_info = bigIntTypeInfo;
            facts.assigned_state = QoreIRAssignedState::Assigned;
            facts.representation = QoreIRValueRepresentation::NativeInt;
            facts.never_nothing = true;
            func.setValueFacts(constant->result, facts);
            *inst = std::move(constant);
            ++inst;
        }
    }
    return folds.size();
}

size_t qore_ir_fold_fresh_hash_key_calls(QoreIRFunction& func,
        const QoreIRFreshHashKeyQuery& get_hash_key) {
    if (!get_hash_key) {
        return 0;
    }
    size_t check_count = 0;
    std::unordered_map<uint32_t, QoreIRInstruction*> definitions;
    for (const auto& block : func.blocks) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR fresh hash-key call definition analysis")) {
            return 0;
        }
        for (const auto& inst : block->instructions) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR fresh hash-key call definition analysis")) {
                return 0;
            }
            if (inst->result.isValid()) {
                definitions.emplace(inst->result.id, inst.get());
            }
        }
    }

    QoreIRScalarUses uses;
    if (!qore_ir_collect_scalar_uses(func, uses, check_count)) {
        return 0;
    }
    struct Fold {
        QoreIRInstruction* make = nullptr;
        QoreIRInstruction* call = nullptr;
        QoreIRValue replacement;
    };
    std::vector<Fold> folds;
    for (const auto& [result_id, definition] : definitions) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR fresh hash-key call candidate analysis")) {
            return 0;
        }
        if (!definition
                || (definition->opcode != QoreIROpcode::MakeHashConstKeys
                    && definition->opcode != QoreIROpcode::MakeHash)
                || definition->exception_target
                || definition->operands.size() > 200) {
            continue;
        }
        std::vector<std::string> keys;
        std::vector<QoreIRValue> values;
        if (definition->opcode == QoreIROpcode::MakeHashConstKeys) {
            const auto* make =
                static_cast<const QoreIRMakeHashConstKeysInstruction*>(
                    definition);
            if (make->keys.size() != definition->operands.size()) {
                continue;
            }
            keys = make->keys;
            values = definition->operands;
        } else {
            if (definition->operands.size() % 2) {
                continue;
            }
            bool constant_keys = true;
            keys.reserve(definition->operands.size() / 2);
            values.reserve(definition->operands.size() / 2);
            for (size_t i = 0; i < definition->operands.size(); i += 2) {
                if (qore_ir_analysis_cancelled(check_count,
                        "IR fresh hash-key call literal key analysis")) {
                    return 0;
                }
                auto key = definitions.find(definition->operands[i].id);
                if (key == definitions.end()
                        || key->second->opcode != QoreIROpcode::ConstString) {
                    constant_keys = false;
                    break;
                }
                const auto* constant =
                    static_cast<const QoreIRConstInstruction*>(key->second);
                if (constant->constant.kind
                        != QoreIRConstant::Kind::String) {
                    constant_keys = false;
                    break;
                }
                keys.push_back(constant->constant.string_value);
                values.push_back(definition->operands[i + 1]);
            }
            if (!constant_keys) {
                continue;
            }
        }
        bool safe_operands = true;
        for (QoreIRValue operand : values) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR fresh hash-key call operand analysis")) {
                return 0;
            }
            const QoreIRValueFacts* facts = func.getValueFacts(operand);
            if (!facts
                    || facts->assigned_state
                        != QoreIRAssignedState::Assigned
                    || !facts->never_nothing
                    || facts->representation
                        != QoreIRValueRepresentation::NativeInt) {
                safe_operands = false;
                break;
            }
        }
        if (!safe_operands) {
            continue;
        }

        auto use = uses.find(result_id);
        if (use == uses.end() || use->second.size() != 1
                || !use->second.front().inst) {
            continue;
        }
        QoreIRInstruction* call =
            const_cast<QoreIRInstruction*>(use->second.front().inst);
        if ((call->opcode != QoreIROpcode::CallDirect
                    && call->opcode != QoreIROpcode::CallStaticDirect)
                || call->exception_target || !call->result.isValid()
                || call->operands.size() != 1
                || call->operands[0].id != result_id) {
            continue;
        }
        bool has_ref_args = true;
        const AbstractQoreFunctionVariant* callee =
            qore_ir_get_resolved_effect_callee(call, has_ref_args);
        std::string key;
        if (!callee || has_ref_args || !get_hash_key(callee, 0, key)
                || key.empty()) {
            continue;
        }
        QoreIRValue replacement;
        for (size_t i = keys.size(); i > 0; --i) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR fresh hash-key call key analysis")) {
                return 0;
            }
            if (keys[i - 1] == key) {
                replacement = values[i - 1];
                break;
            }
        }
        if (!replacement.isValid()) {
            continue;
        }
        folds.push_back({definition, call, replacement});
    }
    if (folds.empty()) {
        return 0;
    }

    std::unordered_map<uint32_t, QoreIRValue> replacements;
    std::unordered_set<const QoreIRInstruction*> eliminated;
    for (const Fold& fold : folds) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR fresh hash-key call rewrite preparation")) {
            return 0;
        }
        replacements.emplace(fold.call->result.id, fold.replacement);
        eliminated.insert(fold.make);
        eliminated.insert(fold.call);
    }
    for (auto& [result_id, replacement] : replacements) {
        std::unordered_set<uint32_t> seen{result_id};
        while (true) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR fresh hash-key call replacement normalization")) {
                return 0;
            }
            auto next = replacements.find(replacement.id);
            if (next == replacements.end()) {
                break;
            }
            if (!seen.insert(replacement.id).second) {
                return 0;
            }
            replacement = next->second;
        }
    }
    for (const auto& block : func.blocks) {
        auto& instructions = block->instructions;
        for (auto inst = instructions.begin(); inst != instructions.end();) {
            if (++check_count % 100 == 0) {
                (void)qore_check_cancel(nullptr,
                    "IR fresh hash-key call folding");
            }
            if (eliminated.count(inst->get())) {
                inst = instructions.erase(inst);
                continue;
            }
            (void)qore_ir_rewrite_value_operands(
                **inst, replacements, check_count, false);
            ++inst;
        }
    }
    return folds.size();
}

size_t qore_ir_fuse_aggregate_return_projections(QoreIRFunction& func,
        const QoreIRAggregateProjectionQuery& get_projection,
        const QoreIRAggregateConsumerQuery& get_consumer) {
    if (!get_projection) {
        return 0;
    }
    size_t check_count = 0;
    struct LocalOperation {
        size_t block_id = 0;
        size_t offset = 0;
        QoreIRLocalInstruction* instruction = nullptr;
    };
    std::unordered_map<uint32_t, const QoreIRInstruction*> definitions;
    std::unordered_map<const QoreIRInstruction*,
        std::pair<size_t, size_t>> instruction_positions;
    std::unordered_map<LocalVar*, std::vector<LocalOperation>> local_operations;
    std::unordered_set<const LocalVar*> written_locals;
    for (size_t block_id = 0; block_id < func.blocks.size(); ++block_id) {
        const auto& block = func.blocks[block_id];
        for (size_t offset = 0; offset < block->instructions.size(); ++offset) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR aggregate-return projection definition analysis")) {
                return 0;
            }
            const auto& inst = block->instructions[offset];
            instruction_positions.emplace(inst.get(),
                std::make_pair(block_id, offset));
            if (inst->result.isValid()) {
                definitions.emplace(inst->result.id, inst.get());
            }
            if (inst->opcode != QoreIROpcode::InstantiateLocal
                    && inst->opcode != QoreIROpcode::UninstantiateLocal) {
                if (const LocalVar* written =
                        qore_ir_get_written_local(inst.get())) {
                    written_locals.insert(written);
                }
            }
            if (inst->opcode == QoreIROpcode::LoadLocal
                    || inst->opcode == QoreIROpcode::StoreLocal
                    || inst->opcode == QoreIROpcode::InstantiateLocal
                    || inst->opcode == QoreIROpcode::UninstantiateLocal) {
                auto* local_inst =
                    static_cast<QoreIRLocalInstruction*>(inst.get());
                local_operations[local_inst->local].push_back(
                    {block_id, offset, local_inst});
            }
        }
    }
    std::unordered_set<const LocalVar*> immutable_parameters;
    for (const auto& [index, local] : func.param_local_vars) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR aggregate-return immutable parameter analysis")) {
            return 0;
        }
        if (local && !written_locals.count(local)) {
            immutable_parameters.insert(local);
        }
    }
    QoreIRScalarUses uses;
    if (!qore_ir_collect_scalar_uses(func, uses, check_count)) {
        return 0;
    }
    QoreIRControlFlowGraph cfg(func);
    if (cfg.cancelled) {
        return 0;
    }

    struct Projection {
        QoreIRInstruction* call = nullptr;
        QoreIRInstruction* consumer = nullptr;
        QoreIRCallDirectInstruction::AOTAggregateProjectionKind kind =
            QoreIRCallDirectInstruction::AOTAggregateProjectionKind::None;
        QoreIRValue result;
        int16_t operand = -1;
        int64_t size = 0;
        int64_t int_constant = 0;
        double float_constant = 0.0;
        QoreIRValue guarded_index;
        bool guarded_hash_key = false;
        bool clone_guarded_index = false;
        bool negative_offsets = false;
        std::vector<QoreIRCallDirectInstruction::
            AOTAggregateProjectionDescriptor> guarded_descriptors;
        std::vector<std::string> guarded_keys;
        std::vector<QoreIRInstruction*> eliminated;
        std::vector<std::pair<uint32_t, QoreIRValue>> replacements;
    };
    auto analyze_projection = [&](QoreIRInstruction* call,
            QoreIRValue base, QoreIRInstruction* consumer,
            Projection& projection,
            bool allow_cross_block_parameter_guard = false) {
        if (!call || !consumer || consumer->exception_target
                || !consumer->result.isValid() || consumer->operands.empty()
                || !base.isValid()) {
            return false;
        }
        bool direct_call = call->opcode == QoreIROpcode::CallDirect;
        bool static_call = call->opcode == QoreIROpcode::CallStaticDirect;
        bool exact_method_call =
            call->opcode == QoreIROpcode::CallMethodDirect
            && qore_ir_is_non_overridable_method_call(*call);
        bool exact_object_method_call =
            call->opcode == QoreIROpcode::DotEvalMethodDirect
            && qore_ir_is_non_overridable_method_call(*call);
        if (!direct_call && !static_call && !exact_method_call
                && !exact_object_method_call) {
            return false;
        }
        if (exact_object_method_call) {
            if (call->operands.empty()) {
                return false;
            }
            const auto* object_call =
                static_cast<const QoreIRDotEvalMethodDirectInstruction*>(
                    call);
            const QoreIRValueFacts* base_facts =
                func.getValueFacts(call->operands[0]);
            if (!base_facts
                    || base_facts->assigned_state
                        != QoreIRAssignedState::Assigned
                    || !base_facts->never_nothing
                    || base_facts->representation
                        != QoreIRValueRepresentation::Boxed
                    || QoreTypeInfo::getUniqueReturnClass(
                        base_facts->type_info) != object_call->qc) {
                return false;
            }
        }
        QoreIRAggregateProjectionQueryKind query_kind;
        QoreIRCallDirectInstruction::AOTAggregateProjectionKind
            projection_kind =
                QoreIRCallDirectInstruction::AOTAggregateProjectionKind::None;
        int64_t index = 0;
        std::string key;
        QoreIRValue guarded_index;
        bool guarded_hash_key = false;
        bool clone_guarded_index = false;
        bool negative_offsets = false;
        size_t base_operand = 0;
        if (consumer->opcode == QoreIROpcode::ListSize
                && consumer->operands.size() == 1) {
            query_kind = QoreIRAggregateProjectionQueryKind::ListSize;
        } else if ((consumer->opcode == QoreIROpcode::ElementsAny
                        || consumer->opcode == QoreIROpcode::ElementsInt)
                && consumer->operands.size() == 1) {
            query_kind =
                QoreIRAggregateProjectionQueryKind::AggregateSizeValue;
        } else if ((consumer->opcode == QoreIROpcode::ExistsAny
                        || consumer->opcode == QoreIROpcode::ExistsBool)
                && consumer->operands.size() == 1) {
            query_kind =
                QoreIRAggregateProjectionQueryKind::AggregateExistsValue;
        } else if ((consumer->opcode == QoreIROpcode::ListGetInt
                        || consumer->opcode == QoreIROpcode::ListGetFloat
                        || consumer->opcode == QoreIROpcode::ListIndexDynamic)
                && consumer->operands.size() == 2) {
            auto index_definition = definitions.find(
                consumer->operands[1].id);
            if (index_definition != definitions.end()
                    && index_definition->second->opcode
                        == QoreIROpcode::ConstInt) {
                const auto* constant =
                    static_cast<const QoreIRConstInstruction*>(
                        index_definition->second);
                if (constant->constant.kind
                        != QoreIRConstant::Kind::Int) {
                    return false;
                }
                index = constant->constant.int_value;
                query_kind = consumer->opcode == QoreIROpcode::ListGetInt
                    ? QoreIRAggregateProjectionQueryKind::ListIndexInt
                    : consumer->opcode == QoreIROpcode::ListGetFloat
                        ? QoreIRAggregateProjectionQueryKind::ListIndexFloat
                        : QoreIRAggregateProjectionQueryKind::ListIndexValue;
            } else {
                if (consumer->opcode != QoreIROpcode::ListIndexDynamic) {
                    return false;
                }
                const auto* dynamic = static_cast<
                    const QoreIRExprInstruction*>(consumer);
                const QoreIRValueFacts* index_facts =
                    func.getValueFacts(consumer->operands[1]);
                if (!dynamic->list_selector_kinds.empty() || !index_facts
                        || index_facts->assigned_state
                            != QoreIRAssignedState::Assigned
                        || !index_facts->never_nothing
                        || index_facts->representation
                            != QoreIRValueRepresentation::NativeInt) {
                    return false;
                }
                guarded_index = consumer->operands[1];
                if (dynamic->expr.hasNode()) {
                    const auto* square = dynamic_cast<
                        const QoreSquareBracketsOperatorNode*>(
                            dynamic->expr.getInternalNode());
                    negative_offsets =
                        square && square->hasNegativeOffsets();
                }
                query_kind = QoreIRAggregateProjectionQueryKind::
                    ListIndexDynamicValue;
            }
        } else if (consumer->opcode
                        == QoreIROpcode::DotEvalMethodDirect
                && consumer->operands.size() == 1) {
            const auto* direct = static_cast<
                const QoreIRDotEvalMethodDirectInstruction*>(consumer);
            if (!direct->pseudo || !direct->qc) {
                return false;
            }
            bool list_pseudo =
                !strcmp(direct->qc->getName(), "<list>");
            bool hash_pseudo =
                !strcmp(direct->qc->getName(), "<hash>");
            if (!list_pseudo && !hash_pseudo) {
                return false;
            }
            if (hash_pseudo) {
                if (!direct->method || !direct->method->getName()) {
                    return false;
                }
                key = direct->method->getName();
            }
            if (list_pseudo
                    && direct->intrinsic == QoreIRIntrinsic::ListFirst) {
                query_kind =
                    QoreIRAggregateProjectionQueryKind::ListIndexValue;
            } else if (list_pseudo && direct->intrinsic
                    == QoreIRIntrinsic::ListLast) {
                index = -1;
                query_kind =
                    QoreIRAggregateProjectionQueryKind::ListIndexValue;
            } else if (hash_pseudo
                    && direct->intrinsic == QoreIRIntrinsic::Size) {
                query_kind =
                    QoreIRAggregateProjectionQueryKind::
                        AggregateSizeValue;
            } else if (direct->intrinsic == QoreIRIntrinsic::Empty) {
                query_kind =
                    QoreIRAggregateProjectionQueryKind::
                        AggregateEmptyValue;
            } else if (direct->intrinsic == QoreIRIntrinsic::Val) {
                query_kind =
                    QoreIRAggregateProjectionQueryKind::
                        AggregateValValue;
            } else {
                return false;
            }
        } else if (consumer->opcode == QoreIROpcode::HashKeyAccessInt
                && consumer->operands.size() == 1) {
            const auto* access =
                static_cast<const QoreIRHashKeyAccessInstruction*>(consumer);
            if (access->key_name.empty()) {
                return false;
            }
            query_kind = QoreIRAggregateProjectionQueryKind::HashKeyInt;
            key = access->key_name;
        } else if ((consumer->opcode == QoreIROpcode::HashKeyAccess
                        || consumer->opcode
                            == QoreIROpcode::HashKeyAccessHash
                        || consumer->opcode
                            == QoreIROpcode::HashKeyAccessHashGuarded)
                && consumer->operands.size() == 1) {
            const auto* access =
                static_cast<const QoreIRHashKeyAccessInstruction*>(consumer);
            if (access->key_name.empty()) {
                return false;
            }
            query_kind = QoreIRAggregateProjectionQueryKind::HashKeyValue;
            key = access->key_name;
        } else if (consumer->opcode == QoreIROpcode::HashDerefDynamic
                && consumer->operands.size() == 2) {
            const QoreIRValueFacts* key_facts =
                func.getValueFacts(consumer->operands[1]);
            if (!key_facts
                    || key_facts->assigned_state
                        != QoreIRAssignedState::Assigned
                    || !key_facts->never_nothing
                    || key_facts->representation
                        != QoreIRValueRepresentation::Boxed
                    || QoreTypeInfo::parseReturns(
                        key_facts->type_info, NT_STRING) != QTI_IDENT) {
                return false;
            }
            guarded_index = consumer->operands[1];
            guarded_hash_key = true;
            query_kind = QoreIRAggregateProjectionQueryKind::
                HashKeyDynamicValue;
        } else if (get_consumer
                && consumer->opcode == QoreIROpcode::CallDirect) {
            bool consumer_has_ref_args = true;
            const AbstractQoreFunctionVariant* consumer_callee =
                qore_ir_get_resolved_effect_callee(
                    consumer, consumer_has_ref_args);
            if (!consumer_callee || consumer_has_ref_args
                    || !get_consumer(consumer_callee, consumer,
                        base_operand, query_kind, index, key)) {
                return false;
            }
        } else {
            return false;
        }
        if (base_operand >= consumer->operands.size()
                || consumer->operands[base_operand].id != base.id) {
            return false;
        }
        if (guarded_index.isValid()) {
            auto call_position = instruction_positions.find(call);
            auto consumer_position =
                instruction_positions.find(consumer);
            if (call_position == instruction_positions.end()
                    || consumer_position == instruction_positions.end()) {
                return false;
            }
            bool same_block = call_position->second.first
                    == consumer_position->second.first
                && call_position->second.second
                    < consumer_position->second.second;
            if (!same_block) {
                auto selector_definition =
                    definitions.find(guarded_index.id);
                const QoreIRLocalInstruction* selector_load =
                    selector_definition != definitions.end()
                        && selector_definition->second->opcode
                            == QoreIROpcode::LoadLocal
                    ? static_cast<const QoreIRLocalInstruction*>(
                        selector_definition->second)
                    : nullptr;
                if (!allow_cross_block_parameter_guard
                        || guarded_hash_key
                        || !selector_load
                        || selector_load->is_ref
                        || selector_load->is_closure
                        || !immutable_parameters.count(
                            selector_load->local)) {
                    return false;
                }
                clone_guarded_index = true;
            }
        }

        bool has_ref_args = true;
        const AbstractQoreFunctionVariant* callee = nullptr;
        if (exact_object_method_call) {
            const auto* object_call =
                static_cast<const QoreIRDotEvalMethodDirectInstruction*>(
                    call);
            has_ref_args = object_call->has_ref_args;
            callee = object_call->variant;
        } else {
            callee = qore_ir_get_resolved_effect_callee(
                call, has_ref_args);
        }
        int16_t operand = -1;
        int64_t size = 0;
        int64_t int_constant = 0;
        double float_constant = 0.0;
        std::vector<QoreIRCallDirectInstruction::
            AOTAggregateProjectionDescriptor> guarded_descriptors;
        std::vector<std::string> guarded_keys;
        if (!callee || has_ref_args
                || !get_projection(callee, call, query_kind, index, key,
                    operand, size, int_constant, float_constant,
                    projection_kind, guarded_descriptors, guarded_keys)
                || projection_kind
                    == QoreIRCallDirectInstruction::AOTAggregateProjectionKind::None) {
            return false;
        }
        if (exact_object_method_call
                && projection_kind
                    != QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::NativeInt
                && projection_kind
                    != QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::NativeFloat) {
            return false;
        }
        auto validate_descriptor = [&](QoreIRCallDirectInstruction::
                AOTAggregateProjectionKind kind, int16_t value_operand,
                int64_t descriptor_int) {
            bool descriptor_constant = kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::NativeIntConstant
                || kind == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::NativeFloatConstant
                || kind == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedIntConstant
                || kind == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedFloatConstant
                || kind == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedBoolConstant
                || kind == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedNothingConstant
                || kind == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::Size;
            if (descriptor_constant) {
                return true;
            }
            if (value_operand < 0
                    || static_cast<size_t>(value_operand)
                        >= call->operands.size()) {
                return false;
            }
            const QoreIRValueFacts* facts =
                func.getValueFacts(call->operands[value_operand]);
            QoreIRValueRepresentation expected = kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedValue
                ? QoreIRValueRepresentation::Boxed
                : kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::NativeInt
                        || kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::
                                    NativeIntAddConstant
                        || kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::
                                    NativeIntBinary
                        || kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::
                                    NativeIntMulConstant
                        || kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::
                                    NativeIntSelect
                        || kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::BoxedInt
                        || kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::
                                    BoxedIntAddConstant
                        || kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::
                                    BoxedIntBinary
                        || kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::
                                    BoxedIntMulConstant
                        || kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::
                                    BoxedBoolIntCompare
                        || kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::
                                    BoxedIntSelect
                    ? QoreIRValueRepresentation::NativeInt
                    : kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::BoxedBool
                        || kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::
                                    BoxedBoolSelect
                        ? QoreIRValueRepresentation::NativeBool
                        : QoreIRValueRepresentation::NativeFloat;
            bool valid = facts
                    && facts->assigned_state
                        == QoreIRAssignedState::Assigned
                    && facts->never_nothing
                    && facts->representation == expected;
            bool selected = kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::NativeIntSelect
                || kind == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::NativeFloatSelect
                || kind == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedIntSelect
                || kind == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedFloatSelect
                || kind == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedBoolSelect;
            if (!valid || !selected) {
                bool binary = kind
                        == QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::NativeIntBinary
                    || kind == QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::BoxedIntBinary
                    || kind == QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::BoxedBoolIntCompare;
                if (!valid || !binary) {
                    return valid;
                }
                uint64_t packed = static_cast<uint64_t>(descriptor_int);
                size_t rhs = static_cast<uint8_t>(packed);
                uint8_t operation = static_cast<uint8_t>(packed >> 8);
                if (packed > UINT16_MAX || rhs >= call->operands.size()
                        || operation > (kind
                                == QoreIRCallDirectInstruction::
                                    AOTAggregateProjectionKind::
                                        BoxedBoolIntCompare
                            ? 5 : 2)) {
                    return false;
                }
                const QoreIRValueFacts* rhs_facts =
                    func.getValueFacts(call->operands[rhs]);
                return rhs_facts
                    && rhs_facts->assigned_state
                        == QoreIRAssignedState::Assigned
                    && rhs_facts->never_nothing
                    && rhs_facts->representation
                        == QoreIRValueRepresentation::NativeInt;
            }
            size_t condition =
                static_cast<uint8_t>(descriptor_int);
            size_t alternate =
                static_cast<uint8_t>(descriptor_int >> 8);
            if (condition >= call->operands.size()
                    || alternate >= call->operands.size()) {
                return false;
            }
            const QoreIRValueFacts* condition_facts =
                func.getValueFacts(call->operands[condition]);
            const QoreIRValueFacts* alternate_facts =
                func.getValueFacts(call->operands[alternate]);
            return condition_facts
                    && condition_facts->assigned_state
                        == QoreIRAssignedState::Assigned
                    && condition_facts->never_nothing
                    && condition_facts->representation
                        == QoreIRValueRepresentation::NativeBool
                    && alternate_facts
                    && alternate_facts->assigned_state
                        == QoreIRAssignedState::Assigned
                    && alternate_facts->never_nothing
                    && alternate_facts->representation == expected;
        };
        if (!validate_descriptor(
                projection_kind, operand, int_constant)) {
            return false;
        }
        for (const auto& descriptor : guarded_descriptors) {
            if (!validate_descriptor(descriptor.kind,
                    descriptor.operand, descriptor.int_constant)) {
                return false;
            }
            if (descriptor.kind
                        != QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::BoxedInt
                    && descriptor.kind
                        != QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::BoxedFloat
                    && descriptor.kind
                        != QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::BoxedBool
                    && descriptor.kind
                        != QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::BoxedIntConstant
                    && descriptor.kind
                        != QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::BoxedFloatConstant
                    && descriptor.kind
                        != QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::BoxedBoolConstant
                    && descriptor.kind
                        != QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::
                                BoxedNothingConstant
                    && descriptor.kind
                        != QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::BoxedIntAddConstant
                    && descriptor.kind
                        != QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::
                                BoxedFloatAddConstant) {
                return false;
            }
        }
        if (!guarded_descriptors.empty()
                && (!guarded_index.isValid()
                    || size != static_cast<int64_t>(
                        guarded_descriptors.size()))) {
            return false;
        }
        if (guarded_hash_key
                && (guarded_keys.size() != guarded_descriptors.size()
                    || size != static_cast<int64_t>(
                        guarded_keys.size()))) {
            return false;
        }
        projection.call = call;
        projection.consumer = consumer;
        projection.kind = projection_kind;
        projection.result = consumer->result;
        projection.operand = operand;
        projection.size = size;
        projection.int_constant = int_constant;
        projection.float_constant = float_constant;
        projection.guarded_index = guarded_index;
        projection.guarded_hash_key = guarded_hash_key;
        projection.clone_guarded_index = clone_guarded_index;
        projection.negative_offsets = negative_offsets;
        projection.guarded_descriptors = std::move(guarded_descriptors);
        projection.guarded_keys = std::move(guarded_keys);
        return true;
    };

    struct VirtualizedCall {
        QoreIRInstruction* call = nullptr;
        std::vector<QoreIRInstruction*> eliminated;
        std::vector<std::pair<uint32_t, QoreIRValue>> replacements;
        std::vector<Projection> materialized_projections;
    };
    struct VirtualizedPhi {
        QoreIRPhiInstruction* phi = nullptr;
        std::unique_ptr<QoreIRPhiInstruction> scalar_phi;
        std::vector<QoreIRInstruction*> eliminated;
        std::vector<Projection> projected_calls;
    };
    std::vector<Projection> projections;
    std::vector<VirtualizedCall> virtualized;
    auto add_virtualized_projection = [](VirtualizedCall& candidate,
            const Projection& projection) {
        if (projection.guarded_index.isValid()) {
            return false;
        }
        if (projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::NativeInt
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::NativeFloat) {
            candidate.eliminated.push_back(projection.consumer);
            candidate.replacements.emplace_back(
                projection.result.id,
                projection.call->operands[projection.operand]);
            return true;
        }
        if (projection.kind
                == QoreIRCallDirectInstruction::
                    AOTAggregateProjectionKind::BoxedValue
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedInt
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedFloat
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedBool
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::NativeIntConstant
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::NativeFloatConstant
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedIntConstant
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedFloatConstant
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedBoolConstant
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedNothingConstant
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::Size) {
            candidate.materialized_projections.push_back(projection);
            return true;
        }
        return false;
    };
    auto same_guarded_index = [&](QoreIRValue lhs, QoreIRValue rhs) {
        if (lhs.id == rhs.id) {
            return true;
        }
        auto left_definition = definitions.find(lhs.id);
        auto right_definition = definitions.find(rhs.id);
        if (left_definition == definitions.end()
                || right_definition == definitions.end()
                || left_definition->second->opcode
                    != QoreIROpcode::LoadLocal
                || right_definition->second->opcode
                    != QoreIROpcode::LoadLocal) {
            return false;
        }
        const auto* left_load = static_cast<const QoreIRLocalInstruction*>(
            left_definition->second);
        const auto* right_load = static_cast<const QoreIRLocalInstruction*>(
            right_definition->second);
        return left_load->local == right_load->local
            && immutable_parameters.count(left_load->local);
    };
    auto same_guarded_projection = [&](const Projection& lhs,
            const Projection& rhs) {
        if (!lhs.guarded_index.isValid() || !rhs.guarded_index.isValid()
                || !same_guarded_index(
                    lhs.guarded_index, rhs.guarded_index)
                || lhs.kind != rhs.kind || lhs.operand != rhs.operand
                || lhs.size != rhs.size
                || lhs.int_constant != rhs.int_constant
                || std::memcmp(&lhs.float_constant, &rhs.float_constant,
                    sizeof(lhs.float_constant))
                || lhs.guarded_hash_key != rhs.guarded_hash_key
                || lhs.negative_offsets != rhs.negative_offsets
                || lhs.guarded_descriptors.size()
                    != rhs.guarded_descriptors.size()
                || lhs.guarded_keys != rhs.guarded_keys) {
            return false;
        }
        for (size_t i = 0; i < lhs.guarded_descriptors.size(); ++i) {
            const auto& left = lhs.guarded_descriptors[i];
            const auto& right = rhs.guarded_descriptors[i];
            if (left.kind != right.kind || left.operand != right.operand
                    || left.int_constant != right.int_constant
                    || std::memcmp(&left.float_constant,
                        &right.float_constant, sizeof(left.float_constant))) {
                return false;
            }
        }
        return true;
    };
    auto descend_nested_projection = [&](Projection& projection) {
        bool descended = false;
        for (size_t depth = 0; depth < 8
                && projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedValue;
                ++depth) {
            QoreIRValue selected = projection.call->operands[
                static_cast<size_t>(projection.operand)];
            auto definition = definitions.find(selected.id);
            auto selected_uses = uses.find(selected.id);
            auto projection_uses = uses.find(projection.result.id);
            if (definition == definitions.end()
                    || definition->second->opcode
                        != QoreIROpcode::CallDirect
                    || selected_uses == uses.end()
                    || selected_uses->second.size() != 1
                    || selected_uses->second.front().inst
                        != projection.call
                    || projection_uses == uses.end()
                    || projection_uses->second.size() != 1
                    || !projection_uses->second.front().inst) {
                break;
            }
            auto* nested_call =
                static_cast<QoreIRCallDirectInstruction*>(
                    const_cast<QoreIRInstruction*>(definition->second));
            if (nested_call->exception_target) {
                break;
            }
            Projection nested;
            if (!analyze_projection(nested_call, projection.result,
                    const_cast<QoreIRInstruction*>(
                        projection_uses->second.front().inst), nested)) {
                break;
            }
            nested.eliminated = std::move(projection.eliminated);
            nested.eliminated.push_back(projection.call);
            nested.eliminated.push_back(projection.consumer);
            projection = std::move(nested);
            descended = true;
        }
        return descended;
    };
    for (size_t block_id = 0; block_id < func.blocks.size(); ++block_id) {
        const auto& block = func.blocks[block_id];
        for (size_t inst_offset = 0;
                inst_offset < block->instructions.size(); ++inst_offset) {
            const auto& inst_ptr = block->instructions[inst_offset];
            if (qore_ir_analysis_cancelled(check_count,
                    "IR aggregate-return projection candidate analysis")) {
                return 0;
            }
            bool direct_call = inst_ptr
                && inst_ptr->opcode == QoreIROpcode::CallDirect;
            bool static_call = inst_ptr
                && inst_ptr->opcode == QoreIROpcode::CallStaticDirect;
            bool exact_method_call = inst_ptr
                && inst_ptr->opcode == QoreIROpcode::CallMethodDirect
                && qore_ir_is_non_overridable_method_call(*inst_ptr);
            bool exact_object_method_call = inst_ptr
                && inst_ptr->opcode == QoreIROpcode::DotEvalMethodDirect
                && qore_ir_is_non_overridable_method_call(*inst_ptr);
            if ((!direct_call && !static_call && !exact_method_call
                        && !exact_object_method_call)
                    || inst_ptr->exception_target
                    || !inst_ptr->result.isValid()) {
                continue;
            }
            auto use = uses.find(inst_ptr->result.id);
            if (use == uses.end() || use->second.empty()) {
                continue;
            }
            if (exact_object_method_call) {
                if (use->second.size() == 1 && use->second.front().inst
                        && use->second.front().inst->opcode
                            != QoreIROpcode::StoreLocal) {
                    Projection projection;
                    if (analyze_projection(inst_ptr.get(),
                            inst_ptr->result,
                            const_cast<QoreIRInstruction*>(
                                use->second.front().inst), projection)) {
                        projections.push_back(std::move(projection));
                    }
                }
                continue;
            }
            if (!direct_call) {
                if (use->second.size() == 1 && use->second.front().inst
                        && use->second.front().inst->opcode
                            != QoreIROpcode::StoreLocal) {
                    Projection projection;
                    if (analyze_projection(inst_ptr.get(), inst_ptr->result,
                            const_cast<QoreIRInstruction*>(
                                use->second.front().inst), projection)) {
                        VirtualizedCall candidate;
                        candidate.call = inst_ptr.get();
                        if (add_virtualized_projection(
                                candidate, projection)) {
                            candidate.eliminated.push_back(inst_ptr.get());
                            virtualized.push_back(std::move(candidate));
                        }
                    }
                    continue;
                }
                if (use->second.size() != 1
                        || !use->second.front().inst
                        || use->second.front().inst->opcode
                            != QoreIROpcode::StoreLocal
                        || use->second.front().block_id != block_id) {
                    continue;
                }
                auto* store = static_cast<QoreIRLocalInstruction*>(
                    const_cast<QoreIRInstruction*>(
                        use->second.front().inst));
                LocalVar* local = store->local;
                bool has_ref_args = true;
                const AbstractQoreFunctionVariant* callee =
                    qore_ir_get_resolved_effect_callee(
                        inst_ptr.get(), has_ref_args);
                if (!local || store->weak || !store->initial_assignment
                        || store->operands.size() != 1
                        || store->operands.front().id
                            != inst_ptr->result.id
                        || !callee || has_ref_args
                        || callee->getReturnTypeInfo()
                            != local->getTypeInfo()
                        || local->closureUse()
                        || QoreTypeInfo::isReference(
                            local->getTypeInfo())
                        || !func.ir_only_locals.count(
                            reinterpret_cast<const void*>(local))) {
                    continue;
                }
                auto local_ops = local_operations.find(local);
                if (local_ops == local_operations.end()) {
                    continue;
                }
                size_t store_offset = block->instructions.size();
                for (const LocalOperation& op : local_ops->second) {
                    if (qore_ir_analysis_cancelled(check_count,
                            "IR exact-method aggregate definition analysis")) {
                        return 0;
                    }
                    if (op.instruction == store) {
                        store_offset = op.offset;
                        break;
                    }
                }
                if (store_offset == block->instructions.size()
                        || store_offset <= inst_offset) {
                    continue;
                }
                VirtualizedCall candidate;
                candidate.call = inst_ptr.get();
                candidate.eliminated.push_back(inst_ptr.get());
                candidate.eliminated.push_back(store);
                bool valid = true;
                size_t projection_count = 0;
                for (const LocalOperation& op : local_ops->second) {
                    if (qore_ir_analysis_cancelled(check_count,
                            "IR exact-method aggregate local analysis")) {
                        return 0;
                    }
                    QoreIRInstruction* local_inst = op.instruction;
                    if (local_inst == store
                            || local_inst->opcode
                                == QoreIROpcode::InstantiateLocal
                            || local_inst->opcode
                                == QoreIROpcode::UninstantiateLocal) {
                        continue;
                    }
                    if (local_inst->opcode
                                != QoreIROpcode::LoadLocal
                            || (op.block_id == block_id
                                ? op.offset <= store_offset
                                : !cfg.dominates(block_id, op.block_id))
                            || !local_inst->result.isValid()) {
                        valid = false;
                        break;
                    }
                    auto load_use = uses.find(local_inst->result.id);
                    if (load_use == uses.end()
                            || load_use->second.empty()) {
                        candidate.eliminated.push_back(local_inst);
                        continue;
                    }
                    for (const QoreIRScalarUse& scalar_use :
                            load_use->second) {
                        if (qore_ir_analysis_cancelled(check_count,
                                "IR exact-method aggregate use analysis")) {
                            return 0;
                        }
                        Projection projection;
                        if (!scalar_use.inst
                                || !analyze_projection(inst_ptr.get(),
                                    local_inst->result,
                                    const_cast<QoreIRInstruction*>(
                                        scalar_use.inst),
                                    projection)
                                || !add_virtualized_projection(
                                    candidate, projection)) {
                            valid = false;
                            break;
                        }
                        ++projection_count;
                    }
                    if (!valid) {
                        break;
                    }
                    candidate.eliminated.push_back(local_inst);
                }
                if (valid && projection_count) {
                    virtualized.push_back(std::move(candidate));
                }
                continue;
            }
            auto* call =
                static_cast<QoreIRCallDirectInstruction*>(inst_ptr.get());
            if (use->second.size() == 1 && use->second.front().inst
                    && use->second.front().inst->opcode
                        != QoreIROpcode::StoreLocal) {
                Projection projection;
                if (analyze_projection(call, inst_ptr->result,
                        const_cast<QoreIRInstruction*>(
                            use->second.front().inst), projection)) {
                    (void)descend_nested_projection(projection);
                    projections.push_back(projection);
                }
                continue;
            }

            if (use->second.size() > 1) {
                VirtualizedCall candidate;
                candidate.call = call;
                std::unique_ptr<Projection> guarded_projection;
                bool saw_virtualized_projection = false;
                bool valid = true;
                for (const QoreIRScalarUse& call_use : use->second) {
                    if (qore_ir_analysis_cancelled(check_count,
                            "IR aggregate multi-use virtualization analysis")) {
                        return 0;
                    }
                    Projection projection;
                    if (!call_use.inst
                            || !analyze_projection(call, inst_ptr->result,
                                const_cast<QoreIRInstruction*>(call_use.inst),
                                projection)) {
                        valid = false;
                        break;
                    }
                    if (projection.guarded_index.isValid()) {
                        if (saw_virtualized_projection) {
                            valid = false;
                            break;
                        }
                        if (!guarded_projection) {
                            guarded_projection =
                                std::make_unique<Projection>(
                                    std::move(projection));
                        } else if (!same_guarded_projection(
                                *guarded_projection, projection)) {
                            valid = false;
                            break;
                        } else {
                            guarded_projection->eliminated.push_back(
                                projection.consumer);
                            guarded_projection->replacements.emplace_back(
                                projection.result.id,
                                guarded_projection->result);
                        }
                    } else {
                        if (guarded_projection
                                || !add_virtualized_projection(
                                    candidate, projection)) {
                            valid = false;
                            break;
                        }
                        saw_virtualized_projection = true;
                    }
                }
                if (valid) {
                    if (guarded_projection) {
                        projections.push_back(
                            std::move(*guarded_projection));
                    } else {
                        candidate.eliminated.push_back(call);
                        virtualized.push_back(std::move(candidate));
                    }
                }
                continue;
            }

            const QoreIRScalarUse& call_use = use->second.front();
            if (!call_use.inst
                    || call_use.inst->opcode != QoreIROpcode::StoreLocal
                    || call_use.block_id != block_id) {
                continue;
            }
            auto* store = static_cast<QoreIRLocalInstruction*>(
                const_cast<QoreIRInstruction*>(call_use.inst));
            LocalVar* local = store->local;
            bool has_ref_args = true;
            const AbstractQoreFunctionVariant* callee =
                qore_ir_get_resolved_effect_callee(call, has_ref_args);
            if (!local || store->weak || !store->initial_assignment
                    || store->operands.size() != 1
                    || store->operands.front().id != call->result.id
                    || !callee || has_ref_args
                    || callee->getReturnTypeInfo() != local->getTypeInfo()
                    || local->closureUse()
                    || QoreTypeInfo::isReference(local->getTypeInfo())
                    || !func.ir_only_locals.count(
                        reinterpret_cast<const void*>(local))) {
                continue;
            }

            auto local_ops = local_operations.find(local);
            if (local_ops == local_operations.end()) {
                continue;
            }
            size_t store_offset = block->instructions.size();
            for (const LocalOperation& op : local_ops->second) {
                if (qore_ir_analysis_cancelled(check_count,
                        "IR aggregate local definition analysis")) {
                    return 0;
                }
                if (op.instruction == store) {
                    store_offset = op.offset;
                    break;
                }
            }
            if (store_offset == block->instructions.size()
                    || store_offset <= inst_offset) {
                continue;
            }

            VirtualizedCall candidate;
            candidate.call = call;
            candidate.eliminated.push_back(call);
            candidate.eliminated.push_back(store);
            bool valid = true;
            size_t projection_count = 0;
            std::unique_ptr<Projection> nested_projection;
            std::unique_ptr<Projection> direct_local_projection;
            std::unique_ptr<Projection> guarded_local_projection;
            bool saw_virtualized_projection = false;
            auto process_local_projection = [&](Projection projection) {
                Projection nested = projection;
                if (descend_nested_projection(nested)) {
                    nested_projection =
                        std::make_unique<Projection>(std::move(nested));
                }
                if (projection.guarded_index.isValid()) {
                    if (saw_virtualized_projection
                            || direct_local_projection) {
                        return false;
                    }
                    if (!guarded_local_projection) {
                        guarded_local_projection =
                            std::make_unique<Projection>(
                                std::move(projection));
                    } else if (!same_guarded_projection(
                            *guarded_local_projection, projection)) {
                        return false;
                    } else {
                        guarded_local_projection->eliminated.push_back(
                            projection.consumer);
                        guarded_local_projection->replacements.emplace_back(
                            projection.result.id,
                            guarded_local_projection->result);
                    }
                } else {
                    if (guarded_local_projection
                            || direct_local_projection
                            || !add_virtualized_projection(
                                candidate, projection)) {
                        if (projection_count) {
                            return false;
                        }
                        direct_local_projection =
                            std::make_unique<Projection>(
                                std::move(projection));
                    } else {
                        saw_virtualized_projection = true;
                    }
                }
                ++projection_count;
                return true;
            };
            for (const LocalOperation& op : local_ops->second) {
                if (qore_ir_analysis_cancelled(check_count,
                        "IR aggregate local virtualization analysis")) {
                    return 0;
                }
                QoreIRInstruction* local_inst = op.instruction;
                if (local_inst == store
                        || local_inst->opcode
                            == QoreIROpcode::InstantiateLocal
                        || local_inst->opcode
                            == QoreIROpcode::UninstantiateLocal) {
                    continue;
                }
                if (local_inst->opcode != QoreIROpcode::LoadLocal
                        || (op.block_id == block_id
                            ? op.offset <= store_offset
                            : !cfg.dominates(block_id, op.block_id))
                        || !local_inst->result.isValid()) {
                    valid = false;
                    break;
                }
                auto load_use = uses.find(local_inst->result.id);
                if (load_use == uses.end() || load_use->second.empty()) {
                    candidate.eliminated.push_back(local_inst);
                    continue;
                }
                for (const QoreIRScalarUse& scalar_use :
                        load_use->second) {
                    if (qore_ir_analysis_cancelled(check_count,
                            "IR shared aggregate local use analysis")) {
                        return 0;
                    }
                    Projection projection;
                    if (!scalar_use.inst
                            || !analyze_projection(
                                call, local_inst->result,
                                const_cast<QoreIRInstruction*>(
                                    scalar_use.inst),
                                projection)
                            || !process_local_projection(
                                std::move(projection))) {
                        valid = false;
                        break;
                    }
                }
                if (!valid) {
                    break;
                }
                candidate.eliminated.push_back(local_inst);
            }
            if (valid && projection_count) {
                if (guarded_local_projection) {
                    guarded_local_projection->eliminated.insert(
                        guarded_local_projection->eliminated.end(),
                        candidate.eliminated.begin() + 1,
                        candidate.eliminated.end());
                    projections.push_back(
                        std::move(*guarded_local_projection));
                } else if (projection_count == 1 && nested_projection) {
                    nested_projection->eliminated.insert(
                        nested_projection->eliminated.end(),
                        candidate.eliminated.begin(),
                        candidate.eliminated.end());
                    projections.push_back(std::move(*nested_projection));
                } else if (projection_count == 1
                        && direct_local_projection) {
                    direct_local_projection->eliminated.insert(
                        direct_local_projection->eliminated.end(),
                        candidate.eliminated.begin() + 1,
                        candidate.eliminated.end());
                    projections.push_back(
                        std::move(*direct_local_projection));
                } else {
                    virtualized.push_back(std::move(candidate));
                }
            }
        }
    }
    std::vector<VirtualizedPhi> virtualized_phis;
    for (const auto& block : func.blocks) {
        for (const auto& inst_ptr : block->instructions) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR aggregate phi projection analysis")) {
                return 0;
            }
            if (!inst_ptr || inst_ptr->opcode != QoreIROpcode::Phi
                    || !inst_ptr->result.isValid()) {
                continue;
            }
            auto* phi = static_cast<QoreIRPhiInstruction*>(inst_ptr.get());
            if (phi->value_kind != QoreIRPhiValueKind::QoreValue
                    || phi->incoming.empty()) {
                continue;
            }
            auto phi_uses = uses.find(phi->result.id);
            if (phi_uses == uses.end() || phi_uses->second.size() != 1
                    || !phi_uses->second.front().inst) {
                continue;
            }
            QoreIRInstruction* consumer = const_cast<QoreIRInstruction*>(
                phi_uses->second.front().inst);
            auto scalar_phi = std::make_unique<QoreIRPhiInstruction>();
            VirtualizedPhi candidate;
            candidate.phi = phi;
            bool valid = true;
            bool direct_native = true;
            bool have_phi_kind = false;
            QoreIRPhiValueKind phi_kind = QoreIRPhiValueKind::QoreValue;
            QoreIRInstruction* final_consumer = nullptr;
            QoreIRValue final_result;
            std::vector<std::pair<Projection, QoreIRBasicBlock*>>
                incoming_projections;
            for (const QoreIRPhiIncoming& incoming : phi->incoming) {
                if (qore_ir_analysis_cancelled(check_count,
                        "IR aggregate phi incoming analysis")) {
                    return 0;
                }
                auto definition = definitions.find(incoming.value.id);
                if (!incoming.block || definition == definitions.end()) {
                    valid = false;
                    break;
                }
                QoreIRInstruction* call =
                    const_cast<QoreIRInstruction*>(definition->second);
                bool direct_call = call->opcode == QoreIROpcode::CallDirect;
                bool static_call =
                    call->opcode == QoreIROpcode::CallStaticDirect;
                bool exact_method_call =
                    call->opcode == QoreIROpcode::CallMethodDirect
                    && qore_ir_is_non_overridable_method_call(*call);
                if (!direct_call && !static_call && !exact_method_call) {
                    valid = false;
                    break;
                }
                auto call_uses = uses.find(call->result.id);
                Projection projection;
                if (call->exception_target
                        || call_uses == uses.end()
                        || call_uses->second.size() != 1
                        || call_uses->second.front().inst != phi
                        || !analyze_projection(call, phi->result, consumer,
                            projection, true)) {
                    valid = false;
                    break;
                }
                if (direct_call) {
                    (void)descend_nested_projection(projection);
                } else if (projection.kind
                            != QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::NativeInt
                        && projection.kind
                            != QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::NativeFloat
                        && projection.kind
                            != QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::BoxedInt
                        && projection.kind
                            != QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::BoxedFloat) {
                    // Static and exact-method calls have no projected-call
                    // lowering yet. Native argument projections are safe
                    // because the call can be eliminated and the incoming
                    // operand can feed the scalar PHI directly.
                    valid = false;
                    break;
                }
                if (!direct_call && (projection.guarded_index.isValid()
                        || !projection.guarded_descriptors.empty())) {
                    // Static and exact-method calls can only be eliminated
                    // into native PHI inputs. Guarded projections require
                    // projected-call lowering, which these call kinds do not
                    // provide.
                    valid = false;
                    break;
                }
                if ((final_consumer
                            && final_consumer != projection.consumer)
                        || (final_result.isValid()
                            && final_result.id != projection.result.id)) {
                    valid = false;
                    break;
                }
                final_consumer = projection.consumer;
                final_result = projection.result;
                bool unguarded_projection =
                    !projection.guarded_index.isValid()
                    && projection.guarded_descriptors.empty();
                if (unguarded_projection && projection.kind
                        == QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::BoxedInt) {
                    projection.kind = QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::NativeInt;
                } else if (unguarded_projection && projection.kind
                        == QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::BoxedFloat) {
                    projection.kind = QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::NativeFloat;
                }
                QoreIRPhiValueKind incoming_kind;
                if (projection.kind
                        == QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::NativeInt
                        || projection.kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::NativeIntConstant
                        || projection.kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::Size) {
                    incoming_kind = QoreIRPhiValueKind::NativeInt;
                } else if (projection.kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::NativeFloat
                        || projection.kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::
                                    NativeFloatConstant
                        || (unguarded_projection
                            && (projection.kind
                                    == QoreIRCallDirectInstruction::
                                        AOTAggregateProjectionKind::
                                            BoxedFloat
                                || projection.kind
                                    == QoreIRCallDirectInstruction::
                                        AOTAggregateProjectionKind::
                                            BoxedFloatConstant))) {
                    incoming_kind = QoreIRPhiValueKind::NativeFloat;
                    if (unguarded_projection && projection.kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::
                                    BoxedFloatConstant) {
                        projection.kind =
                            QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::
                                    NativeFloatConstant;
                    }
                } else if (projection.kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::BoxedValue
                        || projection.kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::BoxedInt
                        || projection.kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::BoxedFloat
                        || projection.kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::BoxedBool
                        || projection.kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::BoxedIntConstant
                        || projection.kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::BoxedBoolConstant) {
                    incoming_kind = QoreIRPhiValueKind::QoreValue;
                } else {
                    valid = false;
                    break;
                }
                if (have_phi_kind && incoming_kind != phi_kind) {
                    valid = false;
                    break;
                }
                have_phi_kind = true;
                phi_kind = incoming_kind;
                direct_native = direct_native
                    && (projection.kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::NativeInt
                        || projection.kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::NativeFloat);
                candidate.eliminated.push_back(projection.consumer);
                candidate.eliminated.insert(candidate.eliminated.end(),
                    projection.eliminated.begin(),
                    projection.eliminated.end());
                incoming_projections.emplace_back(
                    std::move(projection), incoming.block);
            }
            if (valid && have_phi_kind && final_consumer
                    && final_result.isValid()) {
                scalar_phi->loc = final_consumer->loc;
                scalar_phi->cached_start_line =
                    final_consumer->cached_start_line;
                scalar_phi->result = final_result;
                scalar_phi->value_kind = phi_kind;
                for (auto& [projection, incoming_block] :
                        incoming_projections) {
                    QoreIRValue value;
                    if (direct_native) {
                        value = projection.call->operands[
                            static_cast<size_t>(projection.operand)];
                        candidate.eliminated.push_back(projection.call);
                    } else {
                        value = projection.call->result;
                        candidate.projected_calls.push_back(
                            std::move(projection));
                    }
                    scalar_phi->incoming.push_back({value, incoming_block});
                    scalar_phi->operands.push_back(value);
                }
                candidate.scalar_phi = std::move(scalar_phi);
                virtualized_phis.push_back(std::move(candidate));
            }
        }
    }
    if (projections.empty() && virtualized.empty()
            && virtualized_phis.empty()) {
        return 0;
    }

    std::unordered_map<QoreIRInstruction*, Projection*> calls;
    std::unordered_set<QoreIRInstruction*> consumers;
    std::unordered_set<QoreIRInstruction*> eliminated;
    std::unordered_map<uint32_t, QoreIRValue> replacements;
    for (Projection& projection : projections) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR aggregate-return projection rewrite preparation")) {
            return 0;
        }
        calls.emplace(projection.call, &projection);
        consumers.insert(projection.consumer);
        eliminated.insert(projection.eliminated.begin(),
            projection.eliminated.end());
        replacements.insert(projection.replacements.begin(),
            projection.replacements.end());
    }
    struct MaterializedProjection {
        std::unique_ptr<QoreIRInstruction> source;
        std::unique_ptr<QoreIRInstruction> replacement;
    };
    std::unordered_map<QoreIRInstruction*, MaterializedProjection>
        materialized_replacements;
    std::unordered_set<uint32_t> rewrite_sources;
    for (VirtualizedCall& candidate : virtualized) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR aggregate virtualization rewrite preparation")) {
            return 0;
        }
        eliminated.insert(candidate.eliminated.begin(),
            candidate.eliminated.end());
        replacements.insert(candidate.replacements.begin(),
            candidate.replacements.end());
        for (const auto& replacement : candidate.replacements) {
            rewrite_sources.insert(replacement.second.id);
        }
        for (const Projection& projection :
                candidate.materialized_projections) {
            MaterializedProjection materialized;
            if (projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedNothingConstant) {
                auto replacement =
                    std::make_unique<QoreIRConstInstruction>();
                replacement->opcode = QoreIROpcode::ConstNothing;
                replacement->loc = projection.consumer->loc;
                replacement->cached_start_line =
                    projection.consumer->cached_start_line;
                replacement->temp_scope_id =
                    projection.consumer->temp_scope_id;
                replacement->result = projection.consumer->result;
                replacement->constant.kind =
                    QoreIRConstant::Kind::Nothing;
                QoreIRValueFacts facts;
                facts.type_info = nothingTypeInfo;
                facts.assigned_state = QoreIRAssignedState::Unassigned;
                facts.representation = QoreIRValueRepresentation::Boxed;
                facts.never_nothing = false;
                func.setValueFacts(replacement->result, facts);
                materialized.replacement = std::move(replacement);
                materialized_replacements.emplace(
                    projection.consumer, std::move(materialized));
                continue;
            }
            QoreIRValue source;
            bool boxed = projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedValue
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedInt
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedFloat
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedBool
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedIntConstant
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedFloatConstant
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedBoolConstant;
            bool constant = projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::NativeIntConstant
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::NativeFloatConstant
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedIntConstant
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedFloatConstant
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedBoolConstant
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::Size;
            if (!constant) {
                source = projection.call->operands[
                    static_cast<size_t>(projection.operand)];
                rewrite_sources.insert(source.id);
            } else {
                auto source_constant =
                    std::make_unique<QoreIRConstInstruction>();
                source_constant->loc = projection.consumer->loc;
                source_constant->cached_start_line =
                    projection.consumer->cached_start_line;
                source_constant->temp_scope_id =
                    projection.consumer->temp_scope_id;
                source_constant->result = boxed
                    ? func.createValue() : projection.consumer->result;
                QoreIRValueFacts facts;
                facts.assigned_state = QoreIRAssignedState::Assigned;
                facts.never_nothing = true;
                if (projection.kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::
                                    NativeFloatConstant
                        || projection.kind
                            == QoreIRCallDirectInstruction::
                                AOTAggregateProjectionKind::
                                    BoxedFloatConstant) {
                    source_constant->opcode = QoreIROpcode::ConstFloat;
                    source_constant->constant.kind =
                        QoreIRConstant::Kind::Float;
                    source_constant->constant.float_value =
                        projection.float_constant;
                    facts.type_info = floatTypeInfo;
                    facts.representation =
                        QoreIRValueRepresentation::NativeFloat;
                } else if (projection.kind
                        == QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::
                                BoxedBoolConstant) {
                    source_constant->opcode = QoreIROpcode::ConstBool;
                    source_constant->constant.kind =
                        QoreIRConstant::Kind::Bool;
                    source_constant->constant.bool_value =
                        projection.int_constant != 0;
                    facts.type_info = boolTypeInfo;
                    facts.representation =
                        QoreIRValueRepresentation::NativeBool;
                } else {
                    source_constant->opcode = QoreIROpcode::ConstInt;
                    source_constant->constant.kind =
                        QoreIRConstant::Kind::Int;
                    source_constant->constant.int_value =
                        projection.kind
                                == QoreIRCallDirectInstruction::
                                    AOTAggregateProjectionKind::Size
                            ? projection.size : projection.int_constant;
                    facts.type_info = bigIntTypeInfo;
                    facts.representation =
                        QoreIRValueRepresentation::NativeInt;
                }
                func.setValueFacts(source_constant->result, facts);
                source = source_constant->result;
                if (boxed) {
                    materialized.source = std::move(source_constant);
                } else {
                    materialized.replacement =
                        std::move(source_constant);
                }
            }
            if (boxed) {
                auto replacement =
                    std::make_unique<QoreIRInstruction>(
                        QoreIROpcode::RefSelf);
                replacement->loc = projection.consumer->loc;
                replacement->cached_start_line =
                    projection.consumer->cached_start_line;
                replacement->temp_scope_id =
                    projection.consumer->temp_scope_id;
                replacement->result = projection.consumer->result;
                replacement->operands.push_back(source);
                materialized.replacement = std::move(replacement);
                QoreIRValueFacts facts;
                if (const QoreIRValueFacts* source_facts =
                        func.getValueFacts(source)) {
                    facts = *source_facts;
                }
                facts.assigned_state = QoreIRAssignedState::Assigned;
                facts.representation = QoreIRValueRepresentation::Boxed;
                facts.never_nothing = true;
                func.setValueFacts(projection.consumer->result, facts);
            }
            materialized_replacements.emplace(
                projection.consumer, std::move(materialized));
        }
    }
    std::unordered_map<QoreIRInstruction*,
        std::unique_ptr<QoreIRPhiInstruction>> phi_replacements;
    std::unordered_map<QoreIRInstruction*, Projection*> projected_phi_calls;
    struct GuardedPhiIndexCloneRequest {
        Projection* projection = nullptr;
        const QoreIRLocalInstruction* source = nullptr;
        QoreIRValueFacts facts;
    };
    std::vector<GuardedPhiIndexCloneRequest> guarded_phi_index_requests;
    std::unordered_set<QoreIRInstruction*> guarded_phi_index_calls;
    for (VirtualizedPhi& candidate : virtualized_phis) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR aggregate phi rewrite preparation")) {
            return 0;
        }
        eliminated.insert(candidate.eliminated.begin(),
            candidate.eliminated.end());
        if (candidate.scalar_phi->value_kind
                == QoreIRPhiValueKind::NativeInt) {
            QoreIRValueFacts facts;
            facts.type_info = bigIntTypeInfo;
            facts.assigned_state = QoreIRAssignedState::Assigned;
            facts.representation = QoreIRValueRepresentation::NativeInt;
            facts.never_nothing = true;
            func.setValueFacts(candidate.scalar_phi->result, facts);
        } else if (candidate.scalar_phi->value_kind
                == QoreIRPhiValueKind::NativeFloat) {
            QoreIRValueFacts facts;
            facts.type_info = floatTypeInfo;
            facts.assigned_state = QoreIRAssignedState::Assigned;
            facts.representation = QoreIRValueRepresentation::NativeFloat;
            facts.never_nothing = true;
            func.setValueFacts(candidate.scalar_phi->result, facts);
        }
        for (Projection& projection : candidate.projected_calls) {
            projected_phi_calls.emplace(projection.call, &projection);
            if (!projection.clone_guarded_index) {
                continue;
            }
            auto definition = definitions.find(
                projection.guarded_index.id);
            const QoreIRValueFacts* facts =
                func.getValueFacts(projection.guarded_index);
            if (definition == definitions.end()
                    || definition->second->opcode
                        != QoreIROpcode::LoadLocal
                    || !facts
                    || !guarded_phi_index_calls.insert(
                        projection.call).second) {
                return 0;
            }
            guarded_phi_index_requests.push_back({
                &projection,
                static_cast<const QoreIRLocalInstruction*>(
                    definition->second),
                *facts,
            });
        }
        phi_replacements.emplace(candidate.phi,
            std::move(candidate.scalar_phi));
    }
    std::unordered_map<QoreIRInstruction*,
        std::unique_ptr<QoreIRInstruction>> guarded_phi_index_clones;
    for (const GuardedPhiIndexCloneRequest& request :
            guarded_phi_index_requests) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR guarded phi index clone preparation")) {
            return 0;
        }
        auto clone = std::make_unique<QoreIRLocalInstruction>(
            QoreIROpcode::LoadLocal, request.source->local,
            request.source->auto_ref);
        clone->loc = request.source->loc;
        clone->cached_start_line = request.source->cached_start_line;
        clone->temp_scope_id = request.source->temp_scope_id;
        clone->is_closure = request.source->is_closure;
        clone->is_ref = request.source->is_ref;
        clone->slot_id = request.source->slot_id;
        clone->result = func.createValue();
        func.setValueFacts(clone->result, request.facts);
        request.projection->guarded_index = clone->result;
        guarded_phi_index_clones.emplace(
            request.projection->call, std::move(clone));
    }
    std::vector<QoreIRCallDirectInstruction*> discard_worklist;
    std::unordered_set<QoreIRInstruction*> queued_discards;
    auto enqueue_discard_inputs = [&](const QoreIRInstruction* instruction) {
        for (QoreIRValue operand : instruction->operands) {
            auto definition = definitions.find(operand.id);
            if (definition == definitions.end()
                    || definition->second->opcode != QoreIROpcode::CallDirect) {
                continue;
            }
            auto* call = static_cast<QoreIRCallDirectInstruction*>(
                const_cast<QoreIRInstruction*>(definition->second));
            if (!eliminated.count(call) && !calls.count(call)
                    && queued_discards.insert(call).second) {
                discard_worklist.push_back(call);
            }
        }
    };
    for (QoreIRInstruction* instruction : eliminated) {
        enqueue_discard_inputs(instruction);
    }
    while (!discard_worklist.empty()) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR dead aggregate-return call analysis")) {
            return 0;
        }
        QoreIRCallDirectInstruction* call = discard_worklist.back();
        discard_worklist.pop_back();
        queued_discards.erase(call);
        if (call->exception_target || !call->result.isValid()
                || eliminated.count(call) || calls.count(call)
                || projected_phi_calls.count(call)) {
            continue;
        }
        if (rewrite_sources.count(call->result.id)) {
            continue;
        }
        auto call_uses = uses.find(call->result.id);
        if (call_uses == uses.end() || call_uses->second.empty()) {
            continue;
        }
        bool dead = true;
        for (const QoreIRScalarUse& call_use : call_uses->second) {
            if (!call_use.inst
                    || !eliminated.count(
                        const_cast<QoreIRInstruction*>(call_use.inst))) {
                dead = false;
                break;
            }
        }
        if (!dead) {
            continue;
        }
        bool has_ref_args = true;
        const AbstractQoreFunctionVariant* callee =
            qore_ir_get_resolved_effect_callee(call, has_ref_args);
        int16_t operand = -1;
        int64_t size = 0;
        int64_t int_constant = 0;
        double float_constant = 0.0;
        auto projection =
            QoreIRCallDirectInstruction::AOTAggregateProjectionKind::None;
        std::vector<QoreIRCallDirectInstruction::
            AOTAggregateProjectionDescriptor> guarded_descriptors;
        std::vector<std::string> guarded_keys;
        if (!callee || has_ref_args
                || !get_projection(callee, call,
                    QoreIRAggregateProjectionQueryKind::DiscardResult,
                    0, std::string(), operand, size, int_constant,
                    float_constant, projection, guarded_descriptors,
                    guarded_keys)) {
            continue;
        }
        eliminated.insert(call);
        enqueue_discard_inputs(call);
    }
    auto apply_call_projection = [&](Projection& projection,
            QoreIRValue result) {
        auto* call = projection.call->opcode == QoreIROpcode::CallDirect
            ? static_cast<QoreIRCallDirectInstruction*>(projection.call)
            : nullptr;
        auto* object_call =
                projection.call->opcode
                    == QoreIROpcode::DotEvalMethodDirect
            ? static_cast<QoreIRDotEvalMethodDirectInstruction*>(
                projection.call)
            : nullptr;
        assert(call || object_call);
        if (call) {
            call->aot_aggregate_projection = projection.kind;
            call->aot_aggregate_projection_operand =
                projection.operand;
            call->aot_aggregate_projection_size = projection.size;
            call->aot_aggregate_projection_int =
                projection.int_constant;
            call->aot_aggregate_projection_float =
                projection.float_constant;
            call->aot_aggregate_projection_guarded_descriptors =
                projection.guarded_descriptors;
            call->aot_aggregate_projection_guarded_keys =
                projection.guarded_keys;
            if (projection.guarded_index.isValid()) {
                call->aot_aggregate_projection_guarded_index = true;
                call->aot_aggregate_projection_guarded_hash_key =
                    projection.guarded_hash_key;
                call->aot_aggregate_projection_negative_offsets =
                    projection.negative_offsets;
                call->operands.push_back(projection.guarded_index);
            }
        } else {
            object_call->aot_aggregate_projection = projection.kind;
            object_call->aot_aggregate_projection_operand =
                projection.operand;
        }
        QoreIRValueFacts facts;
        facts.assigned_state = QoreIRAssignedState::Assigned;
        facts.never_nothing = true;
        if (projection.kind
                == QoreIRCallDirectInstruction::
                    AOTAggregateProjectionKind::BoxedValue) {
            const QoreIRValueFacts* source_facts =
                func.getValueFacts(projection.call->operands[
                    static_cast<size_t>(projection.operand)]);
            if (source_facts) {
                facts = *source_facts;
                facts.assigned_state = QoreIRAssignedState::Assigned;
                facts.never_nothing = true;
                facts.representation = QoreIRValueRepresentation::Boxed;
            }
        } else if (projection.kind
                == QoreIRCallDirectInstruction::
                    AOTAggregateProjectionKind::NativeFloat
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::NativeFloatAddConstant
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::NativeFloatSelect
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedFloat
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedFloatAddConstant
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedFloatSelect
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::NativeFloatConstant
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedFloatConstant) {
            facts.type_info = floatTypeInfo;
            facts.representation = projection.kind
                        == QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::NativeFloat
                    || projection.kind
                        == QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::
                                NativeFloatAddConstant
                    || projection.kind
                        == QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::
                                NativeFloatSelect
                    || projection.kind
                        == QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::NativeFloatConstant
                ? QoreIRValueRepresentation::NativeFloat
                : QoreIRValueRepresentation::Boxed;
        } else if (projection.kind
                        == QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::BoxedBool
                || projection.kind
                    == QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::BoxedBoolSelect
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedBoolIntCompare
                || projection.kind
                    == QoreIRCallDirectInstruction::
                        AOTAggregateProjectionKind::BoxedBoolConstant) {
            facts.type_info = boolTypeInfo;
            facts.representation = QoreIRValueRepresentation::Boxed;
        } else if (projection.kind
                == QoreIRCallDirectInstruction::
                    AOTAggregateProjectionKind::BoxedNothingConstant) {
            facts.type_info = nothingTypeInfo;
            facts.assigned_state = QoreIRAssignedState::Unassigned;
            facts.representation = QoreIRValueRepresentation::Boxed;
            facts.never_nothing = false;
        } else {
            facts.type_info = bigIntTypeInfo;
            facts.representation = projection.kind
                        == QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::NativeInt
                    || projection.kind
                        == QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::
                                NativeIntAddConstant
                    || projection.kind
                        == QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::NativeIntBinary
                    || projection.kind
                        == QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::
                                NativeIntMulConstant
                    || projection.kind
                        == QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::NativeIntSelect
                    || projection.kind
                        == QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::BoxedIntBinary
                    || projection.kind
                        == QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::
                                BoxedIntMulConstant
                    || projection.kind
                        == QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::NativeIntConstant
                    || projection.kind
                        == QoreIRCallDirectInstruction::
                            AOTAggregateProjectionKind::Size
                ? QoreIRValueRepresentation::NativeInt
                : QoreIRValueRepresentation::Boxed;
        }
        if (projection.guarded_index.isValid()) {
            facts.assigned_state = QoreIRAssignedState::MaybeAssigned;
            facts.representation = QoreIRValueRepresentation::Boxed;
            facts.never_nothing = false;
        }
        func.setValueFacts(result, facts);
    };
    std::unordered_map<QoreIRInstruction*, Projection*> guarded_calls;
    std::unordered_map<QoreIRInstruction*, Projection*> guarded_consumers;
    for (Projection& projection : projections) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR guarded aggregate projection rewrite preparation")) {
            return 0;
        }
        if (projection.guarded_index.isValid()) {
            guarded_calls.emplace(projection.call, &projection);
            guarded_consumers.emplace(projection.consumer, &projection);
        }
    }
    std::unordered_map<QoreIRInstruction*,
        std::unique_ptr<QoreIRInstruction>> moved_guarded_calls;
    for (const auto& block : func.blocks) {
        auto& instructions = block->instructions;
        for (auto inst = instructions.begin(); inst != instructions.end();) {
            // Complete the committed rewrite even if cancellation is requested.
            if (++check_count % 100 == 0) {
                (void)qore_check_cancel(nullptr,
                    "IR aggregate-return projection fusion");
            }
            auto materialized =
                materialized_replacements.find(inst->get());
            if (materialized != materialized_replacements.end()) {
                if (materialized->second.source) {
                    size_t offset = static_cast<size_t>(
                        std::distance(instructions.begin(), inst));
                    instructions.insert(inst,
                        std::move(materialized->second.source));
                    inst = instructions.begin()
                        + static_cast<std::ptrdiff_t>(offset + 1);
                }
                *inst = std::move(materialized->second.replacement);
                (void)qore_ir_rewrite_value_operands(
                    **inst, replacements, check_count, false);
                ++inst;
                continue;
            }
            auto phi_replacement = phi_replacements.find(inst->get());
            if (phi_replacement != phi_replacements.end()) {
                *inst = std::move(phi_replacement->second);
                ++inst;
                continue;
            }
            auto guarded_call = guarded_calls.find(inst->get());
            if (guarded_call != guarded_calls.end()) {
                (void)qore_ir_rewrite_value_operands(
                    **inst, replacements, check_count, false);
                moved_guarded_calls.emplace(
                    guarded_call->second->consumer, std::move(*inst));
                inst = instructions.erase(inst);
                continue;
            }
            auto guarded_consumer = guarded_consumers.find(inst->get());
            if (guarded_consumer != guarded_consumers.end()) {
                auto moved =
                    moved_guarded_calls.find(inst->get());
                if (moved == moved_guarded_calls.end()) {
                    ++inst;
                    continue;
                }
                Projection& projection = *guarded_consumer->second;
                *inst = std::move(moved->second);
                moved_guarded_calls.erase(moved);
                projection.call->result = projection.result;
                apply_call_projection(projection, projection.result);
                ++inst;
                continue;
            }
            if (consumers.count(inst->get()) || eliminated.count(inst->get())) {
                inst = instructions.erase(inst);
                continue;
            }
            auto guarded_phi_index =
                guarded_phi_index_clones.find(inst->get());
            if (guarded_phi_index != guarded_phi_index_clones.end()) {
                size_t offset = static_cast<size_t>(
                    std::distance(instructions.begin(), inst));
                instructions.insert(inst,
                    std::move(guarded_phi_index->second));
                inst = instructions.begin()
                    + static_cast<std::ptrdiff_t>(offset + 1);
            }
            (void)qore_ir_rewrite_value_operands(
                **inst, replacements, check_count, false);
            auto projected_phi_call = projected_phi_calls.find(inst->get());
            if (projected_phi_call != projected_phi_calls.end()) {
                apply_call_projection(*projected_phi_call->second,
                    projected_phi_call->second->call->result);
                ++inst;
                continue;
            }
            auto call = calls.find(inst->get());
            if (call == calls.end()) {
                ++inst;
                continue;
            }
            Projection& projection = *call->second;
            projection.call->result = projection.result;
            apply_call_projection(projection, projection.result);
            ++inst;
        }
    }
    return projections.size() + virtualized.size()
        + virtualized_phis.size();
}

size_t qore_ir_fold_boxed_return_param_calls(QoreIRFunction& func,
        const QoreIRBoxedReturnParamQuery& get_return_param) {
    if (!get_return_param) {
        return 0;
    }
    size_t check_count = 0;
    std::unordered_map<uint32_t, const QoreIRInstruction*> definitions;
    std::unordered_map<uint32_t, QoreIRValue> replacements;
    std::unordered_map<uint32_t, const QoreIRInstruction*> calls;
    for (const auto& block : func.blocks) {
        for (const auto& inst_ptr : block->instructions) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR boxed return-parameter call analysis")) {
                return 0;
            }
            if (inst_ptr && inst_ptr->result.isValid()) {
                definitions.emplace(inst_ptr->result.id, inst_ptr.get());
            }
            const auto* invoke = inst_ptr
                    && inst_ptr->opcode == QoreIROpcode::Invoke
                ? static_cast<const QoreIRInvokeInstruction*>(inst_ptr.get())
                : nullptr;
            bool supported_invoke = invoke
                && (invoke->invoke_opcode == QoreIROpcode::CallDirect
                    || invoke->invoke_opcode
                        == QoreIROpcode::CallStaticDirect);
            if (!inst_ptr || !inst_ptr->result.isValid()
                    || (inst_ptr->exception_target && !supported_invoke)
                    || (inst_ptr->opcode != QoreIROpcode::CallDirect
                        && inst_ptr->opcode
                            != QoreIROpcode::CallStaticDirect
                        && (inst_ptr->opcode
                                != QoreIROpcode::CallMethodDirect
                            || !qore_ir_is_non_overridable_method_call(
                                *inst_ptr))
                        && !supported_invoke)) {
                continue;
            }
            QoreIRInstruction* call = inst_ptr.get();
            bool has_ref_args = true;
            const AbstractQoreFunctionVariant* callee =
                qore_ir_get_resolved_effect_callee(call, has_ref_args);
            int8_t param = -1;
            if (!callee || !get_return_param(callee, call, param)
                    || param < 0
                    || static_cast<size_t>(param) >= call->operands.size()) {
                continue;
            }
            QoreIRValue replacement =
                call->operands[static_cast<size_t>(param)];
            if (!replacement.isValid()
                    || replacement.id == call->result.id) {
                continue;
            }
            replacements.emplace(call->result.id, replacement);
            calls.emplace(call->result.id, call);
        }
    }
    if (replacements.empty()) {
        return 0;
    }
    std::unordered_map<uint32_t, uint8_t> normalization_state;
    normalization_state.reserve(replacements.size());
    for (const auto& [result_id, replacement] : replacements) {
        (void)replacement;
        if (qore_ir_analysis_cancelled(check_count,
                "IR boxed return-parameter replacement normalization")) {
            return 0;
        }
        if (normalization_state[result_id] == 2) {
            continue;
        }
        std::vector<uint32_t> path;
        uint32_t current = result_id;
        QoreIRValue normalized;
        while (true) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR boxed return-parameter replacement normalization")) {
                return 0;
            }
            uint8_t& state = normalization_state[current];
            if (state == 2) {
                normalized = replacements.find(current)->second;
                break;
            }
            if (state == 1) {
                return 0;
            }
            state = 1;
            path.push_back(current);
            auto current_replacement = replacements.find(current);
            if (current_replacement == replacements.end()) {
                return 0;
            }
            auto next = replacements.find(current_replacement->second.id);
            if (next == replacements.end()) {
                normalized = current_replacement->second;
                break;
            }
            current = next->first;
        }
        for (auto path_it = path.rbegin(); path_it != path.rend();
                ++path_it) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR boxed return-parameter path compression")) {
                return 0;
            }
            replacements.find(*path_it)->second = normalized;
            normalization_state[*path_it] = 2;
        }
    }
    for (auto replacement = replacements.begin();
            replacement != replacements.end();) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR boxed return-parameter argument validation")) {
            return 0;
        }
        const QoreIRValueFacts* facts =
            func.getValueFacts(replacement->second);
        bool assigned = facts
            && facts->assigned_state == QoreIRAssignedState::Assigned
            && facts->never_nothing;
        bool native_value = assigned
            && (facts->representation == QoreIRValueRepresentation::NativeInt
                || facts->representation
                    == QoreIRValueRepresentation::NativeFloat
                || facts->representation
                    == QoreIRValueRepresentation::NativeBool);
        bool safe_facts = assigned
            && (facts->representation == QoreIRValueRepresentation::Boxed
                || facts->representation
                    == QoreIRValueRepresentation::Unknown);
        auto definition = definitions.find(replacement->second.id);
        bool fresh_boxed = definition != definitions.end()
            && (definition->second->opcode == QoreIROpcode::MakeHash
                || definition->second->opcode
                    == QoreIROpcode::MakeHashConstKeys
                || definition->second->opcode == QoreIROpcode::MakeList);
        if (!safe_facts && !native_value && !fresh_boxed) {
            replacement = replacements.erase(replacement);
            continue;
        }
        ++replacement;
    }
    if (replacements.empty()) {
        return 0;
    }
    std::unordered_set<const QoreIRInstruction*> eliminated;
    for (const auto& [result_id, replacement] : replacements) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR boxed return-parameter rewrite preparation")) {
            return 0;
        }
        (void)replacement;
        auto call = calls.find(result_id);
        if (call == calls.end()) {
            return 0;
        }
        eliminated.insert(call->second);
    }
    for (const auto& block : func.blocks) {
        auto& instructions = block->instructions;
        for (auto inst = instructions.begin(); inst != instructions.end();) {
            // Complete the committed rewrite even if cancellation is requested.
            if (++check_count % 100 == 0) {
                (void)qore_check_cancel(nullptr,
                    "IR boxed return-parameter call folding");
            }
            if (eliminated.count(inst->get())) {
                if ((*inst)->opcode == QoreIROpcode::Invoke) {
                    const auto* invoke =
                        static_cast<const QoreIRInvokeInstruction*>(
                            inst->get());
                    auto branch =
                        std::make_unique<QoreIRBranchInstruction>();
                    branch->target = invoke->normal_target;
                    branch->loc = invoke->loc;
                    branch->cached_start_line =
                        invoke->cached_start_line;
                    branch->intrinsic = invoke->intrinsic;
                    *inst = std::move(branch);
                    ++inst;
                } else {
                    inst = instructions.erase(inst);
                }
                continue;
            }
            (void)qore_ir_rewrite_value_operands(
                **inst, replacements, check_count, false);
            ++inst;
        }
    }
    return replacements.size();
}

size_t qore_ir_fuse_string_producer_consumers(QoreIRFunction& func,
        const QoreIRStringProducerQuery& is_supported) {
    if (!is_supported) {
        return 0;
    }
    size_t check_count = 0;
    QoreIRScalarUses uses;
    if (!qore_ir_collect_scalar_uses(func, uses, check_count)) {
        return 0;
    }

    struct InstructionPosition {
        QoreIRInstruction* inst = nullptr;
        size_t block = 0;
        size_t offset = 0;
    };
    std::unordered_map<uint32_t, InstructionPosition> definitions;
    std::unordered_map<const QoreIRInstruction*, InstructionPosition> positions;
    std::unordered_map<LocalVar*, std::vector<InstructionPosition>>
        local_operations;
    std::unordered_set<const LocalVar*> written_locals;
    for (size_t block = 0; block < func.blocks.size(); ++block) {
        for (size_t offset = 0;
                offset < func.blocks[block]->instructions.size(); ++offset) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR string producer-consumer definition analysis")) {
                return 0;
            }
            QoreIRInstruction* inst =
                func.blocks[block]->instructions[offset].get();
            InstructionPosition position{inst, block, offset};
            positions.emplace(inst, position);
            if (inst->result.isValid()) {
                definitions.emplace(inst->result.id, position);
            }
            if ((inst->opcode == QoreIROpcode::LoadLocal
                        || inst->opcode == QoreIROpcode::StoreLocal
                        || inst->opcode == QoreIROpcode::InstantiateLocal
                        || inst->opcode == QoreIROpcode::UninstantiateLocal)
                    && static_cast<QoreIRLocalInstruction*>(inst)->local) {
                local_operations[
                    static_cast<QoreIRLocalInstruction*>(inst)->local]
                        .push_back(position);
            }
            if (inst->opcode == QoreIROpcode::StoreLocal) {
                written_locals.insert(
                    static_cast<QoreIRLocalInstruction*>(inst)->local);
            }
        }
    }
    auto get_string_producer = [](QoreIRInstruction* inst)
            -> QoreIRStringConsumerCallInstruction* {
        if (!inst || inst->exception_target || !inst->result.isValid()
                || (inst->opcode != QoreIROpcode::CallDirect
                    && inst->opcode != QoreIROpcode::CallStaticDirect
                    && (inst->opcode != QoreIROpcode::CallMethodDirect
                        || !qore_ir_is_non_overridable_method_call(*inst)))) {
            return nullptr;
        }
        return static_cast<QoreIRStringConsumerCallInstruction*>(inst);
    };
    std::unique_ptr<QoreIRControlFlowGraph> cfg;
    auto side_effect_free_interval = [&](const QoreIRInstruction* producer,
            const QoreIRInstruction* consumer) {
        auto producer_pos = positions.find(producer);
        auto consumer_pos = positions.find(consumer);
        if (producer_pos == positions.end() || consumer_pos == positions.end()
                || producer_pos->second.block != consumer_pos->second.block
                || producer_pos->second.offset >= consumer_pos->second.offset) {
            return false;
        }
        const auto& instructions =
            func.blocks[producer_pos->second.block]->instructions;
        for (size_t offset = producer_pos->second.offset + 1;
                offset < consumer_pos->second.offset; ++offset) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR string producer-consumer interval analysis")) {
                return false;
            }
            switch (instructions[offset]->opcode) {
                case QoreIROpcode::LoadLocal:
                case QoreIROpcode::ConstInt:
                case QoreIROpcode::ConstString:
                    break;
                default:
                    return false;
            }
        }
        return true;
    };
    auto get_const_int = [&](QoreIRValue value, int64_t& result) {
        auto definition = definitions.find(value.id);
        if (definition == definitions.end()
                || definition->second.inst->opcode != QoreIROpcode::ConstInt) {
            return false;
        }
        const auto* constant = static_cast<const QoreIRConstInstruction*>(
            definition->second.inst);
        if (constant->constant.kind != QoreIRConstant::Kind::Int) {
            return false;
        }
        result = constant->constant.int_value;
        return true;
    };
    auto get_pattern_operand = [&](
            const QoreIRStringConsumerCallInstruction* producer,
            QoreIRValue pattern, bool cross_block) -> int16_t {
        auto pattern_definition = definitions.find(pattern.id);
        for (size_t i = 0; i < producer->operands.size(); ++i) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR string producer-consumer pattern analysis")) {
                return -1;
            }
            QoreIRValue operand = producer->operands[i];
            if (operand.id == pattern.id) {
                return static_cast<int16_t>(i);
            }
            auto operand_definition = definitions.find(operand.id);
            if (pattern_definition == definitions.end()
                    || operand_definition == definitions.end()) {
                continue;
            }
            QoreIRInstruction* pattern_inst =
                pattern_definition->second.inst;
            QoreIRInstruction* operand_inst =
                operand_definition->second.inst;
            if (pattern_inst->opcode == QoreIROpcode::LoadLocal
                    && operand_inst->opcode == QoreIROpcode::LoadLocal) {
                const LocalVar* local =
                    static_cast<const QoreIRLocalInstruction*>(
                        pattern_inst)->local;
                if (local
                        && local
                            == static_cast<const QoreIRLocalInstruction*>(
                                operand_inst)->local
                        && !local->closureUse()
                        && !QoreTypeInfo::isReference(local->getTypeInfo())
                        && (!cross_block || !written_locals.count(local))) {
                    return static_cast<int16_t>(i);
                }
            }
            if (pattern_inst->opcode == QoreIROpcode::ConstString
                    && operand_inst->opcode == QoreIROpcode::ConstString
                    && static_cast<const QoreIRConstInstruction*>(
                        pattern_inst)->constant.string_value
                        == static_cast<const QoreIRConstInstruction*>(
                            operand_inst)->constant.string_value) {
                return static_cast<int16_t>(i);
            }
        }
        return -1;
    };

    struct Fusion {
        QoreIRStringConsumerCallInstruction* producer = nullptr;
        QoreIRInstruction* consumer = nullptr;
        QoreIRCallDirectInstruction::AOTStringConsumerKind kind =
            QoreIRCallDirectInstruction::AOTStringConsumerKind::None;
        QoreIRValue result;
        int16_t pattern_operand = -1;
        int64_t arg0 = 0;
        int64_t arg1 = 0;
        bool has_arg1 = false;
        bool preserve_producer_result = false;
        std::vector<QoreIRInstruction*> eliminated;
        std::vector<std::pair<uint32_t, QoreIRValue>> replacements;
    };
    auto analyze_consumer = [&](QoreIRStringConsumerCallInstruction* producer,
            QoreIRValue base, QoreIRInstruction* consumer, bool cross_block,
            Fusion& fusion) {
        if (!producer || !consumer
                || consumer->opcode != QoreIROpcode::DotEvalMethodDirect
                || consumer->exception_target
                || !consumer->result.isValid()
                || consumer->operands.empty()
                || consumer->operands[0].id != base.id) {
            return false;
        }
        const auto* method =
            static_cast<const QoreIRDotEvalMethodDirectInstruction*>(consumer);
        if (!method->pseudo || method->has_ref_args || !method->qc
                || strcmp(method->qc->getName(), "<string>")) {
            return false;
        }
        QoreIRIntrinsic intrinsic = method->intrinsic;
        if (intrinsic == QoreIRIntrinsic::None) {
            intrinsic = qore_ir_resolve_pseudo_intrinsic(method->method,
                method->qc, method->fallback_method_name);
        }
        auto kind = QoreIRCallDirectInstruction::AOTStringConsumerKind::None;
        int16_t pattern_operand = -1;
        int64_t arg0 = 0;
        int64_t arg1 = 0;
        bool has_arg1 = false;
        if (intrinsic == QoreIRIntrinsic::Size
                || intrinsic == QoreIRIntrinsic::StringStrlen) {
            if (consumer->operands.size() != 1) {
                return false;
            }
            kind = QoreIRCallDirectInstruction::AOTStringConsumerKind::Size;
        } else if (intrinsic == QoreIRIntrinsic::StringLength) {
            if (consumer->operands.size() != 1) {
                return false;
            }
            kind = QoreIRCallDirectInstruction::AOTStringConsumerKind::Length;
        } else if (intrinsic == QoreIRIntrinsic::StringStartsWith
                || intrinsic == QoreIRIntrinsic::StringEndsWith
                || intrinsic == QoreIRIntrinsic::StringContains) {
            if (consumer->operands.size() != 2
                    || !method->pseudo_arg0_known_assigned_string) {
                return false;
            }
            pattern_operand = get_pattern_operand(
                producer, consumer->operands[1], cross_block);
            if (pattern_operand < 0) {
                return false;
            }
            kind = intrinsic == QoreIRIntrinsic::StringStartsWith
                ? QoreIRCallDirectInstruction::AOTStringConsumerKind::StartsWith
                : intrinsic == QoreIRIntrinsic::StringEndsWith
                    ? QoreIRCallDirectInstruction::AOTStringConsumerKind::EndsWith
                    : QoreIRCallDirectInstruction::AOTStringConsumerKind::Contains;
        } else if (intrinsic == QoreIRIntrinsic::StringFind
                || intrinsic == QoreIRIntrinsic::StringRFind) {
            if ((consumer->operands.size() != 2
                        && consumer->operands.size() != 3)
                    || !method->pseudo_arg0_known_assigned_string
                    || (consumer->operands.size() == 3
                        && (!method->pseudo_arg1_known_assigned_int
                            || !get_const_int(
                                consumer->operands[2], arg0)))) {
                return false;
            }
            pattern_operand = get_pattern_operand(
                producer, consumer->operands[1], cross_block);
            if (pattern_operand < 0) {
                return false;
            }
            if (consumer->operands.size() == 2) {
                arg0 = intrinsic == QoreIRIntrinsic::StringFind ? 0 : -1;
            }
            kind = intrinsic == QoreIRIntrinsic::StringFind
                ? QoreIRCallDirectInstruction::AOTStringConsumerKind::Find
                : QoreIRCallDirectInstruction::AOTStringConsumerKind::RFind;
        } else if (intrinsic == QoreIRIntrinsic::StringSubstr) {
            if ((consumer->operands.size() != 2
                        && consumer->operands.size() != 3)
                    || !method->pseudo_arg0_known_assigned_int
                    || !get_const_int(consumer->operands[1], arg0)
                    || (consumer->operands.size() == 3
                        && (!method->pseudo_arg1_known_assigned_int
                            || !get_const_int(
                                consumer->operands[2], arg1)))) {
                return false;
            }
            has_arg1 = consumer->operands.size() == 3;
            kind =
                QoreIRCallDirectInstruction::AOTStringConsumerKind::Substr;
        } else {
            return false;
        }
        if (!cross_block && kind
                    != QoreIRCallDirectInstruction::AOTStringConsumerKind::Size
                && kind
                    != QoreIRCallDirectInstruction::AOTStringConsumerKind::Length
                && !side_effect_free_interval(producer, consumer)) {
            return false;
        }
        fusion.producer = producer;
        fusion.consumer = consumer;
        fusion.kind = kind;
        fusion.result = consumer->result;
        fusion.pattern_operand = pattern_operand;
        fusion.arg0 = arg0;
        fusion.arg1 = arg1;
        fusion.has_arg1 = has_arg1;
        return true;
    };
    std::vector<Fusion> fusions;
    for (const auto& block : func.blocks) {
        for (const auto& inst_ptr : block->instructions) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR string producer-consumer analysis")) {
                return 0;
            }
            QoreIRStringConsumerCallInstruction* producer =
                get_string_producer(inst_ptr.get());
            if (!producer) {
                continue;
            }
            auto use = uses.find(producer->result.id);
            if (use == uses.end() || use->second.size() != 1
                    || !use->second.front().inst) {
                continue;
            }
            QoreIRInstruction* consumer =
                const_cast<QoreIRInstruction*>(use->second.front().inst);
            if (consumer->opcode != QoreIROpcode::DotEvalMethodDirect
                    || consumer->exception_target
                    || !consumer->result.isValid()
                    || consumer->operands.empty()
                    || consumer->operands[0].id != producer->result.id) {
                continue;
            }
            bool has_ref_args = true;
            const AbstractQoreFunctionVariant* callee =
                qore_ir_get_resolved_effect_callee(producer, has_ref_args);
            Fusion fusion;
            if (!callee || has_ref_args
                    || !analyze_consumer(producer, producer->result, consumer,
                        false, fusion)
                    || !is_supported(callee, producer, fusion.kind)) {
                continue;
            }
            fusion.eliminated.push_back(consumer);
            fusions.push_back(std::move(fusion));
        }
    }
    for (const auto& block : func.blocks) {
        for (const auto& inst_ptr : block->instructions) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR string producer local analysis")) {
                return 0;
            }
            QoreIRStringConsumerCallInstruction* producer =
                get_string_producer(inst_ptr.get());
            if (!producer) {
                continue;
            }
            auto producer_use = uses.find(producer->result.id);
            if (producer_use == uses.end()
                    || producer_use->second.size() != 1
                    || !producer_use->second.front().inst
                    || producer_use->second.front().inst->opcode
                        != QoreIROpcode::StoreLocal) {
                continue;
            }
            auto* store = static_cast<QoreIRLocalInstruction*>(
                const_cast<QoreIRInstruction*>(
                    producer_use->second.front().inst));
            LocalVar* local = store->local;
            bool has_ref_args = true;
            const AbstractQoreFunctionVariant* callee =
                qore_ir_get_resolved_effect_callee(producer, has_ref_args);
            if (!local || store->weak || !store->initial_assignment
                    || store->operands.size() != 1
                    || store->operands.front().id != producer->result.id
                    || !callee || has_ref_args
                    || callee->getReturnTypeInfo() != local->getTypeInfo()
                    || QoreTypeInfo::parseReturns(
                        local->getTypeInfo(), NT_STRING) != QTI_IDENT
                    || local->closureUse()
                    || QoreTypeInfo::isReference(local->getTypeInfo())
                    || !func.ir_only_locals.count(
                        reinterpret_cast<const void*>(local))) {
                continue;
            }
            auto operations = local_operations.find(local);
            auto producer_pos = positions.find(producer);
            auto store_pos = positions.find(store);
            if (operations == local_operations.end()
                    || producer_pos == positions.end()
                    || store_pos == positions.end()
                    || producer_pos->second.block != store_pos->second.block
                    || producer_pos->second.offset >= store_pos->second.offset) {
                continue;
            }
            if (!cfg) {
                cfg = std::make_unique<QoreIRControlFlowGraph>(func);
                if (cfg->cancelled) {
                    return 0;
                }
            }
            Fusion candidate;
            candidate.producer = producer;
            candidate.eliminated.push_back(store);
            bool valid = true;
            for (const InstructionPosition& operation : operations->second) {
                if (qore_ir_analysis_cancelled(check_count,
                        "IR string producer local operation analysis")) {
                    return 0;
                }
                QoreIRInstruction* local_inst = operation.inst;
                if (local_inst == store
                        || local_inst->opcode
                            == QoreIROpcode::InstantiateLocal
                        || local_inst->opcode
                            == QoreIROpcode::UninstantiateLocal) {
                    continue;
                }
                if (local_inst->opcode != QoreIROpcode::LoadLocal
                        || (operation.block == store_pos->second.block
                            ? operation.offset <= store_pos->second.offset
                            : !cfg->dominates(
                                store_pos->second.block, operation.block))
                        || !local_inst->result.isValid()) {
                    valid = false;
                    break;
                }
                auto load_uses = uses.find(local_inst->result.id);
                if (load_uses == uses.end() || load_uses->second.empty()) {
                    candidate.eliminated.push_back(local_inst);
                    continue;
                }
                Fusion consumer_fusion;
                if (candidate.consumer || load_uses->second.size() != 1
                        || !load_uses->second.front().inst
                        || !analyze_consumer(producer, local_inst->result,
                            const_cast<QoreIRInstruction*>(
                                load_uses->second.front().inst), true,
                            consumer_fusion)
                        || !is_supported(
                            callee, producer, consumer_fusion.kind)) {
                    valid = false;
                    break;
                }
                candidate.consumer = consumer_fusion.consumer;
                candidate.kind = consumer_fusion.kind;
                candidate.result = consumer_fusion.result;
                candidate.pattern_operand =
                    consumer_fusion.pattern_operand;
                candidate.arg0 = consumer_fusion.arg0;
                candidate.arg1 = consumer_fusion.arg1;
                candidate.has_arg1 = consumer_fusion.has_arg1;
                if (candidate.kind
                        == QoreIRCallDirectInstruction::
                            AOTStringConsumerKind::Substr) {
                    candidate.preserve_producer_result = true;
                    candidate.eliminated.clear();
                    candidate.eliminated.push_back(candidate.consumer);
                    candidate.replacements.emplace_back(
                        candidate.result.id, local_inst->result);
                } else {
                    candidate.eliminated.push_back(local_inst);
                    candidate.eliminated.push_back(candidate.consumer);
                }
            }
            if (valid && candidate.consumer) {
                fusions.push_back(std::move(candidate));
            }
        }
    }

    struct PhiFusion {
        QoreIRPhiInstruction* phi = nullptr;
        QoreIRInstruction* consumer = nullptr;
        QoreIRCallDirectInstruction::AOTStringConsumerKind kind =
            QoreIRCallDirectInstruction::AOTStringConsumerKind::None;
        std::unique_ptr<QoreIRPhiInstruction> replacement;
        std::vector<Fusion> producers;
    };
    std::vector<PhiFusion> phi_fusions;
    for (const auto& block : func.blocks) {
        for (const auto& inst_ptr : block->instructions) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR string producer phi analysis")) {
                return 0;
            }
            if (!inst_ptr || inst_ptr->opcode != QoreIROpcode::Phi
                    || !inst_ptr->result.isValid()) {
                continue;
            }
            auto* phi = static_cast<QoreIRPhiInstruction*>(inst_ptr.get());
            if (phi->value_kind != QoreIRPhiValueKind::QoreValue
                    || phi->incoming.empty()) {
                continue;
            }
            auto phi_uses = uses.find(phi->result.id);
            if (phi_uses == uses.end() || phi_uses->second.size() != 1
                    || !phi_uses->second.front().inst) {
                continue;
            }
            PhiFusion candidate;
            candidate.phi = phi;
            candidate.consumer = const_cast<QoreIRInstruction*>(
                phi_uses->second.front().inst);
            auto replacement = std::make_unique<QoreIRPhiInstruction>();
            replacement->loc = candidate.consumer->loc;
            replacement->cached_start_line =
                candidate.consumer->cached_start_line;
            replacement->temp_scope_id = candidate.consumer->temp_scope_id;
            replacement->result = candidate.consumer->result;
            bool valid = true;
            std::unordered_set<QoreIRInstruction*> seen_producers;
            for (const QoreIRPhiIncoming& incoming : phi->incoming) {
                if (qore_ir_analysis_cancelled(check_count,
                        "IR string producer phi incoming analysis")) {
                    return 0;
                }
                auto definition = definitions.find(incoming.value.id);
                if (!incoming.block || definition == definitions.end()) {
                    valid = false;
                    break;
                }
                QoreIRStringConsumerCallInstruction* producer =
                    get_string_producer(definition->second.inst);
                if (!producer) {
                    valid = false;
                    break;
                }
                auto producer_uses = uses.find(producer->result.id);
                bool has_ref_args = true;
                const AbstractQoreFunctionVariant* callee =
                    qore_ir_get_resolved_effect_callee(
                        producer, has_ref_args);
                if (producer->exception_target || !callee || has_ref_args
                        || producer_uses == uses.end()
                        || producer_uses->second.size() != 1
                        || producer_uses->second.front().inst != phi
                        || !seen_producers.insert(producer).second) {
                    valid = false;
                    break;
                }
                Fusion producer_fusion;
                if (!analyze_consumer(producer, phi->result,
                        candidate.consumer, true, producer_fusion)
                        || (candidate.kind
                                != QoreIRCallDirectInstruction::
                                    AOTStringConsumerKind::None
                            && candidate.kind != producer_fusion.kind)
                        || !is_supported(
                            callee, producer, producer_fusion.kind)) {
                    valid = false;
                    break;
                }
                candidate.kind = producer_fusion.kind;
                producer_fusion.result = producer->result;
                producer_fusion.preserve_producer_result = true;
                candidate.producers.push_back(std::move(producer_fusion));
                replacement->incoming.push_back(
                    {producer->result, incoming.block});
                replacement->operands.push_back(producer->result);
            }
            if (candidate.kind
                    != QoreIRCallDirectInstruction::
                        AOTStringConsumerKind::Size
                    && candidate.kind
                        != QoreIRCallDirectInstruction::
                            AOTStringConsumerKind::Length) {
                valid = false;
            } else {
                replacement->value_kind = QoreIRPhiValueKind::NativeInt;
            }
            if (valid) {
                candidate.replacement = std::move(replacement);
                phi_fusions.push_back(std::move(candidate));
            }
        }
    }
    if (fusions.empty() && phi_fusions.empty()) {
        return 0;
    }

    std::unordered_map<QoreIRInstruction*, Fusion*> producers;
    std::unordered_map<QoreIRInstruction*, Fusion*> phi_producers;
    std::unordered_map<QoreIRInstruction*,
        std::unique_ptr<QoreIRPhiInstruction>> phi_replacements;
    std::unordered_set<QoreIRInstruction*> eliminated;
    std::unordered_map<uint32_t, QoreIRValue> replacements;
    for (Fusion& fusion : fusions) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR string producer-consumer rewrite preparation")) {
            return 0;
        }
        producers.emplace(fusion.producer, &fusion);
        eliminated.insert(
            fusion.eliminated.begin(), fusion.eliminated.end());
        replacements.insert(
            fusion.replacements.begin(), fusion.replacements.end());
    }
    for (PhiFusion& fusion : phi_fusions) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR string producer phi rewrite preparation")) {
            return 0;
        }
        for (Fusion& producer : fusion.producers) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR string producer phi call rewrite preparation")) {
                return 0;
            }
            phi_producers.emplace(producer.producer, &producer);
        }
        eliminated.insert(fusion.consumer);
        QoreIRValueFacts facts;
        facts.type_info = bigIntTypeInfo;
        facts.assigned_state = QoreIRAssignedState::Assigned;
        facts.representation = QoreIRValueRepresentation::NativeInt;
        facts.never_nothing = true;
        func.setValueFacts(fusion.replacement->result, facts);
        phi_replacements.emplace(
            fusion.phi, std::move(fusion.replacement));
    }
    auto set_result_facts = [&](QoreIRValue result,
            QoreIRCallDirectInstruction::AOTStringConsumerKind kind) {
        QoreIRValueFacts facts;
        facts.assigned_state = QoreIRAssignedState::Assigned;
        facts.never_nothing = true;
        if (kind
                    == QoreIRCallDirectInstruction::AOTStringConsumerKind::StartsWith
                || kind
                    == QoreIRCallDirectInstruction::AOTStringConsumerKind::EndsWith
                || kind
                    == QoreIRCallDirectInstruction::AOTStringConsumerKind::Contains) {
            facts.type_info = boolTypeInfo;
            facts.representation = QoreIRValueRepresentation::NativeBool;
        } else if (kind
                == QoreIRCallDirectInstruction::AOTStringConsumerKind::Substr) {
            facts.type_info = stringTypeInfo;
            facts.representation = QoreIRValueRepresentation::Boxed;
        } else {
            facts.type_info = bigIntTypeInfo;
            facts.representation = QoreIRValueRepresentation::NativeInt;
        }
        func.setValueFacts(result, facts);
    };
    for (const auto& block : func.blocks) {
        auto& instructions = block->instructions;
        for (auto inst = instructions.begin(); inst != instructions.end();) {
            if (++check_count % 100 == 0) {
                (void)qore_check_cancel(nullptr,
                    "IR string producer-consumer fusion");
            }
            if (eliminated.count(inst->get())) {
                inst = instructions.erase(inst);
                continue;
            }
            (void)qore_ir_rewrite_value_operands(
                **inst, replacements, check_count, false);
            auto phi_replacement = phi_replacements.find(inst->get());
            if (phi_replacement != phi_replacements.end()) {
                *inst = std::move(phi_replacement->second);
                ++inst;
                continue;
            }
            auto producer = producers.find(inst->get());
            if (producer != producers.end()) {
                Fusion& fusion = *producer->second;
                fusion.producer->aot_string_consumer = fusion.kind;
                fusion.producer->aot_string_consumer_pattern_operand =
                    fusion.pattern_operand;
                fusion.producer->aot_string_consumer_arg0 = fusion.arg0;
                fusion.producer->aot_string_consumer_arg1 = fusion.arg1;
                fusion.producer->aot_string_consumer_has_arg1 =
                    fusion.has_arg1;
                if (!fusion.preserve_producer_result) {
                    fusion.producer->result = fusion.result;
                }
                set_result_facts(fusion.producer->result, fusion.kind);
                ++inst;
                continue;
            }
            auto phi_producer = phi_producers.find(inst->get());
            if (phi_producer != phi_producers.end()) {
                Fusion& fusion = *phi_producer->second;
                fusion.producer->aot_string_consumer = fusion.kind;
                fusion.producer->aot_string_consumer_pattern_operand =
                    fusion.pattern_operand;
                fusion.producer->aot_string_consumer_arg0 = fusion.arg0;
                fusion.producer->aot_string_consumer_arg1 = fusion.arg1;
                fusion.producer->aot_string_consumer_has_arg1 =
                    fusion.has_arg1;
                set_result_facts(fusion.producer->result, fusion.kind);
            }
            ++inst;
        }
    }
    return fusions.size() + phi_fusions.size();
}
