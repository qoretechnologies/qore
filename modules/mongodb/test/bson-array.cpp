/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
#include "bson_conversion.h"
#include <qore/QoreSandboxManager.h>

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

unsigned checks = 0;

void require(bool condition, const char* message) {
    ++checks;
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class Document {
public:
    bson_t value;
    Document() { bson_init(&value); }
    ~Document() { bson_destroy(&value); }
    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;
};

void checkCancel(size_t index, ExceptionSink& xsink) {
    if (index && !(index % 100) && qore_check_cancel(&xsink, "checking BSON array conversion")) {
        throw std::runtime_error("BSON array test cancelled");
    }
}

bson_iter_t arrayIterator(const bson_t& doc, const char* key) {
    size_t offset = 0;
    require(bson_validate(&doc, BSON_VALIDATE_UTF8, &offset), "invalid BSON document");
    bson_iter_t parent;
    require(bson_iter_init_find(&parent, &doc, key), "array field missing");
    require(BSON_ITER_HOLDS_ARRAY(&parent), "field is not an array");
    bson_iter_t array;
    require(bson_iter_recurse(&parent, &array), "cannot traverse array");
    return array;
}

void integerArrays() {
    ExceptionSink xsink;
    for (size_t count : {size_t(0), size_t(1), size_t(10), size_t(101), size_t(1005)}) {
        ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), &xsink);
        for (size_t i = 0; i < count; ++i) {
            checkCancel(i, xsink);
            require(!list->push(QoreValue(int64(i) - 500), &xsink) && !xsink, "cannot construct integer fixture");
        }
        Document doc;
        require(!qore_list_to_bson_array(&doc.value, "items", *list, &xsink) && !xsink,
            "integer array conversion failed");
        bson_iter_t iter = arrayIterator(doc.value, "items");
        size_t i = 0;
        while (bson_iter_next(&iter)) {
            checkCancel(i, xsink);
            require(i < count, "too many array elements");
            require(std::to_string(i) == bson_iter_key(&iter), "array key is not sequential");
            require(BSON_ITER_HOLDS_INT64(&iter), "integer type changed");
            require(bson_iter_int64(&iter) == int64(i) - 500, "integer value changed");
            ++i;
        }
        require(i == count, "array element count changed");
        ReferenceHolder<QoreHashNode> roundtrip(bson_to_qore_hash(&doc.value, &xsink), &xsink);
        require(roundtrip && !xsink, "cannot decode integer array");
        const QoreListNode* decoded = roundtrip->getKeyValue("items").get<const QoreListNode>();
        require(decoded && decoded->size() == count, "round-trip array size changed");
        for (size_t n = 0; n < count; ++n) {
            checkCancel(n, xsink);
            require(decoded->retrieveEntry(n).getType() == NT_INT
                && decoded->retrieveEntry(n).getAsBigInt() == int64(n) - 500, "round-trip integer changed");
        }
    }
}

