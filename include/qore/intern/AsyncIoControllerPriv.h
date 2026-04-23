/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    AsyncIoControllerPriv.h

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

#ifndef _QORE_ASYNCIOCONTROLLERPRIV_H

#define _QORE_ASYNCIOCONTROLLERPRIV_H

#include <qore/Qore.h>
#include <qore/QoreQueue.h>
#include <qore/QoreAbstractLoggerInterface.h>
#include "qore/intern/QoreLoggerBridge.h"
#include "qore/intern/QoreEventLoop.h"
#include "qore/intern/QoreEventNotifier.h"
#include "qore/intern/QoreIoUring.h"

#include "qore/intern/ThreadPool.h"
#include "qore/intern/MpscQueue.h"

#include <algorithm>
#include <atomic>
#include <deque>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Forward declarations
class QoreObject;
class QoreSocketObject;
class SocketPollOperationBase;

// AsyncIoController class id, defined in QC_AsyncIoController.cpp; declared here
// so connection TUs can use getReferencedPrivateData(CID_ASYNCIOCONTROLLER, ...)
// in non-SCU builds.
extern qore_classid_t CID_ASYNCIOCONTROLLER;

//! Lightweight async dispatcher for executing user callbacks and Qore abort() off the I/O thread
/** Manages a small pool of Qore worker threads (with full interpreter stack) that
    execute user callbacks and Qore-language abort() calls on behalf of the I/O thread
    (which must not run untrusted code or block on user callbacks).

    Workers are lazily created up to \c max_workers when work arrives.

    @since %Qore 2.3
*/
class AsyncIoControllerPriv;  // forward declaration for DT_CONTINUE_POLL

class QoreCallDispatcher {
public:
    //! Dispatch type for async work items
    enum DispatchType {
        DT_ABORT,           //!< Call abort() on spop_obj
        DT_ON_COMPLETE,     //!< Call onComplete(result) on spop_obj
        DT_CALLBACK,        //!< Call a code reference with args (timer callbacks)
        DT_CONTINUE_POLL,   //!< Call continuePoll() on spop_obj and send result back to I/O thread
        DT_STREAM_DATA_NOTIFY, //!< Call onStreamData(stream_id) on spop_obj (stream queue drain notification)
        DT_POLL_COMPLETE_NOTIFY, //!< Call onPollComplete() on spop_obj (WebSocket frame arrival notification)
    };

    //! Async work item for fire-and-forget dispatch
    struct AsyncWorkItem {
        QoreObject* spop_obj;                //!< Referenced (ownership transferred, or nullptr)
        QoreHashNode* result;                //!< For onComplete: referenced result hash (or nullptr)
        ResolvedCallReferenceNode* callback; //!< For DT_CALLBACK: referenced (or nullptr)
        QoreListNode* args;                  //!< For DT_CALLBACK: referenced (or nullptr)
        DispatchType type;                   //!< What method to call
        AsyncIoControllerPriv* controller;   //!< For DT_CONTINUE_POLL: controller (referenced)
        std::string key;                     //!< For DT_CONTINUE_POLL: operation key
        std::string stream_key;              //!< For DT_STREAM_DATA_NOTIFY: stream key
        QoreProgram* pgm = nullptr;          //!< Program reference to prevent premature deletion
        std::string owner;                   //!< Owner identifier for per-owner flush (empty if untracked)
    };

    //! Creates the dispatcher
    /** @param max_workers maximum number of Qore worker threads; 0 = auto (min(hardware_concurrency, 32))
        @param controller the owning AsyncIoControllerPriv for logging (may be nullptr)
    */
    DLLLOCAL QoreCallDispatcher(int max_workers = 0, AsyncIoControllerPriv* controller = nullptr);

    //! Destructor — does NOT stop workers; call stop() first
    DLLLOCAL ~QoreCallDispatcher();

    //! Dispatch an abort() call asynchronously (fire-and-forget)
    /** @param spop_obj the AbstractPollOperation object (referenced — ownership transferred)
        @param owner optional owner identifier for per-owner flush tracking
    */
    DLLLOCAL void dispatchAbortAsync(QoreObject* spop_obj,
        const std::string& owner = std::string());

    //! Dispatch an onComplete(result) call asynchronously (fire-and-forget)
    /** @param spop_obj the AbstractPollOperation object (referenced — ownership transferred)
        @param result the SocketPollResultInfo hash (referenced — ownership transferred)
        @param owner optional owner identifier for per-owner flush tracking
    */
    DLLLOCAL void dispatchOnCompleteAsync(QoreObject* spop_obj, QoreHashNode* result,
        const std::string& owner = std::string());

    //! Dispatch a code callback asynchronously (for timer events)
    /** @param callback the callback code (referenced — ownership transferred)
        @param args the argument list (referenced — ownership transferred, or nullptr)
    */
    DLLLOCAL void dispatchAsync(ResolvedCallReferenceNode* callback, QoreListNode* args);

    //! Dispatch continuePoll() asynchronously with result delivery back to I/O thread
    /** @param spop_obj the AbstractPollOperation object (referenced — ownership transferred)
        @param controller the AsyncIoControllerPriv to deliver the result to (referenced)
        @param key the operation key for result correlation
        @param owner optional owner identifier for per-owner flush tracking
    */
    DLLLOCAL void dispatchContinuePollAsync(QoreObject* spop_obj,
        AsyncIoControllerPriv* controller, const std::string& key,
        const std::string& owner = std::string());

    //! Dispatch onStreamData(stream_key) asynchronously (fire-and-forget)
    /** Called by the I/O thread after stream data arrives.
        The worker thread calls onStreamData() which the Qore subclass overrides
        to wake the handler thread.

        @param spop_obj the poll operation object (referenced — ownership transferred)
        @param stream_key stream identifier (H2: "stream_id", H3: "session_id:stream_id")
        @param owner optional owner identifier for per-owner flush tracking
    */
    DLLLOCAL void dispatchStreamDataAsync(QoreObject* spop_obj, const std::string& stream_key,
        const std::string& owner = std::string());

