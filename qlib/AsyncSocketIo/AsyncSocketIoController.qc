# -*- mode: qore; indent-tabs-mode: nil -*-
# Qore AsyncSocketIoController class definition

/*  AsyncSocketIoController.qc Copyright 2016 - 2026 Qore Technologies, s.r.o.

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
*/

#! The AsyncSocketIo namespace
/** All public declarations in the %AsyncSocketIo module are defined in this namespace
*/
public namespace AsyncSocketIo {
#! I/O thread commands
public enum IoCommand : string {
    #! Add a new operation
    Add = "add",
    #! Cancel a specific operation
    Cancel = "cancel",
    #! Cancel all operations for an owner
    CancelOwner = "cancel_owner",
    #! Quit the I/O thread
    Quit = "quit",
    #! Wake the I/O thread to force re-poll
    Wake = "wake",
}

#! Socket polling operation info hash
public hashdecl SocketPollOperationInfo {
    #! Socket object
    Socket sock;

    #! Poll operation
    AbstractPollOperation spop;

    #! Last poll info for this operation
    *hash<SocketPollInfo> poll_info;

    #! I/O timeout; use a negative value to disable timeout (operation will wait indefinitely)
    date to = AsyncSocketIoController::DefaultIoOperationTimeout;

    #! Owner identifier for cancellation; required for proper cleanup during shutdown via cancelByOwner()
    string owner;

    #! Free-form data to be returned when the operation completes
    *hash<auto> other;

    #! Optional result Queue; if provided, results will be pushed to this Queue instead of a new one
    /** This allows multiple operations to share a single result Queue for easier coordination
    */
    *Queue resultQueue;

    #! Optional completion callback; called in the I/O thread when the operation completes
    /** When provided, results are delivered via callback instead of Queue.
        The callback signature must accept a single hash<SocketPollResultInfo> argument.
        Mutually exclusive with \c resultQueue.
    */
    *code callback;

    #! Optional custom cache key; default: sock.uniqueHash()
    /** Allows multiple concurrent operations on the same socket (e.g., concurrent accepts
        on a listener) by using distinct keys like "accept:<hash>:1", "accept:<hash>:2".
    */
    *string key;
}

#! Socket poll result
public hashdecl SocketPollResultInfo {
    #! The socket that completed the operation
    Socket sock;

    #! The poll operation that completed
    AbstractPollOperation spop;

    #! If the operation was canceled
    *bool canceled;

    #! Any exception raised while executing the operation
    *hash<ExceptionInfo> ex;

    #! Free-form data provided when the operation was submitted
    *hash<auto> other;
}

#! Class for controlling async socket I/O
public class AsyncSocketIoController inherits LoggerWrapper {
    public {
        #! Polling event map
        const PollEventMap = {
            SOCK_POLLIN: "read",
            SOCK_POLLOUT: "write",
            SOCK_POLLIN | SOCK_POLLOUT: "read and write",
        };

        #! Default I/O timeout duration
        const DefaultIoOperationTimeout = 30s;
    }

    private {
        #! Lock for atomic actions
        Mutex m();

        #! Socket cache; unique hash -> poll info
        hash<string, hash<PollInfo>> cache;

        #! I/O thread TID
        int tid;

        #! Stop monitoring thread when the last operation is complete
        bool autostop = True;

        #! Shutdown flag to prevent new operations during stop
        bool shutting_down = False;

        #! Force continuePoll on all operations after a wake (needed for HTTP/2 pending writes)
        bool force_poll = False;

        #! Map of socket unique hash -> reference count (number of operations using this socket)
        hash<string, int> socket_refcounts;
    }

    private:internal {
        #! Event notifier for cross-thread signaling
        EventNotifier notifier();

        #! I/O thread condition for readiness/exit state changes
        Condition io_cond();

        #! True if a thread is waiting for I/O thread termination
        bool io_waiting = False;

        #! True if the I/O thread is exiting
        bool io_exiting = False;

        #! I/O thread command queue (protected by m)
        list<hash<auto>> cmdq = ();

        #! Cancel operation waiting Condition; socket unique key -> Condition
        hash<string, Condition> cancel_cond_map;

        #! Flag indicating I/O thread is ready
        bool ready_flag = False;

        #! Submit sequence counter; incremented under lock on each submit() call
        int submit_seq = 0;

        #! Processed sequence counter; updated by I/O thread after Phase 1 cache snapshot
        int processed_seq = 0;

        #! Condition variable signaled when processed_seq advances
        Condition processed_cond();

        #! Persistent event loop for efficient kqueue/epoll/poll
        EventLoop loop();

        #! Map of operation key -> currently registered socket
        /** Tracks which socket is registered in the EventLoop for each operation,
            since the poll socket can change between continuePoll() calls
        */
        hash<string, Socket> registered_sockets;

        #! Map of registered socket unique hash -> current poll events applied in EventLoop
        hash<string, int> registered_events;

        #! Map of operation key -> events requested by that key
        /** Used to compute the event union when multiple operations share a socket
        */
        hash<string, int> key_events;

        #! Reverse index: socket unique hash -> set of operation keys using that socket
        /** Allows O(1) lookup of all keys sharing a socket, making computeEventUnion() O(k)
            instead of O(N) where k is the number of operations per socket (typically 1).
        */
        hash<string, hash<string, bool>> sock_hash_to_keys;
    }

    #! Creates the object
    constructor(bool autostop, *LoggerInterface logger) : LoggerWrapper(logger) {
        self.autostop = autostop;
        constructorInit();
    }

    #! Creates the object
    constructor(*LoggerInterface logger) : LoggerWrapper(logger) {
        constructorInit();
    }

    #! Stops the monitoring thread and destroys the object
    destructor() {
        stop();
    }

    #! Returns information about the object
    hash<auto> getInfo() {
        return {
            "autostop": autostop,
            "sockets": (map {
                $1.key: ($1.value.spop ? "I/O" : $1.value.timeout_date),
            }, cache.pairIterator()),
            "io_tid": tid,
            "cmdq": cmdq.size(),
        };
    }

    #! Returns the number of operations in the cache (useful for testing cleanup)
    int getCacheSize() {
        AutoLock al(m);
        return cache.size();
    }

    #! Set autostop flag
    setAutostop(bool autostop = True) {
        self.autostop = autostop;
    }

