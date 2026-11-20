/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
#include "bson_conversion.h"
#include <qore/QoreSandboxManager.h>

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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

// returns the description of the INVALID-ENCODING exception in the sink and clears it, or "(other)" for another
// exception
std::string takeInvalidEncodingDesc(ExceptionSink& xsink) {
    QoreValue err = xsink.getExceptionErr();
    QoreValue desc = xsink.getExceptionDesc();
    std::string rv = err.getType() == NT_STRING && !std::strcmp(err.get<const QoreStringNode>()->c_str(),
        "INVALID-ENCODING") && desc.getType() == NT_STRING ? desc.get<const QoreStringNode>()->c_str() : "(other)";
    xsink.clear();
    return rv;
}

// converts the document to a hash and returns the description of the INVALID-ENCODING exception raised, or an
// empty string if none is raised
std::string invalidEncodingDesc(const bson_t& doc) {
    ExceptionSink xsink;
    ReferenceHolder<QoreHashNode> h(bson_to_qore_hash(&doc, &xsink), &xsink);
    if (!xsink) {
        return std::string();
    }
    require(!h, "a hash was returned with an exception");
    return takeInvalidEncodingDesc(xsink);
}

// returns the bytes of the string
std::string bytes(const QoreString& str) {
    return std::string(str.c_str(), str.size());
}

