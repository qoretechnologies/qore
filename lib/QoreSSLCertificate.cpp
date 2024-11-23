/*
    QoreSSLCertificate.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2023 Qore Technologies, s.r.o.

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

#include <qore/Qore.h>
#include "qore/intern/QC_SSLCertificate.h"
#include "qore/intern/QoreSSLIntern.h"

#include <openssl/err.h>

QoreSSLCertificate::~QoreSSLCertificate() {
    delete priv;
}

QoreSSLCertificate::QoreSSLCertificate(X509* c) : priv(new qore_sslcert_private(c)) {
}

QoreSSLCertificate::QoreSSLCertificate(const BinaryNode* bin, ExceptionSink* xsink)
        : priv(new qore_sslcert_private(0)) {
    OPENSSL_CONST unsigned char* p = (OPENSSL_CONST unsigned char*)bin->getPtr();
    priv->cert = d2i_X509(0, &p, (int)bin->size());
    if (!priv->cert) {
        long e = ERR_get_error();
        char buf[121];
        ERR_error_string(e, buf);
        xsink->raiseException("SSLCERTIFICATE-CONSTRUCTOR-ERROR", buf);
    }
}

// DEPRECATED constructor
QoreSSLCertificate::QoreSSLCertificate(const char* fn, ExceptionSink* xsink) : priv(new qore_sslcert_private(0)) {
    FILE* fp = fopen(fn, "r");
    if (!fp) {
        xsink->raiseErrnoException("SSLCERTIFICATE-CONSTRUCTOR-ERROR", errno, "'%s'", fn);
        return;
    }

    PEM_read_X509(fp, &priv->cert, 0, 0);
    fclose(fp);
    if (!priv->cert) {
        xsink->raiseException("SSLCERTIFICATE-CONSTRUCTOR-ERROR", "error parsing certificate file '%s'", fn);
    }
}

QoreSSLCertificate::QoreSSLCertificate(const QoreString* str, ExceptionSink* xsink) : priv(new qore_sslcert_private(0)) {
    TempEncodingHelper tstr(str, QCS_DEFAULT, xsink);
    if (*xsink) {
        return;
    }

    QoreMemBIO mbio(str);
    PEM_read_bio_X509(mbio.getBIO(), &priv->cert, 0, 0);
    if (!priv->cert) {
        xsink->raiseException("SSLCERTIFICATE-CONSTRUCTOR-ERROR", "error parsing certificate PEM string");
    }
    while (!mbio.eof()) {
        X509* cert = PEM_read_bio_X509(mbio.getBIO(), nullptr, 0, 0);
        if (cert) {
            priv->addCert(cert);
        }
    }
}

QoreSSLCertificate::operator bool() const { return (bool)priv->cert; }

QoreValue QoreSSLCertificate::doPurposeValue(int id, int ca) const {
   int rc = X509_check_purpose(priv->cert, id, ca);
   if (rc == 1)
      return true;

   if (!rc)
      return false;

   return rc;
}

X509* QoreSSLCertificate::getData() const {
   return priv->cert;
}

QoreStringNode* QoreSSLCertificate::getPEM(ExceptionSink* xsink) const {
    QoreMemBIO b;

    if (!b.writePEMX509(priv->cert)) {
        xsink->raiseException("X509-ERROR", "could not create PEM string from X.509 certificate data");
        return nullptr;
    }

    return b.getAsString();
}

BinaryNode* QoreSSLCertificate::getDER(ExceptionSink* xsink) const {
    QoreMemBIO bio;
    if (i2d_X509_bio(*bio, priv->cert) <= 0) {
        xsink->raiseException("X509-ERROR", "could not create DER binary from X.509 certificate data");
        return nullptr;
    }
    return bio.getAsBinary();
}

QoreHashNode* QoreSSLCertificate::getSubjectHash() const {
    return X509_NAME_to_hash(X509_get_subject_name(priv->cert));
}

QoreHashNode* QoreSSLCertificate::getIssuerHash() const {
    return X509_NAME_to_hash(X509_get_issuer_name(priv->cert));
}

int64 QoreSSLCertificate::getSerialNumber() const {
    return (int64)ASN1_INTEGER_get(X509_get_serialNumber(priv->cert));
}

int64 QoreSSLCertificate::getVersion() const {
    return (int64)(X509_get_version(priv->cert) + 1);
}

DateTimeNode* QoreSSLCertificate::getNotBeforeDate() const {
    return ASN1_TIME_to_DateTime(X509_get_notBefore(priv->cert));
}

DateTimeNode* QoreSSLCertificate::getNotAfterDate() const {
    return ASN1_TIME_to_DateTime(X509_get_notAfter(priv->cert));
}

QoreStringNode* QoreSSLCertificate::getSignatureType() const {
    return ASN1_OBJECT_to_QoreStringNode(priv->getAlgorithm());
}

BinaryNode* QoreSSLCertificate::getSignature() const {
    return priv->getBinary();
}

QoreStringNode* QoreSSLCertificate::getPublicKeyAlgorithm() const {
    return priv->getPublicKeyAlgorithm();
}

// returns the public key for the certificate in DER format
BinaryNode* QoreSSLCertificate::getPublicKey() const {
    EVP_PKEY* key = priv->getPublicKey();
    if (!key)
        return nullptr;

    int size = i2d_PUBKEY(key, 0);
    //printd(5, "QoreSSLCertificate::getPublicKey() public key size=%d\n", size);
    unsigned char* buf = 0;
    i2d_PUBKEY(key, &buf);
    if (!buf)
        return nullptr;

    return new BinaryNode(buf, size);
}

QoreHashNode* QoreSSLCertificate::getPurposeHash() const {
    QoreHashNode* h = new QoreHashNode(autoTypeInfo);
    QoreString tstr;
    for (int i = 0; i < X509_PURPOSE_get_count(); i++) {
        X509_PURPOSE* pt = X509_PURPOSE_get0(i);

        int id = X509_PURPOSE_get_id(pt);
        const char* name,* nameca;
        switch (id) {
            case X509_PURPOSE_SSL_CLIENT:
                name = "SSLclient";
                nameca = "SSLclientCA";
                break;
            case X509_PURPOSE_SSL_SERVER:
                name = "SSLserver";
                nameca = "SSLserverCA";
                break;
            case X509_PURPOSE_NS_SSL_SERVER:
                name = "netscapeSSLserver";
                nameca = "netscapeSSLserverCA";
                break;
            case X509_PURPOSE_SMIME_SIGN:
                name = "SMIMEsigning";
                nameca = "SMIMEsigningCA";
                break;
            case X509_PURPOSE_SMIME_ENCRYPT:
                name = "SMIMEencryption";
                nameca = "SMIMEencryptionCA";
                break;
            case X509_PURPOSE_CRL_SIGN:
                name = "CRLsigning";
                nameca = "CRLsigningCA";
                break;
            case X509_PURPOSE_ANY:
                name = "anyPurpose";
                nameca = "anyPurposeCA";
                break;
            case X509_PURPOSE_OCSP_HELPER:
                name = "OCSPhelper";
                nameca = "OCSPhelperCA";
                break;
            default:
                name = X509_PURPOSE_get0_name(pt);
                tstr.clear();
                tstr.concat(name);
                tstr.concat("CA");
                nameca = (char* )tstr.getBuffer();
                break;
        }

        // get non-CA value
        h->setKeyValue(name, doPurposeValue(id, 0), nullptr);
        // get CA value
        h->setKeyValue(nameca, doPurposeValue(id, 1), nullptr);
    }
    return h;
}

QoreHashNode* QoreSSLCertificate::getInfo() const {
    QoreHashNode* h = new QoreHashNode(autoTypeInfo);
    // get version
    h->setKeyValue("version", getVersion(), nullptr);
    // get serial number
    h->setKeyValue("serialNumber", getSerialNumber(), nullptr);
    // do subject
    h->setKeyValue("subject", getSubjectHash(), nullptr);
    // do issuer
    h->setKeyValue("issuer", getIssuerHash(), nullptr);
    // get purposes
    h->setKeyValue("purposes", getPurposeHash(), nullptr);
    // get not before date
    h->setKeyValue("notBefore", getNotBeforeDate(), nullptr);
    // get not after date
    h->setKeyValue("notAfter", getNotAfterDate(), nullptr);
    // get signature type
    h->setKeyValue("signatureType", getSignatureType(), nullptr);
    // get signature
    //h->setKeyValue("signature", getSignature(), nullptr);
    // get public key
    //h->setKeyValue("publicKey", getPublicKey(), nullptr);

    return h;
}

QoreSSLCertificate* QoreSSLCertificate::certRefSelf() const {
    const_cast<QoreSSLCertificate*>(this)->ref();
    return const_cast<QoreSSLCertificate*>(this);
}