    #! Get autostop flag
    bool getAutostop() {
        return autostop;
    }

    #! Starts monitoring
    /** This method starts the I/O thread and waits for it to signal that it's ready
        before returning. This ensures deterministic behavior where callers can
        immediately submit operations after start() returns.

        @throw CONTROLLER-START-ERROR if the I/O thread is already running
    */
    start() {
        {
            AutoLock al(m);

            if (tid) {
                throw "CONTROLLER-START-ERROR", sprintf("I/O thread is already running in TID %d", tid);
            }

            shutting_down = False;
            startIntern();
        }
        # Wait for the I/O thread to signal it's ready (outside lock)
        waitReady();
    }

    #! Stops monitoring
    stop() {
        bool signal = False;
        {
            AutoLock al(m);
            if (tid) {
                shutting_down = True;
                signal = enqueueCmdLocked(IoCommand::Quit);
            } else {
                shutting_down = True;
            }
        }
        if (signal) {
            notifier.notify();
        }
        waitStop();
    }

    #! Stops monitoring and clears all connections
    stopClear() {
        int count;
        bool signal = False;
        {
            AutoLock al(m);
            count = cache.size();
            shutting_down = True;
            if (tid) {
                signal = enqueueCmdLocked(IoCommand::Quit);
            }
        }
        if (signal) {
            notifier.notify();
        }
        waitStop();
        log(LoggerLevel::INFO, "Canceled all operations (%d operation%s canceled)", count, count == 1 ? "" : "s");
    }

    #! Waits for the monitoring thread to stop if it's running
    waitStop() {
        AutoLock al(m);
        if (!tid) {
            return;
        }
        io_waiting = True;
        on_exit io_waiting = False;
        while (tid) {
            io_cond.wait(m);
        }
    }

    #! Returns @ref True if the I/O thread is running
    bool running() {
        AutoLock al(m);
        return tid != 0;
    }

    #! Waits for the I/O thread to be ready
    /** This method blocks until the I/O thread has started and is ready to process operations.
        Use this after submitting operations to ensure the I/O thread is polling before
        attempting to connect to the socket.

        @param to maximum time to wait; use @ref NOTHING for no timeout

        @return @ref True if the I/O thread is ready, @ref False if the timeout expired
    */
    bool waitReady(*timeout to) {
        AutoLock al(m);
        if (ready_flag) {
            return True;
        }
        if (!tid) {
            return False;
        }
        io_waiting = True;
        on_exit io_waiting = False;
        if (to) {
            return io_cond.wait(m, to) == 0 && ready_flag;
        }
        while (!ready_flag && tid) {
            io_cond.wait(m);
        }
        return ready_flag;
    }

    #! Waits until the I/O thread has processed all currently submitted operations
    /** This method blocks until the I/O thread's Phase 1 cache snapshot has captured all
        operations that were submitted before the call. This provides a stronger guarantee
        than waitReady(): not only is the I/O thread running, but it has actually polled
        all pending operations at least once.

        @param to maximum time to wait; use @ref NOTHING for no timeout

        @return @ref True if all submitted operations have been processed, @ref False if the
        timeout expired or the I/O thread is not running

        @since %AsyncSocketIo 1.0
    */
    bool waitForProcessing(*timeout to) {
        AutoLock al(m);
        if (!tid) {
            return False;
        }
        int target = submit_seq;
        if (processed_seq >= target) {
            return True;
        }
        if (to) {
            date deadline = now_us() + to;
            while (processed_seq < target && tid) {
                timeout remaining = deadline - now_us();
                if (remaining <= 0s) {
                    return False;
                }
                processed_cond.wait(m, remaining);
            }
            return processed_seq >= target;
        }
        while (processed_seq < target && tid) {
            processed_cond.wait(m);
        }
        return processed_seq >= target;
    }

    #! Submits a socket operation to be performed and returns a Queue where the results will be returned
    /** @param info a hash with the socket operation and supporting info
        @param replace if True, then any operation currently in place for the given socket will be replaced

        @return a Queue that will have the result of the operation pushed to it; if \a info.resultQueue is
        provided, that Queue is returned; returns @ref NOTHING when \a info.callback is provided

        @throw CONTROLLER-ADD-ERROR invalid or incomplete data in \a info; a different operation on this socket is
        already in progress
    */
    *Queue submit(hash<SocketPollOperationInfo> info, *bool replace) {
        if (!info.sock) {
            throw "CONTROLLER-ADD-ERROR", sprintf("Missing 'sock' value; got: %y", info);
        }
        if (!info.spop) {
            throw "CONTROLLER-ADD-ERROR", sprintf("Missing 'spop' value; got: %y", info);
        }
        if (!info.owner) {
            throw "CONTROLLER-ADD-ERROR", sprintf("Missing 'owner' value; required for proper shutdown via "
                "cancelByOwner(); got: %y", info);
        }
        @assert(!info.callback || !info.resultQueue,
            "callback and resultQueue are mutually exclusive");
        @assert(!info.key || info.key.size() > 0,
            "key must be non-empty if provided");
        # get unique key: use custom key if provided, otherwise socket unique hash
        string uh = info.key ?? info.sock.uniqueHash();
        hash<PollInfo> cinfo;
        bool cancel_signal = False;
        bool wait_cancel = False;
        *Queue result_queue = info.callback ? NOTHING : (info.resultQueue ?? new Queue());
        {
            AutoLock al(m);
            if (shutting_down) {
                throw "CONTROLLER-ADD-ERROR", sprintf("Cannot add operation for key %y; controller is shutting down",
                    uh);
            }
            if (*hash<PollInfo> existing = cache{uh}) {
                if (replace) {
                    cancel_signal = enqueueCancelLocked(uh);
                    wait_cancel = True;
                } else {
                    # otherwise throw an exception
                    throw "CONTROLLER-ADD-ERROR", sprintf("An operation with key %y is already in process", uh);
                }
            }
        }
        if (cancel_signal) {
            notifier.notify();
        }
        if (wait_cancel) {
            waitCancel(uh);
        }
        bool add_signal = False;
        {
            AutoLock al(m);
            if (shutting_down) {
                throw "CONTROLLER-ADD-ERROR", sprintf("Cannot add operation for key %y; controller is shutting down",
                    uh);
            }
            cinfo = cache{uh} = <PollInfo>{
                "sock": info.sock,
                "spop": info.spop,
                "to": info.to,
                "owner": info.owner,
                "other": info.other,
                "q": result_queue,
                "callback": info.callback,
            };
            @debug {
                if (getenv("QORE_ASYNCIO_DEBUG")) {
                    log(LoggerLevel::INFO, "submit: added operation for key %s goal=%y to=%y cache_size=%d callback=%y",
                        uh, info.other.goal, info.to, cache.size(), info.callback ? True : False);
                }
                if (getenv("QORE_ASYNCIO_TRACE")) {
                    log(LoggerLevel::DEBUG, "submit: info=%y", info);
                }
            }
            ++submit_seq;
            if (io_exiting || !tid) {
                startIntern();
            }
            # IO_ADD is a no-op in processCommands() — the I/O thread picks up new
            # cache entries in Phase 1 automatically.  Just notify the event loop to
            # wake the I/O thread so it re-polls with the new operation.
            add_signal = True;
        }
        if (add_signal) {
            notifier.notify();
        }
        return result_queue;
    }

