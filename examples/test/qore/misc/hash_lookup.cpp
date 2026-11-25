/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <qore/Qore.h>
#include "qore/intern/qore_siphash.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

static unsigned checks = 0;

static void require(bool valid, const char* message) {
    ++checks;
    if (!valid) {
        throw std::runtime_error(message);
    }
}

static void encodedKeys() {
    ExceptionSink xsink;
    ReferenceHolder<QoreHashNode> hash(new QoreHashNode(autoTypeInfo), &xsink);
    for (const char* text : {"", "account", "caf\xc3\xa9", "\xc3\x85ngstr\xc3\xb6m"}) {
        QoreString utf8(text, QCS_UTF8);
        for (const QoreEncoding* encoding : {QCS_UTF8, QCS_ISO_8859_1, QCS_UTF16LE, QCS_UTF16BE}) {
            std::unique_ptr<QoreString> key(utf8.convertEncoding(encoding, &xsink));
            require(!xsink && key, "key encoding fixture failed");
            const QoreString original(*key);
            for (bool nothing : {false, true}) {
                QoreValue value = nothing ? QoreValue() : QoreValue(int64(314159));
                require(!hash->setKeyValue(text, value, &xsink) && !xsink, "cannot populate UTF-8 key");
                for (bool initial : {false, true}) {
                    bool exists = initial;
                    QoreValue result = hash->getKeyValueExistence(*key, exists, &xsink);
                    require(!xsink && exists, "encoded key not found");
                    require(nothing ? result.isNothing() : result.getType() == NT_INT && result.getAsBigInt() == 314159,
                        "encoded existence lookup returned the wrong value");
                    result = hash->getKeyValue(*key, &xsink);
                    require(!xsink && (nothing ? result.isNothing()
                        : result.getType() == NT_INT && result.getAsBigInt() == 314159),
                        "encoded value lookup returned the wrong value");
                    require(key->getEncoding() == original.getEncoding() && key->size() == original.size()
                        && !std::memcmp(key->c_str(), original.c_str(), key->size()), "lookup changed the key");
                }
            }
        }
    }
    // A multibyte key must not accidentally match the ASCII prefix before its first NUL byte.
    require(!hash->setKeyValue("c", QoreValue(int64(271828)), &xsink) && !xsink, "cannot set prefix key");
    QoreString missing("c\0a\0f\0e\0", 8, QCS_UTF16LE);
    for (bool initial : {false, true}) {
        bool exists = initial;
        QoreValue result = hash->getKeyValueExistence(missing, exists, &xsink);
        require(!xsink && !exists && result.isNothing(), "missing encoded key matched a byte prefix");
        result = hash->getKeyValue(missing, &xsink);
        require(!xsink && result.isNothing(), "missing encoded value lookup matched a byte prefix");
    }
}

static void conversionErrors() {
    ExceptionSink xsink;
    ReferenceHolder<QoreHashNode> hash(new QoreHashNode(autoTypeInfo), &xsink);
    struct InvalidKey {
        const char* bytes;
        size_t length;
    };
    for (const auto& row : {InvalidKey{"a", 1}, InvalidKey{"\0\xd8", 2},
            InvalidKey{"\0\xdc", 2}, InvalidKey{"\0\xd8" "a\0", 4}}) {
        QoreString key(row.bytes, row.length, QCS_UTF16LE);
        for (bool initial : {false, true}) {
            bool exists = initial;
            QoreValue result = hash->getKeyValueExistence(key, exists, &xsink);
            bool error = static_cast<bool>(xsink);
            xsink.clear();
            require(error && result.isNothing(), "malformed encoded key was not rejected");
            require(!exists, "conversion failure did not clear existence output");
            result = hash->getKeyValue(key, &xsink);
            error = static_cast<bool>(xsink);
            xsink.clear();
            require(error && result.isNothing(), "value lookup accepted a malformed encoded key");
        }
    }
    QoreString key("recovery", QCS_UTF8);
    require(!hash->setKeyValue(key, QoreValue(int64(73)), &xsink) && !xsink, "cannot populate recovery key");
    bool exists = false;
    QoreValue result = hash->getKeyValueExistence(key, exists, &xsink);
    require(!xsink && exists && result.getAsBigInt() == 73, "lookup did not recover after conversion failure");
}

