/* indent-tabs-mode: nil -*- */
/*
    EncryptionTransforms.cpp

    Qore Programming Language

    Copyright (C) 2016 - 2024 Qore Technologies, s.r.o.

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

#include "qore/Qore.h"
#include "qore/intern/EncryptionTransforms.h"
#include "qore/intern/ql_crypto.h"
#include "qore/intern/QoreHashNodeIntern.h"

#include <bzlib.h>
#include <cerrno>
#include <map>
#include <string>
#include <zlib.h>

#include <openssl/evp.h>
#include <openssl/des.h>
#include <openssl/hmac.h>

// no need for the crypto or digest maps with openssl 3+
#ifdef OPENSSL_3_PLUS
#include <string>

typedef std::map<std::string, std::string> strmap_t;

// thread-safe cache for fetched ciphers to avoid memory leaks
// refs #4910: EVP_CIPHER_fetch() returns references that must be freed
typedef std::map<std::string, EVP_CIPHER*, ltstrcase> cipher_cache_t;
static cipher_cache_t cipher_cache;
static QoreThreadLock cipher_cache_lock;

// map old cipher names -> new cipher names for backwards compatibility
static strmap_t cipher_compat_aliases = {
    {"blowfish-cfb", "bf-cfb"},
    {"blowfish-ofb", "bf-ofb"},
    {"desede", "des-ede-cbc"},
    {"desede-cfb", "des-ede-cfb"},
    {"desede-ofb", "des-ede-ofb"},
    {"desede3", "des3"},
    {"desede3-cfb", "des-ede3-cfb"},
    {"desede3-ofb", "des-ede3-ofb"},
    {"cast5", "cast5-cbc"},
    {"aes128", "id-aes128-gcm"},
    {"aes192", "id-aes192-gcm"},
    {"aes256", "id-aes256-gcm"},
};
#else
crypto_map_t crypto_map = {
    {"blowfish", {0, EVP_bf_cbc(), 8, false, false, false, false, false}},
    {"blowfish-cfb", {0, EVP_bf_cfb(), 8, false, false, false, false, false}},
    {"blowfish-ofb", {0, EVP_bf_ofb(), 8, false, false, false, false, false}},
    {"des", {8, EVP_des_cbc(), 8, false, false, false, false, false}},
    {"des-cfb", {8, EVP_des_cfb(), 8, false, false, false, false, false}},
    {"des-ofb", {8, EVP_des_ofb(), 8, false, false, false, false, false}},
    {"desede", {16, EVP_des_ede_cbc(), 8, false, false, false, false, false}},
    {"desede-cfb", {16, EVP_des_ede_cfb(), 8, false, false, false, false, false}},
    {"desede-ofb", {16, EVP_des_ede_ofb(), 8, false, false, false, false, false}},
    {"desede3", {24, EVP_des_ede3_cbc(), 8, false, false, false, false, false}},
    {"desede3-cfb", {24, EVP_des_ede3_cfb(), 8, false, false, false, false, false}},
    {"desede3-ofb", {24, EVP_des_ede3_ofb(), 8, false, false, false, false, false}},
    {"desx", {24, EVP_desx_cbc(), 8, false, false, false, false, false}},
    {"rc2", {0, EVP_rc2_cbc(), 8, false, false, false, false, false}},
    {"rc2-cfb", {0, EVP_rc2_cfb(), 8, false, false, false, false, false}},
    {"rc2-ofb", {0, EVP_rc2_ofb(), 8, false, false, false, false, false}},
    {"rc4", {0, EVP_rc4(), -1, false, false, false, false, false}},
#ifndef OPENSSL_NO_RC5
    {"rc5", {0, EVP_rc5_32_12_16_cbc(), 8, false, false, false, false, false}},
    {"rc5-cfb", {0, EVP_rc5_32_12_16_cfb(), 8, false, false, false, false, false}},
    {"rc5-ofb", {0, EVP_rc5_32_12_16_ofb(), 8, false, false, false, false, false}},
#endif
    {"cast5", {0, EVP_cast5_cbc(), 8, false, false, false, false, false}},
    {"cast5-cfb", {0, EVP_cast5_cfb(), 8, false, false, false, false, false}},
    {"cast5-ofb", {0, EVP_cast5_ofb(), 8, false, false, false, false, false}},

    // AES ciphers
    {"aes128", {16, EVP_aes_128_gcm(), 0, true, false, false, false, false}},
    {"aes192", {24, EVP_aes_192_gcm(), 0, true, false, false, false, false}},
    {"aes256", {32, EVP_aes_256_gcm(), 0, true, false, false, false, false}},
};

digest_map_t digest_map = {
#if !defined(OPENSSL_NO_MD2) && !defined(NO_MD2)
    {"md2", EVP_md2()},
#endif
    {"md4", EVP_md4()},
    {"md5", EVP_md5()},
#ifdef HAVE_OPENSSL_SHA
    {"sha", EVP_sha()},
#endif
    {"sha1", EVP_sha1()},
#if !defined(OPENSSL_NO_SHA256) && defined(HAVE_OPENSSL_SHA512)
    {"sha224", EVP_sha224()},
    {"sha256", EVP_sha256()},
    {"sha384", EVP_sha384()},
    {"sha512", EVP_sha512()},
#endif
#ifndef HAVE_OPENSSL_INIT_CRYPTO
    {"dss", EVP_dss()},
    {"dss1", EVP_dss1()},
#endif
#ifdef OPENSSL_HAVE_MDC2
    {"mdc2", EVP_mdc2()},
#endif
    {"ripemd160", EVP_ripemd160()}
};

QoreHashNode* CryptoEntry::getInfo() const {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), nullptr);

    rv->setKeyValue("key_len", key_len, nullptr);
    rv->setKeyValue("iv_len", iv_len, nullptr);
    rv->setKeyValue("type", new QoreStringNode(gcm ? "GCM" : "CBC"), nullptr);

    return rv.release();
}
#endif

class CryptoTransform : public Transform {
public:
    DLLLOCAL CryptoTransform(const char* cipher, bool do_crypt, const char* key, unsigned key_len, const char* iv,
            unsigned iv_len, const char* mac, unsigned mac_len, unsigned tag_length, const ReferenceNode* mac_ref,
            const char* aad, unsigned aad_len, ExceptionSink* xsink) : cipher(cipher),
            err(do_crypt ? "ENCRYPT-ERROR" : "DECRYPT-ERROR"), tag_length(tag_length),
            mac_ref(mac_ref ? mac_ref->refRefSelf() : nullptr), do_crypt(do_crypt) {
        // if encrypting, we can't send a mac
        assert(!(do_crypt && (mac || mac_len)));
        // if decrypting, then we can't send tag or AAD
        assert(!(!do_crypt && (tag_length || mac_ref)));

        if (findCipher(cipher, do_crypt, xsink)) {
            assert(*xsink);
            return;
        }

        if (iv && ce.iv_len > 0 && (int)iv_len < ce.iv_len) {
            xsink->raiseException(err, "cannot create a %scryption transformation object for algorithm '%s' with an "
                "initialization vector of size %d; %d bytes are required", do_crypt ? "en" : "de", cipher, iv_len,
                ce.iv_len);
            return;
        }

        if (key_len < ce.key_len) {
            xsink->raiseException(err, "%scryption algorithm '%s' requires a key length of %d bytes; got %d byte%s",
                do_crypt ? "en" : "de", cipher, ce.key_len, key_len, key_len == 1 ? "" : "s");
            return;
        }

        state = STATE_OK;

        EVP_CipherInit_ex(*ctx, ce.cipher_type, nullptr, nullptr, nullptr, do_crypt);

        if (iv && ce.iv_len == -1) {
        //if (iv && ce.gcm) {
            //printd(5, "CryptoTransform::CryptoTransform() %s: setting iv_len: %d\n", cipher, iv_len);
            assert(iv_len);
            if (!EVP_CIPHER_CTX_ctrl(*ctx, EVP_CTRL_GCM_SET_IVLEN, iv_len, nullptr)) {
                xsink->raiseException(err, "error setting %s initialization vector length: %d", cipher, iv_len);
                state = STATE_ERROR;
                return;
            }
        }

        if (key_len) {
            if (key_len > EVP_MAX_KEY_LENGTH) {
                key_len = EVP_MAX_KEY_LENGTH;
            }

            //printd(5, "CryptoTransform::CryptoTransform() %scrypt %s: ce.key_len: %d key_len: %d\n",
            //    do_crypt ? "en" : "de", cipher, ce.key_len, key_len);
            if (!EVP_CIPHER_CTX_set_key_length(*ctx, key_len) || !EVP_CipherInit_ex(*ctx, nullptr, nullptr,
                (const unsigned char*)key, (const unsigned char*)iv, -1)) {
                bool ok = false;
                if (ce.key_len && ce.key_len != key_len) {
                    key_len = ce.key_len;
                    if (EVP_CIPHER_CTX_set_key_length(*ctx, key_len) && EVP_CipherInit_ex(*ctx, nullptr, nullptr,
                        (const unsigned char*)key, (const unsigned char*)iv, -1)) {
                        ok = true;
                    }
                }
                if (!ok) {
                    // should not happen
                    xsink->raiseException(err, "error setting %s key length: %d", cipher, key_len);
                    state = STATE_ERROR;
                    return;
                }
            }
        }

        // set AAD if applicable
        if (ce.gcm) {
            if (aad) {
                int outlen;
                if (!EVP_CipherUpdate(*ctx, nullptr, &outlen, (unsigned char*)aad, aad_len)) {
                    xsink->raiseException(err, "error setting %s AAD length: %d", cipher, aad_len);
                    state = STATE_ERROR;
                    return;
                }
            }

            // set the MAC
            if (!do_crypt && mac) {
                if (!EVP_CIPHER_CTX_ctrl(*ctx, EVP_CTRL_GCM_SET_TAG, mac_len, (void*)mac)) {
                    xsink->raiseException(err, "error setting %s tag (MAC) length: %d", cipher, mac_len);
                    state = STATE_ERROR;
                    return;
                }
            }
        }
    }

    DLLLOCAL ~CryptoTransform() {
        if (mac_ref) {
            ExceptionSink xsink;
            mac_ref->deref(&xsink);
            xsink.clear();
        }
    }

    DLLLOCAL std::pair<int64, int64> apply(const void* src, int64 srcLen, void* dst, int64 dstLen,
            ExceptionSink* xsink) {
        if (state == STATE_DONE)
            return std::make_pair(0, 0);

        if (state != STATE_OK) {
            xsink->raiseException(err, "invalid encryption stream state");
            return std::make_pair(0, 0);
        }

        if (!src) {
            // if the input is done
            int tmplen;
            if (!EVP_CipherFinal_ex(*ctx, (unsigned char*)dst, &tmplen)) {
                xsink->raiseException(err, "error %scrypting final %s block", do_crypt ? "en" : "de", cipher);
                state = STATE_ERROR;
                return std::make_pair(0, 0);
            }
            //printd(5, "src: %p srlen: %d dst: %p dstLen: %d tmplen: %d\n", src, srcLen, dst, dstLen, tmplen);

            // get mac and write to MAC reference, if any
            if (ce.gcm && do_crypt && mac_ref && tag_length) {
                SimpleRefHolder<BinaryNode> mac(new BinaryNode);
                mac->preallocate(tag_length);
                if (!EVP_CIPHER_CTX_ctrl(*ctx, EVP_CTRL_GCM_GET_TAG, tag_length, const_cast<void*>(mac->getPtr()))) {
                    xsink->raiseException(err, "error getting %s tag (MAC) length: %d", cipher, tag_length);
                    state = STATE_ERROR;
                    return std::make_pair(0, 0);
                }

                // write to reference
                QoreTypeSafeReferenceHelper rh(mac_ref, xsink);
                if (rh) {
                    rh.assign(mac.release());
                }
                if (*xsink) {
                    state = STATE_ERROR;
                    return std::make_pair(0, 0);
                }
            }

            state = STATE_DONE;
            return std::make_pair(0, tmplen);
        }
        assert(srcLen);

        // issue #3111: make sure the output buffer is at least as big as the input size
        assert(srcLen <= dstLen);

        // issue #3111 outlen is output-only in EVP_CipherUpdate()
        int outlen;
        if (!EVP_CipherUpdate(*ctx, (unsigned char*)dst, &outlen, (unsigned char*)src, srcLen)) {
            xsink->raiseException(err, "error %scrypting %s block", do_crypt ? "en" : "de", cipher);
            state = STATE_ERROR;
            return std::make_pair(0, 0);
        }
        //printd(5, "CryptoTransform::apply() %s: OK\n", cipher);

        // issue #3111: make sure we did not overwrite our meemory buffer
        assert(outlen <= dstLen);

        //printd(5, "CryptoTransform::apply() %scrypt %s: src: %p srcLen: %lld dst: %p dstLen: %lld outlen: %d "
        //    "*xsink: %d\n", do_crypt ? "en" : "de", cipher, src, srcLen, dst, dstLen, outlen, (bool)*xsink);
        return std::make_pair(srcLen, outlen);
    }

    DLLLOCAL virtual size_t outputBufferSize() {
        return BUFSIZE + (EVP_MAX_BLOCK_LENGTH * 2);
    }

    DLLLOCAL virtual size_t inputBufferSize() {
        return BUFSIZE;
    }

private:
    DLLLOCAL static constexpr size_t BUFSIZE = 4096;

    const char* cipher;
    CryptoEntry ce;
    QoreEvpCipherCtxHelper ctx;
    const char* err;
    enum State {
        STATE_OK, STATE_DONE, STATE_ERROR, STATE_NOT_INIT
    };

    State state = STATE_NOT_INIT;

    // number of bytes in MAC requested
    unsigned tag_length;
    // reference to store the MAC
    ReferenceNode* mac_ref;

    bool do_crypt;

    DLLLOCAL int findCipher(const char* cipher, bool do_crypt, ExceptionSink* xsink) {
#ifdef OPENSSL_3_PLUS
        ce.cipher_type = q_lookup_cipher(cipher);
        if (!ce.cipher_type) {
            xsink->raiseException(err, "unknown %scryption algorithm '%s'", do_crypt ? "en" : "de", cipher);
            return -1;
        }

        ce.gcm = EVP_CIPHER_get_mode(ce.cipher_type) == EVP_CIPH_GCM_MODE;
        unsigned long flags = EVP_CIPHER_get_flags(ce.cipher_type);
        if (flags & EVP_CIPH_CUSTOM_IV) {
            ce.iv_len = -1;
        } else {
            int len = EVP_CIPHER_get_iv_length(ce.cipher_type);
            ce.iv_len = len ? len : -1;
        }
        ce.aead = flags & EVP_CIPH_FLAG_AEAD_CIPHER ? true : false;
        ce.cts = flags & EVP_CIPH_FLAG_CTS ? true : false;
        ce.tls_multi = flags & EVP_CIPH_FLAG_TLS1_1_MULTIBLOCK ? true : false;
        ce.randkey = flags & EVP_CIPH_RAND_KEY ? true : false;

        ce.key_len = EVP_CIPHER_get_key_length(ce.cipher_type);
        return 0;
#else
        crypto_map_t::iterator i = crypto_map.find(cipher);
        if (i == crypto_map.end()) {
            xsink->raiseException(err, "unknown %scryption algorithm '%s'", do_crypt ? "en" : "de", cipher);
            return -1;
        }
        ce = i->second;
        return 0;
#endif
    }
};

Transform* EncryptionTransforms::getCryptoTransform(const char* cipher, bool do_crypt, const char* key,
    unsigned key_len, const char* iv, unsigned iv_len, const char* mac, unsigned mac_len, unsigned tag_length,
    const ReferenceNode* mac_ref, const char* aad, unsigned aad_len, ExceptionSink* xsink) {
    ReferenceHolder<Transform> rv(new CryptoTransform(cipher, do_crypt, key, key_len, iv, iv_len, mac, mac_len,
        tag_length, mac_ref, aad, aad_len, xsink), xsink);
    return *xsink ? nullptr : rv.release();
}

#ifdef OPENSSL_3_PLUS
#include <openssl/provider.h>
#include <openssl/kdf.h>
#include <openssl/encoder.h>
#include <openssl/decoder.h>
#include <openssl/safestack.h>

/* Checks to see if algorithms are fetchable */
#define IS_FETCHABLE(type, TYPE)                                \
    static int is_ ## type ## _fetchable(const TYPE *alg)       \
    {                                                           \
        TYPE *impl;                                             \
        const char *name = TYPE ## _get0_name(alg);             \
                                                                \
        ERR_set_mark();                                         \
        impl = TYPE ## _fetch(nullptr, name, nullptr);          \
        ERR_pop_to_mark();                                      \
        if (impl == NULL)                                       \
            return 0;                                           \
        TYPE ## _free(impl);                                    \
        return 1;                                               \
    }
