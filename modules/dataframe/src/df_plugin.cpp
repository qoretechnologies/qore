/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    df_plugin.cpp

    DataFrame plugin-type registration and dispatch hooks

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "df_plugin.h"

#include "QC_DataFrame.h"

#include <qore/QorePluginType.h>

#include <array>
#include <cstring>

using namespace QoreDataFrameNS;

extern "C" DLLEXPORT uint64_t dataframe_subscript_column(uint64_t container_bits, uint64_t key_bits,
    ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t dataframe_subscript_row(uint64_t container_bits, uint64_t key_bits,
    ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t dataframe_slice(uint64_t self_bits, uint64_t args_bits, ExceptionSink* xsink);

namespace {

constexpr uint16_t DATAFRAME_TYPE_ID = 0;
constexpr uint16_t DATAFRAME_OP_SUBSCRIPT_COLUMN = 0;
constexpr uint16_t DATAFRAME_OP_SUBSCRIPT_ROW = 1;
constexpr uint16_t DATAFRAME_OP_SLICE = 2;

QoreValue dataframeValueFromBits(uint64_t bits) {
    QoreValue value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

uint64_t dataframeBitsFromValue(const QoreValue& value) {
    uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

void dataframeIncref(uint64_t value_bits) noexcept {
    dataframeValueFromBits(value_bits).ref();
}

void dataframeDecref(uint64_t value_bits) noexcept {
    QoreValue value = dataframeValueFromBits(value_bits);
    value.discard(nullptr);
}

uint64_t dataframeClone(uint64_t value_bits, ExceptionSink*) {
    dataframeIncref(value_bits);
    return value_bits;
}

bool dataframeEqual(uint64_t lhs_bits, uint64_t rhs_bits, ExceptionSink*) {
    return lhs_bits == rhs_bits;
}

int64_t dataframeHash(uint64_t value_bits, ExceptionSink*) {
    return static_cast<int64_t>(value_bits);
}

void dataframeCleanup(uint64_t value_bits) noexcept {
    dataframeDecref(value_bits);
}

int dataframeSerialize(uint64_t, QorePluginByteWriteCallback, void*, ExceptionSink* xsink) {
    if (xsink) {
        xsink->raiseException("DATAFRAME-SERIALIZATION-ERROR",
            "DataFrame plugin values cannot be serialized directly; serialize records, columns, CSV, or Parquet "
            "data instead");
    }
    return -1;
}

uint64_t dataframeDeserialize(QorePluginByteReadCallback, uint32_t, void*, ExceptionSink* xsink) {
    if (xsink) {
        xsink->raiseException("DATAFRAME-SERIALIZATION-ERROR",
            "DataFrame plugin values cannot be deserialized directly; deserialize records, columns, CSV, or "
            "Parquet data instead");
    }
    return 0;
}

QoreDataFrame* getReferencedDataFrame(QoreValue value, const char* context, ExceptionSink* xsink) {
    if (value.getType() != NT_OBJECT) {
        if (xsink) {
            xsink->raiseException("DATAFRAME-PLUGIN-ERROR",
                "%s requires a DataFrame object, got %s", context, value.getTypeName());
        }
        return nullptr;
    }
    QoreObject* obj = value.get<QoreObject>();
    QoreDataFrame* df = static_cast<QoreDataFrame*>(obj->getReferencedPrivateData(CID_DATAFRAME, xsink));
    if (!df && xsink && !*xsink) {
        xsink->raiseException("DATAFRAME-PLUGIN-ERROR",
            "%s requires a DataFrame object with DataFrame private data", context);
    }
    return df;
}

uint64_t dataframeObjectResult(QoreDataFrame* df, ExceptionSink* xsink) {
    if (!df) {
        return dataframeBitsFromValue(QoreValue());
    }
    ReferenceHolder<QoreObject> obj(new QoreObject(QC_DATAFRAME, nullptr), xsink);
    obj->setPrivate(CID_DATAFRAME, df);
    return dataframeBitsFromValue(QoreValue(obj.release()));
}

QorePluginValueOps dataframeValueOps() {
    QorePluginValueOps ops = {};
    ops.incref = dataframeIncref;
    ops.decref = dataframeDecref;
    ops.clone = dataframeClone;
    ops.equal = dataframeEqual;
    ops.hash = dataframeHash;
    ops.cleanup_slot = dataframeCleanup;
    return ops;
}

QorePluginOpcodeInfoExtended dataframeInfo(bool pure) {
    QorePluginOpcodeInfoExtended info = {};
    info.may_have_side_effects = false;
    info.may_throw_exception = true;
    info.is_pure_modulo_xsink = pure;
    info.type_promotion_kind = QorePluginOpcodeTypePromotion::Exact;
    info.cost_class = 1;
    return info;
}

QorePluginTypeRegistration dataframeRegistration(QorePluginTypeDescriptor& type,
        std::array<QorePluginOperation, 3>& ops) {
    type = {};
    type.local_type_id = DATAFRAME_TYPE_ID;
    type.type_name = "DataFrame";
    type.type_info = QC_DATAFRAME->getTypeInfo();
    type.value_ops = dataframeValueOps();
    type.serialize = dataframeSerialize;
    type.deserialize = dataframeDeserialize;
    type.serializer_format_version = 1;
    type.baseline_qdom_domains = QDOM_DEFAULT;

    ops[DATAFRAME_OP_SUBSCRIPT_COLUMN] = {};
    ops[DATAFRAME_OP_SUBSCRIPT_COLUMN].local_id = DATAFRAME_OP_SUBSCRIPT_COLUMN;
    ops[DATAFRAME_OP_SUBSCRIPT_COLUMN].operation_name = "subscript";
    ops[DATAFRAME_OP_SUBSCRIPT_COLUMN].signature.arity = 2;
    ops[DATAFRAME_OP_SUBSCRIPT_COLUMN].signature.primary_type = QC_DATAFRAME->getTypeInfo();
    ops[DATAFRAME_OP_SUBSCRIPT_COLUMN].signature.secondary_type = stringTypeInfo;
    ops[DATAFRAME_OP_SUBSCRIPT_COLUMN].signature.return_type = autoListTypeInfo;
    ops[DATAFRAME_OP_SUBSCRIPT_COLUMN].signature.access = QorePluginValueAccess::ReadOnly;
    ops[DATAFRAME_OP_SUBSCRIPT_COLUMN].signature.result_alias = QorePluginResultAlias::FreshNoAliasInputs;
    ops[DATAFRAME_OP_SUBSCRIPT_COLUMN].signature.helper_abi = QorePluginHelperAbi::SubscriptValue;
    ops[DATAFRAME_OP_SUBSCRIPT_COLUMN].info = dataframeInfo(true);
    ops[DATAFRAME_OP_SUBSCRIPT_COLUMN].runtime_helper =
        reinterpret_cast<void (*)()>(dataframe_subscript_column);
    ops[DATAFRAME_OP_SUBSCRIPT_COLUMN].runtime_helper_symbol = "dataframe_subscript_column";
    ops[DATAFRAME_OP_SUBSCRIPT_COLUMN].qdom_domains = QDOM_DEFAULT;

    ops[DATAFRAME_OP_SUBSCRIPT_ROW] = {};
    ops[DATAFRAME_OP_SUBSCRIPT_ROW].local_id = DATAFRAME_OP_SUBSCRIPT_ROW;
    ops[DATAFRAME_OP_SUBSCRIPT_ROW].operation_name = "subscript";
    ops[DATAFRAME_OP_SUBSCRIPT_ROW].signature.arity = 2;
    ops[DATAFRAME_OP_SUBSCRIPT_ROW].signature.primary_type = QC_DATAFRAME->getTypeInfo();
    ops[DATAFRAME_OP_SUBSCRIPT_ROW].signature.secondary_type = bigIntTypeInfo;
    ops[DATAFRAME_OP_SUBSCRIPT_ROW].signature.return_type = autoHashTypeInfo;
    ops[DATAFRAME_OP_SUBSCRIPT_ROW].signature.access = QorePluginValueAccess::ReadOnly;
    ops[DATAFRAME_OP_SUBSCRIPT_ROW].signature.result_alias = QorePluginResultAlias::FreshNoAliasInputs;
    ops[DATAFRAME_OP_SUBSCRIPT_ROW].signature.helper_abi = QorePluginHelperAbi::SubscriptValue;
    ops[DATAFRAME_OP_SUBSCRIPT_ROW].info = dataframeInfo(true);
    ops[DATAFRAME_OP_SUBSCRIPT_ROW].runtime_helper =
        reinterpret_cast<void (*)()>(dataframe_subscript_row);
    ops[DATAFRAME_OP_SUBSCRIPT_ROW].runtime_helper_symbol = "dataframe_subscript_row";
    ops[DATAFRAME_OP_SUBSCRIPT_ROW].qdom_domains = QDOM_DEFAULT;

    ops[DATAFRAME_OP_SLICE] = {};
    ops[DATAFRAME_OP_SLICE].local_id = DATAFRAME_OP_SLICE;
    ops[DATAFRAME_OP_SLICE].operation_name = "slice";
    ops[DATAFRAME_OP_SLICE].signature.arity = 0xff;
    ops[DATAFRAME_OP_SLICE].signature.primary_type = QC_DATAFRAME->getTypeInfo();
    ops[DATAFRAME_OP_SLICE].signature.return_type = QC_DATAFRAME->getTypeInfo();
    ops[DATAFRAME_OP_SLICE].signature.access = QorePluginValueAccess::ReadOnly;
    ops[DATAFRAME_OP_SLICE].signature.result_alias = QorePluginResultAlias::FreshNoAliasInputs;
    ops[DATAFRAME_OP_SLICE].signature.helper_abi = QorePluginHelperAbi::CallValueList;
    ops[DATAFRAME_OP_SLICE].info = dataframeInfo(true);
    ops[DATAFRAME_OP_SLICE].runtime_helper = reinterpret_cast<void (*)()>(dataframe_slice);
    ops[DATAFRAME_OP_SLICE].runtime_helper_symbol = "dataframe_slice";
    ops[DATAFRAME_OP_SLICE].qdom_domains = QDOM_DEFAULT;

    QorePluginTypeRegistration reg = {};
    reg.module_name = "dataframe";
    reg.plugin_abi_version = QORE_PLUGIN_ABI_VERSION_V1;
    reg.operation_set_version = "1.0.0";
    reg.types = &type;
    reg.num_types = 1;
    reg.operations = ops.data();
    reg.num_operations = static_cast<int>(ops.size());
    return reg;
}

} // namespace

extern "C" DLLEXPORT uint64_t dataframe_subscript_column(uint64_t container_bits, uint64_t key_bits,
        ExceptionSink* xsink) {
    QoreValue container = dataframeValueFromBits(container_bits);
    QoreValue key = dataframeValueFromBits(key_bits);
    ReferenceHolder<QoreDataFrame> df(getReferencedDataFrame(container, "DataFrame column subscript", xsink), xsink);
    if (*xsink || !df) {
        return dataframeBitsFromValue(QoreValue());
    }
    QoreStringValueHelper name(key);
    QoreListNode* column = df->getColumn(name->c_str(), xsink);
    return dataframeBitsFromValue(*xsink ? QoreValue() : QoreValue(column));
}

extern "C" DLLEXPORT uint64_t dataframe_subscript_row(uint64_t container_bits, uint64_t key_bits,
        ExceptionSink* xsink) {
    QoreValue container = dataframeValueFromBits(container_bits);
    QoreValue key = dataframeValueFromBits(key_bits);
    ReferenceHolder<QoreDataFrame> df(getReferencedDataFrame(container, "DataFrame row subscript", xsink), xsink);
    if (*xsink || !df) {
        return dataframeBitsFromValue(QoreValue());
    }
    QoreHashNode* row = df->getRow(key.getAsBigInt(), xsink);
    return dataframeBitsFromValue(*xsink ? QoreValue() : QoreValue(row));
}

extern "C" DLLEXPORT uint64_t dataframe_slice(uint64_t self_bits, uint64_t args_bits, ExceptionSink* xsink) {
    QoreValue self = dataframeValueFromBits(self_bits);
    QoreValue args_value = dataframeValueFromBits(args_bits);
    ReferenceHolder<QoreDataFrame> df(getReferencedDataFrame(self, "DataFrame range slice", xsink), xsink);
    if (*xsink || !df) {
        return dataframeBitsFromValue(QoreValue());
    }
    const QoreListNode* args = args_value.get<const QoreListNode>();
    if (!args || args->size() != 2) {
        xsink->raiseException("DATAFRAME-PLUGIN-ERROR",
            "DataFrame range slice requires exactly 2 range arguments");
        return dataframeBitsFromValue(QoreValue());
    }
    QoreValue start = args->retrieveEntry(0);
    QoreValue stop = args->retrieveEntry(1);
    return dataframeObjectResult(df->sliceRange(start, stop, xsink), xsink);
}

void registerDataFramePluginTypes(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    QorePluginTypeDescriptor type;
    std::array<QorePluginOperation, 3> ops;
    QorePluginTypeRegistration reg = dataframeRegistration(type, ops);

    QorePluginRegistrationContextV1 plugin_ctx = {};
    plugin_ctx.struct_size = sizeof(plugin_ctx);
    plugin_ctx.module_path = ctx.path.c_str();
    plugin_ctx.module_handle = ctx.plugin_module_handle;
    qore_register_plugin_types_v1(&plugin_ctx, &reg, &xsink);
}