    #! Executes a socket operation asynchronously and waits for the results
    /** @param info a hash with the socket operation and supporting info
        @param replace if True, then any operation currently in place for the given socket will be replaced

        @return The result of the asynchronous operation

        @throw CONTROLLER-ADD-ERROR invalid or incomplete data in \a info; a different operation on this socket is
        already in progress
    */
    hash<SocketPollResultInfo> exec(hash<SocketPollOperationInfo> info, *bool replace) {
        @assert(!info.callback, "exec() cannot be used with callback mode");
        return submit(info, replace).get();
    }

    #! Cancels the operation with the given Socket's unique hash as the key
    /** @param sock the socket to cancel any operation in progress for

        @note This method only cancels operations keyed by the socket's unique hash (the default).
        Operations submitted with a custom \c key will not be found by this method; use
        cancelByKey() or cancelByOwner() for those.

        Stops the I/O thread if the last operation is canceled and the \a autostop flag is enabled.

        @see cancelByKey()
        @see cancelByOwner()
        @see cancelEx()
    */
    bool cancel(Socket sock) {
        bool rv;
        bool stopped;
        bool cancel_signal = False;
        bool stop_signal = False;

        string uh = sock.uniqueHash();
        {
            AutoLock al(m);
            if (cache{uh}) {
                rv = True;
                cancel_signal = enqueueCancelLocked(uh);
            } else {
                rv = False;
            }
        }
        if (cancel_signal) {
            notifier.notify();
        }
        if (rv) {
            waitCancel(uh);
        }
        {
            AutoLock al(m);
            if (autostop && rv && !cache && tid) {
                stop_signal = enqueueCmdLocked(IoCommand::Quit);
                stopped = True;
            }
        }
        if (stop_signal) {
            notifier.notify();
        }
        if (stopped) {
            waitStop();
        }
        return rv;
    }

    #! Wakes the I/O thread to re-evaluate poll state
    wake() {
        sendCmd(IoCommand::Wake);
    }

    #! Cancels any operation in progress for the given Socket
    /** @param sock the socket to cancel any operation in progress for

        @return @ref True if an operation for the socket was canceled, @ref False if not

        @note Stops the I/O thread if the last operation is canceled and the \a autostop flag is enabled

        @throw CONTROLLER-CANCEL-ERROR thrown if the Socket has no operation in progress

        @see cancel()
    */
    bool cancelEx(Socket sock) {
        if (!cancel(sock)) {
            throw "CONTROLLER-CANCEL-ERROR", sprintf("Socket %y has no operation in progress", sock.uniqueHash());
        }
        return True;
    }

    #! Cancels the operation with the given custom key
    /** @param key the custom key used when submitting the operation

        @return True if an operation was found and canceled, False otherwise

        @note This method is needed for operations submitted with a custom \c key field,
        since cancel(Socket) only looks up the socket's unique hash.

        @since %AsyncSocketIo 2.0

        @see cancel()
        @see cancelByOwner()
    */
    bool cancelByKey(string key) {
        bool rv;
        bool stopped;
        bool cancel_signal = False;
        bool stop_signal = False;

        {
            AutoLock al(m);
            if (cache{key}) {
                rv = True;
                cancel_signal = enqueueCancelLocked(key);
            } else {
                rv = False;
            }
        }
        if (cancel_signal) {
            notifier.notify();
        }
        if (rv) {
            waitCancel(key);
        }
        {
            AutoLock al(m);
            if (autostop && rv && !cache && tid) {
                stop_signal = enqueueCmdLocked(IoCommand::Quit);
                stopped = True;
            }
        }
        if (stop_signal) {
            notifier.notify();
        }
        if (stopped) {
            waitStop();
        }
        return rv;
    }

    #! Cancels all operations belonging to a specific owner
    /** @param owner the owner identifier (typically from uniqueHash()) to cancel operations for

        Operations are matched by checking the \c owner field in the @ref SocketPollOperationInfo hash.

        @return the number of operations canceled

        @note This method blocks until all matching operations have been canceled.
        The I/O thread continues running and other owners' operations are not affected.

        @since %AsyncSocketIo 1.1
    */
    int cancelByOwner(string owner) {
        int rv;
        bool signal = False;
        Condition done_cond();
        {
            AutoLock al(m);
            if (!tid || !cache) {
                return 0;
            }
            # count matching operations
            foreach string key in (keys cache) {
                if (cache{key}.owner == owner) {
                    ++rv;
                }
            }
            if (!rv) {
                return 0;
            }
            signal = enqueueCmdLocked(IoCommand::CancelOwner, (owner, done_cond));
        }
        if (signal) {
            notifier.notify();
        }
        # wait for the I/O thread to complete cancellation using condition variable
        {
            AutoLock al(m);
            while (True) {
                # Check if I/O thread has exited - if so, operations are implicitly canceled
                if (!tid) {
                    break;
                }
                bool found = False;
                foreach string key in (keys cache) {
                    if (cache{key}.owner == owner) {
                        found = True;
                        break;
                    }
                }
                if (!found) {
                    break;
                }
                done_cond.wait(m);
            }
        }
        return rv;
    }

