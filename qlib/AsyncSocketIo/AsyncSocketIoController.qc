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

        #! I/O thread command: add
        const IO_ADD = "add";

        #! I/O thread command: cancel
        const IO_CANCEL = "cancel";

        #! I/O thread command: cancel by owner
        const IO_CANCEL_OWNER = "cancel_owner";

        #! I/O thread command: quit
        const IO_QUIT = "quit";

        #! I/O thread command: wake
        const IO_WAKE = "wake";
    }

    private:internal {
        #! File to signal the I/O thread
        File sem_write;

        #! File to read in the I/O thread
        ReadOnlyFile sem_read;

        #! Semaphore descriptor info
        hash<SocketPollInfo> sem_info;

        #! True if a signal is pending on the command pipe (wake byte already queued)
        bool sem_pending = False;

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

        #! Persistent event loop for efficient kqueue/epoll/poll
        EventLoop loop();

        #! Map of operation key -> currently registered socket
        /** Tracks which socket is registered in the EventLoop for each operation,
            since the poll socket can change between continuePoll() calls
        */
        hash<string, Socket> registered_sockets;

        #! Map of registered socket unique hash -> current poll events
        /** Tracks the events currently registered for each socket in the EventLoop
        */
        hash<string, int> registered_events;
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

    #! Set autostop flag
    setAutostop(bool autostop = True) {
        self.autostop = autostop;
    }

    #! Get autostop flag
    bool getAutostop() {
        return autostop;
    }

    #! Starts monitoring
    /** @throw CONTROLLER-START-ERROR if the I/O thread is already running
    */
    start() {
        AutoLock al(m);

        if (tid) {
            throw "CONTROLLER-START-ERROR", sprintf("I/O thread is already running in TID %d", tid);
        }

        shutting_down = False;
        startIntern();
    }

    #! Stops monitoring
    stop() {
        bool signal = False;
        {
            AutoLock al(m);
            if (tid) {
                shutting_down = True;
                signal = enqueueCmdLocked(IO_QUIT);
            } else {
                shutting_down = True;
            }
        }
        if (signal) {
            sem_write.write(".");
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
                signal = enqueueCmdLocked(IO_QUIT);
            }
        }
        if (signal) {
            sem_write.write(".");
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

        @param timeout maximum time to wait; use @ref NOTHING for no timeout

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

    #! Submits a socket operation to be performed and returns a Queue where the results will be returned
    /** @param info a hash with the socket operation and supporting info
        @param replace if True, then any operation currently in place for the given socket will be replaced

        @return Queue a Queue that will have the result of the operation pushed to it; if \a info.resultQueue is
        provided, that Queue is returned

        @throw CONTROLLER-ADD-ERROR invalid or incomplete data in \a info; a different operation on this socket is
        already in progress
    */
    Queue submit(hash<SocketPollOperationInfo> info, *bool replace) {
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
        # get unique key for socket object
        string uh = info.sock.uniqueHash();
        hash<PollInfo> cinfo;
        bool cancel_signal = False;
        bool wait_cancel = False;
        Queue result_queue = info.resultQueue ?? new Queue();
        {
            AutoLock al(m);
            if (shutting_down) {
                throw "CONTROLLER-ADD-ERROR", sprintf("Cannot add operation for socket %y; controller is shutting down",
                    uh);
            }
            if (*hash<PollInfo> existing = cache{uh}) {
                if (replace) {
                    cancel_signal = enqueueCancelLocked(uh);
                    wait_cancel = True;
                } else {
                    # otherwise throw an exception
                    throw "CONTROLLER-ADD-ERROR", sprintf("An operation with socket %y is already in process", uh);
                }
            }
        }
        if (cancel_signal) {
            sem_write.write(".");
        }
        if (wait_cancel) {
            waitCancel(uh);
        }
        bool add_signal = False;
        {
            AutoLock al(m);
            if (shutting_down) {
                throw "CONTROLLER-ADD-ERROR", sprintf("Cannot add operation for socket %y; controller is shutting down",
                    uh);
            }
            cinfo = cache{uh} = <PollInfo>{
                "sock": info.sock,
                "spop": info.spop,
                "to": info.to,
                "owner": info.owner,
                "other": info.other,
                "q": result_queue,
            };
            @debug {
                if (getenv("QORE_ASYNCIO_DEBUG")) {
                    log(LoggerLevel::INFO, "submit: added operation for socket %s goal=%y to=%y cache_size=%d",
                        uh, info.other.goal, info.to, cache.size());
                }
                if (getenv("QORE_ASYNCIO_TRACE")) {
                    log(LoggerLevel::DEBUG, "submit: info=%y", info);
                }
            }
            if (io_exiting || !tid) {
                startIntern();
            }
            add_signal = enqueueCmdLocked(IO_ADD);
        }
        if (add_signal) {
            sem_write.write(".");
        }
        return cinfo.q;
    }

    #! Executes a socket operation asynchronously and waits for the results
    /** @param info a hash with the socket operation and supporting info
        @param replace if True, then any operation currently in place for the given socket will be replaced

        @return The result of the asynchronous operation

        @throw CONTROLLER-ADD-ERROR invalid or incomplete data in \a info; a different operation on this socket is
        already in progress
    */
    hash<SocketPollResultInfo> exec(hash<SocketPollOperationInfo> info, *bool replace) {
        return submit(info, replace).get();
    }

    #! Cancels any operation in progress for the given Socket
    /** @param sock the socket to cancel any operation in progress for

        @return @ref True if an operation for the socket was canceled, @ref False if not

        @note Stops the I/O thread if the last operation is canceled and the \a autostop flag is enabled

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
            sem_write.write(".");
        }
        if (rv) {
            waitCancel(uh);
        }
        {
            AutoLock al(m);
            if (autostop && rv && !cache && tid) {
                stop_signal = enqueueCmdLocked(IO_QUIT);
                stopped = True;
            }
        }
        if (stop_signal) {
            sem_write.write(".");
        }
        if (stopped) {
            waitStop();
        }
        return rv;
    }

    #! Wakes the I/O thread to re-evaluate poll state
    wake() {
        sendCmd(IO_WAKE);
    }

    #! Cancels any operation in progress for the given Socket
    /** @param sock the socket to cancel any operation in progress for

        @return @ref True if an operation for the socket was canceled, @ref False if not

        @note Stops the I/O thread if the last operation is canceled and the \a autostop flag is enabled

        @throw CONTROLLER-CANCEL-ERROR thrown if the Socket has no operation in progress

        @see cancel()
    */
    cancelEx(Socket sock) {
        if (!cancel(sock)) {
            throw "CONTROLLER-CANCEL-ERROR", sprintf("Socket %y has no operation in progress", sock.uniqueHash());
        }
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
            signal = enqueueCmdLocked(IO_CANCEL_OWNER, (owner,));
        }
        if (signal) {
            sem_write.write(".");
        }
        # wait for the I/O thread to complete cancellation
        # we use a simple polling approach since multiple operations may be canceled
        while (True) {
            {
                AutoLock al(m);
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
            }
            usleep(1ms);
        }
        return rv;
    }

    #! Common constructor code
    private constructorInit() {
        hash<PipeInfo> info = File::getPipe();
        sem_read = info.read;
        sem_write = info.write;
        sem_info = <SocketPollInfo>{
            "events": SOCK_POLLIN,
            "socket": sem_read,
        };
        # Register semaphore pipe in persistent EventLoop
        loop.add(sem_read, SOCK_POLLIN);
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
        return enqueueCmdLocked(IO_CANCEL, (uh,));
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
                io_exiting = False;
                sem_pending = False;
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

        string sem_hash = sem_read.uniqueHash();
        hash<string, bool> ready_sockets;
        while (True) {
            # Process commands at start of each cycle to ensure deterministic handling
            # even if poll() doesn't wake up from pipe writes (platform-specific issues)
            if (processCommands()) {
                break;
            }
            try {
                *date poll_deadline;
                # Declare 'now' outside lock block so it can be used for poll_timeout calculation
                # This ensures poll_timeout is calculated relative to the same timestamp used for
                # timeout checks, avoiding race conditions where poll_deadline becomes negative
                date now;
                {
                    AutoLock al(m);
                    @debug {
                        if (getenv("QORE_ASYNCIO_DEBUG")) {
                            log(LoggerLevel::INFO, "poll cycle start: cache_size=%d loop_size=%d",
                                cache.size(), loop.size());
                        }
                    }
                    # Note: sem_read is already registered in EventLoop from constructor
                    # scan for timed out operations
                    now = now_us();
                    # take the first key that has not already been processed on this run
                    hash<string, bool> dmap;
                    while ((*string key = (cache - keys dmap).firstKey()).val()) {
                        hash<PollInfo> cinfo = cache{key};
                        # Only set timeout_date if a non-negative timeout was specified (negative means no timeout)
                        if (!cinfo.timeout_date && cinfo.to >= 0s) {
                            cinfo.timeout_date = cache{key}.timeout_date = now_us() + cinfo.to;
                        }
                        dmap{key} = True;
                        Socket sock = cinfo.sock;
                        Socket poll_sock = cinfo.poll_info ? cinfo.poll_info.socket : sock;
                        string sock_key = poll_sock.uniqueHash();
                        try {
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
                                throw "SOCKET-TIMEOUT", sprintf("The I/O operation did not complete in the timeout "
                                    "period (%y)", cinfo.to);
                            }
                            if (cinfo.poll_info
                                && !(ready_sockets{sock_key} ?? False)) {
                                # Skip continuePoll for sockets that weren't marked ready by the kernel
                                # Note: On macOS, kqueue is used instead of poll() which properly detects
                                # pending connections on listener sockets. Socket is already registered
                                # in EventLoop, so no action needed.
                                @debug {
                                    if (getenv("QORE_ASYNCIO_TRACE")) {
                                        bool only_sem_ready = ready_sockets.size() == 1 && (ready_sockets{sem_hash} ?? False);
                                        log(LoggerLevel::INFO,
                                            "%s: skip continuePoll for unready socket (state=%y sock=%s ready=%d only_sem=%y goal=%y)",
                                            key, cinfo.spop.getState(),
                                            sprintf("%s %s", poll_sock.className(),
                                                poll_sock.uniqueHash()[0..8]),
                                            ready_sockets.size(), only_sem_ready, cinfo.other.goal);
                                    }
                                }
                                # Socket already registered in EventLoop; just update poll_deadline for timeout handling
                                if (cinfo.timeout_date && (!poll_deadline || (poll_deadline > cinfo.timeout_date))) {
                                    poll_deadline = cinfo.timeout_date;
                                }
                                continue;
                            }
                            bool first_poll = !cinfo.poll_info;
                            *hash<SocketPollInfo> info = cinfo.spop.continuePoll();
                            @debug {
                                if (getenv("QORE_ASYNCIO_TRACE")) {
                                    log(LoggerLevel::INFO, "%y: state: %y: got poll info: %y", key,
                                        cinfo.spop.getState(), info ? {
                                            "events": PollEventMap{info.events} ?? "unknown",
                                            "socket": sprintf("%s %s", info.socket.className(),
                                                info.socket.uniqueHash()[0..8]),
                                            "timeout_date": cinfo.timeout_date,
                                            "now": now,
                                            "to": cinfo.to,
                                        } : NOTHING);
                                }
                                # Specific debug for accept operations
                                if (getenv("QORE_ACCEPT_POLL_DEBUG")) {
                                    *string goal = cinfo.other.goal;
                                    if (goal && goal =~ /^listener:/) {
                                        log(LoggerLevel::INFO,
                                            "ACCEPT_POLL: key=%y goal=%y first_poll=%y state=%y info=%y "
                                            "listener_fd=%y",
                                            key, goal, first_poll, cinfo.spop.getState(),
                                            info ? sprintf("events=%d need_poll=%y", info.events, info.events > 0)
                                                : "COMPLETE",
                                            cinfo.sock.isOpen() ? cinfo.sock.getSocket() : "closed");
                                    }
                                }
                            }
                            @debug {
                                if (getenv("QORE_ASYNCIO_TRACE")
                                    && info
                                    && cinfo.spop.getState() == "receiving") {
                                    date last = recv_debug_last{key} ?? 0s;
                                    if (!last || (now - last) > 5s) {
                                        recv_debug_last{key} = now;
                                        string sock_desc = sprintf("%s %s", cinfo.sock.className(),
                                            cinfo.sock.uniqueHash()[0..8]);
                                        string sock_fd = "unknown";
                                        try {
                                            int fd = cinfo.sock.getSocket();
                                            sock_fd = fd >= 0 ? sprintf("%d", fd) : "closed";
                                        } catch (hash<ExceptionInfo> ex) {
                                            # ignore socket info errors in debug logging
                                        }
                                        log(LoggerLevel::INFO,
                                            "%s: receiving poll spin; sock=%s fd=%s timeout_date=%y now=%y to=%y goal=%y state=%y listener_id=%y http2=%y",
                                            key, sock_desc, sock_fd,
                                            cinfo.timeout_date, now, cinfo.to, cinfo.other.goal,
                                            cinfo.other.state, cinfo.other.listener_id, cinfo.other.http2);
                                    }
                                }
                            }
                            cinfo.poll_info = cache{key}.poll_info = info;
                            if (!info) {
                                # operation successful - remove from EventLoop
                                updateEventLoopRegistration(key, NOTHING);
                                remove cache{key};
                                if (*Condition c = remove cancel_cond_map{key}) {
                                    c.broadcast();
                                }
                                cinfo.q.push(<SocketPollResultInfo>{
                                    "sock": cinfo.sock,
                                    "spop": cinfo.spop,
                                    "other": cinfo.other,
                                });
                                remove dmap{key};
                            } else {
                                # Register or update socket in EventLoop
                                updateEventLoopRegistration(key, info.socket, info.events);
                                # Find the MINIMUM (earliest) timeout so poll returns in time to handle it
                                # Only consider operations with timeouts
                                if (cinfo.timeout_date && (!poll_deadline || (poll_deadline > cinfo.timeout_date))) {
                                    poll_deadline = cinfo.timeout_date;
                                }
                            }
                        } catch (hash<ExceptionInfo> ex) {
                            @debug {
                                if (getenv("QORE_ASYNCIO_TRACE")) {
                                    int lvl = (ex.err == "SOCKET-HTTP-ERROR"
                                        && ex.desc =~ /remote end closed connection/)
                                        ? LoggerLevel::DEBUG : LoggerLevel::ERROR;
                                    log(lvl, "%y: state: %y: poll exception: %s (sock=%s goal=%y)",
                                        key, cinfo.spop.getState(), get_exception_string(ex),
                                        sprintf("%s %s", cinfo.sock.className(), cinfo.sock.uniqueHash()[0..8]),
                                        cinfo.other.goal);
                                }
                            }
                            if (ex.err == "SOCKET-HTTP-ERROR"
                                && ex.desc =~ /remote end closed connection/) {
                                @debug {
                                    if (getenv("QORE_ASYNCIO_DEBUG")) {
                                        string peer_desc = "unknown";
                                        # Avoid SOCKET-NOT-OPEN when logging peer info after close
                                        if (cinfo.sock.isOpen()) {
                                            hash<auto> peer_info = cinfo.sock.getPeerInfo();
                                            peer_desc = peer_info."address_desc"
                                                ?? peer_info."address"
                                                ?? peer_info."host"
                                                ?? "unknown";
                                        }
                                        string sock_id = sprintf("%s %s", cinfo.sock.className(),
                                            cinfo.sock.uniqueHash()[0..8]);
                                        log(LoggerLevel::INFO,
                                            "%s: remote end closed connection (sock=%s peer=%s goal=%y)",
                                            key, sock_id, peer_desc, cinfo.other.goal);
                                    }
                                }
                                log(LoggerLevel::DETAIL, "%s: remote end closed connection", key);
                            } else {
                                log(LoggerLevel::ERROR, "%s: %s", key, get_exception_string(ex));
                            }
                            # poll operation failed with an exception - remove from EventLoop
                            updateEventLoopRegistration(key, NOTHING);
                            remove cache{key};
                            if (*Condition c = remove cancel_cond_map{key}) {
                                c.broadcast();
                            }
                            cinfo.q.push(<SocketPollResultInfo>{
                                "sock": cinfo.sock,
                                "spop": cinfo.spop,
                                "ex": ex,
                                "other": cinfo.other,
                            });
                            remove dmap{key};
                        }
                    }

                    @debug {
                        if (getenv("QORE_ASYNCIO_DEBUG")) {
                            log(LoggerLevel::INFO, "poll cycle ready: cache=%d loop_size=%d",
                                cache.size(), loop.size());
                        }
                    }

                    if (!cache && autostop && !cmdq.size()) {
                        log(LoggerLevel::DEBUG, "All operations have completed; exiting the I/O thread");
                        io_exiting = True;
                        if (io_waiting) {
                            io_cond.broadcast();
                        }
                        break;
                    }
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
                            "EventLoop::poll() pre: items=%d cmdq=%d sem_pending=%y shutting_down=%y",
                            loop.size(), cmdq.size(), sem_pending, shutting_down);
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
                        int non_pipe_ready = 0;
                        if (ready_list) {
                            foreach hash<SocketPollInfo> info in (ready_list) {
                                if (info.socket && info.socket.uniqueHash() != sem_hash) {
                                    ++non_pipe_ready;
                                }
                            }
                        }
                        log(LoggerLevel::INFO, "EventLoop::poll() returned: items=%d elapsed=%y",
                            loop.size(), now_us() - poll_start);
                        if (ready_list && ready_list.size() && !non_pipe_ready) {
                            log(LoggerLevel::INFO,
                                "EventLoop::poll() ready_list only pipe; items=%d cmdq=%d sem_read_open=%y sem_write_open=%y",
                                ready_list.size(), cmdq.size(), sem_read.isOpen(), sem_write.isOpen());
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
        # process any queued commands first without blocking
        try {
            @debug {
                if (getenv("QORE_ASYNCIO_PIPE_DEBUG")) {
                    log(LoggerLevel::INFO, "processCommands: enter cmdq=%d tid=%d",
                        cmdq.size(), gettid());
                }
            }
            @debug(int cmd_count = 0);
            while (True) {
                hash<auto> h;
                {
                    AutoLock al(m);
                    if (!cmdq.size()) {
                        break;
                    }
                    h = shift cmdq;
                }
                @debug(++cmd_count);
                @debug(log(LoggerLevel::INFO, "I/O thread got command: %y", h));
                list<auto> args = h.args ?? ();
                switch (h.cmd) {
                    case IO_QUIT: {
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
                        map $1.broadcast(), cond_list;
                        map doCancelIntern($1), cancel_list;
                        return True;
                    }

                    case IO_ADD:
                        # processed automatically
                        break;

                    case IO_CANCEL: {
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
                        if (c) {
                            c.broadcast();
                        }
                        if (cinfo) {
                            doCancelIntern(cinfo);
                        }
                        break;
                    }

                    case IO_CANCEL_OWNER: {
                        # Cancel all operations belonging to a specific owner
                        string owner = args[0];
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
                        map $1.broadcast(), cond_list;
                        map doCancelIntern($1), cancel_list;
                        @debug(log(LoggerLevel::INFO, "AsyncSocketIoController canceled %d operations for owner %s",
                            cancel_list.size(), owner));
                        break;
                    }

                    case IO_WAKE:
                        # no-op; waking the I/O thread is enough
                        break;
                }
            }
            # then read all data from semaphore file; use short non-blocking reads to drain the pipe
            @debug(int drained_bytes = 0);
            @debug {
                if (getenv("QORE_ASYNCIO_TRACE")) {
                    log(LoggerLevel::INFO, "processCommands: before drain cmdq=%d", cmdq.size());
                }
            }
            while (True) {
                try {
                    binary data = sem_read.readShortBinary(4096, 0);
                    if (!data.size()) {
                        @debug {
                            if (getenv("QORE_ASYNCIO_TRACE")) {
                                log(LoggerLevel::INFO,
                                    "pipe read: zero bytes; cmdq=%d sem_read_open=%y sem_write_open=%y",
                                    cmdq.size(), sem_read.isOpen(), sem_write.isOpen());
                            }
                        }
                        break;
                    }
                    @debug(drained_bytes += data.size());
                    @debug {
                        if (getenv("QORE_ASYNCIO_TRACE")) {
                            log(LoggerLevel::INFO, "pipe read: bytes=%d sem_read_open=%y sem_write_open=%y",
                                data.size(), sem_read.isOpen(), sem_write.isOpen());
                        }
                    }
                } catch (hash<ExceptionInfo> ex) {
                    if (ex.err != "FILE-READ-TIMEOUT") {
                        rethrow;
                    }
                    @debug {
                        if (getenv("QORE_ASYNCIO_TRACE")) {
                            log(LoggerLevel::INFO,
                                "pipe read timeout; cmdq=%d sem_read_open=%y sem_write_open=%y",
                                cmdq.size(), sem_read.isOpen(), sem_write.isOpen());
                        }
                    }
                    break;
                }
            }
            # No need to resignal - with "always signal" on enqueue, the pipe
            # is already written to when commands are added. Resignaling here
            # would cause double-signaling and busy loops.
            @debug {
                if (getenv("QORE_ASYNCIO_PIPE_DEBUG")) {
                    log(LoggerLevel::INFO, "processCommands: done cmd_count=%d drained_bytes=%d cmdq=%d",
                        cmd_count, drained_bytes, cmdq.size());
                }
            }
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
        # push cancelation message on the queue
        cinfo.q.push(<SocketPollResultInfo>{
            "sock": cinfo.sock,
            "spop": cinfo.spop,
            "canceled": True,
            "other": cinfo.other,
        });
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
        # Always signal the I/O thread to avoid race conditions with sem_pending
        # The cost of extra pipe writes is negligible compared to potential lost wakeups
        @debug(log(LoggerLevel::INFO, "enqueueCmdLocked(): enqueued command %y args %y (signal)", cmd, args));
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
                log(LoggerLevel::INFO, "sendCmd: cmd=%y args=%y cmdq=%d tid=%d sem_read_open=%y sem_write_open=%y",
                    cmd, argv, cmdq.size(), gettid(), sem_read.isOpen(), sem_write.isOpen());
            }
        }
        {
            AutoLock al(m);
            signal = sendCmdLocked(cmd, argv);
        }
        @debug(log(LoggerLevel::INFO, "wrote command %y args %y to the command queue", cmd, argv));
        if (signal) {
            sem_write.write(".");
            @debug(log(LoggerLevel::INFO, "notified I/O thread to wake up"));
        }
    }

    #! Updates EventLoop registration for a socket
    /** @param key the operation cache key
        @param socket the socket to register (or NOTHING to unregister)
        @param events the poll events (SOCK_POLLIN and/or SOCK_POLLOUT)

        Handles add, modify, and remove operations based on current state
    */
    private updateEventLoopRegistration(string key, *Socket socket, *int events) {
        *Socket prev_sock = registered_sockets{key};
        string prev_sock_hash = prev_sock ? prev_sock.uniqueHash() : "";
        string new_sock_hash = socket ? socket.uniqueHash() : "";

        if (!socket) {
            # Remove registration for this operation
            if (prev_sock) {
                try {
                    loop.del(prev_sock);
                    @debug {
                        if (getenv("QORE_ASYNCIO_TRACE")) {
                            log(LoggerLevel::DEBUG, "EventLoop remove: key=%s sock=%s",
                                key, prev_sock_hash[0..8]);
                        }
                    }
                } catch (hash<ExceptionInfo> ex) {
                    # Ignore errors when removing - socket may already be closed
                    @debug {
                        if (getenv("QORE_ASYNCIO_TRACE")) {
                            log(LoggerLevel::DEBUG, "EventLoop remove ignored error: %s", ex.err);
                        }
                    }
                }
                remove registered_sockets{key};
                remove registered_events{prev_sock_hash};
            }
            return;
        }

        int prev_events = registered_events{new_sock_hash} ?? 0;

        if (new_sock_hash == prev_sock_hash) {
            # Same socket - check if events changed
            if (events != prev_events) {
                loop.modify(socket, events);
                registered_events{new_sock_hash} = events;
                @debug {
                    if (getenv("QORE_ASYNCIO_TRACE")) {
                        log(LoggerLevel::DEBUG, "EventLoop modify: key=%s sock=%s events=%d->%d",
                            key, new_sock_hash[0..8], prev_events, events);
                    }
                }
            }
        } else {
            # Different socket or new registration
            if (prev_sock) {
                # Remove old socket
                try {
                    loop.del(prev_sock);
                    @debug {
                        if (getenv("QORE_ASYNCIO_TRACE")) {
                            log(LoggerLevel::DEBUG, "EventLoop remove (for replace): key=%s sock=%s",
                                key, prev_sock_hash[0..8]);
                        }
                    }
                } catch (hash<ExceptionInfo> ex) {
                    @debug {
                        if (getenv("QORE_ASYNCIO_TRACE")) {
                            log(LoggerLevel::DEBUG, "EventLoop remove (for replace) ignored error: %s", ex.err);
                        }
                    }
                }
                remove registered_events{prev_sock_hash};
            }
            # Add new socket (only if not already registered by another operation)
            if (!registered_events{new_sock_hash}) {
                loop.add(socket, events);
                @debug {
                    if (getenv("QORE_ASYNCIO_TRACE")) {
                        log(LoggerLevel::DEBUG, "EventLoop add: key=%s sock=%s events=%d",
                            key, new_sock_hash[0..8], events);
                    }
                }
            } else if (events != prev_events) {
                loop.modify(socket, events);
                @debug {
                    if (getenv("QORE_ASYNCIO_TRACE")) {
                        log(LoggerLevel::DEBUG, "EventLoop modify (shared): key=%s sock=%s events=%d->%d",
                            key, new_sock_hash[0..8], prev_events, events);
                    }
                }
            }
            registered_sockets{key} = socket;
            registered_events{new_sock_hash} = events;
        }
    }
}
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

    #! Result Queue
    Queue q();
}
}
