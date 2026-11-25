/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_SSLTrustStore.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 David Nichols

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

#ifndef _QORE_CLASS_SSLTRUSTSTORE_H
#define _QORE_CLASS_SSLTRUSTSTORE_H

#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <string>

DLLEXPORT extern qore_classid_t CID_SSLTRUSTSTORE;
DLLEXPORT extern QoreClass* QC_SSLTRUSTSTORE;

DLLLOCAL QoreClass* initSSLTrustStoreClass(QoreNamespace& ns);

//! An immutable set of trusted CA certificates in memory (qoretechnologies/qore#7248)
/** The OpenSSL X509_STORE is built once per distinct set of certificates and shared by every object made from the
    same set, across Programs: a process-wide cache keyed by the SHA-256 digest of the sorted DER encodings of the
    certificates counts its users.  A connection that verifies with it takes a reference of its own on the
    X509_STORE (SSL_set1_verify_cert_store()), so the store outlives the object while a connection uses it.

    The certificates cannot be read back, and the store cannot be changed after it has been created.
*/
class QoreSSLTrustStore : public AbstractPrivateData {
public:
    //! Creates the object from the store of a cache entry; the object is a user of the entry
    DLLLOCAL QoreSSLTrustStore(X509_STORE* store, const std::string& key, size_t count)
            : store(store), key(key), count(count) {
    }

    //! Returns the shared X509_STORE; not to be changed; take a reference to keep it
    DLLLOCAL X509_STORE* getStore() const {
        return store;
    }

    //! Returns the number of certificates
    DLLLOCAL size_t size() const {
        return count;
    }

    //! Returns the hex SHA-256 identity of the certificate set
    DLLLOCAL const std::string& getId() const {
        return key;
    }

    //! Applies the store as the verification store of an SSL connection; returns 0 on success
    /** The connection takes a reference of its own on the store
    */
    DLLLOCAL int applyTo(SSL* ssl) const;

    //! Returns a trust store for the given DER-encoded certificates; the caller owns the reference returned
    /** Builds the X509_STORE only if no store for the same set of certificates exists in the process

        @param certs the certificates (X509 pointers are not taken over)
        @param count the number of certificates
    */
    DLLLOCAL static QoreSSLTrustStore* get(X509* const* certs, size_t count, ExceptionSink* xsink);

    //! Returns a trust store for the CA certificates of CA locations, read with the filesystem policy of the caller
    /** Under a sandbox policy, CA locations are read when they are set, into a trust store, so that no file is read by
        its name when a connection is verified (qore#7518); the CA path gives the certificates of its files with
        OpenSSL hashed names; the caller owns the reference returned

        @return the trust store, or nullptr if an exception has been raised
    */
    DLLLOCAL static QoreSSLTrustStore* fromCaLocations(const char* ca_file, const char* ca_path, ExceptionSink* xsink);

    //! Returns the number of distinct stores in the process cache (for tests)
    DLLLOCAL static size_t getCacheSize();

protected:
    DLLLOCAL virtual ~QoreSSLTrustStore();

private:
    //! The shared store; one reference per cache entry, freed with the last user
    X509_STORE* store;
    //! The identity of the certificate set
    std::string key;
    //! The number of certificates
    size_t count;
};

#endif
