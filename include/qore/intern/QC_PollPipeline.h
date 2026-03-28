/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_PollPipeline.h

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

#ifndef _QORE_CLASS_POLLPIPELINE_H

#define _QORE_CLASS_POLLPIPELINE_H

#include "qore/intern/QC_SocketPollOperationBase.h"
#include "qore/intern/QC_SocketPollOperation.h"
#include "qore/intern/AsyncCompletionAction.h"

#include <string>
#include <vector>
#include <functional>

//! Declarative C++ poll pipeline — chains inner poll operations as steps
/** Qore modules describe their I/O state machine by adding steps via builder
    methods. The I/O thread executes the entire pipeline natively through
    continuePoll(), only delivering the final result via AbstractAsyncAction.

    Step types map to existing C++ inner poll operations:
    - CONNECT → SocketConnectPollOperation
    - SSL_UPGRADE → SocketUpgradeClientSslPollOperation
    - SEND → SocketSendPollOperation
    - RECV_HEADER → SocketReadHttpHeaderPollOperation
    - RECV_EXACT → SocketRecvPollOperation (N bytes)
    - RECV_UNTIL → SocketRecvUntilBytesPollOperation (delimiter)
    - RECV_DATA → SocketRecvDataPollOperation (available data)
    - BRANCH → conditional jump (pure C++, no I/O)
    - TRANSFORM → C++ function modifies context (no I/O)
    - DELIVER_RESULT → execute AbstractAsyncAction

    @since %Qore 2.3
*/
class PollPipelinePriv : public SocketPollOperationBase {
public:
    //! Step types
    enum class StepType : uint8_t {
        CONNECT,
        CONNECT_SSL,
        SSL_UPGRADE,
        SEND,
        RECV_HEADER,
        RECV_EXACT,
        RECV_UNTIL,
        RECV_DATA,
        READ_HTTP_BODY,
        DELEGATE,
        BRANCH,
        TRANSFORM,
        DELIVER_RESULT
    };

    //! Pipeline execution context — accumulated across steps on I/O thread
    struct Context {
        int status_code = 0;
        QoreHashNode* headers = nullptr;    //!< ref'd, from RECV_HEADER
        BinaryNode* body = nullptr;         //!< ref'd, accumulated body data
        QoreValue last_output;              //!< output from previous step
        QoreHashNode* extra = nullptr;      //!< ref'd, arbitrary step data

        DLLLOCAL void cleanup(ExceptionSink* xsink) {
            if (headers) { headers->deref(xsink); headers = nullptr; }
            if (body) { body->deref(xsink); body = nullptr; }
            last_output.discard(xsink);
            last_output = QoreValue();
            if (extra) { extra->deref(xsink); extra = nullptr; }
        }
    };

    //! Branch condition function — returns true to take true_step, false for false_step
    using BranchCondition = std::function<bool(const Context&)>;

    //! Transform function — modifies context in place
    using TransformFn = std::function<void(Context&, ExceptionSink*)>;

    //! A single pipeline step
    struct Step {
        StepType type;

        // Config per type
        std::string target;         //!< CONNECT/CONNECT_SSL: "host:port"
        int data_idx = -1;          //!< SEND: index into binary_data table
        int delegate_idx = -1;      //!< DELEGATE: index into delegate_ops table
        size_t recv_size = 0;       //!< RECV_EXACT: byte count
        int pattern_idx = -1;       //!< RECV_UNTIL: index into string_data table
        bool recv_as_string = false;//!< RECV_EXACT/RECV_UNTIL/RECV_DATA: return as string

        // BRANCH config
        BranchCondition condition;
        int true_step = -1;
        int false_step = -1;

        // TRANSFORM config
        TransformFn transform;

        int next_step = -1;         //!< override next step (-1 = sequential)
    };

    //! Creates the pipeline with a socket
    DLLLOCAL PollPipelinePriv(QoreObject* self, QoreSocketObject* sock);
    DLLLOCAL virtual ~PollPipelinePriv();

    // --- SocketPollOperationBase overrides ---

    DLLLOCAL bool goalReached() const override { return completed; }
    DLLLOCAL QoreHashNode* continuePoll(ExceptionSink* xsink) override;
    DLLLOCAL void abort(ExceptionSink* xsink) override;
    DLLLOCAL QoreValue getOutput() const override;

    // --- Builder methods (app thread, before submit) ---

    //! Adds a TCP connect step; returns the step index
    DLLLOCAL int addConnect(const char* target);

    //! Adds a TCP connect + SSL step; returns the step index
    DLLLOCAL int addConnectSsl(const char* target);

    //! Adds an SSL upgrade step (for existing TCP connection)
    DLLLOCAL int addSslUpgrade();

    //! Adds a send step; data is ref'd and stored
    DLLLOCAL int addSend(BinaryNode* data);