static void declarationErrors() {
    ExceptionSink xsink;
    TypedHashDeclHolder declaration(new TypedHashDecl("LookupRecord", "LookupRecord"));
    declaration->addMember("present", bigIntTypeInfo, QoreValue(int64(91)));
    ReferenceHolder<QoreHashNode> hash(new QoreHashNode(*declaration, &xsink), &xsink);
    require(!xsink, "hashdecl fixture initialization failed");
    for (bool encoded : {false, true}) {
        for (bool initial : {false, true}) {
            bool exists = initial;
            QoreString key("absent", QCS_ISO_8859_1);
            QoreValue result = encoded ? hash->getKeyValueExistence(key, exists, &xsink)
                : hash->getKeyValueExistence("absent", exists, &xsink);
            bool error = static_cast<bool>(xsink);
            QoreValue code = xsink.getExceptionErr();
            bool invalid_member = code.getType() == NT_STRING
                && !std::strcmp(code.get<const QoreStringNode>()->c_str(), "INVALID-MEMBER");
            xsink.clear();
            require(error && invalid_member && result.isNothing(), "unknown hashdecl member did not raise INVALID-MEMBER");
            require(!exists, "invalid hashdecl member did not clear existence output");
            result = hash->getKeyValueExistence("present", exists, &xsink);
            require(!xsink && exists && result.getAsBigInt() == 91, "valid member lookup did not recover");
        }
    }
}

// The keys that the hash has, in order, joined with ","
static std::string keyList(const QoreHashNode& hash) {
    std::string rv;
    ConstHashIterator i(hash);
    while (i.next()) {
        if (!rv.empty()) {
            rv += ",";
        }
        rv += i.getKey();
    }
    return rv;
}

// A copy of a hash finds every key, and members deleted and removed through an iterator are no longer found
// (qore#7265)
static void copiesAndIteratorDeletes() {
    ExceptionSink xsink;
    ReferenceHolder<QoreHashNode> hash(new QoreHashNode(autoTypeInfo), &xsink);
    for (int64 i = 0; i < 30; ++i) {
        std::string key = "k" + std::to_string(i);
        require(!hash->setKeyValue(key.c_str(), QoreValue(i), &xsink) && !xsink, "cannot populate copy fixture");
    }
    ReferenceHolder<QoreHashNode> copy(hash->copy(), &xsink);
    require(copy->size() == 30 && keyList(**copy) == keyList(**hash), "the copy does not have the keys in order");
    {
        HashIterator i(*copy);
        while (i.next()) {
            int64 n = std::atoll(i.getKey() + 1);
            if (!(n % 2)) {
                i.deleteKey(&xsink);
                require(!xsink, "deleting a member through an iterator raised an exception");
            } else if (!(n % 3)) {
                QoreValue v = i.removeKeyValue();
                require(v.getType() == NT_INT && v.getAsBigInt() == n, "removing a member returned the wrong value");
            }
        }
    }
    require(copy->size() == 10, "the members were not deleted from the copy");
    for (int64 i = 0; i < 30; ++i) {
        std::string key = "k" + std::to_string(i);
        bool exists;
        QoreValue v = copy->getKeyValueExistence(key.c_str(), exists, &xsink);
        bool kept = (i % 2) && (i % 3);
        require(!xsink && exists == kept && (!kept || v.getAsBigInt() == i), "the copy finds the wrong keys");
        v = hash->getKeyValueExistence(key.c_str(), exists, &xsink);
        require(!xsink && exists && v.getAsBigInt() == i, "the original changed");
    }
    require(keyList(**copy) == "k1,k5,k7,k11,k13,k17,k19,k23,k25,k29", "the copy has the wrong keys after deletes");
    // a copy of a hash with deleted keys, which then grows
    ReferenceHolder<QoreHashNode> copy2(copy->copy(), &xsink);
    for (int64 i = 30; i < 60; ++i) {
        std::string key = "k" + std::to_string(i);
        require(!copy2->setKeyValue(key.c_str(), QoreValue(i), &xsink) && !xsink, "cannot add to the copy");
    }
    require(copy2->size() == 40 && keyList(**copy) == "k1,k5,k7,k11,k13,k17,k19,k23,k25,k29",
        "the copy of the copy changed the copy");
    for (int64 i = 0; i < 60; ++i) {
        std::string key = "k" + std::to_string(i);
        bool exists;
        QoreValue v = copy2->getKeyValueExistence(key.c_str(), exists, &xsink);
        bool kept = i >= 30 || ((i % 2) && (i % 3));
        require(!xsink && exists == kept && (!kept || v.getAsBigInt() == i), "the grown copy finds the wrong keys");
    }
}

