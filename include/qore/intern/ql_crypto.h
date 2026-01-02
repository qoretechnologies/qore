/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ql_crypto.h

    libcrypto-based cryptographic functions

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_QL_CRYPTO_H

#define _QORE_QL_CRYPTO_H

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/des.h>
#include <openssl/hmac.h>

#define MD2_ERR "MD2-DIGEST-ERROR"
#define MD4_ERR "MD4-DIGEST-ERROR"
#define MD5_ERR "MD5-DIGEST-ERROR"
#define SHA_ERR "SHA-DIGEST-ERROR"
#define SHA1_ERR "SHA1-DIGEST-ERROR"
static const char SHA224_ERR[] = "SHA224-DIGEST-ERROR";
static const char SHA256_ERR[] = "SHA256-DIGEST-ERROR";
static const char SHA384_ERR[] = "SHA384-DIGEST-ERROR";
static const char SHA512_ERR[] = "SHA512-DIGEST-ERROR";
#define DSS_ERR "DSS-DIGEST-ERROR"
#define DSS1_ERR "DSS1-DIGEST-ERROR"
static const char MDC2_ERR[] = "MDC2-DIGEST-ERROR";
#define RIPEMD160_ERR "RIPEMD160-DIGEST-ERROR"

DLLLOCAL void init_crypto_functions(QoreNamespace& ns);

class BaseHelper {
protected:
    unsigned char* input;
    size_t input_len;

    unsigned char md_value[EVP_MAX_MD_SIZE > HMAC_MAX_MD_CBLOCK ? EVP_MAX_MD_SIZE : HMAC_MAX_MD_CBLOCK];
    unsigned int md_len;

    DLLLOCAL void setInput(const QoreString& str) {
        input = (unsigned char*)str.c_str();
        input_len = str.strlen();
    }

    DLLLOCAL void setInput(const BinaryNode& b) {
        input = (unsigned char*)b.getPtr();
        input_len = b.size();
    }

    DLLLOCAL void setInput(const QoreValue pt) {
        if (pt.getType() == NT_STRING) {
            setInput(*pt.get<const QoreStringNode>());
        } else {
            assert(pt.getType() == NT_BINARY);
            setInput(*pt.get<const BinaryNode>());
        }
    }

public:
    DLLLOCAL unsigned int size() const {
        return md_len;
    }

    DLLLOCAL const void* getBuffer() const {
        return (const void*)md_value;
    }

    DLLLOCAL const void* c_str() const {
        return (const void*)md_value;
    }

    DLLLOCAL void getString(QoreString& str) const {
        for (unsigned i = 0; i < md_len; i++)
        str.sprintf("%02x", md_value[i]);
    }

    DLLLOCAL QoreStringNode* getString() const {
        QoreStringNode* str = new QoreStringNode;
        for (unsigned i = 0; i < md_len; i++)
        str->sprintf("%02x", md_value[i]);

        return str;
    }

    DLLLOCAL BinaryNode* getBinary() const {
        BinaryNode* b = new BinaryNode;
        b->append(md_value, md_len);
        return b;
    }
};

class QoreEvpHelper {
public:
    DLLLOCAL QoreEvpHelper() : mdctx(EVP_MD_CTX_create()) {
    }

    DLLLOCAL ~QoreEvpHelper() {
        if (mdctx) {
            EVP_MD_CTX_destroy(mdctx);
        }
    }

    DLLLOCAL EVP_MD_CTX* operator*() {
        return mdctx;
    }

    DLLLOCAL const EVP_MD_CTX* operator*() const {
        return mdctx;
    }

private:
    EVP_MD_CTX* mdctx;
};

class QoreEvpCipherCtxHelper {
public:
    DLLLOCAL QoreEvpCipherCtxHelper() : ctx(EVP_CIPHER_CTX_new()) {
        if (ctx) {
            EVP_CIPHER_CTX_init(ctx);
        }
    }

    DLLLOCAL ~QoreEvpCipherCtxHelper() {
        if (ctx) {
            EVP_CIPHER_CTX_free(ctx);
        }
    }

    DLLLOCAL EVP_CIPHER_CTX* operator*() {
        return ctx;
    }

    DLLLOCAL const EVP_CIPHER_CTX* operator*() const {
        return ctx;
    }

