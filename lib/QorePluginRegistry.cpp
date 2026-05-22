/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QorePluginRegistry.cpp

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.
*/

#include <qore/Qore.h>
#include <qore/QoreBufferNode.h>
#include <qore/QorePluginLLVM.h>
#include <qore/QorePluginType.h>
#include <qore/QoreReflection.h>
#include <qore/intern/QorePluginRegistry.h>
#include <qore/intern/qore_program_private.h>
#include <qore/intern/xxhash.h>
#include <qore/intern/qore_list_private.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <limits>
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
    QorePluginLLVMCodegenCallback llvm_codegen = nullptr;
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
    static const bool enabled = std::getenv("QORE_PLUGIN_REGISTER_TRACE") != nullptr;
    return enabled;
}

bool pluginDispatchTrace() {
    static const bool enabled = std::getenv("QORE_PLUGIN_DISPATCH_TRACE") != nullptr;
    return enabled;
}

bool pluginVerifyEnabled() {
#ifndef NDEBUG
    return true;
#else
    static const bool enabled = std::getenv("QORE_PLUGIN_VERIFY") != nullptr;
    return enabled;
#endif
}

bool pluginVerifyTrace() {
    static const bool enabled = std::getenv("QORE_PLUGIN_VERIFY_TRACE") != nullptr;
    return enabled;
}