    #! Common constructor code
    private constructorInit() {
        # Register EventNotifier in persistent EventLoop for cross-thread wakeup
        loop.add(notifier, SOCK_POLLIN);
    }

    #! Queues a cancel command for an in-progress operation; caller must hold m
    private bool enqueueCancelLocked(string uh) {
        @assert(m.trylock() == -1);
        if (shutting_down || !tid || !cache{uh}) {
            if (cancel_cond_map{uh}) {
                cancel_cond_map{uh}.broadcast();
                remove cancel_cond_map{uh};
            }
            return False;
        }
        # register the cancel condition before sending the command to avoid races
        # reuse existing condition if present to avoid losing waiters
        if (!cancel_cond_map{uh}) {
            cancel_cond_map{uh} = new Condition();
        }
        return enqueueCmdLocked(IoCommand::Cancel, (uh,));
    }

    #! Waits for a queued cancel to complete
    private waitCancel(string uh) {
        AutoLock al(m);
        while (cancel_cond_map{uh}) {
            cancel_cond_map{uh}.wait(m);
        }
        # in case of a race condition where the operation completed before we could cancel it
        if (cancel_cond_map{uh}) {
            remove cancel_cond_map{uh};
        }
    }

    private startIntern() {
        @assert(m.trylock() == -1);
        io_waiting = True;
        on_exit io_waiting = False;
        while (io_exiting) {
            io_cond.wait(m);
        }
        if (tid) {
            return;
        }
        shutting_down = False;
        tid = background ioThread();
    }