IS_FETCHABLE(cipher, EVP_CIPHER)
IS_FETCHABLE(digest, EVP_MD)
/* currently unused
IS_FETCHABLE(mac, EVP_MAC)
IS_FETCHABLE(kdf, EVP_KDF)
IS_FETCHABLE(rand, EVP_RAND)
IS_FETCHABLE(keymgmt, EVP_KEYMGMT)
IS_FETCHABLE(signature, EVP_SIGNATURE)
IS_FETCHABLE(kem, EVP_KEM)
IS_FETCHABLE(asym_cipher, EVP_ASYM_CIPHER)
IS_FETCHABLE(keyexch, EVP_KEYEXCH)
IS_FETCHABLE(decoder, OSSL_DECODER)
IS_FETCHABLE(encoder, OSSL_ENCODER)
*/

DEFINE_STACK_OF(EVP_CIPHER)
DEFINE_STACK_OF(EVP_MD)

static int cipher_cmp(const EVP_CIPHER* const* a, const EVP_CIPHER* const* b) {
    return strcmp(OSSL_PROVIDER_get0_name(EVP_CIPHER_get0_provider(*a)),
        OSSL_PROVIDER_get0_name(EVP_CIPHER_get0_provider(*b)));
}

static void collect_ciphers(EVP_CIPHER* cipher, void* stack) {
    STACK_OF(EVP_CIPHER)* cipher_stack = (STACK_OF(EVP_CIPHER)*)stack;

    if (is_cipher_fetchable(cipher) && sk_EVP_CIPHER_push(cipher_stack, cipher) > 0) {
        EVP_CIPHER_up_ref(cipher);
    }
}