    //! Dispatch onPollComplete() asynchronously (fire-and-forget)
    /** Called by the I/O thread after WebSocketClientPollOperationBase::continuePoll()
        pushes frames to the recv_queue. The worker thread calls onPollComplete()
        which the Qore subclass overrides to schedule delivery.

        @param spop_obj the WebSocketClientPollOperationBase object (referenced — ownership transferred)
        @param owner optional owner identifier for per-owner flush tracking
    */
    DLLLOCAL void dispatchPollCompleteAsync(QoreObject* spop_obj,
        const std::string& owner = std::string());

    //! Stop all worker threads
    DLLLOCAL void stop(ExceptionSink* xsink);

    //! Wait for all pending work items to be processed
    /** Blocks until the async queue is empty AND no workers are actively
        processing items. Used during shutdown to ensure all onComplete()
        callbacks are delivered before stopping thread pools that depend
        on them.
    */
    DLLLOCAL void waitForIdle();

    //! Mark a program as shutting down so workers skip callbacks for it
    /** Called at the start of cancelByProgram(), before any I/O thread commands.
        Workers that pick up items for this program will silently discard them
        instead of calling evalMethod (which would hit PROGRAM-ERROR after ptid
        is set).
    */
    DLLLOCAL void markProgramShuttingDown(QoreProgram* pgm);

    //! Remove a program from the shutting-down set
    DLLLOCAL void clearProgramShuttingDown(QoreProgram* pgm);

    //! Wait for in-flight callbacks belonging to a specific program to complete
    /** Unlike waitForIdle() which waits for ALL callbacks, this only waits for
        callbacks whose program matches \a pgm. This avoids deadlock when the
        caller holds a lock that callbacks from OTHER programs also need.

        @param pgm the program to wait for (must already be marked as shutting down)
    */
    DLLLOCAL void waitForProgramIdle(QoreProgram* pgm);

    //! Mark an owner as shutting down so workers skip callbacks for it
    /** Workers that pick up items tagged with this owner will silently discard
        them instead of calling the user callback (which may touch now-invalid
        state on the owner object).
    */
    DLLLOCAL void markOwnerShuttingDown(const std::string& owner);

    //! Remove an owner from the shutting-down set
    DLLLOCAL void clearOwnerShuttingDown(const std::string& owner);

    //! Wait for in-flight callbacks belonging to a specific owner to complete
    /** Unlike waitForIdle() which waits for ALL callbacks, this only waits for
        callbacks tagged with \a owner.  This avoids deadlock when the caller
        holds a lock that callbacks from OTHER owners also need.

        @param owner the owner identifier to wait for
    */
    DLLLOCAL void waitForOwnerIdle(const std::string& owner);

    //! Maximum worker cap — workers exit when idle, so a large cap is safe; this just
    //! prevents runaway thread creation under extreme load
    static constexpr int DEFAULT_WORKER_CAP = 4096;

    //! Idle timeout before a worker thread exits (milliseconds)
    static constexpr int WORKER_IDLE_TIMEOUT_MS = 60000;

private:
    QoreThreadLock m;
    QoreCondition work_avail;               //!< Signaled when work is available
    QoreCondition workers_done;             //!< Signaled when all workers have exited
    QoreCondition idle_cond;                //!< Signaled when queue drains and processing completes
    std::deque<AsyncWorkItem> async_queue;  //!< Pending async work items
    int active_workers = 0;                 //!< Number of running worker threads
    int active_processing = 0;             //!< Number of workers currently processing items
    int max_workers;                        //!< Maximum workers
    bool stopping = false;                  //!< Set during shutdown
    AsyncIoControllerPriv* ctrl = nullptr;  //!< Owning controller for logging (not ref'd — controller outlives dispatcher)
    std::unordered_set<QoreProgram*> shutting_down_programs;  //!< Programs being destroyed — skip callbacks
    std::unordered_map<QoreProgram*, int> active_per_program; //!< Per-program in-flight callback count
    QoreCondition pgm_idle_cond;                //!< Signaled when a program's active count reaches zero
    std::unordered_set<std::string> shutting_down_owners;      //!< Owners being torn down — skip callbacks
    std::unordered_map<std::string, int> active_per_owner;     //!< Per-owner in-flight callback count
    QoreCondition owner_idle_cond;              //!< Signaled when an owner's active count reaches zero

    //! Enqueue a work item, starting a worker if needed
    DLLLOCAL void enqueue(AsyncWorkItem&& item);

    //! Worker thread entry point
    DLLLOCAL static void workerEntry(ExceptionSink* xsink, void* arg);

    //! Worker thread main loop
    DLLLOCAL void workerLoop(ExceptionSink* xsink);
};

// hashdecl pointers
DLLEXPORT extern const TypedHashDecl* hashdeclSocketPollOperationInfo;
DLLEXPORT extern const TypedHashDecl* hashdeclSocketPollResultInfo;

//! C++ async I/O controller - direct translation of AsyncSocketIoController.qc
/** This class implements the core async I/O event loop used by HTTP/2 server,
    HTTP/2 client multiplexing, WebSocket, and SSE.

    Thread safety: all public methods are thread-safe. Internal state is protected
    by a single mutex with careful lock ordering to avoid deadlocks.

    @since %Qore 2.3
*/
class AsyncIoControllerPriv : public AbstractPrivateData {
    friend class QoreCallDispatcher;  // for enqueueContinuePollResult from worker thread

public:
    //! Default I/O operation timeout (30 seconds)
    static constexpr int64 DEFAULT_IO_TIMEOUT_US = 30000000LL;

    //! Autostop grace period (2 seconds)
    /** When the cache empties and autostop is enabled, the I/O thread waits
        this long before exiting.  This avoids unnecessary stop/restart cycles
        when operations are submitted in rapid succession (e.g., between test
        cases or when creating multiple HTTP servers).  Without the grace
        period, the I/O thread exits immediately and the next submit() must
        wait for the old thread to finish cleanup before starting a new one,
        adding latency that can cause QUIC handshake timeouts on
        resource-constrained systems.

        @since %Qore 2.3
    */
    static constexpr int64 AUTOSTOP_GRACE_US = 2000000LL;

