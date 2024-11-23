/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreSSLCertificate.h

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

#ifndef _QORE_QORESSLCERTIFICATE_H

#define _QORE_QORESSLCERTIFICATE_H

#include <qore/QoreSSLBase.h>

#include <openssl/ssl.h>
#include <openssl/pem.h>

//! represents an X509 certificate, reference-counted, dynamically-allocated only
class QoreSSLCertificate : public AbstractPrivateData, public QoreSSLBase {
friend class SSLSocketHelper;
public:
    //! creates the object from a pointer to an X509 data structure, the QoreSSLCertificate object takes ownership of the X509 pointer
    /** @param c a pointer to an X509 data structure, the QoreSSLCertificate object takes ownership of the X509 pointer
    */
    DLLEXPORT QoreSSLCertificate(X509* c);

    //! creates the object from a pointer to a BinaryNode object (certificate data in DER format)
    /** @param bin a pointer to a BinaryNode object with the raw binary certificate information
        @param xsink Qore-language exceptions are raised here in case of errors
    */
    DLLEXPORT QoreSSLCertificate(const BinaryNode* bin, ExceptionSink* xsink);

    //! create the object from a pointer to a QoreString representing the X.509 certificate in PEM format
    /** @param str a pointer to a QoreString with the certificatge in PEM format
        @param xsink Qore-language exceptions are raised here in case of errors
    */
    DLLEXPORT QoreSSLCertificate(const QoreString* str, ExceptionSink* xsink);

    //! creates the object from a filename
    /** @param fn the filename of the certificate file in PEM format
        @param xsink Qore-language exceptions are raised here in case of errors
    */
    DLLLOCAL QoreSSLCertificate(const char* fn, ExceptionSink* xsink);

    //! returns true if the object is valid, false if not
    DLLEXPORT operator bool() const;

    //! returns a string in PEM format representing the certificate; caller owns the QoreStringNode reference returned
    /** @return a string in PEM format representing the certificate; caller owns the QoreStringNode reference returned
    */
    DLLEXPORT QoreStringNode* getPEM(ExceptionSink* xsink) const;

    //! returns a binary in DER format representing the certificate, caller owns the BinaryNode reference count returned
    /** @return a binary in DER format representing the certificate, caller owns the BinaryNode reference count returned

        @since %Qore 0.9
    */
    DLLEXPORT BinaryNode* getDER(ExceptionSink* xsink) const;

    // caller does NOT own the X509 pointer returned; "const" cannot be used because of the openssl API does not support it
    DLLEXPORT X509* getData() const;

    // caller owns value returned
    DLLEXPORT QoreHashNode* getSubjectHash() const;

    // caller owns value returned
    DLLEXPORT QoreHashNode* getIssuerHash() const;
    DLLEXPORT int64 getSerialNumber() const;
    DLLEXPORT int64 getVersion() const;

    // caller owns value returned
    DLLEXPORT QoreHashNode* getPurposeHash() const;

    // caller owns value returned
    DLLEXPORT DateTimeNode* getNotBeforeDate() const;

    // caller owns value returned
    DLLEXPORT DateTimeNode* getNotAfterDate() const;

    // caller owns value returned
    DLLEXPORT QoreStringNode* getSignatureType() const;

    // caller owns value returned
    DLLEXPORT BinaryNode* getSignature() const;

    // caller owns value returned
    DLLEXPORT QoreStringNode* getPublicKeyAlgorithm() const;

    // caller owns value returned
    DLLEXPORT BinaryNode* getPublicKey() const;

    // caller owns value returned
    DLLEXPORT QoreHashNode* getInfo() const;

    // caller owns reference returned
    DLLEXPORT QoreSSLCertificate* certRefSelf() const;

protected:
    //! the destructor is protected to ensure that it's only dynamically allocated (use deref() to delete)
    DLLLOCAL virtual ~QoreSSLCertificate();

private:
    //! private implementation of the class
    struct qore_sslcert_private* priv;

    DLLLOCAL QoreValue doPurposeValue(int id, int ca) const;

    //! this function is not implemented
    DLLLOCAL QoreSSLCertificate(const QoreSSLCertificate&) = delete;

    //! this function is not implemented
    DLLLOCAL QoreSSLCertificate& operator=(const QoreSSLCertificate&) = delete;
};

#endif