static int md_cmp(const EVP_MD* const* a, const EVP_MD* const* b) {
    return strcmp(OSSL_PROVIDER_get0_name(EVP_MD_get0_provider(*a)),
                  OSSL_PROVIDER_get0_name(EVP_MD_get0_provider(*b)));
}

static void collect_digests(EVP_MD* digest, void* stack) {
    STACK_OF(EVP_MD)* digest_stack = (STACK_OF(EVP_MD)*)stack;
    if (is_digest_fetchable(digest) && sk_EVP_MD_push(digest_stack, digest) > 0) {
        EVP_MD_up_ref(digest);
    }
}

static int name_cmp(const char* const* a, const char* const* b) {
    return strcasecmp(*a, *b);
}

static void collect_names(const char* name, void* stack) {
    STACK_OF(OPENSSL_CSTRING)* names = (STACK_OF(OPENSSL_CSTRING)*)stack;
    sk_OPENSSL_CSTRING_push(names, name);
}

const EVP_CIPHER* q_lookup_cipher(const char* cipher) {
    // first lookup aliases for backwards compatibility
    strmap_t::const_iterator i = cipher_compat_aliases.find(cipher);
    if (i != cipher_compat_aliases.end()) {
        cipher = i->second.c_str();
    }

    // refs #4910: use cache to avoid memory leaks from EVP_CIPHER_fetch()
    AutoLocker al(cipher_cache_lock);
    cipher_cache_t::iterator ci = cipher_cache.find(cipher);
    if (ci != cipher_cache.end()) {
        return ci->second;
    }

    // fetch and cache
    EVP_CIPHER* c = EVP_CIPHER_fetch(nullptr, cipher, nullptr);
    if (c) {
        cipher_cache[cipher] = c;
    }
    return c;
}

