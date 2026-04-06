/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_HttpClientPingPollOperationBase.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_CLASS_HTTPCLIENTPINGPOLLOPERATIONBASE_H

#define _QORE_CLASS_HTTPCLIENTPINGPOLLOPERATIONBASE_H

#include "qore/SocketPollOperationBase.h"
#include "qore/ReferenceHolder.h"

#include <string>

//! C++ base for HTTP client ping/verify poll operations
/** This single class handles WebSocket ping, SSE connect verification, and
    generic HTTP ping operations. It wraps an inner poll operation (from
    HTTPClient.startPollSendRecv) and validates the response when it completes.

    Configuration via constructor parameters:
    - Status code range (e.g. 101-101 for WebSocket, 200-299 for HTTP)
    - Optional header name + prefix validation (e.g. Content-Type: text/event-stream)
    - Optional WebSocket Sec-WebSocket-Accept key validation

    The inner operation handles the entire HTTP request/response lifecycle
    (connect, optional SSL, send request, read response). This outer class
    just delegates continuePoll() to the inner op and validates the response
    when it completes.

    @since %Qore 2.3
*/
class HttpClientPingPollOperationPriv : public SocketPollOperationBase {
public:
    //! Creates the ping/verify poll operation
    /** @param self the QoreObject wrapping this private data
        @param inner_op the inner poll operation (from HTTPClient.startPollSendRecv); already ref'd by caller
        @param inner_obj the QoreObject wrapping inner_op; already ref'd by caller
        @param expected_status_lo lowest acceptable HTTP status code (inclusive)
        @param expected_status_hi highest acceptable HTTP status code (inclusive)
        @param err_code Qore exception error code for validation failures
        @param validate_header_name optional response header name to validate (lowercase)
        @param validate_header_prefix optional required prefix for the header value
        @param ws_key optional WebSocket key for Sec-WebSocket-Accept validation
    */
    DLLLOCAL HttpClientPingPollOperationPriv(QoreObject* self,
        SocketPollOperationBase* inner_op, QoreObject* inner_obj,
        int expected_status_lo, int expected_status_hi,
        const char* err_code,
        const char* validate_header_name = nullptr,
        const char* validate_header_prefix = nullptr,
        const char* ws_key = nullptr);

    DLLLOCAL virtual ~HttpClientPingPollOperationPriv();

    // --- SocketPollOperationBase overrides ---

    DLLLOCAL bool goalReached() const override {
        return goal_reached;
    }

    DLLLOCAL QoreHashNode* continuePoll(ExceptionSink* xsink) override;
    DLLLOCAL void abort(ExceptionSink* xsink) override;
    DLLLOCAL QoreValue getOutput() const override;

    // --- Accessors ---

    DLLLOCAL bool isClosed() const {
        return closed;
    }

    DLLLOCAL bool hasError() const {
        return error_info != nullptr;
    }

    DLLLOCAL QoreHashNode* getErrorInfo() const {
        if (error_info) {
            error_info->ref();
        }
        return error_info;
    }

    DLLLOCAL void cleanup(ExceptionSink* xsink);

protected:
    DLLLOCAL const char* getStateImpl() const override;

private:
    //! The inner poll operation (ref'd)
    SocketPollOperationBase* inner_op;

    //! QoreObject wrapper for inner_op (ref'd, keeps inner_op alive)
    QoreObject* inner_obj;

    //! Whether the operation has completed (success or failure)
    bool completed = false;

    //! Whether the goal has been reached (response validated successfully)
    bool goal_reached = false;

    //! Whether the operation has been closed/aborted
    bool closed = false;

    // --- Validation configuration ---

    //! Lowest acceptable HTTP status code (inclusive)
    int expected_status_lo;

    //! Highest acceptable HTTP status code (inclusive)
    int expected_status_hi;

    //! Qore exception error code for validation failures
    std::string err_code;

    //! Optional response header name to validate (lowercase, empty = no check)
    std::string validate_header_name;

    //! Optional required prefix for the validated header value
    std::string validate_header_prefix;

    //! Pre-computed expected Sec-WebSocket-Accept value (empty = no WS check)
    std::string expected_ws_accept;

    // --- Output ---

    //! Response output hash (ref'd or nullptr)
    QoreHashNode* output = nullptr;

    //! Error info hash (ref'd or nullptr)
    QoreHashNode* error_info = nullptr;

    //! Validates the HTTP response and sets goal_reached or error
    DLLLOCAL void validateResponse(ExceptionSink* xsink);

    //! Sets error state with the configured error code
    DLLLOCAL void setError(const char* desc, ExceptionSink* xsink);
};

DLLLOCAL QoreClass* initHttpClientPingPollOperationBaseClass(QoreNamespace& qorens);

#endif // _QORE_CLASS_HTTPCLIENTPINGPOLLOPERATIONBASE_H
