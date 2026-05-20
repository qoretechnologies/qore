/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QorePluginRegistry.cpp

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.
*/

#include <qore/Qore.h>
#include <qore/QorePluginType.h>
#include <qore/QoreReflection.h>
#include <qore/intern/QorePluginRegistry.h>
#include <qore/intern/xxhash.h>
#include <qore/intern/qore_list_private.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

thread_local const QorePluginModuleHandle* current_plugin_module_handle = nullptr;
std::atomic<uint64_t> plugin_handle_generation{1};

struct RegisteredPluginExtension {
    std::string extension_id;
    const void* extension_data = nullptr;
    bool required = false;
};

struct RegisteredPluginType {
    uint16_t local_type_id = 0;
    std::string type_name;
    const QoreTypeInfo* type_info = nullptr;
    QorePluginValueOps value_ops = {};
    QorePluginSerializeCallback serialize = nullptr;
    QorePluginDeserializeCallback deserialize = nullptr;
    uint16_t serializer_format_version = 0;
    int64_t baseline_qdom_domains = 0;
};

struct RegisteredPluginOperation {
    uint32_t global_id = 0;
    uint16_t local_id = 0;
    std::string operation_name;
    QorePluginOperationSignature signature = {};
    uint8_t canonical_signature_version = QORE_PLUGIN_CANONICAL_SIGNATURE_VERSION_V1;
    uint64_t signature_hash = 0;
    QorePluginOpcodeInfoExtended info = {};
    void (*runtime_helper)() = nullptr;
    std::string runtime_helper_symbol;
    QorePluginLoweringCallback lowering_pattern = nullptr;
    uint64_t lowering_claimed_node_kinds = 0;
    int64_t qdom_domains = 0;
    std::vector<RegisteredPluginExtension> extensions;
};

struct RegisteredPluginDependency {
    std::string module_name;
    std::string min_plugin_abi_version;
    std::string min_operation_set_version;
};

struct RegisteredPluginModule {
    std::string module_name;
    std::string module_path;
    std::string plugin_abi_version;
    std::string operation_set_version;
    std::vector<RegisteredPluginType> types;
    std::vector<RegisteredPluginOperation> operations;
    std::vector<RegisteredPluginDependency> dependencies;
    bool pending = false;
};

std::mutex plugin_registry_mutex;
std::map<std::string, RegisteredPluginModule> plugin_modules;
std::map<const QorePluginModuleHandle*, RegisteredPluginModule> pending_plugin_modules;
struct GlobalPluginOperationRef {
    std::string module_name;
    uint16_t local_operation_id = 0;
    size_t operation_index = 0;
};
std::map<uint32_t, GlobalPluginOperationRef> global_plugin_operations;
uint32_t next_global_plugin_operation_id = 1;

bool pluginRegisterTrace() {
    return std::getenv("QORE_PLUGIN_REGISTER_TRACE") != nullptr;
}

bool pluginDispatchTrace() {
    return std::getenv("QORE_PLUGIN_DISPATCH_TRACE") != nullptr;
}

void traceRegister(const std::string& msg) {
    if (pluginRegisterTrace()) {
        std::fprintf(stderr, "QORE_PLUGIN_REGISTER_TRACE: %s\n", msg.c_str());
    }
}

void traceDispatch(const std::string& msg) {
    if (pluginDispatchTrace()) {
        std::fprintf(stderr, "QORE_PLUGIN_DISPATCH_TRACE: %s\n", msg.c_str());
    }
}

uint64_t nothingBits() {
    QoreValue v;
    uint64_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    return bits;
}

QoreValue valueFromBits(uint64_t bits) {
    QoreValue v;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}

uint64_t bitsFromValue(const QoreValue& v) {
    uint64_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    return bits;
}

std::string nameOrUnknown(const char* name) {
    return name && *name ? name : "<unknown>";
}

std::string escapeDiagnosticName(const char* name) {
    if (!name) {
        return "<null>";
    }

    std::string rv;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(name); *p; ++p) {
        unsigned char c = *p;
        if (c < 0x20 || c == 0x7f || c == '"' || c == '\'' || c == '\\') {
            char buf[5];
            std::snprintf(buf, sizeof(buf), "\\x%02X", c);
            rv += buf;
        } else {
            rv.push_back(static_cast<char>(c));
        }
    }
    return rv;
}

bool checkPluginRegistryCancel(int i, ExceptionSink* xsink, const char* operation) {
    return i && !(i % 100) && qore_check_cancel(xsink, operation);
}

bool hasException(ExceptionSink* xsink) {
    return xsink && *xsink;
}

void appendU8(std::vector<uint8_t>& out, uint8_t v) {
    out.push_back(v);
}

void appendU32LE(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xff));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xff));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xff));
}

void appendTypeRef(std::vector<uint8_t>& out, const QoreTypeInfo* ti) {
    if (!ti) {
        appendU8(out, 0);
        return;
    }
    appendU8(out, 1);
    const char* path = qore_type_get_path(ti);
    uint32_t len = path ? static_cast<uint32_t>(std::strlen(path)) : 0;
    appendU32LE(out, len);
    if (len) {
        out.insert(out.end(), reinterpret_cast<const uint8_t*>(path),
            reinterpret_cast<const uint8_t*>(path) + len);
    }
}

bool validExtensionId(const char* id) {
    if (!id || !*id) {
        return false;
    }

    bool need_label_char = true;
    for (const char* p = id; *p; ++p) {
        char c = *p;
        if (c == '.') {
            if (need_label_char) {
                return false;
            }
            need_label_char = true;
            continue;
        }
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
            return false;
        }
        need_label_char = false;
    }
    return !need_label_char;
}

const char* helperAbiName(QorePluginHelperAbi abi) {
    switch (abi) {
        case QorePluginHelperAbi::UnaryValue: return "UnaryValue";
        case QorePluginHelperAbi::BinaryValue: return "BinaryValue";
        case QorePluginHelperAbi::CallValueList: return "CallValueList";
        case QorePluginHelperAbi::SubscriptValue: return "SubscriptValue";
        case QorePluginHelperAbi::Construct: return "Construct";
        case QorePluginHelperAbi::DenseBufferUnary: return "DenseBufferUnary";
        case QorePluginHelperAbi::DenseBufferBinary: return "DenseBufferBinary";
    }
    return "<unknown>";
}

const char* valueAccessName(QorePluginValueAccess access) {
    switch (access) {
        case QorePluginValueAccess::ReadOnly: return "ReadOnly";
        case QorePluginValueAccess::MutatesLhs: return "MutatesLhs";
        case QorePluginValueAccess::MutatesRhs: return "MutatesRhs";
        case QorePluginValueAccess::MutatesBoth: return "MutatesBoth";
    }
    return "<unknown>";
}

const char* resultAliasName(QorePluginResultAlias alias) {
    switch (alias) {
        case QorePluginResultAlias::Unknown: return "Unknown";
        case QorePluginResultAlias::MayAliasInputs: return "MayAliasInputs";
        case QorePluginResultAlias::FreshNoAliasInputs: return "FreshNoAliasInputs";
        case QorePluginResultAlias::ReturnsLhs: return "ReturnsLhs";
        case QorePluginResultAlias::ReturnsRhs: return "ReturnsRhs";
    }
    return "<unknown>";
}