void mixedArrays() {
    ExceptionSink xsink;
    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), &xsink);
    require(!list->push(QoreValue(), &xsink) && !xsink, "cannot add leading null");
    require(!list->push(QoreValue(true), &xsink) && !xsink, "cannot add boolean");
    require(!list->push(QoreValue(3.25), &xsink) && !xsink, "cannot add double");
    require(!list->push(new QoreStringNode("caf\xc3\xa9", QCS_UTF8), &xsink) && !xsink, "cannot add UTF-8 string");
    ReferenceHolder<QoreListNode> nested(new QoreListNode(autoTypeInfo), &xsink);
    require(!nested->push(QoreValue(int64(42)), &xsink) && !xsink, "cannot add nested integer");
    require(!nested->push(QoreValue(), &xsink) && !xsink, "cannot add nested null");
    require(!list->push(nested.release(), &xsink) && !xsink, "cannot add nested array");
    ReferenceHolder<QoreHashNode> record(new QoreHashNode(autoTypeInfo), &xsink);
    require(!record->setKeyValue("id", QoreValue(int64(73)), &xsink) && !xsink, "cannot populate nested document");
    require(!list->push(record.release(), &xsink) && !xsink, "cannot add nested document");
    require(!list->push(QoreValue(), &xsink) && !xsink, "cannot add trailing null");

    Document doc;
    require(!qore_list_to_bson_array(&doc.value, "items", *list, &xsink) && !xsink,
        "mixed array conversion failed");
    bson_iter_t iter = arrayIterator(doc.value, "items");
    const bson_type_t types[] = {BSON_TYPE_NULL, BSON_TYPE_BOOL, BSON_TYPE_DOUBLE, BSON_TYPE_UTF8,
        BSON_TYPE_ARRAY, BSON_TYPE_DOCUMENT, BSON_TYPE_NULL};
    for (size_t i = 0; i < 7; ++i) {
        require(bson_iter_next(&iter), "mixed array element missing");
        require(std::to_string(i) == bson_iter_key(&iter), "mixed array contains a key gap");
        require(bson_iter_type(&iter) == types[i], "mixed array element type changed");
    }
    require(!bson_iter_next(&iter), "extra mixed array element");
    ReferenceHolder<QoreHashNode> roundtrip(bson_to_qore_hash(&doc.value, &xsink), &xsink);
    require(roundtrip && !xsink, "cannot decode mixed array");
    const QoreListNode* decoded = roundtrip->getKeyValue("items").get<const QoreListNode>();
    require(decoded && decoded->size() == 7, "mixed round-trip array size changed");
    require(decoded->retrieveEntry(0).isNothing() && decoded->retrieveEntry(6).isNothing(), "null values changed");
    require(decoded->retrieveEntry(1).getAsBool(), "boolean value changed");
    require(decoded->retrieveEntry(2).getAsFloat() == 3.25, "double value changed");
    require(!std::strcmp(decoded->retrieveEntry(3).get<const QoreStringNode>()->c_str(), "caf\xc3\xa9"),
        "UTF-8 value changed");
    const QoreListNode* inner = decoded->retrieveEntry(4).get<const QoreListNode>();
    require(inner && inner->size() == 2 && inner->retrieveEntry(0).getAsBigInt() == 42
        && inner->retrieveEntry(1).isNothing(), "nested array changed");
    const QoreHashNode* inner_doc = decoded->retrieveEntry(5).get<const QoreHashNode>();
    require(inner_doc && inner_doc->getKeyValue("id").getAsBigInt() == 73, "nested document changed");
}

void conversionFailures() {
    ExceptionSink xsink;
    for (size_t prefix : {size_t(0), size_t(5), size_t(1005)}) {
        ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), &xsink);
        for (size_t i = 0; i < prefix; ++i) {
            checkCancel(i, xsink);
            require(!list->push(QoreValue(int64(i)), &xsink) && !xsink, "cannot construct failure prefix");
        }
        require(!list->push(new QoreStringNode("a", 1, QCS_UTF16LE), &xsink) && !xsink,
            "cannot construct malformed encoding fixture");
        Document doc;
        int rc = qore_list_to_bson_array(&doc.value, "items", *list, &xsink);
        bool error = static_cast<bool>(xsink);
        xsink.clear();
        require(rc == -1 && error, "invalid string encoding was accepted");
        // Failed documents are discarded by the caller; a fresh conversion
        // must work after both small and buffer-growing partial arrays.
        Document recovered;
        ReferenceHolder<QoreListNode> valid(new QoreListNode(autoTypeInfo), &xsink);
        require(!valid->push(QoreValue(int64(91)), &xsink) && !xsink, "cannot construct recovery fixture");
        require(!qore_list_to_bson_array(&recovered.value, "items", *valid, &xsink) && !xsink,
            "conversion did not recover");
        bson_iter_t iter = arrayIterator(recovered.value, "items");
        require(bson_iter_next(&iter) && bson_iter_int64(&iter) == 91, "recovery value changed");
        require(!bson_iter_next(&iter), "recovery contains stale values");
    }
}

bool takeError(ExceptionSink& xsink, const char* expected) {
    QoreValue error = xsink.getExceptionErr();
    bool matches = error.getType() == NT_STRING
        && !std::strcmp(error.get<const QoreStringNode>()->c_str(), expected);
    xsink.clear();
    return matches;
}

struct CancelReset {
    ~CancelReset() { qore_clear_thread_cancel(); }
};