bool pluginCrossTypeTrace() {
    static const bool enabled = std::getenv("QORE_PLUGIN_CROSS_TYPE_TRACE") != nullptr;
    return enabled;
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

void traceVerify(const std::string& msg) {
    if (pluginVerifyTrace()) {
        std::fprintf(stderr, "QORE_PLUGIN_VERIFY_TRACE: %s\n", msg.c_str());
    }
}

void traceCrossType(const std::string& msg) {
    if (pluginCrossTypeTrace()) {
        std::fprintf(stderr, "QORE_PLUGIN_CROSS_TYPE_TRACE: %s\n", msg.c_str());
    }
}

size_t pluginFallbackBufferLimit() {
    const char* env = std::getenv("QORE_PLUGIN_FALLBACK_BUFFER");
    if (!env || !*env) {
        return 1024;
    }
    if (*env == '-') {
        traceRegister(std::string("invalid QORE_PLUGIN_FALLBACK_BUFFER='") + env
            + "', using default capacity 1024");
        return 1024;
    }

    errno = 0;
    char* end = nullptr;
    unsigned long value = std::strtoul(env, &end, 10);
    if (errno || end == env || (end && *end)) {
        traceRegister(std::string("invalid QORE_PLUGIN_FALLBACK_BUFFER='") + env
            + "', using default capacity 1024");
        return 1024;
    }
    if (value > 65536) {
        traceRegister(std::string("QORE_PLUGIN_FALLBACK_BUFFER='") + env
            + "' exceeds maximum capacity 65536, capping");
        return 65536;
    }
    return static_cast<size_t>(value);
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

class QorePluginValueNode : public SimpleValueQoreNode {
public:
    QorePluginValueNode(const QorePluginResolvedTypeInfo& type, uint64_t value_bits)
            : SimpleValueQoreNode(NT_PLUGIN_VALUE), type(type), value_bits(value_bits) {
    }

    ~QorePluginValueNode() override {
        if (type.value_ops.decref) {
            type.value_ops.decref(value_bits);
        }
    }

    const QorePluginResolvedTypeInfo& getPluginType() const {
        return type;
    }

    uint64_t getValueBits() const {
        return value_bits;
    }

    int getAsString(QoreString& str, int, ExceptionSink*) const override {
        str.sprintf("<plugin-value %s:%s 0x%016llx>",
            type.module_name.c_str(), type.type_name.c_str(), static_cast<unsigned long long>(value_bits));
        return 0;
    }

    QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const override {
        del = true;
        QoreString* rv = new QoreString;
        getAsString(*rv, foff, xsink);
        return rv;
    }

    AbstractQoreNode* realCopy() const override {
        if (type.value_ops.incref) {
            type.value_ops.incref(value_bits);
        }
        return new QorePluginValueNode(type, value_bits);
    }

    bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const override {
        return is_equal_hard(v, xsink);
    }

    bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const override {
        if (get_node_type(v) != NT_PLUGIN_VALUE) {
            return false;
        }
        const QorePluginValueNode* other = static_cast<const QorePluginValueNode*>(v);
        if (type.module_name != other->type.module_name || type.local_type_id != other->type.local_type_id) {
            return false;
        }
        return type.value_ops.equal
            ? type.value_ops.equal(value_bits, other->value_bits, xsink)
            : value_bits == other->value_bits;
    }

    const char* getTypeName() const override {
        return type.type_name.c_str();
    }

private:
    QorePluginResolvedTypeInfo type;
    uint64_t value_bits = 0;
};

struct PluginByteVectorWriter {
    std::vector<uint8_t> payload;
};

int pluginByteVectorWrite(const void* data, uint32_t len, void* user_data, ExceptionSink* xsink) {
    PluginByteVectorWriter* writer = static_cast<PluginByteVectorWriter*>(user_data);
    if (!writer || (len && !data)) {
        if (xsink) {
            xsink->raiseException("PLUGIN-SERIALIZATION-ERROR",
                "plugin value serializer supplied invalid write callback arguments "
                "(field=\"payload\", expected=\"valid data pointer\", actual=\"null\", "
                "subreason=\"invalid_serializer_write\", section=3.9)");
        }
        return -1;
    }
    if (writer->payload.size() > std::numeric_limits<uint32_t>::max() - len) {
        if (xsink) {
            xsink->raiseException("PLUGIN-SERIALIZATION-ERROR",
                "plugin value serializer payload exceeds the 32-bit QORD payload limit "
                "(field=\"payload_length\", expected=\"<= %u\", actual=\"overflow\", "
                "subreason=\"payload_too_large\", section=3.9)",
                std::numeric_limits<uint32_t>::max());
        }
        return -1;
    }
    const uint8_t* p = static_cast<const uint8_t*>(data);
    writer->payload.insert(writer->payload.end(), p, p + len);
    return 0;
}

struct PluginByteVectorReader {
    const uint8_t* payload = nullptr;
    uint32_t payload_len = 0;
    uint32_t offset = 0;
};

int pluginByteVectorRead(void* data, uint32_t len, void* user_data, ExceptionSink* xsink) {
    PluginByteVectorReader* reader = static_cast<PluginByteVectorReader*>(user_data);
    if (!reader || (len && !data)) {
        if (xsink) {
            xsink->raiseException("PLUGIN-DESERIALIZATION-ERROR",
                "plugin value deserializer supplied invalid read callback arguments "
                "(field=\"payload\", expected=\"valid data pointer\", actual=\"null\", "
                "subreason=\"invalid_deserializer_read\", section=3.9)");
        }
        return -1;
    }
    if (len > reader->payload_len - reader->offset) {
        if (xsink) {
            xsink->raiseException("PLUGIN-DESERIALIZATION-ERROR",
                "plugin value deserializer read beyond the serialized payload "
                "(field=\"payload_length\", expected=\"%u remaining byte(s)\", actual=\"%u requested byte(s)\", "
                "subreason=\"payload_underflow\", section=3.9)",
                reader->payload_len - reader->offset, len);
        }
        return -1;
    }
    if (len) {
        memcpy(data, reader->payload + reader->offset, len);
        reader->offset += len;
    }
    return 0;
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

const char* typePromotionKindName(QorePluginOpcodeTypePromotion kind) {
    switch (kind) {
        case QorePluginOpcodeTypePromotion::Exact: return "Exact";
        case QorePluginOpcodeTypePromotion::WideningLattice: return "WideningLattice";
        case QorePluginOpcodeTypePromotion::IdentityLHS: return "IdentityLHS";
        case QorePluginOpcodeTypePromotion::Custom: return "Custom";
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

bool verifyPluginResult(const ResolvedPluginOperation& op, const char* helper_name, uint64_t result_bits,
    uint64_t lhs_bits, bool have_lhs, uint64_t rhs_bits, bool have_rhs, ExceptionSink* xsink);

struct DenseBufferValueFrame {
    QoreBufferNode* buffer = nullptr;
    void* mutable_data = nullptr;
    const void* const_data = nullptr;
    int64_t size = 0;
    int64_t stride = 1;
};

std::string denseBufferTypeName(const QoreBufferNode& buffer) {
    std::string rv = "buffer<";
    if (buffer.hasNullableElements()) {
        rv += "*";
    }
    rv += qore_buffer_element_type_name(buffer.getElementType());
    rv += ">";
    return rv;
}

std::string actualValueTypeName(const QoreValue& value) {
    if (value.getType() == NT_BUFFER) {
        const QoreBufferNode* buffer = value.get<const QoreBufferNode>();
        if (buffer) {
            return denseBufferTypeName(*buffer);
        }
    }
    QoreString scratch;
    return value.getFullTypeName(true, scratch);
}

std::string pluginOperationLabel(const ResolvedPluginOperation& op) {
    return escapeDiagnosticName(op.module_name.c_str()) + ":" + escapeDiagnosticName(op.operation_name.c_str());
}

bool raiseDenseBufferValueError(const ResolvedPluginOperation& op, const char* helper_name, const char* role,
        const char* expected, const std::string& actual, const char* detail, const char* subreason,
        ExceptionSink* xsink) {
    if (xsink) {
        xsink->raiseException("PLUGIN-HELPER-ABI-MISMATCH",
            "cannot dispatch %s plugin operation \"%s\": %s argument %s; got %s "
            "(field=\"%s\", expected=\"%s\", actual=\"%s\", subreason=\"%s\", section=3.5)",
            helper_name, pluginOperationLabel(op).c_str(), role, detail, actual.c_str(), role, expected,
            actual.c_str(), subreason);
    }
    return true;
}

bool prepareDenseBufferValueFrame(const ResolvedPluginOperation& op, const char* helper_name, const char* role,
        uint64_t value_bits, bool writable, DenseBufferValueFrame& out, ExceptionSink* xsink) {
    QoreValue value = valueFromBits(value_bits);
    QoreBufferNode* buffer = value.getType() == NT_BUFFER ? value.get<QoreBufferNode>() : nullptr;
    if (!buffer) {
        return raiseDenseBufferValueError(op, helper_name, role,
            "non-nullable buffer<int8|int16|int32|int64|float32|float64>",
            actualValueTypeName(value), "must be a Qore buffer<T> value", "dense_buffer_argument_type", xsink);
    }
    if (buffer->hasNullableElements()) {
        return raiseDenseBufferValueError(op, helper_name, role,
            "non-nullable buffer<int8|int16|int32|int64|float32|float64>",
            denseBufferTypeName(*buffer), "must not have nullable elements because the DenseBuffer helper ABI "
            "does not carry a validity bitmap", "dense_buffer_nullable_elements", xsink);
    }
    if (buffer->getElementType() == QoreBufferElementType::Bool) {
        return raiseDenseBufferValueError(op, helper_name, role,
            "buffer<int8|int16|int32|int64|float32|float64>", denseBufferTypeName(*buffer),
            "must use byte-addressable numeric storage; buffer<bool> is bit-packed",
            "dense_buffer_bitpacked_bool", xsink);
    }
    if (!qore_buffer_element_storage_size(buffer->getElementType())) {
        return raiseDenseBufferValueError(op, helper_name, role,
            "buffer<int8|int16|int32|int64|float32|float64>", denseBufferTypeName(*buffer),
            "has no byte-addressable element storage", "dense_buffer_storage", xsink);
    }
    if (buffer->size() > static_cast<size_t>(std::numeric_limits<int64_t>::max())) {
        return raiseDenseBufferValueError(op, helper_name, role, "buffer length <= INT64_MAX",
            denseBufferTypeName(*buffer), "is too large for the DenseBuffer helper ABI",
            "dense_buffer_size_overflow", xsink);
    }

    out.buffer = buffer;
    out.mutable_data = writable ? buffer->getRawData() : nullptr;
    out.const_data = buffer->getRawData();
    out.size = static_cast<int64_t>(buffer->size());
    out.stride = 1;
    return false;
}

bool validateDenseBufferNoAlias(const ResolvedPluginOperation& op, const char* helper_name,
        const DenseBufferValueFrame& result, const DenseBufferValueFrame& value, const char* value_role,
        ExceptionSink* xsink) {
    if (result.buffer != value.buffer) {
        return false;
    }
    if (xsink) {
        xsink->raiseException("PLUGIN-HELPER-ABI-MISMATCH",
            "cannot dispatch %s plugin operation \"%s\": result buffer must not be the same Qore buffer as the "
            "%s argument because the DenseBuffer helper ABI exposes separate output and input frames "
            "(field=\"result\", expected=\"distinct buffer\", actual=\"aliased %s\", "
            "subreason=\"dense_buffer_alias\", section=3.5)",
            helper_name, pluginOperationLabel(op).c_str(), value_role, value_role);
    }
    return true;
}

bool validateDenseBufferSameElementType(const ResolvedPluginOperation& op, const char* helper_name,
        const DenseBufferValueFrame& result, const DenseBufferValueFrame& value, const char* value_role,
        ExceptionSink* xsink) {
    if (result.buffer->getElementType() == value.buffer->getElementType()) {
        return false;
    }
    if (xsink) {
        std::string expected = denseBufferTypeName(*result.buffer);
        std::string actual = denseBufferTypeName(*value.buffer);
        xsink->raiseException("PLUGIN-HELPER-ABI-MISMATCH",
            "cannot dispatch %s plugin operation \"%s\": %s argument element storage must match the result buffer "
            "because the DenseBuffer helper ABI passes raw untyped pointers (field=\"%s\", expected=\"%s\", "
            "actual=\"%s\", subreason=\"dense_buffer_element_type_mismatch\", section=3.5)",
            helper_name, pluginOperationLabel(op).c_str(), value_role, value_role, expected.c_str(),
            actual.c_str());
    }
    return true;
}

uint64_t dispatchResolvedDenseBufferUnary(const ResolvedPluginOperation& op, void* result_buffer_data,
        int64_t result_size, const void* value_data, int64_t value_size, int64_t value_stride,
        ExceptionSink* xsink) {
    if (pluginDispatchTrace()) {
        traceDispatch("dense-buffer-unary id=" + std::to_string(op.global_id) + " module='" + op.module_name
            + "' operation='" + op.operation_name + "' helper="
            + std::to_string(reinterpret_cast<uintptr_t>(op.runtime_helper)));
    }
    auto helper = reinterpret_cast<PluginDenseBufferUnaryHelper>(op.runtime_helper);
    uint64_t rv = helper(result_buffer_data, result_size, value_data, value_size, value_stride, xsink);
    if (xsink && *xsink) {
        return nothingBits();
    }
    return verifyPluginResult(op, "dense-buffer-unary", rv, 0, false, 0, false, xsink) ? nothingBits() : rv;
}

uint64_t dispatchResolvedDenseBufferBinary(const ResolvedPluginOperation& op, void* result_buffer_data,
        int64_t result_size, const void* lhs_data, int64_t lhs_size, int64_t lhs_stride, const void* rhs_data,
        int64_t rhs_size, int64_t rhs_stride, ExceptionSink* xsink) {
    if (pluginDispatchTrace()) {
        traceDispatch("dense-buffer-binary id=" + std::to_string(op.global_id) + " module='" + op.module_name
            + "' operation='" + op.operation_name + "' helper="
            + std::to_string(reinterpret_cast<uintptr_t>(op.runtime_helper)));
    }
    auto helper = reinterpret_cast<PluginDenseBufferBinaryHelper>(op.runtime_helper);
    uint64_t rv = helper(result_buffer_data, result_size, lhs_data, lhs_size, lhs_stride, rhs_data, rhs_size,
        rhs_stride, xsink);
    if (xsink && *xsink) {
        return nothingBits();
    }
    return verifyPluginResult(op, "dense-buffer-binary", rv, 0, false, 0, false, xsink) ? nothingBits() : rv;
}

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

std::string bitsToHex(uint64_t bits) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "0x%016llx", static_cast<unsigned long long>(bits));
    return buf;
}

bool raisePluginVerifierError(const ResolvedPluginOperation& op, const char* helper_name, const char* code,
        const char* field, const char* expected, const char* actual, const char* subreason,
        const char* section, ExceptionSink* xsink) {
    traceVerify(std::string("failed helper='") + helper_name + "' module='" + op.module_name
        + "' operation='" + op.operation_name + "' field='" + field + "' expected='" + expected
        + "' actual='" + actual + "' subreason='" + subreason + "'");
    if (xsink) {
        xsink->raiseException(code, "plugin runtime verifier failed: module=\"%s\", operation=\"%s\", "
            "helper=\"%s\", field=\"%s\", expected=\"%s\", actual=\"%s\", subreason=\"%s\", section=%s",
            escapeDiagnosticName(op.module_name.c_str()).c_str(),
            escapeDiagnosticName(op.operation_name.c_str()).c_str(), helper_name, field, expected, actual,
            subreason, section);
    }
    return true;
}

bool verifyPluginResult(const ResolvedPluginOperation& op, const char* helper_name, uint64_t result_bits,
        uint64_t lhs_bits, bool has_lhs, uint64_t rhs_bits, bool has_rhs, ExceptionSink* xsink) {
    if (!pluginVerifyEnabled() || hasException(xsink)) {
        return false;
    }

    if (op.signature.result_alias == QorePluginResultAlias::ReturnsLhs) {
        if (!has_lhs || result_bits != lhs_bits) {
            std::string actual = has_lhs
                ? "result=" + bitsToHex(result_bits) + ", lhs=" + bitsToHex(lhs_bits)
                : "operation has no lhs operand";
            return raisePluginVerifierError(op, helper_name, "PLUGIN-HELPER-ALIAS-CONTRACT-VIOLATED",
                "result_alias", "returned bits equal lhs bits", actual.c_str(), "alias_contract_violation",
                "3.3", xsink);
        }
    } else if (op.signature.result_alias == QorePluginResultAlias::ReturnsRhs) {
        if (!has_rhs || result_bits != rhs_bits) {
            std::string actual = has_rhs
                ? "result=" + bitsToHex(result_bits) + ", rhs=" + bitsToHex(rhs_bits)
                : "operation has no rhs operand";
            return raisePluginVerifierError(op, helper_name, "PLUGIN-HELPER-ALIAS-CONTRACT-VIOLATED",
                "result_alias", "returned bits equal rhs bits", actual.c_str(), "alias_contract_violation",
                "3.3", xsink);
        }
    }

    QoreValue result = valueFromBits(result_bits);
    if (qore_type_is_assignable_from(op.signature.return_type, result) == QTI_NOT_EQUAL) {
        std::string expected = qore_type_get_path(op.signature.return_type);
        return raisePluginVerifierError(op, helper_name, "PLUGIN-HELPER-RESULT-TYPE-MISMATCH",
            "return_type", expected.c_str(), result.getTypeName(), "result_type_mismatch", "3.4", xsink);
    }

    traceVerify(std::string("passed helper='") + helper_name + "' module='" + op.module_name
        + "' operation='" + op.operation_name + "' return_type='"
        + qore_type_get_path(op.signature.return_type) + "' result_alias='"
        + resultAliasName(op.signature.result_alias) + "'");
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
                section ? section : "design/plugin-types-and-dense-data.md#plugin-type-abi");
        }
        return !collect_all;
    }
};