using PluginUnaryHelper = uint64_t (*)(uint64_t, ExceptionSink*);
using PluginBinaryHelper = uint64_t (*)(uint64_t, uint64_t, ExceptionSink*);
using PluginCallHelper = uint64_t (*)(uint64_t, uint64_t, ExceptionSink*);
using PluginConstructHelper = uint64_t (*)(uint64_t, ExceptionSink*);
using PluginDenseBufferUnaryHelper = uint64_t (*)(void*, int64_t, const void*, int64_t, int64_t, ExceptionSink*);
using PluginDenseBufferBinaryHelper = uint64_t (*)(void*, int64_t, const void*, int64_t, int64_t,
    const void*, int64_t, int64_t, ExceptionSink*);

struct ResolvedPluginOperation {
    uint32_t global_id = 0;
    std::string module_name;
    std::string operation_name;
    QorePluginOperationSignature signature = {};
    void (*runtime_helper)() = nullptr;
};

QoreValue makePluginArgListFromBits(const uint64_t* arg_bits, int32_t nargs, ExceptionSink* xsink) {
    if (nargs < 0 || (nargs && !arg_bits)) {
        if (xsink) {
            xsink->raiseException("PLUGIN-HELPER-ABI-MISMATCH",
                "cannot build plugin argument list: field=\"args\", expected=\"%s\", actual=\"%s\", "
                "subreason=\"helper_abi_mismatch\", section=3.5",
                nargs < 0 ? "non-negative argument count" : "non-null argument array",
                nargs < 0 ? "negative argument count" : "null argument array");
        }
        return QoreValue();
    }
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
    qore_list_private* priv = qore_list_private::get(**args);
    priv->reserve(nargs > 0 ? static_cast<size_t>(nargs) : 0);
    for (int32_t i = 0; i < nargs; ++i) {
        if (checkPluginRegistryCancel(i, xsink, "plugin runtime argument list construction")) {
            return QoreValue();
        }
        QoreValue arg = valueFromBits(arg_bits[i]);
        if (arg.hasNode()) {
            arg.refSelf();
        }
        priv->pushIntern(arg);
    }
    return QoreValue(args.release());
}

bool validateDenseBufferFrame(const char* helper_name, void* result_buffer_data, int64_t result_size,
        const void* lhs_data, int64_t lhs_size, const void* rhs_data, int64_t rhs_size, ExceptionSink* xsink) {
    if (result_size < 0 || lhs_size < 0 || rhs_size < 0) {
        if (xsink) {
            xsink->raiseException("PLUGIN-HELPER-ABI-MISMATCH",
                "cannot dispatch %s: dense-buffer sizes must be non-negative "
                "(field=\"size\", expected=\"non-negative\", actual=\"negative\", "
                "subreason=\"helper_abi_mismatch\", section=3.5)",
                helper_name);
        }
        return true;
    }
    if ((result_size && !result_buffer_data) || (lhs_size && !lhs_data) || (rhs_size && !rhs_data)) {
        if (xsink) {
            xsink->raiseException("PLUGIN-HELPER-ABI-MISMATCH",
                "cannot dispatch %s: dense-buffer data pointers must be non-null for non-empty buffers "
                "(field=\"data\", expected=\"non-null data for non-empty buffers\", actual=\"null\", "
                "subreason=\"helper_abi_mismatch\", section=3.5)",
                helper_name);
        }
        return true;
    }
    return false;
}

uint64_t computeSignatureHashV1(const QorePluginOperationSignature& signature) {
    std::vector<uint8_t> data;
    data.reserve(128);
    appendU8(data, QORE_PLUGIN_CANONICAL_SIGNATURE_VERSION_V1);
    appendU8(data, signature.arity);
    appendU8(data, static_cast<uint8_t>(signature.helper_abi));
    appendU8(data, static_cast<uint8_t>(signature.access));
    appendU8(data, static_cast<uint8_t>(signature.result_alias));
    appendU8(data, signature.primary_nullable ? 1 : 0);
    appendU8(data, signature.secondary_nullable ? 1 : 0);
    appendU8(data, signature.return_nullable ? 1 : 0);
    appendTypeRef(data, signature.primary_type);
    appendTypeRef(data, signature.arity == 2 ? signature.secondary_type : nullptr);
    appendTypeRef(data, signature.return_type);
    return XXH64(data.data(), data.size(), 0);
}

bool resolvePluginOperation(uint32_t global_id, QorePluginHelperAbi expected_abi,
        ResolvedPluginOperation& out, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lock(plugin_registry_mutex);
    auto gi = global_plugin_operations.find(global_id);
    if (gi == global_plugin_operations.end()) {
        if (xsink) {
            xsink->raiseException("PLUGIN-HELPER-ABI-MISMATCH",
                "cannot dispatch plugin operation: global_operation_id=%u is not registered "
                "(field=\"global_operation_id\", expected=\"registered operation id\", actual=\"missing\", "
                "subreason=\"operation_not_registered\", section=3.5)",
                global_id);
        }
        return true;
    }

    auto mi = plugin_modules.find(gi->second.module_name);
    if (mi == plugin_modules.end()) {
        if (xsink) {
            xsink->raiseException("PLUGIN-HELPER-ABI-MISMATCH",
                "cannot dispatch plugin operation: module=\"%s\" for global_operation_id=%u is not loaded "
                "(field=\"module_name\", expected=\"loaded plugin module\", actual=\"missing\", "
                "subreason=\"module_not_loaded\", section=3.5)",
                escapeDiagnosticName(gi->second.module_name.c_str()).c_str(), global_id);
        }
        return true;
    }

    const RegisteredPluginOperation* op = nullptr;
    if (gi->second.operation_index < mi->second.operations.size()) {
        const RegisteredPluginOperation& candidate = mi->second.operations[gi->second.operation_index];
        if (candidate.local_id == gi->second.local_operation_id) {
            op = &candidate;
        }
    }
    if (!op) {
        if (xsink) {
            xsink->raiseException("PLUGIN-HELPER-ABI-MISMATCH",
                "cannot dispatch plugin operation: module=\"%s\", local_id=%u is not registered "
                "(field=\"local_operation_id\", expected=\"registered operation id\", actual=\"missing\", "
                "subreason=\"operation_not_registered\", section=3.5)",
                escapeDiagnosticName(mi->second.module_name.c_str()).c_str(), gi->second.local_operation_id);
        }
        return true;
    }
    if (op->signature.helper_abi != expected_abi) {
        if (xsink) {
            xsink->raiseException("PLUGIN-HELPER-ABI-MISMATCH",
                "cannot dispatch plugin operation: module=\"%s\", operation=\"%s\", field=\"helper_abi\", "
                "expected=\"%s\", actual=\"%s\", subreason=\"helper_abi_mismatch\", section=3.5",
                escapeDiagnosticName(mi->second.module_name.c_str()).c_str(),
                escapeDiagnosticName(op->operation_name.c_str()).c_str(),
                helperAbiName(expected_abi), helperAbiName(op->signature.helper_abi));
        }
        return true;
    }
    if (!op->runtime_helper) {
        if (xsink) {
            xsink->raiseException("PLUGIN-REGISTRATION-HELPER-SYMBOL-MISSING",
                "cannot dispatch plugin operation: module=\"%s\", operation=\"%s\", field=\"runtime_helper\", "
                "expected=\"resolved helper pointer\", actual=\"null\", subreason=\"helper_symbol_not_found\", "
                "section=3.5",
                escapeDiagnosticName(mi->second.module_name.c_str()).c_str(),
                escapeDiagnosticName(op->operation_name.c_str()).c_str());
        }
        return true;
    }

    out.global_id = global_id;
    out.module_name = mi->second.module_name;
    out.operation_name = op->operation_name;
    out.signature = op->signature;
    out.runtime_helper = op->runtime_helper;
    return false;
}

