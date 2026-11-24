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
#include "qore/intern/QoreForkHandlers.h"

#include <algorithm>
#include <atomic>
#include <deque>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <functional>
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

//! A pollable I/O object whose async I/O operations run on the I/O thread of the socket it wraps
/** The lightweight wrappers that C++ socket methods create for controller-backed calls have their own I/O identity
    hash, but they use a socket that other operations use too, so every operation on a wrapper must run on the I/O
    thread of the socket's other operations; see qore_socket_private::async_io_route_key and
    AsyncIoControllerPriv::getRouteKey().

    @since %Qore 3.0
*/
class AsyncIoRoutedPollable : public AbstractPollableIoObjectBase {
public:
    //! Returns the route key that selects the I/O thread of every operation on this object
    DLLLOCAL virtual const std::string& getAsyncIoRouteKey() const = 0;

};

//! Lends the previous poll info hash of an operation to the C++ continuePoll() call the I/O thread makes for it
/** The controller replaces an operation's poll info with the hash that continuePoll() returns, so a previous hash
    that nothing else references can be reused for the result: SocketPollOperationBase::getSocketPollInfoHash()
    takes the lent hash and sets its members as a new hash has them, instead of creating a new hash while the
    previous one is released.  The hash is moved out of the operation for the call; a hash that continuePoll() does
    not take is released when the lender goes out of scope.

    @since %Qore 3.0
*/
class SocketPollInfoLender {
public:
    //! Lends \a poll_info if it is a SocketPollInfo hash that only the operation references
    /** @param poll_info the operation's poll info; set to nullptr when it is lent
        @param xsink for releasing a hash that is not taken
    */
    DLLLOCAL SocketPollInfoLender(QoreHashNode*& poll_info, ExceptionSink* xsink);

    //! Releases the lent hash if continuePoll() did not take it
    DLLLOCAL ~SocketPollInfoLender();

    //! Takes the hash lent to the current thread, if any
    /** @return the hash, whose only reference the caller takes over, or nullptr
    */
    DLLLOCAL static QoreHashNode* take();

    //! Returns true if the hash lent by this lender was taken
    DLLLOCAL bool taken() const {
        return lent && !isLent();
    }

private:
    QoreHashNode* lent = nullptr;
    //! The hash lent by an enclosing lender on this thread, restored when this one goes out of scope
    QoreHashNode* prev = nullptr;
    ExceptionSink* xsink;

    DLLLOCAL bool isLent() const;
};

//! Converts an absolute microsecond deadline to the millisecond timeout expected by the OS poll API
/** @param deadline_us the absolute deadline in microseconds
    @param now_us the current absolute time in microseconds
    @return 0 if the deadline has elapsed; otherwise the positive interval rounded up to milliseconds and clamped to INT_MAX

    Positive sub-millisecond intervals are rounded up so the controller sleeps instead of spinning.
*/
DLLLOCAL int qore_async_io_deadline_to_poll_timeout_ms(int64 deadline_us, int64 now_us);

//! Lightweight async dispatcher for executing user callbacks and Qore abort() off the I/O thread
/** Manages a small pool of Qore worker threads (with full interpreter stack) that
    execute user callbacks and Qore-language abort() calls on behalf of the I/O thread
    (which must not run untrusted code or block on user callbacks).

    Workers are lazily created up to \c max_workers when work arrives.

    @since %Qore 3.0
*/
class AsyncIoControllerPriv;  // forward declaration for DT_CONTINUE_POLL

//! References to be released by a callback worker instead of the async I/O thread
/** Releasing the last reference to a Qore value runs its destructors, and Qore code must not run on the I/O
    thread: a destructor that calls back into the controller and waits for the I/O thread (for example
    \c HttpClientConnectionManager::closeAll() -> \c flushCallbacksByOwner()) can never be satisfied there, which
    stalls the I/O thread and with it every other user of the controller.

    The poll operation members mirror the references held by a poll operation's controller entry and are released
    in the same order as @ref AsyncIoControllerPriv::PollInfo::cleanup() releases them, followed by \c udata.
*/
struct AsyncIoDeferredRelease {
    QoreObject* sock_obj = nullptr;                  //!< Pollable I/O object (referenced, or nullptr)
    AbstractPollableIoObjectBase* sock = nullptr;    //!< Pollable I/O private data (referenced, or nullptr)
    QoreObject* spop_obj = nullptr;                  //!< Poll operation object (referenced, or nullptr)
    QoreHashNode* poll_info = nullptr;               //!< Poll info hash (referenced, or nullptr)
    QoreHashNode* other = nullptr;                   //!< Free-form data (referenced, or nullptr)
    Queue* queue = nullptr;                          //!< Result queue (referenced, or nullptr)
    QoreObject* queue_obj = nullptr;                 //!< Result queue object (referenced, or nullptr)
    Queue* state_queue = nullptr;                    //!< State event queue (referenced, or nullptr)
    QoreObject* state_queue_obj = nullptr;           //!< State event queue object (referenced, or nullptr)
    SocketPollOperationBase* spop_base = nullptr;    //!< C++ poll operation (referenced, or nullptr)
    QoreValue udata;                                 //!< Timer user data, e.g. a callback (referenced)
    //! True if the references belong to work that the module cleanup handed over
    //! (QoreSandboxContext::module_cleanup_delegate): release() then runs with the delegation, wherever it runs (a
    //! callback worker, a synchronous caller, the thread that ends a debug hold), as releasing them can run the
    //! destructors of objects of module classes, which enter the Programs of the modules (see
    //! qore_close_module_programs()); counted until the references are released (QoreModuleCleanupWorkFlag), as the
    //! module cleanup waits for their release (qore#6928)
    QoreModuleCleanupWorkFlag module_cleanup_delegate;
    //! True if the references belong to an operation that the module cleanup canceled
    //! (QoreSandboxContext::module_cleanup_canceled): the destructors that releasing them runs cannot start new
    //! asynchronous work (AsyncIoControllerPriv::checkModuleCleanupNewWork(), qore#7027)
    bool module_cleanup_canceled = false;

    //! the functional-domain restrictions of the code that submitted the work whose references these are, which the
    //! release runs with (see QoreSubmitterRestrictionsHelper, qore#6844)
    QoreParseOptions caller_restrictions;
    //! the restrictions of that code as the checks that a lent object's method does not lend see them
    //! (QoreSandboxContext::lent_to_restrictions)
    QoreParseOptions lent_to_restrictions;

    //! Sets the module cleanup flags of the references from the context of the work that they belong to
    DLLLOCAL void setModuleCleanup(const std::shared_ptr<const QoreSandboxContext>& ctx);

    //! Returns True if no references are held
    DLLLOCAL bool empty() const {
        return !sock_obj && !sock && !spop_obj && !poll_info && !other && !queue && !queue_obj && !state_queue
            && !state_queue_obj && !spop_base && !udata.hasNode();
    }

    //! Returns the program the released objects belong to, if known
    DLLLOCAL QoreProgram* getProgram() const;

    //! Releases all references in order and clears them
    DLLLOCAL void release(ExceptionSink* xsink);
};

//! The references that the controller drops for an operation that a synchronous exec() call waits for
/** The controller's references to an operation can be the last ones, so an I/O thread hands them to a callback
    worker to release (AsyncIoControllerPriv::releaseOffIoThread()), which wakes a third thread for every operation.
    The caller of exec() waits on its own thread while the operation runs, and runs Qore code there: it takes the
    references instead, and releases them when its wait ends (finish()).  A reference that the controller drops
    after that, which happens when the caller gave the call up or took its result before the controller was done
    with the operation, is refused (add() returns false), and the controller hands it to a callback worker as before.

    The lock is held only to add or to take the references: never while they are released or while anything waits.

    See design/sync-socket-first-step.md, "5. A cheaper round trip for the calls that do wait".

    @since %Qore 3.0
*/
class AsyncIoSyncRelease {
public:
    DLLLOCAL AsyncIoSyncRelease() = default;
    DLLLOCAL AsyncIoSyncRelease(const AsyncIoSyncRelease&) = delete;
    DLLLOCAL AsyncIoSyncRelease& operator=(const AsyncIoSyncRelease&) = delete;

    //! The caller always calls finish() before it drops its own pointer, and nothing is added after that
    DLLLOCAL ~AsyncIoSyncRelease() {
        assert(refs.empty());
    }

    //! Takes references for the caller
    /** @param r the references; cleared if they were taken

        @return true if the references were taken, false if the caller's wait has ended, in which case @a r is
        unchanged
    */
    DLLLOCAL bool add(AsyncIoDeferredRelease& r) {
        if (r.empty()) {
            return true;
        }
        AutoLocker al(m);
        if (done) {
            return false;
        }
        refs.push_back(r);
        r = AsyncIoDeferredRelease();
        return true;
    }

    //! Ends the caller's wait: releases the references taken on the calling thread; later ones are refused
    DLLLOCAL void finish(ExceptionSink* xsink) {
        std::vector<AsyncIoDeferredRelease> taken;
        {
            AutoLocker al(m);
            done = true;
            taken.swap(refs);
        }
        for (AsyncIoDeferredRelease& r : taken) {
            r.release(xsink);
        }
    }

private:
    QoreThreadLock m;
    //! True once the caller's wait has ended
    bool done = false;
    std::vector<AsyncIoDeferredRelease> refs;
};

//! A native task run on a callback worker, off the async I/O execution path
/** Native poll operations use this for a step that may block, and so must run neither on the async I/O thread nor
    in a continuePoll() dispatch; for example, acquiring a connection to another server to follow an HTTP redirect.

    Exactly one of run() and discard() is called, and the dispatcher then deletes the task.  A task holds its own
    references to whatever it needs; neither method may throw a C++ exception.

    @since %Qore 3.0
*/
class AsyncIoNativeTask {
public:
    DLLLOCAL virtual ~AsyncIoNativeTask() = default;

    //! Runs the task on a callback worker
    DLLLOCAL virtual void run(ExceptionSink* xsink) = 0;

    //! Called instead of run() when the task cannot be run, for example because the dispatcher is stopping
    /** May be called on any thread, including the async I/O thread, so it must not block
    */
    DLLLOCAL virtual void discard(ExceptionSink* xsink) = 0;

    //! Returns the program the task runs in, if any; not referenced
    DLLLOCAL virtual QoreProgram* getProgram() const = 0;
};

#ifdef DEBUG
//! Overrides the async I/O thread flag for focused unit tests
/** @return the previous async I/O thread flag value
*/
DLLLOCAL bool qore_set_async_io_thread_for_test(bool value);
#endif

class QoreSandboxContext;

//! The sandbox context of the thread that submitted a poll operation
/** Captured by AsyncIoControllerPriv::submit() and applied with QoreSandboxContextHelper around every step of the
    operation that runs on a callback worker, so that the operation, and any connection it makes there, is governed by
    the sandbox of the code that submitted it and not by the context of the worker; empty if no sandbox applies and the
    operation is not work that the module cleanup hands over (QoreSandboxContext::module_cleanup_delegate), whose
    callbacks can enter the Programs of the modules while the cleanup has closed them (QoreModuleCleanupDelegateHelper).
*/
typedef std::shared_ptr<const QoreSandboxContext> AsyncIoSandboxContext;

//! The run-time parse options of the code that registered a callback that is not run in a Program (a timer's), which
//! the callback runs with; empty if none were captured (qore#6208)
/** A callback that is a call reference to builtin code sets no options of its own; one that module code registered
    for a sandboxed caller would otherwise lose the caller's restrictions
*/
typedef std::shared_ptr<const QoreThreadRuntimeParseOptions> AsyncIoRuntimeOptions;

//! Returns true if \a ctx belongs to work that the module cleanup handed over
//! (QoreSandboxContext::module_cleanup_delegate)
DLLLOCAL inline bool async_io_module_cleanup_delegate(const std::shared_ptr<const QoreSandboxContext>& ctx) {
    return ctx && ctx->module_cleanup_delegate;
}

//! Returns true if @a ctx belongs to work of the module cleanup that the cleanup does not count (qore#7027)
/** An operation, a timer or a work item that work of the cleanup submitted, whose context it captured
    (QoreSandboxContext::module_cleanup_delegate without QoreSandboxContext::module_cleanup_work): the cleanup counts
    its work items when they are queued, and cancels the operations and timers when it waits for its work
    (AsyncIoControllerPriv::cancelModuleCleanupUncounted()); timers and the operations that the controller does not
    submit for it are such work, while the operations that it submits are counted (AsyncIoControllerPriv::submit())
*/
DLLLOCAL inline bool async_io_module_cleanup_uncounted(const std::shared_ptr<const QoreSandboxContext>& ctx) {
    return ctx && ctx->module_cleanup_delegate && !ctx->module_cleanup_work;
}

//! Returns true if @a ctx belongs to an operation that the module cleanup canceled
//! (QoreSandboxContext::module_cleanup_canceled, qore#7027)
DLLLOCAL inline bool async_io_module_cleanup_canceled(const std::shared_ptr<const QoreSandboxContext>& ctx) {
    return ctx && ctx->module_cleanup_canceled;
}

//! Returns true if a cancel round of the module cleanup cancels the operation or timer of @a ctx (qore#7027)
/** Before the deadline of the cleanup, the work that it does not count (async_io_module_cleanup_uncounted()); at and
    after it (@a deadline), all of its work that it has not canceled yet
*/
DLLLOCAL inline bool async_io_module_cleanup_cancel_match(const std::shared_ptr<const QoreSandboxContext>& ctx,
        bool deadline) {
    return deadline ? ctx && ctx->module_cleanup_delegate && !ctx->module_cleanup_canceled
        : async_io_module_cleanup_uncounted(ctx);
}

//! Returns the functional-domain restrictions of the code that captured the context, or none (qore#6844)
DLLLOCAL inline QoreParseOptions async_io_caller_restrictions(const std::shared_ptr<const QoreSandboxContext>& ctx) {
    return ctx ? ctx->caller_restrictions : QoreParseOptions();
}

//! Returns the lent-to restrictions of the code that captured the context, or none (qore#6844)
DLLLOCAL inline QoreParseOptions async_io_lent_to_restrictions(
        const std::shared_ptr<const QoreSandboxContext>& ctx) {
    return ctx ? ctx->lent_to_restrictions : QoreParseOptions();
}

//! Captures the sandbox of the current thread for work continued on a callback worker; empty if no sandbox governs it
//! and the thread does not run the module cleanup or work that the cleanup handed over
/** A nested capture keeps a context that is applied, so work submitted by a step of another operation inherits that
    operation's sandbox.
*/
DLLLOCAL AsyncIoSandboxContext capture_async_io_sandbox_context();

//! The operations that the calling code may cancel or flush by key or owner (qore#7010)
/** Code that is sandboxed acts only on the operations of the application it runs for (see
    AsyncIoControllerPriv::callerAppId()); trusted code acts on any operation
*/
struct AioAppRestriction {
    //! true if the calling code is restricted to the operations of \a app_id
    bool restricted = false;
    //! the application whose operations the calling code may act on, if \a restricted
    qore_program_id_t app_id = 0;
};

//! The timers that the calling code may cancel by ID (qore#7010)
/** Code that is sandboxed cancels only the timers of the application that it runs for; if it runs for no known
    application, only the timers for no application that its own work added (see
    AsyncIoControllerPriv::timerCancelAllowed()); trusted code cancels any timer
*/
struct AioTimerRestriction {
    //! true if the calling code is sandboxed
    bool restricted = false;
    //! true if the calling code is sandboxed but runs for no known application
    bool none = false;
    //! the application whose timers the calling code may cancel, if \a restricted and not \a none
    qore_program_id_t app_id = 0;
    //! the generation of the calling code's work (runtime_get_thread_generation())
    uint64_t gen = 0;
};

//! the application that the code of a callback of an operation runs for (qore#7010)
DLLLOCAL extern thread_local qore_program_id_t aio_callback_app_id;
DLLLOCAL extern thread_local bool aio_callback_app_set;

//! Sets the application that the code of a callback of an operation runs for while it exists (qore#7010)
/** The callback (the operation's step, completion, abort, stream data or result, or a timer's code) runs on a
    worker thread for the application that the operation was submitted for (see AsyncIoControllerPriv::callerAppId()):
    an operation that it submits belongs to that application, also when no Program of the application is on the
    thread's stack
*/
class AioCallbackAppScope {
public:
    DLLLOCAL explicit AioCallbackAppScope(qore_program_id_t app_id);
    DLLLOCAL ~AioCallbackAppScope();

private:
    qore_program_id_t old_id;
    bool old_set;
};

class QoreCallDispatcher {
    friend class AsyncIoDispatcherCancelTest;
    friend class AsyncIoReleaseCountedScopeTest;
public:
    //! Dispatch type for async work items
    enum DispatchType {
        DT_ABORT,           //!< Call abort() on spop_obj
        DT_ON_COMPLETE,     //!< Call onComplete(result) on spop_obj
        DT_CALLBACK,        //!< Call a code reference with args (timer callbacks)
        DT_CONTINUE_POLL,   //!< Call continuePoll() on spop_obj and send result back to I/O thread
        DT_STREAM_DATA_NOTIFY, //!< Call onStreamData(stream_id) on spop_obj (stream queue drain notification)
        DT_STREAM_SEND_RESULT_NOTIFY, //!< Call onStreamSendResult(stream_key, sent) on spop_obj
        DT_POLL_COMPLETE_NOTIFY, //!< Call onPollComplete() on spop_obj (WebSocket frame arrival notification)
        DT_RELEASE,         //!< Release the references in release only, off the async I/O thread
        DT_NATIVE_TASK,     //!< Run the native task in native_task, off the async I/O execution path
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
        //! For DT_CONTINUE_POLL: index of the I/O thread whose cache holds the operation
        /** The result must go back to this thread: with several I/O threads an operation's thread is chosen by its
            socket, so it cannot be derived from the operation key.
        */
        int thread_idx = -1;
        QoreProgram* pgm = nullptr;          //!< Program reference to prevent premature deletion
        std::string owner;                   //!< Owner identifier for per-owner flush (empty if untracked)
        //! the application that the item's operation (or timer) is for: its code runs for it (qore#7010)
        qore_program_id_t app_pgm_id = 0;
        AsyncIoDeferredRelease* release = nullptr; //!< For DT_RELEASE: owned references to release (or nullptr)
        AsyncIoNativeTask* native_task = nullptr; //!< For DT_NATIVE_TASK: owned task (or nullptr)
        bool sent = false;                   //!< For DT_STREAM_SEND_RESULT_NOTIFY: the transmission result
        //! The sandbox context of the poll operation's submitter, applied while the item runs; see
        //! AsyncIoSandboxContext
        AsyncIoSandboxContext sandbox_ctx;
        //! For DT_CALLBACK: the run-time parse options of the code that registered the callback, installed while it
        //! runs, or empty (see AsyncIoRuntimeOptions)
        AsyncIoRuntimeOptions runtime_po;
        //! For DT_CALLBACK: the sandbox context applied while the item's arguments are released, or empty
        /** The arguments of the timer callback of the host hold the data of a timer of other code, whose destructors
            are that code's: they are released in its context, while the callback runs in the host's (qore#7010)
        */
        AsyncIoSandboxContext release_ctx;
        //! For DT_CONTINUE_POLL: read the operation's state after continuePoll() for its state events
        bool read_state = false;
        //! For DT_CONTINUE_POLL: the ID of the operation in the I/O thread's cache; see OpCache
        uint64_t op_id = 0;
        //! For DT_ABORT: the task in native_task delivers the result after abort() (see
        //! AsyncIoControllerPriv::AbortThenDeliverTask) and calls onComplete() when it is run
        bool after_abort_on_complete = false;
        //! Counts the item as work of the module cleanup if its context is delegated but not counted (qore#7027)
        /** Set when the item is queued (see async_io_module_cleanup_uncounted()): the item of an operation that work
            of the cleanup submitted and that ended on its own, which the cleanup waits for as it waits for its own
        */
        QoreModuleCleanupWorkFlag module_cleanup_work;
#ifdef DEBUG
        //! Debug-only: the item was held by the dbg_async_io_hold_module_cleanup_work() test hook; the worker writes on
        //! stderr when it has ended (qore#6928)
        bool dbg_held_module_cleanup_work = false;
#endif
    };

    //! Creates the dispatcher
    /** @param max_workers maximum number of Qore worker threads; 0 = auto (min(hardware_concurrency, 32))
        @param controller the owning AsyncIoControllerPriv for logging (may be nullptr)
        @param marked_programs the Programs whose asynchronous work is being canceled
            (AsyncIoControllerPriv::cancelByProgram()), which the dispatcher is created marked for (see
            markProgramShuttingDown()), as the dispatcher that it replaces was (qore#7311)
    */
    DLLLOCAL QoreCallDispatcher(int max_workers = 0, AsyncIoControllerPriv* controller = nullptr,
        const std::unordered_map<QoreProgram*, int>* marked_programs = nullptr);

    //! Destructor — does NOT stop workers; call stop() first
    DLLLOCAL ~QoreCallDispatcher();

