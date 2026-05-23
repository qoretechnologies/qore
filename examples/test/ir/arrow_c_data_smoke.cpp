/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    arrow_c_data_smoke.cpp

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.
*/

#include <qore/Qore.h>
#include <qore/QoreColumnarResult.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

static void fail(const char* msg, ExceptionSink& xsink) {
    if (xsink) {
        QoreStringValueHelper err(xsink.getExceptionErr());
        QoreStringValueHelper desc(xsink.getExceptionDesc());
        std::fprintf(stderr, "%s: %s: %s\n", msg, err->c_str(), desc->c_str());
    } else {
        std::fprintf(stderr, "%s\n", msg);
    }
    std::exit(1);
}

static void test_arrow_schema_release(ArrowSchema* schema) {
    if (schema) {
        schema->release = nullptr;
    }
}

static void test_arrow_array_release(ArrowArray* array) {
    if (array) {
        array->release = nullptr;
    }
}

static QoreHashNode* make_customer(int64 id, const char* name, ExceptionSink& xsink) {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), &xsink);
    rv->setKeyValue("id", id, &xsink);
    if (xsink) {
        fail("customer id setup failed", xsink);
    }
    rv->setKeyValue("name", name ? QoreValue::makeStringValue(name) : QoreValue(), &xsink);
    if (xsink) {
        fail("customer name setup failed", xsink);
    }
    return rv.release();
}

static QoreListNode* make_items(std::initializer_list<int64> values, ExceptionSink& xsink) {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), &xsink);
    for (int64 value : values) {
        rv->push(value, &xsink);
        if (xsink) {
            fail("items setup failed", xsink);
        }
    }
    return rv.release();
}

static BinaryNode* make_binary(const char* value) {
    SimpleRefHolder<BinaryNode> rv(new BinaryNode);
    rv->append(value, std::strlen(value));
    return rv.release();
}

static QoreHashNode* make_attrs(const char* segment, const char* tier, ExceptionSink& xsink) {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), &xsink);
    rv->setKeyValue("segment", QoreValue::makeStringValue(segment), &xsink);
    if (xsink) {
        fail("attrs segment setup failed", xsink);
    }
    rv->setKeyValue("tier", QoreValue::makeStringValue(tier), &xsink);
    if (xsink) {
        fail("attrs tier setup failed", xsink);
    }
    return rv.release();
}