// a BSON string or key that is not valid UTF-8 raises INVALID-ENCODING where the document is converted (qore#6604);
// libbson does not check the strings of a document it reads
void invalidUtf8() {
    const char invalid[] = "a\xe9" "b";
    // a string value, also in a nested document and in an array
    {
        Document doc;
        bson_append_utf8(&doc.value, "s", -1, invalid, 3);
        require(invalidEncodingDesc(doc.value) == "MongoDB: the BSON string is not valid UTF-8: invalid byte sequence "
            "at byte offset 1", "an invalid BSON string was accepted");
    }
    {
        Document sub;
        bson_append_utf8(&sub.value, "t", -1, "\xed\xa0\x80", 3);
        Document doc;
        bson_append_document(&doc.value, "d", -1, &sub.value);
        require(invalidEncodingDesc(doc.value) == "MongoDB: the BSON string is not valid UTF-8: invalid byte sequence "
            "at byte offset 0", "a surrogate in a nested BSON string was accepted");
    }
    {
        Document arr;
        bson_append_utf8(&arr.value, "0", -1, "ok", 2);
        bson_append_utf8(&arr.value, "1", -1, "\xc0\xaf", 2);
        Document doc;
        bson_append_array(&doc.value, "a", -1, &arr.value);
        require(invalidEncodingDesc(doc.value) == "MongoDB: the BSON string is not valid UTF-8: invalid byte sequence "
            "at byte offset 0", "an overlong form in a BSON array was accepted");
    }
    // a key
    {
        // libbson refuses to append a key that is not valid UTF-8, but a document read is not checked: the bytes of
        // {"a\\xe9b": 1} (an int32 element)
        static const uint8_t data[] = {14, 0, 0, 0, 0x10, 'a', 0xe9, 'b', 0, 1, 0, 0, 0, 0};
        bson_t doc;
        require(bson_init_static(&doc, data, sizeof data), "cannot read the BSON document with an invalid key");
        require(invalidEncodingDesc(doc) == "MongoDB: the BSON key is not valid UTF-8: invalid byte sequence at "
            "byte offset 1", "an invalid BSON key was accepted");
    }
    // code, symbol, code with scope, a regular expression and its options, a DBPointer collection
    {
        Document doc;
        bson_append_code(&doc.value, "c", -1, invalid);
        require(invalidEncodingDesc(doc.value) == "MongoDB: the BSON code is not valid UTF-8: invalid byte sequence at "
            "byte offset 1", "invalid BSON code was accepted");
    }
    {
        Document doc;
        bson_append_symbol(&doc.value, "y", -1, invalid, 3);
        require(invalidEncodingDesc(doc.value) == "MongoDB: the BSON symbol is not valid UTF-8: invalid byte sequence "
            "at byte offset 1", "an invalid BSON symbol was accepted");
    }
    {
        Document scope;
        Document doc;
        bson_append_code_with_scope(&doc.value, "w", -1, invalid, &scope.value);
        require(invalidEncodingDesc(doc.value) == "MongoDB: the BSON code is not valid UTF-8: invalid byte sequence at "
            "byte offset 1", "invalid BSON code with scope was accepted");
    }
    {
        // an invalid string in the scope: the code and the partial scope are released (valgrind checks it)
        Document scope;
        bson_append_utf8(&scope.value, "v", -1, "ok", 2);
        bson_append_utf8(&scope.value, "x", -1, invalid, 3);
        Document doc;
        bson_append_int32(&doc.value, "before", -1, 1);
        bson_append_code_with_scope(&doc.value, "w", -1, "f()", &scope.value);
        bson_append_int32(&doc.value, "after", -1, 2);
        require(invalidEncodingDesc(doc.value) == "MongoDB: the BSON string is not valid UTF-8: invalid byte sequence "
            "at byte offset 1", "an invalid string in the scope of BSON code with scope was accepted");
        // and in a code with scope in an array
        Document arr;
        bson_append_code_with_scope(&arr.value, "0", -1, "f()", &scope.value);
        Document adoc;
        bson_append_array(&adoc.value, "a", -1, &arr.value);
        require(invalidEncodingDesc(adoc.value) == "MongoDB: the BSON string is not valid UTF-8: invalid byte "
            "sequence at byte offset 1", "an invalid string in the scope of BSON code with scope in an array was "
            "accepted");
    }
    {
        // a valid scope is converted
        Document scope;
        bson_append_utf8(&scope.value, "v", -1, "\xc3\xa9", 2);
        Document doc;
        bson_append_code_with_scope(&doc.value, "w", -1, "f()", &scope.value);
        ExceptionSink xsink;
        ReferenceHolder<QoreHashNode> h(bson_to_qore_hash(&doc.value, &xsink), &xsink);
        require(h && !xsink, "valid BSON code with scope was refused");
        const QoreHashNode* w = h->getKeyValue("w").get<const QoreHashNode>();
        require(w, "BSON code with scope is not a hash");
        const QoreHashNode* s = w->getKeyValue("$scope").get<const QoreHashNode>();
        require(s && s->getKeyValue("v").getType() == NT_STRING
            && bytes(*s->getKeyValue("v").get<const QoreStringNode>()) == "\xc3\xa9", "BSON scope changed");
    }
    {
        Document doc;
        bson_append_regex(&doc.value, "r", -1, invalid, "i");
        require(invalidEncodingDesc(doc.value) == "MongoDB: the BSON regular expression is not valid UTF-8: invalid "
            "byte sequence at byte offset 1", "an invalid BSON regular expression was accepted");
    }
    {
        // libbson keeps only the option letters it knows on append: the bytes of {"r": /x/<ff>}
        static const uint8_t data[] = {12, 0, 0, 0, 0x0b, 'r', 0, 'x', 0, 0xff, 0, 0};
        bson_t doc;
        require(bson_init_static(&doc, data, sizeof data), "cannot read the BSON document with invalid options");
        require(invalidEncodingDesc(doc) == "MongoDB: the BSON regular expression options is not valid UTF-8: "
            "invalid byte sequence at byte offset 0", "invalid BSON regular expression options were accepted");
    }
    {
        Document doc;
        bson_oid_t oid;
        bson_oid_init_from_string(&oid, "0123456789abcdef01234567");
        bson_append_dbpointer(&doc.value, "p", -1, invalid, &oid);
        require(invalidEncodingDesc(doc.value) == "MongoDB: the BSON DBPointer collection is not valid UTF-8: invalid "
            "byte sequence at byte offset 1", "an invalid BSON DBPointer collection was accepted");
    }
    // valid text other than ASCII is converted, tagged UTF-8
    {
        Document doc;
        bson_append_utf8(&doc.value, "k\xc3\xa9", -1, "\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80", -1);
        bson_append_regex(&doc.value, "r", -1, "^\xc3\xa9", "i");
        ExceptionSink xsink;
        ReferenceHolder<QoreHashNode> h(bson_to_qore_hash(&doc.value, &xsink), &xsink);
        require(h && !xsink, "valid BSON text was refused");
        QoreValue v = h->getKeyValue("k\xc3\xa9");
        require(v.getType() == NT_STRING && v.get<const QoreStringNode>()->getEncoding() == QCS_UTF8
            && !std::strcmp(v.get<const QoreStringNode>()->c_str(), "\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80"),
            "valid BSON string changed");
        QoreValue r = h->getKeyValue("r");
        require(r.getType() == NT_HASH, "BSON regular expression is not a hash");
        QoreValue re = r.get<const QoreHashNode>()->getKeyValue("$regex");
        require(re.getType() == NT_STRING && re.get<const QoreStringNode>()->getEncoding() == QCS_UTF8
            && !std::strcmp(re.get<const QoreStringNode>()->c_str(), "^\xc3\xa9"),
            "valid BSON regular expression changed or is not tagged UTF-8");
    }
}