QoreHashNode* q_get_cipher_hash(const EVP_CIPHER* c) {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), nullptr);
    qore_hash_private* priv = qore_hash_private::get(**rv);

    const char* str = EVP_CIPHER_get0_name(c);
    if (str) {
        priv->setKeyValueIntern("name", new QoreStringNode(str));
    }
    str = EVP_CIPHER_get0_description(c);
    if (str) {
        priv->setKeyValueIntern("desc", new QoreStringNode(str));
    }
    {
        const char* type;
        switch (EVP_CIPHER_get_mode(c)) {
            case EVP_CIPH_ECB_MODE: type = "ECB"; break;
            case EVP_CIPH_CBC_MODE: type = "CBC"; break;
            case EVP_CIPH_CFB_MODE: type = "CFB"; break;
            case EVP_CIPH_OFB_MODE: type = "OFB"; break;
            case EVP_CIPH_CTR_MODE: type = "CTR"; break;
            case EVP_CIPH_GCM_MODE: type = "GCM"; break;
            case EVP_CIPH_CCM_MODE: type = "CCM"; break;
            case EVP_CIPH_XTS_MODE: type = "XTS"; break;
            case EVP_CIPH_WRAP_MODE: type = "WRAP"; break;
            case EVP_CIPH_OCB_MODE: type = "OCB"; break;
            case EVP_CIPH_SIV_MODE: type = "SIV"; break;
            case EVP_CIPH_STREAM_CIPHER: type = "STREAM"; break;
            default: type = "unknown"; break;
        }
        priv->setKeyValueIntern("type", new QoreStringNode(type));
    }

    priv->setKeyValueIntern("key_len", EVP_CIPHER_get_key_length(c));

    //const OSSL_PROVIDER* provider = EVP_CIPHER_get0_provider(c);
    unsigned long flags = EVP_CIPHER_get_flags(c);
    if (flags & EVP_CIPH_CUSTOM_IV) {
        priv->setKeyValueIntern("iv_len", -1);
    } else {
        int len = EVP_CIPHER_get_iv_length(c);
        priv->setKeyValueIntern("iv_len", len ? len : -1);
    }
    priv->setKeyValueIntern("aead", flags & EVP_CIPH_FLAG_AEAD_CIPHER ? true : false);
    priv->setKeyValueIntern("cts", flags & EVP_CIPH_FLAG_CTS ? true : false);
    priv->setKeyValueIntern("tls_multi", flags & EVP_CIPH_FLAG_TLS1_1_MULTIBLOCK ? true : false);
    priv->setKeyValueIntern("randkey", flags & EVP_CIPH_RAND_KEY ? true : false);

    return rv.release();
}
#endif