// HashIterator::deleteKey() deletes a member whose value's release runs code (an object's destructor that changes
// a global variable), and the iteration continues with the next member (qore#7265)
static void iteratorDeleteRunsCode() {
    ExceptionSink xsink;
    QoreProgram* pgm = new QoreProgram();
    QoreString code("%new-style\n"
        "our int destroyed = 0;\n"
        "class X { destructor() { ++destroyed; } }\n"
        "hash<auto> sub make() { return {\"a\": 1, \"b\": new X(), \"c\": 3, \"d\": new X(), \"e\": 5}; }\n"
        "int sub count() { return destroyed; }\n");
    QoreString label("hash-iterator-delete");
    pgm->parse(&code, &label, &xsink);
    require(!xsink, "cannot parse the iterator fixture");
    // the top-level code initializes the global variable
    pgm->run(&xsink).discard(&xsink);
    require(!xsink, "cannot run the iterator fixture");
    {
        ValueHolder v(pgm->callFunction("make", nullptr, &xsink), &xsink);
        require(!xsink && v->getType() == NT_HASH, "the iterator fixture did not give a hash");
        QoreHashNode* h = v->get<QoreHashNode>();
        std::string visited;
        HashIterator i(*h);
        while (i.next()) {
            visited += i.getKey();
            if (i.get().getType() == NT_OBJECT) {
                int64 before = pgm->callFunction("count", nullptr, &xsink).getAsBigInt();
                require(!xsink, "cannot read the iterator fixture's count");
                i.deleteKey(&xsink);
                require(!xsink, "deleting an object member through an iterator raised an exception");
                int64 after = pgm->callFunction("count", nullptr, &xsink).getAsBigInt();
                require(after == before + 1, "the destructor of the deleted member's object did not run");
            }
        }
        require(visited == "abcde", "the iteration did not continue after the members were deleted");
        require(keyList(*h) == "a,c,e", "the hash has the wrong keys after the deletes");
    }
    pgm->waitForTerminationAndDeref(&xsink);
    require(!xsink, "the iterator fixture's program raised an exception");
}

// The keys that the hash has, in reverse order, joined with ","
static std::string reverseKeyList(const QoreHashNode& hash) {
    std::string rv;
    ReverseConstHashIterator i(hash);
    while (i.next()) {
        if (!rv.empty()) {
            rv += ",";
        }
        rv += i.getKey();
    }
    return rv;
}

static void setInt(QoreHashNode& hash, const std::string& key, int64 value, ExceptionSink& xsink) {
    require(!hash.setKeyValue(key.c_str(), QoreValue(value), &xsink) && !xsink, "cannot set a key");
}