void cancellation() {
    ExceptionSink xsink;
    for (size_t size : {size_t(0), size_t(1), size_t(99), size_t(100), size_t(101), size_t(1005)}) {
        ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), &xsink);
        for (size_t i = 0; i < size; ++i) {
            checkCancel(i, xsink);
            require(!list->push(QoreValue(int64(i)), &xsink) && !xsink, "cannot construct cancellation fixture");
        }
        CancelReset reset;
        require(!qore_cancel_thread(q_gettid(), "BSON array cancellation"), "cannot request cancellation");
        Document doc;
        int rc = qore_list_to_bson_array(&doc.value, "items", *list, &xsink);
        bool cancelled = takeError(xsink, "THREAD-CANCELLED");
        require(rc == -1 && cancelled, "BSON array ignored cancellation");
        require(!bson_count_keys(&doc.value), "entry cancellation modified the document");
        {
            QoreCancelDeferralHelper defer;
            require(!qore_list_to_bson_array(&doc.value, "items", *list, &xsink) && !xsink,
                "cleanup deferral did not permit conversion");
        }
        bson_iter_t iter = arrayIterator(doc.value, "items");
        size_t count = 0;
        // The request is still pending; keep fixture validation in a cleanup
        // deferral until checking that the converter observes it again.
        {
            QoreCancelDeferralHelper defer;
            while (bson_iter_next(&iter)) {
                checkCancel(count, xsink);
                require(std::to_string(count) == bson_iter_key(&iter)
                    && bson_iter_int64(&iter) == int64(count), "deferred array value changed");
                ++count;
            }
        }
        require(count == size, "deferred array size changed");
        Document after;
        rc = qore_list_to_bson_array(&after.value, "items", *list, &xsink);
        cancelled = takeError(xsink, "THREAD-CANCELLED");
        require(rc == -1 && cancelled, "cleanup deferral lost cancellation");
        qore_clear_thread_cancel();
        require(!qore_list_to_bson_array(&after.value, "items", *list, &xsink) && !xsink,
            "array conversion did not recover after cancellation");
        require(list->size() == size, "cancellation changed the input list");
    }
}

void programInterruption() {
    ExceptionSink xsink;
    QoreProgramHelper program(QoreParseOptions{}, xsink);
    require(!xsink, "cannot create sandbox program");
    ReferenceHolder<QoreSandboxManager> manager(new QoreSandboxManager, &xsink);
    program->setSandboxManager(*manager);
    QoreProgramContextHelper context(*program);
    struct InterruptReset {
        QoreSandboxManager* manager;
        ~InterruptReset() { manager->clearInterrupt(); }
    } reset{*manager};
    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), &xsink);
    require(!list->push(QoreValue(int64(37)), &xsink) && !xsink, "cannot construct sandbox fixture");
    manager->requestInterrupt();
    Document doc;
    int rc = qore_list_to_bson_array(&doc.value, "items", *list, &xsink);
    bool interrupted = takeError(xsink, "PROGRAM-INTERRUPTED");
    require(rc == -1 && interrupted, "BSON array ignored sandbox interruption");
    {
        QoreCancelDeferralHelper defer;
        require(!qore_list_to_bson_array(&doc.value, "items", *list, &xsink) && !xsink,
            "sandbox cleanup deferral did not permit conversion");
    }
    Document after;
    rc = qore_list_to_bson_array(&after.value, "items", *list, &xsink);
    interrupted = takeError(xsink, "PROGRAM-INTERRUPTED");
    require(rc == -1 && interrupted, "cleanup deferral lost sandbox interruption");
    manager->clearInterrupt();
    require(!qore_list_to_bson_array(&after.value, "items", *list, &xsink) && !xsink,
        "conversion did not recover after sandbox interruption");
}

} // namespace

int main() {
    qore_init(QL_MIT, "UTF-8", false, QLO_DISABLE_SIGNAL_HANDLING);
    int status = 0;
    try {
        integerArrays();
        mixedArrays();
        conversionFailures();
        cancellation();
        programInterruption();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL after %u checks: %s\n", checks, error.what());
        status = 1;
    }
    qore_cleanup();
    if (!status) {
        std::printf("PASS: %u BSON array checks\n", checks);
    }
    return status;
}
