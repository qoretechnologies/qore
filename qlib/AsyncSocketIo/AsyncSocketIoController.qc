# -*- mode: qore; indent-tabs-mode: nil -*-
# Qore AsyncSocketIoController class definition

/*  AsyncSocketIoController.qc Copyright 2016 - 2024 Qore Technologies, s.r.o.

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

    #! I/O timeout
    date to = AsyncSocketIoController::DefaultIoOperationTimeout;

    #! Free-form data to be returned when the operation completes
    *hash<auto> other;
}

#! Socket poll result
public hashdecl SocketPollResultInfo {
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

        #! Autostart monitoring thread with first connection
        bool autostart = True;

        #! I/O thread command: add
        const IO_ADD = "add";

        #! I/O thread command: cancel
        const IO_CANCEL = "cancel";

        #! I/O thread command: quit
        const IO_QUIT = "quit";
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

        #! For listener sockets

    }

    #! Creates the object
    constructor(bool autostart, *LoggerInterface logger) : LoggerWrapper(logger) {
        self.autostart = autostart;
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
            "autostart": autostart,
            "sockets": (map {
                $1.key: ($1.value.spop ? "I/O" : $1.value.timeout_date),
            }, cache.pairIterator()),
            "io_tid": tid,
            "cmdq": cmdq.size(),
        };
    }

    #! Set autostart flag
    setAutostart(bool autostart = True) {
        self.autostart = autostart;
    }

    #! Get autostart flag
    bool getAutostart() {
        return autostart;
    }

    #! Starts monitoring
    /** @throw CONTROLLER-START-ERROR if the I/O thread is already running
    */
    start() {
        AutoLock al(m);

        if (tid) {
            throw "CONTROLLER-START-ERROR", sprintf("I/O thread is already running in TID %d", tid);
        }

        startIntern();
    }

    #! Stops monitoring
    stop() {
        {
            AutoLock al(m);
            if (tid) {
                sendCmd(IO_QUIT);
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

    #! Submits a socket operation to be performed and returns a Queue where the results will be returned
    /** @param info a hash with the socket operation and supporting info
        @param replace if True, then any operation currently in place for the given socket will be replaced

        @return Queue a Queue that will have the result of the operation pushed to it

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
        # get unique key for socket object
        string uh = info.sock.uniqueHash();
        hash<PollInfo> cinfo;
        {
            AutoLock al(m);
            if (*hash<PollInfo> info = cache{uh}) {
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
                "other": info.other,
            };
            if (autostart && !tid) {
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

        @note Stops the I/O thread if the last operation is canceled and the \a autostart flag is enabled

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
                if (autostart && !cache) {
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

    #! Cancels any operation in progress for the given Socket
    /** @param sock the socket to cancel any operation in progress for

        @return @ref True if an operation for the socket was canceled, @ref False if not

        @note Stops the I/O thread if the last operation is canceled and the \a autostart flag is enabled

        @throw CONTROLLER-CANCEL-ERROR thrown if the Socket has no operation in progress

        @see cancel()
    */
    cancelEx(Socket sock) {
        if (!cancel(sock)) {
            throw "CONTROLLER-CANCEL-ERROR", sprintf("Socket %y has no operation in progress", sock.uniqueHash());
        }
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
        # called locked
        # cancel the current operation
        hash<PollInfo> cinfo = cache{uh};
        sendCmd(IO_CANCEL, uh);

        # wait for the I/O thread to confirm
        cancel_cond_map{uh} = new Condition();
        while (cancel_cond_map{uh}) {
            cancel_cond_map{uh}.wait(m);
        }
        # in case of a race condition where the operation completed before we could cancel it
        if (cancel_cond_map{uh}) {
            remove cancel_cond_map{uh};
        }
    }

    private startIntern() {
        mcnt.inc();
        on_error mcnt.dec();
        tid = background ioThread();
    }

    private int stopIntern() {
        int rv = cache.size();
        sendCmd(IO_QUIT);
        return rv;
    }

    private ioThread() {
        log(LoggerLevel::INFO, "Started Socket polling operation controller I/O thread");
        on_exit {
            if (tid) {
                AutoLock al(m);
                remove tid;
            }
            log(LoggerLevel::INFO, "Exiting Socket polling operation controller I/O thread");
            mcnt.dec();
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
                        if (!cinfo.timeout_date) {
                            cinfo.timeout_date = cache{key}.timeout_date = now_us() + cinfo.to;
                        }
                        dmap{key} = True;
                        Socket sock = cinfo.sock;
                        try {
                            if (cinfo.timeout_date < now) {
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
                                cinfo.q.push(<SocketPollResultInfo>{
                                    "spop": cinfo.spop,
                                    "other": cinfo.other,
                                });
                                remove dmap{key};
                            } else {
                                poll_list += info;
                                if (!poll_timeout || (poll_timeout < cinfo.timeout_date)) {
                                    poll_timeout = cinfo.timeout_date;
                                }
                            }
                        } catch (hash<ExceptionInfo> ex) {
                            log(LoggerLevel::ERROR, "%s", get_exception_string(ex));
                            # poll operation failed with an exception
                            remove cache{key};
                            cinfo.q.push(<SocketPollResultInfo>{
                                "spop": cinfo.spop,
                                "ex": ex,
                                "other": cinfo.other,
                            });
                            remove dmap{key};
                        }
                    }

                    log(LoggerLevel::DEBUG, "poll_list: %y", poll_timeout, (map $1 + {
                        "events": PollEventMap{$1.events} ?? "unknown",
                        "socket": sprintf("%s %s", $1.socket.className(), $1.socket.uniqueHash()[0..8]),
                    }, poll_list));

                    if (!cache && autostart) {
                        log(LoggerLevel::DEBUG, "All operations have completed; exiting the I/O thread");
                        remove tid;
                        break;
                    }
                }
                # ignore return value, all active ping operations are sent in each poll call
                Socket::poll(poll_list, poll_timeout);

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
                AutoLock al(m);
                switch (h.cmd) {
                    case IO_QUIT:
                        # cancel all operations in progress
                        map doCancelIntern($1), cache.iterator();
                        return True;

                    case IO_ADD:
                        # processed automatically
                        break;

                    case IO_CANCEL: {
                        hash<PollInfo> cinfo = cache{h.args[0]};
                        doCancelIntern(cinfo);
                        break;
                    }
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
            cinfo.spop.abort();
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Ignoring error aborting canceled async I/O: %s", get_exception_string(ex));
        }
        if (*Condition c = remove cancel_cond_map{cinfo.sock.uniqueHash()}) {
            c.signal();
        }
        # push cancelation message on the queue
        cinfo.q.push(<SocketPollResultInfo>{
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
    #! Poll start timestamp
    date timeout_date;

    #! Socket connection
    Socket sock;

    #! Poll operation
    AbstractPollOperation spop;

    #! I/O timeout
    date to = AsyncSocketIoController::DefaultIoOperationTimeout;

    #! Other data stored alongside the socket
    *hash<auto> other;

    #! Result Queue
    Queue q();
}
}