// the names and arguments given to and taken from libmongoc are UTF-8 text (qore#6604); the methods of the classes that
// use these helpers need a server, so the helpers are tested here
void driverText() {
    // names from the driver
    {
        char n0[] = "db";
        char n1[] = "caf\xc3\xa9";
        char* names[] = {n0, n1, nullptr};
        ExceptionSink xsink;
        ReferenceHolder<QoreListNode> l(mongo_utf8_string_list(names, "MongoClient::listDatabaseNames()",
            "database name", &xsink), &xsink);
        require(l && !xsink && l->size() == 2, "valid names were refused");
        const QoreStringNode* s = l->retrieveEntry(1).get<const QoreStringNode>();
        require(s && s->getEncoding() == QCS_UTF8 && bytes(*s) == "caf\xc3\xa9", "a name changed or is not UTF-8");
        require(l->retrieveEntry(0).get<const QoreStringNode>()->getEncoding() == QCS_UTF8,
            "an ASCII name is not tagged UTF-8");
    }
    {
        char n0[] = "db";
        char n1[] = "a\xe9";
        char n2[] = "c";
        char* names[] = {n0, n1, n2, nullptr};
        ExceptionSink xsink;
        ReferenceHolder<QoreListNode> l(mongo_utf8_string_list(names, "MongoDatabase::listCollectionNames()",
            "collection name", &xsink), &xsink);
        require(!l, "a list was returned with an invalid name");
        require(takeInvalidEncodingDesc(xsink) == "MongoDatabase::listCollectionNames(): the collection name is not "
            "valid UTF-8: invalid byte sequence at byte offset 1", "an invalid name was accepted");
    }
    {
        char* names[] = {nullptr};
        ExceptionSink xsink;
        ReferenceHolder<QoreListNode> l(mongo_utf8_string_list(names, "w", "name", &xsink), &xsink);
        require(l && !xsink && l->empty(), "an empty list of names changed");
    }
    // arguments to the driver
    {
        ExceptionSink xsink;
        QoreString latin1("caf\xe9", QCS_ISO_8859_1);
        TempEncodingHelper utf8;
        require(!mongo_utf8_arg(latin1, utf8, "MongoClient::getDatabase()", "database name", &xsink) && !xsink
            && utf8->getEncoding() == QCS_UTF8 && bytes(**utf8) == "caf\xc3\xa9",
            "an ISO-8859-1 name was not converted to UTF-8");
        QoreString utf16("c\0\xe9\0", 4, QCS_UTF16LE);
        require(!mongo_utf8_arg(utf16, utf8, "MongoClient::getDatabase()", "database name", &xsink) && !xsink
            && bytes(**utf8) == "c\xc3\xa9", "a UTF-16LE name was not converted to UTF-8");
        QoreString ascii("mydb", QCS_UTF8);
        require(!mongo_utf8_arg(ascii, utf8, "MongoClient::getDatabase()", "database name", &xsink) && !xsink
            && *utf8 == &ascii, "a UTF-8 name was copied");
        QoreString bad("a\xe9", QCS_UTF8);
        require(mongo_utf8_arg(bad, utf8, "MongoDatabase::getCollection()", "collection name", &xsink) == -1,
            "an invalid name was accepted");
        require(takeInvalidEncodingDesc(xsink) == "MongoDatabase::getCollection(): the collection name is not valid "
            "UTF-8: invalid byte sequence at byte offset 1", "an invalid name raised another exception");
        QoreString bad_sjis("a\x82", QEM.findCreate("SHIFT_JIS"));
        require(mongo_utf8_arg(bad_sjis, utf8, "MongoClient::constructor()", "MongoDB URI", &xsink) == -1,
            "an invalid Shift_JIS URI was accepted");
        require(takeInvalidEncodingDesc(xsink).find("MongoClient::constructor(): the MongoDB URI is not valid ") == 0,
            "an invalid Shift_JIS URI raised another exception");
    }
    // keys in the default encoding (UTF-8 here) are given to libbson as they are
    {
        ExceptionSink xsink;
        const char* key = "k\xc3\xa9";
        MongoUtf8Key k(key, &xsink);
        require(k && !xsink && k.c_str() == key, "a UTF-8 key was copied");
    }
}