QoreHashNode* init_digest_map_hash() {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), nullptr);
    qore_hash_private* priv = qore_hash_private::get(**rv);

#ifdef OPENSSL_3_PLUS
    // get all digests
    STACK_OF(EVP_MD)* digests = sk_EVP_MD_new(md_cmp);
    if (digests) {
        EVP_MD_do_all_provided(nullptr, collect_digests, digests);
        sk_EVP_MD_sort(digests);

        for (int i = 0; i < sk_EVP_MD_num(digests); ++i) {
            const EVP_MD* m = sk_EVP_MD_value(digests, i);
            STACK_OF(OPENSSL_CSTRING)* names = sk_OPENSSL_CSTRING_new(name_cmp);
            if (names && EVP_MD_names_do_all(m, collect_names, names)) {
                sk_OPENSSL_CSTRING_sort(names);
                // add only the last name in the list
                int len = sk_OPENSSL_CSTRING_num(names);
                if (len) {
                    const char* name = sk_OPENSSL_CSTRING_value(names, len - 1);
                    assert(name);
                    assert(!priv->existsKey(name));
                    QoreString str(name);
                    str.tolwr();
                    priv->setKeyValueIntern(str.c_str(), true);
                }
            }
            sk_OPENSSL_CSTRING_free(names);
        }
        sk_EVP_MD_pop_free(digests, EVP_MD_free);
    }