    //! Creates the controller
    /** @param autostop if true, the I/O thread will stop when all operations complete
        @param xsink for exception handling
    */
    DLLLOCAL AsyncIoControllerPriv(bool autostop, ExceptionSink* xsink);

    //! Destructor
    DLLLOCAL virtual ~AsyncIoControllerPriv();

    //! Submit an async operation
    /** @param self the QoreObject wrapping this controller
        @param info the SocketPollOperationInfo hash
        @param replace if true, cancel any existing operation with the same key
        @param xsink for exception handling
        @return a Queue object for receiving the result (or nullptr if callback was provided)
    */
    DLLLOCAL QoreObject* submit(QoreObject* self, QoreHashNode* info, bool replace, ExceptionSink* xsink);

    //! Execute an async operation synchronously
    /** @param self the QoreObject wrapping this controller
        @param info the SocketPollOperationInfo hash
        @param replace if true, cancel any existing operation with the same key
    //! Cancel an operation by socket
    /** @param sock the socket to cancel
        @param xsink for exception handling
        @return true if an operation was found and canceled
    */
    DLLLOCAL bool cancel(AbstractPollableIoObjectBase* sock, ExceptionSink* xsink);

    //! Cancel an operation by key
    /** @param key the operation key
        @param xsink for exception handling
        @return true if an operation was found and canceled
    */
    DLLLOCAL bool cancelByKey(const QoreStringNode* key, ExceptionSink* xsink);

    //! Cancel all operations for an owner
    /** @param owner the owner identifier
        @param xsink for exception handling
        @return the number of operations canceled
    */
    DLLLOCAL int cancelByOwner(const QoreStringNode* owner, ExceptionSink* xsink);


    //! Wake the I/O thread for a specific socket that has pending data
    /** @param sock_hash the socket hash identifying the target operation
    */
    DLLLOCAL void wakeSocket(const std::string& sock_hash);

    //! Wakes a socket operation by QoreObject identity
    /** Looks up the socket's registered hash from registered_sockets to find the
        correct cache entry. This avoids the hash mismatch that occurs when the
        AbstractPollableIoObjectBase pointer changes during QUIC handshake.
        @param sock_obj the socket QoreObject
        @param xsink exception sink
    */
    DLLLOCAL void wakeSocketByObject(QoreObject* sock_obj, ExceptionSink* xsink);

    //! Wait for all pending onComplete/abort callbacks to be processed
    /** Call after cancelByOwner() to ensure all cancelled operation callbacks
        have been delivered before stopping thread pools that depend on them.
    */
    DLLLOCAL void flushCallbacks();

    //! Wait for in-flight callbacks belonging to a specific owner to drain
    /** Narrowed-scope companion to @ref flushCallbacks() that only waits for
        callbacks whose operations were submitted with the given \a owner.
        Suitable for per-owner teardown paths (e.g. a connection manager's
        destructor) where the global flush would deadlock on unrelated
        long-running work on the shared controller.

        Marks the owner as shutting down for the duration of the wait: any
        callback picked up by a worker while the mark is set is silently
        discarded instead of dispatched to user code.  The mark is cleared
        before return, so subsequent unrelated submits by a new object using
        the same owner string are unaffected.
    */
    DLLLOCAL void flushCallbacksByOwner(const std::string& owner);

    //! Start the I/O thread
    /** @param xsink for exception handling
    */
    DLLLOCAL void start(ExceptionSink* xsink);

    //! Stop the I/O thread gracefully
    /** @param xsink for exception handling
    */
    DLLLOCAL void stop(ExceptionSink* xsink);

    //! Stop and clear all operations
    /** @param xsink for exception handling
    */
    DLLLOCAL void stopClear(ExceptionSink* xsink);

    //! Wait for the I/O thread to stop
    DLLLOCAL bool waitStop(ExceptionSink* xsink);

    //! Wait for the I/O thread to be ready
    /** @param timeout_ms timeout in ms; 0 = wait forever
        @param xsink for exception handling
        @return true if the I/O thread is ready
    */
    DLLLOCAL bool waitReady(int timeout_ms, ExceptionSink* xsink);

    //! Wait until the I/O thread has processed all currently submitted operations
    /** @param timeout_ms timeout in ms; 0 = wait forever
        @param xsink for exception handling
        @return true if all submitted operations have been processed
    */
    DLLLOCAL bool waitForProcessing(int timeout_ms, ExceptionSink* xsink);

    //! Returns true if the I/O thread is running
    DLLLOCAL bool running() const;

    //! Returns the number of cached operations
    DLLLOCAL int getCacheSize() const;

    //! Sets the autostop flag
    DLLLOCAL void setAutostop(bool autostop);

    //! Returns the autostop flag
    DLLLOCAL bool getAutostop() const;

    //! Returns info about the controller state
    DLLLOCAL QoreHashNode* getInfo(ExceptionSink* xsink);

    //! Sets the logger
    /** @param logger_obj the LoggerInterface Qore object (or nullptr to clear)
        @param xsink for exception handling
    */
    DLLLOCAL void setLogger(QoreObject* logger_obj, ExceptionSink* xsink);

    //! Drops timer_callback and timer user-data values (may hold cross-module
    //! QoreObject refs). Safe to call while the controller is still running —
    //! timer firings after this simply see no callback/data.
    DLLLOCAL void clearCrossModuleRefs(ExceptionSink* xsink);

    //! Adds a timer to fire at the given deadline
    /** @param deadline the absolute deadline
        @param udata Qore user data to associate with the timer (referenced)
        @param xsink for exception handling
        @return the timer ID (> 0)
    */
    DLLLOCAL int64_t addTimer(const DateTimeNode* deadline, QoreValue udata, ExceptionSink* xsink);

    //! Cancels a timer
    /** @param id the timer ID returned by addTimer()
        @param xsink for exception handling
        @return true if the timer was found and canceled
    */
    DLLLOCAL bool cancelTimer(int64_t id, ExceptionSink* xsink);

