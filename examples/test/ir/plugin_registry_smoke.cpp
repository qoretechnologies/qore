/* -*- indent-tabs-mode: nil -*- */
/*
    plugin_registry_smoke.cpp

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.
*/

#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <algorithm>
#include <array>
#include <iostream>
#include <string>
#include <vector>

#include <qore/Qore.h>
#include <qore/QoreBufferNode.h>
#include <qore/QorePluginLLVM.h>
#include <qore/QorePluginType.h>
#include <qore/intern/QoreAOTBinary.h>
#include <qore/intern/QoreIRLowering.h>
#include <qore/intern/QoreIRVerifier.h>
#include <qore/intern/QorePluginRegistry.h>
#include <qore/intern/QoreParseHashNode.h>
#include <qore/intern/QorePlusOperatorNode.h>
#include <qore/intern/QoreSerializable.h>

static void smokeIncref(uint64_t) noexcept {
}

static void smokeDecref(uint64_t) noexcept {
}

static uint64_t smokeClone(uint64_t value, ExceptionSink*) {
    return value;
}

static bool smokeEqual(uint64_t lhs, uint64_t rhs, ExceptionSink*) {
    return lhs == rhs;
}

static int64_t smokeHash(uint64_t value, ExceptionSink*) {
    return static_cast<int64_t>(value);
}

static void smokeCleanup(uint64_t) noexcept {
}

static int smokeSerialize(uint64_t value_bits, QorePluginByteWriteCallback write, void* write_user_data,
        ExceptionSink* xsink) {
    uint8_t bytes[sizeof(uint64_t)];
    for (size_t i = 0; i < sizeof(bytes); ++i) {
        bytes[i] = static_cast<uint8_t>((value_bits >> (i * 8)) & 0xff);
    }
    return write(bytes, sizeof(bytes), write_user_data, xsink);
}

static uint64_t smokeDeserialize(QorePluginByteReadCallback read, uint32_t payload_len, void* read_user_data,
        ExceptionSink* xsink) {
    if (payload_len != sizeof(uint64_t)) {
        if (xsink) {
            xsink->raiseException("PLUGIN-SMOKE-DESERIALIZATION-ERROR",
                "plugin smoke payload length must be 8 bytes, got %u", payload_len);
        }
        return 0;
    }
    uint8_t bytes[sizeof(uint64_t)] = {};
    if (read(bytes, sizeof(bytes), read_user_data, xsink)) {
        return 0;
    }
    uint64_t value_bits = 0;
    for (size_t i = 0; i < sizeof(bytes); ++i) {
        value_bits |= static_cast<uint64_t>(bytes[i]) << (i * 8);
    }
    return value_bits;
}

static QoreValue smokeValueFromBits(uint64_t bits) {
    QoreValue v;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}

static uint64_t smokeBitsFromValue(const QoreValue& v) {
    uint64_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    return bits;
}

class ScopedEnvVar {
public:
    ScopedEnvVar(const char* name, const char* value) : name(name) {
        const char* old = std::getenv(name);
        if (old) {
            old_value = old;
            old_set = true;
        }
        setenv(name, value, 1);
    }

    ~ScopedEnvVar() {
        if (old_set) {
            setenv(name.c_str(), old_value.c_str(), 1);
        } else {
            unsetenv(name.c_str());
        }
    }

private:
    std::string name;
    std::string old_value;
    bool old_set = false;
};

static uint64_t smokeBinary(uint64_t lhs, uint64_t rhs, ExceptionSink*) {
    QoreValue lhs_value = smokeValueFromBits(lhs);
    QoreValue rhs_value = smokeValueFromBits(rhs);
    return smokeBitsFromValue(QoreValue(lhs_value.getAsBigInt() + rhs_value.getAsBigInt()));
}

static uint64_t smokeBadReturnType(uint64_t, uint64_t, ExceptionSink*) {
    return smokeBitsFromValue(QoreValue(true));
}

static uint64_t smokeDenseUnary(void* result_buffer_data, int64_t result_size, const void* value_data,
        int64_t value_size, int64_t value_stride, ExceptionSink* xsink) {
    int64_t* result = static_cast<int64_t*>(result_buffer_data);
    const int64_t* value = static_cast<const int64_t*>(value_data);
    int64_t n = std::min(result_size, value_size);
    for (int64_t i = 0; i < n; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "dense unary plugin smoke helper")) {
            return smokeBitsFromValue(QoreValue());
        }
        result[i] = value[i * value_stride] + 1;
    }
    return smokeBitsFromValue(QoreValue());
}

static llvm::Value* smokeLLVMCodegen(QorePluginLLVMCodegenContext* ctx) {
    if (!ctx || ctx->struct_size < sizeof(QorePluginLLVMCodegenContext)
            || ctx->abi_version != QORE_PLUGIN_LLVM_EXTENSION_ABI_VERSION || !ctx->qore_value_type) {
        if (ctx && ctx->error_message) {
            *ctx->error_message = "invalid smoke LLVM codegen context";
        }
        return nullptr;
    }
    return llvm::ConstantInt::get(ctx->qore_value_type, smokeBitsFromValue(QoreValue(static_cast<int64>(4242))));
}

static QorePluginLLVMExtension smokeLLVMExtension = {
    sizeof(QorePluginLLVMExtension),
    QORE_PLUGIN_LLVM_EXTENSION_ABI_VERSION,
    QORE_PLUGIN_LLVM_CURRENT_MAJOR,
    smokeLLVMCodegen,
};

static QorePluginExtension smokeOperationExtensions[] = {
    { QORE_PLUGIN_LLVM_CODEGEN_EXTENSION_ID, &smokeLLVMExtension, false },
};

static int smokeLoweringCallCount = 0;