    //! Dispatch an abort() call asynchronously (fire-and-forget)
    /** @param spop_obj the AbstractPollOperation object (referenced — ownership transferred)
        @param owner optional owner identifier for per-owner flush tracking
        @param sandbox_ctx the sandbox of the operation's submitter
        @param after_abort a task that delivers the operation's result after abort() returns (ownership transferred),
        or nullptr: the worker runs it after abort() unless the operation's Program or owner has begun to shut down
        in the meantime, and discards it (AsyncIoNativeTask::discard()) otherwise, and also if the item is not run
        @param after_abort_on_complete true if @a after_abort calls the operation's onComplete() when it is run
    */
    DLLLOCAL void dispatchAbortAsync(QoreObject* spop_obj,
        const std::string& owner = std::string(), AsyncIoSandboxContext sandbox_ctx = AsyncIoSandboxContext(),
        AsyncIoNativeTask* after_abort = nullptr, bool after_abort_on_complete = false,
        qore_program_id_t app_pgm_id = 0);

    //! Dispatch an onComplete(result) call asynchronously (fire-and-forget)
    /** @param spop_obj the AbstractPollOperation object (referenced — ownership transferred)
        @param result the SocketPollResultInfo hash (referenced — ownership transferred)
        @param owner optional owner identifier for per-owner flush tracking
    */
    DLLLOCAL void dispatchOnCompleteAsync(QoreObject* spop_obj, QoreHashNode* result,
        const std::string& owner = std::string(), AsyncIoSandboxContext sandbox_ctx = AsyncIoSandboxContext(),
        qore_program_id_t app_pgm_id = 0);

    //! Dispatch a code callback asynchronously (for timer events)
    /** @param callback the callback code (referenced — ownership transferred)
        @param args the argument list (referenced — ownership transferred, or nullptr)
        @param owner optional owner identifier for per-owner flush tracking (empty = untracked)
        @param sandbox_ctx the sandbox of the code that added the timer, which governs the callback
    */
    DLLLOCAL void dispatchAsync(ResolvedCallReferenceNode* callback, QoreListNode* args,
        const std::string& owner = std::string(), AsyncIoSandboxContext sandbox_ctx = AsyncIoSandboxContext(),
        AsyncIoRuntimeOptions runtime_po = AsyncIoRuntimeOptions(), qore_program_id_t app_pgm_id = 0,
        AsyncIoSandboxContext release_ctx = AsyncIoSandboxContext());

    //! Dispatch continuePoll() asynchronously with result delivery back to I/O thread
    /** @param spop_obj the AbstractPollOperation object (referenced — ownership transferred)
        @param controller the AsyncIoControllerPriv to deliver the result to (referenced)
        @param key the operation key
        @param op_id the operation's ID in the cache of its I/O thread, which identifies the result; another
        operation can take over the key while the step is in flight
        @param thread_idx the index of the I/O thread whose cache holds the operation; the result is delivered to
        this thread
        @param owner optional owner identifier for per-owner flush tracking
        @param read_state if true, the worker reads the operation's state with getState() after continuePoll() and
        sends it back with the result, for the operation's state events
    */
    DLLLOCAL void dispatchContinuePollAsync(QoreObject* spop_obj,
        AsyncIoControllerPriv* controller, const std::string& key, uint64_t op_id, int thread_idx,
        const std::string& owner = std::string(), AsyncIoSandboxContext sandbox_ctx = AsyncIoSandboxContext(),
        bool read_state = false, qore_program_id_t app_pgm_id = 0);

    //! Dispatch onStreamData(stream_key) asynchronously (fire-and-forget)
    /** Called by the I/O thread after stream data arrives.
        The worker thread calls onStreamData() which the Qore subclass overrides
        to wake the handler thread.

        @param spop_obj the poll operation object (referenced — ownership transferred)
        @param stream_key stream identifier (H2: "stream_id", H3: "session_id:stream_id")
        @param owner optional owner identifier for per-owner flush tracking
    */
    DLLLOCAL void dispatchStreamDataAsync(QoreObject* spop_obj, const std::string& stream_key,
        const std::string& owner = std::string(), AsyncIoSandboxContext sandbox_ctx = AsyncIoSandboxContext(),
        qore_program_id_t app_pgm_id = 0);

    //! Dispatch onStreamSendResult(stream_key, sent) asynchronously (fire-and-forget)
    /** Called by the I/O thread with the transmission result of a response whose stream is watched by an HTTP/2
        or HTTP/3 server poll operation.

        @param spop_obj the poll operation object (referenced — ownership transferred)
        @param stream_key stream identifier (H2: "stream_id", H3: "session_id:stream_id")
        @param sent true if the response was sent, false if not
        @param owner optional owner identifier for per-owner flush tracking
    */
    DLLLOCAL void dispatchStreamSendResultAsync(QoreObject* spop_obj, const std::string& stream_key, bool sent,
        const std::string& owner = std::string(), AsyncIoSandboxContext sandbox_ctx = AsyncIoSandboxContext(),
        qore_program_id_t app_pgm_id = 0);

    //! Dispatch onPollComplete() asynchronously (fire-and-forget)
    /** Called by the I/O thread after WebSocketClientPollOperationBase::continuePoll()
        pushes frames to the recv_queue. The worker thread calls onPollComplete()
        which the Qore subclass overrides to schedule delivery.

        @param spop_obj the WebSocketClientPollOperationBase object (referenced — ownership transferred)
        @param owner optional owner identifier for per-owner flush tracking
    */
    DLLLOCAL void dispatchPollCompleteAsync(QoreObject* spop_obj,
        const std::string& owner = std::string(), AsyncIoSandboxContext sandbox_ctx = AsyncIoSandboxContext(),
        qore_program_id_t app_pgm_id = 0);

    //! Runs a native task on a callback worker (fire-and-forget)
    /** The item is not owner-tracked.  When it cannot be queued, the task is discarded instead of run.

        @param task the task to run (ownership transferred)
    */
    DLLLOCAL void dispatchNativeTaskAsync(AsyncIoNativeTask* task);

    //! Releases references on a callback worker (fire-and-forget)
    /** Used by the async I/O thread for references whose release can run Qore destructors.  The item is not
        owner-tracked, so it is never discarded or waited for by an owner barrier; when it cannot be queued
        (the dispatcher is stopping or the objects' program is shutting down) the references are released on the
        calling thread, as for any discarded work item.

        @param release the references to release (ownership transferred)
    */
    DLLLOCAL void dispatchReleaseAsync(AsyncIoDeferredRelease* release);

    //! Stop all worker threads
    DLLLOCAL void stop(ExceptionSink* xsink);

    //! Wait for all pending callbacks to be processed
    /** Blocks until no callback work remains: the async queue holds no
        non-continuePoll items AND no workers are processing a callback
        (onComplete/abort/callback/stream/poll-complete).  Used during
        shutdown to ensure all such callbacks are delivered before stopping
        thread pools that depend on them.

        Deliberately does NOT wait for in-flight DT_CONTINUE_POLL dispatches
        (see @ref active_continue_poll): a poll op's continuePoll() may block
        indefinitely (OAuth2 refresh / happy-eyeballs via waitForNotifier())
        and is torn down via cancel()/cancelByOwner() + op completion, not via
        this barrier — waiting for it here deadlocks shutdown.

        Called on a worker of this dispatcher (a callback that flushes the callbacks, or a destructor that its
        release of an object runs, such as that of a server's async I/O controller stopped in a callback), it does
        not wait for the callback that the worker runs, which ends only when this call has returned (qore#6508).
    */
    DLLLOCAL void waitForIdle();

    //! Waits on idle_cond for waitForIdle(), with the dispatcher's lock held; counted for dbgWaitIdleWaiter() in a
    //! debug build
    DLLLOCAL void idleWait() {
#ifdef DEBUG
        ++dbg_idle_waiters;
        dbg_idle_waiter_cond.broadcast();
#endif
        idle_cond.wait(m);
#ifdef DEBUG
        --dbg_idle_waiters;
#endif
    }

#ifdef DEBUG
    //! Debug-only: the number of release items parked for the given Program (see parked_program_releases)
    DLLLOCAL size_t dbgParkedReleases(QoreProgram* pgm);

    //! Debug-only: waits until no item is queued or being processed, continuePoll() calls included
    /** Unlike waitForIdle(), this waits for continuePoll() calls too, which can block; it is meant for tests that
        need every operation of the controller to have run (see qore_quic_test_advance_clock())

        @return true if the dispatcher is idle, false if it was not when \a timeout_ms elapsed
    */
    DLLLOCAL bool dbgWaitAllIdle(int timeout_ms);

    //! Debug-only: waits until a thread waits in waitForIdle() for a callback (the wait blocks)
    /** For tests that must release a callback only once a flush waits for it (qore#6508)

        @return true if a thread waits in waitForIdle(), false if none did when \a timeout_ms elapsed
    */
    DLLLOCAL bool dbgWaitIdleWaiter(int timeout_ms);

    //! Debug-only: waits until a thread waits in waitForOwnerIdle() (in flushCallbacksByOwner(), whose mark is then
    //! set; qore#7010)
    /** @return true if a thread waits in waitForOwnerIdle(), false if none did when \a timeout_ms elapsed
    */
    DLLLOCAL bool dbgWaitOwnerIdleWaiter(int timeout_ms);
    //! Debug-only: waits until the given number of onComplete() items have been queued for the workers
    /** Counted once the item is in the queue (or discarded by enqueue()), so a flush of the callbacks made after the
        wait covers it; for tests that must wait for the completion of an operation that no Qore code of theirs can
        see, such as one whose callbacks are skipped (qore#6685); an onComplete() run after the operation's abort()
        (AbortThenDeliverTask) is not counted

        @return true if the count was reached, false if it was not when \a timeout_ms elapsed
    */
    DLLLOCAL bool dbgWaitOnCompleteQueued(int64_t count, int timeout_ms);

    //! Debug-only: makes the given allocation of markProgramShuttingDown() in the calling thread fail with
    //! std::bad_alloc (1 = the first), 0 for none (qore#7226)
    DLLLOCAL static void dbgSetMarkAllocFailAt(int n);

    //! Debug-only: a function that releaseWorkItem() calls in the calling thread before it releases an item, as the
    //! destructor that releasing an item runs could (qore#7226); empty for none
    DLLLOCAL static void dbgSetReleaseHook(std::function<void()> hook);

    //! Debug-only: the number of calls of markProgramShuttingDown() and clearProgramShuttingDown() in progress; the
    //! dispatcher must not be deleted while one is in progress (qore#7226)
    std::atomic<int> dbg_users{0};

    //! Debug-only: the number of onComplete() calls dispatched so far
    DLLLOCAL int64_t dbgOnCompleteDispatched() const {
        return dbg_on_complete_dispatched.load(std::memory_order_acquire);
    }
#endif

    //! Mark a program as shutting down so workers skip callbacks for it
    /** Called at the start of cancelByProgram(), before any I/O thread commands.
        Workers that pick up items for this program will silently discard them
        instead of calling evalMethod (which would hit PROGRAM-ERROR after ptid
        is set).

        This call also drops any already-queued items belonging to \a pgm and
        releases their object references in-place, except the release items (DT_RELEASE), which are parked and
        released in the Program by clearProgramShuttingDown(), as release items enqueued after the mark are; the memory
        for the items is reserved before any state is changed (qore#7226).  The combined mark + drain
        runs under the dispatcher lock, closing the race where a worker pops
        a pgm-owned item between an out-of-band mark and a separate drain
        and then derefs an spop_obj whose underlying class has just been torn
        down by the program's cleanup path (libqore SIGSEGV at workerLoop+0x3c6
        on a stale spop_obj pointing into recycled JVM-heap memory).

        Items submitted AFTER this call are still protected by the worker's
        \c pgm_shutting_down check at the top of \c workerLoop — those items
        are skipped at the user-callback step and only their refs are released
        by the worker's own cleanup, when the spop_obj is still valid because
        the strong ref taken at submit time has not been torn down yet.
    */
    DLLLOCAL void markProgramShuttingDown(QoreProgram* pgm, ExceptionSink* xsink);

    //! Remove a program from the shutting-down set
    /** Releases the release items of the Program that were parked while it was marked (see enqueue()) first, in the
        calling thread (the one that cancels the Program's work, before the Program's threads are waited for), each
        in the Program (its parse options, and counted in its threads), as a worker releases a release item
    */
    DLLLOCAL void clearProgramShuttingDown(QoreProgram* pgm, ExceptionSink* xsink);

    //! Releases the release items parked for a Program in that Program, as clearProgramShuttingDown() and stop() do
    /** Each release runs in the Program (its parse options, and counted in its threads) with an empty exception sink;
        the exceptions are given to \a xsink at the end.  The caller does not hold the lock.

        @param pgm the Program whose items are released
        @param items the items; empty on return
        @param xsink for exceptions
    */
    DLLLOCAL void releaseParkedItems(QoreProgram* pgm, std::vector<AsyncWorkItem>& items, ExceptionSink* xsink);

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

        This call also drops any already-queued items belonging to \a owner and
        releases their references, and enqueue() discards (rather than queues)
        further items for the owner while the mark is set.  Both are required for
        waitForOwnerIdle() to be a *bounded* wait: queued items are only counted
        down when a worker pops them, so a caller blocked in waitForOwnerIdle()
        while every worker is busy would wait forever for work that can only be
        performed by the pool it is starving.  That is a real, reproducible
        deadlock: a WebSocket frame dispatch running on the HTTP handler pool
        holds the connection's process mutex while destroying a private
        HttpClientConnectionManager, every callback worker blocks on that same
        mutex delivering further stream data, and the manager's cancellation
        callbacks then sit in the queue with nobody left to drain them.

        Draining here is also strictly safer than letting a worker discard the
        item later: an item popped after clearOwnerShuttingDown() no longer sees
        the mark and would run the user callback past the barrier.

        Out of memory here ends the process, as on a callback worker: a discarded step's result can be what a caller
        of cancelByProgram() waits for, and a partly moved queue must not be left behind.
    */
    DLLLOCAL void markOwnerShuttingDown(const std::string& owner,
        const AioAppRestriction& restrict_app = AioAppRestriction()) noexcept;

    //! Remove an owner from the shutting-down set
    DLLLOCAL void clearOwnerShuttingDown(const std::string& owner,
        const AioAppRestriction& restrict_app = AioAppRestriction());

    //! Returns the key of the mark of an owner: the owner itself, or for restricted code its application's mark
    DLLLOCAL static std::string ownerMarkKey(const std::string& owner, const AioAppRestriction& restrict_app);

    //! Returns true if the owner of the item is marked as shutting down, for all its items or for its application's
    /** Called with m held
    */
    DLLLOCAL bool ownerShuttingDownLocked(const AsyncWorkItem& item) const;

    //! Wait for in-flight callbacks belonging to a specific owner to complete
    /** Unlike waitForIdle() which waits for ALL callbacks, this only waits for
        callbacks tagged with \a owner.  This avoids deadlock when the caller
        holds a lock that callbacks from OTHER owners also need.

        Only callbacks a worker has already popped are waited for; queued items
        are removed by markOwnerShuttingDown() and further ones are rejected by
        enqueue() while the owner is marked, so this wait never depends on the
        worker pool having a free thread.  Callers MUST therefore mark the owner
        before waiting (flushCallbacksByOwner() does).

        @param owner the owner identifier to wait for
    */
    DLLLOCAL void waitForOwnerIdle(const std::string& owner);

    //! Upper bound for automatic worker sizing; explicit setMaxCallbackWorkers() values may be higher
    static constexpr int DEFAULT_WORKER_CAP = 512;

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
    //! Number of workers currently inside a DT_CONTINUE_POLL dispatch
    /** Subset of @ref active_processing.  waitForIdle()/flushCallbacks() must
        NOT wait for these: a poll op's continuePoll() can legitimately block
        (e.g. an OAuth2 token refresh / happy-eyeballs connect via
        waitForNotifier()), its lifecycle is governed by cancel()/cancelByOwner()
        + op completion, and flushCallbacks() is documented to drain pending
        onComplete/abort *callbacks* only.  Counting continuePoll here let a
        blocked ping continuePoll wedge HttpServer::stop()->flushCallbacks()
        forever (qorus-core shutdown deadlock). */
    int active_continue_poll = 0;
#ifdef DEBUG
    //! Debug-only: the number of onComplete() calls dispatched so far (see dbgOnCompleteDispatched())
    std::atomic<int64_t> dbg_on_complete_dispatched{0};
    //! Debug-only: the number of threads blocked in waitForIdle() (see dbgWaitIdleWaiter())
    int dbg_idle_waiters = 0;
    //! Debug-only: signaled when a thread blocks in waitForIdle()
    QoreCondition dbg_idle_waiter_cond;
    //! Debug-only: the number of threads blocked in waitForOwnerIdle() (see dbgWaitOwnerIdleWaiter())
    int dbg_owner_idle_waiters = 0;
    //! Debug-only: the number of onComplete() items queued (see dbgWaitOnCompleteQueued()); protected by m
    int64_t dbg_on_complete_queued = 0;
    //! Debug-only: signaled when an onComplete() item has been queued
    QoreCondition dbg_on_complete_queued_cond;
#endif
    int max_workers;                        //!< Maximum workers
    bool stopping = false;                  //!< Set during shutdown
    AsyncIoControllerPriv* ctrl = nullptr;  //!< Owning controller for logging (not ref'd — controller outlives dispatcher)
    std::unordered_set<QoreProgram*> shutting_down_programs;  //!< Programs being destroyed — skip callbacks
    //! Release items (DT_RELEASE) of Programs being destroyed, released by clearProgramShuttingDown()
    /** Such an item can run the cleanup of a C++ poll operation, which must run neither in the thread that enqueues
        it (an I/O thread, a thread in a method of an HTTP connection, a scan of the garbage collector) nor on a worker
        that can run after the Program's threads were waited for, outside the Program
    */
    std::unordered_map<QoreProgram*, std::vector<AsyncWorkItem>> parked_program_releases;
    std::unordered_map<QoreProgram*, int> active_per_program; //!< Per-program in-flight callback count
    QoreCondition pgm_idle_cond;                //!< Signaled when a program's active count reaches zero
    //! Owners being torn down — skip callbacks; nesting depth, not a flag
    /** flushCallbacksByOwner() can nest for the same owner: releasing a work item's
        references can run a destructor that flushes the same owner again.  A plain set
        would let the inner call's clearOwnerShuttingDown() drop the mark while the outer
        call is still draining, re-opening the barrier it holds.
    */
    std::unordered_map<std::string, int> shutting_down_owners;
    std::unordered_map<std::string, int> active_per_owner;     //!< Per-owner in-flight callback count
    QoreCondition owner_idle_cond;              //!< Signaled when an owner's active count reaches zero
    QoreThreadLock qore_continue_poll_lock;     //!< Serializes Qore-level continuePoll() dispatch

    //! Enqueue a work item, starting a worker if needed
    DLLLOCAL void enqueue(AsyncWorkItem&& item);

#ifdef DEBUG
    //! Debug-only: holds @a item, taking it over, if the dbg_async_io_hold_module_cleanup_work() test hook holds the
    //! onComplete() work items that the module cleanup hands over; returns true if it is held (qore#6928)
    DLLLOCAL bool dbgHoldModuleCleanupWorkItem(AsyncWorkItem& item);

    //! re-queues the items held
    friend void qore_dbg_release_held_module_cleanup_work(const char* by, bool end_hold);
#endif

    //! Releases every reference held by a work item that will never be dispatched
    /** Must be called with the dispatcher lock NOT held: these derefs can run Qore
        destructors that re-enter the dispatcher (for example an
        HttpClientConnectionManager destructor calling flushCallbacksByOwner()),
        which would deadlock on the non-recursive dispatcher lock.

        @param item the work item to release; all of its reference fields are cleared
        @param xsink exception sink for the derefs

        @note a DT_CONTINUE_POLL item's controller is told the operation completed so
        the I/O thread drops its cache entry, mirroring what workerLoop() does when it
        pops an item whose owner or program is shutting down
    */
    DLLLOCAL static void releaseWorkItem(AsyncWorkItem& item, ExceptionSink* xsink);

    //! Tells the I/O thread that a discarded step has ended; out of memory ends the process, as on a callback worker
    /** A caller of cancelByProgram() can be waiting for this result
    */
    DLLLOCAL QORE_INTERN_NOINLINE QORE_INTERN_COLD static void sendDiscardedStepResult(AsyncIoControllerPriv* ctrl,
            const std::string& key, uint64_t op_id, int thread_idx) noexcept;

    //! Worker thread entry point
    DLLLOCAL static void workerEntry(ExceptionSink* xsink, void* arg);

    //! Worker thread main loop
    DLLLOCAL void workerLoop(ExceptionSink* xsink);
};

// hashdecl pointers
DLLEXPORT extern const TypedHashDecl* hashdeclSocketPollOperationInfo;
DLLEXPORT extern const TypedHashDecl* hashdeclSocketPollResultInfo;