#else
    for (auto& i : digest_map) {
        priv->setKeyValueIntern(i.first.c_str(), true);
    }
#endif

    return rv.release();
}

// returns hash<string, hash<auto>>
QoreHashNode* init_cipher_map_hash() {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(qore_get_complex_hash_type(autoHashTypeInfo)), nullptr);
    qore_hash_private* priv = qore_hash_private::get(**rv);

#ifdef OPENSSL_3_PLUS
    STACK_OF(EVP_CIPHER)* ciphers = sk_EVP_CIPHER_new(cipher_cmp);
    if (ciphers) {
        EVP_CIPHER_do_all_provided(nullptr, collect_ciphers, ciphers);
        sk_EVP_CIPHER_sort(ciphers);

        for (int i = 0; i < sk_EVP_CIPHER_num(ciphers); ++i) {
            const EVP_CIPHER* c = sk_EVP_CIPHER_value(ciphers, i);
            STACK_OF(OPENSSL_CSTRING)* names = sk_OPENSSL_CSTRING_new(name_cmp);
            if (names && EVP_CIPHER_names_do_all(c, collect_names, names)) {
                sk_OPENSSL_CSTRING_sort(names);
                // add only the last name in the list
                int len = sk_OPENSSL_CSTRING_num(names);
                if (len) {
                    const char* name = sk_OPENSSL_CSTRING_value(names, len - 1);
                    //printd(5, "init_cipher_map_hash() adding cipher %d (%d): '%s'\n", i, j, name);
                    QoreString str(name);
                    str.tolwr();
                    priv->setKeyValueIntern(str.c_str(), q_get_cipher_hash(c));
                }
            }
            sk_OPENSSL_CSTRING_free(names);
        }
        sk_EVP_CIPHER_pop_free(ciphers, EVP_CIPHER_free);
    }

#else
    for (auto& i : crypto_map) {
        priv->setKeyValueIntern(i.first.c_str(), i.second.getInfo());
    }
#endif

    return rv.release();
}

#ifdef OPENSSL_3_PLUS
void crypto_cache_cleanup() {
    // refs #4910: free all cached cipher references
    {
        AutoLocker al(cipher_cache_lock);
        for (auto& i : cipher_cache) {
            EVP_CIPHER_free(i.second);
        }
        cipher_cache.clear();
    }
    // also clean up digest cache
    digest_cache_cleanup();
}
#endif
