/* -*- indent-tabs-mode: nil -*- */
/*
    plugin_registry_smoke.cpp

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.
*/

#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <array>
#include <iostream>
#include <string>
#include <vector>

#include <qore/Qore.h>
#include <qore/QorePluginType.h>
#include <qore/intern/QoreAOTBinary.h>
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
        std::array<QorePluginOperation, 2>& ops) {
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
    ops[0].runtime_helper = reinterpret_cast<void (*)()>(smokeBinary);

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
    ops[1].runtime_helper = reinterpret_cast<void (*)()>(smokeDenseBinary);

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
    std::array<QorePluginOperation, 2> ops;
    QorePluginTypeRegistration reg = smokeRegistration(type, ops);
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
    std::array<QorePluginOperation, 2> ops;
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

    ReferenceHolder<QoreListNode> modules(qore_plugin_get_process_modules(&xsink), &xsink);
    if (xsink || !modules || modules->size() != 1) {
        std::cerr << "process plugin module introspection failed\n";
        return false;
    }

    ReferenceHolder<QoreListNode> types(qore_plugin_get_process_types("plugin-smoke", &xsink), &xsink);
    ReferenceHolder<QoreListNode> reflected_ops(qore_plugin_get_process_operations("plugin-smoke", &xsink), &xsink);
    if (xsink || !types || !reflected_ops || types->size() != 1 || reflected_ops->size() != 2) {
        std::cerr << "process plugin descriptor introspection failed\n";
        return false;
    }

    uint32_t global_id = 0;
    if (qore_plugin_get_process_operation_id("plugin-smoke", 0, &global_id, &xsink) || xsink || !global_id) {
        std::cerr << "process plugin operation id lookup failed\n";
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
