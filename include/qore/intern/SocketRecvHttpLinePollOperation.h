/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SocketRecvHttpLinePollOperation.h

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

#ifndef _QORE_INTERN_SOCKETRECVHTTPLINEPOLLOPERATION_H
#define _QORE_INTERN_SOCKETRECVHTTPLINEPOLLOPERATION_H

#include <qore/SocketPollOperation.h>

//! Receives until a byte pattern is matched, for an HTTP header section, a line of a chunked body or a line of a trailer
//! section, with the size limit of the socket (qore#6867)
/** An HTTP-HEADER-TOO-LARGE exception is raised, and no more data is read or kept, when the data received, with the
    \a used bytes of the section that were read before, is larger than the maximum header size of the socket (see
    Socket::setMaxHeaderSize()).
*/
class SocketRecvHttpLinePollOperation : public SocketRecvUntilBytesPollOperation {
public:
    //! Creates the operation
    /** @param xsink exception sink
        @param pattern the byte pattern to match (ref'd internally)
        @param sock the socket (will be ref'd)
        @param to_string if true, return a string; otherwise binary
        @param defer_init if true, socket non-blocking setup is deferred until the async controller runs the operation
        @param what what is read, for the exception: \c "header", \c "chunked body trailer" or \c "chunk size line"
        @param used the size of the part of the section that was read before, which counts against the limit
        @param message_max if not 0, the data is a message (a response that can be large) and this is its maximum size
            in bytes (a SOCKET-RESPONSE-TOO-LARGE exception is raised for more), instead of the maximum header size of
            the socket (qore#6914)
    */
    DLLLOCAL SocketRecvHttpLinePollOperation(ExceptionSink* xsink, const QoreStringNode* pattern,
            QoreSocketObject* sock, bool to_string, bool defer_init, const char* what, size_t used = 0,
            size_t message_max = 0);

private:
    DLLLOCAL virtual int initPollState(ExceptionSink* xsink) override;

    //! limits the poll state; the socket lock must be held
    DLLLOCAL int applyLimit(ExceptionSink* xsink);

    SimpleRefHolder<QoreStringNode> http_pattern;
    const char* what;
    size_t used;
    size_t message_max;
};

#endif // _QORE_INTERN_SOCKETRECVHTTPLINEPOLLOPERATION_H