struct PluginValidationState {
    ExceptionSink* xsink = nullptr;
    bool collect_all = false;
    bool failed = false;

    bool fail(const char* code, const char* module, const char* item, const char* field, const char* expected,
            const char* actual, const char* subreason, const char* section) {
        failed = true;
        if (xsink) {
            xsink->raiseException(code, "plugin registration validation failed: module=\"%s\", item=\"%s\", "
                "field=\"%s\", expected=\"%s\", actual=\"%s\", subreason=\"%s\", section=%s",
                escapeDiagnosticName(nameOrUnknown(module).c_str()).c_str(),
                escapeDiagnosticName(nameOrUnknown(item).c_str()).c_str(),
                field ? field : "<unknown>",
                expected ? expected : "<unspecified>",
                actual ? actual : "<unspecified>",
                subreason ? subreason : "<unspecified>",
                section ? section : "design-pending/plugin-types-and-dense-data.md#3.3");
        }
        return !collect_all;
    }
};

bool checkCountAndArray(PluginValidationState& state, const char* module, const char* field, int count,
        const void* ptr) {
    if (count < 0) {
        state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", module, nullptr, field,
            "non-negative count", std::to_string(count).c_str(), "negative_count", "3.3");
        return true;
    }
    if (count > 65536) {
        state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", module, nullptr, field,
            "count <= 65536", std::to_string(count).c_str(), "count_out_of_range", "3.3");
        return true;
    }
    if (count > 0 && !ptr) {
        state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", module, nullptr, field,
            "non-null pointer when count is positive", "null", "null_array", "3.3");
        return true;
    }
    return false;
}

