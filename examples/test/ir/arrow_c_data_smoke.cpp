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

    ArrowSchema schema = {};
    ArrowArray array = {};
    if (qore_columnar_result_export_arrow_c_data(*result, &schema, &array, &xsink)) {
        fail("Arrow C Data export failed", xsink);
    }

    if (!schema.release || !array.release || std::strcmp(schema.format, "+s") || schema.n_children != 5
            || array.n_children != 5) {
        fail("exported root Arrow C Data shape is invalid", xsink);
    }
    if (std::strcmp(schema.children[0]->format, "l") || std::strcmp(schema.children[1]->format, "U")
            || std::strcmp(schema.children[2]->format, "d:18,2,128")
            || std::strcmp(schema.children[3]->format, "+s")
            || std::strcmp(schema.children[4]->format, "+l")) {
        fail("exported Arrow child formats are invalid", xsink);
    }

    ReferenceHolder<QoreColumnarResult> imported(
        qore_columnar_result_import_arrow_c_data(&schema, &array, &xsink), &xsink);
    if (xsink || !imported) {
        fail("Arrow C Data import failed", xsink);
    }
    if (schema.release || array.release) {
        fail("Arrow C Data import did not consume input objects", xsink);
    }
    if (imported->numRows() != 3 || imported->numColumns() != 5) {
        fail("imported ColumnarResult shape is invalid", xsink);
    }

    ValueHolder imported_id(imported->getColumnValue("id", &xsink), &xsink);
    ValueHolder imported_name(imported->getColumnValue("name", &xsink), &xsink);
    ValueHolder imported_amount(imported->getColumnValue("amount", &xsink), &xsink);
    ValueHolder imported_customer(imported->getColumnValue("customer", &xsink), &xsink);
    ValueHolder imported_items(imported->getColumnValue("items", &xsink), &xsink);
    if (xsink || imported_id->getType() != NT_BUFFER || imported_name->getType() != NT_BUFFER
            || imported_amount->getType() != NT_BUFFER || imported_customer->getType() != NT_LIST
            || imported_items->getType() != NT_LIST) {
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

    ReferenceHolder<QoreListNode> imported_schema(imported->getSchemaV2(&xsink), &xsink);
    const QoreHashNode* amount_schema_hash = imported_schema->retrieveEntry(2).get<const QoreHashNode>();
    const QoreHashNode* customer_schema_hash = imported_schema->retrieveEntry(3).get<const QoreHashNode>();
    const QoreHashNode* items_schema_hash = imported_schema->retrieveEntry(4).get<const QoreHashNode>();
    QoreStringValueHelper amount_kind(amount_schema_hash->getKeyValue("kind"));
    QoreStringValueHelper customer_kind(customer_schema_hash->getKeyValue("kind"));
    QoreStringValueHelper items_kind(items_schema_hash->getKeyValue("kind"));
    if (xsink || std::strcmp(amount_kind->c_str(), "decimal128")
            || std::strcmp(customer_kind->c_str(), "struct")
            || std::strcmp(items_kind->c_str(), "list")) {
        fail("imported schemaV2 metadata is invalid", xsink);
    }

    qore_cleanup();
    return EXIT_SUCCESS;
}
