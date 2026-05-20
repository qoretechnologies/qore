/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    sample-buffer.cpp

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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
*/

#include <qore/Qore.h>
#include <qore/QorePluginType.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

static void sampleIncref(uint64_t) noexcept {
}

static void sampleDecref(uint64_t) noexcept {
}

static uint64_t sampleClone(uint64_t value_bits, ExceptionSink*) {
    return value_bits;
}

static bool sampleEqual(uint64_t lhs_bits, uint64_t rhs_bits, ExceptionSink*) {
    return lhs_bits == rhs_bits;
}

static int64_t sampleHash(uint64_t value_bits, ExceptionSink*) {
    return static_cast<int64_t>(value_bits);
}

static void sampleCleanup(uint64_t) noexcept {
}

static int sampleSerialize(uint64_t value_bits, QorePluginByteWriteCallback write,
        void* write_user_data, ExceptionSink* xsink) {
    uint8_t bytes[sizeof(uint64_t)];
    for (size_t i = 0; i < sizeof(bytes); ++i) {
        bytes[i] = static_cast<uint8_t>((value_bits >> (i * 8)) & 0xff);
    }
    return write(bytes, sizeof(bytes), write_user_data, xsink);
}

static uint64_t sampleDeserialize(QorePluginByteReadCallback read, uint32_t payload_len,
        void* read_user_data, ExceptionSink* xsink) {
    if (payload_len != sizeof(uint64_t)) {
        if (xsink) {
            xsink->raiseException("SAMPLE-BUFFER-SERIALIZATION-ERROR",
                "sample-buffer value payload length must be 8 bytes, got %u", payload_len);
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

static QoreValue sampleValueFromBits(uint64_t bits) {
    QoreValue value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint64_t sampleBitsFromValue(const QoreValue& value) {
    uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint64_t sampleAdd(uint64_t lhs_bits, uint64_t rhs_bits, ExceptionSink*) {
    QoreValue lhs = sampleValueFromBits(lhs_bits);
    QoreValue rhs = sampleValueFromBits(rhs_bits);
    return sampleBitsFromValue(QoreValue(lhs.getAsBigInt() + rhs.getAsBigInt()));
}

static uint64_t sampleDenseAddI64(void* result_buffer_data, int64_t result_size,
        const void* lhs_data, int64_t lhs_size, int64_t lhs_stride,
        const void* rhs_data, int64_t rhs_size, int64_t rhs_stride,
        ExceptionSink* xsink) {
    int64_t* result = static_cast<int64_t*>(result_buffer_data);
    const int64_t* lhs = static_cast<const int64_t*>(lhs_data);
    const int64_t* rhs = static_cast<const int64_t*>(rhs_data);
    int64_t size = std::min(result_size, std::min(lhs_size, rhs_size));

    for (int64_t i = 0; i < size; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "sample-buffer dense_add_i64 helper")) {
            return sampleBitsFromValue(QoreValue());
        }
        result[i] = lhs[i * lhs_stride] + rhs[i * rhs_stride];
    }
    return sampleBitsFromValue(QoreValue());
}

static QorePluginLoweringResult sampleLoweringHook(QoreIRLoweringContext*, const AbstractQoreNode*,
        const QoreParseContext*, QoreIRBuilder*) {
    return QorePluginLoweringResult::NotApplicable;
}

static QorePluginValueOps sampleValueOps() {
    QorePluginValueOps ops = {};
    ops.incref = sampleIncref;
    ops.decref = sampleDecref;
    ops.clone = sampleClone;
    ops.equal = sampleEqual;
    ops.hash = sampleHash;
    ops.cleanup_slot = sampleCleanup;
    return ops;
}

static QorePluginOpcodeInfoExtended samplePureInfo(bool vectorizable) {
    QorePluginOpcodeInfoExtended info = {};
    info.may_have_side_effects = false;
    info.may_throw_exception = false;
    info.never_returns_nothing = true;
    info.is_pure_modulo_xsink = true;
    info.can_vectorize = vectorizable;
    info.type_promotion_kind = QorePluginOpcodeTypePromotion::Exact;
    info.is_simd_friendly = vectorizable;
    info.cost_class = vectorizable ? 1 : 0;
    return info;
}

static QorePluginTypeRegistration sampleRegistration(QorePluginTypeDescriptor& type,
        std::array<QorePluginOperation, 2>& ops) {
    type = {};
    type.local_type_id = 0;
    type.type_name = "SampleBufferValue";
    type.type_info = autoTypeInfo;
    type.value_ops = sampleValueOps();
    type.serialize = sampleSerialize;
    type.deserialize = sampleDeserialize;
    type.serializer_format_version = 1;
    type.baseline_qdom_domains = QDOM_DEFAULT;

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
    ops[0].info = samplePureInfo(false);
    ops[0].runtime_helper = reinterpret_cast<void (*)()>(sampleAdd);
    ops[0].runtime_helper_symbol = "sample_buffer_add";
    ops[0].lowering_pattern = sampleLoweringHook;
    ops[0].qdom_domains = QDOM_DEFAULT;

    ops[1] = {};
    ops[1].local_id = 1;
    ops[1].operation_name = "dense_add_i64";
    ops[1].signature.arity = 2;
    ops[1].signature.primary_type = autoTypeInfo;
    ops[1].signature.secondary_type = autoTypeInfo;
    ops[1].signature.return_type = autoTypeInfo;
    ops[1].signature.access = QorePluginValueAccess::ReadOnly;
    ops[1].signature.result_alias = QorePluginResultAlias::FreshNoAliasInputs;
    ops[1].signature.helper_abi = QorePluginHelperAbi::DenseBufferBinary;
    ops[1].info = samplePureInfo(true);
    ops[1].runtime_helper = reinterpret_cast<void (*)()>(sampleDenseAddI64);
    ops[1].runtime_helper_symbol = "sample_buffer_dense_add_i64";
    ops[1].lowering_pattern = sampleLoweringHook;
    ops[1].qdom_domains = QDOM_DEFAULT;

    QorePluginTypeRegistration reg = {};
    reg.module_name = "sample-buffer";
    reg.plugin_abi_version = QORE_PLUGIN_ABI_VERSION_V1;
    reg.operation_set_version = "1.0.0";
    reg.types = &type;
    reg.num_types = 1;
    reg.operations = ops.data();
    reg.num_operations = static_cast<int>(ops.size());
    return reg;
}

extern "C" DLLEXPORT uint64_t sample_buffer_add(uint64_t lhs_bits, uint64_t rhs_bits,
        ExceptionSink* xsink) {
    return sampleAdd(lhs_bits, rhs_bits, xsink);
}

extern "C" DLLEXPORT uint64_t sample_buffer_dense_add_i64(void* result_buffer_data,
        int64_t result_size, const void* lhs_data, int64_t lhs_size, int64_t lhs_stride,
        const void* rhs_data, int64_t rhs_size, int64_t rhs_stride, ExceptionSink* xsink) {
    return sampleDenseAddI64(result_buffer_data, result_size, lhs_data, lhs_size, lhs_stride,
        rhs_data, rhs_size, rhs_stride, xsink);
}

static void sampleBufferModuleInit(QoreModuleInitContext& module_ctx, ExceptionSink& xsink) {
    QorePluginTypeDescriptor type;
    std::array<QorePluginOperation, 2> ops;
    QorePluginTypeRegistration reg = sampleRegistration(type, ops);

    QorePluginRegistrationContextV1 plugin_ctx = {};
    plugin_ctx.struct_size = sizeof(plugin_ctx);
    plugin_ctx.module_path = module_ctx.path.c_str();
    plugin_ctx.module_handle = module_ctx.plugin_module_handle;
    qore_register_plugin_types_v1(&plugin_ctx, &reg, &xsink);
}

static void sampleBufferModuleNsInit(QoreNamespace*, QoreNamespace*, ExceptionSink&) {
}

static void sampleBufferModuleDelete() {
}

extern "C" DLLEXPORT void sample_buffer_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "sample-buffer";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "sample plugin-type module";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = sampleBufferModuleInit;
    mod_info.ns_init = sampleBufferModuleNsInit;
    mod_info.del = sampleBufferModuleDelete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}