    DLLLOCAL operator bool() const {
        return ctx ? true : false;
    }

private:
    EVP_CIPHER_CTX* ctx;
};

class DigestHelper : public BaseHelper {
public:
    DLLLOCAL DigestHelper(const QoreValue v) {
        setInput(v);
    }

    DLLLOCAL DigestHelper(const QoreListNode* params) {
        setInput(get_param_value(params, 0));
    }

    DLLLOCAL DigestHelper(const QoreString& str) {
        setInput(str);
    }

    DLLLOCAL DigestHelper(const BinaryNode& b) {
        setInput(b);
    }

    DLLLOCAL DigestHelper(const void* buf, size_t len) {
        input = (unsigned char*)buf;
        input_len = len;
    }

    DLLLOCAL int doDigest(const char* err, const EVP_MD* md, ExceptionSink* xsink = nullptr) {
        //printd(5, "DigestHelper::doDigest() err: %p md: %p xs: %p\n", err, md, xsink);
        QoreEvpHelper mdctx;
        if (!*mdctx) {
            if (xsink) {
                xsink->raiseException(err, "error creating digest object");
            } else {
                printd(0, "DigestHelper::doDigest(): error creating digest object\n");
            }
            return -1;
        }

        if (!EVP_DigestInit_ex(*mdctx, md, nullptr)) {
            if (xsink) {
                xsink->raiseException(err, "error initializing digest");
            } else {
                printd(0, "DigestHelper::doDigest(): error initializing digest (%p, %p)\n", *mdctx, md);
            }
            return -1;
        }
        if (!EVP_DigestUpdate(*mdctx, input, input_len) || !EVP_DigestFinal_ex(*mdctx, md_value, &md_len)) {
            if (xsink) {
                xsink->raiseException(err, "error calculating digest");
            } else {
                printd(0, "DigestHelper::doDigest(): error calculating digest\n");
            }
            return -1;
        }

        return 0;
    }
};

#if !defined(OPENSSL_VERSION_MAJOR) || OPENSSL_VERSION_MAJOR < 3
class QoreHmacHelper {
public:
    DLLLOCAL QoreHmacHelper() {
#if defined(HAVE_OPENSSL_INIT_CRYPTO)
        ctx = HMAC_CTX_new();
#else
        HMAC_CTX_init(&ctx);
#endif
    }

    DLLLOCAL ~QoreHmacHelper() {
#ifdef HAVE_OPENSSL_INIT_CRYPTO
        HMAC_CTX_free(ctx);
#else
        HMAC_CTX_cleanup(&ctx);
#endif
    }

    DLLLOCAL HMAC_CTX* operator*() {
#ifdef HAVE_OPENSSL_INIT_CRYPTO
        return ctx;
#else
        return &ctx;
#endif
    }

    DLLLOCAL const HMAC_CTX* operator*() const {
#ifdef HAVE_OPENSSL_INIT_CRYPTO
        return ctx;
#else
        return &ctx;
#endif
    }

private:
#ifdef HAVE_OPENSSL_INIT_CRYPTO
    typedef HMAC_CTX* q_hmac_t;
#else
    typedef HMAC_CTX q_hmac_t;
#endif

    q_hmac_t ctx;
};
#endif

class HMACHelper : public BaseHelper {
public:
    DLLLOCAL HMACHelper(const QoreValue v) {
        setInput(v);
    }

    DLLLOCAL HMACHelper(const QoreListNode* params) {
        setInput(get_param_value(params, 0));
    }

    DLLLOCAL HMACHelper(const QoreStringNode& str) {
        setInput(str);
    }

    DLLLOCAL HMACHelper(const BinaryNode& b) {
        setInput(b);
    }

    DLLLOCAL HMACHelper(const void* buf, size_t len) {
        input = (unsigned char*)buf;
        input_len = len;
    }

    DLLLOCAL int doHMAC(const char* err, const char* digest, const char* ptr, size_t len, ExceptionSink* xsink);
};

#ifdef OPENSSL_3_PLUS
// refs #4910: cleanup function to free cached digest references
DLLLOCAL void digest_cache_cleanup();
#endif

#endif // _QORE_QL_CRYPTO_H
