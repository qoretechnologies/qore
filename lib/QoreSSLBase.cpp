/*
    QoreSSLBase.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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
#include <qore/QoreSSLBase.h>

#include <memory>

#define OBJ_BUF_LEN 80

// static method
QoreHashNode* QoreSSLBase::X509_NAME_to_hash(X509_NAME* n) {
    QoreHashNode* h = new QoreHashNode(autoTypeInfo);
    for (int i = 0; i < X509_NAME_entry_count(n); i++) {
        X509_NAME_ENTRY *e = X509_NAME_get_entry(n, i);

        ASN1_OBJECT *ko = X509_NAME_ENTRY_get_object(e);
        char key[OBJ_BUF_LEN + 1];

        OBJ_obj2txt(key, OBJ_BUF_LEN, ko, 0);
        ASN1_STRING *val = X509_NAME_ENTRY_get_data(e);
#ifdef HAVE_OPENSSL_INIT_CRYPTO
        //printd(5, "do_X509_name() %s=%s\n", key, ASN1_STRING_get0_data(val));
        h->setKeyValue(key, new QoreStringNode((const char *)ASN1_STRING_get0_data(val)), 0);
#else
        //printd(5, "do_X509_name() %s=%s\n", key, ASN1_STRING_data(val));
        h->setKeyValue(key, new QoreStringNode((const char *)ASN1_STRING_data(val)), 0);
#endif
    }
    return h;
}

// static method
DateTimeNode* QoreSSLBase::ASN1_TIME_to_DateTime(ASN1_STRING* t) {
    if (!t || !ASN1_TIME_check(t)) {
        return nullptr;
    }
    // Let OpenSSL expand the UTCTime century (1950-2049). This API also supports
    // the older OpenSSL versions supported by Qore, unlike ASN1_TIME_to_tm().
    std::unique_ptr<ASN1_GENERALIZEDTIME, decltype(&ASN1_GENERALIZEDTIME_free)> time(
        nullptr, ASN1_GENERALIZEDTIME_free);
    if (ASN1_STRING_type(t) == V_ASN1_UTCTIME) {
        time.reset(ASN1_TIME_to_generalizedtime(t, nullptr));
        if (!time) {
            return nullptr;
        }
        t = time.get();
    }
    // Do not convert GeneralizedTime again: OpenSSL can discard fractional seconds.
#ifdef HAVE_OPENSSL_INIT_CRYPTO
    const unsigned char* data = ASN1_STRING_get0_data(t);
#else
    const unsigned char* data = ASN1_STRING_data(t);
#endif
    QoreString str(reinterpret_cast<const char*>(data), ASN1_STRING_length(t));
    // ASN.1 permits omitted seconds; Qore's date parser needs them to read the zone.
    if (str.size() > 12 && (data[12] == 'Z' || data[12] == '+' || data[12] == '-')) {
        str.insert("00", 12);
    }
    ExceptionSink xsink;
    ReferenceHolder<DateTimeNode> date(new DateTimeNode(nullptr, str.c_str(), &xsink), &xsink);
    if (xsink) {
        xsink.clear();
        return nullptr;
    }
    date->setZone(nullptr);
    return date.release();
}

// static method
QoreStringNode* QoreSSLBase::ASN1_OBJECT_to_QoreStringNode(ASN1_OBJECT* o) {
    BIO* bp = BIO_new(BIO_s_mem());
    i2a_ASN1_OBJECT(bp, o);
    char* buf;
    long len = BIO_get_mem_data(bp, &buf);
    QoreStringNode* str = new QoreStringNode(buf, (int)len);
    BIO_free(bp);
    return str;
}