    private ioThread() {
        log(LoggerLevel::INFO, "Started Socket polling operation controller I/O thread");
        hash<string, date> recv_debug_last;
        on_exit {
            {
                AutoLock al(m);
                ready_flag = False;
                processed_seq = 0;
                submit_seq = 0;
                processed_cond.broadcast();
                io_exiting = False;
                int cur_tid = gettid();
                if (tid == cur_tid) {
                    tid = 0;
                } else {
                    @debug {
                        if (getenv("QORE_ASYNCIO_DEBUG")) {
                            log(LoggerLevel::INFO,
                                "ioThread exit: tid mismatch (cur=%d active=%d)", cur_tid, tid);
                        }
                    }
                }
                if (io_waiting) {
                    io_cond.broadcast();
                }
            }
            log(LoggerLevel::INFO, "Exiting Socket polling operation controller I/O thread");
        }

        # Signal that I/O thread is ready
        {
            AutoLock al(m);
            ready_flag = True;
            io_cond.broadcast();
        }

        string notifier_hash = notifier.uniqueHash();
        hash<string, bool> ready_sockets;
        while (True) {
            # Process commands at start of each cycle to ensure deterministic handling
            if (processCommands()) {
                break;
            }
            try {
                *date poll_deadline;
                # Declare 'now' outside lock block so it can be used for poll_timeout calculation
                # This ensures poll_timeout is calculated relative to the same timestamp used for
                # timeout checks, avoiding race conditions where poll_deadline becomes negative
                date now;
                bool force_poll_cycle = False;
                bool do_autostop = False;
                # Deferred delivery list: callbacks and queue pushes are collected under the
                # lock and delivered after the lock is released.  This avoids LOCK-ERROR when
                # a callback calls submit() (which also acquires m).
                list<hash<auto>> deferred_deliveries;

                # ── PHASE 1: Snapshot under lock ──────────────────────────────
                # Collect the set of operations that need continuePoll() and
                # identify timed-out operations.  The lock is released before
                # any continuePoll() call so that submit() from worker threads
                # can add new operations without waiting for I/O processing.
                list<hash<auto>> ops_to_poll;
                {
                    AutoLock al(m);
                    @debug {
                        if (getenv("QORE_ASYNCIO_DEBUG")) {
                            log(LoggerLevel::INFO, "poll cycle start: cache_size=%d loop_size=%d",
                                cache.size(), loop.size());
                        }
                    }
                    if (force_poll) {
                        force_poll_cycle = True;
                        force_poll = False;
                    }
                    # Note: notifier is already registered in EventLoop from constructor
                    now = now_us();
                    foreach string key in (keys cache) {
                        hash<PollInfo> cinfo = cache{key};
                        # Only set timeout_date if a non-negative timeout was specified (negative means no timeout)
                        if (!cinfo.timeout_date && cinfo.to >= 0s) {
                            cinfo.timeout_date = cache{key}.timeout_date = now_us() + cinfo.to;
                        }
                        # Only check for timeout if timeout_date is set (not set when to < 0)
                        if (cinfo.timeout_date && cinfo.timeout_date <= now) {
                            @debug {
                                if (getenv("QORE_ASYNCIO_TRACE")) {
                                    log(LoggerLevel::INFO,
                                        "%s: timeout expired; state=%y sock=%s goal=%y listener_id=%y http2=%y to=%y timeout_date=%y now=%y",
                                        key, cinfo.spop.getState(),
                                        sprintf("%s %s", cinfo.sock.className(), cinfo.sock.uniqueHash()[0..8]),
                                        cinfo.other.goal, cinfo.other.listener_id, cinfo.other.http2,
                                        cinfo.to, cinfo.timeout_date, now);
                                }
                            }
                            ops_to_poll += {
                                "key": key,
                                "cinfo": cinfo,
                                "timed_out": True,
                            };
                            continue;
                        }
                        Socket poll_sock = cinfo.poll_info ? cinfo.poll_info.socket : cinfo.sock;
                        string sock_key = poll_sock.uniqueHash();
                        if (cinfo.poll_info
                            && !force_poll_cycle
                            && !(ready_sockets{sock_key} ?? False)) {
                            # Skip continuePoll for sockets that weren't marked ready by the kernel
                            @debug {
                                if (getenv("QORE_ASYNCIO_TRACE")) {
                                    bool only_notifier_ready = ready_sockets.size() == 1 && (ready_sockets{notifier_hash} ?? False);
                                    log(LoggerLevel::INFO,
                                        "%s: skip continuePoll for unready socket (state=%y sock=%s ready=%d only_notifier=%y goal=%y)",
                                        key, cinfo.spop.getState(),
                                        sprintf("%s %s", poll_sock.className(),
                                            poll_sock.uniqueHash()[0..8]),
                                        ready_sockets.size(), only_notifier_ready, cinfo.other.goal);
                                }
                            }
                            # Socket already registered in EventLoop; just update poll_deadline for timeout handling
                            if (cinfo.timeout_date && (!poll_deadline || (poll_deadline > cinfo.timeout_date))) {
                                poll_deadline = cinfo.timeout_date;
                            }
                            continue;
                        }
                        ops_to_poll += {
                            "key": key,
                            "cinfo": cinfo,
                        };
                    }
                    processed_seq = submit_seq;
                    processed_cond.broadcast();
                }

                # ── PHASE 2: continuePoll outside the lock ────────────────────
                # This is the main scalability improvement: worker threads can
                # call submit() while we drive I/O operations forward.
                hash<string, hash<auto>> poll_results;
                foreach hash<auto> op in (ops_to_poll) {
                    string key = op.key;
                    hash<PollInfo> cinfo = op.cinfo;
                    if (op.timed_out) {
                        try {
                            throw "SOCKET-TIMEOUT", sprintf("The I/O operation did not complete in the timeout "
                                "period (%y)", cinfo.to);
                        } catch (hash<ExceptionInfo> ex) {
                            poll_results{key} = {"ex": ex};
                        }
                        continue;
                    }
                    try {
                        @debug { string state = cinfo.spop.getState(); }
                        *hash<SocketPollInfo> info = cinfo.spop.continuePoll();
                        @debug { auto goal = cinfo.other.goal; }
                        poll_results{key} = {"info": info};
                    } catch (hash<ExceptionInfo> ex) {
                        if (ex.err == "SOCKET-HTTP-ERROR"
                            && ex.desc =~ /remote end closed connection/) {
                            log(LoggerLevel::DETAIL, "%s: remote end closed connection", key);
                        } else {
                            log(LoggerLevel::ERROR, "%s: %s", key, get_exception_string(ex));
                        }
                        poll_results{key} = {"ex": ex};
                    }
                }

                # ── PHASE 3: Update cache and EventLoop under lock ────────────
                # Apply poll results back to the cache.  EventLoop updates access
                # I/O-thread-only state but are done under the lock together with
                # cache mutations for simplicity.
                {
                    AutoLock al(m);
                    foreach hash<auto> op in (ops_to_poll) {
                        string key = op.key;
                        if (!cache{key}) {
                            continue;  # canceled during PHASE 2
                        }
                        hash<PollInfo> cinfo = op.cinfo;
                        *hash<auto> pr = poll_results{key};
                        if (pr.ex) {
                            # poll operation failed with an exception or timeout
                            updateEventLoopRegistration(key, NOTHING);
                            remove cache{key};
                            if (*Condition c = remove cancel_cond_map{key}) {
                                c.broadcast();
                            }
                            hash<SocketPollResultInfo> err_result = <SocketPollResultInfo>{
                                "sock": cinfo.sock,
                                "spop": cinfo.spop,
                                "ex": pr.ex,
                                "other": cinfo.other,
                            };
                            deferred_deliveries += {
                                "key": key,
                                "callback": cinfo.callback,
                                "q": cinfo.q,
                                "result": err_result,
                            };
                        } else {
                            *hash<SocketPollInfo> info = pr.info;
                            cache{key}.poll_info = info;
                            if (!info) {
                                # operation successful - remove from EventLoop
                                updateEventLoopRegistration(key, NOTHING);
                                remove cache{key};
                                if (*Condition c = remove cancel_cond_map{key}) {
                                    c.broadcast();
                                }
                                hash<SocketPollResultInfo> result = <SocketPollResultInfo>{
                                    "sock": cinfo.sock,
                                    "spop": cinfo.spop,
                                    "other": cinfo.other,
                                };
                                deferred_deliveries += {
                                    "key": key,
                                    "callback": cinfo.callback,
                                    "q": cinfo.q,
                                    "result": result,
                                };
                            } else {
                                # Register or update socket in EventLoop
                                updateEventLoopRegistration(key, info.socket, info.events);
                                if (cinfo.timeout_date && (!poll_deadline || (poll_deadline > cinfo.timeout_date))) {
                                    poll_deadline = cinfo.timeout_date;
                                }
                            }
                        }
                    }

                    @debug {
                        if (getenv("QORE_ASYNCIO_DEBUG")) {
                            log(LoggerLevel::INFO, "poll cycle ready: cache=%d loop_size=%d",
                                cache.size(), loop.size());
                        }
                    }

                    if (!cache && autostop && !cmdq.size() && !deferred_deliveries) {
                        log(LoggerLevel::DEBUG, "All operations have completed; exiting the I/O thread");
                        io_exiting = True;
                        if (io_waiting) {
                            io_cond.broadcast();
                        }
                        do_autostop = True;
                    }
                }
                # Deliver results outside the lock so callbacks can safely call submit()
                foreach hash<auto> delivery in (deferred_deliveries) {
                    if (delivery.callback) {
                        @debug(log(LoggerLevel::INFO, "delivering result via callback for key %y",
                            delivery.key));
                        try {
                            delivery.callback(delivery.result);
                        } catch (hash<ExceptionInfo> cbex) {
                            log(LoggerLevel::ERROR, "%s: callback exception: %s",
                                delivery.key, get_exception_string(cbex));
                        }
                    } else if (delivery.q) {
                        delivery.q.push(delivery.result);
                    }
                }
                if (!do_autostop && deferred_deliveries) {
                    # Re-check autostop after deliveries: callbacks may have submitted
                    # new operations via submit(), so cache may no longer be empty
                    AutoLock al(m);
                    if (!cache && autostop && !cmdq.size()) {
                        log(LoggerLevel::DEBUG, "All operations have completed after deliveries; exiting the I/O thread");
                        io_exiting = True;
                        if (io_waiting) {
                            io_cond.broadcast();
                        }
                        do_autostop = True;
                    }
                }
                if (do_autostop) {
                    break;
                }
                # NOTE: We use the same 'now' timestamp for poll_timeout calculation as used for
                # timeout checks to avoid race conditions where poll_deadline becomes negative
                # due to time passing during EventLoop registration
                date poll_timeout = -1s;
                if (poll_deadline) {
                    poll_timeout = poll_deadline - now;
                    if (poll_timeout < 0s) {
                        # Timeout already expired - use minimum poll to check for timeouts
                        poll_timeout = 0s;
                    }
                }
                @debug {
                    if (getenv("QORE_ASYNCIO_TRACE")) {
                        log(LoggerLevel::INFO, "EventLoop::poll() call: items=%d timeout=%y deadline=%y",
                            loop.size(), poll_timeout, poll_deadline);
                    }
                }
                @debug {
                    if (getenv("QORE_ASYNCIO_PIPE_DEBUG")) {
                        log(LoggerLevel::INFO,
                            "EventLoop::poll() pre: items=%d cmdq=%d shutting_down=%y",
                            loop.size(), cmdq.size(), shutting_down);
                    }
                }
                date poll_start = now_us();
                # Use persistent EventLoop for efficient kqueue/epoll/poll
                *list<hash<SocketPollInfo>> ready_list = loop.poll(poll_timeout);
                ready_sockets = {};
                if (ready_list) {
                    foreach hash<SocketPollInfo> info in (ready_list) {
                        if (info.socket) {
                            ready_sockets{info.socket.uniqueHash()} = True;
                        }
                    }
                }
                @debug {
                    if (getenv("QORE_ASYNCIO_TRACE")) {
                        int non_notifier_ready = 0;
                        if (ready_list) {
                            foreach hash<SocketPollInfo> info in (ready_list) {
                                if (info.socket && info.socket.uniqueHash() != notifier_hash) {
                                    ++non_notifier_ready;
                                }
                            }
                        }
                        log(LoggerLevel::INFO, "EventLoop::poll() returned: items=%d elapsed=%y",
                            loop.size(), now_us() - poll_start);
                        if (ready_list && ready_list.size() && !non_notifier_ready) {
                            log(LoggerLevel::INFO,
                                "EventLoop::poll() ready_list only notifier; items=%d cmdq=%d notifier_valid=%y",
                                ready_list.size(), cmdq.size(), notifier.isValid());
                        }
                        if (ready_list) {
                            log(LoggerLevel::INFO, "EventLoop::poll() ready_list: %y", (map $1 + {
                                "events": PollEventMap{$1.events} ?? "unknown",
                                "events_raw": $1.events,
                                "socket": sprintf("%s %s", $1.socket.className(), $1.socket.uniqueHash()[0..8]),
                            }, ready_list));
                        }
                    }
                }

                # first process commands, if any
                if (processCommands()) {
                    break;
                }
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "%s", get_exception_string(ex));
            }
        }
        # process any final commands
        processCommands();
    }

