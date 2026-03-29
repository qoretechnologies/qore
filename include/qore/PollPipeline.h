/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    PollPipeline.h

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

#ifndef _QORE_POLLPIPELINE_H

#define _QORE_POLLPIPELINE_H

#include <qore/SocketPollOperationBase.h>
#include <qore/AsyncCompletionAction.h>
#include <qore/QoreQueue.h>

#include <string>
#include <vector>
#include <functional>

//! Declarative C++ poll pipeline — chains inner poll operations as steps
/** Qore modules describe their I/O state machine by adding steps via builder
    methods. The I/O thread executes the entire pipeline natively through
    continuePoll(), only delivering the final result via AbstractAsyncAction.

    Step types map to existing C++ inner poll operations:
    - CONNECT -> SocketConnectPollOperation
    - SSL_UPGRADE -> SocketUpgradeClientSslPollOperation
    - SEND -> SocketSendPollOperation
    - RECV_HEADER -> SocketReadHttpHeaderPollOperation
    - RECV_EXACT -> SocketRecvPollOperation (N bytes)
    - RECV_UNTIL -> SocketRecvUntilBytesPollOperation (delimiter)
    - RECV_DATA -> SocketRecvDataPollOperation (available data)
    - BRANCH -> conditional jump (pure C++, no I/O)
    - TRANSFORM -> C++ function modifies context (no I/O)
    - DELIVER_RESULT -> execute AbstractAsyncAction

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
        SEND_FROM_CONTEXT,
        RECV_LINE_RESPONSE,
        BRANCH,
        TRANSFORM,
        GOTO,
        DELIVER_RESULT,
        RECV_FRAMED,       //!< Non-blocking framed receive with persistent buffer
        PUSH_QUEUE,        //!< Push ctx.last_output to an internal Queue
        DRAIN_QUEUE_SEND,  //!< Drain an internal Queue and send via socket
        YIELD              //!< Return to event loop, resume at target step
    };

    //! Pipeline execution context — accumulated across steps on I/O thread
    struct Context {
        int status_code = 0;
        QoreHashNode* headers = nullptr;    //!< ref'd, from RECV_HEADER
        BinaryNode* body = nullptr;         //!< ref'd, accumulated body data
        QoreValue last_output;              //!< output from previous step
        QoreHashNode* extra = nullptr;      //!< ref'd, arbitrary step data
        QoreStringNode* line_accum = nullptr; //!< RECV_LINE_RESPONSE accumulator (dedicated field)
        BinaryNode* frame_buf = nullptr;    //!< RECV_FRAMED persistent buffer (ref'd)
        bool has_frame = false;             //!< set by RECV_FRAMED when a complete frame is available
        bool eof = false;                   //!< set by RECV_FRAMED on EOF (empty recv)

        DLLEXPORT void cleanup(ExceptionSink* xsink) {
            if (headers) { headers->deref(xsink); headers = nullptr; }
            if (body) { body->deref(xsink); body = nullptr; }
            last_output.discard(xsink);
            last_output = QoreValue();
            if (extra) { extra->deref(xsink); extra = nullptr; }
            if (line_accum) { line_accum->deref(xsink); line_accum = nullptr; }
            if (frame_buf) { frame_buf->deref(xsink); frame_buf = nullptr; }
            has_frame = false;
            eof = false;
        }
    };

    //! Branch condition function — returns true to take true_step, false for false_step
    using BranchCondition = std::function<bool(const Context&)>;

    //! Transform function — modifies context in place
    using TransformFn = std::function<void(Context&, ExceptionSink*)>;

    //! Frame size detection function for RECV_FRAMED
    /** @param data pointer to buffer start
        @param len buffer length in bytes
        @return frame size in bytes if a complete frame is available, or 0 if incomplete
    */
    using FrameSizeFunc = std::function<size_t(const void* data, size_t len)>;

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

        // RECV_FRAMED config
        int frame_sizer_idx = -1;   //!< index into frame_sizer_table

        // PUSH_QUEUE / DRAIN_QUEUE_SEND config
        int queue_idx = -1;         //!< index into queue_table

        int next_step = -1;         //!< override next step (-1 = sequential)
    };

    //! Creates the pipeline with a socket
    DLLEXPORT PollPipelinePriv(QoreObject* self, QoreSocketObject* sock);
    DLLEXPORT virtual ~PollPipelinePriv();

    // --- SocketPollOperationBase overrides ---

    DLLEXPORT bool goalReached() const override { return completed; }
    DLLEXPORT QoreHashNode* continuePoll(ExceptionSink* xsink) override;
    DLLEXPORT void abort(ExceptionSink* xsink) override;
    DLLEXPORT QoreValue getOutput() const override;
    DLLEXPORT int getAndClearItemsPushed() override;

    // --- Builder methods (app thread, before submit) ---

    //! Adds a TCP connect step; returns the step index
    DLLEXPORT int addConnect(const char* target);

    //! Adds a TCP connect + SSL step; returns the step index
    DLLEXPORT int addConnectSsl(const char* target);

    //! Adds an SSL upgrade step (for existing TCP connection)
    DLLEXPORT int addSslUpgrade();

    //! Adds a send step; data is ref'd and stored
    DLLEXPORT int addSend(BinaryNode* data);

    //! Adds an HTTP header read step
    DLLEXPORT int addRecvHeader();

    //! Adds an HTTP body read step (auto-detects mode from context headers)
    /** Uses the status_code and headers from the preceding RECV_HEADER step
        to determine Content-Length, chunked, or connection-close mode.
        @param method the HTTP method (for HEAD no-body detection), empty = not HEAD
    */
    DLLEXPORT int addReadHttpBody(const char* method = "");

    //! Adds a delegate step that wraps an existing SocketPollOperationBase
    /** The pipeline delegates continuePoll to the wrapped operation until it
        completes, then extracts its output into the pipeline context.
        @param op the poll operation to delegate to (ref will be taken)
        @param op_obj the QoreObject wrapping the op (ref'd for lifecycle)
    */
    DLLEXPORT int addDelegate(SocketPollOperationBase* op, QoreObject* op_obj);

    //! Adds a fixed-size recv step
    DLLEXPORT int addRecvExact(size_t size, bool as_string = false);

    //! Adds a recv-until-pattern step
    DLLEXPORT int addRecvUntil(const char* pattern, bool as_string = true);

    //! Adds a recv-available-data step
    DLLEXPORT int addRecvData(bool as_string = false);

    //! Adds a conditional branch step
    DLLEXPORT int addBranch(BranchCondition condition, int true_step, int false_step);

    //! Adds a transform step (C++ function modifies context)
    DLLEXPORT int addTransform(TransformFn transform);

    //! Adds a status-code validation transform (throws if not in range)
    DLLEXPORT int addValidateStatus(int lo, int hi, const char* err_code);

    //! Adds a branch based on status code range
    DLLEXPORT int addBranchOnStatus(int lo, int hi, int true_step, int false_step);

    //! Adds a header value validation transform (throws if header doesn't match prefix)
    DLLEXPORT int addValidateHeaderPrefix(const char* header_name, const char* expected_prefix,
        const char* err_code);

    //! Adds a transform that extracts a field from last_output into last_output
    DLLEXPORT int addExtractOutputField(const char* field_name);

    //! Adds a WebSocket Sec-WebSocket-Accept key validation transform
    DLLEXPORT int addValidateWebSocketAccept(const char* ws_key, const char* err_code);

    // --- Context-reactive steps ---

    //! Sends data from a named context slot (ctx.extra[key])
    DLLEXPORT int addSendFromContext(const char* context_key);

    //! Reads lines until a non-continuation response (e.g., "250 " not "250-")
    /** Stores reply code and accumulated text in named context slots.
        Used for SMTP/POP3 multi-line responses.
    */
    DLLEXPORT int addRecvLineResponse(const char* code_key, const char* text_key, const char* err_code);

    //! Branches on whether a context extra key is truthy
    DLLEXPORT int addBranchOnContextKey(const char* key, int true_step, int false_step);

    //! Branches on whether a context extra string key equals a value
    DLLEXPORT int addBranchOnContextValue(const char* key, const char* value, int true_step, int false_step);

    //! Sets a string value in context extra
    DLLEXPORT int addSetContextValue(const char* key, const char* value);

    //! Sets binary data in context extra (for SEND_FROM_CONTEXT)
    DLLEXPORT int addSetContextBinary(const char* key, BinaryNode* data);

    //! Validates that the last output starts with the given prefix; throws if not
    /** Checks ctx.last_output (as string) for the expected prefix. If the
        output does not start with the prefix, an exception is raised with
        the given error code and the response text.

        Used for POP3 and similar protocols that use +OK/-ERR line prefixes.

        @param prefix the expected prefix (e.g., "+OK")
        @param err_code the exception error code on mismatch
    */
    DLLEXPORT int addValidateResponsePrefix(const char* prefix, const char* err_code);

    //! Checks if a context string contains a substring (case-insensitive) and stores bool result
    DLLEXPORT int addCheckContains(const char* source_key, const char* substring, const char* result_key);

    //! Sets a boolean value in context extra
    DLLEXPORT int addSetContextBool(const char* key, bool value);

    //! Selects one of two context values based on a boolean condition key
    /** If ctx.extra[condition_key] is truthy, copies ctx.extra[true_source_key] to ctx.extra[result_key];
        otherwise copies ctx.extra[false_source_key] to ctx.extra[result_key].
    */
    DLLEXPORT int addSelectContextValue(const char* result_key, const char* condition_key,
        const char* true_source_key, const char* false_source_key);

    //! Unconditional jump to a target step (enables loops)
    DLLEXPORT int addGoto(int target_step);

    //! Stores the logical OR of two boolean context keys into a result key
    /** Sets ctx.extra[result_key] = ctx.extra[key_a] || ctx.extra[key_b]
    */
    DLLEXPORT int addContextOr(const char* result_key, const char* key_a, const char* key_b);

    //! Adds a result delivery step
    DLLEXPORT int addDeliverResult();

    //! Sets the result action (Promise/Channel) for DELIVER_RESULT steps
    DLLEXPORT void setResultAction(AbstractAsyncAction* action);

    // --- New composable primitives for persistent I/O loops ---

    //! Adds a non-blocking framed receive step with persistent buffer
    /** Reads available data from the socket into a persistent buffer, then checks
        for a complete frame using the provided size function. If a complete frame
        is available, extracts it as ctx.last_output and sets ctx.has_frame = true.
        If no data is available or no complete frame yet, sets ctx.has_frame = false.
        On EOF (empty recv), sets ctx.eof = true.

        Reusable for WebSocket, HTTP/2, AMQP, or any framed protocol.

        @param sizer function that returns frame size (bytes) or 0 if incomplete
        @return the step index
        @since %Qore 2.3
    */
    DLLEXPORT int addRecvFramed(FrameSizeFunc sizer);

    //! Creates an internal Queue and returns its ID
    /** The Queue is owned by the pipeline and cleaned up automatically.
        Used with addPushQueue() and addDrainQueueSend().
        @return the queue ID (index into internal queue table)
        @since %Qore 2.3
    */
    DLLEXPORT int addQueue();

    //! Adds a step that pushes ctx.last_output to an internal Queue
    /** @param queue_id the queue ID returned by addQueue()
        @return the step index
        @since %Qore 2.3
    */
    DLLEXPORT int addPushQueue(int queue_id);

    //! Adds a step that drains an internal Queue and sends via socket
    /** Non-blocking: drains all available items, concatenates as binary,
        sends via SocketSendPollOperation. If queue is empty, completes immediately.
        @param queue_id the queue ID returned by addQueue()
        @return the step index
        @since %Qore 2.3
    */
    DLLEXPORT int addDrainQueueSend(int queue_id);

    //! Adds a yield step that returns to the event loop
    /** Ends the current continuePoll cycle, returning poll info to the controller.
        On the next continuePoll call, execution resumes at the target step.
        Enables persistent I/O loops (e.g., WebSocket bidirectional I/O).
        @param resume_step the step index to resume at on next continuePoll
        @return the step index
        @since %Qore 2.3
    */
    DLLEXPORT int addYield(int resume_step);

    // --- Queue access methods (thread-safe, for user threads) ---

    //! Gets the next item from an internal Queue (non-blocking)
    /** @param queue_id the queue ID
        @param xsink exception sink
        @return the item or QoreValue() if empty
        @since %Qore 2.3
    */
    DLLEXPORT QoreValue getQueueItem(int queue_id, ExceptionSink* xsink);

    //! Gets the next item from an internal Queue (blocking with timeout)
    /** @param queue_id the queue ID
        @param timeout_ms timeout in milliseconds (0 = wait forever)
        @param xsink exception sink
        @return the item or QoreValue() if timed out
        @since %Qore 2.3
    */
    DLLEXPORT QoreValue getQueueItemBlocking(int queue_id, int timeout_ms, ExceptionSink* xsink);

    //! Pushes an item to an internal Queue (thread-safe)
    /** @param queue_id the queue ID
        @param item the value to push (ref taken)
        @param xsink exception sink
        @since %Qore 2.3
    */
    DLLEXPORT void pushQueueItem(int queue_id, QoreValue item, ExceptionSink* xsink);

    //! Returns true if an internal Queue has items
    /** @param queue_id the queue ID
        @return true if items are available
        @since %Qore 2.3
    */
    DLLEXPORT bool queueHasItems(int queue_id) const;

    //! Returns the size of an internal Queue
    /** @param queue_id the queue ID
        @return number of items in the queue
        @since %Qore 2.3
    */
    DLLEXPORT int getQueueSize(int queue_id) const;

    //! Sets the branch targets for a BRANCH step after construction
    /** Useful when branch targets are steps that haven't been added yet.
        @param step_idx the BRANCH step index
        @param true_step the step index for the true condition
        @param false_step the step index for the false condition
        @since %Qore 2.3
    */
    DLLEXPORT void setStepTarget(int step_idx, int true_step, int false_step) {
        assert(step_idx >= 0 && step_idx < (int)steps.size());
        steps[step_idx].true_step = true_step;
        steps[step_idx].false_step = false_step;
    }

    //! Returns the number of steps
    DLLEXPORT int getStepCount() const { return (int)steps.size(); }

    //! Returns a Queue pointer from the internal queue table
    /** @param queue_id the queue ID
        @return the Queue pointer (not ref'd, valid for pipeline lifetime)
    */
    DLLEXPORT Queue* getQueuePtr(int queue_id) const {
        assert(queue_id >= 0 && queue_id < (int)queue_table.size());
        return queue_table[queue_id];
    }

    // --- Accessors ---

    DLLEXPORT bool isClosed() const { return closed; }

    DLLEXPORT void cleanup(ExceptionSink* xsink);

protected:
    DLLEXPORT const char* getStateImpl() const override;

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

    //! Frame size function table (for RECV_FRAMED steps)
    std::vector<FrameSizeFunc> frame_sizer_table;

    //! Internal queue table — owned Queues for PUSH_QUEUE/DRAIN_QUEUE_SEND
    std::vector<Queue*> queue_table;

    //! Per-continuePoll counter for items pushed to queues via PUSH_QUEUE
    int items_pushed_in_cycle = 0;

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

#endif // _QORE_POLLPIPELINE_H
