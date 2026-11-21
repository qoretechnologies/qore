/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
// BSON strings are UTF-8: the strings converted from BSON are tagged UTF-8 and their keys converted to the default
// encoding, and the strings and keys converted to BSON are converted to UTF-8, with a default encoding that is not
// UTF-8 (issue #6615)
#include "bson_conversion.h"

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

// owns a document that a conversion returns
class DocumentHolder {
public:
    explicit DocumentHolder(bson_t* doc) : doc(doc) {}
    ~DocumentHolder() {
        if (doc) {
            bson_destroy(doc);
        }
    }
    DocumentHolder(const DocumentHolder&) = delete;
    DocumentHolder& operator=(const DocumentHolder&) = delete;
    bson_t* get() const { return doc; }

private:
    bson_t* doc;
};

// "café" in UTF-8 and in ISO-8859-1
constexpr const char* utf8_text = "caf\xc3\xa9";
constexpr const char* latin1_text = "caf\xe9";

// the string is read in either representation: a value can hold a short string inline, with no QoreStringNode
bool hasBytes(const QoreValue v, const char* bytes, const QoreEncoding* enc) {
    QoreStringDataHelper str(v);
    return str && str.getEncoding() == enc && str == bytes;
}

QoreValue getString(const QoreHashNode* h, const char* key) {
    return h->getKeyValue(key);
}

// strings and keys read from BSON
void fromBson() {
    ExceptionSink xsink;
    Document doc;
    // a key, a string, a regular expression and a DBPointer with non-ASCII characters
    require(bson_append_utf8(&doc.value, utf8_text, -1, utf8_text, -1), "cannot append a string");
    require(bson_append_utf8(&doc.value, "string", -1, utf8_text, -1), "cannot append a string");
    require(bson_append_regex(&doc.value, "regex", -1, utf8_text, "i"), "cannot append a regular expression");
    bson_oid_t oid;
    bson_oid_init_from_string(&oid, "0123456789abcdef01234567");
    require(bson_append_dbpointer(&doc.value, "dbpointer", -1, utf8_text, &oid), "cannot append a DBPointer");
    require(bson_append_code(&doc.value, "code", -1, utf8_text), "cannot append code");
    require(bson_append_symbol(&doc.value, "symbol", -1, utf8_text, -1), "cannot append a symbol");
    Document scope;
    require(bson_append_utf8(&scope.value, utf8_text, -1, utf8_text, -1), "cannot append a scope string");
    require(bson_append_code_with_scope(&doc.value, "codewscope", -1, utf8_text, &scope.value),
        "cannot append code with scope");

    ReferenceHolder<QoreHashNode> h(bson_to_qore_hash(&doc.value, &xsink), &xsink);
    require(h && !xsink, "cannot convert the document");

    // the key is in the default encoding
    require(QCS_DEFAULT != QCS_UTF8, "the default encoding is UTF-8");
    require(h->existsKey(latin1_text), "the key is not converted to the default encoding");
    require(!h->existsKey(utf8_text), "the key has the bytes of UTF-8");
    require(hasBytes(getString(*h, latin1_text), utf8_text, QCS_UTF8), "the value of a non-ASCII key");
    require(hasBytes(getString(*h, "string"), utf8_text, QCS_UTF8), "a string");
    require(hasBytes(getString(*h, "code"), utf8_text, QCS_UTF8), "code");
    require(hasBytes(getString(*h, "symbol"), utf8_text, QCS_UTF8), "a symbol");

    const QoreHashNode* regex = h->getKeyValue("regex").get<const QoreHashNode>();
    require(regex, "a regular expression is not a hash");
    require(hasBytes(getString(regex, "$regex"), utf8_text, QCS_UTF8), "a regular expression's pattern");
    require(hasBytes(getString(regex, "$options"), "i", QCS_UTF8), "a regular expression's options");

    const QoreHashNode* dbpointer = h->getKeyValue("dbpointer").get<const QoreHashNode>();
    require(dbpointer, "a DBPointer is not a hash");
    require(hasBytes(getString(dbpointer, "$ref"), utf8_text, QCS_UTF8), "a DBPointer's collection");

    const QoreHashNode* codewscope = h->getKeyValue("codewscope").get<const QoreHashNode>();
    require(codewscope, "code with scope is not a hash");
    require(hasBytes(getString(codewscope, "$code"), utf8_text, QCS_UTF8), "the code of code with scope");
    const QoreHashNode* scope_hash = codewscope->getKeyValue("$scope").get<const QoreHashNode>();
    require(scope_hash, "the scope of code with scope is not a hash");
    require(hasBytes(getString(scope_hash, latin1_text), utf8_text, QCS_UTF8), "a key in the scope");
}