    private *bool processCommands() {
        # Process any queued commands first without blocking
        #
        # IMPORTANT: There is a race condition between command enqueueing and notification.
        # The main thread does: 1) enqueue command (with lock), 2) notify (without lock)
        # The I/O thread does: 1) process commands (with lock), 2) acknowledge (without lock)
        #
        # Race scenario:
        # - I/O thread: finds cmdq empty, exits command loop
        # - Main thread: enqueues command, notifies
        # - I/O thread: acknowledge loop consumes notification
        # - I/O thread: returns without processing the new command!
        #
        # Fix: After acknowledging, check if new commands arrived during the acknowledge.
        # Loop until both the command queue AND notifications are consumed.
        try {
            @debug {
                if (getenv("QORE_ASYNCIO_PIPE_DEBUG")) {
                    log(LoggerLevel::INFO, "processCommands: enter cmdq=%d tid=%d",
                        cmdq.size(), gettid());
                }
            }
            @debug(int cmd_count = 0);
            # Outer loop: repeat until both cmdq and notifier are empty
            while (True) {
                # Inner loop: process all commands in the queue
                while (True) {
                    hash<auto> h;
                    bool no_cmds = False;
                    {
                        AutoLock al(m);
                        if (!cmdq.size()) {
                            no_cmds = True;
                        } else {
                            h = shift cmdq;
                        }
                    }
                    if (no_cmds) {
                        break;
                    }
                @debug(++cmd_count);
                list<auto> args = h.args ?? ();
                switch (h.cmd) {
                    case IoCommand::Quit: {
                        # cancel all operations in progress without holding the lock while aborting
                        list<hash<PollInfo>> cancel_list = ();
                        list<Condition> cond_list = ();
                        {
                            AutoLock al(m);
                            @debug(log(LoggerLevel::INFO, "AsyncSocketIoController canceling %d operations for shutdown",
                                cache.size()));
                            foreach string key in (keys cache) {
                                # Remove socket from EventLoop
                                updateEventLoopRegistration(key, NOTHING);
                                cancel_list += cache{key};
                                if (*Condition c = remove cancel_cond_map{key}) {
                                    cond_list += c;
                                }
                                remove cache{key};
                            }
                        }
                        # IMPORTANT: doCancelIntern() must complete BEFORE broadcasting to waiters
                        # This ensures socket references are released before callers return
                        foreach hash<PollInfo> item in (cancel_list) {
                            doCancelIntern(item);
                        }
                        foreach Condition item in (cond_list) {
                            item.broadcast();
                        }
                        return True;
                    }

                    case IoCommand::Add:
                        # processed automatically
                        break;

                    case IoCommand::Cancel: {
                        hash<PollInfo> cinfo;
                        *Condition c;
                        {
                            AutoLock al(m);
                            if (cache{args[0]}) {
                                # Remove socket from EventLoop
                                updateEventLoopRegistration(args[0], NOTHING);
                                cinfo = cache{args[0]};
                                remove cache{args[0]};
                                c = remove cancel_cond_map{args[0]};
                            }
                        }
                        if (!cinfo) {
                            @debug(log(LoggerLevel::INFO, "AsyncSocketIoController cancel request for unknown key: %y",
                                args[0]));
                        }
                        # IMPORTANT: doCancelIntern() must complete BEFORE broadcasting to waiters
                        # This ensures the socket reference in cinfo is released before the caller
                        # returns, preventing race conditions where a new listener tries to bind
                        # to the same port while the old socket is still referenced
                        if (cinfo) {
                            doCancelIntern(cinfo);
                        }
                        if (c) {
                            c.broadcast();
                        }
                        break;
                    }

                    case IoCommand::CancelOwner: {
                        # Cancel all operations belonging to a specific owner
                        string owner = args[0];
                        *Condition done_cond = args[1];
                        list<hash<PollInfo>> cancel_list = ();
                        list<Condition> cond_list = ();
                        {
                            AutoLock al(m);
                            @debug(log(LoggerLevel::INFO, "AsyncSocketIoController canceling operations for owner %s",
                                owner));
                            foreach string key in (keys cache) {
                                if (cache{key}.owner == owner) {
                                    # Remove socket from EventLoop
                                    updateEventLoopRegistration(key, NOTHING);
                                    cancel_list += cache{key};
                                    if (*Condition c = remove cancel_cond_map{key}) {
                                        cond_list += c;
                                    }
                                    remove cache{key};
                                }
                            }
                        }
                        # IMPORTANT: doCancelIntern() must complete BEFORE broadcasting to waiters
                        # This ensures socket references are released before callers return
                        foreach hash<PollInfo> pi in (cancel_list) {
                            doCancelIntern(pi);
                        }
                        foreach Condition c in (cond_list) {
                            c.broadcast();
                        }
                        # Notify the caller that all cancellations are complete
                        @debug(log(LoggerLevel::INFO, "AsyncSocketIoController canceled %d operations for owner %s",
                            cancel_list.size(), owner));
                        if (done_cond) {
                            done_cond.broadcast();
                        }
                        break;
                    }

                    case IoCommand::Wake:
                        {
                            AutoLock al(m);
                            force_poll = True;
                        }
                        break;
                }
                }  # end of inner command processing loop

                # Acknowledge all pending notifications from the EventNotifier
                @debug {
                    if (getenv("QORE_ASYNCIO_TRACE")) {
                        log(LoggerLevel::INFO, "processCommands: before acknowledge cmdq=%d", cmdq.size());
                    }
                }
                notifier.acknowledge();

                # Check if new commands arrived during the drain (fixes race condition)
                # If cmdq has commands, loop back to process them before returning
                bool queue_empty;
                {
                    AutoLock al(m);
                    queue_empty = !cmdq.size();
                    @debug {
                        if (getenv("QORE_ASYNCIO_PIPE_DEBUG")) {
                            if (queue_empty) {
                                log(LoggerLevel::INFO, "processCommands: done cmd_count=%d cmdq=%d",
                                    cmd_count, cmdq.size());
                            } else {
                                log(LoggerLevel::INFO,
                                    "processCommands: new commands arrived during drain, looping back cmdq=%d",
                                    cmdq.size());
                            }
                        }
                    }
                }
                if (queue_empty) {
                    break;  # Exit outer loop
                }
            }  # end of outer loop
        } catch(hash<ExceptionInfo> ex) {
            if (ex.err != "QUEUE-TIMEOUT") {
                rethrow;
            }
            @debug {
                if (getenv("QORE_ASYNCIO_TRACE")) {
                    log(LoggerLevel::INFO, "processCommands: queue timeout (cmdq=%d)", cmdq.size());
                }
            }
        }
    }

