/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <qore/Qore.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>

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

int main(int argc, char** argv) {
    if (argc > 2 || (argc == 2 && std::strcmp(argv[1], "encoded")
            && std::strcmp(argv[1], "conversion") && std::strcmp(argv[1], "declaration"))) {
        std::fprintf(stderr, "usage: hash-lookup-test [encoded|conversion|declaration]\n");
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