// An iterator stays on its member while other members are deleted and added, and iterates in the order of the hash,
// forward and in reverse; a hash that grows while an iterator is on one of its members keeps the iterator valid
// (qore#7272)
static void iteratorStability() {
    ExceptionSink xsink;
    ReferenceHolder<QoreHashNode> hash(new QoreHashNode(autoTypeInfo), &xsink);
    for (int64 i = 0; i < 6; ++i) {
        setInt(**hash, "k" + std::to_string(i), i, xsink);
    }
    require(reverseKeyList(**hash) == "k5,k4,k3,k2,k1,k0", "the reverse iteration has the wrong order");
    require(!std::strcmp(hash->getFirstKey(), "k0") && !std::strcmp(hash->getLastKey(), "k5"),
        "the first or last key is wrong");
    {
        // members before and after the iterator's member are deleted and added while it iterates: the iteration
        // goes on from its member, and visits the members added at the end
        std::string visited;
        HashIterator i(**hash);
        while (i.next()) {
            visited += i.getKey();
            visited += i.first() ? "^" : "";
            visited += i.last() ? "$" : "";
            visited += ";";
            if (!std::strcmp(i.getKey(), "k2")) {
                hash->deleteKey("k0", &xsink);
                hash->deleteKey("k3", &xsink);
                require(!xsink, "deleting keys raised an exception");
                setInt(**hash, "a0", 100, xsink);
                setInt(**hash, "k0", 0, xsink);
                require(!std::strcmp(i.getKey(), "k2") && i.get().getAsBigInt() == 2,
                    "the iterator left its member");
            } else if (!std::strcmp(i.getKey(), "k5")) {
                QoreValue v = hash->takeKeyValue("k4");
                require(v.getAsBigInt() == 4, "the taken value is wrong");
            }
        }
        require(visited == "k0^;k1;k2;k4;k5;a0;k0$;", "the iteration did not follow the changes of the hash");
        require(keyList(**hash) == "k1,k2,k5,a0,k0", "the hash has the wrong keys after the changes");
        require(reverseKeyList(**hash) == "k0,a0,k5,k2,k1", "the reverse iteration has the wrong order");
    }
    {
        // a reverse iterator goes on from its member when the member it would visit next and one it visited are
        // deleted
        std::string visited;
        ReverseHashIterator i(**hash);
        while (i.next()) {
            visited += i.getKey();
            visited += ";";
            if (!std::strcmp(i.getKey(), "a0")) {
                hash->deleteKey("k5", &xsink);
                hash->deleteKey("k0", &xsink);
                require(!xsink, "deleting a key raised an exception");
                require(!std::strcmp(i.getKey(), "a0") && i.first() && i.last() == false,
                    "the reverse iterator left its member");
            }
        }
        require(visited == "k0;a0;k2;k1;", "the reverse iteration did not follow the deletes");
        require(keyList(**hash) == "k1,k2,a0", "the hash has the wrong keys after the reverse deletes");
    }
    {
        // deleting the first and the last member through an iterator; the iterator then starts again from the first
        HashIterator i(**hash);
        require(i.next() && !std::strcmp(i.getKey(), "k1"), "the iteration does not start at the first key");
        i.deleteKey(&xsink);
        require(!xsink && !i.valid(), "deleting the first member left the iterator on a member");
        require(i.next() && !std::strcmp(i.getKey(), "k2"), "the iteration does not go on with the new first key");
        require(i.next() && !std::strcmp(i.getKey(), "a0") && i.last(), "the last key is wrong");
        QoreValue v = i.removeKeyValue();
        require(v.getAsBigInt() == 100 && i.valid() && !std::strcmp(i.getKey(), "k2"),
            "removing the last member did not move the iterator back");
        require(!i.next(), "the iteration goes on after the last member");
        require(keyList(**hash) == "k2" && !std::strcmp(hash->getFirstKey(), "k2")
            && !std::strcmp(hash->getLastKey(), "k2"), "the hash has the wrong keys");
    }
    {
        // the hash grows past many sizes of its key map while an iterator is on its first member and a const
        // iterator is on its last
        HashIterator i(**hash);
        require(i.next(), "the iterator has no member");
        ConstHashIterator ci(**hash);
        require(ci.next() && ci.last(), "the const iterator is not on the last member");
        for (int64 n = 0; n < 5000; ++n) {
            setInt(**hash, "g" + std::to_string(n), n, xsink);
        }
        require(!std::strcmp(i.getKey(), "k2") && !std::strcmp(ci.getKey(), "k2"),
            "the iterators left their member when the hash grew");
        int64 count = 1;
        while (i.next()) {
            require(i.get().getAsBigInt() == count - 1, "the iteration after the growth has the wrong order");
            ++count;
        }
        require(count == 5001 && hash->size() == 5001, "the iteration did not visit every member added");
        require(ci.next() && !std::strcmp(ci.getKey(), "g0"), "the const iterator does not go on in order");
    }
}