    private doCancelIntern(hash<PollInfo> cinfo) {
        try {
            @debug(log(LoggerLevel::INFO, "canceling async I/O op=%s sock=%s",
                cinfo.spop.className(),
                sprintf("%s %s", cinfo.sock.className(), cinfo.sock.uniqueHash()[0..8])));
            cinfo.spop.abort();
            @debug(log(LoggerLevel::INFO, "canceled async I/O op=%s sock=%s",
                cinfo.spop.className(),
                sprintf("%s %s", cinfo.sock.className(), cinfo.sock.uniqueHash()[0..8])));
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Ignoring error aborting canceled async I/O: %s", get_exception_string(ex));
        }
        # deliver cancelation result
        hash<SocketPollResultInfo> cancel_result = <SocketPollResultInfo>{
            "sock": cinfo.sock,
            "spop": cinfo.spop,
            "canceled": True,
            "other": cinfo.other,
        };
        if (cinfo.callback) {
            @debug(log(LoggerLevel::INFO, "delivering cancel via callback for sock %s",
                sprintf("%s %s", cinfo.sock.className(), cinfo.sock.uniqueHash()[0..8])));
            try {
                cinfo.callback(cancel_result);
            } catch (hash<ExceptionInfo> cbex) {
                log(LoggerLevel::ERROR, "cancel callback exception for sock %s: %s",
                    cinfo.sock.uniqueHash(), get_exception_string(cbex));
            }
        } else {
            cinfo.q.push(cancel_result);
        }
    }

    private bool enqueueCmdLocked(string cmd, *list<auto> args) {
        @assert(m.trylock() == -1);
        if (io_exiting || !tid) {
            startIntern();
        }
        cmdq += {
            "cmd": cmd,
            "args": args,
        };
        # Always signal the I/O thread to ensure prompt wakeup
        # The cost of extra notifier signals is negligible compared to potential lost wakeups
        return True;
    }

    private bool sendCmdLocked(string cmd, *list<auto> args) {
        @assert(m.trylock() == -1);
        return enqueueCmdLocked(cmd, args);
    }

    private sendCmd(string cmd) {
        bool signal = False;
        @debug {
            if (getenv("QORE_ASYNCIO_PIPE_DEBUG")) {
                log(LoggerLevel::INFO, "sendCmd: cmd=%y args=%y cmdq=%d tid=%d notifier_valid=%y",
                    cmd, argv, cmdq.size(), gettid(), notifier.isValid());
            }
        }
        {
            AutoLock al(m);
            signal = sendCmdLocked(cmd, argv);
        }
        if (signal) {
            notifier.notify();
        }
    }

    #! Computes the event mask union for all operations on the given socket hash
    /** @param sock_hash the unique hash of the socket to compute the event union for

        @return the union of all event masks for operations on the given socket

        @note Uses the reverse index sock_hash_to_keys for O(k) lookup where k is the
        number of operations per socket (typically 1), avoiding the former O(N) full scan.
    */
    private int computeEventUnion(string sock_hash) {
        int result = 0;
        foreach string k in (keys sock_hash_to_keys{sock_hash}) {
            result |= key_events{k};
        }
        return result;
    }

    #! Applies the correct event union to the EventLoop for a socket
    private applyEventUnion(Socket socket, string sock_hash) {
        int union_events = computeEventUnion(sock_hash);
        int prev_events = registered_events{sock_hash} ?? 0;
        if (union_events != prev_events) {
            loop.modify(socket, union_events);
            registered_events{sock_hash} = union_events;
            @debug {
                if (getenv("QORE_ASYNCIO_TRACE")) {
                    log(LoggerLevel::DEBUG, "EventLoop modify union: sock=%s events=%d->%d",
                        sock_hash[0..8], prev_events, union_events);
                }
            }
        }
    }