    //! Sets the timer callback
    /** @param cb the callback to call when a timer fires (or nullptr to clear)
        @param xsink for exception handling
    */
    DLLLOCAL void setTimerCallback(ResolvedCallReferenceNode* cb, ExceptionSink* xsink);

    //! Sets the maximum number of callback dispatcher worker threads
    /** Controls the concurrency of user callback and abort() dispatch.
        Takes effect on the next call_dispatcher creation (after stop/restart or first use).
        @param max_workers the maximum number of workers; 0 = auto (min(hardware_concurrency, 32))
    */
    DLLLOCAL void setMaxCallbackWorkers(int max_workers);

    //! Set the number of I/O threads (must be called before first submit)
    DLLLOCAL void setMaxIoThreads(int num_threads, ExceptionSink* xsink);

    //! Submit a task to the controller's thread pool
    /** The thread pool is lazily created on first use. ThreadPool threads use
        QTF_EXTERNAL_LIFECYCLE so they don't block QoreProgramHelper shutdown;
        the pool is stopped during controller stop/destruction.

        @param task the task code (referenced by caller)
        @param cancel optional cancel code (referenced by caller, or nullptr)
        @param xsink for exception handling
        @return 0 on success, -1 on error
    */
    DLLLOCAL int submitTask(ResolvedCallReferenceNode* task, ResolvedCallReferenceNode* cancel,
        ExceptionSink* xsink);

    //! Cancels all operations whose callbacks belong to the given QoreProgram
    /** Called from the program cleanup callback before namespace data is freed,
        so callbacks can safely execute with valid type info.
        @param pgm the QoreProgram being destroyed
        @param xsink for exception handling
    */
    DLLLOCAL void cancelByProgram(QoreProgram* pgm, ExceptionSink* xsink);

    //! Custom deref with ExceptionSink
    DLLLOCAL virtual void deref(ExceptionSink* xsink);

private:
    //! I/O thread commands
    enum class IoCommand {
        SubmitOp,            //!< Submit a new operation (carries PollInfo data)
        Cancel,
        CancelOwner,
        CancelByProgram,     //!< Cancel all operations belonging to a QoreProgram
        Quit,
        WakeSocket,          //!< Targeted wake: re-poll a specific socket's operation
        AddTimer,
        CancelTimer,
        ContinuePollResult,  //!< Result from async continuePoll() dispatch
        GetInfo,             //!< Snapshot cache info on the I/O thread
    };

    //! Forward declaration — full definition below (requires PollInfo)
    struct AsyncOpCompletion;

    //! Refcounted holder for the Cancel command's match count.
    /** Replaces the former raw pointer to a stack-local
        std::atomic<int> found_count in cancelByKey().  The stack-local
        form had a narrow UAF window: two concurrent cancelByKey() calls
        for the same key share one CancelCond via cancel_cond_map (the
        second caller reuses the first's entry).  When the I/O thread
        processes the first Cancel command, signalCancelLocked()
        broadcasts the shared cond, waking both waiters.  The second
        waiter's waitCancel() returns — its stack frame (and its
        found_count) can then be freed.  If a SubmitOp for the same
        key is interleaved between the two Cancel commands in the I/O
        thread's cmdq (e.g. cmdq=[cmd_X, SubmitOp, cmd_Z]), cmd_Z's
        processing finds the re-added cache entry, sets found=true,
        and does fetch_add() on the dead stack pointer.  Heap-
        allocating the counter isolates it from caller-stack
        lifetime; refcounting coordinates cmd-side and caller-side
        releases. */
    struct CancelCountRef : public QoreReferenceCounter {
        //! Number of matches; incremented by the I/O thread under `found`.
        std::atomic<int> count{0};

        //! Atomic deref; deletes on last ref.
        DLLLOCAL void deref() {
            if (ROdereference()) {
                delete this;
            }
        }

        //! Public alias for ROreference()
        using QoreReferenceCounter::ROreference;

    protected:
        DLLLOCAL ~CancelCountRef() = default;
    };

    //! Command queue entry
    struct Command {
        IoCommand cmd = IoCommand::WakeSocket;
        std::string key;                //!< For Cancel / ContinuePollResult / SubmitOp: the cache key
        std::string owner;              //!< For CancelOwner / SubmitOp: the owner string
        //! For CancelOwner/CancelByProgram/GetInfo: shared completion object.
        /** When non-null, owns one ref on the referenced AsyncOpCompletion.
            The ref is released when the command is processed (normal path
            in processCmd) or cleaned up (abandoned path in
            cleanupAbandonedCommand / I/O-thread-exit drain).  Replaces
            the former raw-pointer group (done_cond / cancel_done_flag /
            pending_threads / cancel_count / info_result / cancel_pinfos)
            — see AsyncOpCompletion docs. */
        AsyncOpCompletion* completion = nullptr;
        //! For Cancel: refcounted holder receiving the match result (0 or 1).
        /** Heap-allocated and refcounted so it outlives any caller
            stack frame that may go out of scope before this command is
            processed — see CancelCountRef docs for the UAF this
            avoids.  When non-null, this Command owns one ref; that
            ref is released by the I/O thread after processing
            (normal path) or by cleanupAbandonedCommand / I/O-thread-
            exit drain (abandoned path). */
        CancelCountRef* cancel_count = nullptr;
        QoreProgram* pgm = nullptr;       //!< For CancelByProgram: the program being destroyed
        int64_t timer_deadline_us = 0;  //!< For AddTimer: absolute deadline in microseconds
        int64_t timer_id = 0;           //!< For AddTimer/CancelTimer: timer ID
        QoreHashNode* continue_poll_result = nullptr;  //!< For ContinuePollResult: new poll info (or nullptr)
        QoreHashNode* continue_poll_ex = nullptr;      //!< For ContinuePollResult: exception (or nullptr)
        bool continue_poll_completed = false;           //!< For ContinuePollResult: true if completed
        std::string sock_hash;          //!< For WakeSocket: socket hash to re-poll
        bool submit_replace = false;    //!< For SubmitOp: replace existing operation with same key