int main() {
    qore_init(QL_GPL, "UTF-8", true);
    ExceptionSink xsink;

    ReferenceHolder<QoreListNode> ids_list(new QoreListNode(autoTypeInfo), &xsink);
    ids_list->push(QoreValue(static_cast<int64>(10)), &xsink);
    ids_list->push(QoreValue(static_cast<int64>(20)), &xsink);
    ids_list->push(QoreValue(static_cast<int64>(30)), &xsink);
    if (xsink) {
        fail("id list setup failed", xsink);
    }

    ReferenceHolder<QoreListNode> names_list(new QoreListNode(autoTypeInfo), &xsink);
    names_list->push(QoreValue::makeStringValue("Alice"), &xsink);
    names_list->push(QoreValue(), &xsink);
    names_list->push(QoreValue::makeStringValue("Carol"), &xsink);
    if (xsink) {
        fail("name list setup failed", xsink);
    }

    ReferenceHolder<QoreBufferNode> ids(
        new QoreBufferNode(QoreBufferElementType::Int64, false, *ids_list, &xsink), &xsink);
    if (xsink) {
        fail("id buffer setup failed", xsink);
    }
    ReferenceHolder<QoreBufferNode> names(
        new QoreBufferNode(QoreBufferElementType::String, true, *names_list, &xsink), &xsink);
    if (xsink) {
        fail("name buffer setup failed", xsink);
    }

    ReferenceHolder<QoreColumnarResult> result(new QoreColumnarResult, &xsink);
    if (result->addColumn("id", ids.release(), QoreColumnarColumnType::Int, QoreBufferElementType::Int64,
            false, "int8", &xsink)) {
        fail("adding id column failed", xsink);
    }
    if (result->addColumn("name", names.release(), QoreColumnarColumnType::String,
            QoreBufferElementType::String, true, "text", &xsink)) {
        fail("adding name column failed", xsink);
    }

    ReferenceHolder<QoreListNode> amount_list(new QoreListNode(autoTypeInfo), &xsink);
    amount_list->push(new QoreNumberNode("12.34"), &xsink);
    if (xsink) {
        fail("amount list setup failed", xsink);
    }
    amount_list->push(QoreValue(), &xsink);
    if (xsink) {
        fail("amount list setup failed", xsink);
    }
    amount_list->push(new QoreNumberNode("-5.60"), &xsink);
    if (xsink) {
        fail("amount list setup failed", xsink);
    }
    ReferenceHolder<QoreBufferNode> amounts(
        new QoreBufferNode(QoreBufferElementType::Decimal128, true, *amount_list, &xsink, 18, 2), &xsink);
    if (xsink) {
        fail("amount buffer setup failed", xsink);
    }
    QoreColumnarTypeDescriptor amount_schema;
    amount_schema.kind = QoreColumnarTypeKind::Decimal128;
    amount_schema.column_type = QoreColumnarColumnType::Number;
    amount_schema.buffer_type = QoreBufferElementType::Decimal128;
    amount_schema.nullable = true;
    amount_schema.precision = 18;
    amount_schema.scale = 2;
    amount_schema.native_type = "decimal(18,2)";
    if (result->addColumn("amount", amounts.release(), amount_schema, &xsink)) {
        fail("adding amount column failed", xsink);
    }

    ReferenceHolder<QoreListNode> customer_list(new QoreListNode(autoTypeInfo), &xsink);
    customer_list->push(make_customer(1, "Ada", xsink), &xsink);
    if (xsink) {
        fail("customer list setup failed", xsink);
    }
    customer_list->push(make_customer(2, nullptr, xsink), &xsink);
    if (xsink) {
        fail("customer list setup failed", xsink);
    }
    customer_list->push(make_customer(3, "Grace", xsink), &xsink);
    if (xsink) {
        fail("customer list setup failed", xsink);
    }
    QoreColumnarTypeDescriptor customer_schema;
    customer_schema.kind = QoreColumnarTypeKind::Struct;
    customer_schema.column_type = QoreColumnarColumnType::Auto;
    customer_schema.nullable = false;
    customer_schema.children.resize(2);
    customer_schema.children[0].name = "id";
    customer_schema.children[0].kind = QoreColumnarTypeKind::Int;
    customer_schema.children[0].column_type = QoreColumnarColumnType::Int;
    customer_schema.children[0].buffer_type = QoreBufferElementType::Int64;
    customer_schema.children[1].name = "name";
    customer_schema.children[1].kind = QoreColumnarTypeKind::String;
    customer_schema.children[1].column_type = QoreColumnarColumnType::String;
    customer_schema.children[1].buffer_type = QoreBufferElementType::String;
    customer_schema.children[1].nullable = true;
    if (result->addColumn("customer", customer_list.release(), customer_schema, &xsink)) {
        fail("adding customer column failed", xsink);
    }

    ReferenceHolder<QoreListNode> items_list(new QoreListNode(autoTypeInfo), &xsink);
    items_list->push(make_items({1, 2}, xsink), &xsink);
    if (xsink) {
        fail("items list setup failed", xsink);
    }
    items_list->push(make_items({3, 4, 5}, xsink), &xsink);
    if (xsink) {
        fail("items list setup failed", xsink);
    }
    items_list->push(QoreValue(), &xsink);
    if (xsink) {
        fail("items list setup failed", xsink);
    }
    QoreColumnarTypeDescriptor items_schema;
    items_schema.kind = QoreColumnarTypeKind::List;
    items_schema.column_type = QoreColumnarColumnType::Auto;
    items_schema.nullable = true;
    items_schema.children.resize(1);
    items_schema.children[0].name = "item";
    items_schema.children[0].kind = QoreColumnarTypeKind::Int;
    items_schema.children[0].column_type = QoreColumnarColumnType::Int;
    items_schema.children[0].buffer_type = QoreBufferElementType::Int64;
    if (result->addColumn("items", items_list.release(), items_schema, &xsink)) {
        fail("adding items column failed", xsink);
    }

    ReferenceHolder<QoreListNode> pair_list(new QoreListNode(autoTypeInfo), &xsink);
    pair_list->push(make_items({1, 2}, xsink), &xsink);
    pair_list->push(make_items({3, 4}, xsink), &xsink);
    pair_list->push(make_items({5, 6}, xsink), &xsink);
    if (xsink) {
        fail("pair list setup failed", xsink);
    }
    QoreColumnarTypeDescriptor pair_schema;
    pair_schema.kind = QoreColumnarTypeKind::FixedSizeList;
    pair_schema.column_type = QoreColumnarColumnType::Auto;
    pair_schema.fixed_size = 2;
    pair_schema.children.resize(1);
    pair_schema.children[0].name = "item";
    pair_schema.children[0].kind = QoreColumnarTypeKind::Int;
    pair_schema.children[0].column_type = QoreColumnarColumnType::Int;
    pair_schema.children[0].buffer_type = QoreBufferElementType::Int64;
    if (result->addColumn("pair", pair_list.release(), pair_schema, &xsink)) {
        fail("adding pair column failed", xsink);
    }

    ReferenceHolder<QoreListNode> attrs_list(new QoreListNode(autoTypeInfo), &xsink);
    attrs_list->push(make_attrs("enterprise", "gold", xsink), &xsink);
    attrs_list->push(make_attrs("smb", "silver", xsink), &xsink);
    attrs_list->push(QoreValue(), &xsink);
    if (xsink) {
        fail("attrs list setup failed", xsink);
    }
    QoreColumnarTypeDescriptor attrs_schema;
    attrs_schema.kind = QoreColumnarTypeKind::Map;
    attrs_schema.column_type = QoreColumnarColumnType::Auto;
    attrs_schema.nullable = true;
    attrs_schema.children.resize(2);
    attrs_schema.children[0].name = "key";
    attrs_schema.children[0].kind = QoreColumnarTypeKind::String;
    attrs_schema.children[0].column_type = QoreColumnarColumnType::String;
    attrs_schema.children[0].buffer_type = QoreBufferElementType::String;
    attrs_schema.children[1].name = "item";
    attrs_schema.children[1].kind = QoreColumnarTypeKind::String;
    attrs_schema.children[1].column_type = QoreColumnarColumnType::String;
    attrs_schema.children[1].buffer_type = QoreBufferElementType::String;
    attrs_schema.children[1].nullable = true;
    if (result->addColumn("attrs", attrs_list.release(), attrs_schema, &xsink)) {
        fail("adding attrs column failed", xsink);
    }

    ReferenceHolder<QoreListNode> blob_list(new QoreListNode(autoTypeInfo), &xsink);
    blob_list->push(make_binary("alpha"), &xsink);
    blob_list->push(QoreValue(), &xsink);
    blob_list->push(make_binary("gamma"), &xsink);
    if (xsink) {
        fail("blob list setup failed", xsink);
    }
    QoreColumnarTypeDescriptor blob_schema;
    blob_schema.kind = QoreColumnarTypeKind::Binary;
    blob_schema.column_type = QoreColumnarColumnType::Binary;
    blob_schema.nullable = true;
    if (result->addColumn("blob", blob_list.release(), blob_schema, &xsink)) {
        fail("adding blob column failed", xsink);
    }

    ReferenceHolder<QoreListNode> ts_list(new QoreListNode(autoTypeInfo), &xsink);
    ts_list->push(DateTimeNode::makeAbsolute(currentTZ(), 1700000000, 123456), &xsink);
    ts_list->push(QoreValue(), &xsink);
    ts_list->push(DateTimeNode::makeAbsolute(currentTZ(), 1700000002, 42), &xsink);
    if (xsink) {
        fail("timestamp list setup failed", xsink);
    }
    QoreColumnarTypeDescriptor ts_schema;
    ts_schema.kind = QoreColumnarTypeKind::Timestamp;
    ts_schema.column_type = QoreColumnarColumnType::Date;
    ts_schema.nullable = true;
    ts_schema.time_unit = "us";
    if (result->addColumn("ts", ts_list.release(), ts_schema, &xsink)) {
        fail("adding timestamp column failed", xsink);
    }

    ArrowSchema schema = {};
    ArrowArray array = {};
    if (qore_columnar_result_export_arrow_c_data(*result, &schema, &array, &xsink)) {
        fail("Arrow C Data export failed", xsink);
    }

    if (!schema.release || !array.release || std::strcmp(schema.format, "+s") || schema.n_children != 9
            || array.n_children != 9) {
        fail("exported root Arrow C Data shape is invalid", xsink);
    }
    if (std::strcmp(schema.children[0]->format, "l") || std::strcmp(schema.children[1]->format, "u")
            || std::strcmp(schema.children[2]->format, "d:18,2,128")
            || std::strcmp(schema.children[3]->format, "+s")
            || std::strcmp(schema.children[4]->format, "+l")
            || std::strcmp(schema.children[5]->format, "+w:2")
            || std::strcmp(schema.children[6]->format, "+m")
            || std::strcmp(schema.children[7]->format, "z")
            || std::strcmp(schema.children[8]->format, "tsu:")) {
        fail("exported Arrow child formats are invalid", xsink);
    }
    schema.children[2]->format = "d:18,2";

    ReferenceHolder<QoreColumnarResult> imported(
        qore_columnar_result_import_arrow_c_data(&schema, &array, &xsink), &xsink);
    if (xsink || !imported) {
        fail("Arrow C Data import failed", xsink);
    }
    if (schema.release || array.release) {
        fail("Arrow C Data import did not consume input objects", xsink);
    }
    if (imported->numRows() != 3 || imported->numColumns() != 9) {
        fail("imported ColumnarResult shape is invalid", xsink);
    }

    ValueHolder imported_id(imported->getColumnValue("id", &xsink), &xsink);
    ValueHolder imported_name(imported->getColumnValue("name", &xsink), &xsink);
    ValueHolder imported_amount(imported->getColumnValue("amount", &xsink), &xsink);
    ValueHolder imported_customer(imported->getColumnValue("customer", &xsink), &xsink);
    ValueHolder imported_items(imported->getColumnValue("items", &xsink), &xsink);
    ValueHolder imported_pair(imported->getColumnValue("pair", &xsink), &xsink);
    ValueHolder imported_attrs(imported->getColumnValue("attrs", &xsink), &xsink);
    ValueHolder imported_blob(imported->getColumnValue("blob", &xsink), &xsink);
    ValueHolder imported_ts(imported->getColumnValue("ts", &xsink), &xsink);
    if (xsink || imported_id->getType() != NT_BUFFER || imported_name->getType() != NT_BUFFER
            || imported_amount->getType() != NT_BUFFER || imported_customer->getType() != NT_LIST
            || imported_items->getType() != NT_LIST || imported_pair->getType() != NT_LIST
            || imported_attrs->getType() != NT_LIST || imported_blob->getType() != NT_LIST
            || imported_ts->getType() != NT_LIST) {
        fail("imported columns have invalid value types", xsink);
    }

    const QoreBufferNode* id_buf = imported_id->get<const QoreBufferNode>();
    const QoreBufferNode* name_buf = imported_name->get<const QoreBufferNode>();
    const QoreBufferNode* amount_buf = imported_amount->get<const QoreBufferNode>();
    if (!id_buf->hasExternalStorage() || id_buf->getReferencedEntry(1, &xsink).getAsBigInt() != 20) {
        fail("imported fixed-width column was not zero-copy or has bad values", xsink);
    }
    if (!name_buf->isElementNull(1)) {
        fail("imported string null was not preserved", xsink);
    }
    ValueHolder third_name(name_buf->getReferencedEntry(2, &xsink), &xsink);
    QoreStringValueHelper third(*third_name);
    if (std::strcmp(third->c_str(), "Carol")) {
        fail("imported string value was not preserved", xsink);
    }
    if (!amount_buf->hasExternalStorage() || amount_buf->getElementType() != QoreBufferElementType::Decimal128
            || amount_buf->getDecimalPrecision() != 18 || amount_buf->getDecimalScale() != 2
            || !amount_buf->isElementNull(1)) {
        fail("imported decimal128 metadata, storage, or nulls were not preserved", xsink);
    }
    ValueHolder first_amount(amount_buf->getReferencedEntry(0, &xsink), &xsink);
    QoreStringValueHelper amount_str(*first_amount);
    if (std::strcmp(amount_str->c_str(), "12.34")) {
        fail("imported decimal128 value was not preserved", xsink);
    }

    const QoreListNode* customer_values = imported_customer->get<const QoreListNode>();
    QoreValue second_customer_value = customer_values->retrieveEntry(1);
    if (second_customer_value.getType() != NT_HASH) {
        fail("imported struct column has invalid row type", xsink);
    }
    const QoreHashNode* second_customer = second_customer_value.get<const QoreHashNode>();
    if (second_customer->getKeyValue("id").getAsBigInt() != 2
            || !second_customer->getKeyValue("name").isNothing()) {
        fail("imported struct values were not preserved", xsink);
    }

    const QoreListNode* item_values = imported_items->get<const QoreListNode>();
    QoreValue second_items_value = item_values->retrieveEntry(1);
    QoreValue third_items_value = item_values->retrieveEntry(2);
    if (second_items_value.getType() != NT_LIST || !third_items_value.isNothing()) {
        fail("imported list column has invalid row values", xsink);
    }
    const QoreListNode* second_items = second_items_value.get<const QoreListNode>();
    if (second_items->size() != 3 || second_items->retrieveEntry(2).getAsBigInt() != 5) {
        fail("imported list values were not preserved", xsink);
    }

    const QoreListNode* pair_values = imported_pair->get<const QoreListNode>();
    const QoreListNode* second_pair = pair_values->retrieveEntry(1).get<const QoreListNode>();
    if (!second_pair || second_pair->size() != 2 || second_pair->retrieveEntry(1).getAsBigInt() != 4) {
        fail("imported fixed_size_list values were not preserved", xsink);
    }

    const QoreListNode* attrs_values = imported_attrs->get<const QoreListNode>();
    QoreValue second_attrs_value = attrs_values->retrieveEntry(1);
    QoreValue third_attrs_value = attrs_values->retrieveEntry(2);
    if (second_attrs_value.getType() != NT_HASH || !third_attrs_value.isNothing()) {
        fail("imported map column has invalid row values", xsink);
    }
    const QoreHashNode* second_attrs = second_attrs_value.get<const QoreHashNode>();
    QoreStringValueHelper tier(second_attrs->getKeyValue("tier"));
    if (std::strcmp(tier->c_str(), "silver")) {
        fail("imported map values were not preserved", xsink);
    }

    const QoreListNode* blob_values = imported_blob->get<const QoreListNode>();
    QoreValue first_blob_value = blob_values->retrieveEntry(0);
    QoreValue second_blob_value = blob_values->retrieveEntry(1);
    if (first_blob_value.getType() != NT_BINARY || !second_blob_value.isNothing()
            || first_blob_value.get<const BinaryNode>()->size() != 5) {
        fail("imported binary values were not preserved", xsink);
    }

    const QoreListNode* ts_values = imported_ts->get<const QoreListNode>();
    QoreValue first_ts_value = ts_values->retrieveEntry(0);
    QoreValue second_ts_value = ts_values->retrieveEntry(1);
    if (first_ts_value.getType() != NT_DATE || !second_ts_value.isNothing()
            || first_ts_value.get<const DateTimeNode>()->getEpochMicrosecondsUTC() != 1700000000123456LL) {
        fail("imported timestamp values were not preserved", xsink);
    }

    ReferenceHolder<QoreListNode> imported_schema(imported->getSchemaV2(&xsink), &xsink);
    const QoreHashNode* amount_schema_hash = imported_schema->retrieveEntry(2).get<const QoreHashNode>();
    const QoreHashNode* customer_schema_hash = imported_schema->retrieveEntry(3).get<const QoreHashNode>();
    const QoreHashNode* items_schema_hash = imported_schema->retrieveEntry(4).get<const QoreHashNode>();
    const QoreHashNode* pair_schema_hash = imported_schema->retrieveEntry(5).get<const QoreHashNode>();
    const QoreHashNode* attrs_schema_hash = imported_schema->retrieveEntry(6).get<const QoreHashNode>();
    const QoreHashNode* blob_schema_hash = imported_schema->retrieveEntry(7).get<const QoreHashNode>();
    const QoreHashNode* ts_schema_hash = imported_schema->retrieveEntry(8).get<const QoreHashNode>();
    QoreStringValueHelper amount_kind(amount_schema_hash->getKeyValue("kind"));
    QoreStringValueHelper customer_kind(customer_schema_hash->getKeyValue("kind"));
    QoreStringValueHelper items_kind(items_schema_hash->getKeyValue("kind"));
    QoreStringValueHelper pair_kind(pair_schema_hash->getKeyValue("kind"));
    QoreStringValueHelper attrs_kind(attrs_schema_hash->getKeyValue("kind"));
    QoreStringValueHelper blob_kind(blob_schema_hash->getKeyValue("kind"));
    QoreStringValueHelper ts_kind(ts_schema_hash->getKeyValue("kind"));
    if (xsink || std::strcmp(amount_kind->c_str(), "decimal128")
            || std::strcmp(customer_kind->c_str(), "struct")
            || std::strcmp(items_kind->c_str(), "list")
            || std::strcmp(pair_kind->c_str(), "fixed_size_list")
            || std::strcmp(attrs_kind->c_str(), "map")
            || std::strcmp(blob_kind->c_str(), "binary")
            || std::strcmp(ts_kind->c_str(), "timestamp")
            || pair_schema_hash->getKeyValue("fixed_size").getAsBigInt() != 2) {
        fail("imported schemaV2 metadata is invalid", xsink);
    }

    ArrowSchema sliced_schema = {};
    ArrowArray sliced_array = {};
    if (qore_columnar_result_export_arrow_c_data(*result, &sliced_schema, &sliced_array, &xsink)) {
        fail("Arrow C Data slice export failed", xsink);
    }
    sliced_array.offset = 1;
    sliced_array.length = 2;
    ReferenceHolder<QoreColumnarResult> sliced_import(
        qore_columnar_result_import_arrow_c_data(&sliced_schema, &sliced_array, &xsink), &xsink);
    if (xsink || !sliced_import || sliced_import->numRows() != 2 || sliced_import->numColumns() != 9) {
        fail("Arrow C Data top-level offset import failed", xsink);
    }
    ValueHolder sliced_id(sliced_import->getColumnValue("id", &xsink), &xsink);
    const QoreBufferNode* sliced_id_buf = sliced_id->get<const QoreBufferNode>();
    if (!sliced_id_buf || sliced_id_buf->getReferencedEntry(0, &xsink).getAsBigInt() != 20
            || sliced_id_buf->getReferencedEntry(1, &xsink).getAsBigInt() != 30) {
        fail("Arrow C Data top-level offset did not slice fixed-width values", xsink);
    }
    ValueHolder sliced_pair(sliced_import->getColumnValue("pair", &xsink), &xsink);
    const QoreListNode* sliced_pair_values = sliced_pair->get<const QoreListNode>();
    const QoreListNode* first_sliced_pair = sliced_pair_values->retrieveEntry(0).get<const QoreListNode>();
    if (!first_sliced_pair || first_sliced_pair->retrieveEntry(0).getAsBigInt() != 3
            || first_sliced_pair->retrieveEntry(1).getAsBigInt() != 4) {
        fail("Arrow C Data top-level offset did not slice nested values", xsink);
    }

    int32_t dict_offsets[] = {0, 3, 7};
    const char dict_bytes[] = "redblue";
    const void* dict_buffers[] = {nullptr, dict_offsets, dict_bytes};
    ArrowSchema dict_schema = {};
    dict_schema.format = "u";
    dict_schema.name = "dictionary";
    dict_schema.release = test_arrow_schema_release;
    ArrowArray dict_array = {};
    dict_array.length = 2;
    dict_array.n_buffers = 3;
    dict_array.buffers = dict_buffers;
    dict_array.release = test_arrow_array_release;

    int32_t dict_indices[] = {0, 1, 0};
    const void* code_buffers[] = {nullptr, dict_indices};
    ArrowSchema code_schema = {};
    code_schema.format = "i";
    code_schema.name = "code";
    code_schema.dictionary = &dict_schema;
    code_schema.release = test_arrow_schema_release;
    ArrowArray code_array = {};
    code_array.length = 3;
    code_array.n_buffers = 2;
    code_array.buffers = code_buffers;
    code_array.dictionary = &dict_array;
    code_array.release = test_arrow_array_release;

    ArrowSchema* dict_root_children[] = {&code_schema};
    ArrowArray* dict_root_arrays[] = {&code_array};
    ArrowSchema dict_root_schema = {};
    dict_root_schema.format = "+s";
    dict_root_schema.n_children = 1;
    dict_root_schema.children = dict_root_children;
    dict_root_schema.release = test_arrow_schema_release;
    ArrowArray dict_root_array = {};
    dict_root_array.length = 3;
    dict_root_array.n_buffers = 1;
    dict_root_array.n_children = 1;
    dict_root_array.children = dict_root_arrays;
    dict_root_array.release = test_arrow_array_release;

    ReferenceHolder<QoreColumnarResult> dictionary_import(
        qore_columnar_result_import_arrow_c_data(&dict_root_schema, &dict_root_array, &xsink), &xsink);
    if (xsink || !dictionary_import || dictionary_import->numRows() != 3 || dictionary_import->numColumns() != 1) {
        fail("Arrow C Data dictionary import failed", xsink);
    }
    ValueHolder dictionary_code(dictionary_import->getColumnValue("code", &xsink), &xsink);
    const QoreListNode* dictionary_values = dictionary_code->get<const QoreListNode>();
    QoreStringValueHelper dictionary_second(dictionary_values->retrieveEntry(1));
    if (std::strcmp(dictionary_second->c_str(), "blue")) {
        fail("Arrow C Data dictionary values were not decoded", xsink);
    }
    ReferenceHolder<QoreListNode> dictionary_schema_list(dictionary_import->getSchemaV2(&xsink), &xsink);
    const QoreHashNode* dictionary_schema_hash = dictionary_schema_list->retrieveEntry(0).get<const QoreHashNode>();
    QoreStringValueHelper dictionary_kind(dictionary_schema_hash->getKeyValue("kind"));
    QoreStringValueHelper dictionary_index(dictionary_schema_hash->getKeyValue("dictionary_index_type"));
    if (std::strcmp(dictionary_kind->c_str(), "dictionary") || std::strcmp(dictionary_index->c_str(), "i")) {
        fail("Arrow C Data dictionary schema metadata was not preserved", xsink);
    }

    qore_cleanup();
    return EXIT_SUCCESS;
}
