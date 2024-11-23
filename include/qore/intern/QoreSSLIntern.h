/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreSSLIntern.h

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

#ifndef _QORE_QORESSLINTERN_H

#define _QORE_QORESSLINTERN_H

class QoreBIO {
public:
    DLLLOCAL ~QoreBIO() { if (b) BIO_free(b); }

    DLLLOCAL int writePEMX509(X509* cert) { return PEM_write_bio_X509(b, cert); }

    DLLLOCAL long getMemData(char** buf) { return BIO_get_mem_data(b, buf); }

    DLLLOCAL QoreStringNode* getAsString() const {
        char* buf;
        long len = BIO_get_mem_data(b, &buf);
        return new QoreStringNode(buf, (int)len);
    }

    DLLLOCAL BinaryNode* getAsBinary() const {
        char* buf;
        long len = BIO_get_mem_data(b, &buf);
        SimpleRefHolder<BinaryNode> rv(new BinaryNode);
        rv->append(buf, (size_t)len);
        return rv.release();
    }

    DLLLOCAL X509* getX509(X509** x) { return d2i_X509_bio(b, x); }

    DLLLOCAL BIO* getBIO() { return b; }
    DLLLOCAL BIO* operator*() const { return b; }

    DLLLOCAL bool eof() const {
        return BIO_eof(b) ? true : false;
    }

protected:
    BIO* b;

    DLLLOCAL QoreBIO(BIO* b) : b(b) {}
};

class QoreMemBIO : public QoreBIO {
public:
    DLLLOCAL QoreMemBIO() : QoreBIO(BIO_new(BIO_s_mem())) {}
    DLLLOCAL QoreMemBIO(const BinaryNode* b) : QoreBIO(BIO_new_mem_buf((void*)b->getPtr(), (int)b->size())) {}
    DLLLOCAL QoreMemBIO(const QoreString* str) : QoreBIO(BIO_new_mem_buf((void*)str->getBuffer(), (int)str->strlen())) {}
};

#endif