// A large hash finds every key while it grows, after most of its keys are deleted, and when it grows again; its keys
// stay in order (qore#7272)
static void largeHash() {
    ExceptionSink xsink;
    ReferenceHolder<QoreHashNode> hash(new QoreHashNode(autoTypeInfo), &xsink);
    const int64 n = 40000;
    for (int64 i = 0; i < n; ++i) {
        setInt(**hash, "k" + std::to_string(i), i, xsink);
        // the keys added before are found at each size where the key map grows
        if (i && !(i & (i - 1))) {
            for (int64 j = 0; j <= i; ++j) {
                bool exists;
                QoreValue v = hash->getKeyValueExistence(("k" + std::to_string(j)).c_str(), exists, &xsink);
                require(!xsink && exists && v.getAsBigInt() == j, "a key is not found after the hash grew");
            }
            bool exists;
            hash->getKeyValueExistence(("k" + std::to_string(i + 1)).c_str(), exists, &xsink);
            require(!exists, "a key that the hash does not have is found");
        }
    }
    require(hash->size() == static_cast<size_t>(n), "the large hash has the wrong size");
    for (int64 i = 0; i < n; ++i) {
        if (i % 5) {
            hash->deleteKey(("k" + std::to_string(i)).c_str(), &xsink);
            require(!xsink, "deleting a key of the large hash raised an exception");
        }
    }
    require(hash->size() == static_cast<size_t>(n / 5), "the large hash has the wrong size after the deletes");
    for (int64 i = 0; i < n; ++i) {
        setInt(**hash, "m" + std::to_string(i), i, xsink);
    }
    {
        ConstHashIterator i(**hash);
        int64 expect = 0;
        while (i.next()) {
            std::string key = expect < n ? "k" + std::to_string(expect) : "m" + std::to_string(expect - n);
            require(key == i.getKey(), "the large hash has the wrong order");
            expect += expect < n ? 5 : 1;
        }
        require(expect == 2 * n, "the iteration of the large hash ended early");
    }
    ReferenceHolder<QoreHashNode> copy(hash->copy(), &xsink);
    for (int64 i = 0; i < n; ++i) {
        bool exists;
        QoreValue v = copy->getKeyValueExistence(("k" + std::to_string(i)).c_str(), exists, &xsink);
        require(!xsink && exists == !(i % 5) && (!exists || v.getAsBigInt() == i), "the copy finds the wrong keys");
        v = copy->getKeyValueExistence(("m" + std::to_string(i)).c_str(), exists, &xsink);
        require(!xsink && exists && v.getAsBigInt() == i, "the copy does not find a key");
    }
    require(keyList(**copy) == keyList(**hash), "the copy of the large hash has another order");
}

//! the SipHash-1-3 outputs for the key 00 01 .. 0f and the inputs of 0 to 63 bytes 00 01 02 ..; the vectors of the
//! SipHash-1-3 test of Rust's standard library (library/coretests/tests/hash/sip.rs, MIT/Apache-2.0)
static const uint64_t siphash13_vectors[64] = {
    0xabac0158050fc4dcull, 0xc9f49bf37d57ca93ull, 0x82cb9b024dc7d44dull,
    0x8bf80ab8e7ddf7fbull, 0xcf75576088d38328ull, 0xdef9d52f49533b67ull,
    0xc50d2b50c59f22a7ull, 0xd3927d989bb11140ull, 0x369095118d299a8eull,
    0x25a48eb36c063de4ull, 0x79de85ee92ff097full, 0x70c118c1f94dc352ull,
    0x78a384b157b4d9a2ull, 0x306f760c1229ffa7ull, 0x605aa111c0f95d34ull,
    0xd320d86d2a519956ull, 0xcc4fdd1a7d908b66ull, 0x9cf2689063dbd80cull,
    0x8ffc389cb473e63eull, 0xf21f9de58d297d1cull, 0xc0dc2f46a6cce040ull,
    0xb992abfe2b45f844ull, 0x7ffe7b9ba320872eull, 0x525a0e7fdae6c123ull,
    0xf464aeb267349c8cull, 0x45cd5928705b0979ull, 0x3a3e35e3ca9913a5ull,
    0xa91dc74e4ade3b35ull, 0xfb0bed02ef6cd00dull, 0x88d93cb44ab1e1f4ull,
    0x540f11d643c5e663ull, 0x2370dd1f8c21d1bcull, 0x81157b6c16a7b60dull,
    0x4d54b9e57a8ff9bfull, 0x759f12781f2a753eull, 0xcea1a3bebf186b91ull,
    0x2cf508d3ada26206ull, 0xb6101c2da3c33057ull, 0xb3f47496ae3a36a1ull,
    0x626b57547b108392ull, 0xc1d2363299e41531ull, 0x667cc1923f1ad944ull,
    0x65704ffec8138825ull, 0x24f280d1c28949a6ull, 0xc2ca1cedfaf8876bull,
    0xc2164bfc9f042196ull, 0xa16e9c9368b1d623ull, 0x49fb169c8b5114fdull,
    0x9f3143f8df074c46ull, 0xc6fdaf2412cc86b3ull, 0x7eaf49d10a52098full,
    0x1cf313559d292f9aull, 0xc44a30dda2f41f12ull, 0x36fae98943a71ed0ull,
    0x318fb34c73f0bce6ull, 0xa27abf3670a7e980ull, 0xb4bcc0db243c6d75ull,
    0x23f8d852fdb71513ull, 0x8f035f4da67d8a08ull, 0xd89cd0e5b7e8f148ull,
    0xf6f4e6bcf7a644eeull, 0xaec59ad80f1837f2ull, 0xc3b2f6154b6694e0ull,
    0x9d199062b7bbb3a8ull,
};