// strings and keys written to BSON
void toBson() {
    ExceptionSink xsink;
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), &xsink);
    // the key is in the default encoding; the strings are in ISO-8859-1 and in UTF-8
    require(!h->setKeyValue(latin1_text, new QoreStringNode(latin1_text, QCS_ISO_8859_1), &xsink) && !xsink,
        "cannot set a key");
    require(!h->setKeyValue("utf8", new QoreStringNode(utf8_text, QCS_UTF8), &xsink) && !xsink, "cannot set a key");
    ReferenceHolder<QoreHashNode> sub(new QoreHashNode(autoTypeInfo), &xsink);
    require(!sub->setKeyValue(latin1_text, new QoreStringNode(latin1_text, QCS_ISO_8859_1), &xsink) && !xsink,
        "cannot set a key");
    require(!h->setKeyValue("sub", sub.release(), &xsink) && !xsink, "cannot set a key");

    DocumentHolder doc(qore_hash_to_bson(*h, &xsink));
    require(doc.get() && !xsink, "cannot convert the hash");

    size_t offset = 0;
    require(bson_validate(doc.get(), BSON_VALIDATE_UTF8, &offset), "the document is not valid UTF-8");

    bson_iter_t iter;
    require(bson_iter_init_find(&iter, doc.get(), utf8_text), "the key is not converted to UTF-8");
    require(BSON_ITER_HOLDS_UTF8(&iter), "the value of a non-ASCII key is not a string");
    uint32_t len = 0;
    const char* str = bson_iter_utf8(&iter, &len);
    require(len == std::strlen(utf8_text) && !std::memcmp(str, utf8_text, len),
        "an ISO-8859-1 string is not converted to UTF-8");
    require(bson_iter_init_find(&iter, doc.get(), "utf8") && BSON_ITER_HOLDS_UTF8(&iter),
        "the UTF-8 string is missing");
    str = bson_iter_utf8(&iter, &len);
    require(len == std::strlen(utf8_text) && !std::memcmp(str, utf8_text, len), "a UTF-8 string is changed");

    bson_iter_t child;
    require(bson_iter_init_find(&iter, doc.get(), "sub") && BSON_ITER_HOLDS_DOCUMENT(&iter)
        && bson_iter_recurse(&iter, &child), "the subdocument is missing");
    require(bson_iter_find(&child, utf8_text), "a key in a subdocument is not converted to UTF-8");

    // the round trip gives the same keys and the UTF-8 strings
    ReferenceHolder<QoreHashNode> rt(bson_to_qore_hash(doc.get(), &xsink), &xsink);
    require(rt && !xsink, "cannot convert the document back");
    require(hasBytes(getString(*rt, latin1_text), utf8_text, QCS_UTF8), "the round trip of a non-ASCII key");
    const QoreHashNode* rt_sub = rt->getKeyValue("sub").get<const QoreHashNode>();
    require(rt_sub && hasBytes(getString(rt_sub, latin1_text), utf8_text, QCS_UTF8),
        "the round trip of a key in a subdocument");
}

// a key that the default encoding cannot hold raises an error
void unconvertibleKey() {
    ExceptionSink xsink;
    Document doc;
    // U+20AC (euro sign) is not in ISO-8859-1
    require(bson_append_utf8(&doc.value, "\xe2\x82\xac", -1, "x", -1), "cannot append a string");
    ReferenceHolder<QoreHashNode> h(bson_to_qore_hash(&doc.value, &xsink), &xsink);
    require(!h && xsink, "a key that the default encoding cannot hold was accepted");
    xsink.clear();
}

} // namespace

int main() {
    // a default encoding that is not UTF-8
    qore_init(QL_MIT, "ISO-8859-1", false, QLO_DISABLE_SIGNAL_HANDLING);
    int status = 0;
    try {
        fromBson();
        toBson();
        unconvertibleKey();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL after %u checks: %s\n", checks, error.what());
        status = 1;
    }
    qore_cleanup();
    if (!status) {
        std::printf("PASS: %u BSON encoding checks\n", checks);
    }
    return status;
}
