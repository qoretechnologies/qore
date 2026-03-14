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

#include <atomic>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Forward declarations
class QoreObject;
class QoreSocketObject;
class SocketPollOperationBase;

//! Lightweight dispatcher for executing Qore-language continuePoll() overrides
/** Manages a small pool of Qore worker threads (with full interpreter stack) that
    can execute Qore method calls on behalf of dedicated I/O threads (which have
    minimal stacks and no interpreter context).

    Workers are lazily created up to \c max_workers when work arrives.

    @since %Qore 2.3
*/
class QoreCallDispatcher {
public:
    //! Work item for cross-thread method dispatch
    struct WorkItem {
        QoreObject* spop_obj;           //!< Poll operation object (NOT referenced — caller keeps alive)
        QoreHashNode* result = nullptr; //!< Output: new poll info hash (referenced) or nullptr
        ExceptionSink xsink;            //!< Output: exceptions from evalMethod
        QoreCondition done;             //!< Signaled when work is complete
        bool completed = false;         //!< Set under dispatcher lock before signal
    };

    //! Creates the dispatcher
    /** @param max_workers maximum number of Qore worker threads
    */
    DLLLOCAL QoreCallDispatcher(int max_workers = DEFAULT_MAX_WORKERS);

    //! Destructor — does NOT stop workers; call stop() first
    DLLLOCAL ~QoreCallDispatcher();

    //! Dispatch a continuePoll() call to a Qore worker thread and block until complete
    /** @param spop_obj the AbstractPollOperation object
        @param caller_xsink receives any exceptions from the call
        @return new poll info hash (referenced) or nullptr
    */
    DLLLOCAL QoreHashNode* dispatchContinuePoll(QoreObject* spop_obj, ExceptionSink* caller_xsink);

    //! Stop all worker threads
    DLLLOCAL void stop(ExceptionSink* xsink);

    //! Default maximum workers
    static constexpr int DEFAULT_MAX_WORKERS = 4;

private:
    QoreThreadLock m;
    QoreCondition work_avail;               //!< Signaled when work is available
    QoreCondition workers_done;             //!< Signaled when all workers have exited
    std::deque<WorkItem*> queue;            //!< Pending work items
    int active_workers = 0;                 //!< Number of running worker threads
    int max_workers;                        //!< Maximum workers
    bool stopping = false;                  //!< Set during shutdown

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
public:
    //! Default I/O operation timeout (30 seconds)
    static constexpr int64 DEFAULT_IO_TIMEOUT_US = 30000000LL;

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
        @param xsink for exception handling
        @return the result hash
    */
    DLLLOCAL QoreHashNode* exec(QoreObject* self, QoreHashNode* info, bool replace, ExceptionSink* xsink);

    //! Cancel an operation by socket
    /** @param sock the socket to cancel
        @param xsink for exception handling
        @return true if an operation was found and canceled
    */
    DLLLOCAL bool cancel(QoreSocketObject* sock, ExceptionSink* xsink);

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

    //! Wake the I/O thread to force re-poll
    DLLLOCAL void wake();

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

    //! Custom deref with ExceptionSink
    DLLLOCAL virtual void deref(ExceptionSink* xsink);

private:
    //! I/O thread commands
    enum class IoCommand {
        Add,
        Cancel,
        CancelOwner,
        Quit,
        Wake,
        AddTimer,
        CancelTimer,
    };

    //! Command queue entry
    struct Command {
        IoCommand cmd = IoCommand::Wake;
        std::string key;                //!< For Cancel: the cache key
        std::string owner;              //!< For CancelOwner: the owner string
        QoreCondition* done_cond = nullptr; //!< For CancelOwner: signaled when all canceled
        bool* cancel_done_flag = nullptr; //!< For CancelOwner: set to true under lock before broadcast
        int64_t timer_deadline_us = 0;  //!< For AddTimer: absolute deadline in microseconds
        int64_t timer_id = 0;           //!< For AddTimer/CancelTimer: timer ID
    };

