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

#include <algorithm>
#include <bit>
#include <cstdlib>
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
            if (call->invoke_opcode != QoreIROpcode::CallClosureDirect) {
                return nullptr;
            }
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
    struct FunctionEffects {
        const AbstractQoreFunctionVariant* variant = nullptr;
        bool local_may_invalidate = false;
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
        size_t& check_count) {
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
        size_t bound_assignment_block = cfg.blocks.size();
        size_t bound_assignment_offset = 0;

        if (terminator->opcode == QoreIROpcode::BrIf) {
            auto* branch = static_cast<QoreIRBranchIfInstruction*>(terminator);
            true_target = branch->true_target;
            false_target = branch->false_target;
            QoreIRInstruction* condition = get_definition(branch->condition);
            if (!condition || condition->operands.size() != 2
                    || (condition->opcode != QoreIROpcode::LtInt
                        && condition->opcode != QoreIROpcode::LeInt)) {
                continue;
            }
            index_local = get_loaded_local(condition->operands[0]);
            QoreIRValue size_value = condition->operands[1];
            if (condition->opcode == QoreIROpcode::LeInt) {
                QoreIRInstruction* subtract = get_definition(size_value);
                int64_t adjustment = 0;
                if (!subtract || subtract->opcode != QoreIROpcode::SubInt
                        || subtract->operands.size() != 2
                        || !get_const_int(subtract->operands[1], adjustment)
                        || adjustment != 1) {
                    continue;
                }
                size_value = subtract->operands[0];
            }
            QoreIRInstruction* size = get_definition(size_value);
            if (!index_local || !size || size->opcode != QoreIROpcode::ListSize
                    || size->operands.size() != 1) {
                continue;
            }
            list_local = get_raw_loaded_local(size->operands[0]);
            list_size_inst = size;
            list_size_load_inst = get_definition(size->operands[0]);
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

        bool nonnegative_initial_value = false;
        for (const auto& inst_ptr : cfg.blocks[loop.preheader]->instructions) {
            QoreIRInstruction* inst = inst_ptr.get();
            if (inst->opcode == QoreIROpcode::StoreLocal) {
                auto* store = static_cast<QoreIRLocalInstruction*>(inst);
                if (store->local == index_local) {
                    int64_t value = -1;
                    nonnegative_initial_value = inst->operands.size() == 1
                        && get_const_int(inst->operands[0], value) && value >= 0;
                }
            } else if (inst->opcode == QoreIROpcode::IncrementLocalInt
                    && static_cast<QoreIRIncrementLocalIntInstruction*>(inst)->local == index_local) {
                nonnegative_initial_value = false;
            } else if (inst->opcode == QoreIROpcode::AddAssignLocalInt
                    && static_cast<QoreIRAddAssignLocalIntInstruction*>(inst)->target == index_local) {
                nonnegative_initial_value = false;
            }
        }
        if (!nonnegative_initial_value) {
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
        for (size_t block_id : loop.blocks) {
            for (const auto& inst_ptr : cfg.blocks[block_id]->instructions) {
                if (qore_ir_analysis_cancelled(check_count, "IR bounded list mutation analysis")) {
                    return changed;
                }
                QoreIRInstruction* inst = inst_ptr.get();
                if ((qore_ir_may_mutate_unknown_local(inst->opcode)
                        && !is_unrelated_local_lvalue_path(inst))
                        || qore_ir_may_mutate_list(inst->opcode)) {
                    invalidated = true;
                    break;
                }
                if (inst->opcode == QoreIROpcode::IncrementLocalInt) {
                    auto* increment = static_cast<QoreIRIncrementLocalIntInstruction*>(inst);
                    if (increment->local == index_local) {
                        if (increment->delta != 1) {
                            invalidated = true;
                            break;
                        }
                        ++increments;
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
        if (invalidated || increments != 1) {
            continue;
        }

        for (size_t block_id : loop.blocks) {
            if (!cfg.dominates(true_id->second, block_id)) {
                continue;
            }
            for (const auto& inst_ptr : cfg.blocks[block_id]->instructions) {
                QoreIRInstruction& inst = *inst_ptr;
                if (inst.opcode != QoreIROpcode::ListIndexDynamic || inst.operands.size() != 2) {
                    continue;
                }
                auto* expr_inst = static_cast<QoreIRExprInstruction*>(&inst);
                if (!expr_inst->list_selector_kinds.empty()
                        || get_raw_loaded_local(inst.operands[0]) != list_local
                        || get_loaded_local(inst.operands[1]) != index_local) {
                    continue;
                }
                inst.opcode = specialized_opcode;
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

void qore_ir_optimize(QoreIRFunction& func, QoreIROptimizationStats* stats) {
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
    bool has_list_push = false;
    if (!getenv("QORE_DISABLE_IR_IN_PLACE_LIST_PUSH")) {
        for (const QoreIRBasicBlock* block : cfg.blocks) {
            if (qore_ir_analysis_cancelled(check_count, "IR list push discovery")) {
                if (stats) {
                    *stats = local_stats;
                }
                return;
            }
            for (const auto& inst : block->instructions) {
                if (qore_ir_analysis_cancelled(check_count, "IR list push discovery")) {
                    if (stats) {
                        *stats = local_stats;
                    }
                    return;
                }
                if (inst->opcode == QoreIROpcode::ListPush) {
                    has_list_push = true;
                    break;
                }
            }
            if (has_list_push) {
                break;
            }
        }
    }
    QoreIRScalarUses uses;
    if ((!loops.empty() || has_list_push)
            && !qore_ir_collect_scalar_uses(func, uses, check_count)) {
        if (stats) {
            *stats = local_stats;
        }
        return;
    }
    if (!getenv("QORE_DISABLE_IR_BOUNDED_TYPED_LIST_READS")) {
        local_stats.bounded_typed_list_reads = qore_ir_specialize_bounded_typed_list_reads(
            func, cfg, loops, check_count);
    }
    if (!getenv("QORE_DISABLE_IR_BORROWED_LIST_READS")) {
        local_stats.borrowed_list_reads = qore_ir_mark_borrowed_list_reads(
            cfg, loops, uses, check_count);
    }
    if (!getenv("QORE_DISABLE_IR_IN_PLACE_LIST_PUSH")) {
        local_stats.in_place_list_pushes = qore_ir_mark_in_place_list_pushes(
            func, cfg, uses, check_count, nullptr);
    }
    if (getenv("QORE_DISABLE_IR_LICM")) {
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

    struct Fusion {
        QoreIRCallDirectInstruction* producer = nullptr;
        QoreIRInstruction* consumer = nullptr;
        QoreIRCallDirectInstruction::AOTStringConsumerKind kind =
            QoreIRCallDirectInstruction::AOTStringConsumerKind::None;
        QoreIRValue result;
    };
    std::vector<Fusion> fusions;
    for (const auto& block : func.blocks) {
        for (const auto& inst_ptr : block->instructions) {
            if (qore_ir_analysis_cancelled(check_count,
                    "IR string producer-consumer analysis")) {
                return 0;
            }
            if (!inst_ptr || inst_ptr->opcode != QoreIROpcode::CallDirect
                    || inst_ptr->exception_target
                    || !inst_ptr->result.isValid()) {
                continue;
            }
            auto use = uses.find(inst_ptr->result.id);
            if (use == uses.end() || use->second.size() != 1
                    || !use->second.front().inst) {
                continue;
            }
            QoreIRInstruction* consumer =
                const_cast<QoreIRInstruction*>(use->second.front().inst);
            if (consumer->opcode != QoreIROpcode::DotEvalMethodDirect
                    || consumer->exception_target
                    || !consumer->result.isValid()
                    || consumer->operands.size() != 1
                    || consumer->operands[0].id != inst_ptr->result.id) {
                continue;
            }
            const auto* method =
                static_cast<const QoreIRDotEvalMethodDirectInstruction*>(consumer);
            if (!method->pseudo || method->has_ref_args || !method->qc
                    || strcmp(method->qc->getName(), "<string>")) {
                continue;
            }
            QoreIRIntrinsic intrinsic = method->intrinsic;
            if (intrinsic == QoreIRIntrinsic::None) {
                intrinsic = qore_ir_resolve_pseudo_intrinsic(method->method,
                    method->qc, method->fallback_method_name);
            }
            QoreIRCallDirectInstruction::AOTStringConsumerKind kind;
            if (intrinsic == QoreIRIntrinsic::Size
                    || intrinsic == QoreIRIntrinsic::StringStrlen) {
                kind = QoreIRCallDirectInstruction::AOTStringConsumerKind::Size;
            } else if (intrinsic == QoreIRIntrinsic::StringLength) {
                kind = QoreIRCallDirectInstruction::AOTStringConsumerKind::Length;
            } else {
                continue;
            }
            auto* producer =
                static_cast<QoreIRCallDirectInstruction*>(inst_ptr.get());
            bool has_ref_args = true;
            const AbstractQoreFunctionVariant* callee =
                qore_ir_get_resolved_effect_callee(producer, has_ref_args);
            if (!callee || has_ref_args || !is_supported(callee, producer)) {
                continue;
            }
            fusions.push_back({producer, consumer, kind, consumer->result});
        }
    }
    if (fusions.empty()) {
        return 0;
    }

    std::unordered_map<QoreIRInstruction*, Fusion*> producers;
    std::unordered_set<QoreIRInstruction*> consumers;
    for (Fusion& fusion : fusions) {
        if (qore_ir_analysis_cancelled(check_count,
                "IR string producer-consumer rewrite preparation")) {
            return 0;
        }
        producers.emplace(fusion.producer, &fusion);
        consumers.insert(fusion.consumer);
    }
    for (const auto& block : func.blocks) {
        auto& instructions = block->instructions;
        for (auto inst = instructions.begin(); inst != instructions.end();) {
            if (++check_count % 100 == 0) {
                (void)qore_check_cancel(nullptr,
                    "IR string producer-consumer fusion");
            }
            if (consumers.count(inst->get())) {
                inst = instructions.erase(inst);
                continue;
            }
            auto producer = producers.find(inst->get());
            if (producer == producers.end()) {
                ++inst;
                continue;
            }
            Fusion& fusion = *producer->second;
            fusion.producer->aot_string_consumer = fusion.kind;
            fusion.producer->result = fusion.result;
            QoreIRValueFacts facts;
            facts.type_info = bigIntTypeInfo;
            facts.assigned_state = QoreIRAssignedState::Assigned;
            facts.representation = QoreIRValueRepresentation::NativeInt;
            facts.never_nothing = true;
            func.setValueFacts(fusion.result, facts);
            ++inst;
        }
    }
    return fusions.size();
}