#ifdef DEBUG
//! Debug builds: if true, the close wakes of registration holds cannot record their socket, as when there is no
//! memory (see AsyncIoControllerPriv::makeCloseWake()); for the C++ unit tests of the fallback that wakes all of the
//! I/O thread's operations
DLLLOCAL extern std::atomic<bool> qore_dbg_close_wake_push_fails;
//! Debug builds: if true, the I/O thread cannot move the sockets of its close wake queue to its wake set, as when
//! there is no memory; for the C++ unit tests of the fallback that wakes all of the I/O thread's operations
DLLLOCAL extern std::atomic<bool> qore_dbg_close_wake_drain_fails;
//! Debug builds: the number of times that the I/O thread's fallback that wakes all of its operations fails, as when
//! there is no memory, before it runs; for the C++ unit tests of its retry
DLLLOCAL extern std::atomic<int> qore_dbg_close_wake_all_fails;
#endif

//! C++ async I/O controller - direct translation of AsyncSocketIoController.qc
/** This class implements the core async I/O event loop used by HTTP/2 server,
    HTTP/2 client multiplexing, WebSocket, and SSE.

    Thread safety: all public methods are thread-safe. Internal state is protected
    by a single mutex with careful lock ordering to avoid deadlocks.

    @since %Qore 3.0
*/
class AsyncIoControllerPriv : public AbstractPrivateData {
#ifdef DEBUG
    friend class AsyncIoAbandonedCommandLockTest;
    friend class AsyncIoErrorCheckLockTest;
    friend class AsyncIoCloseTest;
    friend class AsyncIoDescriptorHoldTest;
    friend class AsyncIoOpCacheTest;
    friend class AsyncIoAbandonedSubmitTest;
    friend class AsyncIoRegistrationCloseTest;
    friend class AsyncIoClosedPollableTest;
    friend class AsyncIoStaleReadinessTest;
    friend class AsyncIoStoppingThreadTest;
    friend class AsyncIoDispatcherCancelTest;
    friend class AsyncIoReleaseCountedScopeTest;
    friend class AsyncIoPreservedSubmitTest;
    friend class AsyncIoFrameSendTest;
#endif
    friend class QoreCallDispatcher;  // for enqueueContinuePollResult from worker thread

public:
    //! Makes the I/O threads of the controller external-lifecycle threads; called for the global controller
    /** The cleanup at process exit stops the global controller and then waits for the external-lifecycle threads
        only, so every I/O thread of the global controller must be one: also a thread that it starts during the
        cleanup, as stop() does to deliver its quit command when an I/O thread has stopped while another one runs.
        That the controller is the global one cannot tell this then, as the cleanup gives up the global controller
        before it stops it.
    */
    DLLLOCAL void setExternalLifecycleThreads() {
        AutoLocker al(m);
        external_lifecycle_threads = true;
    }

    //! Default I/O operation timeout (30 seconds)
    static constexpr int64 DEFAULT_IO_TIMEOUT_US = 30000000LL;

    //! The maximum number of I/O threads a controller runs by default
    /** By default a controller runs one I/O thread per CPU available to the process, up to this number; the
        \c QORE_IO_THREADS environment variable and @ref setMaxIoThreads() set any positive count.  Beyond a few
        threads the gain for one process is small, while operations that involve every I/O thread (such as
        cancelByOwner()) cost more with each thread, and a host often runs several %Qore processes, each with its
        own controller.

        @since %Qore 3.0
    */
    static constexpr int DEFAULT_MAX_IO_THREADS = 8;

    //! Returns the default number of I/O threads
    /** One per CPU available to the process, at most @ref DEFAULT_MAX_IO_THREADS, and at least 1.  The CPUs
        available are those of the process's CPU affinity mask where supported (otherwise
        std::thread::hardware_concurrency()), and on Linux at most the CPU time its cgroups allow (see
        @ref getCgroupCpuLimit()), as in a container started with a CPU limit.

        @since %Qore 3.0
    */
    DLLLOCAL static int getDefaultIoThreadCount();

    //! Reads a file's contents; returns false if it cannot be read
    typedef std::function<bool(const std::string& path, std::string& contents)> file_reader_t;

    //! Returns the number of CPUs that a cgroup v2 \c cpu.max value allows, or -1 if it sets no limit
    /** @param cpu_max the contents of a \c cpu.max file: \c "<quota> <period>", or \c "max <period>" for no limit

        @return the quota divided by the period, rounded up, and at least 1; -1 for no limit or a value that cannot
        be parsed
    */
    DLLLOCAL static int parseCgroupV2CpuMax(const std::string& cpu_max);

    //! Returns the number of CPUs that cgroup v1 \c cpu.cfs_quota_us and \c cpu.cfs_period_us values allow
    /** @param quota the contents of a \c cpu.cfs_quota_us file; \c -1 means no limit
        @param period the contents of a \c cpu.cfs_period_us file

        @return the quota divided by the period, rounded up, and at least 1; -1 for no limit or values that cannot
        be parsed
    */
    DLLLOCAL static int parseCgroupV1CpuQuota(const std::string& quota, const std::string& period);

    //! Returns the number of CPUs that the cgroups of a process allow, or -1 if they set no limit
    /** The limit is the lowest set by the process's cgroup or any of its parents in the cgroup v2 hierarchy
        (\c cpu.max) and the cgroup v1 \c cpu hierarchy (\c cpu.cfs_quota_us / \c cpu.cfs_period_us); a file that
        cannot be read or parsed sets no limit.

        @param proc_cgroup the contents of \c /proc/self/cgroup
        @param mountinfo the contents of \c /proc/self/mountinfo
        @param reader reads the cgroup files

        @return the lowest limit, or -1 if no cgroup sets one
    */
    DLLLOCAL static int getCgroupCpuLimit(const std::string& proc_cgroup, const std::string& mountinfo,
            const file_reader_t& reader);

    //! Entries the timeout heap can hold beyond twice the number of operations before it is compacted
    /** See the timeout heap compaction in ioThread(): stale entries of finished operations are only removed
        lazily, so the heap is rebuilt from the cache when it grows beyond this bound.

        @since %Qore 3.0
    */
    static constexpr size_t TIMEOUT_HEAP_COMPACT_SLACK = 64;

    //! Autostop grace period (2 seconds)
    /** When the cache empties and autostop is enabled, the I/O thread waits
        this long before exiting.  This avoids unnecessary stop/restart cycles
        when operations are submitted in rapid succession (e.g., between test
        cases or when creating multiple HTTP servers).  Without the grace
        period, the I/O thread exits immediately and the next submit() must
        wait for the old thread to finish cleanup before starting a new one,
        adding latency that can cause QUIC handshake timeouts on
        resource-constrained systems.

        @since %Qore 3.0
    */
    static constexpr int64 AUTOSTOP_GRACE_US = 2000000LL;
    //! Returns the idle period before an I/O thread stops (AUTOSTOP_GRACE_US; a test can change it in a debug build)
    DLLLOCAL static int64 autostopGraceUs();


    //! Creates the controller
    /** @param autostop if true, the I/O thread will stop when all operations complete
        @param xsink for exception handling
    */
    DLLLOCAL AsyncIoControllerPriv(bool autostop, ExceptionSink* xsink);

    //! Destructor
    DLLLOCAL virtual ~AsyncIoControllerPriv();

    //! returns true if the controller is used in a child process created with fork() after it was created
    /** Its I/O threads, workers and waiters do not exist in the child, and its locks can be held by them, so the
        controller cannot be used, stopped or destroyed there: its operations raise an exception, its waits and its
        stop return at once, and it is not freed (see QoreForkHandlers.h).  The default controller is replaced in
        the child (see qore_get_async_io_controller_obj()).
    */
    DLLLOCAL bool forked() const {
        return fork_generation != qore_fork_get_generation();
    }

    //! raises ASYNC-IO-ERROR and returns -1 if the controller is used in a child process (see forked())
    DLLLOCAL int checkForked(const char* method, ExceptionSink* xsink) const;

    //! Submit an async operation
    /** @param self the QoreObject wrapping this controller
        @param info the SocketPollOperationInfo hash
        @param replace if true, cancel any existing operation with the same key
        @param xsink for exception handling
        @param thread_idx if not nullptr, receives the index of the I/O thread the operation was submitted to
        @param key_out if not nullptr, receives the cache key of the operation (the custom key or the socket's hash)
        @return a Queue object for receiving the result (or nullptr if callback was provided)
    */
    DLLLOCAL QoreObject* submit(QoreObject* self, QoreHashNode* info, bool replace, ExceptionSink* xsink,
        int* thread_idx = nullptr, std::string* key_out = nullptr,
        std::shared_ptr<AsyncIoSyncRelease> sync_release = std::shared_ptr<AsyncIoSyncRelease>());

    //! Execute an async operation synchronously
    /** @param self the QoreObject wrapping this controller
        @param info the SocketPollOperationInfo hash
        @param replace if true, cancel any existing operation with the same key
        @param xsink for exception handling
        @return the SocketPollResultInfo hash

        @note the references that the controller drops for the operation before the wait ends are released on the
        calling thread when it ends (see AsyncIoSyncRelease): a destructor that releasing the last reference to the
        operation's object runs, runs on the calling thread, and its exception is raised in @a xsink
    */
    DLLLOCAL QoreHashNode* exec(QoreObject* self, QoreHashNode* info, bool replace, ExceptionSink* xsink);

    //! Cancel an operation by socket
    /** @param sock the socket to cancel
        @param xsink for exception handling
        @return true if an operation was found and canceled
    */
    DLLLOCAL bool cancel(AbstractPollableIoObjectBase* sock, ExceptionSink* xsink);

    //! Cancel all operations for a socket and close it on the controller thread
    /** @param sock the socket to cancel and close
        @param xsink for exception handling
        @return true if at least one operation was found and canceled
    */
    DLLLOCAL bool cancelAndClose(AbstractPollableIoObjectBase* sock, ExceptionSink* xsink);

    //! Close a socket on the controller thread after canceling controller work for it
    /** @param sock the socket to close
        @param xsink for exception handling
        @return 0 on success, -1 on error
    */
    DLLLOCAL int close(AbstractPollableIoObjectBase* sock, ExceptionSink* xsink);

    //! Cancel an operation by key
    /** @param key the operation key
        @param xsink for exception handling
        @return true if an operation was found and canceled
    */
    DLLLOCAL bool cancelByKey(const QoreStringNode* key, ExceptionSink* xsink);

    //! Returns the index of the calling thread among this controller's I/O threads, or -1 if it is not one of them
    /** A thread that is an I/O thread of another controller also gets -1.
    */
    DLLLOCAL int ownIoThreadIndex() const;

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

#ifdef DEBUG
    //! Debug-only: wakes the operations of every socket that has a route to an I/O thread
    /** Each operation runs again and computes its timers anew; used after the QUIC test clock has been advanced
        (see qore_quic_test_advance_clock()), so that timers that are due on the new clock are handled at once instead
        of when the wait computed with the old clock ends

        @return the number of sockets woken
    */
    DLLLOCAL size_t dbgWakeAllSockets();

    //! Debug-only: waits until the worker threads have run every item they were given (see dbgWaitAllIdle())
    /** The operations implemented in %Qore, and their onComplete() callbacks, run in worker threads, which
        waitForProcessing() does not wait for: it covers the commands given to the I/O threads only

        @param timeout_ms the maximum time to wait
        @param on_complete_dispatched set to the number of onComplete() calls dispatched so far, with which a caller
        sees whether an operation completed (and could have been submitted again) while it waited

        @return true if the worker threads are idle, false if they were not when \a timeout_ms elapsed
    */
    DLLLOCAL bool dbgWaitWorkersIdle(int timeout_ms, int64_t& on_complete_dispatched);

    //! Debug-only: the number of onComplete() calls dispatched so far; 0 if no dispatcher has been created
    DLLLOCAL int64_t dbgOnCompleteDispatched();

    //! Debug-only: sets or clears the shutdown mark of an owner, as flushCallbacksByOwner() does around its wait
    /** For tests of the work of an owner that is shutting down, which cannot otherwise hold the mark at a given point
    */
    DLLLOCAL void dbgSetOwnerShuttingDown(const std::string& owner, bool mark);

    //! Debug-only: waits until a thread waits in a flush of the callbacks (see QoreCallDispatcher::dbgWaitIdleWaiter())
    DLLLOCAL bool dbgWaitFlushBlocked(int timeout_ms);

    //! Debug-only: waits until a thread waits in a flush of the callbacks of an owner (see
    //! QoreCallDispatcher::dbgWaitOwnerIdleWaiter())
    DLLLOCAL bool dbgWaitOwnerFlushBlocked(int timeout_ms);

    //! Debug-only: returns the generation of the work that added the timer (TimerInfo::adder_gen), or -1 if there is
    //! no timer with the ID
    DLLLOCAL int64 dbgTimerAdderGen(int64_t id);

    //! Debug-only: holds every I/O thread before it drains its command queue (True), or releases them (False)
    /** A held thread processes no command, so the commands pushed meanwhile are processed in order when it is
        released; it runs the actions of dbgRunOnHeldIoThread() while it is held.  Stopping the controller releases
        it (qore#7010)
    */
    DLLLOCAL void dbgHoldIoCommands(bool hold);

    //! Debug-only: waits until the first I/O thread is held before its command queue; false if the wait timed out
    DLLLOCAL bool dbgWaitIoCommandsHeld(int timeout_ms);

    //! Debug-only: runs \a action on the first I/O thread while it is held before its command queue, before it
    //! processes the commands queued, and waits for it to complete
    /** @return 0 if the action ran, -1 if the thread is not held, or the wait timed out or was ended by the release of
        the thread, with an exception raised in \a xsink
    */
    DLLLOCAL int dbgRunOnHeldIoThread(std::function<void(ExceptionSink*)> action, int timeout_ms,
        ExceptionSink* xsink);

    //! Debug-only: cancels the operations of the owner on the held first I/O thread, as code there that runs for the
    //! given application (-1 none, 0 trusted code), before the thread processes the commands queued
    DLLLOCAL int dbgCancelOwnerOnIoThread(const std::string& owner, int64 app_id, int timeout_ms,
        ExceptionSink* xsink);

    //! Debug-only: queues a cancel of the operations of the owner, as code that runs for the given application (-1
    //! none, 0 trusted code) requests it on another I/O thread, to the held first I/O thread
    /** The command is processed when the thread is released, after the commands queued before it and before those
        queued after it: it records a canceled-key tombstone for the key of each operation that it cancels, which
        refuses as stale a submission of the key queued after it, as it was pushed before the cancel was processed
        @return 0 if the command was queued, -1 if the thread is not held, with an exception raised in \a xsink
    */
    DLLLOCAL int dbgQueueCancelOwner(const std::string& owner, int64 app_id, ExceptionSink* xsink);

    //! Debug-only: returns true if a submission takes the key of an operation that it may not replace (see
    //! submitTakesKeyOfOther())
    /** For the submission of sandboxed code that runs for no known application, which no Qore code reaches today
    */
    DLLLOCAL static bool dbgSubmitTakesKeyOfOther(bool sandboxed, qore_program_id_t app_id,
            qore_program_id_t other_app_id, bool same_object) {
        PollInfo pinfo;
        pinfo.app_pgm_id = other_app_id;
        // the identities of the objects only are compared
        QoreObject* const obj = reinterpret_cast<QoreObject*>(&pinfo);
        pinfo.spop_obj = same_object ? obj : nullptr;
        bool rv = submitTakesKeyOfOther(sandboxed, app_id, pinfo, obj);
        pinfo.spop_obj = nullptr;
        return rv;
    }
    //! Debug-only: waits until the given number of onComplete() items have been queued since the dispatcher was
    //! created (see QoreCallDispatcher::dbgWaitOnCompleteQueued())
    DLLLOCAL bool dbgWaitOnCompleteQueued(int64_t count, int timeout_ms);
#endif

    //! Wakes a socket operation by QoreObject identity
    /** Looks up the socket's registered hash from registered_sockets to find the
        correct cache entry. This avoids the hash mismatch that occurs when the
        AbstractPollableIoObjectBase pointer changes during QUIC handshake.
        @param sock_obj the socket QoreObject
        @param xsink exception sink
    */
    DLLLOCAL void wakeSocketByObject(QoreObject* sock_obj, ExceptionSink* xsink);

    //! Called by the destructor of the Qore object of a C++ poll operation: the controller cancels the operation
    /** An explicit \c delete runs the destructor of an operation that a controller still holds and polls; the
        operation's state is then released only with its last reference (see the deref() of the operation classes),
        and the controller sees the deletion as a cancellation of the operation:
        - an operation in the cache of an I/O thread is canceled by a CancelDeletedOp command, which each running
          I/O thread receives (the operation does not know its thread); it is not waited for
        - an operation whose submission has not been processed yet is canceled when it is (see attachOp())
        - an operation that is deleted is not continued again (see isOpDeleted())

        Does not block on the I/O threads and runs no Qore code.

        @param op the C++ poll operation of the object that is being deleted
    */
    DLLLOCAL static void opObjectDeleted(SocketPollOperationBase* op);

    //! Registers a submission of a poll operation without a C++ poll operation by its Qore object (qore#6531)
    /** The deletion of the object (qoreOpObjectDeleted()) cancels the operation with this controller; the
        registration ends with the operation's cache entry (PollInfo::cleanup()) or the submission that failed
    */
    DLLLOCAL void registerQoreOp(const QoreObject* obj);

    //! Ends a registration made by registerQoreOp()
    DLLLOCAL static void unregisterQoreOp(const QoreObject* obj);

    //! Returns true if the Qore object of a registered poll operation was deleted
    DLLLOCAL static bool isQoreOpDeleted(const QoreObject* obj);

#ifdef DEBUG
    //! Debug-only: the number of objects in the registry of the operations without a C++ poll operation
    DLLLOCAL static size_t dbgQoreOpRegisteredCount();
#endif

    //! Called when the Qore object of a poll operation is deleted: a submitted operation without a C++ poll operation
    //! is canceled (qore#6531)
    /** Does not wait for the I/O threads and runs no Qore code
    */
    DLLLOCAL static void qoreOpObjectDeleted(const QoreObject* obj);

    //! Wakes the I/O thread of the controller that holds the given C++ poll operation; never raises an exception
    /** See SocketPollOperationBase::wakeIoThread()
    */
    DLLLOCAL static void wakeOpIoThread(SocketPollOperationBase* op);

    //! Releases the given (last) reference of a C++ poll operation on a callback worker of the global controller
    /** Used where releasing the last reference would run the operation's cleanup in a thread that must not run it;
        released in the calling thread if there is no controller (in a child process created with fork())
    */
    DLLLOCAL static void releaseOpOnWorker(SocketPollOperationBase* op);

    //! Returns true if the Qore object of the given C++ poll operation was deleted (see opObjectDeleted())
    /** Called by an I/O thread before each step of an operation; a single atomic load
    */
    DLLLOCAL static bool isOpDeleted(const SocketPollOperationBase* op);

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

    //! Wait until the given I/O threads have processed all operations submitted to them
    /** @param thread_idxs the indexes of the I/O threads to wait for, as returned by submit()
        @param timeout_ms timeout in ms; 0 = wait forever
        @param xsink for exception handling
        @return true if all operations submitted to the given threads have been processed
    */
    DLLLOCAL bool waitForProcessing(const std::set<int>& thread_idxs, int timeout_ms, ExceptionSink* xsink);

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

    //! Returns the settings of the controller only: the keys of getInfo() that give no operation of any caller
    DLLLOCAL QoreHashNode* getSettingsInfo(ExceptionSink* xsink);

    //! Sets the logger
    /** @param logger_obj the LoggerInterface Qore object (or nullptr to clear)
        @param xsink for exception handling
    */
    DLLLOCAL void setLogger(QoreObject* logger_obj, ExceptionSink* xsink);

    //! Returns the logger object set with setLogger(), referenced for the caller, or nullptr if none is set
    DLLLOCAL QoreObject* getLogger() const;

    //! Drops timer_callback and timer user-data values (may hold cross-module
    //! QoreObject refs). Safe to call while the controller is still running —
    //! timer firings after this simply see no callback/data.
    DLLLOCAL void clearCrossModuleRefs(ExceptionSink* xsink);

    //! Adds a timer to fire at the given deadline
    /** @param deadline the absolute deadline
        @param udata Qore user data to associate with the timer (referenced)
        @param xsink for exception handling
        @param owner optional owner identifier; when set, the timer's callback dispatch is tracked under
        this owner so it can be drained via flushCallbacksByOwner() (enables deterministic teardown of
        fired-but-not-yet-run timer callbacks)
        @return the timer ID (> 0)
    */
    DLLLOCAL int64_t addTimer(const DateTimeNode* deadline, QoreValue udata, ExceptionSink* xsink,
        const std::string& owner = std::string(), ResolvedCallReferenceNode* cancel = nullptr);

    //! Adds a timer that fires after the given delay, measured on the monotonic clock
    /** As addTimer(), but the deadline is the time of the call plus \a delay_us on the monotonic clock: a step of the
        system clock does not move it, as it moves a timer that addTimer() armed for now plus an interval.

        @param delay_us the delay in microseconds; 0 or less fires at once
        @param udata Qore user data to associate with the timer (referenced)
        @param xsink for exception handling
        @param owner optional owner identifier; see addTimer()
        @return the timer ID (> 0)
    */
    DLLLOCAL int64_t addTimerDelay(int64_t delay_us, QoreValue udata, ExceptionSink* xsink,
        const std::string& owner = std::string(), ResolvedCallReferenceNode* cancel = nullptr);

