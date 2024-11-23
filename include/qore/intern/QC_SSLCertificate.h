/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QC_SSLCertificate.h

  Qore Programming Language

  Copyright (C) 2003 - 2023 David Nichols

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

#ifndef _QORE_CLASS_SSLCERTIFICATE_H

#define _QORE_CLASS_SSLCERTIFICATE_H

DLLEXPORT extern qore_classid_t CID_SSLCERTIFICATE;
DLLEXPORT extern QoreClass *QC_SSLCERTIFICATE;
DLLLOCAL QoreClass *initSSLCertificateClass(QoreNamespace& ns);

#include <qore/QoreSSLCertificate.h>

#include <vector>

struct qore_sslcert_private {
    X509* cert;
    typedef std::vector<X509*> certvec_t;
    certvec_t chain;

    DLLLOCAL qore_sslcert_private(X509* c) : cert(c) {
    }

    DLLLOCAL ~qore_sslcert_private() {
        if (cert) {
            X509_free(cert);
        }
        for (auto& i : chain) {
            X509_free(i);
        }
    }

    DLLLOCAL void addCert(X509* cert) {
        chain.push_back(cert);
    }

    DLLLOCAL ASN1_OBJECT* getAlgorithm() {
#ifdef HAVE_X509_GET_SIGNATURE_NID
        return OBJ_nid2obj(X509_get_signature_nid(cert));
#else
        return cert->sig_alg->algorithm;
#endif
    }

    DLLLOCAL BinaryNode* getBinary() {
#ifdef HAVE_X509_GET_SIGNATURE_NID
        OPENSSL_X509_GET0_SIGNATURE_CONST ASN1_BIT_STRING* sig;
        OPENSSL_X509_GET0_SIGNATURE_CONST X509_ALGOR* alg;
        X509_get0_signature(&sig, &alg, cert);
        BinaryNode* rv = new BinaryNode;
        rv->append(sig->data, sig->length);
        return rv;
#else
        int len = cert->signature->length;
        char* buf = (char*)malloc(len);
        // FIXME: should throw an out of memory exception here
        if (!buf) {
            return new BinaryNode;
        }

        memcpy(buf, cert->signature->data, len);
        return new BinaryNode(buf, len);
#endif
    }

    DLLLOCAL EVP_PKEY* getPublicKey() {
#ifdef HAVE_X509_GET_SIGNATURE_NID
#ifdef HAVE_X509_GET0_PUBKEY
        return X509_get0_pubkey(cert);
#else
        return X509_get_pubkey(cert);
#endif
#else
        return X509_PUBKEY_get(cert->cert_info->key);
#endif
    }

    DLLLOCAL QoreStringNode* getPublicKeyAlgorithm() {
#ifdef HAVE_X509_GET_SIGNATURE_NID
        EVP_PKEY* pkey = getPublicKey();
        int nid;
        if (EVP_PKEY_get_default_digest_nid(pkey, &nid) <= 0)
            return new QoreStringNode("unknown");
        return QoreSSLBase::ASN1_OBJECT_to_QoreStringNode(OBJ_nid2obj(nid));
#else
        return QoreSSLBase::ASN1_OBJECT_to_QoreStringNode(cert->cert_info->key->algor->algorithm);
#endif
    }
};

#endif // _QORE_CLASS_SSLCERTIFICATE_H