        // --- SubmitOp data (ownership transferred to I/O thread) ---
        QoreObject* submit_sock_obj = nullptr;           //!< Referenced socket object
        AbstractPollableIoObjectBase* submit_sock = nullptr; //!< Referenced socket private data
        QoreObject* submit_spop_obj = nullptr;           //!< Referenced poll operation object
        SocketPollOperationBase* submit_spop_base = nullptr; //!< Referenced C++ poll operation base
        QoreHashNode* submit_poll_info = nullptr;        //!< Referenced initial poll info (or nullptr)
        QoreHashNode* submit_other = nullptr;            //!< Referenced other data (or nullptr)
        Queue* submit_queue = nullptr;                   //!< Referenced result queue (or nullptr)
        int64 submit_timeout_us = 0;                     //!< Timeout in microseconds
        bool submit_has_qore_abort = false;              //!< True if abort() is overridden in Qore
        bool submit_has_qore_on_complete = false;        //!< True if onComplete() is overridden in Qore
    };

    //! Internal poll info (mirrors Qore Priv::PollInfo)
    struct PollInfo {
        int64 timeout_date_us;          //!< Absolute timeout (microseconds since epoch), 0 = not set
        QoreObject* sock_obj;           //!< Pollable I/O QoreObject (referenced)
        AbstractPollableIoObjectBase* sock; //!< Pollable I/O private data
        QoreObject* spop_obj;           //!< AbstractPollOperation QoreObject (referenced)
        QoreHashNode* poll_info;        //!< Last SocketPollInfo hash (referenced, or nullptr)
        int64 timeout_us;               //!< Timeout in microseconds (negative = no timeout)
        std::string owner;              //!< Owner identifier
        QoreHashNode* other;            //!< Free-form data (referenced, or nullptr)
        Queue* queue;                   //!< Result queue (referenced, or nullptr)
        SocketPollOperationBase* spop_base; //!< C++ poll operation (referenced, or nullptr)
        bool has_qore_abort;            //!< True if abort() is overridden in Qore
        bool has_qore_on_complete;      //!< True if onComplete() is overridden in Qore
        bool continue_poll_in_flight;   //!< True when continuePoll() dispatched to worker
        int64 poll_timeout_deadline_us; //!< Absolute deadline for protocol-level poll timeout (QUIC)
        std::string cached_sock_hash;   //!< Cached socket hash for O(1) Phase 1 readiness check
        int cached_events = 0;          //!< Cached poll events for Phase 3 fast path
        uint32_t cached_fd_gen = 0;     //!< Cached fd generation for QUIC migration detection
        uint64_t last_queued_gen = 0;   //!< Phase 1 generation when last queued (duplicate prevention)
        //! Back-ref to the owning controller (not ref'd; outlives pinfo).
        //! Used by cleanup() to erase the submit-time obj_to_sock_hash entry
        //! before deref'ing sock_obj — without this the map can accumulate
        //! dangling pointer keys when an op completes before Phase 3's
        //! updateEventLoopRegistration would normally tie map lifetime to
        //! socket_refcounts.
        class AsyncIoControllerPriv* controller = nullptr;

        DLLLOCAL PollInfo() : timeout_date_us(0), sock_obj(nullptr), sock(nullptr),
            spop_obj(nullptr), poll_info(nullptr), timeout_us(DEFAULT_IO_TIMEOUT_US),
            other(nullptr), queue(nullptr),
            spop_base(nullptr), has_qore_abort(false), has_qore_on_complete(false),
            continue_poll_in_flight(false), poll_timeout_deadline_us(0) {
        }

        DLLLOCAL ~PollInfo() {
            // NOTE: caller must have already dereferenced all objects via cleanup()
        }

        //! Clean up all references
        DLLLOCAL void cleanup(ExceptionSink* xsink);
    };

    //! Shared completion state for cross-thread async I/O commands.
    /** Used by CancelOwner / CancelByProgram / GetInfo where a caller
        submits one command per I/O thread and waits for all of them to
        complete.

        Before this class existed, the caller put a stack-local
        QoreCondition + bool flag + std::atomic<int> counter into the
        Command and passed them by raw pointer.  That had a
        use-after-return window: if the caller's wait returned (flag set
        + broadcast by the last I/O thread under the controller mutex)
        and the caller's stack frame went out of scope before every
        other cleanup path (Quit drain, abandoned-cmd drain) finished
        touching the same cond/flag/counter, that cleanup path wrote to
        stale stack memory.  On arm64 this manifested as a single-bit
        flip of a stack-saved return address (`a_inc(&cond->_c_seq)` on
        a stale pointer whose offset 8 landed on a saved `x30` slot) and
        a later SIGBUS in an unrelated function's epilogue.

        Lifetime:
        - Caller allocates with `new AsyncOpCompletion(N)`, which sets
          `pending_threads = N` and starts with refcount = 1 (the
          caller's own ref).
        - For each of the N commands dispatched, the caller calls
          `ROreference()` to add one ref before pushing the command.
        - When an I/O thread finishes processing its command (or drains
          it in cleanup), it calls `completeOne()` to decrement
          `pending_threads` (signaling the waiter if it was the last),
          then `deref()` to drop its own ref.
        - When the caller's wait returns, it consumes any outputs
          (`info_result`, `cancel_pinfos`), then calls `deref()`.
        - The object is destroyed when the last holder releases.

        Skip-broadcast optimization: if an I/O thread sees
        `is_unique()` in `completeOne()`, it holds the only ref — the
        caller has already released and no one is waiting, so the
        broadcast is skipped. */
    struct AsyncOpCompletion : public QoreReferenceCounter {
        //! Signaled when `done` transitions to true (the last completeOne)
        QoreCondition cond;
        //! Set to true under the controller mutex by the last completeOne
        bool done = false;
        //! Number of I/O threads still expected to call completeOne
        std::atomic<int> pending_threads{0};
        //! Accumulated count (CancelOwner: operations cancelled)
        std::atomic<int> cancel_count{0};
        //! Result hash (GetInfo); ref'd by the builder, consumed by caller
        QoreHashNode* info_result = nullptr;
        //! Accumulated PollInfos (CancelByProgram), guarded by pinfos_lock
        std::vector<PollInfo> cancel_pinfos;
        //! Protects cancel_pinfos against concurrent appends from I/O threads
        QoreThreadLock pinfos_lock;