//! known-answer test of the keyed hash of hardened key maps (qore#7557)
static void siphash13() {
    unsigned char input[64];
    for (unsigned i = 0; i < 64; ++i) {
        input[i] = static_cast<unsigned char>(i);
    }
    const uint64_t k0 = 0x0706050403020100ull;
    const uint64_t k1 = 0x0f0e0d0c0b0a0908ull;
    for (size_t len = 0; len < 64; ++len) {
        require(qore_siphash13(k0, k1, input, len) == siphash13_vectors[len], "a SipHash-1-3 vector does not match");
    }
    // the input is read byte by byte: an unaligned input gives the same hash
    unsigned char shifted[65];
    std::memcpy(shifted + 1, input, 64);
    for (size_t len = 0; len < 64; ++len) {
        require(qore_siphash13(k0, k1, shifted + 1, len) == siphash13_vectors[len],
            "SipHash-1-3 of an unaligned input differs");
    }
    // the key matters
    require(qore_siphash13(k0 ^ 1, k1, input, 16) != siphash13_vectors[16], "SipHash-1-3 ignores the first key word");
    require(qore_siphash13(k0, k1 ^ 1, input, 16) != siphash13_vectors[16], "SipHash-1-3 ignores the second key word");
}

int main(int argc, char** argv) {
    if (argc > 2 || (argc == 2 && std::strcmp(argv[1], "encoded")
            && std::strcmp(argv[1], "conversion") && std::strcmp(argv[1], "declaration")
            && std::strcmp(argv[1], "copy") && std::strcmp(argv[1], "iterator")
            && std::strcmp(argv[1], "stability") && std::strcmp(argv[1], "large")
            && std::strcmp(argv[1], "siphash"))) {
        std::fprintf(stderr, "usage: hash-lookup-test [encoded|conversion|declaration|copy|iterator|stability|"
            "large|siphash]\n");
        return 2;
    }
    qore_init(QL_MIT, "UTF-8", false, QLO_DISABLE_SIGNAL_HANDLING);
    int status = 0;
    try {
        if (argc == 1 || !std::strcmp(argv[1], "encoded")) {
            encodedKeys();
        }
        if (argc == 1 || !std::strcmp(argv[1], "conversion")) {
            conversionErrors();
        }
        if (argc == 1 || !std::strcmp(argv[1], "declaration")) {
            declarationErrors();
        }
        if (argc == 1 || !std::strcmp(argv[1], "copy")) {
            copiesAndIteratorDeletes();
        }
        if (argc == 1 || !std::strcmp(argv[1], "iterator")) {
            iteratorDeleteRunsCode();
        }
        if (argc == 1 || !std::strcmp(argv[1], "stability")) {
            iteratorStability();
        }
        if (argc == 1 || !std::strcmp(argv[1], "large")) {
            largeHash();
        }
        if (argc == 1 || !std::strcmp(argv[1], "siphash")) {
            siphash13();
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL after %u checks: %s\n", checks, error.what());
        status = 1;
    }
    qore_cleanup();
    if (!status) {
        std::printf("PASS: %u hash lookup checks\n", checks);
    }
    return status;
}