bool validateRegistrationDescriptor(const QorePluginTypeRegistration* reg, PluginValidationState& state,
        const QorePluginModuleHandle* handle, bool registration_mode) {
    if (!reg) {
        return state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", nullptr, nullptr, "reg",
            "non-null registration descriptor", "null", "null_registration_descriptor", "3.3");
    }

    const char* module = reg->module_name;
    if (!module || !*module) {
        if (state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", module, nullptr, "module_name",
                "non-empty module name", module ? "empty" : "null", "null_module_name", "3.3")) {
            return true;
        }
    }
    if (!reg->plugin_abi_version || !*reg->plugin_abi_version) {
        if (state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", module, nullptr, "plugin_abi_version",
                QORE_PLUGIN_ABI_VERSION_V1, reg->plugin_abi_version ? "empty" : "null",
                "null_plugin_abi_version", "3.3")) {
            return true;
        }
    } else if (std::strcmp(reg->plugin_abi_version, QORE_PLUGIN_ABI_VERSION_V1)) {
        if (state.fail("PLUGIN-REGISTRATION-OPERATION-SET-VERSION-INCOMPATIBLE", module, nullptr,
                "plugin_abi_version", QORE_PLUGIN_ABI_VERSION_V1, reg->plugin_abi_version,
                "unsupported_plugin_abi_version", "3.3")) {
            return true;
        }
    }
    if (!reg->operation_set_version || !*reg->operation_set_version) {
        if (state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", module, nullptr, "operation_set_version",
                "non-empty operation-set version", reg->operation_set_version ? "empty" : "null",
                "null_operation_set_version", "3.3")) {
            return true;
        }
    }

    if (checkCountAndArray(state, module, "types", reg->num_types, reg->types)
            || checkCountAndArray(state, module, "operations", reg->num_operations, reg->operations)
            || checkCountAndArray(state, module, "dependencies", reg->num_dependencies, reg->dependencies)) {
        return true;
    }

    std::vector<bool> seen_types(reg->num_types, false);
    for (int i = 0; i < reg->num_types; ++i) {
        if (checkPluginRegistryCancel(i, state.xsink, "plugin type descriptor validation")) {
            return true;
        }
        const QorePluginTypeDescriptor& type = reg->types[i];
        const char* item = type.type_name;
        if (type.local_type_id >= reg->num_types) {
            if (state.fail("PLUGIN-REGISTRATION-DUPLICATE-LOCAL-ID", module, item, "local_type_id",
                    "contiguous id in [0,num_types)", std::to_string(type.local_type_id).c_str(),
                    "non_contiguous_local_id", "3.3")) {
                return true;
            }
        } else if (seen_types[type.local_type_id]) {
            if (state.fail("PLUGIN-REGISTRATION-DUPLICATE-LOCAL-ID", module, item, "local_type_id",
                    "unique local type id", std::to_string(type.local_type_id).c_str(),
                    "duplicate_local_id", "3.3")) {
                return true;
            }
        } else {
            seen_types[type.local_type_id] = true;
        }
        if (!type.type_name || !*type.type_name) {
            if (state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", module, item, "type_name",
                    "non-empty type name", type.type_name ? "empty" : "null", "null_type_name", "3.3")) {
                return true;
            }
        }
        if (!type.type_info) {
            if (state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", module, item, "type_info",
                    "non-null QoreTypeInfo", "null", "null_type_info", "3.3")) {
                return true;
            }
        }
        if (!type.value_ops.incref || !type.value_ops.decref || !type.value_ops.clone
                || !type.value_ops.equal || !type.value_ops.hash || !type.value_ops.cleanup_slot) {
            if (state.fail("PLUGIN-REGISTRATION-NULL-LIFECYCLE", module, item, "value_ops",
                    "all lifecycle callbacks non-null", "one or more null callbacks",
                    "null_lifecycle_callback", "3.3")) {
                return true;
            }
        }
        if (!type.serialize || !type.deserialize) {
            if (state.fail("PLUGIN-REGISTRATION-NULL-CODEC", module, item, "serialize/deserialize",
                    "non-null QORD codec callbacks", "one or more null callbacks", "null_codec", "3.3/3.9")) {
                return true;
            }
        }
        if (!type.serializer_format_version) {
            if (state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", module, item,
                    "serializer_format_version", "nonzero serializer format version", "0",
                    "reserved_field_nonzero", "3.9")) {
                return true;
            }
        }
    }

    std::vector<bool> seen_ops(reg->num_operations, false);
    std::set<std::string> signatures;
    for (int i = 0; i < reg->num_operations; ++i) {
        if (checkPluginRegistryCancel(i, state.xsink, "plugin operation descriptor validation")) {
            return true;
        }
        const QorePluginOperation& op = reg->operations[i];
        const char* item = op.operation_name;
        if (op.local_id >= reg->num_operations) {
            if (state.fail("PLUGIN-REGISTRATION-DUPLICATE-LOCAL-ID", module, item, "local_id",
                    "contiguous id in [0,num_operations)", std::to_string(op.local_id).c_str(),
                    "non_contiguous_local_id", "3.3")) {
                return true;
            }
        } else if (seen_ops[op.local_id]) {
            if (state.fail("PLUGIN-REGISTRATION-DUPLICATE-LOCAL-ID", module, item, "local_id",
                    "unique local operation id", std::to_string(op.local_id).c_str(),
                    "duplicate_local_id", "3.3")) {
                return true;
            }
        } else {
            seen_ops[op.local_id] = true;
        }
        if (!op.operation_name || !*op.operation_name) {
            if (state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", module, item, "operation_name",
                    "non-empty operation name", op.operation_name ? "empty" : "null",
                    "null_operation_name", "3.3")) {
                return true;
            }
        }

        const QorePluginOperationSignature& sig = op.signature;
        bool arity_ok = sig.arity == 0 || sig.arity == 1 || sig.arity == 2 || sig.arity == 0xff;
        if (!arity_ok || !sig.primary_type || !sig.return_type || (sig.arity == 2) != static_cast<bool>(sig.secondary_type)) {
            if (state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", module, item, "signature",
                    "arity in {0,1,2,255}, non-null primary/return type, secondary type only for binary arity",
                    "invalid signature shape", "invalid_signature_shape", "3.3")) {
                return true;
            }
        }
        if (static_cast<uint8_t>(sig.helper_abi) > static_cast<uint8_t>(QorePluginHelperAbi::DenseBufferBinary)) {
            if (state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", module, item, "helper_abi",
                    "known QorePluginHelperAbi", std::to_string(static_cast<uint8_t>(sig.helper_abi)).c_str(),
                    "unknown_helper_abi", "3.3")) {
                return true;
            }
        }
        if (static_cast<uint8_t>(sig.access) > static_cast<uint8_t>(QorePluginValueAccess::MutatesBoth)) {
            if (state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", module, item, "access",
                    "known QorePluginValueAccess", std::to_string(static_cast<uint8_t>(sig.access)).c_str(),
                    "unknown_value_access", "3.3")) {
                return true;
            }
        }
        if (static_cast<uint8_t>(sig.result_alias) > static_cast<uint8_t>(QorePluginResultAlias::ReturnsRhs)) {
            if (state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", module, item, "result_alias",
                    "known QorePluginResultAlias", std::to_string(static_cast<uint8_t>(sig.result_alias)).c_str(),
                    "unknown_result_alias", "3.3")) {
                return true;
            }
        }

        if (!op.runtime_helper && (!op.runtime_helper_symbol || !*op.runtime_helper_symbol)) {
            if (state.fail("PLUGIN-REGISTRATION-HELPER-SYMBOL-MISSING", module, item,
                    "runtime_helper/runtime_helper_symbol", "runtime helper pointer or resolvable symbol",
                    "null helper and no symbol", "helper_symbol_not_found", "3.3")) {
                return true;
            }
        } else if (!op.runtime_helper && handle) {
            void* sym = qore_plugin_resolve_module_symbol(handle, op.runtime_helper_symbol);
            if (!sym) {
                if (state.fail("PLUGIN-REGISTRATION-HELPER-SYMBOL-MISSING", module, item,
                        "runtime_helper_symbol", "symbol exported by registering module",
                        op.runtime_helper_symbol, "helper_symbol_not_found", "3.3")) {
                    return true;
                }
            }
        } else if (!op.runtime_helper && registration_mode) {
            if (state.fail("PLUGIN-REGISTRATION-HELPER-SYMBOL-MISSING", module, item,
                    "module_handle", "live module handle for helper symbol resolution",
                    "null", "module_handle_missing", "3.3")) {
                return true;
            }
        }

        if (checkCountAndArray(state, module, "extensions", op.num_extensions, op.extensions)) {
            return true;
        }
        for (int e = 0; e < op.num_extensions; ++e) {
            if (checkPluginRegistryCancel(e, state.xsink, "plugin operation extension validation")) {
                return true;
            }
            const QorePluginExtension& ext = op.extensions[e];
            if (!validExtensionId(ext.extension_id)) {
                if (state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", module, item,
                        "extensions.extension_id", "non-empty dot-separated ASCII labels",
                        ext.extension_id ? ext.extension_id : "null", "invalid_extension_id", "3.3")) {
                    return true;
                }
            }
            if (ext.required) {
                if (state.fail("PLUGIN-EXTENSION-UNRECOGNIZED-REQUIRED", module, item,
                        "extensions.required", "optional extension or runtime-recognized required extension",
                        ext.extension_id ? ext.extension_id : "null", "extension_unrecognized_required", "3.3")) {
                    return true;
                }
            }
        }

        std::ostringstream key;
        key << nameOrUnknown(op.operation_name) << ':' << static_cast<int>(sig.arity) << ':'
            << sig.primary_type << ':' << sig.secondary_type;
        if (!signatures.insert(key.str()).second) {
            if (state.fail("PLUGIN-REGISTRATION-SIGNATURE-CONFLICT", module, item, "signature",
                    "unique operation signature per module", key.str().c_str(), "duplicate_signature", "3.3")) {
                return true;
            }
        }
    }

    for (int i = 0; i < reg->num_dependencies; ++i) {
        if (checkPluginRegistryCancel(i, state.xsink, "plugin dependency descriptor validation")) {
            return true;
        }
        const QorePluginDependency& dep = reg->dependencies[i];
        if (!dep.module_name || !*dep.module_name || !dep.min_plugin_abi_version || !*dep.min_plugin_abi_version
                || !dep.min_operation_set_version || !*dep.min_operation_set_version) {
            if (state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", module, dep.module_name, "dependencies",
                    "non-empty module_name, min_plugin_abi_version, min_operation_set_version",
                    "one or more null/empty dependency fields", "invalid_dependency", "3.3")) {
                return true;
            }
        }
    }

    return false;
}