        //! Build a completion for a fan-out of @a n_pending I/O threads.
        /** Starts with refcount = 1 (the caller's initial ref).  For
            each command pushed, the caller must call ROreference(). */
        DLLLOCAL explicit AsyncOpCompletion(int n_pending) {
            pending_threads.store(n_pending, std::memory_order_relaxed);
        }

        //! Called by an I/O thread when it finishes processing its cmd.
        /** Decrements pending_threads and signals the waiter if this was
            the last.  Caller must hold the controller mutex m.  If this
            object is held uniquely (refcount == 1), the caller has
            already released and no broadcast is needed.
            @return true if this was the last decrement */
        DLLLOCAL bool completeOne() {
            if (is_unique()) {
                pending_threads.fetch_sub(1, std::memory_order_relaxed);
                return true;
            }
            if (pending_threads.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                done = true;
                cond.broadcast();
                return true;
            }
            return false;
        }

        //! Wait until done == true.  Caller must hold mutex m.
        DLLLOCAL void waitForCompletion(QoreThreadLock& lck) {
            while (!done) {
                cond.wait(lck);
            }
        }

        //! Atomic deref; deletes on last ref.
        DLLLOCAL void deref() {
            if (ROdereference()) {
                delete this;
            }
        }

        //! Public alias for ROreference()
        using QoreReferenceCounter::ROreference;

    protected:
        DLLLOCAL ~AsyncOpCompletion() {
            // Caller must have consumed info_result before last deref.
            assert(!info_result);
        }
    };

    //! Deferred delivery info
    struct DeferredDelivery {
        std::string key;
        Queue* queue;                      //!< Referenced or nullptr
        QoreObject* spop_obj;              //!< Referenced or nullptr (for onComplete dispatch)
        bool has_on_complete;              //!< True if spop has onComplete() override
        QoreHashNode* result;              //!< Referenced
        std::string owner;                 //!< Owner identifier for per-owner flush tracking
    };

    //! Timer info stored under lock
    struct TimerInfo {
        QoreValue udata;                //!< Referenced Qore user data
    };

    //! Per-I/O-thread context — each thread owns its own event loop, cache, and socket state
    /** Multiple IoThreadContext instances enable nginx-style per-thread isolation with
        zero lock contention during normal I/O processing.
        @since %Qore 2.3
    */
    struct IoThreadContext {
        MpscQueue<Command> cmdq;          //!< Lock-free command queue for this thread
        std::atomic<bool> running{false};  //!< True when this thread accepts commands
        int tid = 0;                       //!< Thread ID (0 if not running)
        int thread_idx = 0;                //!< Index in io_threads vector
        int64 autostop_idle_since = 0;     //!< Timestamp when cache first became empty

        QoreEventLoop* loop = nullptr;
        QoreEventNotifier* notifier = nullptr;


        //! Operation cache — fully owned by this I/O thread
        std::unordered_map<std::string, PollInfo> cache;
        std::atomic<int> cache_size{0};   //!< Atomic cache size for lock-free getCacheSize()
        std::unordered_map<std::string, int> socket_refcounts;

        //! Socket hashes that need re-polling
        std::unordered_set<std::string> wake_socket_hashes;

        //! Keys needing first continuePoll (empty cached_sock_hash)
        std::vector<std::string> new_entry_keys;

        //! Timeout min-heap entry
        struct TimeoutEntry {
            int64 deadline_us;
            std::string key;
            bool operator>(const TimeoutEntry& o) const { return deadline_us > o.deadline_us; }
        };
        //! Min-heap for operation/protocol timeouts (lazy deletion)
        std::priority_queue<TimeoutEntry, std::vector<TimeoutEntry>,
            std::greater<TimeoutEntry>> timeout_heap;

        //! Generation counter for Phase 1 duplicate prevention
        uint64_t phase1_gen = 0;

        // Registered socket tracking
        std::unordered_map<std::string, QoreObject*> registered_sockets;
        std::unordered_map<std::string, int> registered_events;
        std::unordered_map<std::string, int> registered_fds;
        std::unordered_map<std::string, int> key_events;
        std::unordered_map<std::string, std::unordered_set<std::string>> sock_hash_to_keys;
        std::unordered_map<int, std::string> fd_to_sock_hash;
        std::unordered_map<std::string, std::unordered_set<int>> key_extra_fds;

        //! Recently-cancelled operation keys — prevents re-submission of stale ops
        /** I/O-thread-only. Value is a TTL counter decremented each processCommands()
            cycle; entry is erased when it reaches zero.
            @since %Qore 2.3
        */
        std::unordered_map<std::string, int> cancelled_keys;

        //! Recently-cancelled owners on this I/O thread — set by CancelOwner
        //! regardless of whether any cache entries were found.  Fixes the race
        //! where cancelByOwner() runs before submitConnectionOp() actually
        //! submits the operation: when SubmitOp later arrives, it detects the
        //! cancelled owner and dispatches onComplete(canceled=true) immediately
        //! instead of inserting into the cache (which would never be cancelled).
        //! Entries auto-expire after 2 idle cycles.
        //! Key: owner string, Value: idle_cycles_remaining
        std::unordered_map<std::string, int> cancelled_owners;

        //! Cancels deferred because continuePoll() was in flight on a worker
        /** When Cancel arrives for an op whose continuePoll() has been
            dispatched to a worker but its ContinuePollResult has not yet
            returned, the cache entry is removed immediately (to block new
            dispatches) but the abort (@c callAbort → socket close → SSL
            shutdown) is stashed here and performed after the
            @c ContinuePollResult arrives.  This guarantees no SSL operation
            on a worker thread is concurrent with @c SSL_shutdown in
            @c close_internal(), avoiding the OpenSSL context corruption
            seen in CI job 170659 (SIGSEGV in @c EVP_CIPHER_get_mode).

            I/O-thread-only.  Key: op key.  Value: the PollInfo copy whose
            cleanup is still pending.
         */
        std::unordered_map<std::string, PollInfo> pending_aborts;
    };