    //! Adds an HTTP header read step
    DLLLOCAL int addRecvHeader();

    //! Adds an HTTP body read step (auto-detects mode from context headers)
    /** Uses the status_code and headers from the preceding RECV_HEADER step
        to determine Content-Length, chunked, or connection-close mode.
        @param method the HTTP method (for HEAD no-body detection), empty = not HEAD
    */
    DLLLOCAL int addReadHttpBody(const char* method = "");

    //! Adds a delegate step that wraps an existing SocketPollOperationBase
    /** The pipeline delegates continuePoll to the wrapped operation until it
        completes, then extracts its output into the pipeline context.
        @param op the poll operation to delegate to (ref will be taken)
        @param op_obj the QoreObject wrapping the op (ref'd for lifecycle)
    */
    DLLLOCAL int addDelegate(SocketPollOperationBase* op, QoreObject* op_obj);

    //! Adds a fixed-size recv step
    DLLLOCAL int addRecvExact(size_t size, bool as_string = false);

    //! Adds a recv-until-pattern step
    DLLLOCAL int addRecvUntil(const char* pattern, bool as_string = true);

    //! Adds a recv-available-data step
    DLLLOCAL int addRecvData(bool as_string = false);

    //! Adds a conditional branch step
    DLLLOCAL int addBranch(BranchCondition condition, int true_step, int false_step);

    //! Adds a transform step (C++ function modifies context)
    DLLLOCAL int addTransform(TransformFn transform);

    //! Adds a status-code validation transform (throws if not in range)
    DLLLOCAL int addValidateStatus(int lo, int hi, const char* err_code);

    //! Adds a branch based on status code range
    DLLLOCAL int addBranchOnStatus(int lo, int hi, int true_step, int false_step);

    //! Adds a header value validation transform (throws if header doesn't match prefix)
    DLLLOCAL int addValidateHeaderPrefix(const char* header_name, const char* expected_prefix,
        const char* err_code);

    //! Adds a transform that extracts a field from last_output into last_output
    DLLLOCAL int addExtractOutputField(const char* field_name);

    //! Adds a WebSocket Sec-WebSocket-Accept key validation transform
    DLLLOCAL int addValidateWebSocketAccept(const char* ws_key, const char* err_code);

    //! Adds a result delivery step
    DLLLOCAL int addDeliverResult();

    //! Sets the result action (Promise/Channel) for DELIVER_RESULT steps
    DLLLOCAL void setResultAction(AbstractAsyncAction* action);

    //! Returns the number of steps
    DLLLOCAL int getStepCount() const { return (int)steps.size(); }

    // --- Accessors ---

    DLLLOCAL bool isClosed() const { return closed; }

    DLLLOCAL void cleanup(ExceptionSink* xsink);

protected:
    DLLLOCAL const char* getStateImpl() const override;

private:
    //! The socket (ref'd)
    QoreSocketObject* sock_obj;

    //! Step vector (configured by builder, executed by continuePoll)
    std::vector<Step> steps;

    //! Current step index
    int current_step = 0;

    //! Current inner poll operation (ref'd or nullptr)
    SocketPollOperationBase* current_op = nullptr;

    //! Pipeline context (I/O thread only)
    Context ctx;

    //! Result action (ref'd or nullptr)
    AbstractAsyncAction* result_action = nullptr;

    //! Binary data table (ref'd, for SEND steps)
    std::vector<BinaryNode*> binary_data;

    //! String data table (ref'd, for RECV_UNTIL patterns)
    std::vector<QoreStringNode*> string_data;

    //! Delegate ops table (ref'd op + ref'd QoreObject pairs, for DELEGATE steps)
    struct DelegateOp {
        SocketPollOperationBase* op;  //!< ref'd
        QoreObject* obj;              //!< ref'd (keeps the op alive)
    };
    std::vector<DelegateOp> delegate_ops;

    //! Output value (from final step or error)
    mutable QoreValue output;

    bool completed = false;
    bool closed = false;

    // --- Internal methods ---

    //! Create the inner poll operation for a step
    DLLLOCAL SocketPollOperationBase* createInnerOp(const Step& step, ExceptionSink* xsink);

    //! Extract output from current_op into context
    DLLLOCAL void extractOutput(ExceptionSink* xsink);

    //! Release current inner operation
    DLLLOCAL void releaseCurrentOp(ExceptionSink* xsink);

    //! Deliver result via action
    DLLLOCAL void deliverResult(ExceptionSink* xsink);

    //! Build result hash from context
    DLLLOCAL QoreHashNode* buildResultHash(ExceptionSink* xsink);

    //! Set error state
    DLLLOCAL void setError(const char* err, const char* desc, ExceptionSink* xsink);
};

DLLLOCAL QoreClass* initPollPipelineClass(QoreNamespace& qorens);

#endif // _QORE_CLASS_POLLPIPELINE_H
