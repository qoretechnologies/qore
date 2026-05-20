/* -*- indent-tabs-mode: nil -*- */
/*
    plugin_registry_smoke.cpp

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.
*/

#include <cstdlib>
#include <cstring>
#include <iostream>

#include <qore/Qore.h>
#include <qore/QorePluginType.h>
#include <qore/intern/QorePluginRegistry.h>

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

static int smokeSerialize(uint64_t, QorePluginByteWriteCallback, void*, ExceptionSink*) {
    return 0;
}

static uint64_t smokeDeserialize(QorePluginByteReadCallback, uint32_t, void*, ExceptionSink*) {
    return 0;
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

static uint64_t smokeBinary(uint64_t lhs, uint64_t rhs, ExceptionSink*) {
    QoreValue lhs_value = smokeValueFromBits(lhs);
    QoreValue rhs_value = smokeValueFromBits(rhs);
    return smokeBitsFromValue(QoreValue(lhs_value.getAsBigInt() + rhs_value.getAsBigInt()));
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

static QorePluginTypeRegistration smokeRegistration(QorePluginTypeDescriptor& type, QorePluginOperation& op) {
    type = {};
    type.local_type_id = 0;
    type.type_name = "SmokeDense";
    type.type_info = autoTypeInfo;
    type.value_ops = smokeValueOps();
    type.serialize = smokeSerialize;
    type.deserialize = smokeDeserialize;
    type.serializer_format_version = 1;

    op = {};
    op.local_id = 0;
    op.operation_name = "add";
    op.signature.arity = 2;
    op.signature.primary_type = autoTypeInfo;
    op.signature.secondary_type = autoTypeInfo;
    op.signature.return_type = autoTypeInfo;
    op.signature.access = QorePluginValueAccess::ReadOnly;
    op.signature.result_alias = QorePluginResultAlias::FreshNoAliasInputs;
    op.signature.helper_abi = QorePluginHelperAbi::BinaryValue;
    op.runtime_helper = reinterpret_cast<void (*)()>(smokeBinary);

    QorePluginTypeRegistration reg = {};
    reg.module_name = "plugin-smoke";
    reg.plugin_abi_version = QORE_PLUGIN_ABI_VERSION_V1;
    reg.operation_set_version = "1.0.0";
    reg.types = &type;
    reg.num_types = 1;
    reg.operations = &op;
    reg.num_operations = 1;
    return reg;
}

static bool checkDryRunValidation() {
    ExceptionSink xsink;
    QorePluginTypeDescriptor type;
    QorePluginOperation op;
    QorePluginTypeRegistration reg = smokeRegistration(type, op);
    QorePluginValidationContext ctx = {};
    ctx.struct_size = sizeof(ctx);
    if (qore_validate_plugin_types_v1(&reg, &ctx, true, &xsink) || xsink) {
        std::cerr << "valid plugin descriptor failed dry-run validation\n";
        return false;
    }

    QorePluginTypeDescriptor bad_type = type;
    bad_type.value_ops.clone = nullptr;
    QorePluginTypeRegistration bad = reg;
    bad.types = &bad_type;
    ExceptionSink bad_xsink;
    if (!qore_validate_plugin_types_v1(&bad, &ctx, false, &bad_xsink) || !bad_xsink) {
        std::cerr << "invalid plugin descriptor passed dry-run validation\n";
        return false;
    }
    bad_xsink.clear();
    return true;
}

static bool checkRegistrationAndIntrospection() {
    ExceptionSink xsink;
    QorePluginTypeDescriptor type;
    QorePluginOperation op;
    QorePluginTypeRegistration reg = smokeRegistration(type, op);

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

    ReferenceHolder<QoreListNode> modules(qore_plugin_get_process_modules(&xsink), &xsink);
    if (xsink || !modules || modules->size() != 1) {
        std::cerr << "process plugin module introspection failed\n";
        return false;
    }

    ReferenceHolder<QoreListNode> types(qore_plugin_get_process_types("plugin-smoke", &xsink), &xsink);
    ReferenceHolder<QoreListNode> ops(qore_plugin_get_process_operations("plugin-smoke", &xsink), &xsink);
    if (xsink || !types || !ops || types->size() != 1 || ops->size() != 1) {
        std::cerr << "process plugin descriptor introspection failed\n";
        return false;
    }

    uint32_t global_id = 0;
    if (qore_plugin_get_process_operation_id("plugin-smoke", 0, &global_id, &xsink) || xsink || !global_id) {
        std::cerr << "process plugin operation id lookup failed\n";
        return false;
    }
    uint64_t signature_hash = qore_plugin_compute_signature_hash_v1(op.signature);
    uint32_t checked_global_id = 0;
    if (qore_plugin_get_process_operation_id_checked("plugin-smoke", 0,
            QORE_PLUGIN_CANONICAL_SIGNATURE_VERSION_V1, signature_hash, &checked_global_id, &xsink)
            || xsink || checked_global_id != global_id) {
        std::cerr << "checked process plugin operation id lookup failed\n";
        return false;
    }

    QoreValue op_value = ops->retrieveEntry(0);
    const QoreHashNode* op_hash = op_value.get<const QoreHashNode>();
    if (!op_hash || op_hash->getKeyValue("global_id").getAsBigInt() != static_cast<int64>(global_id)) {
        std::cerr << "process plugin operation introspection did not expose global_id\n";
        return false;
    }
    if (op_hash->getKeyValue("signature_hash").getAsBigInt() != static_cast<int64>(signature_hash)) {
        std::cerr << "process plugin operation introspection did not expose signature_hash\n";
        return false;
    }

    ExceptionSink mismatch_xsink;
    if (!qore_plugin_get_process_operation_id_checked("plugin-smoke", 0,
            QORE_PLUGIN_CANONICAL_SIGNATURE_VERSION_V1, signature_hash ^ 1u, nullptr, &mismatch_xsink)
            || !mismatch_xsink) {
        std::cerr << "checked process plugin operation id lookup accepted a signature mismatch\n";
        return false;
    }
    mismatch_xsink.clear();

    uint64_t result_bits = qore_rt_plugin_binary(global_id,
        smokeBitsFromValue(QoreValue(static_cast<int64>(40))),
        smokeBitsFromValue(QoreValue(static_cast<int64>(2))), &xsink);
    QoreValue result = smokeValueFromBits(result_bits);
    if (xsink || result.getAsBigInt() != 42) {
        std::cerr << "plugin runtime binary dispatch failed\n";
        return false;
    }

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
