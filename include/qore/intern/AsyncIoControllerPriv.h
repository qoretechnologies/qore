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
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Forward declarations
class QoreObject;
class QoreSocketObject;
class SocketPollOperationBase;

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
    */
    DLLLOCAL void dispatchAbortAsync(QoreObject* spop_obj);

    //! Dispatch an onComplete(result) call asynchronously (fire-and-forget)
    /** @param spop_obj the AbstractPollOperation object (referenced — ownership transferred)
        @param result the SocketPollResultInfo hash (referenced — ownership transferred)
    */
    DLLLOCAL void dispatchOnCompleteAsync(QoreObject* spop_obj, QoreHashNode* result);

    //! Dispatch a code callback asynchronously (for timer events)
    /** @param callback the callback code (referenced — ownership transferred)
        @param args the argument list (referenced — ownership transferred, or nullptr)
    */
    DLLLOCAL void dispatchAsync(ResolvedCallReferenceNode* callback, QoreListNode* args);

    //! Dispatch continuePoll() asynchronously with result delivery back to I/O thread
    /** @param spop_obj the AbstractPollOperation object (referenced — ownership transferred)
        @param controller the AsyncIoControllerPriv to deliver the result to (referenced)
        @param key the operation key for result correlation
    */
    DLLLOCAL void dispatchContinuePollAsync(QoreObject* spop_obj,
        AsyncIoControllerPriv* controller, const std::string& key);

    //! Dispatch onStreamData(stream_key) asynchronously (fire-and-forget)
    /** Called by the I/O thread after stream data arrives.
        The worker thread calls onStreamData() which the Qore subclass overrides
        to wake the handler thread.

        @param spop_obj the poll operation object (referenced — ownership transferred)
        @param stream_key stream identifier (H2: "stream_id", H3: "session_id:stream_id")
    */
    DLLLOCAL void dispatchStreamDataAsync(QoreObject* spop_obj, const std::string& stream_key);

    //! Dispatch onPollComplete() asynchronously (fire-and-forget)
    /** Called by the I/O thread after WebSocketClientPollOperationBase::continuePoll()
        pushes frames to the recv_queue. The worker thread calls onPollComplete()
        which the Qore subclass overrides to schedule delivery.

        @param spop_obj the WebSocketClientPollOperationBase object (referenced — ownership transferred)
    */
    DLLLOCAL void dispatchPollCompleteAsync(QoreObject* spop_obj);

    //! Stop all worker threads
    DLLLOCAL void stop(ExceptionSink* xsink);

    //! Wait for all pending work items to be processed
    /** Blocks until the async queue is empty AND no workers are actively
        processing items. Used during shutdown to ensure all onComplete()
        callbacks are delivered before stopping thread pools that depend
        on them.
    */
    DLLLOCAL void waitForIdle();

    //! Maximum cap for auto-scaled workers
    static constexpr int MAX_WORKER_CAP = 32;

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
    DLLLOCAL QoreHashNode* getInfo(ExceptionSink* xsink) const;

    //! Sets the logger
    /** @param logger_obj the LoggerInterface Qore object (or nullptr to clear)
        @param xsink for exception handling
    */
    DLLLOCAL void setLogger(QoreObject* logger_obj, ExceptionSink* xsink);

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
        Add,
        Cancel,
        CancelOwner,
        Quit,
        WakeSocket,          //!< Targeted wake: re-poll a specific socket's operation
        AddTimer,
        CancelTimer,
        ContinuePollResult,  //!< Result from async continuePoll() dispatch
    };

    //! Command queue entry
    struct Command {
        IoCommand cmd = IoCommand::WakeSocket;
        std::string key;                //!< For Cancel / ContinuePollResult: the cache key
        std::string owner;              //!< For CancelOwner: the owner string
        QoreCondition* done_cond = nullptr; //!< For CancelOwner: signaled when all canceled
        bool* cancel_done_flag = nullptr; //!< For CancelOwner: set to true under lock before broadcast
        int64_t timer_deadline_us = 0;  //!< For AddTimer: absolute deadline in microseconds
        int64_t timer_id = 0;           //!< For AddTimer/CancelTimer: timer ID
        QoreHashNode* continue_poll_result = nullptr;  //!< For ContinuePollResult: new poll info (or nullptr)
        QoreHashNode* continue_poll_ex = nullptr;      //!< For ContinuePollResult: exception (or nullptr)
        bool continue_poll_completed = false;           //!< For ContinuePollResult: true if completed
        std::string sock_hash;          //!< For WakeSocket: socket hash to re-poll
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

    //! Deferred delivery info
    struct DeferredDelivery {
        std::string key;
        Queue* queue;                      //!< Referenced or nullptr
        QoreObject* spop_obj;              //!< Referenced or nullptr (for onComplete dispatch)
        bool has_on_complete;              //!< True if spop has onComplete() override
        QoreHashNode* result;              //!< Referenced
    };

    //! Timer info stored under lock
    struct TimerInfo {
        QoreValue udata;                //!< Referenced Qore user data
    };

    // --- Lock-free state (MPSC: multiple producer threads, single I/O thread consumer) ---
    MpscQueue<Command> cmdq;              //!< Lock-free command queue (replaces mutex-protected deque)
    std::atomic<bool> io_running{false};  //!< True when I/O thread accepts commands (no lock needed to check)

    // --- Mutex-protected state ---
    mutable QoreThreadLock m;
    std::unordered_map<std::string, PollInfo> cache;
    int tid;                              //!< I/O thread ID (0 if not running)
    bool autostop_flag;
    bool shutting_down;
    std::unordered_map<std::string, int> socket_refcounts;
    std::unordered_map<std::string, QoreCondition*> cancel_cond_map;
    QoreCondition io_cond;
    bool io_waiting;
    bool io_exiting;
    bool ready_flag;
    int submit_seq;                       //!< Incremented on each submit() call
    int processed_seq;                    //!< Updated by I/O thread after Phase 1 snapshot
    int64 autostop_idle_since;            //!< Timestamp when cache first became empty (0 = not idle)
    QoreCondition processed_cond;         //!< Signaled when processed_seq advances
    QoreLoggerBridge* logger;              //!< Referenced or nullptr
    std::unordered_map<int64_t, TimerInfo> timer_info_map; //!< Timer ID -> user data
    ResolvedCallReferenceNode* timer_callback; //!< Timer callback (referenced, or nullptr)

    //! Atomic counter for pre-allocating timer IDs across threads
    std::atomic<int64_t> next_ctrl_timer_id{1};

    // --- I/O thread only state ---
    QoreEventLoop* loop;
    QoreEventNotifier* notifier;

    //! Socket hashes that need re-polling (set by WakeSocket cmd, consumed by Phase 1)
    //! I/O thread only — both producer (processCommands) and consumer (Phase 1) are on I/O thread
    std::unordered_set<std::string> wake_socket_hashes;

    // Registered socket tracking (I/O thread only, but updated under lock in phase 3)
    std::unordered_map<std::string, QoreObject*> registered_sockets; //!< key -> Socket obj
    std::unordered_map<std::string, int> registered_events;          //!< sock hash -> current events
    std::unordered_map<std::string, int> registered_fds;             //!< sock hash -> registered fd
    std::unordered_map<std::string, int> key_events;                 //!< key -> events for this key
    std::unordered_map<std::string, std::unordered_set<std::string>> sock_hash_to_keys; //!< reverse index
    std::unordered_map<int, std::string> fd_to_sock_hash;             //!< fd -> sock_hash (O(1) epoll dispatch)

    //! Extra fd tracking: operation key -> set of registered extra fds
    /** @since %Qore 2.3
    */
    std::unordered_map<std::string, std::unordered_set<int>> key_extra_fds;

    // --- Internal methods ---

    //! Ensure call_dispatcher exists (lock-free fast path)
    DLLLOCAL void ensureCallDispatcher();

    //! Start the I/O thread (caller must hold lock)
    DLLLOCAL void startIntern(ExceptionSink* xsink);

    //! The I/O thread main function
    DLLLOCAL static void ioThreadEntry(ExceptionSink* xsink, void* arg);

    //! The I/O thread main loop
    DLLLOCAL void ioThread(ExceptionSink* xsink);

    //! Process commands from the command queue
    /** @return true if the I/O thread should exit
    */
    DLLLOCAL bool processCommands(ExceptionSink* xsink);

    //! Cancel an operation internally (delivers result, called from I/O thread)
    DLLLOCAL void doCancelIntern(PollInfo& pinfo, ExceptionSink* xsink);

    //! Update EventLoop registration for an operation
    /** @param key the operation key
        @param socket the new socket to register (nullptr to unregister)
        @param sock_hash the socket's unique hash
        @param events the events to monitor
        @param xsink for exception handling
    */
    DLLLOCAL void updateEventLoopRegistration(const std::string& key, QoreObject* socket,
        const std::string& sock_hash, int events, ExceptionSink* xsink);

    //! Unregister an operation from the EventLoop
    DLLLOCAL void unregisterFromEventLoop(const std::string& key, ExceptionSink* xsink);

    //! Update extra fd registrations for an operation
    /** @since %Qore 2.3
    */
    DLLLOCAL void updateExtraFds(const std::string& key, QoreObject* socket,
        QoreHashNode* poll_info, ExceptionSink* xsink);

    //! Unregister extra fds for an operation
    /** @since %Qore 2.3
    */
    DLLLOCAL void unregisterExtraFds(const std::string& key, ExceptionSink* xsink);

    //! Compute the event union for a socket
    DLLLOCAL int computeEventUnion(const std::string& sock_hash) const;

    //! Apply the event union for a socket
    DLLLOCAL void applyEventUnion(QoreObject* socket, const std::string& sock_hash, ExceptionSink* xsink);

    //! Enqueue a command (caller must hold lock)
    /** @return true if the notifier should be signaled
    */
    DLLLOCAL bool enqueueCmdLocked(IoCommand cmd, const std::string& key = std::string(),
        const std::string& owner = std::string(), QoreCondition* done_cond = nullptr,
        bool* cancel_done_flag = nullptr);

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
    DLLLOCAL void enqueueStreamDataDispatch(QoreObject* spop_obj, const std::string& stream_key);

    //! Call abort on an AbstractPollOperation object
    DLLLOCAL static void callAbort(QoreObject* spop_obj, ExceptionSink* xsink);

    //! Build a result hash
    DLLLOCAL static QoreHashNode* buildResultHash(PollInfo& pinfo, bool canceled,
        QoreHashNode* ex_hash, ExceptionSink* xsink);

    //! Deliver a result via onComplete or queue (dispatches onComplete to worker thread)
    DLLLOCAL void deliverResult(Queue* queue, QoreObject* spop_obj, bool has_on_complete,
        QoreHashNode* result, ExceptionSink* xsink);

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

//! Returns true if the given controller is the global singleton
DLLLOCAL bool qore_is_async_io_controller_singleton(AsyncIoControllerPriv* ctrl);

#endif // _QORE_ASYNCIOCONTROLLERPRIV_H