static QorePluginLoweringResult smokeOperatorLowering(QoreIRLoweringContext* ctx, const AbstractQoreNode* ast_node,
        const QoreParseContext*, QoreIRBuilder* builder) {
    if (!ctx || !ast_node || !builder || ast_node->getType() != NT_OPERATOR) {
        return QorePluginLoweringResult::NotApplicable;
    }

    ++smokeLoweringCallCount;
    QoreIRConstInstruction* lowered = builder->createConstInt(4242);
    ctx->setResult(lowered->result);
    return QorePluginLoweringResult::Lowered;
}

static QorePluginLoweringResult smokeClaimViolationLowering(QoreIRLoweringContext*, const AbstractQoreNode*,
        const QoreParseContext*, QoreIRBuilder*) {
    return QorePluginLoweringResult::NotApplicable;
}

static uint64_t smokeDenseBinary(void* result_buffer_data, int64_t result_size, const void* lhs_data,
        int64_t lhs_size, int64_t lhs_stride, const void* rhs_data, int64_t rhs_size, int64_t rhs_stride,
        ExceptionSink* xsink) {
    int64_t* result = static_cast<int64_t*>(result_buffer_data);
    const int64_t* lhs = static_cast<const int64_t*>(lhs_data);
    const int64_t* rhs = static_cast<const int64_t*>(rhs_data);
    int64_t n = std::min(result_size, std::min(lhs_size, rhs_size));
    for (int64_t i = 0; i < n; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "dense plugin smoke helper")) {
            return smokeBitsFromValue(QoreValue());
        }
        result[i] = lhs[i * lhs_stride] + rhs[i * rhs_stride];
    }
    return smokeBitsFromValue(QoreValue());
}

static QorePluginValueOps smokeValueOps() {
    QorePluginValueOps ops = {};
    ops.incref = smokeIncref;
    ops.decref = smokeDecref;
    ops.clone = smokeClone;
    ops.equal = smokeEqual;
    ops.hash = smokeHash;
    ops.cleanup_slot = smokeCleanup;
    return ops;
}

static QorePluginTypeRegistration smokeRegistration(QorePluginTypeDescriptor& type,
        std::array<QorePluginOperation, 5>& ops) {
    type = {};
    type.local_type_id = 0;
    type.type_name = "SmokeDense";
    type.type_info = autoTypeInfo;
    type.value_ops = smokeValueOps();
    type.serialize = smokeSerialize;
    type.deserialize = smokeDeserialize;
    type.serializer_format_version = 1;

    ops[0] = {};
    ops[0].local_id = 0;
    ops[0].operation_name = "add";
    ops[0].signature.arity = 2;
    ops[0].signature.primary_type = autoTypeInfo;
    ops[0].signature.secondary_type = autoTypeInfo;
    ops[0].signature.return_type = autoTypeInfo;
    ops[0].signature.access = QorePluginValueAccess::ReadOnly;
    ops[0].signature.result_alias = QorePluginResultAlias::FreshNoAliasInputs;
    ops[0].signature.helper_abi = QorePluginHelperAbi::BinaryValue;
    ops[0].info.is_commutative = true;
    ops[0].info.is_associative = true;
    ops[0].info.is_pure_modulo_xsink = true;
    ops[0].info.fp_reassociation_allowed = true;
    ops[0].runtime_helper = reinterpret_cast<void (*)()>(smokeBinary);
    ops[0].lowering_pattern = smokeOperatorLowering;
    ops[0].lowering_claimed_node_kinds = 1ULL << NT_OPERATOR;
    ops[0].extensions = smokeOperationExtensions;
    ops[0].num_extensions = 1;

    ops[1] = {};
    ops[1].local_id = 1;
    ops[1].operation_name = "dense_add";
    ops[1].signature.arity = 2;
    ops[1].signature.primary_type = autoTypeInfo;
    ops[1].signature.secondary_type = autoTypeInfo;
    ops[1].signature.return_type = autoTypeInfo;
    ops[1].signature.access = QorePluginValueAccess::ReadOnly;
    ops[1].signature.result_alias = QorePluginResultAlias::FreshNoAliasInputs;
    ops[1].signature.helper_abi = QorePluginHelperAbi::DenseBufferBinary;
    ops[1].info.is_pure_modulo_xsink = true;
    ops[1].info.can_vectorize = true;
    ops[1].runtime_helper = reinterpret_cast<void (*)()>(smokeDenseBinary);

    ops[2] = {};
    ops[2].local_id = 2;
    ops[2].operation_name = "bad_alias";
    ops[2].signature.arity = 2;
    ops[2].signature.primary_type = autoTypeInfo;
    ops[2].signature.secondary_type = autoTypeInfo;
    ops[2].signature.return_type = autoTypeInfo;
    ops[2].signature.access = QorePluginValueAccess::ReadOnly;
    ops[2].signature.result_alias = QorePluginResultAlias::ReturnsLhs;
    ops[2].signature.helper_abi = QorePluginHelperAbi::BinaryValue;
    ops[2].runtime_helper = reinterpret_cast<void (*)()>(smokeBinary);

    ops[3] = {};
    ops[3].local_id = 3;
    ops[3].operation_name = "bad_return_type";
    ops[3].signature.arity = 2;
    ops[3].signature.primary_type = autoTypeInfo;
    ops[3].signature.secondary_type = autoTypeInfo;
    ops[3].signature.return_type = bigIntTypeInfo;
    ops[3].signature.access = QorePluginValueAccess::ReadOnly;
    ops[3].signature.result_alias = QorePluginResultAlias::FreshNoAliasInputs;
    ops[3].signature.helper_abi = QorePluginHelperAbi::BinaryValue;
    ops[3].runtime_helper = reinterpret_cast<void (*)()>(smokeBadReturnType);

    ops[4] = {};
    ops[4].local_id = 4;
    ops[4].operation_name = "dense_inc";
    ops[4].signature.arity = 1;
    ops[4].signature.primary_type = autoTypeInfo;
    ops[4].signature.return_type = autoTypeInfo;
    ops[4].signature.access = QorePluginValueAccess::ReadOnly;
    ops[4].signature.result_alias = QorePluginResultAlias::FreshNoAliasInputs;
    ops[4].signature.helper_abi = QorePluginHelperAbi::DenseBufferUnary;
    ops[4].runtime_helper = reinterpret_cast<void (*)()>(smokeDenseUnary);
    ops[4].lowering_pattern = smokeClaimViolationLowering;
    ops[4].lowering_claimed_node_kinds = 1ULL << NT_PARSE_HASH;

    QorePluginTypeRegistration reg = {};
    reg.module_name = "plugin-smoke";
    reg.plugin_abi_version = QORE_PLUGIN_ABI_VERSION_V1;
    reg.operation_set_version = "1.0.0";
    reg.types = &type;
    reg.num_types = 1;
    reg.operations = ops.data();
    reg.num_operations = static_cast<int>(ops.size());
    return reg;
}