bool isLLVMCodegenExtension(const QorePluginExtension& ext) {
    return ext.extension_id && !std::strcmp(ext.extension_id, QORE_PLUGIN_LLVM_CODEGEN_EXTENSION_ID);
}

const QorePluginLLVMExtension* getLLVMExtensionPayload(const QorePluginExtension& ext) {
    return isLLVMCodegenExtension(ext) ? static_cast<const QorePluginLLVMExtension*>(ext.extension_data) : nullptr;
}

bool validateLLVMCodegenExtension(PluginValidationState& state, const char* module, const char* item,
        const QorePluginExtension& ext) {
    if (!isLLVMCodegenExtension(ext)) {
        return false;
    }

    auto fail = [&](const char* code, const char* field, const char* expected, const char* actual,
            const char* subreason) -> bool {
        if (!ext.required) {
            traceRegister(std::string("ignoring optional LLVM codegen extension for module='")
                + nameOrUnknown(module) + "' operation='" + nameOrUnknown(item) + "': field='" + field
                + "' expected='" + expected + "' actual='" + actual + "'");
            return false;
        }
        return state.fail(code, module, item, field, expected, actual, subreason, "3.6");
    };

    if (!ext.extension_data) {
        return fail("PLUGIN-EXTENSION-VALIDATION-FAILED", "extensions.extension_data",
            "non-null QorePluginLLVMExtension", "null", "llvm_extension_null");
    }
    const QorePluginLLVMExtension* llvm_ext = getLLVMExtensionPayload(ext);
    if (llvm_ext->struct_size < sizeof(QorePluginLLVMExtension)) {
        return fail("PLUGIN-EXTENSION-VALIDATION-FAILED", "extensions.extension_data.struct_size",
            "sizeof(QorePluginLLVMExtension)", std::to_string(llvm_ext->struct_size).c_str(),
            "llvm_extension_struct_size");
    }
    if (llvm_ext->abi_version != QORE_PLUGIN_LLVM_EXTENSION_ABI_VERSION) {
        return fail("PLUGIN-EXTENSION-ABI-MISMATCH", "extensions.extension_data.abi_version",
            std::to_string(QORE_PLUGIN_LLVM_EXTENSION_ABI_VERSION).c_str(),
            std::to_string(llvm_ext->abi_version).c_str(), "llvm_extension_abi_mismatch");
    }
    if (llvm_ext->llvm_major_version != QORE_PLUGIN_LLVM_CURRENT_MAJOR) {
        return fail("PLUGIN-EXTENSION-ABI-MISMATCH", "extensions.extension_data.llvm_major_version",
            std::to_string(QORE_PLUGIN_LLVM_CURRENT_MAJOR).c_str(),
            std::to_string(llvm_ext->llvm_major_version).c_str(), "llvm_extension_llvm_major_mismatch");
    }
    if (!llvm_ext->codegen) {
        return fail("PLUGIN-EXTENSION-VALIDATION-FAILED", "extensions.extension_data.codegen",
            "non-null QorePluginLLVMCodegenCallback", "null", "llvm_codegen_null");
    }
    return false;
}