bool copyRegistration(const QorePluginTypeRegistration& reg, const char* module_path,
        const QorePluginModuleHandle* handle, RegisteredPluginModule& module, ExceptionSink* xsink) {
    module.module_name = reg.module_name;
    module.module_path = module_path ? module_path : (handle ? handle->module_path : "");
    module.plugin_abi_version = reg.plugin_abi_version;
    module.operation_set_version = reg.operation_set_version;
    module.pending = true;

    module.types.reserve(reg.num_types);
    for (int i = 0; i < reg.num_types; ++i) {
        if (checkPluginRegistryCancel(i, xsink, "plugin type descriptor registration")) {
            return true;
        }
        const QorePluginTypeDescriptor& src = reg.types[i];
        RegisteredPluginType dst;
        dst.local_type_id = src.local_type_id;
        dst.type_name = src.type_name;
        dst.type_info = src.type_info;
        dst.value_ops = src.value_ops;
        dst.serialize = src.serialize;
        dst.deserialize = src.deserialize;
        dst.serializer_format_version = src.serializer_format_version;
        dst.baseline_qdom_domains = src.baseline_qdom_domains;
        module.types.push_back(dst);
    }

    module.operations.reserve(reg.num_operations);
    for (int i = 0; i < reg.num_operations; ++i) {
        if (checkPluginRegistryCancel(i, xsink, "plugin operation descriptor registration")) {
            return true;
        }
        const QorePluginOperation& src = reg.operations[i];
        RegisteredPluginOperation dst;
        dst.local_id = src.local_id;
        dst.operation_name = src.operation_name;
        dst.signature = src.signature;
        dst.canonical_signature_version = QORE_PLUGIN_CANONICAL_SIGNATURE_VERSION_V1;
        dst.signature_hash = computeSignatureHashV1(src.signature);
        dst.info = src.info;
        dst.runtime_helper = src.runtime_helper;
        if (!dst.runtime_helper && src.runtime_helper_symbol && handle) {
            dst.runtime_helper = reinterpret_cast<void (*)()>(
                qore_plugin_resolve_module_symbol(handle, src.runtime_helper_symbol));
        }
        if (src.runtime_helper_symbol) {
            dst.runtime_helper_symbol = src.runtime_helper_symbol;
        }
        dst.lowering_pattern = src.lowering_pattern;
        dst.lowering_claimed_node_kinds = src.lowering_claimed_node_kinds;
        dst.qdom_domains = src.qdom_domains;
        dst.extensions.reserve(src.num_extensions);
        for (int e = 0; e < src.num_extensions; ++e) {
            if (checkPluginRegistryCancel(e, xsink, "plugin operation extension registration")) {
                return true;
            }
            RegisteredPluginExtension ext;
            ext.extension_id = src.extensions[e].extension_id;
            ext.extension_data = src.extensions[e].extension_data;
            ext.required = src.extensions[e].required;
            dst.extensions.push_back(ext);
        }
        module.operations.push_back(dst);
    }

    module.dependencies.reserve(reg.num_dependencies);
    for (int i = 0; i < reg.num_dependencies; ++i) {
        if (checkPluginRegistryCancel(i, xsink, "plugin dependency descriptor registration")) {
            return true;
        }
        RegisteredPluginDependency dst;
        dst.module_name = reg.dependencies[i].module_name;
        dst.min_plugin_abi_version = reg.dependencies[i].min_plugin_abi_version;
        dst.min_operation_set_version = reg.dependencies[i].min_operation_set_version;
        module.dependencies.push_back(dst);
    }
    return false;
}

