// Copyright 2026 Qore Technologies, s.r.o.; SPDX-License-Identifier: MIT
#include <qore/Qore.h>
#include <qore/RuntimeConfig.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

class AbstractDatasource;  // The generated rejecting wrappers never dereference this pointer.

// These are the actual QPP-generated wrappers and the unchanged string helper.
#include "ignored-bindings.inc"

namespace {
unsigned checks = 0;
void require(bool value, const char* message) {
    ++checks;
    if (!value) {
        throw std::runtime_error(message);
    }
}
void expectedError(ExceptionSink& xsink, const char* expected) {
    QoreValue err = xsink.getExceptionErr();
    bool match = err.getType() == NT_STRING
        && !std::strcmp(err.get<const QoreStringNode>()->c_str(), expected);
    xsink.clear();
    require(match, "wrapper raised the wrong exception");
}
void exceptions(RuntimeConfig& cfg, ExceptionSink& xsink) {
    ReferenceHolder<QoreListNode> begin(new QoreListNode(autoTypeInfo), &xsink);
    require(!begin->push(new QoreStringNode("customer"), &xsink) && !xsink, "table argument");
    ReferenceHolder<QoreListNode> columns(new QoreListNode(autoTypeInfo), &xsink);
    require(!columns->push(new QoreStringNode("id"), &xsink) && !xsink, "column argument");
    require(!begin->push(columns.release(), &xsink) && !xsink, "columns argument");
    require(!begin->push(QoreValue(), &xsink) && !xsink, "options argument");
    QoreValue started = AbstractDatasource_bulkLoadBegin_VsC12list_string_C11_hash_auto_(
        nullptr, nullptr, *begin, cfg, &xsink);
    require(!started.getAsBool(), "abstract bulk load unexpectedly started");
    expectedError(xsink, "ABSTRACT-DATASOURCE-ERROR");
    ReferenceHolder<QoreListNode> rows(new QoreListNode(autoTypeInfo), &xsink);
    require(!rows->push(new QoreHashNode(autoTypeInfo), &xsink) && !xsink, "rows argument");
    AbstractDatasource_bulkLoadRows_C10hash_auto_(nullptr, nullptr, *rows, cfg, &xsink);
    expectedError(xsink, "ABSTRACT-DATASOURCE-ERROR");
    for (bool flag : {false, true}) {
        ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
        require(!args->push(QoreValue(flag), &xsink) && !xsink, "boolean argument");
        AbstractDatasource_bulkLoadEnd_Vb(nullptr, nullptr, *args, cfg, &xsink);
        expectedError(xsink, "ABSTRACT-DATASOURCE-ERROR");
        AsyncIoController_constructor_Vb(nullptr, *args, cfg, &xsink);
        expectedError(xsink, "ASYNCIOCONTROLLER-CONSTRUCTOR-ERROR");
    }
}
void base64(RuntimeConfig& cfg, ExceptionSink& xsink) {
    struct Sample { std::string bytes; const char* encoded; };
    const Sample samples[] = {{"", ""}, {"f", "Zg"}, {"fo", "Zm8"}, {"foo", "Zm9v"},
        {"foob", "Zm9vYg"}, {"fooba", "Zm9vYmE"}, {"foobar", "Zm9vYmFy"},
        {std::string("\0\xff\xef", 3), "AP_v"}, {"caf\xc3\xa9", "Y2Fmw6k"}};
    for (const auto& sample : samples) {
        ReferenceHolder<QoreStringNode> str(new QoreStringNode(sample.bytes.data(), sample.bytes.size(), QCS_UTF8), &xsink);
        SimpleRefHolder<BinaryNode> bin(new BinaryNode);
        bin->append(sample.bytes.data(), sample.bytes.size());
        for (int64 limit : {int64(-1), int64(0), int64(1), int64(4), int64(64), int64(2147483647)}) {
            ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
            require(!args->push(QoreValue(limit), &xsink) && !xsink, "line-length argument");
            ReferenceHolder<QoreStringNode> s(PseudoString_toBase64Url_vi(nullptr, QoreValue(*str), *args, cfg, &xsink)
                .get<QoreStringNode>(), &xsink);
            require(s && !xsink && !std::strcmp(s->c_str(), sample.encoded), "string Base64 URL changed");
            ReferenceHolder<QoreStringNode> b(PseudoBinary_toBase64Url_vi(nullptr, QoreValue(*bin), *args, cfg, &xsink)
                .get<QoreStringNode>(), &xsink);
            require(b && !xsink && !std::strcmp(b->c_str(), sample.encoded), "binary Base64 URL changed");
        }
    }
}
}
int main() {
    qore_init(QL_MIT, "UTF-8", false, QLO_DISABLE_SIGNAL_HANDLING);
    int status = 0;
    try {
        RuntimeConfig cfg;
        ExceptionSink xsink;
        exceptions(cfg, xsink);
        base64(cfg, xsink);
        require(!xsink, "unexpected final exception");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL after %u checks: %s\n", checks, e.what());
        status = 1;
    }
    qore_cleanup();
    if (!status) {
        std::printf("PASS: %u generated-wrapper checks\n", checks);
    }
    return status;
}