    //! Get the I/O thread index for a given operation key (hash-based affinity)
    DLLLOCAL int getThreadIndex(const std::string& key) const {
        if (io_threads.size() <= 1) {
            return 0;
        }
        return std::hash<std::string>{}(key) % io_threads.size();
    }

    //! Get the I/O thread context for a given operation key
    DLLLOCAL IoThreadContext& getThreadForKey(const std::string& key) {
        return *io_threads[getThreadIndex(key)];
    }

    // --- I/O thread contexts ---
    std::vector<std::unique_ptr<IoThreadContext>> io_threads;  //!< One per I/O thread (default: 1)
    int num_io_threads;                       //!< Configured thread count

    //! Maps socket hash → thread index for wakeSocket routing
    /** Updated by I/O threads when sockets are registered/unregistered with the event loop.
        Protected by a lightweight spinlock (separate from main mutex m) since it's
        only accessed briefly for insert/erase/lookup.
    */
    mutable QoreThreadLock sock_route_lock;
    std::unordered_map<std::string, int> sock_to_thread;  //!< sock_hash → thread index
    std::unordered_map<QoreObject*, std::string> obj_to_sock_hash;  //!< QoreObject* → sock_hash for wakeSocketByObject

    // Backward-compatible accessors — returns thread 0 context.
    // Safe for startup/shutdown checks; callers doing cache access must use
    // getThreadForKey() instead when N>1 I/O threads are active.
    DLLLOCAL IoThreadContext& ctx() { return *io_threads[0]; }
    DLLLOCAL const IoThreadContext& ctx() const { return *io_threads[0]; }

    //! Returns true if any I/O thread is running (caller must hold lock)
    DLLLOCAL bool anyThreadRunning() const {
        for (auto& tp : io_threads) {
            if (tp->tid) {
                return true;
            }
        }
        return false;
    }

    //! Per-key cancel cond with refcount.  The cond is deleted only when the
    //! LAST holder (map entry + any active waiters) drops its ref — destroying
    //! it earlier is UB on macOS if a waiter is still inside pthread_cond_wait
    //! (between "woken by broadcast" and "returned to caller"), which deadlocks
    //! the waiter.
    struct CancelCond {
        QoreCondition cond;
        int refs = 0;  //!< protected by AsyncIoControllerPriv::m
    };

    //! Remove cancel_cond_map[key] (if present), broadcast its cond, and drop
    //! the map's ref.  Caller must hold m.
    DLLLOCAL void signalCancelLocked(const std::string& key) {
        auto it = cancel_cond_map.find(key);
        if (it == cancel_cond_map.end()) {
            return;
        }
        CancelCond* cc = it->second;
        cancel_cond_map.erase(it);
        cc->cond.broadcast();
        if (--cc->refs == 0) {
            delete cc;
        }
    }

    // --- Mutex-protected state (rare paths: startup, shutdown, cancel wait) ---
    mutable QoreThreadLock m;
    bool autostop_flag;
    bool shutting_down;
    std::unordered_map<std::string, CancelCond*> cancel_cond_map;
    QoreCondition io_cond;
    bool io_waiting;
    bool io_exiting;
    bool ready_flag;
    std::atomic<int> submit_seq;          //!< Incremented on each submit() call
    int processed_seq;                    //!< Updated by I/O thread after Phase 1 snapshot
    QoreCondition processed_cond;         //!< Signaled when processed_seq advances
    QoreLoggerBridge* logger;              //!< Referenced or nullptr
    std::unordered_map<int64_t, TimerInfo> timer_info_map; //!< Timer ID -> user data
    ResolvedCallReferenceNode* timer_callback; //!< Timer callback (referenced, or nullptr)

    //! Atomic counter for pre-allocating timer IDs across threads
    std::atomic<int64_t> next_ctrl_timer_id{1};

    // --- Internal methods ---

    //! Ensure call_dispatcher exists (lock-free fast path)
    DLLLOCAL void ensureCallDispatcher();

    //! Start the I/O thread (caller must hold lock)
    DLLLOCAL void startIntern(ExceptionSink* xsink);

    //! The I/O thread main function (arg points to IoThreadStartInfo)
    DLLLOCAL static void ioThreadEntry(ExceptionSink* xsink, void* arg);

    //! The I/O thread main loop
    DLLLOCAL void ioThread(IoThreadContext& t, ExceptionSink* xsink);

    //! Process commands from the command queue
    /** @return true if the I/O thread should exit
    */
    DLLLOCAL bool processCommands(IoThreadContext& t, ExceptionSink* xsink);

    //! Releases all refcounted resources on a command that will not be
    //! processed — used to avoid leaks when the I/O thread is shutting
    //! down and commands were drained from cmdq but no further processing
    //! will occur.
    /** Handles SubmitOp (derefs sock, spop, priv ref, poll_info, other,
        queue), ContinuePollResult (derefs result/ex hashes), and signals
        any condition-variable waiters (CancelOwner/CancelByProgram/GetInfo/
        Cancel) so callers don't hang.

        @note Caller must hold @ref m — the helper touches
        @ref cancel_cond_map (shared state) in the @ref IoCommand::Cancel
        case.
    */
    DLLLOCAL void cleanupAbandonedCommand(Command& cmd, ExceptionSink* xsink);

    //! Cancel an operation internally (delivers result, called from I/O thread)
    DLLLOCAL void doCancelIntern(PollInfo& pinfo, ExceptionSink* xsink);

    //! Update EventLoop registration for an operation
    DLLLOCAL void updateEventLoopRegistration(IoThreadContext& t, const std::string& key,
        QoreObject* socket, const std::string& sock_hash, int events, ExceptionSink* xsink);

    //! Unregister an operation from the EventLoop
    DLLLOCAL void unregisterFromEventLoop(IoThreadContext& t, const std::string& key,
        ExceptionSink* xsink);