QorePluginLLVMCodegenCallback getValidLLVMCodegenCallback(const QorePluginExtension& ext) {
    const QorePluginLLVMExtension* llvm_ext = getLLVMExtensionPayload(ext);
    if (!llvm_ext || !ext.extension_data || llvm_ext->struct_size < sizeof(QorePluginLLVMExtension)
            || llvm_ext->abi_version != QORE_PLUGIN_LLVM_EXTENSION_ABI_VERSION
            || llvm_ext->llvm_major_version != QORE_PLUGIN_LLVM_CURRENT_MAJOR || !llvm_ext->codegen) {
        return nullptr;
    }
    return llvm_ext->codegen;
}

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
        if (static_cast<uint8_t>(op.info.type_promotion_kind)
                > static_cast<uint8_t>(QorePluginOpcodeTypePromotion::Custom)) {
            if (state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", module, item,
                    "info.type_promotion_kind", "known QorePluginOpcodeTypePromotion",
                    std::to_string(static_cast<uint8_t>(op.info.type_promotion_kind)).c_str(),
                    "unknown_type_promotion_kind", "3.3")) {
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
            bool recognized_extension = isLLVMCodegenExtension(ext);
            if (recognized_extension && validateLLVMCodegenExtension(state, module, item, ext)) {
                return true;
            }
            if (ext.required && !recognized_extension) {
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
            if (QorePluginLLVMCodegenCallback codegen = getValidLLVMCodegenCallback(src.extensions[e])) {
                dst.llvm_codegen = codegen;
            }
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
    h->setKeyValue("can_return_nothing", op.info.can_return_nothing, xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("never_returns_nothing", op.info.never_returns_nothing, xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("is_commutative", op.info.is_commutative, xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("is_associative", op.info.is_associative, xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("is_idempotent", op.info.is_idempotent, xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("annihilator_zero", op.info.annihilator_zero, xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("has_identity", op.info.has_identity, xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("identity_kind", static_cast<int64>(op.info.identity_kind), xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("is_pure_modulo_xsink", op.info.is_pure_modulo_xsink, xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("can_vectorize", op.info.can_vectorize, xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("fp_reassociation_allowed", op.info.fp_reassociation_allowed, xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("type_promotion_kind", new QoreStringNode(typePromotionKindName(op.info.type_promotion_kind)),
        xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("is_simd_friendly", op.info.is_simd_friendly, xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("cost_class", static_cast<int64>(op.info.cost_class), xsink);
    return hasException(xsink) ? nullptr : h.release();
}

bool pluginProgramHasModule(QoreProgram* pgm, const std::string& module_name) {
    return pgm && qore_program_private::get(*pgm)->hasFeature(module_name.c_str());
}

std::vector<std::string> getProcessPluginModuleNames(ExceptionSink* xsink) {
    std::vector<std::string> names;
    std::lock_guard<std::mutex> lock(plugin_registry_mutex);
    names.reserve(plugin_modules.size());
    int n = 0;
    for (const auto& i : plugin_modules) {
        if (checkPluginRegistryCancel(n, xsink, "plugin registry module-name snapshot")) {
            names.clear();
            return names;
        }
        names.push_back(i.first);
        ++n;
    }
    return names;
}

std::set<std::string> getActivePluginModuleSet(QoreProgram* pgm, ExceptionSink* xsink) {
    std::set<std::string> active;
    std::vector<std::string> names = getProcessPluginModuleNames(xsink);
    if (hasException(xsink)) {
        return active;
    }

    int n = 0;
    for (const std::string& name : names) {
        if (checkPluginRegistryCancel(n, xsink, "plugin registry active-module filtering")) {
            active.clear();
            return active;
        }
        if (pluginProgramHasModule(pgm, name)) {
            active.insert(name);
        }
        ++n;
    }
    return active;
}

QoreHashNode* makeFallbackSiteHash(const qore_program_private_base::PluginFallbackSiteInfo& site,
        ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
    h->setKeyValue("file", site.file.empty() ? QoreValue() : QoreValue(new QoreStringNode(site.file)), xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("line", static_cast<int64>(site.line), xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("operation_name", new QoreStringNode(site.operation_name), xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("lhs_type", new QoreStringNode(site.lhs_type), xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("rhs_type", site.rhs_type.empty() ? QoreValue() : QoreValue(new QoreStringNode(site.rhs_type)),
        xsink);
    if (hasException(xsink)) {
        return nullptr;
    }
    h->setKeyValue("reason", new QoreStringNode(site.reason), xsink);
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

int qore_plugin_get_type_info(const char* module_name, uint16_t local_type_id,
        QorePluginResolvedTypeInfo& info, ExceptionSink* xsink) {
    info = QorePluginResolvedTypeInfo();
    std::lock_guard<std::mutex> lock(plugin_registry_mutex);
    auto i = plugin_modules.find(module_name ? module_name : "");
    if (i == plugin_modules.end()) {
        if (xsink) {
            xsink->raiseException("PLUGIN-REGISTRY-MODULE-NOT-LOADED",
                "plugin registry has no loaded module named \"%s\" "
                "(method=\"getTypeInfo\", subreason=\"module_not_loaded\", section=3.9)",
                escapeDiagnosticName(module_name).c_str());
        }
        return -1;
    }

    int n = 0;
    for (const RegisteredPluginType& type : i->second.types) {
        if (checkPluginRegistryCancel(n, xsink, "plugin registry type lookup")) {
            return -1;
        }
        if (type.local_type_id == local_type_id) {
            info.module_name = i->second.module_name;
            info.local_type_id = type.local_type_id;
            info.type_name = type.type_name;
            info.type_info = type.type_info;
            info.value_ops = type.value_ops;
            info.serialize = type.serialize;
            info.deserialize = type.deserialize;
            info.serializer_format_version = type.serializer_format_version;
            return 0;
        }
        ++n;
    }

    if (xsink) {
        xsink->raiseException("PLUGIN-REGISTRY-TYPE-NOT-REGISTERED",
            "plugin registry module \"%s\" has no local type id %u "
            "(method=\"getTypeInfo\", field=\"local_type_id\", expected=\"registered type id\", "
            "actual=\"missing\", subreason=\"type_not_registered\", section=3.9)",
            escapeDiagnosticName(module_name).c_str(), local_type_id);
    }
    return -1;
}

int qore_plugin_get_llvm_codegen_info(uint32_t global_operation_id, QorePluginLLVMCodegenInfo& info,
        ExceptionSink* xsink) {
    info = QorePluginLLVMCodegenInfo();
    std::lock_guard<std::mutex> lock(plugin_registry_mutex);
    auto gi = global_plugin_operations.find(global_operation_id);
    if (gi == global_plugin_operations.end()) {
        if (xsink) {
            xsink->raiseException("PLUGIN-REGISTRY-OPERATION-NOT-REGISTERED",
                "plugin registry has no global operation id %u "
                "(method=\"getLlvmCodegenInfo\", field=\"global_operation_id\", expected=\"registered id\", "
                "actual=\"missing\", subreason=\"operation_not_registered\", section=3.6)",
                global_operation_id);
        }
        return -1;
    }
    auto mi = plugin_modules.find(gi->second.module_name);
    if (mi == plugin_modules.end()) {
        if (xsink) {
            xsink->raiseException("PLUGIN-REGISTRY-MODULE-NOT-LOADED",
                "plugin registry has no loaded module named \"%s\" "
                "(method=\"getLlvmCodegenInfo\", subreason=\"module_not_loaded\", section=3.6)",
                escapeDiagnosticName(gi->second.module_name.c_str()).c_str());
        }
        return -1;
    }
    if (gi->second.operation_index >= mi->second.operations.size()) {
        if (xsink) {
            xsink->raiseException("PLUGIN-REGISTRY-OPERATION-NOT-REGISTERED",
                "plugin registry module \"%s\" has no operation at index %zu "
                "(method=\"getLlvmCodegenInfo\", field=\"operation_index\", expected=\"registered index\", "
                "actual=\"missing\", subreason=\"operation_not_registered\", section=3.6)",
                escapeDiagnosticName(mi->second.module_name.c_str()).c_str(), gi->second.operation_index);
        }
        return -1;
    }
    const RegisteredPluginOperation& op = mi->second.operations[gi->second.operation_index];
    if (op.local_id != gi->second.local_operation_id) {
        if (xsink) {
            xsink->raiseException("PLUGIN-REGISTRY-OPERATION-NOT-REGISTERED",
                "plugin registry module \"%s\" operation index %zu does not match local operation id %u "
                "(method=\"getLlvmCodegenInfo\", field=\"local_operation_id\", expected=\"registered id\", "
                "actual=\"%u\", subreason=\"operation_not_registered\", section=3.6)",
                escapeDiagnosticName(mi->second.module_name.c_str()).c_str(), gi->second.operation_index,
                gi->second.local_operation_id, op.local_id);
        }
        return -1;
    }

    info.module_name = mi->second.module_name;
    info.local_operation_id = op.local_id;
    info.operation_name = op.operation_name;
    info.signature = op.signature;
    info.info = op.info;
    info.codegen = op.llvm_codegen;
    return 0;
}

static bool loweringClaimMatches(uint64_t claimed_node_kinds, qore_type_t node_type) {
    return !claimed_node_kinds || (node_type >= 0 && node_type < 64 && (claimed_node_kinds & (1ULL << node_type)));
}

int qore_plugin_get_lowering_infos(QoreProgram* pgm, qore_type_t node_type,
        std::vector<QorePluginLoweringInfo>& infos, ExceptionSink* xsink) {
    infos.clear();
    std::set<std::string> active_modules;
    if (pgm) {
        active_modules = getActivePluginModuleSet(pgm, xsink);
        if (hasException(xsink)) {
            return -1;
        }
    }

    std::lock_guard<std::mutex> lock(plugin_registry_mutex);
    int n = 0;
    for (const auto& module_pair : plugin_modules) {
        const RegisteredPluginModule& module = module_pair.second;
        if (pgm && active_modules.find(module.module_name) == active_modules.end()) {
            continue;
        }
        for (const RegisteredPluginOperation& op : module.operations) {
            if (checkPluginRegistryCancel(n, xsink, "plugin registry lowering callback snapshot")) {
                infos.clear();
                return -1;
            }
            ++n;
            if (!op.lowering_pattern || !loweringClaimMatches(op.lowering_claimed_node_kinds, node_type)) {
                continue;
            }
            QorePluginLoweringInfo info;
            info.module_name = module.module_name;
            info.local_operation_id = op.local_id;
            info.operation_name = op.operation_name;
            info.info = op.info;
            info.claimed_node_kinds = op.lowering_claimed_node_kinds;
            info.lowering = op.lowering_pattern;
            infos.push_back(std::move(info));
        }
    }
    return 0;
}

int qore_plugin_resolve_program_operation_info(QoreProgram* pgm, const QoreTypeInfo* lhs_type,
        const QoreTypeInfo* rhs_type, const char* operation_name, QorePluginHelperAbi helper_abi,
        QorePluginResolvedOperationInfo& info, ExceptionSink* xsink) {
    info = QorePluginResolvedOperationInfo();
    if (!lhs_type || !operation_name || !*operation_name) {
        return 1;
    }

    std::set<std::string> active_modules;
    if (pgm) {
        active_modules = getActivePluginModuleSet(pgm, xsink);
        if (hasException(xsink)) {
            return -1;
        }
    }

    std::lock_guard<std::mutex> lock(plugin_registry_mutex);
    int n = 0;
    for (const auto& module_pair : plugin_modules) {
        const RegisteredPluginModule& module = module_pair.second;
        if (pgm && active_modules.find(module.module_name) == active_modules.end()) {
            continue;
        }
        for (const RegisteredPluginOperation& op : module.operations) {
            if (checkPluginRegistryCancel(n, xsink, "plugin registry typed operation resolution")) {
                return -1;
            }
            ++n;
            if (op.operation_name != operation_name || op.signature.helper_abi != helper_abi) {
                continue;
            }

            bool arity_ok = false;
            if (rhs_type) {
                arity_ok = op.signature.arity == 2;
            } else if (helper_abi == QorePluginHelperAbi::CallValueList) {
                arity_ok = op.signature.arity == 0xff;
            } else {
                arity_ok = op.signature.arity == 1;
            }
            if (!arity_ok) {
                continue;
            }
            bool may_not_match = false;
            if (!qore_type_is_assignable_from(op.signature.primary_type, lhs_type, may_not_match)
                    || may_not_match) {
                continue;
            }
            if (rhs_type && (!qore_type_is_assignable_from(op.signature.secondary_type, rhs_type, may_not_match)
                    || may_not_match)) {
                continue;
            }

            info.global_operation_id = op.global_id;
            info.module_name = module.module_name;
            info.local_operation_id = op.local_id;
            info.operation_name = op.operation_name;
            info.signature = op.signature;
            info.info = op.info;
            info.canonical_signature_version = op.canonical_signature_version;
            info.signature_hash = op.signature_hash;
            return 0;
        }
    }

    return 1;
}

static int resolveProgramOperationInfoForValues(QoreProgram* pgm, QoreValue lhs, const QoreValue* rhs,
        const char* operation_name, QorePluginHelperAbi helper_abi, QorePluginResolvedOperationInfo& info,
        ExceptionSink* xsink) {
    info = QorePluginResolvedOperationInfo();
    if (!operation_name || !*operation_name) {
        return 1;
    }

    std::set<std::string> active_modules;
    if (pgm) {
        active_modules = getActivePluginModuleSet(pgm, xsink);
        if (hasException(xsink)) {
            return -1;
        }
    }

    std::lock_guard<std::mutex> lock(plugin_registry_mutex);
    int n = 0;
    for (const auto& module_pair : plugin_modules) {
        const RegisteredPluginModule& module = module_pair.second;
        if (pgm && active_modules.find(module.module_name) == active_modules.end()) {
            continue;
        }
        for (const RegisteredPluginOperation& op : module.operations) {
            if (checkPluginRegistryCancel(n, xsink, "plugin registry value operation resolution")) {
                return -1;
            }
            ++n;
            if (op.operation_name != operation_name || op.signature.helper_abi != helper_abi) {
                continue;
            }

            bool arity_ok = false;
            if (rhs) {
                arity_ok = op.signature.arity == 2;
            } else if (helper_abi == QorePluginHelperAbi::CallValueList) {
                arity_ok = op.signature.arity == 0xff;
            } else {
                arity_ok = op.signature.arity == 1;
            }
            if (!arity_ok) {
                continue;
            }
            if (qore_type_is_assignable_from(op.signature.primary_type, lhs) == QTI_NOT_EQUAL) {
                continue;
            }
            if (rhs && qore_type_is_assignable_from(op.signature.secondary_type, *rhs) == QTI_NOT_EQUAL) {
                continue;
            }

            info.global_operation_id = op.global_id;
            info.module_name = module.module_name;
            info.local_operation_id = op.local_id;
            info.operation_name = op.operation_name;
            info.signature = op.signature;
            info.info = op.info;
            info.canonical_signature_version = op.canonical_signature_version;
            info.signature_hash = op.signature_hash;
            return 0;
        }
    }

    return 1;
}

QoreValue qore_plugin_try_dispatch_unary(QoreProgram* pgm, const char* operation_name,
        QorePluginHelperAbi helper_abi, QoreValue value, bool& matched, ExceptionSink* xsink) {
    matched = false;
    if (helper_abi != QorePluginHelperAbi::UnaryValue) {
        return QoreValue();
    }

    QorePluginResolvedOperationInfo info;
    int rc = resolveProgramOperationInfoForValues(pgm, value, nullptr, operation_name, helper_abi, info, xsink);
    if (rc) {
        return QoreValue();
    }

    matched = true;
    uint64_t rv = qore_rt_plugin_unary(info.global_operation_id, bitsFromValue(value), xsink);
    return valueFromBits(rv);
}

QoreValue qore_plugin_try_dispatch_binary(QoreProgram* pgm, const char* operation_name,
        QorePluginHelperAbi helper_abi, QoreValue lhs, QoreValue rhs, bool& matched, ExceptionSink* xsink) {
    matched = false;
    if (helper_abi != QorePluginHelperAbi::BinaryValue && helper_abi != QorePluginHelperAbi::SubscriptValue) {
        return QoreValue();
    }

    QorePluginResolvedOperationInfo info;
    int rc = resolveProgramOperationInfoForValues(pgm, lhs, &rhs, operation_name, helper_abi, info, xsink);
    if (rc) {
        return QoreValue();
    }

    matched = true;
    uint64_t rv = helper_abi == QorePluginHelperAbi::SubscriptValue
        ? qore_rt_plugin_subscript(info.global_operation_id, bitsFromValue(lhs), bitsFromValue(rhs), xsink)
        : qore_rt_plugin_binary(info.global_operation_id, bitsFromValue(lhs), bitsFromValue(rhs), xsink);
    return valueFromBits(rv);
}

QoreValue qore_plugin_try_dispatch_call(QoreProgram* pgm, const char* operation_name, QoreValue self,
        const QoreListNode* args, bool& matched, ExceptionSink* xsink) {
    matched = false;
    QorePluginResolvedOperationInfo info;
    int rc = resolveProgramOperationInfoForValues(pgm, self, nullptr, operation_name,
        QorePluginHelperAbi::CallValueList, info, xsink);
    if (rc) {
        return QoreValue();
    }

    matched = true;
    QoreValue args_value(const_cast<QoreListNode*>(args));
    uint64_t rv = qore_rt_plugin_call(info.global_operation_id, bitsFromValue(self), bitsFromValue(args_value),
        xsink);
    return valueFromBits(rv);
}

bool qore_plugin_is_value_node(const AbstractQoreNode* node) {
    return get_node_type(node) == NT_PLUGIN_VALUE;
}

const QoreTypeInfo* qore_plugin_get_value_type_info(const AbstractQoreNode* node) {
    if (!qore_plugin_is_value_node(node)) {
        return nullptr;
    }
    const QorePluginValueNode* value = static_cast<const QorePluginValueNode*>(node);
    return value->getPluginType().type_info;
}

bool qore_plugin_get_value_profile_info(const AbstractQoreNode* node, std::string& module_name,
        uint16_t& local_type_id, const QoreTypeInfo*& type_info) {
    if (!qore_plugin_is_value_node(node)) {
        return false;
    }
    const QorePluginValueNode* value = static_cast<const QorePluginValueNode*>(node);
    const QorePluginResolvedTypeInfo& type = value->getPluginType();
    module_name = type.module_name;
    local_type_id = type.local_type_id;
    type_info = type.type_info;
    return true;
}

int qore_plugin_serialize_value_node(const AbstractQoreNode* node,
        QorePluginSerializedValueInfo& info, ExceptionSink* xsink) {
    info = QorePluginSerializedValueInfo();
    if (!qore_plugin_is_value_node(node)) {
        if (xsink) {
            xsink->raiseException("PLUGIN-SERIALIZATION-ERROR",
                "cannot serialize non-plugin value node as a plugin value instance "
                "(field=\"type\", expected=\"plugin value\", actual=\"%s\", "
                "subreason=\"not_plugin_value\", section=3.9)",
                get_type_name(node));
        }
        return -1;
    }

    const QorePluginValueNode* value = static_cast<const QorePluginValueNode*>(node);
    const QorePluginResolvedTypeInfo& type = value->getPluginType();
    if (!type.serialize) {
        if (xsink) {
            xsink->raiseException("PLUGIN-SERIALIZATION-ERROR",
                "plugin value type \"%s:%s\" does not provide a serializer "
                "(field=\"serialize\", expected=\"serializer callback\", actual=\"null\", "
                "subreason=\"serializer_missing\", section=3.9)",
                escapeDiagnosticName(type.module_name.c_str()).c_str(),
                escapeDiagnosticName(type.type_name.c_str()).c_str());
        }
        return -1;
    }

    PluginByteVectorWriter writer;
    if (type.serialize(value->getValueBits(), pluginByteVectorWrite, &writer, xsink) || hasException(xsink)) {
        return -1;
    }

    info.module_name = type.module_name;
    info.local_type_id = type.local_type_id;
    info.serializer_format_version = type.serializer_format_version;
    info.payload = std::move(writer.payload);
    return 0;
}

QoreValue qore_plugin_deserialize_value(const char* module_name, uint16_t local_type_id,
        uint16_t serializer_format_version, const uint8_t* payload, uint32_t payload_len, ExceptionSink* xsink) {
    QorePluginResolvedTypeInfo type;
    if (qore_plugin_get_type_info(module_name, local_type_id, type, xsink)) {
        return QoreValue();
    }
    if (type.serializer_format_version != serializer_format_version) {
        if (xsink) {
            xsink->raiseException("PLUGIN-DESERIALIZATION-ERROR",
                "plugin value type \"%s:%s\" serializer format version mismatch "
                "(field=\"serializer_format_version\", expected=\"%u\", actual=\"%u\", "
                "subreason=\"serializer_version_mismatch\", section=3.9)",
                escapeDiagnosticName(type.module_name.c_str()).c_str(),
                escapeDiagnosticName(type.type_name.c_str()).c_str(),
                type.serializer_format_version, serializer_format_version);
        }
        return QoreValue();
    }
    if (!type.deserialize) {
        if (xsink) {
            xsink->raiseException("PLUGIN-DESERIALIZATION-ERROR",
                "plugin value type \"%s:%s\" does not provide a deserializer "
                "(field=\"deserialize\", expected=\"deserializer callback\", actual=\"null\", "
                "subreason=\"deserializer_missing\", section=3.9)",
                escapeDiagnosticName(type.module_name.c_str()).c_str(),
                escapeDiagnosticName(type.type_name.c_str()).c_str());
        }
        return QoreValue();
    }
    if (payload_len && !payload) {
        if (xsink) {
            xsink->raiseException("PLUGIN-DESERIALIZATION-ERROR",
                "plugin value payload pointer is null with non-zero payload length "
                "(field=\"payload\", expected=\"valid data pointer\", actual=\"null\", "
                "subreason=\"payload_null\", section=3.9)");
        }
        return QoreValue();
    }

    PluginByteVectorReader reader{payload, payload_len, 0};
    uint64_t value_bits = type.deserialize(pluginByteVectorRead, payload_len, &reader, xsink);
    if (hasException(xsink)) {
        return QoreValue();
    }
    if (reader.offset != payload_len) {
        if (xsink) {
            xsink->raiseException("PLUGIN-DESERIALIZATION-ERROR",
                "plugin value deserializer consumed %u of %u payload byte(s) "
                "(field=\"payload_length\", expected=\"all bytes consumed\", actual=\"%u remaining byte(s)\", "
                "subreason=\"payload_trailing_bytes\", section=3.9)",
                reader.offset, payload_len, payload_len - reader.offset);
        }
        if (type.value_ops.decref) {
            type.value_ops.decref(value_bits);
        }
        return QoreValue();
    }

    return QoreValue(new QorePluginValueNode(type, value_bits));
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
        constexpr uint32_t flag_field_end = static_cast<uint32_t>(offsetof(QorePluginValidationContext,
            module_handle));
        constexpr uint32_t module_handle_field_end = static_cast<uint32_t>(
            offsetof(QorePluginValidationContext, module_handle) + sizeof(ctx->module_handle));

        if (ctx->struct_size < flag_field_end) {
            state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", reg ? reg->module_name : nullptr, nullptr,
                "validation_context.struct_size", "at least offsetof(QorePluginValidationContext, module_handle)",
                std::to_string(ctx->struct_size).c_str(), "validation_context_too_small", "3.12");
            return -1;
        }
        if (ctx->flags) {
            if (state.fail("PLUGIN-REGISTRATION-INVALID-DESCRIPTOR", reg ? reg->module_name : nullptr, nullptr,
                    "validation_context.flags", "0", std::to_string(ctx->flags).c_str(),
                    "unknown_validation_context_flag", "3.12")) {
                return -1;
            }
        }
        if (ctx->struct_size >= module_handle_field_end && ctx->module_handle) {
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

extern "C" AbstractQoreNode* qore_plugin_make_value_v1(const char* module_name, uint16_t local_type_id,
        uint64_t value_bits, uint32_t flags, ExceptionSink* xsink) {
    if (flags & QORE_PLUGIN_VALUE_CREATE_RESERVED_MASK) {
        if (xsink) {
            xsink->raiseException("PLUGIN-VALUE-CREATE-ERROR",
                "cannot create plugin value \"%s:%u\": unsupported create flags 0x%08x "
                "(field=\"flags\", expected=\"QORE_PLUGIN_VALUE_ADOPT or QORE_PLUGIN_VALUE_BORROWED\", "
                "actual=\"reserved bits set\", subreason=\"reserved_flags\", section=3.9)",
                escapeDiagnosticName(module_name).c_str(), local_type_id, flags);
        }
        return nullptr;
    }

    QorePluginResolvedTypeInfo type;
    if (qore_plugin_get_type_info(module_name, local_type_id, type, xsink)) {
        return nullptr;
    }
    if (flags & QORE_PLUGIN_VALUE_BORROWED) {
        if (type.value_ops.incref) {
            type.value_ops.incref(value_bits);
        }
    }
    return new QorePluginValueNode(type, value_bits);
}

extern "C" int qore_plugin_get_value_bits_v1(const AbstractQoreNode* node, const char** module_name,
        uint16_t* local_type_id, uint64_t* value_bits, ExceptionSink* xsink) {
    if (!qore_plugin_is_value_node(node)) {
        if (xsink) {
            xsink->raiseException("PLUGIN-VALUE-ACCESS-ERROR",
                "cannot extract plugin value bits from type \"%s\" "
                "(field=\"type\", expected=\"plugin value\", actual=\"%s\", "
                "subreason=\"not_plugin_value\", section=3.9)",
                get_type_name(node), get_type_name(node));
        }
        return -1;
    }
    const QorePluginValueNode* value = static_cast<const QorePluginValueNode*>(node);
    const QorePluginResolvedTypeInfo& type = value->getPluginType();
    if (module_name) {
        *module_name = type.module_name.c_str();
    }
    if (local_type_id) {
        *local_type_id = type.local_type_id;
    }
    if (value_bits) {
        *value_bits = value->getValueBits();
    }
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

QoreListNode* qore_plugin_get_program_modules(QoreProgram* pgm, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(stringTypeInfo), xsink);
    std::vector<std::string> names = getProcessPluginModuleNames(xsink);
    if (hasException(xsink)) {
        return nullptr;
    }

    int n = 0;
    for (const std::string& name : names) {
        if (checkPluginRegistryCancel(n, xsink, "plugin registry active-module introspection")) {
            return nullptr;
        }
        if (pluginProgramHasModule(pgm, name)) {
            rv->push(new QoreStringNode(name), xsink);
            if (hasException(xsink)) {
                return nullptr;
            }
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

QoreListNode* qore_plugin_get_program_types(QoreProgram* pgm, const char* module_name, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    const bool active = pluginProgramHasModule(pgm, module_name ? module_name : "");
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
    if (!active) {
        return rv.release();
    }

    int n = 0;
    for (const RegisteredPluginType& type : i->second.types) {
        if (checkPluginRegistryCancel(n, xsink, "plugin registry active type introspection")) {
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

QoreListNode* qore_plugin_get_program_operations(QoreProgram* pgm, const char* module_name, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    const bool active = pluginProgramHasModule(pgm, module_name ? module_name : "");
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
    if (!active) {
        return rv.release();
    }

    int n = 0;
    for (const RegisteredPluginOperation& op : i->second.operations) {
        if (checkPluginRegistryCancel(n, xsink, "plugin registry active operation introspection")) {
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

QoreHashNode* qore_plugin_resolve_process_operation(const QoreTypeInfo* lhs_type, const QoreTypeInfo* rhs_type,
        const char* operation_name, ExceptionSink* xsink) {
    if (!lhs_type || !operation_name || !*operation_name) {
        traceCrossType("resolve: missing lhs type or operation name");
        return nullptr;
    }

    const char* lhs_path = qore_type_get_path(lhs_type);
    const char* rhs_path = rhs_type ? qore_type_get_path(rhs_type) : "<none>";
    traceCrossType("resolve: operation='" + std::string(operation_name) + "' lhs='" + lhs_path + "' rhs='"
        + rhs_path + "'");

    std::lock_guard<std::mutex> lock(plugin_registry_mutex);
    int n = 0;
    for (const auto& module_pair : plugin_modules) {
        const RegisteredPluginModule& module = module_pair.second;
        for (const RegisteredPluginOperation& op : module.operations) {
            if (checkPluginRegistryCancel(n, xsink, "plugin registry operation resolution")) {
                return nullptr;
            }
            ++n;
            if (op.operation_name != operation_name) {
                continue;
            }

            bool arity_ok = rhs_type ? op.signature.arity == 2 : op.signature.arity == 1;
            if (!arity_ok) {
                traceCrossType("resolve: skipped '" + module.module_name + "::" + op.operation_name
                    + "' due to arity");
                continue;
            }
            if (!qore_type_is_assignable_from(op.signature.primary_type, lhs_type)) {
                traceCrossType("resolve: skipped '" + module.module_name + "::" + op.operation_name
                    + "' due to primary type");
                continue;
            }
            if (rhs_type && !qore_type_is_assignable_from(op.signature.secondary_type, rhs_type)) {
                traceCrossType("resolve: skipped '" + module.module_name + "::" + op.operation_name
                    + "' due to secondary type");
                continue;
            }

            traceCrossType("resolve: selected '" + module.module_name + "::" + op.operation_name
                + "' local_id=" + std::to_string(op.local_id));
            return makeOperationHash(module, op, xsink);
        }
    }

    traceCrossType("resolve: no matching operation");
    return nullptr;
}

QoreHashNode* qore_plugin_resolve_program_operation(QoreProgram* pgm, const QoreTypeInfo* lhs_type,
        const QoreTypeInfo* rhs_type, const char* operation_name, ExceptionSink* xsink) {
    if (!lhs_type || !operation_name || !*operation_name) {
        traceCrossType("program resolve: missing lhs type or operation name");
        return nullptr;
    }

    std::set<std::string> active_modules = getActivePluginModuleSet(pgm, xsink);
    if (hasException(xsink)) {
        return nullptr;
    }

    const char* lhs_path = qore_type_get_path(lhs_type);
    const char* rhs_path = rhs_type ? qore_type_get_path(rhs_type) : "<none>";
    traceCrossType("program resolve: operation='" + std::string(operation_name) + "' lhs='" + lhs_path + "' rhs='"
        + rhs_path + "' active_modules=" + std::to_string(active_modules.size()));

    std::lock_guard<std::mutex> lock(plugin_registry_mutex);
    int n = 0;
    for (const auto& module_pair : plugin_modules) {
        const RegisteredPluginModule& module = module_pair.second;
        if (active_modules.find(module.module_name) == active_modules.end()) {
            traceCrossType("program resolve: skipped inactive module '" + module.module_name + "'");
            continue;
        }
        for (const RegisteredPluginOperation& op : module.operations) {
            if (checkPluginRegistryCancel(n, xsink, "plugin registry program operation resolution")) {
                return nullptr;
            }
            ++n;
            if (op.operation_name != operation_name) {
                continue;
            }

            bool arity_ok = rhs_type ? op.signature.arity == 2 : op.signature.arity == 1;
            if (!arity_ok) {
                traceCrossType("program resolve: skipped '" + module.module_name + "::" + op.operation_name
                    + "' due to arity");
                continue;
            }
            if (!qore_type_is_assignable_from(op.signature.primary_type, lhs_type)) {
                traceCrossType("program resolve: skipped '" + module.module_name + "::" + op.operation_name
                    + "' due to primary type");
                continue;
            }
            if (rhs_type && !qore_type_is_assignable_from(op.signature.secondary_type, rhs_type)) {
                traceCrossType("program resolve: skipped '" + module.module_name + "::" + op.operation_name
                    + "' due to secondary type");
                continue;
            }

            traceCrossType("program resolve: selected '" + module.module_name + "::" + op.operation_name
                + "' local_id=" + std::to_string(op.local_id));
            return makeOperationHash(module, op, xsink);
        }
    }

    traceCrossType("program resolve: no matching active operation");
    return nullptr;
}

void qore_plugin_record_fallback_site(QoreProgram* pgm, const char* file, int line, const char* operation_name,
        const QoreTypeInfo* lhs_type, const QoreTypeInfo* rhs_type, const char* reason) {
    if (!pgm) {
        return;
    }

    qore_program_private_base::PluginFallbackSiteInfo info;
    info.file = file ? file : "";
    info.line = line;
    info.operation_name = operation_name ? operation_name : "";
    info.lhs_type = lhs_type ? qore_type_get_path(lhs_type) : "<unknown>";
    info.rhs_type = rhs_type ? qore_type_get_path(rhs_type) : "";
    info.reason = reason ? reason : "";
    qore_program_private::get(*pgm)->recordPluginFallbackSite(std::move(info), pluginFallbackBufferLimit());
}

QoreListNode* qore_plugin_get_recent_fallback_sites(QoreProgram* pgm, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    if (!pgm) {
        return rv.release();
    }

    std::vector<qore_program_private_base::PluginFallbackSiteInfo> sites =
        qore_program_private::get(*pgm)->getPluginFallbackSites();
    int n = 0;
    for (const qore_program_private_base::PluginFallbackSiteInfo& site : sites) {
        if (checkPluginRegistryCancel(n, xsink, "plugin fallback-site introspection")) {
            return nullptr;
        }
        QoreHashNode* h = makeFallbackSiteHash(site, xsink);
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

void qore_plugin_clear_fallback_sites(QoreProgram* pgm) {
    if (pgm) {
        qore_program_private::get(*pgm)->clearPluginFallbackSites();
    }
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
    if (pluginDispatchTrace()) {
        traceDispatch("unary id=" + std::to_string(global_operation_id) + " module='" + op.module_name
            + "' operation='" + op.operation_name + "' helper="
            + std::to_string(reinterpret_cast<uintptr_t>(op.runtime_helper)));
    }
    auto helper = reinterpret_cast<PluginUnaryHelper>(op.runtime_helper);
    uint64_t rv = helper(value_bits, xsink);
    if (xsink && *xsink) {
        return nothingBits();
    }
    return verifyPluginResult(op, "unary", rv, value_bits, true, 0, false, xsink) ? nothingBits() : rv;
}

extern "C" uint64_t qore_rt_plugin_binary(uint32_t global_operation_id, uint64_t lhs_bits,
        uint64_t rhs_bits, ExceptionSink* xsink) {
    ResolvedPluginOperation op;
    if (resolvePluginOperation(global_operation_id, QorePluginHelperAbi::BinaryValue, op, xsink)) {
        return nothingBits();
    }
    if (pluginDispatchTrace()) {
        traceDispatch("binary id=" + std::to_string(global_operation_id) + " module='" + op.module_name
            + "' operation='" + op.operation_name + "' helper="
            + std::to_string(reinterpret_cast<uintptr_t>(op.runtime_helper)));
    }
    auto helper = reinterpret_cast<PluginBinaryHelper>(op.runtime_helper);
    uint64_t rv = helper(lhs_bits, rhs_bits, xsink);
    if (xsink && *xsink) {
        return nothingBits();
    }
    return verifyPluginResult(op, "binary", rv, lhs_bits, true, rhs_bits, true, xsink) ? nothingBits() : rv;
}

extern "C" uint64_t qore_rt_plugin_call(uint32_t global_operation_id, uint64_t self_bits,
        uint64_t args_list_bits, ExceptionSink* xsink) {
    ResolvedPluginOperation op;
    if (resolvePluginOperation(global_operation_id, QorePluginHelperAbi::CallValueList, op, xsink)) {
        return nothingBits();
    }
    if (pluginDispatchTrace()) {
        traceDispatch("call id=" + std::to_string(global_operation_id) + " module='" + op.module_name
            + "' operation='" + op.operation_name + "' helper="
            + std::to_string(reinterpret_cast<uintptr_t>(op.runtime_helper)));
    }
    auto helper = reinterpret_cast<PluginCallHelper>(op.runtime_helper);
    uint64_t rv = helper(self_bits, args_list_bits, xsink);
    if (xsink && *xsink) {
        return nothingBits();
    }
    return verifyPluginResult(op, "call", rv, self_bits, true, args_list_bits, true, xsink) ? nothingBits() : rv;
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
    if (pluginDispatchTrace()) {
        traceDispatch("subscript id=" + std::to_string(global_operation_id) + " module='" + op.module_name
            + "' operation='" + op.operation_name + "' helper="
            + std::to_string(reinterpret_cast<uintptr_t>(op.runtime_helper)));
    }
    auto helper = reinterpret_cast<PluginBinaryHelper>(op.runtime_helper);
    uint64_t rv = helper(container_bits, key_bits, xsink);
    if (xsink && *xsink) {
        return nothingBits();
    }
    return verifyPluginResult(op, "subscript", rv, container_bits, true, key_bits, true, xsink)
        ? nothingBits() : rv;
}

extern "C" uint64_t qore_rt_plugin_construct(uint32_t global_operation_id, uint64_t args_list_bits,
        ExceptionSink* xsink) {
    ResolvedPluginOperation op;
    if (resolvePluginOperation(global_operation_id, QorePluginHelperAbi::Construct, op, xsink)) {
        return nothingBits();
    }
    if (pluginDispatchTrace()) {
        traceDispatch("construct id=" + std::to_string(global_operation_id) + " module='" + op.module_name
            + "' operation='" + op.operation_name + "' helper="
            + std::to_string(reinterpret_cast<uintptr_t>(op.runtime_helper)));
    }
    auto helper = reinterpret_cast<PluginConstructHelper>(op.runtime_helper);
    uint64_t rv = helper(args_list_bits, xsink);
    if (xsink && *xsink) {
        return nothingBits();
    }
    return verifyPluginResult(op, "construct", rv, 0, false, args_list_bits, true, xsink) ? nothingBits() : rv;
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
    return dispatchResolvedDenseBufferUnary(op, result_buffer_data, result_size, value_data, value_size,
        value_stride, xsink);
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
    return dispatchResolvedDenseBufferBinary(op, result_buffer_data, result_size, lhs_data, lhs_size, lhs_stride,
        rhs_data, rhs_size, rhs_stride, xsink);
}

extern "C" uint64_t qore_rt_plugin_dense_buffer_unary_values(uint32_t global_operation_id,
        uint64_t result_buffer_bits, uint64_t value_bits, ExceptionSink* xsink) {
    ResolvedPluginOperation op;
    if (resolvePluginOperation(global_operation_id, QorePluginHelperAbi::DenseBufferUnary, op, xsink)) {
        return nothingBits();
    }

    DenseBufferValueFrame result;
    DenseBufferValueFrame value;
    if (prepareDenseBufferValueFrame(op, "dense-buffer-unary", "result", result_buffer_bits, true, result, xsink)
            || prepareDenseBufferValueFrame(op, "dense-buffer-unary", "value", value_bits, false, value, xsink)
            || validateDenseBufferNoAlias(op, "dense-buffer-unary", result, value, "value", xsink)
            || validateDenseBufferSameElementType(op, "dense-buffer-unary", result, value, "value", xsink)
            || validateDenseBufferFrame("dense-buffer unary plugin helper", result.mutable_data, result.size,
                value.const_data, value.size, nullptr, 0, xsink)) {
        return nothingBits();
    }

    return dispatchResolvedDenseBufferUnary(op, result.mutable_data, result.size, value.const_data, value.size,
        value.stride, xsink);
}

extern "C" uint64_t qore_rt_plugin_dense_buffer_binary_values(uint32_t global_operation_id,
        uint64_t result_buffer_bits, uint64_t lhs_bits, uint64_t rhs_bits, ExceptionSink* xsink) {
    ResolvedPluginOperation op;
    if (resolvePluginOperation(global_operation_id, QorePluginHelperAbi::DenseBufferBinary, op, xsink)) {
        return nothingBits();
    }

    DenseBufferValueFrame result;
    DenseBufferValueFrame lhs;
    DenseBufferValueFrame rhs;
    if (prepareDenseBufferValueFrame(op, "dense-buffer-binary", "result", result_buffer_bits, true, result, xsink)
            || prepareDenseBufferValueFrame(op, "dense-buffer-binary", "lhs", lhs_bits, false, lhs, xsink)
            || prepareDenseBufferValueFrame(op, "dense-buffer-binary", "rhs", rhs_bits, false, rhs, xsink)
            || validateDenseBufferNoAlias(op, "dense-buffer-binary", result, lhs, "lhs", xsink)
            || validateDenseBufferNoAlias(op, "dense-buffer-binary", result, rhs, "rhs", xsink)
            || validateDenseBufferSameElementType(op, "dense-buffer-binary", result, lhs, "lhs", xsink)
            || validateDenseBufferSameElementType(op, "dense-buffer-binary", result, rhs, "rhs", xsink)
            || validateDenseBufferFrame("dense-buffer binary plugin helper", result.mutable_data, result.size,
                lhs.const_data, lhs.size, rhs.const_data, rhs.size, xsink)) {
        return nothingBits();
    }

    return dispatchResolvedDenseBufferBinary(op, result.mutable_data, result.size, lhs.const_data, lhs.size,
        lhs.stride, rhs.const_data, rhs.size, rhs.stride, xsink);
}

extern "C" int64_t qore_rt_guard_plugin_type_profiled(uint64_t value_bits, const QoreTypeInfo* type_info,
        const char* module_name, uint32_t local_type_id) {
    QoreValue value = valueFromBits(value_bits);
    if (value.getType() == NT_PLUGIN_VALUE && module_name) {
        const QorePluginValueNode* plugin_value = static_cast<const QorePluginValueNode*>(value.getInternalNode());
        const QorePluginResolvedTypeInfo& plugin_type = plugin_value->getPluginType();
        if (plugin_type.local_type_id == local_type_id
                && plugin_type.type_info == type_info
                && plugin_type.module_name == module_name) {
            return 1;
        }
    }
    return QoreTypeInfo::runtimeAcceptsValue(type_info, value) != QTI_NOT_EQUAL ? 1 : 0;
}