    #! Updates EventLoop registration for a socket
    /** @param key the operation cache key
        @param socket the socket to register (or NOTHING to unregister)
        @param events the poll events (SOCK_POLLIN and/or SOCK_POLLOUT)

        Uses reference counting to handle multiple operations sharing the same socket
        (e.g., multiple concurrent accept operations on the same listener socket)
    */
    private updateEventLoopRegistration(string key, *Socket socket, *int events) {
        *Socket prev_sock = registered_sockets{key};
        string prev_sock_hash = prev_sock ? prev_sock.uniqueHash() : "";
        string new_sock_hash = socket ? socket.uniqueHash() : "";

        if (!socket) {
            # Remove registration for this key
            remove key_events{key};
            if (prev_sock) {
                # Update reverse index
                remove sock_hash_to_keys{prev_sock_hash}{key};
                if (!sock_hash_to_keys{prev_sock_hash}) {
                    remove sock_hash_to_keys{prev_sock_hash};
                }
                remove registered_sockets{key};
                # Decrement reference count
                int refcount = socket_refcounts{prev_sock_hash} ?? 0;
                if (refcount <= 1) {
                    # Last reference - actually remove from EventLoop
                    try {
                        loop.del(prev_sock);
                        @debug {
                            if (getenv("QORE_ASYNCIO_TRACE")) {
                                log(LoggerLevel::DEBUG, "EventLoop remove: key=%s sock=%s refcount=0",
                                    key, prev_sock_hash[0..8]);
                            }
                        }
                    } catch (hash<ExceptionInfo> ex) {
                        @debug {
                            if (getenv("QORE_ASYNCIO_TRACE")) {
                                log(LoggerLevel::DEBUG, "EventLoop remove ignored error: %s", ex.err);
                            }
                        }
                    }
                    remove registered_events{prev_sock_hash};
                    remove socket_refcounts{prev_sock_hash};
                } else {
                    # Other operations still using this socket - update refcount and recompute union
                    socket_refcounts{prev_sock_hash} = refcount - 1;
                    applyEventUnion(prev_sock, prev_sock_hash);
                    @debug {
                        if (getenv("QORE_ASYNCIO_TRACE")) {
                            log(LoggerLevel::DEBUG, "EventLoop deref: key=%s sock=%s refcount=%d",
                                key, prev_sock_hash[0..8], refcount - 1);
                        }
                    }
                }
            }
            return;
        }

        # Track per-key events
        key_events{key} = events;

        if (new_sock_hash == prev_sock_hash) {
            # Same socket - recompute event union; reverse index already correct
            applyEventUnion(socket, new_sock_hash);
        } else {
            # Different socket or new registration
            if (prev_sock) {
                # Remove key from previous socket's reverse index entry
                remove sock_hash_to_keys{prev_sock_hash}{key};
                if (!sock_hash_to_keys{prev_sock_hash}) {
                    remove sock_hash_to_keys{prev_sock_hash};
                }
                # Decrement refcount for previous socket
                int old_refcount = socket_refcounts{prev_sock_hash} ?? 0;
                if (old_refcount <= 1) {
                    try {
                        loop.del(prev_sock);
                    } catch (hash<ExceptionInfo> ex) {
                        @debug {
                            if (getenv("QORE_ASYNCIO_TRACE")) {
                                log(LoggerLevel::DEBUG, "EventLoop remove (for replace) ignored error: %s", ex.err);
                            }
                        }
                    }
                    remove registered_events{prev_sock_hash};
                    remove socket_refcounts{prev_sock_hash};
                } else {
                    socket_refcounts{prev_sock_hash} = old_refcount - 1;
                    applyEventUnion(prev_sock, prev_sock_hash);
                }
            }
            # Add key to new socket's reverse index entry
            sock_hash_to_keys{new_sock_hash}{key} = True;
            # Add/update new socket
            registered_sockets{key} = socket;
            int new_refcount = socket_refcounts{new_sock_hash} ?? 0;
            if (new_refcount == 0) {
                # First registration for this socket - use the event union (may just be this key's events)
                int union_events = computeEventUnion(new_sock_hash);
                loop.add(socket, union_events);
                socket_refcounts{new_sock_hash} = 1;
                registered_events{new_sock_hash} = union_events;
                @debug {
                    if (getenv("QORE_ASYNCIO_TRACE")) {
                        log(LoggerLevel::DEBUG, "EventLoop add: key=%s sock=%s events=%d refcount=1",
                            key, new_sock_hash[0..8], union_events);
                    }
                }
            } else {
                # Socket already registered by another operation - update refcount and recompute union
                socket_refcounts{new_sock_hash} = new_refcount + 1;
                applyEventUnion(socket, new_sock_hash);
                @debug {
                    if (getenv("QORE_ASYNCIO_TRACE")) {
                        log(LoggerLevel::DEBUG, "EventLoop ref: key=%s sock=%s refcount=%d",
                            key, new_sock_hash[0..8], new_refcount + 1);
                    }
                }
            }
        }
    }
}

#! Returns the global async I/O controller singleton
/** The controller is created during module initialization with autostop enabled.
*/
public AsyncSocketIoController sub getGlobalAsyncIoController() {
    return globalController;
}

our *AsyncSocketIoController globalController;
}

# private symbols
namespace Priv {
# internal poll info
hashdecl PollInfo {
    #! Poll start timestamp; not set if timeout is disabled (to <= 0)
    *date timeout_date;

    #! Socket connection
    Socket sock;

    #! Poll operation
    AbstractPollOperation spop;

    #! Last poll info for this operation
    *hash<SocketPollInfo> poll_info;

    #! I/O timeout; negative means no timeout
    date to = AsyncSocketIoController::DefaultIoOperationTimeout;

    #! Owner identifier for cancellation
    string owner;

    #! Other data stored alongside the socket
    *hash<auto> other;

    #! Result Queue (NOTHING when using callback mode)
    *Queue q;

    #! Optional completion callback for I/O thread delivery
    *code callback;
}
}
