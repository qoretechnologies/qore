/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHTTPClient.h

    Qore Programming Language

    Copyright (C) 2006 - 2024 Qore Technologies, s.r.o.

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

/* this file is deprecated; use QoreHttpClientObject instead
   this class is only kept for ABI compatibility
*/

#ifndef QORE_HTTP_CLIENT_H_
#define QORE_HTTP_CLIENT_H_

#include <qore/common.h>
#include <qore/AbstractPrivateData.h>
#include <qore/QoreThreadLock.h>
#include <qore/QoreSocket.h>
#include <qore/QoreHttpClientObject.h>

class Queue;

//! provides a way to communicate with HTTP servers using Qore data structures
/** thread-safe, uses QoreSocket for socket communication
*/
class QoreHTTPClient : public QoreHttpClientObject {
public:
    //! creates the QoreHTTPClient object
    DLLEXPORT QoreHTTPClient();

    //! destroys the object and frees all associated memory
    DLLEXPORT virtual ~QoreHTTPClient();

    DLLEXPORT void setDefaultPath(const char* pth);
    DLLEXPORT void setDefaultHeaderValue(const char* header, const char* val);
    DLLEXPORT void addProtocol(const char* prot, int port, bool ssl = false);

    DLLEXPORT int setOptions(const QoreHashNode* opts, ExceptionSink* xsink);
    DLLEXPORT const QoreEncoding* getEncoding() const;

    DLLEXPORT int connect(ExceptionSink* xsink);
    DLLEXPORT QoreHashNode* send(const char* meth, const char* path, const QoreHashNode* headers, const void* data,
            unsigned size, bool getbody, QoreHashNode* info, ExceptionSink* xsink);

    DLLEXPORT void setEventQueue(Queue* cbq, ExceptionSink* xsink);

    DLLEXPORT void setEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data);
};

#endif
