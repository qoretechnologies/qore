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

    ArrowSchema schema = {};
    ArrowArray array = {};
    if (qore_columnar_result_export_arrow_c_data(*result, &schema, &array, &xsink)) {
        fail("Arrow C Data export failed", xsink);
    }

    if (!schema.release || !array.release || std::strcmp(schema.format, "+s") || schema.n_children != 2
            || array.n_children != 2) {
        fail("exported root Arrow C Data shape is invalid", xsink);
    }
    if (std::strcmp(schema.children[0]->format, "l") || std::strcmp(schema.children[1]->format, "U")) {
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
    if (imported->numRows() != 3 || imported->numColumns() != 2) {
        fail("imported ColumnarResult shape is invalid", xsink);
    }

    ValueHolder imported_id(imported->getColumnValue("id", &xsink), &xsink);
    ValueHolder imported_name(imported->getColumnValue("name", &xsink), &xsink);
    if (xsink || imported_id->getType() != NT_BUFFER || imported_name->getType() != NT_BUFFER) {
        fail("imported columns are not dense buffers", xsink);
    }

    const QoreBufferNode* id_buf = imported_id->get<const QoreBufferNode>();
    const QoreBufferNode* name_buf = imported_name->get<const QoreBufferNode>();
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

    qore_cleanup();
    return EXIT_SUCCESS;
}