    //! Internal poll info (mirrors Qore Priv::PollInfo)
    struct PollInfo {
        int64 timeout_date_us;          //!< Absolute timeout (microseconds since epoch), 0 = not set
        QoreObject* sock_obj;           //!< Socket QoreObject (referenced)
        QoreSocketObject* sock;         //!< Socket private data
        QoreObject* spop_obj;           //!< AbstractPollOperation QoreObject (referenced)
        QoreHashNode* poll_info;        //!< Last SocketPollInfo hash (referenced, or nullptr)
        int64 timeout_us;               //!< Timeout in microseconds (negative = no timeout)
        std::string owner;              //!< Owner identifier
        QoreHashNode* other;            //!< Free-form data (referenced, or nullptr)
        Queue* queue;                   //!< Result queue (referenced, or nullptr)
        ResolvedCallReferenceNode* callback; //!< Completion callback (referenced, or nullptr)

        DLLLOCAL PollInfo() : timeout_date_us(0), sock_obj(nullptr), sock(nullptr),
            spop_obj(nullptr), poll_info(nullptr), timeout_us(DEFAULT_IO_TIMEOUT_US),
            other(nullptr), queue(nullptr), callback(nullptr) {
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
        ResolvedCallReferenceNode* callback; //!< Referenced or nullptr
        QoreHashNode* result;              //!< Referenced
    };

    //! Timer info stored under lock
    struct TimerInfo {
        QoreValue udata;                //!< Referenced Qore user data
    };

    // --- Mutex-protected state ---
    mutable QoreThreadLock m;
    std::unordered_map<std::string, PollInfo> cache;
    int tid;                              //!< I/O thread ID (0 if not running)
    bool autostop_flag;
    bool shutting_down;
    bool force_poll;
    std::unordered_map<std::string, int> socket_refcounts;
    std::deque<Command> cmdq;
    std::unordered_map<std::string, QoreCondition*> cancel_cond_map;
    QoreCondition io_cond;
    bool io_waiting;
    bool io_exiting;
    bool ready_flag;
    int submit_seq;                       //!< Incremented on each submit() call
    int processed_seq;                    //!< Updated by I/O thread after Phase 1 snapshot
    QoreCondition processed_cond;         //!< Signaled when processed_seq advances
    QoreLoggerBridge* logger;              //!< Referenced or nullptr
    std::unordered_map<int64_t, TimerInfo> timer_info_map; //!< Timer ID -> user data
    ResolvedCallReferenceNode* timer_callback; //!< Timer callback (referenced, or nullptr)

    //! Atomic counter for pre-allocating timer IDs across threads
    std::atomic<int64_t> next_ctrl_timer_id{1};

    // --- I/O thread only state ---
    QoreEventLoop* loop;
    QoreEventNotifier* notifier;

    // Registered socket tracking (I/O thread only, but updated under lock in phase 3)
    std::unordered_map<std::string, QoreObject*> registered_sockets; //!< key -> Socket obj
    std::unordered_map<std::string, int> registered_events;          //!< sock hash -> current events
    std::unordered_map<std::string, int> registered_fds;             //!< sock hash -> registered fd
    std::unordered_map<std::string, int> key_events;                 //!< key -> events for this key
    std::unordered_map<std::string, std::unordered_set<std::string>> sock_hash_to_keys; //!< reverse index

    //! Extra fd tracking: operation key -> set of registered extra fds
    /** @since %Qore 2.3
    */
    std::unordered_map<std::string, std::unordered_set<int>> key_extra_fds;

    // --- Internal methods ---

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

    //! Get the socket unique hash from a QoreSocketObject
    DLLLOCAL static std::string getSocketHash(QoreSocketObject* sock);

    //! Get socket and poll info from a SocketPollInfo hash
    DLLLOCAL static QoreObject* getSocketFromPollInfo(QoreHashNode* poll_info, std::string& sock_hash,
        int& events, ExceptionSink* xsink);

    //! Call continuePoll on an AbstractPollOperation object
    DLLLOCAL static QoreHashNode* callContinuePoll(QoreObject* spop_obj, ExceptionSink* xsink);