// the checks run in a process whose default encoding is ISO-8859-1: hash keys are converted between the default
// encoding and the UTF-8 of BSON keys
void defaultEncodingKeys() {
    require(QCS_DEFAULT == QCS_ISO_8859_1, "the default encoding is not ISO-8859-1");
    // a UTF-8 BSON key is converted to the default encoding
    {
        Document sub;
        bson_append_int32(&sub.value, "n\xc3\xa9", -1, 2);
        Document doc;
        bson_append_utf8(&doc.value, "k\xc3\xa9", -1, "v\xc3\xa9", -1);
        bson_append_document(&doc.value, "d", -1, &sub.value);
        ExceptionSink xsink;
        ReferenceHolder<QoreHashNode> h(bson_to_qore_hash(&doc.value, &xsink), &xsink);
        require(h && !xsink, "a document with keys other than ASCII was refused");
        QoreValue v = h->getKeyValue("k\xe9");
        require(v.getType() == NT_STRING && v.get<const QoreStringNode>()->getEncoding() == QCS_UTF8
            && bytes(*v.get<const QoreStringNode>()) == "v\xc3\xa9", "a key was not converted to ISO-8859-1");
        require(!h->existsKey("k\xc3\xa9"), "a UTF-8 key was kept as its bytes");
        const QoreHashNode* d = h->getKeyValue("d").get<const QoreHashNode>();
        require(d && d->getKeyValue("n\xe9").getAsBigInt() == 2, "a nested key was not converted to ISO-8859-1");

        // and back: the key in the default encoding is converted to UTF-8
        bson_t* out = qore_hash_to_bson(*h, &xsink);
        require(out && !xsink, "a hash with keys in ISO-8859-1 was not converted");
        bson_iter_t iter;
        require(bson_iter_init(&iter, out) && bson_iter_next(&iter), "the converted document is empty");
        require(std::string(bson_iter_key(&iter)) == "k\xc3\xa9", "a key was not converted to UTF-8");
        require(bson_iter_next(&iter) && BSON_ITER_HOLDS_DOCUMENT(&iter), "the nested document is missing");
        bson_iter_t child;
        require(bson_iter_recurse(&iter, &child) && bson_iter_next(&child)
            && std::string(bson_iter_key(&child)) == "n\xc3\xa9", "a nested key was not converted to UTF-8");
        size_t offset = 0;
        require(bson_validate(out, BSON_VALIDATE_UTF8, &offset), "the converted document is not valid UTF-8");
        bson_destroy(out);

        MongoUtf8Key k("k\xe9", &xsink);
        require(k && !xsink && std::string(k.c_str()) == "k\xc3\xa9", "MongoUtf8Key did not convert the key");
    }
    // a key that ISO-8859-1 cannot represent cannot be a hash key; the values converted are released
    {
        Document doc;
        bson_append_utf8(&doc.value, "a", -1, "x", 1);
        bson_append_utf8(&doc.value, "\xe2\x82\xac", -1, "v", 1);
        ExceptionSink xsink;
        ReferenceHolder<QoreHashNode> h(bson_to_qore_hash(&doc.value, &xsink), &xsink);
        require(!h && xsink, "a key that the default encoding cannot represent was accepted");
        QoreValue err = xsink.getExceptionErr();
        require(err.getType() == NT_STRING
            && !std::strcmp(err.get<const QoreStringNode>()->c_str(), "ENCODING-CONVERSION-ERROR"),
            "a key that the default encoding cannot represent raised another exception");
        xsink.clear();
    }
}

int runDefaultEncodingKeys() {
    qore_init(QL_MIT, "ISO-8859-1", false, QLO_DISABLE_SIGNAL_HANDLING);
    int status = 0;
    try {
        defaultEncodingKeys();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL (ISO-8859-1 default encoding) after %u checks: %s\n", checks, error.what());
        status = 1;
    }
    qore_cleanup();
    if (!status) {
        std::printf("PASS: %u BSON key checks with an ISO-8859-1 default encoding\n", checks);
    }
    return status;
}

} // namespace

int main() {
    // the library is initialized once in a process, so the checks with another default encoding run in a child
    // process, forked before the library is initialized
    std::fflush(stdout);
    pid_t pid = fork();
    if (pid < 0) {
        std::perror("fork");
        return 1;
    }
    if (!pid) {
        return runDefaultEncodingKeys();
    }
    qore_init(QL_MIT, "UTF-8", false, QLO_DISABLE_SIGNAL_HANDLING);
    int status = 0;
    try {
        integerArrays();
        mixedArrays();
        conversionFailures();
        cancellation();
        programInterruption();
        invalidUtf8();
        driverText();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL after %u checks: %s\n", checks, error.what());
        status = 1;
    }
    qore_cleanup();
    if (!status) {
        std::printf("PASS: %u BSON array checks\n", checks);
    }
    int child_status = 0;
    if (waitpid(pid, &child_status, 0) != pid || !WIFEXITED(child_status) || WEXITSTATUS(child_status)) {
        std::fprintf(stderr, "FAIL: the checks with an ISO-8859-1 default encoding failed\n");
        status = 1;
    }
    return status;
}