    //! Adds a timer with the given deadline: since the epoch, or on the monotonic clock if \a monotonic is true
    DLLLOCAL int64_t addTimerIntern(int64_t deadline_us, bool monotonic, const char* method, QoreValue udata,
        ExceptionSink* xsink, const std::string& owner, ResolvedCallReferenceNode* cancel);

    //! Cancels a timer for the module cleanup and runs its cancel callback (TimerInfo::cancel_cb), if any, as the
    //! work of a canceled operation (qore#7116); returns true if the timer was found
    DLLLOCAL bool cancelTimerForModuleCleanup(int64_t id, ExceptionSink* xsink);

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

    //! Marks the Program as shutting down in the current dispatcher, again until the dispatcher is one that was marked
    /** The mark drains and releases the Program's queued items: releasing one can run a destructor that stops the
        controller, which replaces the dispatcher, and makes a delivery, which creates a new one; the new dispatcher is
        marked too, so that no item of the Program is left on an unmarked dispatcher (qore#7226)
    */
    DLLLOCAL void remarkProgram(QoreProgram* pgm, ExceptionSink* xsink);

    //! Cancels for the module cleanup the operations and timers of work of the cleanup that it does not count
    /** Called by the module cleanup when it waits for its work (qore_module_cleanup_drain_work()), on its own thread:
        an operation or timer whose context is delegated but not counted (async_io_module_cleanup_uncounted()), which
        work of the cleanup submitted, would otherwise run its onComplete() or callback after the Program of its
        module has been deleted; canceled for the cleanup, its onComplete() is counted work of the cleanup (qore#7027)

        With \a deadline (the cleanup reached its deadline: qore_module_cleanup_deadline_passed()), all of the work of
        the cleanup that it has not canceled yet, counted or not (async_io_module_cleanup_cancel_match()); the names of
        the operations are then appended to \a names, if given

        @return the number of operations and timers canceled
    */
    DLLLOCAL size_t cancelModuleCleanupUncounted(bool deadline, std::vector<std::string>* names,
            ExceptionSink* xsink);

    //! Raises ASYNC-IO-SHUTDOWN-ERROR and returns -1 if the current thread cannot start new asynchronous work
    /** Called by the %Qore-language entry points that start asynchronous work (AsyncIoController::submit(),
        addTimer*(), submitTask()): the work of an operation that the module cleanup canceled cannot start any, as the
        cleanup would wait for it in turn; nor can the work that the cleanup handed over add a timer (\a timer) after
        its deadline; synchronous I/O and AsyncIoController::exec() are not refused (qore#7027)
    */
    DLLLOCAL static int checkModuleCleanupNewWork(const char* method, bool timer, ExceptionSink* xsink);


    //! Custom deref with ExceptionSink
    DLLLOCAL virtual void deref(ExceptionSink* xsink);

    //! Runs a native task on a callback worker, off the async I/O execution path
    /** @param task the task (ownership transferred); discarded instead of run if it cannot be dispatched

        @since %Qore 3.0
    */
    DLLLOCAL void dispatchNativeTask(AsyncIoNativeTask* task);

private:
    //! the fork generation of the process that created the controller (see forked())
    const unsigned fork_generation = qore_fork_get_generation();
    //! I/O thread commands
    enum class IoCommand {
        SubmitOp,            //!< Submit a new operation (carries PollInfo data)
        Cancel,
        CancelSocket,        //!< Cancel all operations associated with a socket hash
        CloseSocket,         //!< Close an I/O object on an I/O thread
        CancelAndCloseSocket,  //!< Cancel all operations of a socket on its I/O thread, then close it there
        CancelOwner,
        CancelByProgram,     //!< Cancel all operations belonging to a QoreProgram
        Quit,
        WakeSocket,          //!< Targeted wake: re-poll a specific socket's operation
        AddTimer,
        CancelTimer,
        ContinuePollResult,  //!< Result from async continuePoll() dispatch
        GetInfo,             //!< Snapshot cache info on the I/O thread
        //! Cancel the operation of a C++ poll operation whose Qore object was deleted (see opObjectDeleted())
        CancelDeletedOp,
        //! Cancel the operation of a poll operation without a C++ poll operation (one implemented in Qore) whose Qore
        //! object was deleted (see qoreOpObjectDeleted(); qore#6531)
        CancelDeletedObj,
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

    //! Returns true if the current thread is the module cleanup or runs work it handed over
    /** (qore_module_cleanup_delegation()); taken by each Command when it is created on the requesting thread
    */
    DLLLOCAL static bool commandModuleCleanupDelegation();

    //! Command queue entry
    struct Command {
        IoCommand cmd = IoCommand::WakeSocket;
        //! True if the thread that created the command is the module cleanup or runs work that it handed over
        /** An operation that the command cancels is then canceled for the cleanup: what the operation runs after
            that is part of the cleanup (see doCancelIntern() and qore_close_module_programs())
            Counted until the command is destroyed, after it has been processed (QoreModuleCleanupWorkFlag): the module
            cleanup waits for it, and for the work that it hands over in turn (qore#6928)
        */
        QoreModuleCleanupWorkFlag module_cleanup_delegate{commandModuleCleanupDelegation()};
        std::string key;                //!< For Cancel / ContinuePollResult / SubmitOp: the cache key
        //! For Cancel: if set, only the operation of this object is canceled (another operation can have taken the key)
        /** Compared by address only; the canceling thread holds a reference to the object until the command has been
            processed, so the address cannot be reused meanwhile
        */
        const QoreObject* cancel_spop_obj = nullptr;
        //! For Cancel / CancelOwner: the operations that the canceling code may cancel (qore#7010)
        AioAppRestriction restrict_app;
        //! For SubmitOp: the application that the operation is submitted for (see callerAppId())
        qore_program_id_t submit_app_id = 0;
        //! For SubmitOp: true if the submitting code is sandboxed: it does not take the key of another application's
        //! operation (qore#7010)
        bool submit_sandboxed = false;
        std::string owner;              //!< For CancelOwner / SubmitOp: the owner string
        //! For SubmitOp: the owner of the poll op whose continuePoll() was
        //! running on this (worker) thread when submit() was called — the
        //! op's *inherited* cancel scope.  cancelByOwner() of this owner also
        //! cancels the op, so cancelling a poll op cancels async work its
        //! continuePoll() spawned (e.g. a nested waitForNotifier()).  Empty
        //! when submitted outside a continuePoll() dispatch.
        std::string inherited_owner;
        //! For CancelOwner/CancelByProgram/GetInfo: shared completion object.
        /** When non-null, owns one ref on the referenced AsyncOpCompletion.
            The ref is released when the command is processed (normal path
            in processCmd) or cleaned up (abandoned path in
            cleanupAbandonedCommand / I/O-thread-exit drain).  Replaces
            the former raw-pointer group (done_cond / cancel_done_flag /
            pending_threads / cancel_count / info_result / cancel_pinfos)
            — see AsyncOpCompletion docs. */
        AsyncOpCompletion* completion = nullptr;
        //! For CancelByProgram: the caller waits for the operations whose cancellation is deferred until their step's
        //! result and cancels them itself (a caller on an I/O thread cannot wait; qore#6703)
        bool wait_deferred = false;
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
        //! For CancelByProgram without a Program: the module cleanup reached its deadline (see
        //! cancelModuleCleanupUncounted())
        bool module_cleanup_deadline = false;
        int64_t timer_deadline_us = 0;  //!< For AddTimer: absolute deadline in microseconds
        //! For AddTimer: true if timer_deadline_us is on the monotonic clock (addTimerDelay()), not since the epoch
        bool timer_monotonic = false;
        int64_t timer_id = 0;           //!< For AddTimer/CancelTimer: timer ID
        QoreHashNode* continue_poll_result = nullptr;  //!< For ContinuePollResult: new poll info (or nullptr)
        QoreHashNode* continue_poll_ex = nullptr;      //!< For ContinuePollResult: exception (or nullptr)
        bool continue_poll_completed = false;           //!< For ContinuePollResult: true if completed
        //! For ContinuePollResult: true if the worker read the operation's state for its state events
        bool continue_poll_has_state = false;
        //! For ContinuePollResult: the operation's state after the step, if continue_poll_has_state
        std::string continue_poll_state;
        //! For ContinuePollResult: the ID of the operation that made the step
        /** The result is applied to the operation with this ID only: the entry with the same key can belong to
            another operation by the time the result arrives (see OpCache).
        */
        uint64_t continue_poll_op_id = 0;
        std::string sock_hash;          //!< For WakeSocket/CancelSocket/CloseSocket: socket hash
        AbstractPollableIoObjectBase* close_sock = nullptr; //!< For CloseSocket: referenced I/O object
        bool submit_replace = false;    //!< For SubmitOp: replace existing operation with same key

        //! Per-thread submit_seq snapshot at push time (post-bump under m).
        /** Used by SubmitOp/CancelOwner so the SubmitOp tombstone gate can
            distinguish a fresh submit pushed strictly after a cancel from a
            stale resubmit pushed before the cancel: a SubmitOp whose
            seq_at_push exceeds the recorded cancel seq is accepted. */
        int seq_at_push = 0;

        // --- SubmitOp data (ownership transferred to I/O thread) ---
        QoreObject* submit_sock_obj = nullptr;           //!< Referenced socket object
        AbstractPollableIoObjectBase* submit_sock = nullptr; //!< Referenced socket private data
        QoreObject* submit_spop_obj = nullptr;           //!< Referenced poll operation object
        SocketPollOperationBase* submit_spop_base = nullptr; //!< Referenced C++ poll operation base
        QoreHashNode* submit_poll_info = nullptr;        //!< Referenced initial poll info (or nullptr)
        QoreHashNode* submit_other = nullptr;            //!< Referenced other data (or nullptr)
        Queue* submit_queue = nullptr;                   //!< Referenced result queue (or nullptr)
        QoreObject* submit_queue_obj = nullptr;          //!< Referenced result queue object (or nullptr)
        Queue* submit_state_queue = nullptr;             //!< Referenced state event queue (or nullptr)
        QoreObject* submit_state_queue_obj = nullptr;    //!< Referenced state event queue object (or nullptr)
        std::vector<std::string> submit_notify_states;   //!< The states to report; empty = all
        int64 submit_timeout_us = 0;                     //!< Timeout in microseconds
        bool submit_has_qore_abort = false;              //!< True if abort() is overridden in Qore
        bool submit_has_qore_on_complete = false;        //!< True if onComplete() is overridden in Qore
        bool submit_socket_async_io = false;             //!< True if SubmitOp owns a Socket async I/O claim
        std::string submit_route_sock_hash;              //!< Submit-time wake route/object mapping, if published
        int submit_route_thread_idx = -1;                //!< I/O thread for submit_route_sock_hash
        AsyncIoSandboxContext submit_sandbox_ctx;        //!< For SubmitOp: the submitter's sandbox context
        //! For SubmitOp: the references taken by the synchronous exec() call that waits for the operation, or empty
        std::shared_ptr<AsyncIoSyncRelease> submit_sync_release;
        //! For SubmitOp: work that the module cleanup handed over submitted the operation, not the cleanup itself
        //! (see cancelAdmittedAfterModuleCleanupDeadline(), qore#7027)
        bool submit_module_cleanup_handed_over = false;
        //! For CancelDeletedOp: the C++ poll operation whose Qore object was deleted (referenced)
        /** The reference keeps the address from being reused for another operation until the command is processed
        */
        SocketPollOperationBase* cancel_spop_base = nullptr;
        //! For CancelDeletedObj: the Qore object of the operation whose object was deleted (referenced; qore#6531)
        QoreObject* deleted_spop_obj = nullptr;
        //! For SubmitOp: the operation, which has no C++ poll operation, is registered by its Qore object (see
        //! registerQoreOp())
        bool submit_qop_registered = false;
    };

    //! A cancellation that a CancelByProgram command deferred until the result of the operation's step (qore#6703)
    /** Held by the deferred PollInfo (shared by its copies).  When the step's result arrives, the I/O thread hands the
        operation back to the caller of cancelByProgram(), which waits for it and cancels it as it cancels the Program's
        other operations; when the last copy goes (also at the I/O thread's exit, which delivers the cancellation
        itself), the caller is released
    */
    struct AsyncOpDeferredCancel {
        AsyncOpCompletion* completion;

        DLLLOCAL explicit AsyncOpDeferredCancel(AsyncOpCompletion* c) : completion(c) {
            completion->addDeferred();
            completion->ROreference();
        }

        DLLLOCAL ~AsyncOpDeferredCancel() {
            completion->completeDeferred();
            completion->deref();
        }

        AsyncOpDeferredCancel(const AsyncOpDeferredCancel&) = delete;
        AsyncOpDeferredCancel& operator=(const AsyncOpDeferredCancel&) = delete;
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
        //! the application that the operation was submitted for (see callerAppId(), qore#7010)
        qore_program_id_t app_pgm_id = 0;
        //! Inherited cancel scope: the owner of the poll op whose
        //! continuePoll() spawned this op (empty if none).  cancelByOwner()
        //! matches this in addition to @ref owner so cancelling a poll op
        //! also cancels nested async work (e.g. a blocked waitForNotifier()).
        std::string inherited_owner;
        QoreHashNode* other;            //!< Free-form data (referenced, or nullptr)
        Queue* queue;                   //!< Result queue (referenced, or nullptr)
        //! The object of \c queue (referenced, or nullptr); results are pushed with it, see deliverResult()
        QoreObject* queue_obj = nullptr;
        //! State event queue (referenced, or nullptr); see noteOperationState() and deliverStateEnd()
        Queue* state_queue = nullptr;
        //! The object of \c state_queue (referenced, or nullptr); events are pushed with it, see pushStateEvent()
        QoreObject* state_queue_obj = nullptr;
        //! The states to report on \c state_queue; empty = every change
        std::vector<std::string> notify_states;
        //! The state read after the operation's last step, if \c state_seen
        std::string last_state;
        //! True once the operation's state has been read after a step
        bool state_seen = false;
        //! True once the last state event (\c done) has been pushed; it is pushed exactly once
        bool state_end_delivered = false;
        SocketPollOperationBase* spop_base; //!< C++ poll operation (referenced, or nullptr)
        //! The ID of the attachment of spop_base that this entry made (see attachOp()), or 0
        /** Only the entry that made the current attachment detaches the operation
        */
        uint64_t spop_attach_id = 0;
        //! true if the operation, which has no C++ poll operation, is registered by its Qore object (see
        //! registerQoreOp(); qore#6531): the entry unregisters it when it is cleaned up
        bool qop_registered = false;
        bool has_qore_abort;            //!< True if abort() is overridden in Qore
        bool has_qore_on_complete;      //!< True if onComplete() is overridden in Qore
        //! Whether the class of the operation has an onPollComplete() method: -1 = not looked up yet, 0 = no, 1 = yes
        /** onPollComplete() is called when a step of the operation pushed items
            (SocketPollOperationBase::getAndClearItemsPushed()); an operation without the method is not notified
            (qore#6804).  The method is looked up by the I/O thread the first time a step of the operation pushed
            items, so that the operations that never push items (most of them) do not pay for the lookup.
        */
        int8_t poll_complete = -1;
        //! True once a terminal completion (goal/timeout/cancel/error) has been
        //! committed for delivery to \c queue / onComplete.  Enforces the
        //! deliver-exactly-once invariant: every PollInfo that owns a result
        //! \c queue delivers exactly one completion before the queue ref is
        //! dropped.  Set at the real delivery funnels (doCancelIntern, Phase 3
        //! deferred delivery); the cleanup() backstop synthesizes a canceled
        //! completion when this is still false at teardown so a waiting
        //! Queue::get() can never block forever on a lost completion.
        bool completion_delivered;
#ifdef DEBUG
        //! Debug-only: true if the operation is counted for its socket's debug hook (dbgNoteControllerOp())
        bool dbg_counted = false;
#endif
        bool continue_poll_in_flight;   //!< True when continuePoll() dispatched to worker
        bool socket_async_io;           //!< True if this operation owns a Socket async I/O claim
        int64 poll_timeout_deadline_us; //!< Absolute deadline for protocol-level poll timeout (QUIC)
        //! The earliest deadline of this operation's entries in the timeout heap; 0 if it has none
        /** See armTimeoutHeap(): an entry is only pushed for a deadline earlier than this one, so the heap holds one
            or two entries per operation instead of one per continuePoll() call.
        */
        int64 heap_deadline_us = 0;
        std::string cached_sock_hash;   //!< Cached socket hash for O(1) Phase 1 readiness check
        int cached_events = 0;          //!< Cached poll events for Phase 3 fast path
        uint32_t cached_fd_gen = 0;     //!< Cached fd generation for QUIC migration detection
        uint32_t socket_wait_fd_generation = 0; //!< Socket fd generation snapshot for current wait
        int socket_wait_fd = -1;        //!< Socket fd snapshot for current wait
        bool socket_wait_generation_valid = false; //!< True when socket wait snapshot is usable
        std::string submit_route_sock_hash; //!< Submit-time wake route/object mapping, if published
        int submit_route_thread_idx = -1;   //!< I/O thread for submit_route_sock_hash
        //! Cached socket QoreObject* used as a cheap pointer-compare in Phase 3
        /** Set whenever updateEventLoopRegistration is called.  Used in the fast
            path to detect when a poll op transitions to polling a different
            socket (e.g. SocketAcceptPollOperation moving from the listener fd
            to the accepted client fd when SSL_accept returns WANT_READ).  The
            socket hash and events both stay the same in that case, so without
            an identity check the new client fd would never get registered
            with epoll/kqueue and the SSL handshake would hang.  Not ref'd —
            registered_sockets[key] holds the live reference; this is just an
            identity tag.
        */
        QoreObject* cached_sock_obj = nullptr;
        uint64_t last_queued_gen = 0;   //!< Phase 1 generation when last queued (duplicate prevention)
        //! The number of the operation's last waits in a row for events of an object that has no descriptor
        /** The object was closed before its descriptor could be registered; see updateEventLoopRegistration() and
            makeDescriptorlessWaitException()
        */
        unsigned descriptorless_waits = 0;
        //! The ID the operation cache gave this operation; see OpCache
        uint64_t op_id = 0;
        //! The sandbox context of the submitter, applied to the operation's steps on callback workers
        AsyncIoSandboxContext sandbox_ctx;
        //! The references taken by the synchronous exec() call that waits for the operation, or empty
        std::shared_ptr<AsyncIoSyncRelease> sync_release;
        //! the callers of the CancelByProgram commands that wait until the operation's cancellation, deferred until the
        //! result of its step, has run (see AsyncOpDeferredCancel): the command that deferred it, or one that found it
        //! deferred by an earlier cancellation
        std::vector<std::shared_ptr<AsyncOpDeferredCancel>> deferred_cancels;
        //! the operation's native abort() has run on its I/O thread, before a socket cancel that waited for the step
        //! ended (see handBackDeferredCancel())
        bool abort_called = false;
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
            completion_delivered(false),
            continue_poll_in_flight(false), socket_async_io(false), poll_timeout_deadline_us(0) {
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
        //! The operations of a CancelByProgram without a Program (cancelModuleCleanupUncounted()) whose cancel waits
        //! for their step in flight (pending_aborts), which are canceled too (qore#7027)
        std::atomic<size_t> in_flight_canceled{0};
        //! The names of the operations of in_flight_canceled at the deadline of the module cleanup, guarded by
        //! pinfos_lock (moduleCleanupOpName())
        std::vector<std::string> in_flight_names;
        //! The operations whose cancellation was deferred until their step's result, handed back to the caller of
        //! cancelByProgram(), which cancels them (qore#6703); guarded by pinfos_lock
        std::vector<PollInfo> deferred_pinfos;
        //! Protects cancel_pinfos against concurrent appends from I/O threads
        QoreThreadLock pinfos_lock;

        //! The operations of a CancelByProgram command whose step was in flight, whose cancellation runs when the
        //! step's result reaches their I/O thread (pending_aborts); guarded by deferred_lock, not by the controller's
        //! mutex: the holder that counts one is released wherever the deferred PollInfo goes (qore#6703)
        int deferred = 0;
        QoreThreadLock deferred_lock;
        //! Signaled when the last deferred cancellation has run
        QoreCondition deferred_cond;

        //! Counts a deferred cancellation (see AsyncOpDeferredCancel)
        DLLLOCAL void addDeferred() {
            AutoLocker al(deferred_lock);
            ++deferred;
        }

        //! Ends a deferred cancellation; signals the waiter if it was the last
        DLLLOCAL void completeDeferred() {
            AutoLocker al(deferred_lock);
            assert(deferred > 0);
            if (!--deferred) {
                deferred_cond.broadcast();
            }
        }

        //! Returns true if a cancellation is still deferred
        DLLLOCAL bool hasDeferred() {
            AutoLocker al(deferred_lock);
            return deferred > 0;
        }

        //! Waits until the deferred cancellations have been handed back; the controller's mutex must not be held
        DLLLOCAL void waitDeferred() {
            AutoLocker al(deferred_lock);
            while (deferred) {
                deferred_cond.wait(deferred_lock);
            }
        }

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
        QoreObject* queue_obj = nullptr;   //!< The object of queue; referenced or nullptr
        QoreObject* spop_obj;              //!< Referenced or nullptr (for onComplete dispatch)
        bool has_on_complete;              //!< True if spop has onComplete() override
        QoreHashNode* result;              //!< Referenced
        std::string owner;                 //!< Owner identifier for per-owner flush tracking
        AsyncIoSandboxContext sandbox_ctx; //!< The submitter's sandbox context, for onComplete()
        //! The references taken by the synchronous exec() call that waits for the result, or empty
        std::shared_ptr<AsyncIoSyncRelease> sync_release;
        //! the application of the operation (qore#7010)
        qore_program_id_t app_pgm_id = 0;
    };