static bool checkDryRunValidation() {
    ExceptionSink xsink;
    QorePluginTypeDescriptor type;
    std::array<QorePluginOperation, 5> ops;
    QorePluginTypeRegistration reg = smokeRegistration(type, ops);
    QorePluginValidationContext ctx = {};
    ctx.struct_size = sizeof(ctx);
    if (qore_validate_plugin_types_v1(&reg, &ctx, true, &xsink) || xsink) {
        std::cerr << "valid plugin descriptor failed dry-run validation\n";
        return false;
    }

    ExceptionSink bad_xsink;
    QorePluginLLVMExtension mismatched_llvm_extension = smokeLLVMExtension;
    mismatched_llvm_extension.llvm_major_version = QORE_PLUGIN_LLVM_CURRENT_MAJOR + 1;
    QorePluginExtension mismatched_extension = {
        QORE_PLUGIN_LLVM_CODEGEN_EXTENSION_ID,
        &mismatched_llvm_extension,
        false,
    };
    ops[0].extensions = &mismatched_extension;
    ops[0].num_extensions = 1;
    if (qore_validate_plugin_types_v1(&reg, &ctx, true, &bad_xsink) || bad_xsink) {
        std::cerr << "optional mismatched LLVM codegen extension failed dry-run validation\n";
        return false;
    }
    mismatched_extension.required = true;
    if (!qore_validate_plugin_types_v1(&reg, &ctx, false, &bad_xsink) || !bad_xsink) {
        std::cerr << "required mismatched LLVM codegen extension passed dry-run validation\n";
        return false;
    }
    bad_xsink.clear();
    ops[0].extensions = smokeOperationExtensions;
    ops[0].num_extensions = 1;

    QorePluginTypeDescriptor bad_type = type;
    bad_type.value_ops.clone = nullptr;
    QorePluginTypeRegistration bad = reg;
    bad.types = &bad_type;
    if (!qore_validate_plugin_types_v1(&bad, &ctx, false, &bad_xsink) || !bad_xsink) {
        std::cerr << "invalid plugin descriptor passed dry-run validation\n";
        return false;
    }
    bad_xsink.clear();

    QorePluginValidationContext old_ctx = {};
    old_ctx.struct_size = offsetof(QorePluginValidationContext, module_handle);
    if (qore_validate_plugin_types_v1(&reg, &old_ctx, true, &bad_xsink) || bad_xsink) {
        std::cerr << "old-size plugin validation context failed dry-run validation\n";
        return false;
    }

    QorePluginValidationContext small_ctx = {};
    small_ctx.struct_size = offsetof(QorePluginValidationContext, module_handle) - 1;
    if (!qore_validate_plugin_types_v1(&reg, &small_ctx, false, &bad_xsink) || !bad_xsink) {
        std::cerr << "too-small plugin validation context passed dry-run validation\n";
        return false;
    }
    bad_xsink.clear();

    QorePluginValidationContext flag_ctx = {};
    flag_ctx.struct_size = sizeof(flag_ctx);
    flag_ctx.flags = QORE_PLUGIN_VALIDATE_RESERVED_MASK;
    if (!qore_validate_plugin_types_v1(&reg, &flag_ctx, false, &bad_xsink) || !bad_xsink) {
        std::cerr << "unknown plugin validation context flag passed dry-run validation\n";
        return false;
    }
    bad_xsink.clear();
    return true;
}

static bool checkLoweringCallbacks() {
    int before = smokeLoweringCallCount;
    ValueHolder expr(QoreValue(new QorePlusOperatorNode(nullptr, QoreValue(1), QoreValue(2))), nullptr);
    QoreIRFunction func("plugin-lowering-smoke");
    QoreIRBuilder builder(&func);
    QoreIRBasicBlock* entry = func.createBlock("entry");
    builder.setBlock(entry);
    QoreIRLowering lowering(builder);
    std::string error;
    QoreIRValue lowered = lowering.lowerExpression(*expr, error);
    if (!lowered.isValid() || !error.empty() || smokeLoweringCallCount != before + 1) {
        std::cerr << "plugin lowering callback was not used: " << error << "\n";
        return false;
    }
    builder.createReturn(lowered);
    if (!QoreIRVerifier::verify(func, error)) {
        std::cerr << "plugin lowering callback produced invalid IR: " << error << "\n";
        return false;
    }
    if (entry->instructions.empty() || entry->instructions.front()->opcode != QoreIROpcode::ConstInt
            || entry->instructions.front()->result.id != lowered.id) {
        std::cerr << "plugin lowering callback did not provide the lowered result\n";
        return false;
    }

    ValueHolder hash_expr(QoreValue(new QoreParseHashNode(nullptr, true)), nullptr);
    QoreIRFunction violation_func("plugin-lowering-claim-violation-smoke");
    QoreIRBuilder violation_builder(&violation_func);
    QoreIRBasicBlock* violation_entry = violation_func.createBlock("entry");
    violation_builder.setBlock(violation_entry);
    QoreIRLowering violation_lowering(violation_builder);
    std::string violation_error;
    QoreIRValue violation_result = violation_lowering.lowerExpression(*hash_expr, violation_error);
    if (violation_result.isValid()
            || violation_error.find("PLUGIN-LOWERING-CLAIM-VIOLATED") == std::string::npos) {
        std::cerr << "plugin lowering claim violation was not reported: " << violation_error << "\n";
        return false;
    }

    return true;
}

