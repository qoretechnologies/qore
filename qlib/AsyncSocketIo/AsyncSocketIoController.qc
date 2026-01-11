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

        #! I/O thread counter
        Counter mcnt();

        #! I/O thread command queue
        Queue cmdq();

        #! Cancel operation waiting Condition; socket unique key -> Condition
        hash<string, Condition> cancel_cond_map;

        #! Condition to signal when I/O thread is ready
        Condition ready_cond();

        #! Flag indicating I/O thread is ready
        bool ready_flag = False;
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
        {
            AutoLock al(m);
            if (tid) {
                shutting_down = True;
                sendCmd(IO_QUIT);
            } else {
                shutting_down = True;
            }
        }
        mcnt.waitForZero();
    }

    #! Stops monitoring and clears all connections
    stopClear() {
        int count;
        {
            AutoLock al(m);
            count = tid ? stopIntern() : 0;
        }
        mcnt.waitForZero();
        log(LoggerLevel::INFO, "Canceled all operations (%d operation%s canceled)", count, count == 1 ? "" : "s");
    }

    #! Waits for the monitoring thread to stop if it's running
    waitStop() {
        mcnt.waitForZero();
    }

    #! Returns @ref True if the I/O thread is running
    bool running() {
        return mcnt.getCount() > 0;
    }

    #! Waits for the I/O thread to be ready
    /** This method blocks until the I/O thread has started and is ready to process operations.
        Use this after submitting operations to ensure the I/O thread is polling before
        attempting to connect to the socket.

        @param timeout maximum time to wait; use @ref NOTHING for no timeout

        @return @ref True if the I/O thread is ready, @ref False if the timeout expired
    */
    bool waitReady(*timeout timeout) {
        AutoLock al(m);
        if (ready_flag) {
            return True;
        }
        if (!tid) {
            return False;
        }
        if (timeout) {
            return ready_cond.wait(m, timeout) == 0 && ready_flag;
        }
        while (!ready_flag && tid) {
            ready_cond.wait(m);
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
        {
            AutoLock al(m);
            if (shutting_down) {
                throw "CONTROLLER-ADD-ERROR", sprintf("Cannot add operation for socket %y; controller is shutting down",
                    uh);
            }
            if (*hash<PollInfo> existing = cache{uh}) {
                if (replace) {
                    cancelIntern(uh);
                } else {
                    # otherwise throw an exception
                    throw "CONTROLLER-ADD-ERROR", sprintf("An operation with socket %y is already in process", uh);
                }
            }
            cinfo = cache{uh} = <PollInfo>{
                "sock": info.sock,
                "spop": info.spop,
                "to": info.to,
                "owner": info.owner,
                "other": info.other,
                "q": info.resultQueue ?? new Queue(),
            };
            @debug(log(LoggerLevel::INFO, "submit: added operation for socket %s goal=%y to=%y cache_size=%d",
                uh, info.other.goal, info.to, cache.size()));
            if (!tid) {
                startIntern();
            }
            sendCmd(IO_ADD);
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

        string uh = sock.uniqueHash();
        {
            m.lock();
            on_exit m.unlock();
            if (cache{uh}) {
                rv = True;
                cancelIntern(uh);
                if (autostop && !cache) {
                    stopIntern();
                    stopped = True;
                }
            } else {
                rv = False;
            }
        }
        if (stopped) {
            mcnt.waitForZero();
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
            sendCmd(IO_CANCEL_OWNER, owner);
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
    }

    #! Cancels an operation in progress and sends an appropriate message on the result queue
    private cancelIntern(string uh) {
        # called locked; caller must hold m to ensure shutting_down/cache state is consistent
        if (shutting_down || !tid || !cache{uh}) {
            if (cancel_cond_map{uh}) {
                cancel_cond_map{uh}.broadcast();
                remove cancel_cond_map{uh};
            }
            return;
        }
        # register the cancel condition before sending the command to avoid races
        cancel_cond_map{uh} = new Condition();
        # cancel the current operation
        sendCmd(IO_CANCEL, uh);

        # wait for the I/O thread to confirm
        while (cancel_cond_map{uh}) {
            cancel_cond_map{uh}.wait(m);
        }
        # in case of a race condition where the operation completed before we could cancel it
        if (cancel_cond_map{uh}) {
            remove cancel_cond_map{uh};
        }
    }

    private startIntern() {
        shutting_down = False;
        mcnt.inc();
        on_error mcnt.dec();
        tid = background ioThread();
    }

    private int stopIntern() {
        int rv = cache.size();
        shutting_down = True;
        sendCmd(IO_QUIT);
        return rv;
    }

    private ioThread() {
        log(LoggerLevel::INFO, "Started Socket polling operation controller I/O thread");
        on_exit {
            {
                AutoLock al(m);
                ready_flag = False;
                if (tid) {
                    remove tid;
                }
                ready_cond.broadcast();
            }
            log(LoggerLevel::INFO, "Exiting Socket polling operation controller I/O thread");
            mcnt.dec();
        }

        # Signal that I/O thread is ready
        {
            AutoLock al(m);
            ready_flag = True;
            ready_cond.broadcast();
        }

        while (True) {
            try {
                date poll_timeout;
                softlist<hash<SocketPollInfo>> poll_list = sem_info;
                {
                    AutoLock al(m);
                    # scan for timed out operations
                    date now = now_us();
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
                        try {
                            # Only check for timeout if timeout_date is set (not set when to < 0)
                            if (cinfo.timeout_date && cinfo.timeout_date <= now) {
                                throw "SOCKET-TIMEOUT", sprintf("The I/O operation did not complete in the timeout "
                                    "period (%y)", cinfo.to);
                            }
                            *hash<SocketPollInfo> info = cinfo.spop.continuePoll();
                            log(LoggerLevel::DEBUG, "%y: state: %y: got poll info: %y", key,
                                cinfo.spop.getState(), info ? {
                                    "events": PollEventMap{info.events} ?? "unknown",
                                    "socket": sprintf("%s %s", info.socket.className(),
                                        info.socket.uniqueHash()[0..8]),
                                } : NOTHING);
                            if (!info) {
                                # operation successful
                                remove cache{key};
                                if (*Condition c = remove cancel_cond_map{key}) {
                                    c.signal();
                                }
                                cinfo.q.push(<SocketPollResultInfo>{
                                    "sock": cinfo.sock,
                                    "spop": cinfo.spop,
                                    "other": cinfo.other,
                                });
                                remove dmap{key};
                            } else {
                                poll_list += info;
                                # Find the MINIMUM (earliest) timeout so poll returns in time to handle it
                                # Only consider operations with timeouts
                                if (cinfo.timeout_date && (!poll_timeout || (poll_timeout > cinfo.timeout_date))) {
                                    poll_timeout = cinfo.timeout_date;
                                }
                            }
                        } catch (hash<ExceptionInfo> ex) {
                            log(LoggerLevel::ERROR, "%s", get_exception_string(ex));
                            # poll operation failed with an exception
                            remove cache{key};
                            if (*Condition c = remove cancel_cond_map{key}) {
                                c.signal();
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

                    @debug(log(LoggerLevel::INFO, "poll_list: cache=%d items=%d poll_list: %y", cache.size(), poll_list.size(), (map $1 + {
                        "events": PollEventMap{$1.events} ?? "unknown",
                        "socket": sprintf("%s %s", $1.socket.className(), $1.socket.uniqueHash()[0..8]),
                    }, poll_list)));

                    if (!cache && autostop) {
                        log(LoggerLevel::DEBUG, "All operations have completed; exiting the I/O thread");
                        remove tid;
                        break;
                    }
                }
                # ignore return value, all active ping operations are sent in each poll call
                # Use -1 (infinite wait) when no timeout is set - the semaphore pipe in poll_list
                # ensures we wake up immediately when commands are sent via sendCmd()
                log(LoggerLevel::DEBUG, "calling poll() with %d items, timeout=%y", poll_list.size(),
                    poll_timeout ?? "infinite");
                Socket::poll(poll_list, poll_timeout ?? -1);
                log(LoggerLevel::DEBUG, "poll() returned");

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
        # first read all data from semaphore file
        try {
            sem_read.read(-1, 0);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err != "FILE-READ-TIMEOUT") {
                rethrow;
            }
        }
        try {
            while (hash<auto> h = cmdq.get(-1)) {
                #log(LoggerLevel::DEBUG, "I/O thread got command: %y", h);
                switch (h.cmd) {
                    case IO_QUIT: {
                        # cancel all operations in progress without holding the lock while aborting
                        list<hash<PollInfo>> cancel_list = ();
                        list<Condition> cond_list = ();
                        {
                            AutoLock al(m);
                            log(LoggerLevel::DEBUG, "AsyncSocketIoController canceling %d operations for shutdown",
                                cache.size());
                            foreach string key in (keys cache) {
                                cancel_list += cache{key};
                                if (*Condition c = remove cancel_cond_map{key}) {
                                    cond_list += c;
                                }
                                remove cache{key};
                            }
                        }
                        foreach Condition c in (cond_list) {
                            c.signal();
                        }
                        foreach hash<PollInfo> cinfo in (cancel_list) {
                            doCancelIntern(cinfo);
                        }
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
                            if (cache{h.args[0]}) {
                                cinfo = cache{h.args[0]};
                                remove cache{h.args[0]};
                                c = remove cancel_cond_map{h.args[0]};
                            }
                        }
                        if (!cinfo) {
                            log(LoggerLevel::DEBUG, "AsyncSocketIoController cancel request for unknown key: %y",
                                h.args[0]);
                        }
                        if (c) {
                            c.signal();
                        }
                        if (cinfo) {
                            doCancelIntern(cinfo);
                        }
                        break;
                    }

                    case IO_CANCEL_OWNER: {
                        # Cancel all operations belonging to a specific owner
                        string owner = h.args[0];
                        list<hash<PollInfo>> cancel_list = ();
                        list<Condition> cond_list = ();
                        {
                            AutoLock al(m);
                            log(LoggerLevel::DEBUG, "AsyncSocketIoController canceling operations for owner %s",
                                owner);
                            foreach string key in (keys cache) {
                                if (cache{key}.owner == owner) {
                                    cancel_list += cache{key};
                                    if (*Condition c = remove cancel_cond_map{key}) {
                                        cond_list += c;
                                    }
                                    remove cache{key};
                                }
                            }
                        }
                        foreach Condition c in (cond_list) {
                            c.signal();
                        }
                        foreach hash<PollInfo> cinfo in (cancel_list) {
                            doCancelIntern(cinfo);
                        }
                        log(LoggerLevel::DEBUG, "AsyncSocketIoController canceled %d operations for owner %s",
                            cancel_list.size(), owner);
                        break;
                    }

                    case IO_WAKE:
                        # no-op; waking the I/O thread is enough
                        break;
                }
            }
        } catch(hash<ExceptionInfo> ex) {
            if (ex.err != "QUEUE-TIMEOUT") {
                rethrow;
            }
        }
    }

    private doCancelIntern(hash<PollInfo> cinfo) {
        try {
            log(LoggerLevel::DEBUG, "canceling async I/O op=%s sock=%s",
                cinfo.spop.className(),
                sprintf("%s %s", cinfo.sock.className(), cinfo.sock.uniqueHash()[0..8]));
            cinfo.spop.abort();
            log(LoggerLevel::DEBUG, "canceled async I/O op=%s sock=%s",
                cinfo.spop.className(),
                sprintf("%s %s", cinfo.sock.className(), cinfo.sock.uniqueHash()[0..8]));
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

    private sendCmd(string cmd) {
        # only send the command if the I/O thread is running
        if (!tid) {
            return;
        }
        cmdq.push({
            "cmd": cmd,
            "args": argv,
        });
        #log(LoggerLevel::DEBUG, "wrote command %y args %y to the command queue", cmd, argv);
        sem_write.write(".");
        #log(LoggerLevel::DEBUG, "notified I/O thread to wake up");
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
