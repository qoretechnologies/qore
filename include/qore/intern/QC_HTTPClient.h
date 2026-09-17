/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_HTTPClient.h

    Qore Programming Language

    Copyright (C) 2006 - 2026 Qore Technologies, s.r.o.

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

#ifndef QC_HTTP_CLIENT_H_
#define QC_HTTP_CLIENT_H_

DLLEXPORT extern qore_classid_t CID_HTTPCLIENT;
DLLEXPORT extern QoreClass *QC_HTTPCLIENT;
DLLEXPORT extern QoreClass *QC_SOCKET;
DLLLOCAL QoreClass *initHTTPClientClass(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_HttpRedirectInfo(QoreNamespace& ns);

class HTTPInfoRefHelper {
protected:
    const ReferenceNode *ref;
    ExceptionSink *xsink;
    ReferenceHolder<QoreHashNode> info;

public:
    DLLLOCAL HTTPInfoRefHelper(const ReferenceNode *n_ref, QoreStringNode *msg, ExceptionSink *n_xsink) : ref(n_ref), xsink(n_xsink), info(new QoreHashNode(autoTypeInfo), xsink) {
        info->setKeyValue("request", msg, xsink);
    }

    DLLLOCAL ~HTTPInfoRefHelper() {
        // we have to create a temporary ExceptionSink if there is
        // an active exception, otherwise writing back the reference will fail
        assert(xsink);
        ExceptionSink *txsink = *xsink ? new ExceptionSink : xsink;

        // write info hash to reference
        QoreTypeSafeReferenceHelper rh(ref, txsink);
        if (!rh)
            return;

        if (rh.assign(info.release()))
            return;

        if (txsink != xsink)
            xsink->assimilate(txsink);
    }

    DLLLOCAL QoreHashNode *operator*() {
        return *info;
    }
};

#endif

// EOF