    //! Update extra fd registrations for an operation
    /** @since %Qore 2.3
    */
    DLLLOCAL void updateExtraFds(IoThreadContext& t, const std::string& key, QoreObject* socket,
        QoreHashNode* poll_info, ExceptionSink* xsink);

    //! Unregister extra fds for an operation
    /** @since %Qore 2.3
    */
    DLLLOCAL void unregisterExtraFds(IoThreadContext& t, const std::string& key, ExceptionSink* xsink);

    //! Release an old fd from event loop tracking, if still owned by expected_hash
    /** Safely removes \c old_fd from the kqueue/epoll registration and erases
        \c fd_to_sock_hash[old_fd], but only if the current owner of that fd is
        still \c expected_hash.

        This guards against the fd-recycling race: when a socket is closed and
        its fd is later reused by a new socket, a late cleanup of the original
        socket must not clobber the new owner's tracking or deregister its
        kqueue filter.  If \c fd_to_sock_hash[old_fd] has moved to a different
        hash, leave everything untouched.

        @param t the I/O thread context
        @param old_fd the fd to release
        @param expected_hash the sock_hash that should currently own \c old_fd
        @param xsink exception sink
        @since %Qore 2.3
    */
    DLLLOCAL void releaseFdIfOwner(IoThreadContext& t, int old_fd,
        const std::string& expected_hash, ExceptionSink* xsink);

    //! Compute the event union for a socket
    DLLLOCAL int computeEventUnion(const IoThreadContext& t, const std::string& sock_hash) const;

    //! Apply the event union for a socket
    DLLLOCAL void applyEventUnion(IoThreadContext& t, QoreObject* socket,
        const std::string& sock_hash, ExceptionSink* xsink);

    //! Enqueue a command (caller must hold lock)
    /** @return true if the notifier should be signaled
    */
    DLLLOCAL bool enqueueCmdLocked(IoCommand cmd, const std::string& key = std::string(),
        const std::string& owner = std::string());

    //! Wait for a cancel to complete
    DLLLOCAL void waitCancel(const std::string& key);

    //! Log a message (acquires lock to snapshot logger — must NOT be called with lock held)
    DLLLOCAL void log(int level, const char* fmt, ...) const;

    //! Get the unique hash from a pollable I/O object
    DLLLOCAL static std::string getSocketHash(AbstractPollableIoObjectBase* sock);

    //! Get socket and poll info from a SocketPollInfo hash
    DLLLOCAL static QoreObject* getSocketFromPollInfo(QoreHashNode* poll_info, std::string& sock_hash,
        int& events, ExceptionSink* xsink);

    //! Call continuePoll on an AbstractPollOperation object
    //! Enqueue a continuePoll result from a worker thread back to the I/O thread
    DLLLOCAL void enqueueContinuePollResult(const std::string& key, QoreHashNode* new_poll_info,
        QoreHashNode* ex_hash, bool completed);

    //! Dispatch a stream-data-ready notification from a worker thread
    /** Called by the DT_CONTINUE_POLL worker after getAndClearDataReadyStreams() to
        notify a Qore poll operation that data is available on a substream.
        Thread-safe: acquires the controller lock internally.
        @param spop_obj the poll operation Qore object (caller holds no extra ref; this
               method adds its own ref before dispatching)
        @param stream_key the stream identifier to pass to onStreamData()
    */
    DLLLOCAL void enqueueStreamDataDispatch(QoreObject* spop_obj, const std::string& stream_key,
        const std::string& owner = std::string());

    //! Call abort on an AbstractPollOperation object
    DLLLOCAL static void callAbort(QoreObject* spop_obj, ExceptionSink* xsink);

    //! Build a result hash
    DLLLOCAL static QoreHashNode* buildResultHash(PollInfo& pinfo, bool canceled,
        QoreHashNode* ex_hash, ExceptionSink* xsink);

    //! Deliver a result via onComplete or queue (dispatches onComplete to worker thread)
    DLLLOCAL void deliverResult(Queue* queue, QoreObject* spop_obj, bool has_on_complete,
        QoreHashNode* result, ExceptionSink* xsink, const std::string& owner = std::string());

    //! Shared call dispatcher for async callback/abort delivery (lazily created, atomic for lock-free check)
    std::atomic<QoreCallDispatcher*> call_dispatcher{nullptr};

    //! Configured max callback workers; 0 = auto
    int max_callback_workers = 0;

    //! Thread pool for dispatching callbacks off the I/O thread (lazily created)
    /** Stopped during stop()/stopClear()/destructor. ThreadPool threads use
        QTF_EXTERNAL_LIFECYCLE so they don't block QoreProgramHelper shutdown.
    */
    ThreadPool* thread_pool = nullptr;

    //! Mutex for thread-safe lazy initialization of the thread pool
    QoreThreadLock pool_mutex;

    //! Flag indicating the pool has been stopped; prevents recreation after shutdown
    bool pool_stopped = false;

    //! Stop the thread pool if it exists (caller must NOT hold pool_mutex)
    DLLLOCAL void stopThreadPool(ExceptionSink* xsink);

};

//! Returns the global AsyncIoController singleton QoreObject (ref'd for caller); lazy-creates on first call
DLLLOCAL QoreObject* qore_get_async_io_controller_obj(ExceptionSink* xsink);

//! Cleans up the global AsyncIoController singleton (called from qore_cleanup())
DLLLOCAL void qore_async_io_controller_cleanup();

//! Early shutdown hook: drops any QoreObject references the singleton holds that
//! may belong to user modules (logger, timer callback, timer user data). Called
//! from qore_cleanup() BEFORE QMM.delUser() so the refs are released while the
//! referenced modules are still fully loaded (safe to destruct their objects).
//! The singleton itself is left alive — its actual stop/destroy still happens
//! in qore_async_io_controller_cleanup() afterward, so the controller can keep
//! serving cancelByProgram() callbacks during module teardown.
DLLLOCAL void qore_async_io_controller_pre_cleanup();

//! Returns true if the given controller is the global singleton
DLLLOCAL bool qore_is_async_io_controller_singleton(AsyncIoControllerPriv* ctrl);

#endif // _QORE_ASYNCIOCONTROLLERPRIV_H