static bool checkRegistrationAndIntrospection() {
    ExceptionSink xsink;
    QorePluginTypeDescriptor type;
    std::array<QorePluginOperation, 5> ops;
    QorePluginTypeRegistration reg = smokeRegistration(type, ops);

    QorePluginModuleHandle handle("plugin-smoke", "/tmp/plugin-smoke.qmod", nullptr);
    {
        QorePluginModuleInitScope scope(handle);
        QorePluginRegistrationContextV1 ctx = {};
        ctx.struct_size = sizeof(ctx);
        ctx.module_path = "/tmp/plugin-smoke.qmod";
        ctx.module_handle = scope.getHandle();

        if (qore_register_plugin_types_v1(&ctx, &reg, &xsink) || xsink) {
            std::cerr << "plugin registration failed\n";
            return false;
        }
        scope.commit(&xsink);
        if (xsink) {
            std::cerr << "plugin registration commit failed\n";
            return false;
        }
    }

    if (!checkLoweringCallbacks()) {
        return false;
    }

    ReferenceHolder<QoreListNode> modules(qore_plugin_get_process_modules(&xsink), &xsink);
    if (xsink || !modules || modules->size() != 1) {
        std::cerr << "process plugin module introspection failed\n";
        return false;
    }

    ReferenceHolder<QoreListNode> types(qore_plugin_get_process_types("plugin-smoke", &xsink), &xsink);
    ReferenceHolder<QoreListNode> reflected_ops(qore_plugin_get_process_operations("plugin-smoke", &xsink), &xsink);
    if (xsink || !types || !reflected_ops || types->size() != 1 || reflected_ops->size() != ops.size()) {
        std::cerr << "process plugin descriptor introspection failed\n";
        return false;
    }

    uint32_t global_id = 0;
    if (qore_plugin_get_process_operation_id("plugin-smoke", 0, &global_id, &xsink) || xsink || !global_id) {
        std::cerr << "process plugin operation id lookup failed\n";
        return false;
    }
    QorePluginLLVMCodegenInfo codegen_info;
    if (qore_plugin_get_llvm_codegen_info(global_id, codegen_info, &xsink) || xsink || !codegen_info.codegen
            || codegen_info.local_operation_id != 0 || !codegen_info.info.fp_reassociation_allowed) {
        std::cerr << "process plugin LLVM codegen lookup failed\n";
        return false;
    }
    llvm::LLVMContext llvm_context;
    llvm::Module llvm_module("plugin-smoke-codegen", llvm_context);
    llvm::IRBuilder<> llvm_builder(llvm_context);
    llvm::FunctionType* llvm_function_type = llvm::FunctionType::get(llvm_builder.getVoidTy(), false);
    llvm::Function* llvm_function = llvm::Function::Create(llvm_function_type, llvm::Function::ExternalLinkage,
        "smoke", llvm_module);
    llvm::BasicBlock* llvm_block = llvm::BasicBlock::Create(llvm_context, "entry", llvm_function);
    llvm_builder.SetInsertPoint(llvm_block);
    const char* codegen_error = nullptr;
    QorePluginLLVMCodegenContext codegen_ctx = {};
    codegen_ctx.struct_size = sizeof(codegen_ctx);
    codegen_ctx.abi_version = QORE_PLUGIN_LLVM_EXTENSION_ABI_VERSION;
    codegen_ctx.global_operation_id = global_id;
    codegen_ctx.helper_abi = codegen_info.signature.helper_abi;
    codegen_ctx.signature = &codegen_info.signature;
    codegen_ctx.llvm_context = &llvm_context;
    codegen_ctx.builder = &llvm_builder;
    codegen_ctx.module = &llvm_module;
    codegen_ctx.function = llvm_function;
    codegen_ctx.qore_value_type = llvm_builder.getInt64Ty();
    codegen_ctx.pointer_type = llvm_builder.getPtrTy();
    codegen_ctx.error_message = &codegen_error;
    llvm::Value* codegen_value = codegen_info.codegen(&codegen_ctx);
    if (!codegen_value || codegen_error || !llvm::isa<llvm::ConstantInt>(codegen_value)
            || llvm::cast<llvm::ConstantInt>(codegen_value)->getZExtValue()
                != smokeBitsFromValue(QoreValue(static_cast<int64>(4242)))) {
        std::cerr << "process plugin LLVM codegen callback returned the wrong value\n";
        return false;
    }
    uint64_t signature_hash = qore_plugin_compute_signature_hash_v1(ops[0].signature);
    uint32_t checked_global_id = 0;
    if (qore_plugin_get_process_operation_id_checked("plugin-smoke", 0,
            QORE_PLUGIN_CANONICAL_SIGNATURE_VERSION_V1, signature_hash, &checked_global_id, &xsink)
            || xsink || checked_global_id != global_id) {
        std::cerr << "checked process plugin operation id lookup failed\n";
        return false;
    }

    QoreValue op_value = reflected_ops->retrieveEntry(0);
    const QoreHashNode* op_hash = op_value.get<const QoreHashNode>();
    if (!op_hash || op_hash->getKeyValue("global_id").getAsBigInt() != static_cast<int64>(global_id)) {
        std::cerr << "process plugin operation introspection did not expose global_id\n";
        return false;
    }
    if (op_hash->getKeyValue("signature_hash").getAsBigInt() != static_cast<int64>(signature_hash)) {
        std::cerr << "process plugin operation introspection did not expose signature_hash\n";
        return false;
    }
    if (!op_hash->getKeyValue("is_associative").getAsBool()
            || !op_hash->getKeyValue("fp_reassociation_allowed").getAsBool()) {
        std::cerr << "process plugin operation introspection did not expose FP reassociation metadata\n";
        return false;
    }

    ReferenceHolder<QoreHashNode> resolved(qore_plugin_resolve_process_operation(bigIntTypeInfo, bigIntTypeInfo,
        "add", &xsink), &xsink);
    if (xsink || !resolved || resolved->getKeyValue("global_id").getAsBigInt() != static_cast<int64>(global_id)) {
        std::cerr << "process plugin operation resolution failed\n";
        return false;
    }
    QorePluginResolvedOperationInfo resolved_info;
    if (qore_plugin_resolve_program_operation_info(nullptr, bigIntTypeInfo, bigIntTypeInfo, "add",
            QorePluginHelperAbi::BinaryValue, resolved_info, &xsink) || xsink
            || qore_plugin_allows_fp_reassociation(resolved_info, QoreParseOptions())
            || !qore_plugin_allows_fp_reassociation(resolved_info, QoreParseOptions::FP_FAST_MATH)) {
        std::cerr << "process plugin FP reassociation gate failed\n";
        return false;
    }
    ReferenceHolder<QoreHashNode> missing_resolved(qore_plugin_resolve_process_operation(bigIntTypeInfo,
        bigIntTypeInfo, "missing_operation", &xsink), &xsink);
    if (xsink || missing_resolved) {
        std::cerr << "missing process plugin operation resolution did not return NOTHING\n";
        return false;
    }

    ReferenceHolder<QoreProgram> inactive_pgm(new QoreProgram, &xsink);
    ReferenceHolder<QoreListNode> inactive_modules(qore_plugin_get_program_modules(*inactive_pgm, &xsink), &xsink);
    ReferenceHolder<QoreListNode> inactive_types(qore_plugin_get_program_types(*inactive_pgm, "plugin-smoke",
        &xsink), &xsink);
    ReferenceHolder<QoreListNode> inactive_ops(qore_plugin_get_program_operations(*inactive_pgm, "plugin-smoke",
        &xsink), &xsink);
    ReferenceHolder<QoreHashNode> inactive_resolved(qore_plugin_resolve_program_operation(*inactive_pgm,
        bigIntTypeInfo, bigIntTypeInfo, "add", &xsink), &xsink);
    if (xsink || !inactive_modules || !inactive_types || !inactive_ops || inactive_modules->size()
            || inactive_types->size() || inactive_ops->size() || inactive_resolved) {
        std::cerr << "inactive program plugin registry filtering failed\n";
        return false;
    }

    ReferenceHolder<QoreProgram> active_pgm(new QoreProgram, &xsink);
    active_pgm->addFeature("plugin-smoke");
    ReferenceHolder<QoreListNode> active_modules(qore_plugin_get_program_modules(*active_pgm, &xsink), &xsink);
    ReferenceHolder<QoreListNode> active_types(qore_plugin_get_program_types(*active_pgm, "plugin-smoke", &xsink),
        &xsink);
    ReferenceHolder<QoreListNode> active_ops(qore_plugin_get_program_operations(*active_pgm, "plugin-smoke",
        &xsink), &xsink);
    ReferenceHolder<QoreHashNode> active_resolved(qore_plugin_resolve_program_operation(*active_pgm,
        bigIntTypeInfo, bigIntTypeInfo, "add", &xsink), &xsink);
    if (xsink || !active_modules || !active_types || !active_ops || !active_resolved
            || active_modules->size() != 1 || active_types->size() != 1 || active_ops->size() != ops.size()
            || active_resolved->getKeyValue("global_id").getAsBigInt() != static_cast<int64>(global_id)) {
        std::cerr << "active program plugin registry filtering failed\n";
        return false;
    }

    {
        ScopedEnvVar fallback_capacity("QORE_PLUGIN_FALLBACK_BUFFER", "1024");
        qore_plugin_record_fallback_site(*active_pgm, "plugin-smoke.q", 17, "add", bigIntTypeInfo, bigIntTypeInfo,
            "smoke fallback");
        ReferenceHolder<QoreListNode> fallback_sites(qore_plugin_get_recent_fallback_sites(*active_pgm, &xsink),
            &xsink);
        if (xsink || !fallback_sites || fallback_sites->size() != 1) {
            std::cerr << "plugin fallback-site recording failed\n";
            return false;
        }
        const QoreHashNode* fallback_hash = fallback_sites->retrieveEntry(0).get<const QoreHashNode>();
        const QoreStringNode* fallback_operation = fallback_hash
            ? fallback_hash->getKeyValue("operation_name").get<const QoreStringNode>()
            : nullptr;
        const QoreStringNode* fallback_reason = fallback_hash
            ? fallback_hash->getKeyValue("reason").get<const QoreStringNode>()
            : nullptr;
        if (!fallback_hash || !fallback_operation || !fallback_reason
                || fallback_hash->getKeyValue("line").getAsBigInt() != 17
                || std::strcmp(fallback_operation->c_str(), "add")
                || std::strcmp(fallback_reason->c_str(), "smoke fallback")) {
            std::cerr << "plugin fallback-site introspection fields are wrong\n";
            return false;
        }
        qore_plugin_clear_fallback_sites(*active_pgm);
        ReferenceHolder<QoreListNode> cleared_fallback_sites(qore_plugin_get_recent_fallback_sites(*active_pgm,
            &xsink), &xsink);
        if (xsink || !cleared_fallback_sites || cleared_fallback_sites->size()) {
            std::cerr << "plugin fallback-site clearing failed\n";
            return false;
        }
    }

    {
        ScopedEnvVar disabled_fallback("QORE_PLUGIN_FALLBACK_BUFFER", "0");
        qore_plugin_record_fallback_site(*active_pgm, "plugin-smoke.q", 18, "add", bigIntTypeInfo, bigIntTypeInfo,
            "disabled fallback");
        ReferenceHolder<QoreListNode> disabled_sites(qore_plugin_get_recent_fallback_sites(*active_pgm, &xsink),
            &xsink);
        if (xsink || !disabled_sites || disabled_sites->size()) {
            std::cerr << "plugin fallback-site disabled buffer failed\n";
            return false;
        }
    }

    {
        ScopedEnvVar one_fallback("QORE_PLUGIN_FALLBACK_BUFFER", "1");
        qore_plugin_record_fallback_site(*active_pgm, "plugin-smoke.q", 19, "add", bigIntTypeInfo, bigIntTypeInfo,
            "first fallback");
        qore_plugin_record_fallback_site(*active_pgm, "plugin-smoke.q", 20, "add", bigIntTypeInfo, bigIntTypeInfo,
            "second fallback");
        ReferenceHolder<QoreListNode> one_sites(qore_plugin_get_recent_fallback_sites(*active_pgm, &xsink),
            &xsink);
        const QoreHashNode* one_hash = one_sites && one_sites->size() == 1
            ? one_sites->retrieveEntry(0).get<const QoreHashNode>()
            : nullptr;
        const QoreStringNode* one_reason = one_hash
            ? one_hash->getKeyValue("reason").get<const QoreStringNode>()
            : nullptr;
        if (xsink || !one_hash || !one_reason || one_hash->getKeyValue("line").getAsBigInt() != 20
                || std::strcmp(one_reason->c_str(), "second fallback")) {
            std::cerr << "plugin fallback-site rolling buffer failed\n";
            return false;
        }
        qore_plugin_clear_fallback_sites(*active_pgm);
    }

    ExceptionSink mismatch_xsink;
    if (!qore_plugin_get_process_operation_id_checked("plugin-smoke", 0,
            QORE_PLUGIN_CANONICAL_SIGNATURE_VERSION_V1, signature_hash ^ 1u, nullptr, &mismatch_xsink)
            || !mismatch_xsink) {
        std::cerr << "checked process plugin operation id lookup accepted a signature mismatch\n";
        return false;
    }
    mismatch_xsink.clear();

    QoreAOTBinaryWriter writer;
    if (!writer.addPluginOperationRef("plugin-smoke", 0, QORE_PLUGIN_CANONICAL_SIGNATURE_VERSION_V1,
            signature_hash)) {
        std::cerr << "failed to add plugin operation ref to AOT writer\n";
        return false;
    }
    uint64_t dense_signature_hash = qore_plugin_compute_signature_hash_v1(ops[1].signature);
    if (!writer.addPluginOperationRef("plugin-smoke", 1, QORE_PLUGIN_CANONICAL_SIGNATURE_VERSION_V1,
            dense_signature_hash)) {
        std::cerr << "failed to add dense plugin operation ref to AOT writer\n";
        return false;
    }
    std::string section_error;
    if (!writer.writePluginSections(section_error)) {
        std::cerr << "failed to write plugin QORD sections: " << section_error << "\n";
        return false;
    }
    QoreAOTBinaryHeader hdr = {};
    hdr.magic = QORE_AOT_BINARY_MAGIC;
    hdr.version = QORE_AOT_BINARY_VERSION;
    hdr.label_offset = writer.strings.add("plugin-smoke-qord");
    hdr.max_opcode_id = 0;
    hdr.qore_version_major = QORE_VERSION_MAJOR;
    hdr.qore_version_minor = QORE_VERSION_MINOR;
    hdr.qore_version_patch = QORE_VERSION_PATCH;
    std::vector<uint8_t> blob;
    if (!writer.finalize(hdr, blob)) {
        std::cerr << "failed to finalize plugin QORD smoke blob\n";
        return false;
    }
    QoreAOTBinaryReader reader;
    std::string read_error;
    if (!reader.open(blob.data(), static_cast<uint32_t>(blob.size()), read_error)) {
        std::cerr << "failed to read plugin QORD smoke blob: " << read_error << "\n";
        return false;
    }
    if (!reader.findSection(QoreAOTSectionType::PLUGIN_IMPORTS)
            || !reader.findSection(QoreAOTSectionType::PLUGIN_TYPE_REGISTRY)
            || !reader.findSection(QoreAOTSectionType::PLUGIN_HELPER_REFS)) {
        std::cerr << "plugin QORD smoke blob is missing plugin sections\n";
        return false;
    }

    const uint64_t plugin_value_bits = 0x1122334455667788ULL;
    ValueHolder plugin_value(qore_plugin_make_value_v1("plugin-smoke", 0, plugin_value_bits,
        QORE_PLUGIN_VALUE_ADOPT, &xsink), &xsink);
    if (xsink || !*plugin_value || plugin_value->getType() != NT_PLUGIN_VALUE) {
        std::cerr << "plugin value node creation failed\n";
        return false;
    }
    const char* plugin_value_module = nullptr;
    uint16_t plugin_value_type_id = 0xffffu;
    uint64_t extracted_value_bits = 0;
    if (qore_plugin_get_value_bits_v1(plugin_value->getInternalNode(), &plugin_value_module, &plugin_value_type_id,
            &extracted_value_bits, &xsink) || xsink || !plugin_value_module
            || std::strcmp(plugin_value_module, "plugin-smoke") || plugin_value_type_id != 0
            || extracted_value_bits != plugin_value_bits) {
        std::cerr << "plugin value bit extraction failed\n";
        return false;
    }

    QoreAOTBinaryWriter value_writer;
    uint32_t value_sec = value_writer.beginSection(QoreAOTSectionType::PROGRAM_METADATA);
    if (!value_writer.writeValue(*plugin_value)) {
        std::cerr << "failed to write plugin value instance\n";
        return false;
    }
    value_writer.endSection(value_sec);
    std::string value_section_error;
    if (!value_writer.writePluginSections(value_section_error)) {
        std::cerr << "failed to write plugin value QORD sections: " << value_section_error << "\n";
        return false;
    }
    QoreAOTBinaryHeader value_hdr = {};
    value_hdr.magic = QORE_AOT_BINARY_MAGIC;
    value_hdr.version = QORE_AOT_BINARY_VERSION;
    value_hdr.label_offset = value_writer.strings.add("plugin-value-qord");
    value_hdr.max_opcode_id = 0;
    value_hdr.qore_version_major = QORE_VERSION_MAJOR;
    value_hdr.qore_version_minor = QORE_VERSION_MINOR;
    value_hdr.qore_version_patch = QORE_VERSION_PATCH;
    std::vector<uint8_t> value_blob;
    if (!value_writer.finalize(value_hdr, value_blob)) {
        std::cerr << "failed to finalize plugin value QORD smoke blob\n";
        return false;
    }
    QoreAOTBinaryReader value_reader;
    std::string value_read_error;
    if (!value_reader.open(value_blob.data(), static_cast<uint32_t>(value_blob.size()), value_read_error)) {
        std::cerr << "failed to read plugin value QORD smoke blob: " << value_read_error << "\n";
        return false;
    }
    const QoreAOTSectionHeader* serialized_value_sec = value_reader.findSection(QoreAOTSectionType::PROGRAM_METADATA);
    if (!serialized_value_sec) {
        std::cerr << "plugin value QORD smoke blob is missing value section\n";
        return false;
    }
    const uint8_t* value_ptr = value_reader.getSectionData(*serialized_value_sec);
    const uint8_t* value_end = value_ptr + serialized_value_sec->size;
    ValueHolder decoded_plugin_value(value_reader.readValue(value_ptr, value_end, value_read_error), &xsink);
    if (!value_read_error.empty() || value_ptr != value_end || !*decoded_plugin_value
            || decoded_plugin_value->getType() != NT_PLUGIN_VALUE) {
        std::cerr << "failed to read plugin value instance: " << value_read_error << "\n";
        return false;
    }
    uint64_t decoded_value_bits = 0;
    if (qore_plugin_get_value_bits_v1(decoded_plugin_value->getInternalNode(), nullptr, nullptr,
            &decoded_value_bits, &xsink) || xsink || decoded_value_bits != plugin_value_bits) {
        std::cerr << "plugin value QORD round trip produced the wrong value bits\n";
        return false;
    }

    ReferenceHolder<QoreHashNode> serialized_plugin_value(QoreSerializable::serializeToData(*plugin_value, 0, &xsink),
        &xsink);
    if (xsink || !serialized_plugin_value) {
        std::cerr << "plugin value Serializable serialization failed\n";
        return false;
    }
    ValueHolder deserialized_plugin_value(QoreSerializable::deserialize(&xsink, **serialized_plugin_value, 0),
        &xsink);
    uint64_t deserialized_value_bits = 0;
    if (xsink || !*deserialized_plugin_value || deserialized_plugin_value->getType() != NT_PLUGIN_VALUE
            || qore_plugin_get_value_bits_v1(deserialized_plugin_value->getInternalNode(), nullptr, nullptr,
                &deserialized_value_bits, &xsink) || xsink || deserialized_value_bits != plugin_value_bits) {
        std::cerr << "plugin value Serializable round trip produced the wrong value bits\n";
        return false;
    }

    uint64_t result_bits = qore_rt_plugin_binary(global_id,
        smokeBitsFromValue(QoreValue(static_cast<int64>(40))),
        smokeBitsFromValue(QoreValue(static_cast<int64>(2))), &xsink);
    QoreValue result = smokeValueFromBits(result_bits);
    if (xsink || result.getAsBigInt() != 42) {
        std::cerr << "plugin runtime binary dispatch failed\n";
        return false;
    }

    uint32_t dense_global_id = 0;
    if (qore_plugin_get_process_operation_id("plugin-smoke", 1, &dense_global_id, &xsink)
            || xsink || !dense_global_id) {
        std::cerr << "process plugin dense operation id lookup failed\n";
        return false;
    }
    int64_t lhs[] = {1, 2, 3};
    int64_t rhs[] = {10, 20, 30};
    int64_t dense_result[] = {0, 0, 0};
    qore_rt_plugin_dense_buffer_binary(dense_global_id, dense_result, 3, lhs, 3, 1, rhs, 3, 1, &xsink);
    if (xsink || dense_result[0] != 11 || dense_result[1] != 22 || dense_result[2] != 33) {
        std::cerr << "plugin runtime dense-buffer binary dispatch failed\n";
        return false;
    }

    uint32_t dense_unary_global_id = 0;
    if (qore_plugin_get_process_operation_id("plugin-smoke", 4, &dense_unary_global_id, &xsink)
            || xsink || !dense_unary_global_id) {
        std::cerr << "process plugin dense unary operation id lookup failed\n";
        return false;
    }
    int64_t unary_input[] = {7, 8, 9};
    int64_t unary_result[] = {0, 0, 0};
    qore_rt_plugin_dense_buffer_unary(dense_unary_global_id, unary_result, 3, unary_input, 3, 1, &xsink);
    if (xsink || unary_result[0] != 8 || unary_result[1] != 9 || unary_result[2] != 10) {
        std::cerr << "plugin runtime dense-buffer unary dispatch failed\n";
        return false;
    }

    ReferenceHolder<QoreBufferNode> lhs_buf(new QoreBufferNode(QoreBufferElementType::Int64, false, 3), &xsink);
    ReferenceHolder<QoreBufferNode> rhs_buf(new QoreBufferNode(QoreBufferElementType::Int64, false, 3), &xsink);
    ReferenceHolder<QoreBufferNode> result_buf(new QoreBufferNode(QoreBufferElementType::Int64, false, 3), &xsink);
    for (size_t i = 0; i < 3; ++i) {
        if (lhs_buf->setEntry(i, QoreValue(static_cast<int64>(i + 1)), &xsink)
                || rhs_buf->setEntry(i, QoreValue(static_cast<int64>((i + 1) * 10)), &xsink)
                || result_buf->setEntry(i, QoreValue(static_cast<int64>(0)), &xsink)
                || xsink) {
            std::cerr << "failed to initialize dense-buffer value dispatch smoke buffers\n";
            return false;
        }
    }
    qore_rt_plugin_dense_buffer_binary_values(dense_global_id, smokeBitsFromValue(QoreValue(*result_buf)),
        smokeBitsFromValue(QoreValue(*lhs_buf)), smokeBitsFromValue(QoreValue(*rhs_buf)), &xsink);
    if (xsink || result_buf->getReferencedEntry(0).getAsBigInt() != 11
            || result_buf->getReferencedEntry(1).getAsBigInt() != 22
            || result_buf->getReferencedEntry(2).getAsBigInt() != 33) {
        std::cerr << "plugin runtime dense-buffer binary QoreValue dispatch failed\n";
        return false;
    }

    ReferenceHolder<QoreBufferNode> unary_value_buf(new QoreBufferNode(QoreBufferElementType::Int64, false, 3),
        &xsink);
    ReferenceHolder<QoreBufferNode> unary_result_buf(new QoreBufferNode(QoreBufferElementType::Int64, false, 3),
        &xsink);
    for (size_t i = 0; i < 3; ++i) {
        if (unary_value_buf->setEntry(i, QoreValue(static_cast<int64>(i + 7)), &xsink)
                || unary_result_buf->setEntry(i, QoreValue(static_cast<int64>(0)), &xsink)
                || xsink) {
            std::cerr << "failed to initialize dense-buffer unary value dispatch smoke buffers\n";
            return false;
        }
    }
    qore_rt_plugin_dense_buffer_unary_values(dense_unary_global_id,
        smokeBitsFromValue(QoreValue(*unary_result_buf)), smokeBitsFromValue(QoreValue(*unary_value_buf)), &xsink);
    if (xsink || unary_result_buf->getReferencedEntry(0).getAsBigInt() != 8
            || unary_result_buf->getReferencedEntry(1).getAsBigInt() != 9
            || unary_result_buf->getReferencedEntry(2).getAsBigInt() != 10) {
        std::cerr << "plugin runtime dense-buffer unary QoreValue dispatch failed\n";
        return false;
    }

    ExceptionSink dense_alias_xsink;
    qore_rt_plugin_dense_buffer_binary_values(dense_global_id, smokeBitsFromValue(QoreValue(*lhs_buf)),
        smokeBitsFromValue(QoreValue(*lhs_buf)), smokeBitsFromValue(QoreValue(*rhs_buf)), &dense_alias_xsink);
    if (!dense_alias_xsink) {
        std::cerr << "plugin runtime dense-buffer QoreValue dispatch accepted result/input aliasing\n";
        return false;
    }
    dense_alias_xsink.clear();

    ReferenceHolder<QoreBufferNode> nullable_buf(new QoreBufferNode(QoreBufferElementType::Int64, true, 3), &xsink);
    ExceptionSink nullable_xsink;
    qore_rt_plugin_dense_buffer_binary_values(dense_global_id, smokeBitsFromValue(QoreValue(*result_buf)),
        smokeBitsFromValue(QoreValue(*nullable_buf)), smokeBitsFromValue(QoreValue(*rhs_buf)), &nullable_xsink);
    if (!nullable_xsink) {
        std::cerr << "plugin runtime dense-buffer QoreValue dispatch accepted nullable input storage\n";
        return false;
    }
    nullable_xsink.clear();

    ReferenceHolder<QoreBufferNode> int32_buf(new QoreBufferNode(QoreBufferElementType::Int32, false, 3), &xsink);
    ExceptionSink element_type_xsink;
    qore_rt_plugin_dense_buffer_binary_values(dense_global_id, smokeBitsFromValue(QoreValue(*result_buf)),
        smokeBitsFromValue(QoreValue(*lhs_buf)), smokeBitsFromValue(QoreValue(*int32_buf)), &element_type_xsink);
    if (!element_type_xsink) {
        std::cerr << "plugin runtime dense-buffer QoreValue dispatch accepted mismatched element storage\n";
        return false;
    }
    element_type_xsink.clear();

    setenv("QORE_PLUGIN_VERIFY", "1", 1);
    uint32_t bad_alias_global_id = 0;
    if (qore_plugin_get_process_operation_id("plugin-smoke", 2, &bad_alias_global_id, &xsink)
            || xsink || !bad_alias_global_id) {
        std::cerr << "process plugin bad-alias operation id lookup failed\n";
        return false;
    }
    ExceptionSink alias_xsink;
    qore_rt_plugin_binary(bad_alias_global_id, smokeBitsFromValue(QoreValue(4)),
        smokeBitsFromValue(QoreValue(5)), &alias_xsink);
    if (!alias_xsink) {
        std::cerr << "plugin runtime verifier did not reject alias contract violation\n";
        return false;
    }
    alias_xsink.clear();

    uint32_t bad_return_global_id = 0;
    if (qore_plugin_get_process_operation_id("plugin-smoke", 3, &bad_return_global_id, &xsink)
            || xsink || !bad_return_global_id) {
        std::cerr << "process plugin bad-return operation id lookup failed\n";
        return false;
    }
    ExceptionSink return_xsink;
    qore_rt_plugin_binary(bad_return_global_id, smokeBitsFromValue(QoreValue(4)),
        smokeBitsFromValue(QoreValue(5)), &return_xsink);
    if (!return_xsink) {
        std::cerr << "plugin runtime verifier did not reject result type mismatch\n";
        return false;
    }
    return_xsink.clear();

    ExceptionSink missing_xsink;
    ReferenceHolder<QoreListNode> missing(qore_plugin_get_process_types("missing-plugin", &missing_xsink),
        &missing_xsink);
    if (!missing_xsink) {
        std::cerr << "missing plugin lookup did not raise an exception\n";
        return false;
    }
    missing_xsink.clear();
    return true;
}

int main() {
    qore_init(QL_GPL);
    bool ok = checkDryRunValidation() && checkRegistrationAndIntrospection();
    qore_cleanup();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