    //! Delivers the result of an operation whose abort() runs on a callback worker, after abort() has returned
    /** A canceled or timed-out operation is aborted before its result is delivered, so that the receiver of the
        result (its onComplete() or the waiter on its result queue) sees the operation in its aborted state.  When
        abort() runs on a callback worker (an abort() implemented in Qore, or one that needsWorkerDispatch()), the
        result is delivered by this task on the same worker once abort() has returned: delivered separately, the
        result could be handled on another worker before, or while, abort() runs.

        Run on the worker, the task pushes the result to the operation's result queue and calls its onComplete()
        itself, in the work item of abort() (under the same Program, sandbox and owner, after the cancellation and the
        thread-local data of abort() have been cleared, as between two items), so that a stop of the dispatcher, which
        waits for the item, also waits for onComplete().  A task that is not run pushes the result to the result
        queue only, as onComplete() would be discarded as a work item of its own: when the item itself is discarded
        (the dispatcher is stopping, or the operation's Program or owner is shutting down; on the I/O thread, the
        task is discarded there, so the result does not wait for a worker), and when the operation's Program or
        owner has begun to shut down while abort() ran.

        As the result is delivered only once abort() has returned, an abort() run on a worker must not wait for the
        receiver of the result (see AbstractPollOperation::abort()).
    */
    class AbortThenDeliverTask : public AsyncIoNativeTask {
    public:
        //! Takes over the references of @a dd
        /** @param ctrl the controller, not referenced: the dispatcher that runs or discards the task is stopped,
            and so the task is run or discarded, before the controller is deleted
            @param dd the delivery
        */
        DLLLOCAL AbortThenDeliverTask(AsyncIoControllerPriv* ctrl, DeferredDelivery&& dd);

        DLLLOCAL ~AbortThenDeliverTask() override;

        DLLLOCAL void run(ExceptionSink* xsink) override {
            deliver(true, xsink);
        }

        DLLLOCAL void discard(ExceptionSink* xsink) override {
            deliver(false, xsink);
        }

        DLLLOCAL QoreProgram* getProgram() const override {
            return dd.spop_obj ? dd.spop_obj->getProgram() : nullptr;
        }

    private:
        AsyncIoControllerPriv* ctrl;
        DeferredDelivery dd;
        bool delivered = false;

        //! Delivers the result once, and calls onComplete() if @a call_on_complete is true
        DLLLOCAL void deliver(bool call_on_complete, ExceptionSink* xsink);
    };

    //! An abort() dispatched to a callback worker with the delivery of the result, after the cache is updated
    struct DeferredAbort {
        QoreObject* spop_obj;               //!< Referenced
        std::string owner;                  //!< Owner identifier for per-owner flush tracking
        AsyncIoSandboxContext sandbox_ctx;  //!< The submitter's sandbox context
        AsyncIoNativeTask* after_abort;     //!< Owned, or nullptr
        bool after_abort_on_complete;       //!< True if after_abort calls onComplete() when it is run
        qore_program_id_t app_pgm_id = 0;   //!< the application of the operation (qore#7010)
    };

    //! Timer info stored under lock
    struct TimerInfo {
        QoreValue udata;                //!< Referenced Qore user data
        //! The code (referenced) run when the module cleanup cancels the timer, or NOTHING (qore#7116)
        /** So that an owner that counts its scheduled timers can balance the count of a timer that the cleanup
            cancels at the end of the process, whose callback never runs; not run when the owner cancels the timer,
            when it fires or when the controller stops
        */
        QoreValue cancel_cb;
        std::string owner;              //!< Owner identifier for per-owner callback flush (empty if untracked)
        AsyncIoSandboxContext sandbox_ctx; //!< The sandbox of the code that added the timer, for its callback
        //! The run-time parse options of the code that added the timer, which its callback runs with (qore#6208)
        AsyncIoRuntimeOptions runtime_po;
        //! the application that the timer's code runs for (see AsyncIoControllerPriv::callerAppId(), qore#7010)
        qore_program_id_t cb_app_id = 0;
        //! True if module code added the timer for an application with restrictions (see app_pgm_id)
        bool for_app = false;
        //! The generation of the work that added the timer (runtime_get_thread_generation(), which unlike a TID is
        //! never reused, and which a pooled or reused thread takes from the work item that it runs): module code that
        //! runs for no application (the code of a thread that module code started) cancels only the timers for no
        //! application that its own work added (qore#7010)
        uint64_t adder_gen = 0;
        //! The ID of the application Program that module code added the timer for, if for_app is set
        /** Module code running for an application with restrictions can cancel only the timers added for the same
            application; see runtime_get_module_caller_app()
        */
        qore_program_id_t app_pgm_id = 0;
    };

    //! Per-I/O-thread context — each thread owns its own event loop, cache, and socket state
    /** Multiple IoThreadContext instances enable nginx-style per-thread isolation with
        zero lock contention during normal I/O processing.
        @since %Qore 3.0
    */
    //! The operation cache of an I/O thread: operations by cache key, with an index by operation ID
    /** The I/O loop refers to an operation by the ID the cache gives it when it is inserted, not by its key: looking
        up an integer instead of hashing the key, and keeping no copies of the key.  IDs are not reused, so a lookup
        by ID cannot find another operation that has taken over the key of a removed one; an operation that replaces
        itself (the same poll operation submitted again for its key) keeps its ID (see emplaceWithId()).

        Only the I/O thread that owns the cache uses it.
    */
    struct IoThreadContext;

    class OpCache {
        // an operation leaves the cache through its I/O thread's context, so that it cannot leave with registrations
        // by accident; see AsyncIoControllerPriv::retireOp()
        friend struct AsyncIoControllerPriv::IoThreadContext;
        friend class AsyncIoOpCacheTest;
    public:
        using map_t = std::unordered_map<std::string, PollInfo>;
        using iterator = map_t::iterator;
        using const_iterator = map_t::const_iterator;
        using node_t = map_t::value_type;

        DLLLOCAL iterator begin() { return map.begin(); }
        DLLLOCAL iterator end() { return map.end(); }
        DLLLOCAL const_iterator begin() const { return map.begin(); }
        DLLLOCAL const_iterator end() const { return map.end(); }
        DLLLOCAL iterator find(const std::string& key) { return map.find(key); }
        DLLLOCAL const_iterator find(const std::string& key) const { return map.find(key); }
        DLLLOCAL size_t size() const { return map.size(); }
        DLLLOCAL bool empty() const { return map.empty(); }

        //! Returns the operation with the given key, inserting it with a new ID if there is none
        DLLLOCAL PollInfo& operator[](const std::string& key) {
            return emplaceWithId(key, 0);
        }

        //! Returns the operation with the given key, inserting it with the given ID (or a new one if 0) if there is none
        DLLLOCAL PollInfo& emplaceWithId(const std::string& key, uint64_t id) {
            auto [it, inserted] = map.try_emplace(key);
            if (inserted) {
                if (!id) {
                    id = ++last_id;
                }
                assert(by_id.find(id) == by_id.end());
                it->second.op_id = id;
                by_id.emplace(id, &*it);
                node_ids.emplace(&*it, id);
            }
            return it->second;
        }

        //! Removes all operations
        DLLLOCAL void clear() {
            by_id.clear();
            node_ids.clear();
            map.clear();
        }

        //! Returns the operation with the given ID, or nullptr if there is none
        DLLLOCAL node_t* findId(uint64_t id) {
            auto i = by_id.find(id);
            return i == by_id.end() ? nullptr : i->second;
        }

    private:
        //! Removes an operation
        DLLLOCAL iterator erase(iterator it) {
            // the ID is looked up by node: the entry's PollInfo can have been reset before it is removed
            auto nit = node_ids.find(&*it);
            assert(nit != node_ids.end());
            by_id.erase(nit->second);
            node_ids.erase(nit);
            return map.erase(it);
        }

        //! Removes the operation with the given ID, if it is in the cache
        DLLLOCAL void eraseId(uint64_t id) {
            node_t* node = findId(id);
            if (node) {
                erase(map.find(node->first));
            }
        }

        map_t map;
        //! Operations by ID; the nodes of an unordered_map stay where they are until they are removed
        std::unordered_map<uint64_t, node_t*> by_id;
        //! The ID of each operation's node
        std::unordered_map<const node_t*, uint64_t> node_ids;
        //! The last ID given to an operation
        uint64_t last_id = 0;
    };

    //! The sockets of an I/O thread that closes made by other threads woke (qore#7003)
    /** A close of a socket that bypasses the controller (ex: an HTTP/2 abort, the destructor of an HTTP client) can
        replace the registered descriptor with an inert one, which the event loop drops without an event; the close
        adds the socket here and notifies the thread, which runs the socket's operations again (see
        wake_socket_hashes), and they see the close.

        The queue has a lock of its own, which is a leaf: nothing is called and no other lock is taken with it held
        (the close adds the socket with it held and notifies the thread after releasing it; the I/O thread moves the
        sockets to its wake set with it held), so a close can be made by a thread that holds the controller's lock or
        another socket's lock.  The queue is shared by the wakes of the thread's registration holds and keeps a
        reference of the thread's notifier: a close that comes after the thread stopped, or after the controller was
        destroyed, adds the socket to a queue that nothing drains any more and notifies a notifier that does nothing
        once it is closed; only the thread's event loop drains the queue.
    */
    struct CloseWakeQueue {
        QoreThreadLock l;
        std::vector<std::string> hashes;
        //! set when a socket could not be added (no memory): the thread wakes all of its sockets
        bool all = false;
        //! the thread's notifier, referenced; none for a context without one (a context that a test makes)
        QoreEventNotifier* notifier;

        DLLLOCAL explicit CloseWakeQueue(QoreEventNotifier* notifier) : notifier(notifier) {
            if (notifier) {
                notifier->ref();
            }
        }

        DLLLOCAL ~CloseWakeQueue() {
            if (notifier) {
                ExceptionSink xsink;
                notifier->deref(&xsink);
                xsink.clear();
            }
        }

        CloseWakeQueue(const CloseWakeQueue&) = delete;
        CloseWakeQueue& operator=(const CloseWakeQueue&) = delete;
    };

    //! Returns the wake of a registration hold of a socket's descriptor that the I/O thread has registered: it adds
    //! the socket to the thread's close wakes and notifies the thread (see CloseWakeQueue)
    DLLLOCAL static std::function<void()> makeCloseWake(IoThreadContext& t, const std::string& sock_hash);

    //! Returns the socket hash that wakes the operation (see pollsSocket())
    DLLLOCAL static std::string closeWakeKey(const PollInfo& pinfo) {
        if (!pinfo.cached_sock_hash.empty()) {
            return pinfo.cached_sock_hash;
        }
        return pinfo.sock ? pinfo.sock->getIoIdentityHash() : std::string();
    }

    struct IoThreadContext {
        //! Release the event loop and notifier on resize, failed setup, or destruction
        DLLLOCAL ~IoThreadContext();

        //! Takes an operation out of the cache and returns it, leaving what is registered for its ID
        /** Only for an operation whose ID lives on in the operation that replaces it at once in the cache
            (OpCache::emplaceWithId()).  Every other operation leaves with retireOp().
        */
        DLLLOCAL PollInfo takeOpKeepingId(OpCache::iterator it) {
            PollInfo rv = it->second;
            it->second = PollInfo();
            cache.erase(it);
            cache_size.fetch_sub(1, std::memory_order_relaxed);
            return rv;
        }

        //! Takes an operation with no registrations out of the cache and returns it; see retireOp()
        /** Used directly only for a thread that is not running: an I/O thread removes all of its registrations when
            it exits.
        */
        DLLLOCAL PollInfo takeUnregisteredOp(OpCache::iterator it) {
            assert(!hasRegistrations(it->second.op_id));
            return takeOpKeepingId(it);
        }

        //! Removes an operation with no registrations that has been processed in place from the cache
        DLLLOCAL void removeUnregisteredOp(uint64_t op_id) {
            assert(!hasRegistrations(op_id));
            cache.eraseId(op_id);
            cache_size.fetch_sub(1, std::memory_order_relaxed);
        }

        //! Returns true if the event loop has anything registered for the operation with the given ID
        DLLLOCAL bool hasRegistrations(uint64_t op_id) const {
            return key_extra_fds.count(op_id) || registered_sockets.count(op_id);
        }

        MpscQueue<Command> cmdq;          //!< Lock-free command queue for this thread
        //! The submissions (SubmitOp commands) for this thread while it reads no commands (guarded by the lock)
        /** A stopping thread keeps the submissions that it finds in its last command drain here, and a submission
            routed to the thread after that drain or while it is stopped is kept here too; they are moved to the
            command queue when the thread is started again (startIntern()).  Unlike the command queue, the controller
            can see them: cancelByProgram() takes the submissions of its Program and cancels them while the Program is
            still valid, as an operation of the Program must not be admitted after its cancellation (qore#7225), and
            stop() releases them.
        */
        std::vector<Command> preserved_submits;
        std::atomic<bool> running{false};  //!< True when this thread accepts commands
        int tid = 0;                       //!< Thread ID (0 if not running)
        int thread_idx = 0;                //!< Index in io_threads vector
        int64 autostop_idle_since = 0;     //!< Timestamp when cache first became empty
        //! True from this thread's decision to stop until it has stopped (guarded by the controller's lock)
        /** Per thread: with several I/O threads, one thread stops while the others run or stop themselves; a stopping
            thread cannot be started again before it has stopped (see startIntern()).
        */
        bool exiting = false;
        //! True once this stopping thread has drained its command queue for the last time, until it is started again
        /** Guarded by the controller's lock.  The thread has then canceled the operations in its cache and processes no
            more commands: a command pushed to it would never be processed, and a caller waiting for it would wait
            forever, so the fan-outs skip it (see readsCommands()).
        */
        bool commands_closed = false;
        //! True once this stopping thread has run the cancellations of its exit cleanup, until it is started again
        /** Guarded by the controller's lock; set before the thread logs that it has exited, so that cancelByProgram()
            does not wait for that logger call (see anyThreadInExitDrain())
        */
        bool exit_cancel_done = false;
#ifdef DEBUG
        //! Test hook: when set, the stopping thread waits on \c dbg_cond before its last command drain until it is
        //! cleared, so that a command queued meanwhile is found by that drain
        bool dbg_hold_before_drain = false;
        //! Test hook: true while the thread waits before its last command drain
        bool dbg_held_before_drain = false;
        //! Test hook: when set, the thread waits on \c dbg_cond after its last command drain until it is cleared
        bool dbg_hold_after_drain = false;
        //! Test hook: true while the thread waits after its last command drain
        bool dbg_held = false;
        //! Test hook: signaled when \c dbg_held is set, and to release the thread
        QoreCondition dbg_cond;
        //! Test hook: called by the thread with the controller's lock held just before it checks its command queue
        //! for the last time before it polls its event loop; it may push a command to the queue
        std::function<void(IoThreadContext&)> dbg_before_poll;
        //! Test hook: when set, the thread waits on \c dbg_cond before it drains its command queue until it is
        //! cleared, running the actions of \c dbg_actions meanwhile (see dbgHoldIoCommands())
        bool dbg_hold_commands = false;
        //! Test hook: true while the thread waits before it drains its command queue
        bool dbg_commands_held = false;
        //! Test hook: the actions that the held thread runs, on the thread, before it drains its command queue
        std::deque<std::function<void()>> dbg_actions;
#endif

        QoreEventLoop* loop = nullptr;
        QoreEventNotifier* notifier = nullptr;


        //! Operation cache — fully owned by this I/O thread
        OpCache cache;
        std::atomic<int> cache_size{0};   //!< Atomic cache size for lock-free getCacheSize()

        //! Per-thread submit counter — incremented at every cmdq.push() targeting
        //! this thread (and at every direct cache insertion done while running on
        //! this thread).  Used by waitForProcessing() to detect that a worker's
        //! submit has reached this thread's cmdq, so the corresponding processed
        //! check is meaningful.  The earlier global submit_seq counter conflates
        //! submits across threads: thread A snapshotting it after draining its
        //! own cmdq could observe a value that includes pending submits to
        //! thread B's cmdq, advance global processed_seq prematurely, and let
        //! waitForProcessing() return before B's cmd is in cache.
        std::atomic<int> submit_seq{0};
        //! Per-thread processed counter — advanced by this thread once its cmdq
        //! is drained (post-processCommands snapshot at end of iteration; or
        //! pre-poll catch-up when cmdq is empty).  waitForProcessing() requires
        //! every thread's processed_seq to reach the snapshot of its submit_seq
        //! captured at call time.
        std::atomic<int> processed_seq{0};
        std::unordered_map<std::string, int> socket_refcounts;

        //! Socket hashes that need re-polling
        std::unordered_set<std::string> wake_socket_hashes;
        //! The sockets that closes made by other threads woke (see makeCloseWake()); made by the thread when it first
        //! needs it
        std::shared_ptr<CloseWakeQueue> close_wakes;

        //! IDs of the operations needing their first continuePoll (empty cached_sock_hash)
        std::vector<uint64_t> new_entry_ids;

        //! Timeout min-heap entry
        struct TimeoutEntry {
            int64 deadline_us;
            uint64_t op_id;
            bool operator>(const TimeoutEntry& o) const { return deadline_us > o.deadline_us; }
        };
        //! Min-heap for operation/protocol timeouts (lazy deletion)
        std::priority_queue<TimeoutEntry, std::vector<TimeoutEntry>,
            std::greater<TimeoutEntry>> timeout_heap;

        //! Generation counter for Phase 1 duplicate prevention
        uint64_t phase1_gen = 0;

        // Registered socket tracking; operations are identified by their ID (see OpCache), sockets by their hash
        //! The socket each operation is registered for, by operation ID (referenced)
        std::unordered_map<uint64_t, QoreObject*> registered_sockets;
        //! The hash each operation in @ref registered_sockets is registered under
        /** Recorded with the registration, because the object cannot be asked when the registration is removed: a
            deleted object has no private data, and without the hash the registration of its descriptor would stay
        */
        std::unordered_map<uint64_t, std::string> registered_hashes;
        std::unordered_map<std::string, int> registered_events;
        std::unordered_map<std::string, int> registered_fds;
        //! The events each operation waits for, by operation ID
        std::unordered_map<uint64_t, int> key_events;
        //! The IDs of the operations registered for each socket
        std::unordered_map<std::string, std::unordered_set<uint64_t>> sock_hash_to_ops;
        //! The key of the event loop registration of each socket's descriptor, by socket hash; see regKey()
        /** One descriptor can be registered for several objects (a socket and a controller wrapper of it, a number
            polled for a %Qore pollable, an extra fd): each registration has its own key, so the event loop reports
            each one's events and removing one keeps the others (see QoreEventLoop::addRegistration())
        */
        std::unordered_map<std::string, uint64_t> reg_keys;
        //! The socket hash of each registration key in @ref reg_keys
        std::unordered_map<uint64_t, std::string> key_hashes;
        //! The next key for a socket registration; never reused, as the event loop outlives the thread's runs
        uint64_t next_reg_key = 1;
        //! The holds that keep the registered descriptors open, by the hash of the object registered with each
        /** A descriptor registered in the event loop cannot be closed by another thread, so its number cannot be
            given to another connection while the loop has it registered; see qore_socket_private::DescriptorHold.
            Each registration (@ref registered_fds) has its own hold: one descriptor can be registered for more than
            one object (a socket and a controller wrapper of it; a number that Socket::poll() polls for a %Qore
            pollable or an extra fd), and a registration without a hold must not release the hold of another one
        */
        std::unordered_map<std::string, std::shared_ptr<void>> fd_holds;
        //! The extra fds of each operation, by operation ID
        /** No operation leaves the I/O thread with an entry here; see retireOp().  While a worker runs a step of an
            operation it has none either (see unregisterOp()).
        */
        std::unordered_map<uint64_t, std::unordered_set<int>> key_extra_fds;
        //! The holds of the owned extra fds of operations, by operation ID and descriptor
        /** An extra fd is registered by number; when its owner made it an owned descriptor (see
            qore_owned_descriptor_create(): the connection attempts of happy eyeballs, the sockets of a c-ares
            channel), the registration holds it open, so the owner's close cannot release its number before the
            registration is removed
        */
        std::map<std::pair<uint64_t, int>, std::shared_ptr<void>> extra_fd_holds;
        //! Extra fd readiness is operation-scoped (by operation ID); primary socket event masks can be zero.
        std::unordered_map<int, std::unordered_map<uint64_t, int>> extra_fd_to_key_events;
        //! The registrations removed because an event came for them and nothing was running for them
        /** See removeOrphanRegistration().  Read through getInfo(); not zero only if a path that retires an operation
            left a registration behind, which is a defect.
        */
        uint64_t orphan_registrations = 0;

