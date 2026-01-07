/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    EncryptionTransforms.h

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

#ifndef _QORE_ENCRYPTIONTRANSFORMS_H
#define _QORE_ENCRYPTIONTRANSFORMS_H

#include "qore/Transform.h"

#include <openssl/evp.h>

class EncryptionTransforms {
public:
    DLLLOCAL static Transform* getCryptoTransform(const char* cipher, bool do_crypt, const char* key,
            unsigned key_len, const char* iv, unsigned iv_len, const char* mac, unsigned mac_len,
            unsigned tag_length, const ReferenceNode* mac_ref, const char* aad, unsigned aad_len,
            ExceptionSink* xsink);
};

struct CryptoEntry {
    // the length of the key; 0 = variable length key
    unsigned key_len;
    // the OpenSSL cipher type
    const EVP_CIPHER* cipher_type;
    // the initialization vector length
    int iv_len;
    // does the algorithm use Galois Counter Mode (GCM)?
    bool gcm;
    // is the algorithm an AEAD cipher?
    bool aead;
    // does the algorithm support CTS?
    bool cts;
    // does the algorithm support TLS 1.1 multiblock operations?
    bool tls_multi;
    // does the algorithm support a random key?
    bool randkey;

#ifndef OPENSSL_3_PLUS
    DLLLOCAL QoreHashNode* getInfo() const;
#endif
};

// no need for the crypto or digest maps with openssl 3+
#ifndef OPENSSL_3_PLUS
// maps from encryption strings to configurations
typedef std::map<std::string, CryptoEntry, ltstrcase> crypto_map_t;
DLLLOCAL extern crypto_map_t crypto_map;

// maps from digest strings to algorithms
typedef std::map<std::string, const EVP_MD*, ltstrcase> digest_map_t;
DLLLOCAL extern digest_map_t digest_map;
#else
const EVP_CIPHER* q_lookup_cipher(const char* cipher);
DLLLOCAL QoreHashNode* q_get_cipher_hash(const EVP_CIPHER* c);
// refs #4910: cleanup function to free cached cipher/digest references
DLLLOCAL void crypto_cache_cleanup();
#endif

// init hash for giest encryption transformation constant
DLLLOCAL QoreHashNode* init_digest_map_hash();
// init hash for cipher encryption transformation constant
DLLLOCAL QoreHashNode* init_cipher_map_hash();

#endif // _QORE_ENCRYPTIONTRANSFORMS_H