    //! Call abort on an AbstractPollOperation object
    DLLLOCAL static void callAbort(QoreObject* spop_obj, ExceptionSink* xsink);

    //! Build a result hash
    DLLLOCAL static QoreHashNode* buildResultHash(PollInfo& pinfo, bool canceled,
        QoreHashNode* ex_hash, ExceptionSink* xsink);

    // --- Dedicated thread support ---

    //! Info for a dedicated I/O thread handling a single poll operation
    /** @since %Qore 2.3
    */
    struct DedicatedThreadInfo {
        AsyncIoControllerPriv* controller = nullptr; //!< Back-pointer (referenced)
        std::string key;                        //!< Cache key for this operation
        PollInfo pinfo;                         //!< The operation (owns refs)
        SocketPollOperationBase* spop_base = nullptr; //!< C++ poll operation (referenced, or nullptr)
        bool has_qore_override = false;         //!< True if continuePoll() is overridden in Qore
        std::atomic<bool> stop_requested{false};//!< Set to request graceful stop
        QoreEventLoop* loop = nullptr;          //!< Created by dedicated thread
        QoreEventNotifier* notifier = nullptr;  //!< Created before thread spawn (referenced)
        QoreCondition exit_cond;                //!< Signaled when thread exits
        bool exited = false;                    //!< Set under controller lock
        int tid = 0;                            //!< Thread ID
    };

    //! Stack size for dedicated I/O threads (128KB — no Qore stack guard enforced)
    /** These threads run pure C++ I/O code with no Qore interpreter overhead.
        The stack guard is disabled via QTF_NO_STACK_GUARD in q_start_thread().
        128KB is needed because glibc reserves ~63KB of static TLS (QUIC/HTTP/3
        buffers in libqore) from each thread's stack before the thread starts.
    */
    static constexpr size_t DEDICATED_THREAD_STACK_SIZE = 128 * 1024;

    //! Dedicated thread map: key -> DedicatedThreadInfo (protected by m)
    std::unordered_map<std::string, DedicatedThreadInfo*> dedicated_threads;

    //! Deferred delete list for DedicatedThreadInfo objects (protected by m)
    /** Threads enqueue themselves here after broadcasting exit_cond; the next
        lock holder (cancelDedicatedThread, stopDedicatedThreads, or destructor)
        processes the list. This prevents a race where the thread deletes dti
        while cancelDedicatedThread is still accessing it inside exit_cond.wait().
    */
    std::vector<DedicatedThreadInfo*> deferred_dti_deletes;

    //! Shared call dispatcher for Qore-language continuePoll() overrides (lazily created)
    QoreCallDispatcher* call_dispatcher = nullptr;

    //! Spawn a dedicated I/O thread for an operation
    /** @param dti the DedicatedThreadInfo (takes ownership)
        @param xsink for exception handling
    */
    DLLLOCAL void spawnDedicatedThread(DedicatedThreadInfo* dti, ExceptionSink* xsink);

    //! Dedicated thread entry point
    DLLLOCAL static void dedicatedThreadEntry(ExceptionSink* xsink, void* arg);

    //! Dedicated thread main loop
    DLLLOCAL void dedicatedThread(DedicatedThreadInfo* dti, ExceptionSink* xsink);

    //! Cancel a dedicated thread by key (caller must NOT hold lock)
    /** @return true if a dedicated thread was found and canceled
    */
    DLLLOCAL bool cancelDedicatedThread(const std::string& key, ExceptionSink* xsink);

    //! Stop all dedicated threads (caller must NOT hold lock)
    DLLLOCAL void stopDedicatedThreads(ExceptionSink* xsink);

    //! Call continuePoll — direct C++ or via dispatcher for Qore overrides
    DLLLOCAL QoreHashNode* callContinuePollDedicated(DedicatedThreadInfo* dti, ExceptionSink* xsink);
};

#endif // _QORE_ASYNCIOCONTROLLERPRIV_H