        //! Tombstone record: TTL (memory-hygiene bound) + sequence gate
        /** The sequence gate is authoritative: a SubmitOp whose @c seq_at_push
            is strictly greater than @c seq is a fresh submit pushed after the
            cancel and is accepted; an equal-or-lesser seq is rejected as a
            racing/stale resubmit.  This makes a synchronous
            cancel-then-fresh-submit (e.g. cancelByOwner() before an FtpClient
            reconnect with the same owner; or a WebSocket reconnect that
            cancels the HCIO poll op then submits the WS poll op on the same
            socket fd) work without depending on TTL aging.
        */
        struct CancelInfo {
            int ttl;
            int seq;
        };

        //! Recently-cancelled operation keys — prevents re-submission of stale ops
        /** I/O-thread-only.  Sequence-gated tombstone (see @ref CancelInfo): a
            SubmitOp for a tombstoned key is rejected only when its
            @c seq_at_push is <= the recorded @c seq (a stale racing
            resubmit); a strictly-greater seq is a deliberate
            cancel-then-resubmit on the same key (e.g. WebSocket reconnect
            taking over the fd from the HCIO poll op) and is accepted.  The
            TTL is decremented each processCommands() cycle and the entry
            erased at zero; it is only a memory-hygiene bound.
            @since %Qore 3.0
        */
        std::unordered_map<std::string, CancelInfo> cancelled_keys;

        //! Recently-cancelled owners on this I/O thread — set by CancelOwner
        //! regardless of whether any cache entries were found.  Fixes the race
        //! where cancelByOwner() runs before submitConnectionOp() actually
        //! submits the operation: when SubmitOp later arrives, it detects the
        //! cancelled owner and dispatches onComplete(canceled=true) immediately
        //! instead of inserting into the cache (which would never be cancelled).
        //! Entries auto-expire after 2 idle cycles.
        /** Tombstone gate is sequence-based: a SubmitOp with @c seq_at_push
            strictly greater than @c CancelInfo::seq is a fresh submit pushed
            after the cancel and is accepted; equal-or-lesser seq is rejected
            as a racing/stale resubmit.  This makes synchronous
            cancelByOwner() followed by a fresh submit (e.g. FtpClient
            disconnect-then-reconnect with the same owner string) work
            without depending on TTL aging.  TTL is retained only as a
            memory-hygiene bound. */
        std::unordered_map<std::string, CancelInfo> cancelled_owners;

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

            I/O-thread-only.  Key: operation ID (an operation with the same
            key can have taken over the cache entry).  Value: the PollInfo
            copy whose cleanup is still pending.
         */
        std::unordered_map<uint64_t, PollInfo> pending_aborts;

        //! Socket-level cancel completions waiting on deferred aborts
        /** When @ref IoCommand::CancelSocket removes an operation whose
            worker-side @c continuePoll() is still in flight, the command's
            completion cannot be signaled until the worker returns and the
            deferred abort is delivered.  This map tracks those socket-scoped
            barriers by socket hash.
         */
        struct PendingSocketCancel {
            //! A socket to close when the barrier completes (see IoCommand::CancelAndCloseSocket)
            struct PendingClose {
                std::string sock_hash;
                AbstractPollableIoObjectBase* sock;  //!< referenced; released when closed
            };

            int pending_count = 0;
            std::vector<AsyncOpCompletion*> completions;
            //! sockets to close, in order, once the last in-flight worker continuePoll() has returned
            std::vector<PendingClose> closes;
        };
        std::unordered_map<std::string, PendingSocketCancel> pending_socket_cancels;

        //! Maps pending-abort operation IDs back to their socket-cancel barrier
        std::unordered_map<uint64_t, std::string> pending_abort_cancel_hash;

        //! The batch of commands with a Quit command, kept for the thread's exit cleanup (I/O thread only)
        /** Taken over without an allocation.  The commands from \c quit_batch_start on were not processed: the callers
            of their CancelByProgram commands wait for the Program's operations that the exit cleanup cancels
            (qore#6703)
        */
        std::vector<Command> quit_batch;
        //! The index of the first command of \c quit_batch that was not processed
        size_t quit_batch_start = 0;
    };

    //! Get the I/O thread index for a route key (hash-based affinity)
    /** The route key of an operation is that of its socket (see @ref getRouteKey()), so every operation on one
        socket runs on the same I/O thread, as it would with a single I/O thread: the socket, its TLS state and its
        HTTP/2 or QUIC session are only ever used by that thread.  Sockets are spread over the threads by this hash.

        @param route_key the route key of a socket; see @ref getRouteKey()
    */
    DLLLOCAL int getThreadIndex(const std::string& route_key) const {
        return getThreadIndexForHash(std::hash<std::string>{}(route_key));
    }

    //! Get the I/O thread index for the hash of a route key; see getThreadIndex()
    DLLLOCAL int getThreadIndexForHash(size_t route_hash) const {
        if (io_threads.size() <= 1) {
            return 0;
        }
        return route_hash % io_threads.size();
    }

    //! Get the I/O thread context for a route key
    /** @param route_key the route key of a socket; see @ref getRouteKey()
    */
    DLLLOCAL IoThreadContext& getThreadForKey(const std::string& route_key) {
        return *io_threads[getThreadIndex(route_key)];
    }

    //! Get the I/O thread context for the hash of a route key; see getThreadIndex()
    DLLLOCAL IoThreadContext& getThreadForHash(size_t route_hash) {
        return *io_threads[getThreadIndexForHash(route_hash)];
    }

    //! Get the I/O thread context of a controller timer; timers are spread over the I/O threads by ID
    /** Caller must hold m.
    */
    DLLLOCAL IoThreadContext& getThreadForTimer(int64_t timer_id) {
        return *io_threads[static_cast<uint64_t>(timer_id) % io_threads.size()];
    }

    // --- I/O thread contexts ---
    std::vector<std::unique_ptr<IoThreadContext>> io_threads;  //!< One per I/O thread (default: getDefaultIoThreadCount())
    int num_io_threads;                       //!< Configured thread count

    //! Maps socket hash → thread index for wakeSocket routing
    /** Updated by I/O threads when sockets are registered/unregistered with the event loop.
        Protected by a lightweight spinlock (separate from main mutex m) since it's
        only accessed briefly for insert/erase/lookup.
    */
    mutable QoreThreadLock sock_route_lock;
    std::unordered_map<std::string, int> sock_to_thread;  //!< sock_hash → thread index

    std::unordered_map<QoreObject*, std::string> obj_to_sock_hash;  //!< QoreObject* → sock_hash for wakeSocketByObject
    std::unordered_map<std::string, unsigned> provisional_sock_routes; //!< submit-time route refcounts

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

    //! Returns true if any I/O thread is stopping; the caller holds \c m
    DLLLOCAL bool anyThreadExiting() const {
        for (auto& tp : io_threads) {
            if (tp->exiting) {
                return true;
            }
        }
        return false;
    }

    //! Returns true if any I/O thread runs and is not stopping; the caller holds \c m
    DLLLOCAL bool anyThreadActive() const {
        for (auto& tp : io_threads) {
            if (tp->tid && !tp->exiting) {
                return true;
            }
        }
        return false;
    }

