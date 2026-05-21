/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    df_plugin.cpp

    DataFrame plugin-type registration and dispatch hooks

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "df_plugin.h"

#include "QC_DataFrame.h"
#include "QC_DataFrameExpr.h"

#include <qore/QorePluginType.h>

#include <array>
#include <cstring>
#include <vector>

using namespace QoreDataFrameNS;

extern "C" DLLEXPORT uint64_t dataframe_subscript_column(uint64_t container_bits, uint64_t key_bits,
    ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t dataframe_subscript_row(uint64_t container_bits, uint64_t key_bits,
    ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t dataframe_subscript_mask(uint64_t container_bits, uint64_t key_bits,
    ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t dataframe_slice(uint64_t self_bits, uint64_t args_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t dataframe_column_eq(uint64_t lhs_bits, uint64_t rhs_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t dataframe_column_ne(uint64_t lhs_bits, uint64_t rhs_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t dataframe_column_lt(uint64_t lhs_bits, uint64_t rhs_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t dataframe_column_le(uint64_t lhs_bits, uint64_t rhs_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t dataframe_column_gt(uint64_t lhs_bits, uint64_t rhs_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t dataframe_column_ge(uint64_t lhs_bits, uint64_t rhs_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t dataframe_row_mask_and(uint64_t lhs_bits, uint64_t rhs_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t dataframe_row_mask_or(uint64_t lhs_bits, uint64_t rhs_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t dataframe_row_mask_xor(uint64_t lhs_bits, uint64_t rhs_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t dataframe_row_mask_not(uint64_t value_bits, ExceptionSink* xsink);

namespace {

constexpr uint16_t DATAFRAME_TYPE_ID = 0;
constexpr uint16_t DATAFRAME_COLUMN_REF_TYPE_ID = 1;
constexpr uint16_t DATAFRAME_ROW_MASK_TYPE_ID = 2;
constexpr uint16_t DATAFRAME_OP_SUBSCRIPT_COLUMN = 0;
constexpr uint16_t DATAFRAME_OP_SUBSCRIPT_ROW = 1;
constexpr uint16_t DATAFRAME_OP_SLICE = 2;
constexpr uint16_t DATAFRAME_OP_SUBSCRIPT_MASK = 3;
constexpr uint16_t DATAFRAME_OP_ROW_MASK_AND = 4;
constexpr uint16_t DATAFRAME_OP_ROW_MASK_OR = 5;
constexpr uint16_t DATAFRAME_OP_ROW_MASK_XOR = 6;
constexpr uint16_t DATAFRAME_OP_ROW_MASK_NOT = 7;

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

QoreDataFrameColumnRef* getReferencedColumnRef(QoreValue value, const char* context, ExceptionSink* xsink) {
    if (value.getType() != NT_OBJECT) {
        if (xsink) {
            xsink->raiseException("DATAFRAME-PLUGIN-ERROR",
                "%s requires a DataFrame ColumnRef object, got %s", context, value.getTypeName());
        }
        return nullptr;
    }
    QoreObject* obj = value.get<QoreObject>();
    QoreDataFrameColumnRef* col = static_cast<QoreDataFrameColumnRef*>(
        obj->getReferencedPrivateData(CID_COLUMNREF, xsink));
    if (!col && xsink && !*xsink) {
        xsink->raiseException("DATAFRAME-PLUGIN-ERROR",
            "%s requires a DataFrame ColumnRef object with ColumnRef private data", context);
    }
    return col;
}

QoreDataFrameRowMask* getReferencedRowMask(QoreValue value, const char* context, ExceptionSink* xsink) {
    if (value.getType() != NT_OBJECT) {
        if (xsink) {
            xsink->raiseException("DATAFRAME-PLUGIN-ERROR",
                "%s requires a DataFrame RowMask object, got %s", context, value.getTypeName());
        }
        return nullptr;
    }
    QoreObject* obj = value.get<QoreObject>();
    QoreDataFrameRowMask* mask = static_cast<QoreDataFrameRowMask*>(
        obj->getReferencedPrivateData(CID_ROWMASK, xsink));
    if (!mask && xsink && !*xsink) {
        xsink->raiseException("DATAFRAME-PLUGIN-ERROR",
            "%s requires a DataFrame RowMask object with RowMask private data", context);
    }
    return mask;
}

uint64_t dataframeObjectResult(QoreClass* qc, qore_classid_t cid, AbstractPrivateData* priv, ExceptionSink* xsink) {
    if (!priv) {
        return dataframeBitsFromValue(QoreValue());
    }
    ReferenceHolder<QoreObject> obj(new QoreObject(qc, nullptr), xsink);
    obj->setPrivate(cid, priv);
    return dataframeBitsFromValue(QoreValue(obj.release()));
}

uint64_t dataframeObjectResult(QoreDataFrame* df, ExceptionSink* xsink) {
    return dataframeObjectResult(QC_DATAFRAME, CID_DATAFRAME, df, xsink);
}

uint64_t dataframeObjectResult(QoreDataFrameRowMask* mask, ExceptionSink* xsink) {
    return dataframeObjectResult(QC_ROWMASK, CID_ROWMASK, mask, xsink);
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

QorePluginOpcodeInfoExtended dataframeMaskInfo(bool associative, bool idempotent) {
    QorePluginOpcodeInfoExtended info = dataframeInfo(true);
    info.is_commutative = true;
    info.is_associative = associative;
    info.is_idempotent = idempotent;
    return info;
}

void initPluginType(QorePluginTypeDescriptor& type, uint16_t local_type_id, const char* type_name,
        const QoreTypeInfo* type_info) {
    type = {};
    type.local_type_id = local_type_id;
    type.type_name = type_name;
    type.type_info = type_info;
    type.value_ops = dataframeValueOps();
    type.serialize = dataframeSerialize;
    type.deserialize = dataframeDeserialize;
    type.serializer_format_version = 1;
    type.baseline_qdom_domains = QDOM_DEFAULT;
}

void addBinaryOperation(std::vector<QorePluginOperation>& ops, uint16_t local_id, const char* operation_name,
        const QoreTypeInfo* primary_type, const QoreTypeInfo* secondary_type, const QoreTypeInfo* return_type,
        QorePluginHelperAbi helper_abi, void (*runtime_helper)(), const char* runtime_helper_symbol,
        const QorePluginOpcodeInfoExtended* info = nullptr) {
    QorePluginOperation op = {};
    op.local_id = local_id;
    op.operation_name = operation_name;
    op.signature.arity = 2;
    op.signature.primary_type = primary_type;
    op.signature.secondary_type = secondary_type;
    op.signature.return_type = return_type;
    op.signature.access = QorePluginValueAccess::ReadOnly;
    op.signature.result_alias = QorePluginResultAlias::FreshNoAliasInputs;
    op.signature.helper_abi = helper_abi;
    op.info = info ? *info : dataframeInfo(true);
    op.runtime_helper = runtime_helper;
    op.runtime_helper_symbol = runtime_helper_symbol;
    op.qdom_domains = QDOM_DEFAULT;
    ops.push_back(op);
}

void addUnaryOperation(std::vector<QorePluginOperation>& ops, uint16_t local_id, const char* operation_name,
        const QoreTypeInfo* primary_type, const QoreTypeInfo* return_type, void (*runtime_helper)(),
        const char* runtime_helper_symbol, const QorePluginOpcodeInfoExtended* info = nullptr) {
    QorePluginOperation op = {};
    op.local_id = local_id;
    op.operation_name = operation_name;
    op.signature.arity = 1;
    op.signature.primary_type = primary_type;
    op.signature.return_type = return_type;
    op.signature.access = QorePluginValueAccess::ReadOnly;
    op.signature.result_alias = QorePluginResultAlias::FreshNoAliasInputs;
    op.signature.helper_abi = QorePluginHelperAbi::UnaryValue;
    op.info = info ? *info : dataframeInfo(true);
    op.runtime_helper = runtime_helper;
    op.runtime_helper_symbol = runtime_helper_symbol;
    op.qdom_domains = QDOM_DEFAULT;
    ops.push_back(op);
}

void addColumnComparisonOperations(std::vector<QorePluginOperation>& ops, uint16_t& local_id,
        const char* operation_name, void (*runtime_helper)(), const char* runtime_helper_symbol) {
    const QoreTypeInfo* scalar_types[] = {
        bigIntTypeInfo,
        floatTypeInfo,
        numberTypeInfo,
        stringTypeInfo,
        boolTypeInfo,
        dateTypeInfo,
        nullTypeInfo,
        nothingTypeInfo,
    };

    for (const QoreTypeInfo* scalar_type : scalar_types) {
        addBinaryOperation(ops, local_id++, operation_name, QC_COLUMNREF->getTypeInfo(), scalar_type,
            QC_ROWMASK->getTypeInfo(), QorePluginHelperAbi::BinaryValue, runtime_helper,
            runtime_helper_symbol);
    }
}

QorePluginTypeRegistration dataframeRegistration(std::array<QorePluginTypeDescriptor, 3>& types,
        std::vector<QorePluginOperation>& ops) {
    initPluginType(types[DATAFRAME_TYPE_ID], DATAFRAME_TYPE_ID, "DataFrame", QC_DATAFRAME->getTypeInfo());
    initPluginType(types[DATAFRAME_COLUMN_REF_TYPE_ID], DATAFRAME_COLUMN_REF_TYPE_ID, "DataFrame::ColumnRef",
        QC_COLUMNREF->getTypeInfo());
    initPluginType(types[DATAFRAME_ROW_MASK_TYPE_ID], DATAFRAME_ROW_MASK_TYPE_ID, "DataFrame::RowMask",
        QC_ROWMASK->getTypeInfo());

    addBinaryOperation(ops, DATAFRAME_OP_SUBSCRIPT_COLUMN, "subscript", QC_DATAFRAME->getTypeInfo(), stringTypeInfo,
        autoListTypeInfo, QorePluginHelperAbi::SubscriptValue,
        reinterpret_cast<void (*)()>(dataframe_subscript_column), "dataframe_subscript_column");
    addBinaryOperation(ops, DATAFRAME_OP_SUBSCRIPT_ROW, "subscript", QC_DATAFRAME->getTypeInfo(), bigIntTypeInfo,
        autoHashTypeInfo, QorePluginHelperAbi::SubscriptValue,
        reinterpret_cast<void (*)()>(dataframe_subscript_row), "dataframe_subscript_row");

    QorePluginOperation slice = {};
    slice.local_id = DATAFRAME_OP_SLICE;
    slice.operation_name = "slice";
    slice.signature.arity = 0xff;
    slice.signature.primary_type = QC_DATAFRAME->getTypeInfo();
    slice.signature.return_type = QC_DATAFRAME->getTypeInfo();
    slice.signature.access = QorePluginValueAccess::ReadOnly;
    slice.signature.result_alias = QorePluginResultAlias::FreshNoAliasInputs;
    slice.signature.helper_abi = QorePluginHelperAbi::CallValueList;
    slice.info = dataframeInfo(true);
    slice.runtime_helper = reinterpret_cast<void (*)()>(dataframe_slice);
    slice.runtime_helper_symbol = "dataframe_slice";
    slice.qdom_domains = QDOM_DEFAULT;
    ops.push_back(slice);

    addBinaryOperation(ops, DATAFRAME_OP_SUBSCRIPT_MASK, "subscript", QC_DATAFRAME->getTypeInfo(),
        QC_ROWMASK->getTypeInfo(), QC_DATAFRAME->getTypeInfo(), QorePluginHelperAbi::SubscriptValue,
        reinterpret_cast<void (*)()>(dataframe_subscript_mask), "dataframe_subscript_mask");

    QorePluginOpcodeInfoExtended associative_mask = dataframeMaskInfo(true, true);
    QorePluginOpcodeInfoExtended xor_mask = dataframeMaskInfo(true, false);
    addBinaryOperation(ops, DATAFRAME_OP_ROW_MASK_AND, "bit_and", QC_ROWMASK->getTypeInfo(),
        QC_ROWMASK->getTypeInfo(), QC_ROWMASK->getTypeInfo(), QorePluginHelperAbi::BinaryValue,
        reinterpret_cast<void (*)()>(dataframe_row_mask_and), "dataframe_row_mask_and", &associative_mask);
    addBinaryOperation(ops, DATAFRAME_OP_ROW_MASK_OR, "bit_or", QC_ROWMASK->getTypeInfo(),
        QC_ROWMASK->getTypeInfo(), QC_ROWMASK->getTypeInfo(), QorePluginHelperAbi::BinaryValue,
        reinterpret_cast<void (*)()>(dataframe_row_mask_or), "dataframe_row_mask_or", &associative_mask);
    addBinaryOperation(ops, DATAFRAME_OP_ROW_MASK_XOR, "bit_xor", QC_ROWMASK->getTypeInfo(),
        QC_ROWMASK->getTypeInfo(), QC_ROWMASK->getTypeInfo(), QorePluginHelperAbi::BinaryValue,
        reinterpret_cast<void (*)()>(dataframe_row_mask_xor), "dataframe_row_mask_xor", &xor_mask);
    addUnaryOperation(ops, DATAFRAME_OP_ROW_MASK_NOT, "bit_not", QC_ROWMASK->getTypeInfo(),
        QC_ROWMASK->getTypeInfo(), reinterpret_cast<void (*)()>(dataframe_row_mask_not),
        "dataframe_row_mask_not");

    uint16_t local_id = DATAFRAME_OP_ROW_MASK_NOT + 1;
    addColumnComparisonOperations(ops, local_id, "eq", reinterpret_cast<void (*)()>(dataframe_column_eq),
        "dataframe_column_eq");
    addColumnComparisonOperations(ops, local_id, "ne", reinterpret_cast<void (*)()>(dataframe_column_ne),
        "dataframe_column_ne");
    addColumnComparisonOperations(ops, local_id, "lt", reinterpret_cast<void (*)()>(dataframe_column_lt),
        "dataframe_column_lt");
    addColumnComparisonOperations(ops, local_id, "le", reinterpret_cast<void (*)()>(dataframe_column_le),
        "dataframe_column_le");
    addColumnComparisonOperations(ops, local_id, "gt", reinterpret_cast<void (*)()>(dataframe_column_gt),
        "dataframe_column_gt");
    addColumnComparisonOperations(ops, local_id, "ge", reinterpret_cast<void (*)()>(dataframe_column_ge),
        "dataframe_column_ge");

    QorePluginTypeRegistration reg = {};
    reg.module_name = "dataframe";
    reg.plugin_abi_version = QORE_PLUGIN_ABI_VERSION_V1;
    reg.operation_set_version = "1.0.0";
    reg.types = types.data();
    reg.num_types = static_cast<int>(types.size());
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

extern "C" DLLEXPORT uint64_t dataframe_subscript_mask(uint64_t container_bits, uint64_t key_bits,
        ExceptionSink* xsink) {
    QoreValue container = dataframeValueFromBits(container_bits);
    QoreValue key = dataframeValueFromBits(key_bits);
    ReferenceHolder<QoreDataFrame> df(getReferencedDataFrame(container, "DataFrame row-mask subscript", xsink),
        xsink);
    ReferenceHolder<QoreDataFrameRowMask> mask(getReferencedRowMask(key, "DataFrame row-mask subscript", xsink),
        xsink);
    if (*xsink || !df || !mask) {
        return dataframeBitsFromValue(QoreValue());
    }
    return dataframeObjectResult(df->filterMask(mask->getMask(), xsink), xsink);
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

static uint64_t dataframeColumnCompare(uint64_t lhs_bits, uint64_t rhs_bits, const char* op,
        ExceptionSink* xsink) {
    QoreValue lhs = dataframeValueFromBits(lhs_bits);
    QoreValue rhs = dataframeValueFromBits(rhs_bits);
    ReferenceHolder<QoreDataFrameColumnRef> col(getReferencedColumnRef(lhs, "DataFrame column comparison", xsink),
        xsink);
    if (*xsink || !col) {
        return dataframeBitsFromValue(QoreValue());
    }
    return dataframeObjectResult(col->compare(op, rhs, xsink), xsink);
}

static uint64_t dataframeRowMaskBinary(uint64_t lhs_bits, uint64_t rhs_bits,
        QoreDataFrameRowMask* (QoreDataFrameRowMask::*op)(const QoreDataFrameRowMask&, ExceptionSink*) const,
        const char* context, ExceptionSink* xsink) {
    QoreValue lhs = dataframeValueFromBits(lhs_bits);
    QoreValue rhs = dataframeValueFromBits(rhs_bits);
    ReferenceHolder<QoreDataFrameRowMask> lhs_mask(getReferencedRowMask(lhs, context, xsink), xsink);
    ReferenceHolder<QoreDataFrameRowMask> rhs_mask(getReferencedRowMask(rhs, context, xsink), xsink);
    if (*xsink || !lhs_mask || !rhs_mask) {
        return dataframeBitsFromValue(QoreValue());
    }
    QoreDataFrameRowMask* lhs_ptr = *lhs_mask;
    QoreDataFrameRowMask* rhs_ptr = *rhs_mask;
    return dataframeObjectResult((lhs_ptr->*op)(*rhs_ptr, xsink), xsink);
}

extern "C" DLLEXPORT uint64_t dataframe_row_mask_and(uint64_t lhs_bits, uint64_t rhs_bits, ExceptionSink* xsink) {
    return dataframeRowMaskBinary(lhs_bits, rhs_bits, &QoreDataFrameRowMask::intersect,
        "DataFrame RowMask intersection", xsink);
}

extern "C" DLLEXPORT uint64_t dataframe_row_mask_or(uint64_t lhs_bits, uint64_t rhs_bits, ExceptionSink* xsink) {
    return dataframeRowMaskBinary(lhs_bits, rhs_bits, &QoreDataFrameRowMask::unionWith,
        "DataFrame RowMask union", xsink);
}

extern "C" DLLEXPORT uint64_t dataframe_row_mask_xor(uint64_t lhs_bits, uint64_t rhs_bits, ExceptionSink* xsink) {
    return dataframeRowMaskBinary(lhs_bits, rhs_bits, &QoreDataFrameRowMask::symmetricDifference,
        "DataFrame RowMask symmetric difference", xsink);
}

extern "C" DLLEXPORT uint64_t dataframe_row_mask_not(uint64_t value_bits, ExceptionSink* xsink) {
    QoreValue value = dataframeValueFromBits(value_bits);
    ReferenceHolder<QoreDataFrameRowMask> mask(getReferencedRowMask(value, "DataFrame RowMask inversion", xsink),
        xsink);
    if (*xsink || !mask) {
        return dataframeBitsFromValue(QoreValue());
    }
    return dataframeObjectResult(mask->invert(xsink), xsink);
}

extern "C" DLLEXPORT uint64_t dataframe_column_eq(uint64_t lhs_bits, uint64_t rhs_bits, ExceptionSink* xsink) {
    return dataframeColumnCompare(lhs_bits, rhs_bits, "==", xsink);
}

extern "C" DLLEXPORT uint64_t dataframe_column_ne(uint64_t lhs_bits, uint64_t rhs_bits, ExceptionSink* xsink) {
    return dataframeColumnCompare(lhs_bits, rhs_bits, "!=", xsink);
}

extern "C" DLLEXPORT uint64_t dataframe_column_lt(uint64_t lhs_bits, uint64_t rhs_bits, ExceptionSink* xsink) {
    return dataframeColumnCompare(lhs_bits, rhs_bits, "<", xsink);
}

extern "C" DLLEXPORT uint64_t dataframe_column_le(uint64_t lhs_bits, uint64_t rhs_bits, ExceptionSink* xsink) {
    return dataframeColumnCompare(lhs_bits, rhs_bits, "<=", xsink);
}

extern "C" DLLEXPORT uint64_t dataframe_column_gt(uint64_t lhs_bits, uint64_t rhs_bits, ExceptionSink* xsink) {
    return dataframeColumnCompare(lhs_bits, rhs_bits, ">", xsink);
}

extern "C" DLLEXPORT uint64_t dataframe_column_ge(uint64_t lhs_bits, uint64_t rhs_bits, ExceptionSink* xsink) {
    return dataframeColumnCompare(lhs_bits, rhs_bits, ">=", xsink);
}

void registerDataFramePluginTypes(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    std::array<QorePluginTypeDescriptor, 3> types;
    std::vector<QorePluginOperation> ops;
    QorePluginTypeRegistration reg = dataframeRegistration(types, ops);

    QorePluginRegistrationContextV1 plugin_ctx = {};
    plugin_ctx.struct_size = sizeof(plugin_ctx);
    plugin_ctx.module_path = ctx.path.c_str();
    plugin_ctx.module_handle = ctx.plugin_module_handle;
    qore_register_plugin_types_v1(&plugin_ctx, &reg, &xsink);
}