QoreHashNode* makeTypeHash(const RegisteredPluginModule& module, const RegisteredPluginType& type,
        ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
    h->setKeyValue("module_name", new QoreStringNode(module.module_name), xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("local_type_id", static_cast<int64>(type.local_type_id), xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("type_name", new QoreStringNode(type.type_name), xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("type_path", new QoreStringNode(qore_type_get_path(type.type_info)), xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("serializer_format_version", static_cast<int64>(type.serializer_format_version), xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("baseline_qdom_domains", type.baseline_qdom_domains, xsink);
    return hasException(xsink) ? nullptr : h.release();
}

QoreHashNode* makeOperationHash(const RegisteredPluginModule& module, const RegisteredPluginOperation& op,
        ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
    h->setKeyValue("module_name", new QoreStringNode(module.module_name), xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("local_id", static_cast<int64>(op.local_id), xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("global_id", static_cast<int64>(op.global_id), xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("canonical_signature_version", static_cast<int64>(op.canonical_signature_version), xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("signature_hash", static_cast<int64>(op.signature_hash), xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("operation_name", new QoreStringNode(op.operation_name), xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("arity", static_cast<int64>(op.signature.arity), xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("primary_type", new QoreStringNode(qore_type_get_path(op.signature.primary_type)), xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("secondary_type", op.signature.secondary_type
        ? QoreValue(new QoreStringNode(qore_type_get_path(op.signature.secondary_type)))
        : QoreValue(), xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("return_type", new QoreStringNode(qore_type_get_path(op.signature.return_type)), xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("helper_abi", new QoreStringNode(helperAbiName(op.signature.helper_abi)), xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("access", new QoreStringNode(valueAccessName(op.signature.access)), xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("result_alias", new QoreStringNode(resultAliasName(op.signature.result_alias)), xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("runtime_helper_symbol", op.runtime_helper_symbol.empty()
        ? QoreValue()
        : QoreValue(new QoreStringNode(op.runtime_helper_symbol)), xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("qdom_domains", op.qdom_domains, xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("may_have_side_effects", op.info.may_have_side_effects, xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("may_throw_exception", op.info.may_throw_exception, xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("is_pure_modulo_xsink", op.info.is_pure_modulo_xsink, xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("can_vectorize", op.info.can_vectorize, xsink);
    return hasException(xsink) ? nullptr : h.release();
}

} // namespace

uint64_t qore_plugin_compute_signature_hash_v1(const QorePluginOperationSignature& signature) {
    return computeSignatureHashV1(signature);
}

int qore_plugin_get_aot_module_info(const char* module_name, QorePluginAOTModuleInfo& info, ExceptionSink* xsink) {
    info = QorePluginAOTModuleInfo();
    std::lock_guard<std::mutex> lock(plugin_registry_mutex);
    auto i = plugin_modules.find(module_name ? module_name : "");
    if (i == plugin_modules.end()) {
        if (xsink) {
            xsink->raiseException("PLUGIN-REGISTRY-MODULE-NOT-LOADED",
                "plugin registry has no loaded module named \"%s\" "
                "(method=\"getAotModuleInfo\", subreason=\"module_not_loaded\", section=3.12)",
                escapeDiagnosticName(module_name).c_str());
        }
        return -1;
    }

    info.module_name = i->second.module_name;
    info.plugin_abi_version = i->second.plugin_abi_version;
    info.operation_set_version = i->second.operation_set_version;
    info.types.reserve(i->second.types.size());
    int n = 0;
    for (const RegisteredPluginType& type : i->second.types) {
        if (checkPluginRegistryCancel(n, xsink, "plugin AOT type metadata lookup")) {
            return -1;
        }
        QorePluginAOTTypeInfo ti;
        ti.local_type_id = type.local_type_id;
        ti.type_name = type.type_name;
        ti.type_path = qore_type_get_path(type.type_info);
        ti.serializer_format_version = type.serializer_format_version;
        info.types.push_back(std::move(ti));
        ++n;
    }

    info.operations.reserve(i->second.operations.size());
    n = 0;
    for (const RegisteredPluginOperation& op : i->second.operations) {
        if (checkPluginRegistryCancel(n, xsink, "plugin AOT operation metadata lookup")) {
            return -1;
        }
        QorePluginAOTOperationInfo oi;
        oi.local_id = op.local_id;
        oi.operation_name = op.operation_name;
        oi.signature = op.signature;
        oi.canonical_signature_version = op.canonical_signature_version;
        oi.signature_hash = op.signature_hash;
        info.operations.push_back(std::move(oi));
        ++n;
    }
    return 0;
}

QorePluginModuleHandle::QorePluginModuleHandle(const char* name, const char* path, void* dl_handle)
        : module_name(name ? name : ""), module_path(path ? path : ""), dl_handle(dl_handle),
        generation(plugin_handle_generation.fetch_add(1, std::memory_order_relaxed)) {
}

QorePluginModuleInitScope::QorePluginModuleInitScope(QorePluginModuleHandle& handle) : handle(handle) {
    assert(!current_plugin_module_handle);
    handle.active = true;
    handle.committed = false;
    current_plugin_module_handle = &handle;
}

QorePluginModuleInitScope::~QorePluginModuleInitScope() {
    if (!committed) {
        qore_plugin_rollback_module_init_registration(handle);
    }
    if (current_plugin_module_handle == &handle) {
        current_plugin_module_handle = nullptr;
    }
    handle.active = false;
}

void QorePluginModuleInitScope::commit(ExceptionSink* xsink) {
    if (qore_plugin_commit_module_init_registration(handle, xsink)) {
        return;
    }
    committed = true;
    handle.committed = true;
}

bool qore_plugin_is_current_module_handle(const QorePluginModuleHandle* handle) {
    return handle && handle == current_plugin_module_handle && handle->magic == QorePluginModuleHandle::Magic
        && handle->active;
}

void* qore_plugin_resolve_module_symbol(const QorePluginModuleHandle* handle, const char* symbol) {
    if (!handle || !symbol || !*symbol || !handle->dl_handle) {
        return nullptr;
    }
    return dlsym(handle->dl_handle, symbol);
}

int qore_plugin_commit_module_init_registration(const QorePluginModuleHandle& handle, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lock(plugin_registry_mutex);
    auto i = pending_plugin_modules.find(&handle);
    if (i == pending_plugin_modules.end()) {
        return 0;
    }

    RegisteredPluginModule module = std::move(i->second);
    pending_plugin_modules.erase(i);
    module.pending = false;

    auto existing = plugin_modules.find(module.module_name);
    if (existing != plugin_modules.end()) {
        if (xsink) {
            xsink->raiseException("PLUGIN-CROSS-TYPE-CONFLICT", "plugin module \"%s\" cannot be committed from "
                "\"%s\": a plugin registration for the same module name was already committed from \"%s\" "
                "(field=\"module_name\", subreason=\"duplicate_committed_module\", section=3.10)",
                module.module_name.c_str(), module.module_path.c_str(), existing->second.module_path.c_str());
        }
        return -1;
    }

    traceRegister("committed plugin module '" + module.module_name + "' with "
        + std::to_string(module.types.size()) + " type(s) and "
        + std::to_string(module.operations.size()) + " operation(s)");

    size_t n = 0;
    std::vector<uint32_t> assigned_global_ids;
    assigned_global_ids.reserve(module.operations.size());
    for (RegisteredPluginOperation& op : module.operations) {
        if (checkPluginRegistryCancel(static_cast<int>(n), xsink, "plugin operation id assignment")) {
            for (uint32_t id : assigned_global_ids) {
                global_plugin_operations.erase(id);
            }
            if (!assigned_global_ids.empty()) {
                next_global_plugin_operation_id = assigned_global_ids.front();
            }
            return -1;
        }
        op.global_id = next_global_plugin_operation_id++;
        global_plugin_operations.emplace(op.global_id,
            GlobalPluginOperationRef{module.module_name, op.local_id, n});
        assigned_global_ids.push_back(op.global_id);
        traceRegister("assigned plugin operation id " + std::to_string(op.global_id)
            + " to '" + module.module_name + "::" + op.operation_name + "'");
        ++n;
    }
    plugin_modules.emplace(module.module_name, std::move(module));
    return 0;
}

void qore_plugin_rollback_module_init_registration(const QorePluginModuleHandle& handle) {
    std::lock_guard<std::mutex> lock(plugin_registry_mutex);
    auto i = pending_plugin_modules.find(&handle);
    if (i != pending_plugin_modules.end()) {
        traceRegister("rolled back pending plugin module '" + i->second.module_name + "'");
        pending_plugin_modules.erase(i);
    }
}

extern "C" int qore_validate_plugin_types_v1(const QorePluginTypeRegistration* reg,
        const QorePluginValidationContext* ctx, bool collect_all, ExceptionSink* xsink) {
    PluginValidationState state{xsink, collect_all, false};
    const QorePluginModuleHandle* handle = nullptr;

    if (ctx) {
        if (ctx->struct_size < offsetof(QorePluginValidationContext, program) + sizeof(ctx->program)) {
            state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", reg ? reg->module_name : nullptr, nullptr,
                "validation_context.struct_size", "at least sizeof(QorePluginValidationContext)",
                std::to_string(ctx->struct_size).c_str(), "registration_context_too_small", "3.12");
            return -1;
        }
        if (ctx->flags) {
            if (state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", reg ? reg->module_name : nullptr, nullptr,
                    "validation_context.flags", "0", std::to_string(ctx->flags).c_str(),
                    "reserved_field_nonzero", "3.12")) {
                return -1;
            }
        }
        if (ctx->module_handle) {
            if (!qore_plugin_is_current_module_handle(ctx->module_handle)) {
                if (state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", reg ? reg->module_name : nullptr, nullptr,
                        "validation_context.module_handle", "current thread's live module-init handle",
                        "stale or foreign handle", "module_handle_stale", "3.3")) {
                    return -1;
                }
            } else {
                handle = ctx->module_handle;
            }
        }
    }

    if (validateRegistrationDescriptor(reg, state, handle, false)) {
        return -1;
    }
    return state.failed ? -1 : 0;
}

extern "C" int qore_register_plugin_types_v1(const QorePluginRegistrationContextV1* ctx,
        const QorePluginTypeRegistration* reg, ExceptionSink* xsink) {
    PluginValidationState state{xsink, false, false};
    if (!ctx) {
        state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", reg ? reg->module_name : nullptr, nullptr, "ctx",
            "non-null registration context", "null", "null_registration_context", "3.3");
        return -1;
    }
    if (ctx->struct_size < sizeof(QorePluginRegistrationContextV1)) {
        state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", reg ? reg->module_name : nullptr, nullptr,
            "ctx.struct_size", "sizeof(QorePluginRegistrationContextV1)",
            std::to_string(ctx->struct_size).c_str(), "registration_context_too_small", "3.3");
        return -1;
    }
    if (ctx->struct_size > sizeof(QorePluginRegistrationContextV1)) {
        state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", reg ? reg->module_name : nullptr, nullptr,
            "ctx.struct_size", "sizeof(QorePluginRegistrationContextV1)",
            std::to_string(ctx->struct_size).c_str(), "registration_context_size_mismatch", "3.3");
        return -1;
    }
    if (ctx->flags) {
        state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", reg ? reg->module_name : nullptr, nullptr,
            "ctx.flags", "0", std::to_string(ctx->flags).c_str(), "reserved_field_nonzero", "3.3");
        return -1;
    }
    if (!ctx->module_path || !*ctx->module_path) {
        state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", reg ? reg->module_name : nullptr, nullptr,
            "ctx.module_path", "non-empty module path", ctx->module_path ? "empty" : "null",
            "module_path_missing", "3.3");
        return -1;
    }
    if (!ctx->module_handle) {
        state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", reg ? reg->module_name : nullptr, nullptr,
            "ctx.module_handle", "current thread's live module-init handle", "null",
            "module_handle_missing", "3.3");
        return -1;
    }
    if (!qore_plugin_is_current_module_handle(ctx->module_handle)) {
        state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", reg ? reg->module_name : nullptr, nullptr,
            "ctx.module_handle", "current thread's live module-init handle", "stale or foreign handle",
            "module_handle_stale", "3.3");
        return -1;
    }

    if (validateRegistrationDescriptor(reg, state, ctx->module_handle, true)) {
        return -1;
    }

    RegisteredPluginModule module;
    if (copyRegistration(*reg, ctx->module_path, ctx->module_handle, module, xsink)) {
        return -1;
    }
    {
        std::lock_guard<std::mutex> lock(plugin_registry_mutex);
        if (pending_plugin_modules.find(ctx->module_handle) != pending_plugin_modules.end()) {
            state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", reg->module_name, nullptr, "module_handle",
                "one registration per module-init transaction", "already registered",
                "registration_already_pending", "3.3");
            return -1;
        }
        if (plugin_modules.find(module.module_name) != plugin_modules.end()) {
            state.fail("PLUGIN-CROSS-TYPE-CONFLICT", reg->module_name, nullptr, "module_name",
                "unique committed plugin module name", module.module_name.c_str(), "duplicate_committed_module",
                "3.10");
            return -1;
        }
        pending_plugin_modules.emplace(ctx->module_handle, std::move(module));
    }

    traceRegister("staged plugin module '" + std::string(reg->module_name) + "' from " + ctx->module_path);
    return 0;
}

QoreListNode* qore_plugin_get_process_modules(ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(stringTypeInfo), xsink);
    std::lock_guard<std::mutex> lock(plugin_registry_mutex);
    int n = 0;
    for (const auto& i : plugin_modules) {
        if (checkPluginRegistryCancel(n, xsink, "plugin registry module introspection")) {
            return nullptr;
        }
        rv->push(new QoreStringNode(i.first), xsink);
        if (hasException(xsink)) {
            return nullptr;
        }
        ++n;
    }
    return rv.release();
}

QoreListNode* qore_plugin_get_process_types(const char* module_name, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    std::lock_guard<std::mutex> lock(plugin_registry_mutex);
    auto i = plugin_modules.find(module_name ? module_name : "");
    if (i == plugin_modules.end()) {
        if (xsink) {
            xsink->raiseException("PLUGIN-REGISTRY-MODULE-NOT-LOADED", "plugin registry has no loaded module named "
                "\"%s\" (method=\"getTypes\", subreason=\"module_not_loaded\", section=3.12)",
                escapeDiagnosticName(module_name).c_str());
        }
        return nullptr;
    }
    int n = 0;
    for (const RegisteredPluginType& type : i->second.types) {
        if (checkPluginRegistryCancel(n, xsink, "plugin registry type introspection")) {
            return nullptr;
        }
        QoreHashNode* h = makeTypeHash(i->second, type, xsink);
        if (hasException(xsink)) {
            return nullptr;
        }
        rv->push(h, xsink);
        if (hasException(xsink)) {
            return nullptr;
        }
        ++n;
    }
    return rv.release();
}

QoreListNode* qore_plugin_get_process_operations(const char* module_name, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    std::lock_guard<std::mutex> lock(plugin_registry_mutex);
    auto i = plugin_modules.find(module_name ? module_name : "");
    if (i == plugin_modules.end()) {
        if (xsink) {
            xsink->raiseException("PLUGIN-REGISTRY-MODULE-NOT-LOADED", "plugin registry has no loaded module named "
                "\"%s\" (method=\"getOperations\", subreason=\"module_not_loaded\", section=3.12)",
                escapeDiagnosticName(module_name).c_str());
        }
        return nullptr;
    }
    int n = 0;
    for (const RegisteredPluginOperation& op : i->second.operations) {
        if (checkPluginRegistryCancel(n, xsink, "plugin registry operation introspection")) {
            return nullptr;
        }
        QoreHashNode* h = makeOperationHash(i->second, op, xsink);
        if (hasException(xsink)) {
            return nullptr;
        }
        rv->push(h, xsink);
        if (hasException(xsink)) {
            return nullptr;
        }
        ++n;
    }
    return rv.release();
}

int qore_plugin_get_process_operation_id(const char* module_name, uint16_t local_operation_id,
        uint32_t* global_operation_id, ExceptionSink* xsink) {
    return qore_plugin_get_process_operation_id_checked(module_name, local_operation_id, 0, 0,
        global_operation_id, xsink);
}

int qore_plugin_get_process_operation_id_checked(const char* module_name, uint16_t local_operation_id,
        uint8_t canonical_signature_version, uint64_t signature_hash, uint32_t* global_operation_id,
        ExceptionSink* xsink) {
    if (global_operation_id) {
        *global_operation_id = 0;
    }
    if (canonical_signature_version
            && canonical_signature_version != QORE_PLUGIN_CANONICAL_SIGNATURE_VERSION_V1) {
        if (xsink) {
            xsink->raiseException("QORD-PLUGIN-SIGNATURE-VERSION-UNSUPPORTED",
                "plugin registry cannot resolve operation %s:%u: canonical signature version %u is unsupported "
                "(field=\"canonical_signature_version\", expected=\"%u\", actual=\"%u\", "
                "subreason=\"unsupported_canonical_version\", section=3.9)",
                escapeDiagnosticName(module_name).c_str(), local_operation_id,
                canonical_signature_version, QORE_PLUGIN_CANONICAL_SIGNATURE_VERSION_V1,
                canonical_signature_version);
        }
        return -1;
    }
    std::lock_guard<std::mutex> lock(plugin_registry_mutex);
    auto i = plugin_modules.find(module_name ? module_name : "");
    if (i == plugin_modules.end()) {
        if (xsink) {
            xsink->raiseException("PLUGIN-REGISTRY-MODULE-NOT-LOADED",
                "plugin registry has no loaded module named \"%s\" "
                "(method=\"getProcessOperationId\", subreason=\"module_not_loaded\", section=3.12)",
                escapeDiagnosticName(module_name).c_str());
        }
        return -1;
    }
    int n = 0;
    for (const RegisteredPluginOperation& op : i->second.operations) {
        if (checkPluginRegistryCancel(n, xsink, "plugin registry operation id lookup")) {
            return -1;
        }
        if (op.local_id == local_operation_id) {
            if (signature_hash && op.signature_hash != signature_hash) {
                if (xsink) {
                    xsink->raiseException("QORD-PLUGIN-SIGNATURE-HASH-MISMATCH",
                        "plugin registry module \"%s\" operation local id %u has signature hash 0x%016llx, "
                        "but the operation reference requires 0x%016llx "
                        "(method=\"getProcessOperationId\", field=\"signature_hash\", "
                        "expected=\"matching canonical signature hash\", actual=\"mismatch\", "
                        "subreason=\"signature_hash_mismatch\", section=3.9)",
                        escapeDiagnosticName(module_name).c_str(), local_operation_id,
                        static_cast<unsigned long long>(op.signature_hash),
                        static_cast<unsigned long long>(signature_hash));
                }
                return -1;
            }
            if (global_operation_id) {
                *global_operation_id = op.global_id;
            }
            return 0;
        }
        ++n;
    }
    if (xsink) {
        xsink->raiseException("PLUGIN-REGISTRY-OPERATION-NOT-REGISTERED",
            "plugin registry module \"%s\" has no operation with local id %u "
            "(method=\"getProcessOperationId\", field=\"local_operation_id\", "
            "expected=\"registered operation id\", actual=\"missing\", subreason=\"operation_not_registered\", "
            "section=3.12)",
            escapeDiagnosticName(module_name).c_str(), local_operation_id);
    }
    return -1;
}

extern "C" uint64_t qore_rt_plugin_unary(uint32_t global_operation_id, uint64_t value_bits,
        ExceptionSink* xsink) {
    ResolvedPluginOperation op;
    if (resolvePluginOperation(global_operation_id, QorePluginHelperAbi::UnaryValue, op, xsink)) {
        return nothingBits();
    }
    traceDispatch("unary id=" + std::to_string(global_operation_id) + " module='" + op.module_name
        + "' operation='" + op.operation_name + "' helper="
        + std::to_string(reinterpret_cast<uintptr_t>(op.runtime_helper)));
    auto helper = reinterpret_cast<PluginUnaryHelper>(op.runtime_helper);
    return helper(value_bits, xsink);
}

extern "C" uint64_t qore_rt_plugin_binary(uint32_t global_operation_id, uint64_t lhs_bits,
        uint64_t rhs_bits, ExceptionSink* xsink) {
    ResolvedPluginOperation op;
    if (resolvePluginOperation(global_operation_id, QorePluginHelperAbi::BinaryValue, op, xsink)) {
        return nothingBits();
    }
    traceDispatch("binary id=" + std::to_string(global_operation_id) + " module='" + op.module_name
        + "' operation='" + op.operation_name + "' helper="
        + std::to_string(reinterpret_cast<uintptr_t>(op.runtime_helper)));
    auto helper = reinterpret_cast<PluginBinaryHelper>(op.runtime_helper);
    return helper(lhs_bits, rhs_bits, xsink);
}

extern "C" uint64_t qore_rt_plugin_call(uint32_t global_operation_id, uint64_t self_bits,
        uint64_t args_list_bits, ExceptionSink* xsink) {
    ResolvedPluginOperation op;
    if (resolvePluginOperation(global_operation_id, QorePluginHelperAbi::CallValueList, op, xsink)) {
        return nothingBits();
    }
    traceDispatch("call id=" + std::to_string(global_operation_id) + " module='" + op.module_name
        + "' operation='" + op.operation_name + "' helper="
        + std::to_string(reinterpret_cast<uintptr_t>(op.runtime_helper)));
    auto helper = reinterpret_cast<PluginCallHelper>(op.runtime_helper);
    return helper(self_bits, args_list_bits, xsink);
}

extern "C" uint64_t qore_rt_plugin_call_args(uint32_t global_operation_id, uint64_t self_bits,
        const uint64_t* arg_bits, int32_t nargs, ExceptionSink* xsink) {
    QoreValue args_value = makePluginArgListFromBits(arg_bits, nargs, xsink);
    if (xsink && *xsink) {
        args_value.discard(xsink);
        return nothingBits();
    }
    uint64_t rv = qore_rt_plugin_call(global_operation_id, self_bits, bitsFromValue(args_value), xsink);
    args_value.discard(xsink);
    return rv;
}

extern "C" uint64_t qore_rt_plugin_subscript(uint32_t global_operation_id, uint64_t container_bits,
        uint64_t key_bits, ExceptionSink* xsink) {
    ResolvedPluginOperation op;
    if (resolvePluginOperation(global_operation_id, QorePluginHelperAbi::SubscriptValue, op, xsink)) {
        return nothingBits();
    }
    traceDispatch("subscript id=" + std::to_string(global_operation_id) + " module='" + op.module_name
        + "' operation='" + op.operation_name + "' helper="
        + std::to_string(reinterpret_cast<uintptr_t>(op.runtime_helper)));
    auto helper = reinterpret_cast<PluginBinaryHelper>(op.runtime_helper);
    return helper(container_bits, key_bits, xsink);
}

extern "C" uint64_t qore_rt_plugin_construct(uint32_t global_operation_id, uint64_t args_list_bits,
        ExceptionSink* xsink) {
    ResolvedPluginOperation op;
    if (resolvePluginOperation(global_operation_id, QorePluginHelperAbi::Construct, op, xsink)) {
        return nothingBits();
    }
    traceDispatch("construct id=" + std::to_string(global_operation_id) + " module='" + op.module_name
        + "' operation='" + op.operation_name + "' helper="
        + std::to_string(reinterpret_cast<uintptr_t>(op.runtime_helper)));
    auto helper = reinterpret_cast<PluginConstructHelper>(op.runtime_helper);
    return helper(args_list_bits, xsink);
}

extern "C" uint64_t qore_rt_plugin_construct_args(uint32_t global_operation_id, const uint64_t* arg_bits,
        int32_t nargs, ExceptionSink* xsink) {
    QoreValue args_value = makePluginArgListFromBits(arg_bits, nargs, xsink);
    if (xsink && *xsink) {
        args_value.discard(xsink);
        return nothingBits();
    }
    uint64_t rv = qore_rt_plugin_construct(global_operation_id, bitsFromValue(args_value), xsink);
    args_value.discard(xsink);
    return rv;
}

extern "C" uint64_t qore_rt_plugin_dense_buffer_unary(uint32_t global_operation_id, void* result_buffer_data,
        int64_t result_size, const void* value_data, int64_t value_size, int64_t value_stride,
        ExceptionSink* xsink) {
    if (validateDenseBufferFrame("dense-buffer unary plugin helper", result_buffer_data, result_size,
            value_data, value_size, nullptr, 0, xsink)) {
        return nothingBits();
    }
    ResolvedPluginOperation op;
    if (resolvePluginOperation(global_operation_id, QorePluginHelperAbi::DenseBufferUnary, op, xsink)) {
        return nothingBits();
    }
    traceDispatch("dense-buffer-unary id=" + std::to_string(global_operation_id) + " module='" + op.module_name
        + "' operation='" + op.operation_name + "' helper="
        + std::to_string(reinterpret_cast<uintptr_t>(op.runtime_helper)));
    auto helper = reinterpret_cast<PluginDenseBufferUnaryHelper>(op.runtime_helper);
    return helper(result_buffer_data, result_size, value_data, value_size, value_stride, xsink);
}

extern "C" uint64_t qore_rt_plugin_dense_buffer_binary(uint32_t global_operation_id, void* result_buffer_data,
        int64_t result_size, const void* lhs_data, int64_t lhs_size, int64_t lhs_stride, const void* rhs_data,
        int64_t rhs_size, int64_t rhs_stride, ExceptionSink* xsink) {
    if (validateDenseBufferFrame("dense-buffer binary plugin helper", result_buffer_data, result_size,
            lhs_data, lhs_size, rhs_data, rhs_size, xsink)) {
        return nothingBits();
    }
    ResolvedPluginOperation op;
    if (resolvePluginOperation(global_operation_id, QorePluginHelperAbi::DenseBufferBinary, op, xsink)) {
        return nothingBits();
    }
    traceDispatch("dense-buffer-binary id=" + std::to_string(global_operation_id) + " module='" + op.module_name
        + "' operation='" + op.operation_name + "' helper="
        + std::to_string(reinterpret_cast<uintptr_t>(op.runtime_helper)));
    auto helper = reinterpret_cast<PluginDenseBufferBinaryHelper>(op.runtime_helper);
    return helper(result_buffer_data, result_size, lhs_data, lhs_size, lhs_stride, rhs_data, rhs_size, rhs_stride,
        xsink);
}