    //! Returns true if the I/O thread processes the commands pushed to it; the caller holds \c m
    /** A thread processes commands from its start until its last command drain when it stops; a stopping thread
        that has not drained its queue yet still processes the commands pushed to it then (cancellations and the like
        are completed by its exit cleanup).
    */
    DLLLOCAL static bool readsCommands(const IoThreadContext& t) {
        return t.tid && !t.commands_closed;
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

    //! Makes sure the timeout heap holds an entry for an operation's next deadline
    /** The next deadline is the earlier of the operation timeout (if armed) and the protocol-level poll timeout (if
        set).  An entry is pushed only if that deadline is earlier than the operation's earliest entry already in the
        heap (@ref PollInfo::heap_deadline_us): a later deadline is re-armed when the earlier entry is popped (Phase 1
        Step C), so the heap holds one or two entries per operation instead of one - with a copy of the key - per
        continuePoll() call.

        I/O-thread-only.

        @param t the I/O thread context holding the operation
        @param pinfo the operation, in the cache of \a t (its entries are pushed with its ID)
        @return the operation's next deadline, or 0 if it has none
    */
    DLLLOCAL int64 armTimeoutHeap(IoThreadContext& t, PollInfo& pinfo);

    //! Returns the absolute deadline of an operation's protocol-level poll timeout hint (\c poll_timeout_ms)
    /** The hint is the time in which the operation must be continued even if no I/O event arrives for it; a hint of
        0 or less is due now, as for an operation whose own deadline expired while its continuePoll() ran.

        @param poll_timeout_ms the hint in milliseconds
        @return the deadline in microseconds since the epoch, as for PollInfo::poll_timeout_deadline_us
    */
    DLLLOCAL static int64 getPollTimeoutDeadline(int64 poll_timeout_ms);

    //! cancelByKey() on one of this controller's I/O threads; never waits for another I/O thread
    /** @param uh the operation key
        @param own_idx the index of the calling I/O thread
        @param xsink for exception handling
        @return true if the operation was found on the calling thread, or a Cancel was queued for another I/O thread
        (or, during a continuePoll() batch, for the calling thread)
    */
    DLLLOCAL bool cancelByKeyOnIoThread(const std::string& uh, int own_idx, ExceptionSink* xsink,
        const AioAppRestriction& restrict_app = AioAppRestriction());

    //! cancelByOwner() with the restriction of the calling code already computed
    DLLLOCAL int cancelByOwnerIntern(const std::string& owner_str, const AioAppRestriction& restrict_app,
        ExceptionSink* xsink);

#ifdef DEBUG
    //! Test hook: holds the calling I/O thread before it drains its command queue while dbgHoldIoCommands() is set
    DLLLOCAL void dbgHoldBeforeCommands(IoThreadContext& t);
#endif

    //! cancelByKey() from a non-I/O thread with several I/O threads: searches every live I/O thread and waits
    /** @param uh the operation key
        @param xsink for exception handling
        @param only_spop_obj if not nullptr, only the operation of this object is canceled; the caller holds a
        reference to it until this function returns
        @return true if an operation was found and canceled
    */
    DLLLOCAL bool cancelByKeyOnAllThreads(const std::string& uh, ExceptionSink* xsink,
        const QoreObject* only_spop_obj = nullptr, const AioAppRestriction& restrict_app = AioAppRestriction());

    //! Returns the ID of the application Program that the calling code runs for (qore#7010)
    /** The initialization of library code and the code of a trusted build run for no application
        (QORE_NO_PROGRAM_ID), also in a callback: an operation that they submit belongs to no tenant.  The application
        of module code is the application that it runs for (qore#6208, see runtime_get_module_call_app_id()); the code
        of a callback of an operation runs for the application that the operation was submitted for (see
        AioCallbackAppScope), except the sandboxed code of an application Program, which runs for its own application

        Operations record the application that they are submitted for; code that is sandboxed cancels, flushes and
        replaces only the operations of its own application, as keys and owners can be guessed
    */
    DLLLOCAL static qore_program_id_t callerAppId();

    //! Returns the operations that the calling code may cancel or flush by key or owner
    /** @param none set to true if the calling code is sandboxed but runs for no known application: it then acts on no
        operation
    */
    DLLLOCAL static AioAppRestriction cancelRestriction(bool& none);

    //! Returns the timers that the calling code may cancel by ID (qore#7010)
    DLLLOCAL static AioTimerRestriction timerCancelRestriction();

    //! Returns true if code with the given restriction may cancel the timer (qore#7010)
    /** Code that is sandboxed cancels only the timers of the application that it runs for (TimerInfo::cb_app_id), as
        timer IDs can be guessed; if it runs for no known application (the code of a thread that module code started,
        which can serve every caller of the module, qore#6208), only the timers for no application that its own work
        added (TimerInfo::adder_gen).  Trusted code cancels any timer
    */
    DLLLOCAL static bool timerCancelAllowed(const AioTimerRestriction& restrict_app, const TimerInfo& tinfo);

    //! Returns true if a submission takes the key of an operation that the submitting code may not replace
    /** Sandboxed code replaces only an operation of its own application, or its own operation object; if it runs for
        no known application, it replaces only its own operation object
    */
    DLLLOCAL static bool submitTakesKeyOfOther(bool sandboxed, qore_program_id_t app_id, const PollInfo& pinfo,
        const QoreObject* spop_obj);

    //! Returns true if the operation may be canceled or flushed by code with the given restriction
    DLLLOCAL static bool opInProgram(const PollInfo& pinfo, const AioAppRestriction& restrict_app) {
        return !restrict_app.restricted || pinfo.app_pgm_id == restrict_app.app_id;
    }

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
#ifdef DEBUG
    //! The controller's lock in debug builds: an error-checking mutex
    /** A thread that takes the lock it already holds (a self-deadlock: the lock is not recursive) or that releases a
        lock it does not hold then fails the assertion in QoreThreadLock::lock() or unlock() at once, instead of
        waiting for itself forever.  Release builds use a plain QoreThreadLock.
    */
    class ErrorCheckLock : public QoreThreadLock {
    public:
        DLLLOCAL ErrorCheckLock() : QoreThreadLock(getAttr()) {
        }

        //! Returns the attributes the lock is created with: those of an error-checking mutex
        DLLLOCAL static const pthread_mutexattr_t* getAttr();
    };
    mutable ErrorCheckLock m;
#else
    mutable QoreThreadLock m;
#endif
    bool autostop_flag;
    //! True if the I/O threads are external-lifecycle threads; see setExternalLifecycleThreads()
    bool external_lifecycle_threads = false;
    bool shutting_down;
    std::unordered_map<std::string, CancelCond*> cancel_cond_map;
    QoreCondition io_cond;
    bool io_waiting;
    bool ready_flag;
    std::atomic<int> submit_seq;          //!< Incremented on each submit() call (global; per-thread tracking lives on IoThreadContext)
    QoreCondition processed_cond;         //!< Signaled when any thread's processed_seq advances
    QoreLoggerBridge* logger;              //!< Referenced or nullptr
    std::unordered_map<int64_t, TimerInfo> timer_info_map; //!< Timer ID -> user data
    ResolvedCallReferenceNode* timer_callback; //!< Timer callback (referenced, or nullptr)
    //! the run-time parse options of the code that set timer_callback, which it runs with (qore#6208); guarded by m
    AsyncIoRuntimeOptions timer_callback_po;

    //! Atomic counter for pre-allocating timer IDs across threads
    std::atomic<int64_t> next_ctrl_timer_id{1};

    // --- Internal methods ---

    //! Ensure call_dispatcher exists (lock-free fast path)
    DLLLOCAL void ensureCallDispatcher();

    //! Releases references held by the controller, off the async I/O thread
    /** Qore code must not run on an I/O thread, and releasing the last reference to a Qore value runs its
        destructors (see @ref AsyncIoDeferredRelease); on an I/O thread the references are therefore handed to a
        callback worker, on any other thread they are released immediately.

        @param refs the references to release; all members are cleared on return
        @param xsink exception sink for immediate releases
    */
    DLLLOCAL void releaseOffIoThread(AsyncIoDeferredRelease& refs, ExceptionSink* xsink);

    //! Records that the given C++ poll operation is held in a cache of this controller (see opObjectDeleted())
    /** Called on the I/O thread whose cache takes the operation.

        @return true if the operation's Qore object was deleted: the operation is canceled instead of polled
    */
    DLLLOCAL bool attachOp(SocketPollOperationBase* op, QoreObject* sock_obj, uint64_t& attach_id);

    //! Records that the given C++ poll operation is no longer held in a cache of this controller
    DLLLOCAL static void detachOp(SocketPollOperationBase* op, uint64_t attach_id);

    //! Sends a CancelDeletedOp command for the given C++ poll operation to the running I/O threads
    /** @param op the operation, referenced by each command
        @param only_idx if not negative, the command is sent to this I/O thread only
    */
    DLLLOCAL void postDeletedOpCancel(SocketPollOperationBase* op, int only_idx = -1);

    //! Sends a CancelDeletedObj command for the given Qore object of a poll operation to the running I/O threads
    /** @param obj the operation's object, referenced by each command
        @param only_idx if not negative, the command is sent to this I/O thread only
    */
    DLLLOCAL void postDeletedObjCancel(const QoreObject* obj, int only_idx = -1);


    //! Releases references held by the controller for an operation, off the async I/O thread
    /** As releaseOffIoThread(AsyncIoDeferredRelease&, ExceptionSink*), except that on an I/O thread the references
        of an operation that a synchronous exec() call waits for (@a sync_release is set) go to that caller, which
        releases them on its own thread when its wait ends, instead of to a callback worker (see
        @ref AsyncIoSyncRelease); a caller whose wait has already ended no longer takes them.

        The references must not be used after the call: the caller can release them at any time.

        @param refs the references to release; all members are cleared on return
        @param xsink exception sink for immediate releases
        @param sync_release the references taken by the synchronous caller of the operation, or nullptr
    */
    DLLLOCAL void releaseOffIoThread(AsyncIoDeferredRelease& refs, ExceptionSink* xsink,
        AsyncIoSyncRelease* sync_release);

#ifdef DEBUG
public:
    //! Debug-only: holds the references that the I/O threads of every controller release off the I/O thread
    /** Called by the dbg_async_io_hold_releases() test hook.  While held, the references handed to
        releaseOffIoThread() on an I/O thread are kept instead of being released by a callback worker, so that a test
        can make an object outlive the delivery of an operation's result deterministically; with @a hold = false,
        the references held are released on the calling thread, which must not be an I/O thread.

        @param hold true to hold the references from now on, false to stop holding them and release those held
        @param xsink exception sink for the releases
    */
    DLLLOCAL static void dbgHoldOffIoThreadReleases(bool hold, ExceptionSink* xsink);

    //! Debug-only: waits until at least @p count references are held, stops holding them, and releases those held
    //! on a callback worker of the global controller, as an I/O thread does without the hold
    /** Called by the dbg_async_io_dispatch_held_releases() test hook; returns when the releases have run (see
        flushCallbacks()), with the number of references released, or -1 if the wait timed out (the references stay
        held then).  Unlike dbgHoldOffIoThreadReleases(false), the references are released by a DT_RELEASE item of
        the callback dispatcher, as in production.
    */
    DLLLOCAL static int64_t dbgDispatchHeldOffIoThreadReleases(size_t count, int64_t timeout_ms,
        ExceptionSink* xsink);

    //! Debug-only: dispatches the release of a reference to the given object while its Program is marked as being
    //! destroyed, on the global controller's dispatcher, as an I/O thread can in that window (issue #6464)
    /** @return a hash: \c parked: the release items of the Program parked after the dispatch; \c after_clear: those
        left after the mark was cleared, which releases them in the calling thread
    */
    DLLLOCAL static QoreHashNode* dbgParkProgramReleaseTest(QoreObject* obj, ExceptionSink* xsink);

    //! Debug-only: the points after which a delivery can pause; see dbgPauseAfterDelivery()
    enum DbgPausePoint {
        //! an I/O thread pushes an operation's result to its result Queue, or dispatches the callback of a fired
        //! timer (see deliverResult())
        DBG_PAUSE_DELIVERY,
        //! a callback worker sends the result of a continuePoll() step to the I/O thread
        DBG_PAUSE_STEP_RESULT,
    };

    //! Debug-only: makes the next delivery at @p point wait after the delivery until this is called with
    //! @p pause = false
    /** Called by the dbg_async_io_pause_after_delivery() and dbg_async_io_pause_after_step_result() test hooks, so
        that a test can check what a thread that receives an operation's result, or sees a timer callback's work
        done, observes before the thread that delivered it does anything after the delivery.  The pause is taken by
        the next delivery at @p point only, and it is taken before the delivery, so that a thread that sees the
        delivery also sees that it was taken.

        @param point the point to pause after
        @param pause true to pause the next delivery, false to end a pause (and to cancel one not taken yet)

        @return with @p pause = false: true if a delivery took the pause since it was requested; false otherwise
    */
    DLLLOCAL static bool dbgPauseAfterDelivery(DbgPausePoint point, bool pause);

    //! Debug-only: returns true if a pause after a delivery at @p point was requested (dbgPauseAfterDelivery()) and
    //! the calling thread (an I/O thread, or a callback worker for DBG_PAUSE_STEP_RESULT) has taken it; the caller
    //! then calls dbgWaitAfterDelivery() after the delivery
    DLLLOCAL static bool dbgTakeDeliveryPause(DbgPausePoint point);

    //! Debug-only: waits until the pause taken by dbgTakeDeliveryPause() is ended
    DLLLOCAL static void dbgWaitAfterDelivery(DbgPausePoint point);

    //! Debug-only: the points of the cancellation of a Program's operations at which a test can pause (qore#6703)
    enum DbgCancelPausePoint : int {
        //! a callback worker that has taken a step of an operation (continuePoll()) waits before it enters the
        //! operation's Program; the end of the processing of a CancelByProgram command ends the pause
        DBG_CANCEL_STEP_BEFORE_ENTRY = 0,
        //! an I/O thread waits before it runs a cancellation that was deferred until the result of the operation's
        //! step (pending_aborts) and that no caller waits for, as a slow I/O thread does
        DBG_CANCEL_DEFERRED_RESULT = 1,
        //! an I/O thread waits after a socket cancel has started to wait for the step of an operation of the socket
        DBG_CANCEL_SOCKET_BARRIER = 2,
        //! the caller of cancelByProgram() waits before it cancels the operations handed back to it
        DBG_CANCEL_BEFORE_HANDED_BACK = 3,
        //! a stopping I/O thread waits before its last drain of the command queue
        DBG_CANCEL_BEFORE_EXIT_DRAIN = 4,
        //! the caller of cancelByProgram() waits after it has sent its commands to the I/O threads
        DBG_CANCEL_PROGRAM_SENT = 5,
        //! an I/O thread waits before it drains its command queue, so that commands sent meanwhile form one batch
        DBG_CANCEL_BEFORE_COMMAND_DRAIN = 6,
        //! not a pause: stop() has begun to wait for a cancelByProgram() call in progress
        DBG_CANCEL_STOP_WAITS = 7,
        //! not a pause: an I/O thread that is otherwise idle stays because a cancellation waits for a step's result
        DBG_CANCEL_AUTOSTOP_KEPT = 8,
        //! the number of points
        DBG_CANCEL_POINT_COUNT = 9,
    };

    //! Debug-only: requests a pause at the given point of the cancellation of a Program's operations, or ends it
    /** @param point the point (DbgCancelPausePoint)
        @param pause true to pause the next thread that reaches the point, false to end a pause (and to cancel one not
        taken yet)

        @return with @p pause = false: true if a thread took the pause since it was requested; false otherwise
    */
    DLLLOCAL static bool dbgPauseCancelPoint(int point, bool pause);

    //! Debug-only: waits until a thread has taken the pause requested at the given point; returns false if none did
    //! when \a timeout_ms elapsed
    DLLLOCAL static bool dbgWaitCancelPointTaken(int point, int timeout_ms);

    //! Debug-only: pauses the calling thread at the given point if a pause was requested there, until it is ended
    DLLLOCAL static void dbgTakeCancelPause(int point);

    //! Debug-only: ends the pause taken at the given point, if any
    DLLLOCAL static void dbgEndCancelPause(int point);

    //! Debug-only: records that a requested point was reached, without pausing; for a thread that holds a lock
    DLLLOCAL static void dbgNoteCancelPoint(int point);

    //! Debug-only: sets the idle period before an I/O thread stops, in microseconds; negative for the default
    DLLLOCAL static void dbgSetAutostopGrace(int64 us);

    //! Debug-only: returns the number of operations attached to a CancelByProgram command that followed Quit
    DLLLOCAL static int64 dbgQuitBatchAttaches();

    //! Debug-only: returns how many times stop() took the dispatcher while another thread's cancelByProgram() ran
    DLLLOCAL static int64 dbgStopDetachedDuringCancel();

    //! Debug-only: returns the number of calls of callAbort() in this process
    DLLLOCAL static int64 dbgAbortCalls();

    //! Debug-only: parks the release of each object that \a make returns, drops the caller's references, and releases
    //! them by ending the mark of their Program
    DLLLOCAL static QoreHashNode* dbgParkProgramReleasesTest(const ResolvedCallReferenceNode* make,
            ExceptionSink* xsink);

    //! Debug-only: holds the callbacks of timers that fire from now on, instead of dispatching them
    /** Called by the dbg_async_io_hold_timer_callbacks() test hook, so that a test can run the callback of a timer
        that fired before the timer's owner stopped it, after the owner has stopped it (see
        dbgReleaseHeldTimerCallbacks()); with @a hold = false, fired timers are dispatched again, and the callbacks
        already held stay held until they are released
    */
    DLLLOCAL static void dbgHoldTimerCallbacks(bool hold);

    //! Debug-only: makes the next hold of a timer callback fail as if its memory could not be allocated
    /** Called by the dbg_async_io_fail_next_timer_hold() test hook: the callback is then dispatched as usual, as it is
        when timer callbacks are not held
    */
    DLLLOCAL static void dbgFailNextTimerHold();

    //! Debug-only: waits until at least @p count timer callbacks are held; returns false if the wait timed out
    DLLLOCAL static bool dbgWaitHeldTimerCallbacks(size_t count, int64_t timeout_ms);

    //! Debug-only: dispatches the timer callbacks held and waits for them to complete; returns their number
    DLLLOCAL static size_t dbgReleaseHeldTimerCallbacks(ExceptionSink* xsink);

    //! Debug-only: holds the results of continuePoll() steps that callback workers send from now on
    /** Called by the dbg_async_io_hold_step_results() test hook: the operation of a held result stays in its step
        (continue_poll_in_flight) until the result is released (dbgReleaseHeldStepResults()), so that a test can
        cancel an operation while its step is in flight, which defers the cancel until the step's result arrives
        (pending_aborts); with @a hold = false, results are sent again, and the results already held stay held until
        they are released
    */
    DLLLOCAL static void dbgHoldStepResults(bool hold);

    //! Debug-only: holds the onComplete() and timer work items that the module cleanup hands over from now on
    /** Called by the dbg_async_io_hold_module_cleanup_work() test hook (qore#6928, qore#7027): such an item (a canceled
        operation's onComplete() or a timer's callback with QoreSandboxContext::module_cleanup_delegate) is held
        instead of queued until the module
        cleanup releases it (qore_dbg_release_held_module_cleanup_work()), so that a test can make it run as late as
        the cleanup lets it; with \a stream_data, also the stream data and poll-complete notifications of such
        operations (the frames that a connection receives), which the cleanup releases only after its deadline
        (qore#7027)
    */
    DLLLOCAL static void dbgHoldModuleCleanupWork(bool hold, bool stream_data = false);

    //! Debug-only: waits until at least \a count items are held by dbgHoldModuleCleanupWork(), of operations whose
    //! class name contains \a class_part if given; returns false if the wait timed out (qore#7027)
    /** Note that the stream data and poll-complete notifications held are released only after the deadline of the
        cleanup, also after an onComplete() of the same operation, and that code that waits for such a notification
        while they are held waits until then
    */
    DLLLOCAL static bool dbgWaitHeldModuleCleanupWork(size_t count, const char* class_part, int64 timeout_ms);

    //! Debug-only: the held test-hook work that the drain of the module cleanup releases (QDMCDP_* flags, qore#7027)
    /** QDMCDP_BEFORE_CANCEL_ROUND: the step results held by dbg_async_io_hold_step_results(), before the drain's cancel
        round; QDMCDP_WAIT_AFTER_CANCEL: the same, when the drain waits after a cancel round; QDMCDP_AFTER_TIMER_SCAN:
        the timer callbacks held by dbg_async_io_hold_timer_callbacks(), after the round's scan of the timers
    */
    DLLLOCAL static void dbgModuleCleanupDrainReleases(int flags);

    //! Debug-only: waits until at least @p count step results are held; returns false if the wait timed out
    DLLLOCAL static bool dbgWaitHeldStepResults(size_t count, int64_t timeout_ms);

    //! Debug-only: sends the step results held to their I/O threads and returns their number
    /** Each result is sent as the worker that made the step would have sent it, with the module cleanup delegation
        of that worker (Command::module_cleanup_delegate), not of the calling thread
    */
    DLLLOCAL static size_t dbgReleaseHeldStepResults(ExceptionSink* xsink);

private:
    //! Debug-only: holds the callback of a fired timer if timer callbacks are held; returns true if it was held
    /** The reference of @p code is taken over when it is held
    */
    DLLLOCAL bool dbgHoldTimerCallback(ResolvedCallReferenceNode* code, const std::string& owner,
            AsyncIoSandboxContext& sandbox_ctx, AsyncIoRuntimeOptions& runtime_po);

    //! Debug-only: holds a step result if step results are held; returns true if it was held
    /** The references of @p new_poll_info and @p ex_hash are taken over when it is held
    */
    DLLLOCAL bool dbgHoldStepResult(const std::string& key, uint64_t op_id, int thread_idx,
            QoreHashNode* new_poll_info, QoreHashNode* ex_hash, bool completed, const std::string* state);
#endif

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

        @note The caller must not hold @ref m: the cleanup delivers a canceled result, logs, releases references
        (which can run destructors) and can create the callback dispatcher, which take @ref m; it takes @ref m itself
        to signal the waiters (@ref cancel_cond_map, completions)
    */
    DLLLOCAL void cleanupAbandonedCommand(Command& cmd, ExceptionSink* xsink);

    //! Completes the completion of an abandoned command, if any; the caller holds @ref m
    DLLLOCAL void completeAbandonedLocked(Command& cmd);

    //! Makes an operation that the module cleanup cancels part of the cleanup
    /** If the current thread is the module cleanup or runs work that it handed over (qore_module_cleanup_delegation()),
        or processes a command that such a thread created (Command::module_cleanup_delegate), the operation gets a
        copy of its sandbox context with QoreSandboxContext::module_cleanup_delegate set, so that its abort(),
        onComplete() and the release of its references can enter the Programs of the modules after the cleanup has
        closed them (qore#6510).  Called by doCancelIntern() and when a cancel waits for the operation's step
        (pending_aborts); keeps the context if the copy cannot be allocated
    */
    DLLLOCAL void applyModuleCleanupCancel(PollInfo& pinfo);

    //! Cancels the operation of \a key that the I/O thread has just admitted, if work that the module cleanup handed
    //! over submitted it (\a handed_over), the cleanup has not canceled it and the cleanup has reached its deadline
    //! (qore#7027)
    /** Called on the I/O thread where an operation enters its cache: such an operation would otherwise be canceled
        only by a later cancel round of the cleanup, which runs only when its wait ends, which the work that waits for
        it synchronously would keep from ending; also named in a warning.  An operation of the cleanup thread itself
        is canceled by the cleanup's next round, and a synchronous wait of it keeps the operation's own timeout
    */
    DLLLOCAL void cancelAdmittedAfterModuleCleanupDeadline(IoThreadContext& t, const std::string& key,
            bool handed_over, ExceptionSink* xsink);

    //! Returns a name of the operation of \a pinfo for a message of the module cleanup: its class and owner
    DLLLOCAL static std::string moduleCleanupOpName(const PollInfo& pinfo);

    //! Completes a submission that is not admitted: its operation is canceled and its result delivered
    /** The result is a canceled result, which carries \a cancel_ex if given.  The operation is described as it is when
        it is admitted, with the sandbox context of its submitter, so that its abort() and onComplete() run as they do
        for an admitted operation that is canceled (a stale submission after a cancellation, a submission that is
        refused because its key holds another operation).  Called from the I/O thread; the payload of \a cmd is
        released.

        @param cmd the SubmitOp command
        @param cancel_ex if given, the exception that the canceled result carries (borrowed)
        @param xsink for exceptions
    */
    DLLLOCAL void refuseSubmit(Command& cmd, QoreHashNode* cancel_ex, ExceptionSink* xsink);

    //! Returns the operation of a SubmitOp command as it is described when it is admitted; the payload of \a cmd
    //! passes to the returned entry, whose cleanup() releases it
    DLLLOCAL PollInfo takeSubmitPollInfo(Command& cmd);

    //! Takes the preserved submissions of the given Program from a thread that reads no commands (qore#7225)
    /** The caller holds the lock; the operations are appended to \a out, to be canceled by the caller

        @param tp the thread
        @param pgm the Program whose submissions are taken
        @param out the operations taken
    */
    DLLLOCAL void takeProgramSubmitsLocked(IoThreadContext& tp, QoreProgram* pgm, std::vector<PollInfo>& out);

    //! Takes the submissions that a thread that reads no commands keeps (qore#7225) and that \a match selects
    /** The caller holds the lock; the operations are appended to \a out, to be canceled by the caller, and the other
        submissions keep their order.  Used by the cancellations of a key, an owner and a Program, which find these
        submissions nowhere else: the thread admits them when it starts again (qore#7311).

        @param tp the thread
        @param match selects the submissions to take
        @param out the operations taken

        @return the number of operations taken
    */
    DLLLOCAL size_t takeKeptSubmitsLocked(IoThreadContext& tp, const std::function<bool(const Command&)>& match,
        std::vector<PollInfo>& out);

    //! Cancels the operations taken from the submissions that threads keep (see takeKeptSubmitsLocked())
    /** Their results are delivered as for admitted operations that are canceled; the caller does not hold the lock.

        @param pinfos the operations taken; empty on return
        @param key if not empty, the key whose waiters are woken (see signalCancelLocked())
        @param xsink for exceptions
    */
    DLLLOCAL void cancelTakenSubmits(std::vector<PollInfo>& pinfos, const std::string& key, ExceptionSink* xsink);

    //! Returns true if the calling code, with the given restriction, may cancel the submitted operation (qore#7010)
    DLLLOCAL static bool submitInProgram(const Command& cmd, const AioAppRestriction& restrict_app) {
        return !restrict_app.restricted || cmd.submit_app_id == restrict_app.app_id;
    }


    //! Dispatches the notifications of the streams of an HTTP/2 or HTTP/3 operation: onStreamData() for the streams that
    //! have data or ended, and the transmission results of watched responses
    /** Called after a poll step and after the abort of a canceled operation (qoretechnologies/qore#7241)
    */
    DLLLOCAL void dispatchStreamNotifications(SocketPollOperationBase* spop_base, QoreObject* spop_obj,
        const std::string& op_owner, const AsyncIoSandboxContext& op_sandbox_ctx, qore_program_id_t op_app_id);

    //! Records that the caller of the CancelByProgram command with this completion waits for the operation's
    //! deferred cancellation (qore#6703)
    DLLLOCAL QORE_INTERN_NOINLINE QORE_INTERN_COLD void attachDeferredCancel(PollInfo& pinfo,
            AsyncOpCompletion* completion);

    //! Marks the Program in the current dispatcher and in any that replaces it during the mark, which discards its
    //! queued items; out of memory ends the process
    /** A discarded step's result can be what a deferred cancellation waits for (see cancelByProgram())
    */
    DLLLOCAL QORE_INTERN_NOINLINE QORE_INTERN_COLD void markProgramNoFail(QoreProgram* pgm,
            ExceptionSink* xsink) noexcept;

    //! Hands an operation whose step has ended back to the caller that waits for it
    /** A socket cancel that waited for the step ends when the operation is handed back, not after its cleanup: the
        operation's abort() can itself close the socket and wait for that cancel.  For such an operation (@a
        socket_barrier), a native abort() runs here first, so that it still precedes the close of the socket, and the
        operation's claim on the socket is released here, so that the socket can be used when the cancel returns.

        @return true if the operation was handed back; false if no caller waits for it or it could not be handed back
        (out of memory), in which case the operation keeps its holders, and the caller's wait ends when the operation
        is released after the calling thread has canceled it
    */
    DLLLOCAL QORE_INTERN_NOINLINE QORE_INTERN_COLD bool handBackDeferredCancel(PollInfo& pinfo, bool socket_barrier,
            ExceptionSink* xsink);

    //! Cancels an operation handed back to cancelByProgram() in the context of the code that submitted it
    DLLLOCAL QORE_INTERN_NOINLINE QORE_INTERN_COLD void cancelHandedBack(PollInfo& pinfo, bool inline_on_complete,
            ExceptionSink* xsink);

    //! Returns true if an I/O thread is past its last command drain and has not ended its exit cancellations; the
    //! caller holds \c m
    /** Such a thread is canceling the operations that it held, and cancelByProgram() cannot reach them with a command
    */
    DLLLOCAL QORE_INTERN_NOINLINE QORE_INTERN_COLD bool anyThreadInExitDrain() const;

    //! Reports the exception of an onComplete() run in the calling thread, as a callback worker reports it
    DLLLOCAL QORE_INTERN_NOINLINE QORE_INTERN_COLD void reportInlineOnCompleteException(const PollInfo& pinfo,
            ExceptionSink& on_complete_xsink);

    //! Cancels the operation
    /** @param pinfo the operation
        @param xsink for exceptions
        @param cancel_ex if given, the exception that the canceled result carries (a refused submission, which is
        reported canceled with the reason; borrowed)
        @param inline_on_complete runs the operation's onComplete() in the calling thread: the module cleanup, which
        cancels an operation of a module's Program before it destroys the module's data (qore#6510, qore#6703)
    */
    DLLLOCAL void doCancelIntern(PollInfo& pinfo, ExceptionSink* xsink, QoreHashNode* cancel_ex = nullptr,
            bool inline_on_complete = false);

    //! Delivers a deferred result and releases the references of @a dd
    DLLLOCAL void deliverDeferred(DeferredDelivery& dd, ExceptionSink* xsink);

    //! Returns a deferred delivery of @a result (referenced; ownership transferred) to the receivers of @a pinfo
    DLLLOCAL DeferredDelivery makeDeferredDelivery(const std::string& key, PollInfo& pinfo, QoreHashNode* result);

    //! Deliver-exactly-once backstop: if \a pinfo owns a result queue but no
    //! terminal completion was ever committed, synthesize and deliver a
    //! canceled completion so a waiting Queue::get() cannot block forever.
    /** Called from PollInfo::cleanup() before the queue ref is dropped.  No
        controller mutex may be held.  Sets pinfo.completion_delivered.
    */
    DLLLOCAL void deliverBackstopCompletionIfUndelivered(PollInfo& pinfo, ExceptionSink* xsink);

    //! Signaled when an I/O thread has ended its exit cancellations (guarded by \c m; see anyThreadInExitDrain())
    QoreCondition exit_drain_cond;

    //! The number of cancelByProgram() calls in progress outside of the I/O threads (guarded by \c m)
    /** stop() does not replace the dispatcher during one: the Program's mark is in the dispatcher, and a replacement
        would run the callbacks of the operations that the call cancels (qore#6703)
    */
    int active_program_cancels = 0;
    //! The Programs whose cancelByProgram() is in progress on any thread, with the number of such calls (guarded by
    //! \c m)
    /** A dispatcher that a stop() made during such a call (by a destructor that the call runs) replaces is created
        marked for them, so that no worker runs an item of such a Program before the call marks the new dispatcher
        again (qore#7311)
    */
    std::unordered_map<QoreProgram*, int> canceling_programs;
    //! The cancelByProgram() calls in progress on the threads that are waiting in stop() (guarded by \c m)
    /** Such a call cannot end before its thread returns from stop(), so stop() does not wait for it
    */
    int program_cancels_in_stop = 0;
    //! Signaled when active_program_cancels decreases or program_cancels_in_stop increases
    QoreCondition program_cancels_cond;

    //! Cancel all operations in one I/O-thread context matching a socket hash
    DLLLOCAL int cancelSocketInContext(IoThreadContext& t, const std::string& sock_hash,
        AsyncOpCompletion*& completion, ExceptionSink* xsink);

    //! Complete any socket-level cancel waiting on a deferred abort key
    DLLLOCAL void completePendingSocketCancel(IoThreadContext& t, uint64_t op_id,
        ExceptionSink* xsink);

    //! Cancel all operations associated with a socket hash
    /** @param sock_hash the socket's I/O identity hash
        @param xsink exception sink
        @param targets the indexes of the I/O threads that can run operations of the socket; with nullptr, every
        I/O thread is asked (the socket's thread cannot be derived from the hash alone: a wrapper around a socket
        routes with the key of the socket it wraps; see getRouteKey())
    */
    DLLLOCAL int cancelBySocketHash(const std::string& sock_hash, ExceptionSink* xsink,
        const std::vector<int>* targets = nullptr);

    //! Returns the indexes of the I/O threads that can run operations of a socket
    /** These are the thread that the socket's operations are registered on, if any, and the thread that its route
        key selects (see getRouteKey()): all operations on one socket run on one of them.  A cancel or close of the
        socket then needs only these threads, not every I/O thread.
    */
    DLLLOCAL std::vector<int> getSocketThreadIndexes(AbstractPollableIoObjectBase* sock,
        const std::string& sock_hash);

    //! Submit a controller-side close command after socket operations are canceled
    /** The socket is closed on the I/O thread of its operations (see @ref getRouteKey()).

        @param sock the socket to close
        @param sock_hash the socket's I/O identity hash
        @param xsink for exception handling
    */
    //! Cancels the operations of a socket and closes it with one command to the I/O thread of the socket
    /** The cancel and the close used to be two commands with a wait for each.  The I/O thread cancels the operations
        of the socket and, if no worker continuePoll() of them is still running, closes it at once; otherwise it
        closes the socket when the last of them has returned, as the separate close command did after the cancel
        barrier.  Not used from an I/O thread, which never waits for another one.

        @param sock the socket
        @param sock_hash the socket's I/O identity hash
        @param thread_idx the index of the I/O thread of the socket
        @param xsink exception sink
        @param rc set to the result of close() when the function handled the close
        @param cancelled if not null, set to the number of operations that were cancelled (those of the
        socket that were not already running a worker continuePoll() are counted when the command is processed, the
        others when their cancel barrier is set; see cancelSocketInContext())
        @return true if the close was handled; false if the caller has to cancel and close in two steps (the
        controller is shutting down)
    */
    DLLLOCAL bool cancelAndCloseOnThread(AbstractPollableIoObjectBase* sock, const std::string& sock_hash,
        int thread_idx, ExceptionSink* xsink, int& rc, int* cancelled = nullptr);

    DLLLOCAL int closeSocketOnController(AbstractPollableIoObjectBase* sock,
        const std::string& sock_hash, ExceptionSink* xsink);

    //! Wake late submissions after descriptor close removes kernel readiness registration
    /** Called without m held, after closeIo(). The current I/O context is updated
        directly; other live contexts receive a WakeSocket command.
    */
    /** @param sock_hash the socket's I/O identity hash
        @param sock the closed socket: only the I/O threads that can run operations of the socket are woken (see
        getSocketThreadIndexes()); with nullptr, every I/O thread is
    */
    DLLLOCAL void wakeSocketAfterClose(const std::string& sock_hash, AbstractPollableIoObjectBase* sock = nullptr);

    //! True if a PollInfo belongs to the socket hash currently being targeted
    DLLLOCAL static bool pollInfoMatchesSocketHash(const PollInfo& pinfo,
        const std::string& sock_hash);

    //! Snapshot the Socket fd generation for the next controller wait
    DLLLOCAL static void snapshotSocketWaitGeneration(PollInfo& pinfo, QoreHashNode* poll_info);

    //! Returns SOCKET-CLOSED if a Socket fd changed during the controller wait
    DLLLOCAL static QoreHashNode* makeSocketWaitGenerationException(PollInfo& pinfo, ExceptionSink* xsink);

    //! Returns true if the poll-info socket fd changed since the last wait snapshot
    DLLLOCAL static bool hasSocketWaitGenerationChanged(PollInfo& pinfo, QoreHashNode* poll_info);

    //! Combines hasSocketWaitGenerationChanged() and snapshotSocketWaitGeneration() with one socket lookup
    /** @return true if the poll-info socket fd changed since the last wait snapshot; the snapshot is then replaced
        with the socket's current fd and generation for the next controller wait
    */
    DLLLOCAL static bool refreshSocketWaitGeneration(PollInfo& pinfo, QoreHashNode* poll_info);

    //! Update EventLoop registration for an operation
    /** When the object has no descriptor although an operation waits for its events, it was closed before its
        descriptor could be registered, and no event can come for it: its operations run again and see the close, as
        when the hold of a descriptor fails (see addSocketFd()).

        @param op_id the operation's ID; see OpCache

        @return true if the operation waits for events of an object that has no descriptor
    */
    DLLLOCAL bool updateEventLoopRegistration(IoThreadContext& t, uint64_t op_id,
        QoreObject* socket, const std::string& sock_hash, int events, bool force_fd_reregister,
        ExceptionSink* xsink);

    //! Changes the event loop registration of the object for the operation; see updateEventLoopRegistration()
    DLLLOCAL void updateEventLoopRegistrationIntern(IoThreadContext& t, uint64_t op_id, int ev_flags,
        QoreObject* socket, const std::string& sock_hash, bool force_fd_reregister, ExceptionSink* xsink);

    //! Records whether the operation's last wait was for events of an object that has no descriptor
    /** @param waits_without_descriptor the return value of updateEventLoopRegistration() for the wait
    */
    DLLLOCAL static void noteDescriptorlessWait(PollInfo& pinfo, bool waits_without_descriptor) {
        pinfo.descriptorless_waits = waits_without_descriptor ? pinfo.descriptorless_waits + 1 : 0;
    }

    //! Returns SOCKET-CLOSED if the operation waited for an object without a descriptor again after it ran again
    /** An operation whose object was closed before its descriptor could be registered runs again to see the close
        (see updateEventLoopRegistration()); one that waits for the object again would only run again and again.
    */
    DLLLOCAL static QoreHashNode* makeDescriptorlessWaitException(PollInfo& pinfo, ExceptionSink* xsink);

    //! Unregister an operation from the EventLoop
    /** @param op_id the operation's ID; see OpCache
    */
    DLLLOCAL void unregisterFromEventLoop(IoThreadContext& t, uint64_t op_id, ExceptionSink* xsink);

    //! Publish a socket wake route
    /** @return true if this call created a provisional route that must be
        cleared if the operation completes before EventLoop registration
    */
    DLLLOCAL bool publishSocketRoute(const std::string& sock_hash, QoreObject* sock_obj,
        int thread_idx, bool preserve_existing);

    //! Clear a submit-time provisional socket wake route
    DLLLOCAL void clearProvisionalSocketRoute(const std::string& sock_hash, QoreObject* sock_obj,
        int thread_idx);

    //! Clear a socket wake route only if it is still owned by the given thread
    DLLLOCAL void clearSocketRouteIfOwner(const std::string& sock_hash, QoreObject* sock_obj,
        int thread_idx);

    //! Update extra fd registrations for an operation
    /** @since %Qore 3.0
    */
    DLLLOCAL void updateExtraFds(IoThreadContext& t, uint64_t op_id, QoreObject* socket,
        QoreHashNode* poll_info, ExceptionSink* xsink);

    //! Returns True if \a poll_info's extra fd set differs from what is registered for the operation
    /** The primary socket registration (socket object, events, fd generation) can be
        completely unchanged while an operation's auxiliary fd set changes underneath
        it — Happy Eyeballs starts an additional racing connect fd when the RFC 8305
        stagger fires, and the inner connect poll state is not the object the
        controller tracks, so its fd generation bump is invisible here.  Callers use
        this to decide whether updateExtraFds() must run on the registration fast
        path.

        @param t the I/O thread context
        @param op_id the operation's ID; see OpCache
        @param poll_info the poll info hash returned by continuePoll()

        @return True if the extra fd set or its event mask changed

        @since %Qore 3.0
    */
    DLLLOCAL bool extraFdsChanged(const IoThreadContext& t, uint64_t op_id,
        const QoreHashNode* poll_info) const;

    //! Unregister extra fds for an operation
    /** @since %Qore 3.0
    */
    DLLLOCAL void unregisterExtraFds(IoThreadContext& t, uint64_t op_id, ExceptionSink* xsink);

    //! Removes everything that the event loop has registered for an operation: its extra fds and its socket
    /** For an operation that leaves (see retireOp()), and for one whose next step runs on a worker: the I/O thread
        cannot run it for an event meanwhile, and a level-triggered descriptor that is ready would make it poll and
        discard the event at 100% CPU until the step returns.  The step's result registers what it waits for again.
    */
    DLLLOCAL void unregisterOp(IoThreadContext& t, uint64_t op_id, ExceptionSink* xsink);

    //! Takes an operation out of the cache, with everything registered for it, and returns it
    /** The source of truth of this invariant: <b>an operation never leaves the cache with a registration in the event
        loop</b>.  Registrations are made for the operation's ID, which no later operation has (see OpCache), and they
        are level-triggered: nothing runs for the event of a registration left behind, so the descriptor, if it stays
        ready, makes the I/O thread poll and discard at 100% CPU, and the registration's hold keeps the descriptor open
        for as long as the thread runs.  So every path that ends an operation uses this function, or has called
        unregisterOp() before it removes the operation (a finished one that is still processed in place); OpCache only
        lets IoThreadContext remove an operation, and removing one with registrations fails an assertion.  The only
        other paths are an operation replaced at once by one that keeps its ID (IoThreadContext::takeOpKeepingId()),
        and a thread that is not running, which removed all of its registrations when it exited.

        A registration left behind in spite of this is removed at its first event; see removeOrphanRegistration().
    */
    DLLLOCAL PollInfo retireOp(IoThreadContext& t, OpCache::iterator it, ExceptionSink* xsink);

    //! Returns true if an operation in the cache waits for the socket with the given hash
    DLLLOCAL static bool socketHasOperation(IoThreadContext& t, const std::string& sock_hash);

    //! Returns true if the operation polls the socket with the given hash
    /** The socket of its last poll info, or before its first result, the socket it was submitted for
    */
    DLLLOCAL static bool pollsSocket(const PollInfo& pinfo, const std::string& sock_hash) {
        if (!pinfo.cached_sock_hash.empty()) {
            return pinfo.cached_sock_hash == sock_hash;
        }
        return pinfo.sock && pinfo.sock->getIoIdentityHash() == sock_hash;
    }

    //! Removes a registration that an event came for and that no running operation or socket has
    /** The events of the event loop are read right after the poll, before anything can change the registrations, so
        an event for such a registration is never a late event of an operation that has just left: the registration is
        one that was left behind, in breach of the invariant of retireOp().  Discarding the event would make the I/O
        thread spin for as long as the descriptor is ready, which is a far worse failure than removing the registration
        of what no one runs for; so it is removed, with the hold of its descriptor, counted in
        IoThreadContext::orphan_registrations and logged once at debug level.

        @param t the I/O thread context
        @param fd the descriptor of the event
        @param key the registration key of the event
    */
    DLLLOCAL void removeOrphanRegistration(IoThreadContext& t, int fd, uint64_t key);

    //! Adds the registration counts of an I/O thread to its hash in getInfo(); must run on the I/O thread
    DLLLOCAL void setRegistrationInfo(QoreHashNode* entry, const IoThreadContext& t, ExceptionSink* xsink);

    //! The bit that marks the event loop registration key of an operation's extra fd; see extraRegKey()
    static constexpr uint64_t ExtraRegKeyBit = 1ULL << 63;

    //! Returns the event loop registration key of the extra fds of the operation with the given ID
    /** An operation lists a descriptor once, so the key is unique among the registrations of the descriptor; socket
        registration keys (see regKey()) never have @ref ExtraRegKeyBit set
    */
    DLLLOCAL static uint64_t extraRegKey(uint64_t op_id) {
        return op_id | ExtraRegKeyBit;
    }

    //! Returns the event loop registration key of the socket with the given hash, making one if it has none
    DLLLOCAL uint64_t regKey(IoThreadContext& t, const std::string& sock_hash);

    //! Removes the registration of an operation's extra fd from the event loop
    DLLLOCAL void removeExtraFdRegistration(IoThreadContext& t, int fd, uint64_t op_id, ExceptionSink* xsink);

    //! Removes the event loop registration of the socket with the given hash for the given descriptor, if any
    /** Only the socket's own registration is removed: another object's registration of the same descriptor, or of a
        descriptor that got the number after the socket's was closed, is left as it is (see
        QoreEventLoop::removeRegistration()).

        @param t the I/O thread context
        @param old_fd the descriptor registered for the socket
        @param expected_hash the socket hash
        @param xsink exception sink
        @since %Qore 3.0
    */
    DLLLOCAL void releaseFdIfOwner(IoThreadContext& t, int old_fd,
        const std::string& expected_hash, ExceptionSink* xsink);

    //! Adds the descriptor of an I/O object to the event loop of an I/O thread and records it for the socket hash
    /** The descriptor is held open while it is registered (see AbstractPollableIoObjectBase::holdPollableDescriptor()),
        so a close made by another thread cannot release its number to another object before the I/O thread has
        removed it from its event loop.

        @return 0 if the descriptor was added, -1 if not because the socket no longer has it (it was closed or
        replaced; its number may already belong to another connection)
    */
    DLLLOCAL int addSocketFd(IoThreadContext& t, AbstractPollableIoObjectBase* s, QoreObject* socket,
        const std::string& sock_hash, int fd, int events, ExceptionSink* xsink);

    //! Forgets the registration of the descriptor of the object with the given hash and releases its hold
    /** Called once the descriptor has been removed from the event loop (see releaseFdIfOwner()), or when it is not
        the object's any more; a close made by another thread meanwhile closes the descriptor now
    */
    DLLLOCAL void eraseRegisteredFd(IoThreadContext& t, const std::string& sock_hash);

    //! Reports the registered descriptor of the object with the given hash ready again if it is a regular file
    /** Called when an operation starts or resumes waiting for the object: poll(2) reports a regular file ready to
        every wait, and the event loop reports one only when it is rearmed (see QoreEventLoop::rearmAlwaysReady()), so
        an operation that waits for a regular file after its readiness was reported (to it, or to another operation
        that waits for the same object) is reported it again instead of waiting until its timeout.  Does nothing for
        any other descriptor, and costs one test when the I/O thread's loop has no regular file.
    */
    DLLLOCAL void rearmIfAlwaysReady(IoThreadContext& t, const std::string& sock_hash);

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
    /** @return false if there is no logger to take the message (neither the controller's own logger nor a global
        one); a logger that is not enabled for the level counts as taking it
    */
    DLLLOCAL bool log(int level, const char* fmt, ...) const;

    //! Logs a message as @ref log() does; an exception that the logger raises is added to @a logger_xsink
    DLLLOCAL bool logWithSink(ExceptionSink* logger_xsink, int level, const char* fmt, ...) const;

    //! Logs a message as @ref log() does, with the arguments in a va_list
    /** @param logger_xsink if not null, receives an exception that the logger raises; otherwise it is discarded
    */
    DLLLOCAL bool logV(ExceptionSink* logger_xsink, int level, const char* fmt, va_list args) const;

public:
    //! Logs a message with this controller's own logger only (acquires lock — must NOT be called with lock held)
    /** @return false if the controller has no logger; the message is then not logged
    */
    DLLLOCAL bool logOwnV(int level, const char* fmt, va_list args, ExceptionSink* logger_xsink = nullptr) const;

private:

    //! Get the unique hash from a pollable I/O object
    /** @return the object's I/O identity hash; valid as long as \a sock is
    */
    DLLLOCAL static const std::string& getSocketHash(AbstractPollableIoObjectBase* sock);

    //! Returns the route key of an I/O object: the key that selects the I/O thread of all of its operations
    /** Normally the object's I/O identity hash; a wrapper around a socket that other operations use routes with the
        key of that socket instead (see AsyncIoRoutedPollable).  The \c thread_key of an operation's submission
        info has no effect on its routing, so no operation on a socket can run on another I/O thread than the
        socket's other operations.

        @param sock the I/O object
        @return the route key; valid as long as \a sock is
    */
    DLLLOCAL static const std::string& getRouteKey(const AbstractPollableIoObjectBase* sock) {
        const AsyncIoRoutedPollable* routed = dynamic_cast<const AsyncIoRoutedPollable*>(sock);
        return routed ? routed->getAsyncIoRouteKey() : sock->getIoIdentityHash();
    }

    //! Get socket and poll info from a SocketPollInfo hash
    DLLLOCAL static QoreObject* getSocketFromPollInfo(QoreHashNode* poll_info, std::string& sock_hash,
        int& events, ExceptionSink* xsink);

    //! Call continuePoll on an AbstractPollOperation object
    //! Enqueue a continuePoll result from a worker thread back to the I/O thread
    /** @param key the operation key
        @param op_id the ID of the operation that made the step, which the result is applied to (see OpCache)
        @param thread_idx the index of the I/O thread that dispatched the continuePoll() and holds the operation
        @param new_poll_info the new poll info (referenced, ownership transferred) or nullptr
        @param ex_hash the exception hash (referenced, ownership transferred) or nullptr
        @param completed true if the operation completed
        @param state the operation's state after the step, read for its state events, or nullptr
    */
    DLLLOCAL void enqueueContinuePollResult(const std::string& key, uint64_t op_id, int thread_idx,
        QoreHashNode* new_poll_info, QoreHashNode* ex_hash, bool completed, const std::string* state = nullptr);

    //! Sends the result of a continuePoll() step to the I/O thread; see enqueueContinuePollResult()
    /** @param module_cleanup_delegate the module cleanup delegation of the thread that made the step (see
        Command::module_cleanup_delegate)
    */
    DLLLOCAL void enqueueContinuePollResultIntern(const std::string& key, uint64_t op_id, int thread_idx,
        QoreHashNode* new_poll_info, QoreHashNode* ex_hash, bool completed, const std::string* state,
        bool module_cleanup_delegate);

    //! Dispatch a stream-data-ready notification from a worker thread
    /** Called by the DT_CONTINUE_POLL worker after getAndClearDataReadyStreams() to
        notify a Qore poll operation that data is available on a substream.
        Thread-safe: acquires the controller lock internally.
        @param spop_obj the poll operation Qore object (caller holds no extra ref; this
               method adds its own ref before dispatching)
        @param stream_key the stream identifier to pass to onStreamData()
    */
    DLLLOCAL void enqueueStreamDataDispatch(QoreObject* spop_obj, const std::string& stream_key,
        const std::string& owner = std::string(), AsyncIoSandboxContext sandbox_ctx = AsyncIoSandboxContext(),
        qore_program_id_t app_pgm_id = 0);

    //! Call abort on an AbstractPollOperation object
    DLLLOCAL static void callAbort(QoreObject* spop_obj, ExceptionSink* xsink);

    //! Build a result hash
    /** @note not static: a failure to populate the hash is logged through the controller's logger
    */
    DLLLOCAL QoreHashNode* buildResultHash(PollInfo& pinfo, bool canceled,
        QoreHashNode* ex_hash, ExceptionSink* xsink);

    //! Logs and clears any exception left in a long-lived ExceptionSink
    /** The I/O thread reuses a single ExceptionSink for an entire loop iteration.  An exception left pending in
        it by one operation would otherwise stay there for the rest of the iteration, where it is both invisible
        (nothing reports it) and harmful: typed-hash key assignment and several other helpers report failure
        through the sink and cannot distinguish a pre-existing exception from their own.  Call this at phase
        boundaries so a stray exception is reported once and cannot leak into unrelated work.

        @param phase a short description of the phase that raised the exception, used in the log message
        @param xsink the sink to drain; may be nullptr or empty, in which case this is a no-op
    */
    DLLLOCAL void logAndClearStrayException(const char* phase, ExceptionSink* xsink) const;

    //! Deliver a result via onComplete or queue (dispatches onComplete to worker thread)
    /** The result is pushed with the queue's object, as Queue::push() does, so that the deterministic garbage
        collector sees the references the result holds: a result queue reachable from the operation with its result
        left in it is part of a cycle that is collected.

        @param queue the result queue (not referenced by the call), or nullptr
        @param queue_obj the object of \a queue (not referenced by the call), or nullptr
    */
    DLLLOCAL void deliverResult(Queue* queue, QoreObject* queue_obj, QoreObject* spop_obj, bool has_on_complete,
        QoreHashNode* result, ExceptionSink* xsink, const std::string& owner = std::string(),
        AsyncIoSandboxContext sandbox_ctx = AsyncIoSandboxContext(), AsyncIoSyncRelease* sync_release = nullptr,
        qore_program_id_t app_pgm_id = 0);

    //! Notes the state an operation with a state queue has after a step, and reports it if it changed
    /** Called on the I/O thread with the state read after each step: the first state read is always reported,
        later ones when they differ from the state read after the step before; with notify states, only changes
        into one of them are reported.

        @param pinfo the operation; must have a state queue
        @param state the state, as returned by getStateImpl() (nullptr is the empty string, as getState() returns)
    */
    DLLLOCAL void noteOperationState(PollInfo& pinfo, const char* state);

    //! Pushes the last state event (\c done) of an operation with a state queue, exactly once
    /** Called on every path that ends an operation, before its result is delivered, so that all its events
        precede its result.  It only pushes to the state queue, which has its own lock, so it can be called whether
        or not the controller mutex is held.
    */
    DLLLOCAL void deliverStateEnd(PollInfo& pinfo);

    //! Pushes a state event to a state queue
    /** The queue never has a maximum size (see submit()), so the push never waits.  The event is pushed with the
        queue's object, as Queue::push() does, so that the deterministic garbage collector sees the references the
        event holds: a queue reachable from the operation (a member, a callback's closure) with events left in it
        is part of a cycle that is collected.  A failure to build or push the event is logged; it cannot affect the
        operation.

        @param state the state, or nullptr for an event without a state
    */
    DLLLOCAL void pushStateEvent(Queue* state_queue, QoreObject* state_queue_obj, QoreObject* sock_obj,
        QoreObject* spop_obj, const std::string* state, bool done, QoreHashNode* other, const std::string& owner);

    //! Shared call dispatcher for async callback/abort delivery (lazily created, atomic for lock-free check)
    std::atomic<QoreCallDispatcher*> call_dispatcher{nullptr};
    //! dispatchers that a stop() replaced while a cancelByProgram() ran on any thread; stopped, and deleted when the
    //! last of those calls returns, as any of them can still use them (guarded by m; qore#7226, qore#6703)
    std::vector<QoreCallDispatcher*> retired_dispatchers;
    //! the calls of cancelByProgram() in progress on all threads, including I/O threads (guarded by m)
    int cancel_by_program_scopes = 0;

    //! Counts a call of cancelByProgram(); the last one to end deletes the retired dispatchers
    struct CancelByProgramScope {
        AsyncIoControllerPriv* ctrl;

        DLLLOCAL CancelByProgramScope(AsyncIoControllerPriv* ctrl);
        DLLLOCAL ~CancelByProgramScope();

        CancelByProgramScope(const CancelByProgramScope&) = delete;
        CancelByProgramScope& operator=(const CancelByProgramScope&) = delete;
    };
#ifdef DEBUG
    //! test hook: called by cancelByProgram() in a worker thread after it has queued its commands (qore#7225)
    std::function<void()> dbg_cancel_by_program_queued;
    //! test hook: the start of an I/O thread fails (qore#7225)
    bool dbg_fail_io_thread_start = false;
#endif

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

//! Returns the global async I/O controller with a reference, or nullptr if none has been created; never creates one
DLLLOCAL AsyncIoControllerPriv* qore_get_async_io_controller_if_exists();

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

#endif // _QORE_ASYNCIOCONTROLLERPRIV_H
