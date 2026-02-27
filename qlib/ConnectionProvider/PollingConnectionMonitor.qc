# -*- mode: qore; indent-tabs-mode: nil -*-
# Qore PollingConnectionMonitor class definition

/*  PollingConnectionMonitor.qc Copyright 2016 - 2026 Qore Technologies, s.r.o.

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

#! The ConnectionProvider namespace
/** All public declarations in the %ConnectionProvider module are defined in this namespace
*/
public namespace ConnectionProvider {
#! Class for monitoring connection supporting the polling API
/** @since ConnectionProvider 1.7
*/
public class PollingConnectionMonitor inherits LoggerWrapper {
    public {
        #! Polling event map
        const PollEventMap = {
            SOCK_POLLIN: "read",
            SOCK_POLLOUT: "write",
            SOCK_POLLIN | SOCK_POLLOUT: "read and write",
        };
    }

    private {
        #! Lock for atomic actions
        Mutex m();

        #! Connection cache; unique name -> connection object wrapper
        hash<string, hash<PollInfo>> cache;

        #! Timer thread TID
        int tid;

        #! Autostart monitoring thread with first connection
        bool autostart = True;

        #! Quit flag for the timer thread
        bool quit = False;

        #! Flag indicating an explicit stop is in progress; prevents add() from
        #! rescinding quit and restarting the timer thread
        bool stopping = False;

        #! ping timeout duration
        date ping_timeout = DefaultPingTimeout;

        #! ping repeat duration
        date ping_repeat = DefaultPingRepeat;

        #! Default ping timeout duration
        const DefaultPingTimeout = 30s;

        #! Default ping repeat duration
        const DefaultPingRepeat = 1m;
    }

    private:internal {
        #! Async I/O controller for delegating socket operations
        AsyncSocketIo::AsyncSocketIoController controller;

        #! Condition variable for timer thread signaling
        Condition timer_cond();

        #! Timer thread WaitGroup
        WaitGroup mcnt();
    }

    #! Creates the object with an explicit async I/O controller
    constructor(AsyncSocketIo::AsyncSocketIoController controller, *LoggerInterface logger)
            : LoggerWrapper(logger) {
        self.controller = controller;
    }

    #! Creates the object using the global async I/O controller
    constructor(*LoggerInterface logger) : LoggerWrapper(logger) {
        self.controller = AsyncSocketIo::getGlobalAsyncIoController();
    }

    #! Stops the monitoring thread and destroys the object
    destructor() {
        stop();
    }

    #! Returns information about the object
    hash<auto> getInfo() {
        return {
            "autostart": autostart,
            "connections": (map {
                $1.key: ($1.value.ping_active ? "I/O" : $1.value.start),
            }, cache.pairIterator()),
            "io_tid": tid,
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
    /** @throw MONITOR-START-ERROR if the timer thread is already running or the monitor is being stopped
    */
    start() {
        AutoLock al(m);

        if (stopping) {
            throw "MONITOR-START-ERROR", "cannot start while the monitor is being stopped";
        }
        if (tid) {
            throw "MONITOR-START-ERROR", sprintf("timer thread is already running in TID %d", tid);
        }

        startIntern();
    }

    #! Stops monitoring
    stop() {
        list<string> cancel_names;
        {
            AutoLock al(m);
            stopping = True;
            if (tid) {
                quit = True;
                timer_cond.signal();
            }
            foreach string name in (keys cache) {
                if (cache{name}.ping_active) {
                    cache{name}.ping_active = False;
                    cancel_names += name;
                }
            }
        }
        # Cancel outside the lock to avoid deadlock with callbacks
        foreach string name in (cancel_names) {
            controller.cancelByOwner("pcm:" + name);
        }
        mcnt.wait();
        {
            AutoLock al(m);
            stopping = False;
        }
    }

    #! Stops monitoring and clears all connections
    stopClear() {
        int count;
        list<string> cancel_names;
        {
            AutoLock al(m);
            stopping = True;
            count = cache.size();
            if (tid) {
                quit = True;
                timer_cond.signal();
            }
            foreach string name in (keys cache) {
                if (cache{name}.ping_active) {
                    cancel_names += name;
                }
            }
            remove cache;
        }
        # Cancel outside the lock to avoid deadlock with callbacks
        foreach string name in (cancel_names) {
            controller.cancelByOwner("pcm:" + name);
        }
        mcnt.wait();
        {
            AutoLock al(m);
            stopping = False;
        }
        log(LoggerLevel::INFO, "removed all connections (%d connection%s removed)", count,
            count == 1 ? "" : "s");
    }

    #! Waits for the monitoring thread to stop if it's running
    waitStop() {
        mcnt.wait();
    }

    #! Returns @ref True if the timer thread is running
    bool running() {
        return mcnt.getCount() > 0;
    }

    #! Adds a connection to be monitored
    /** @param conn the connection to be monitored; must support the connection polling API
        @param other a free-form hash to be stored alongside the connection
        @param start_pending wait until the next cycle to poll

        @throw MONITOR-CONNECTION-ERROR the connection does not support the connection polling API
        @throw MONITOR-ADD-ERROR a different connection with this name is already being monitored
    */
    add(AbstractConnection conn, *hash<auto> other, *bool start_pending) {
        string name = conn.getName();
        if (!conn.supportsPollingApi()) {
            throw "MONITOR-CONNECTION-ERROR", sprintf("Connection %y does not support the polling API and cannot be "
                "added to the polling connection monitor", name);
        }
        AutoLock al(m);
        if (*hash<PollInfo> info = cache{name}) {
            # if the connection has already been added, then ignore
            if (info.conn == conn) {
                return;
            }
            # otherwise throw an exception
            throw "MONITOR-ADD-ERROR", sprintf("Connection %y with URL %y cannot be added, as connection %y with "
                "URL %y is already being monitored; update the connection if this definition should replace the "
                "existing one", name, conn.getSafeUrl(), name, info.conn.getSafeUrl());
        }
        addOrUpdateIntern(conn, other, start_pending);
    }

    #! Adds or updates an existing connection that is already being monitored
    /** @param conn the connection to be monitored; must support the connection polling API
        @param other a free-form hash to be stored alongside the connection
        @param start_pending wait until the next cycle to poll

        @throw MONITOR-CONNECTION-ERROR the connection does not support the connection polling API
    */
    addOrUpdate(AbstractConnection conn, *hash<auto> other, *bool start_pending) {
        string name = conn.getName();
        if (!conn.supportsPollingApi()) {
            throw "MONITOR-CONNECTION-ERROR", sprintf("Connection %y does not support the polling API and cannot be "
                "added to the polling connection monitor", name);
        }
        bool need_cancel;
        {
            AutoLock al(m);
            if (*hash<PollInfo> info = cache{name}) {
                # if the connection has already been added, then ignore
                if (info.conn == conn) {
                    return;
                }
                # mark for cancellation outside the lock
                if (info.ping_active) {
                    cache{name}.ping_active = False;
                    need_cancel = True;
                }
            }
        }
        # Cancel outside the lock to avoid deadlock with callbacks
        if (need_cancel) {
            controller.cancelByOwner("pcm:" + name);
        }
        {
            AutoLock al(m);
            addOrUpdateIntern(conn, other, start_pending);
        }
    }

    #! Removes the given connection
    /** @param name the connection to be removed

        @return @ref True if the connection was removed, @ref False if no such connection is being monitored

        @note Stops the timer thread if the last connection is removed and the \a autostart flag is enabled

        @see removeConnectionEx()
    */
    bool removeConnection(string name) {
        bool rv;
        bool need_cancel;

        {
            m.lock();
            on_exit m.unlock();
            if (cache{name}) {
                need_cancel = cache{name}.ping_active;
                remove cache{name};
                rv = True;
                if (autostart && !cache) {
                    quit = True;
                    timer_cond.signal();
                } else {
                    timer_cond.signal();
                }
            } else {
                rv = False;
            }
        }
        # Cancel outside the lock to avoid deadlock with callbacks
        if (need_cancel) {
            controller.cancelByOwner("pcm:" + name);
        }
        return rv;
    }

    #! Removes the given connection
    /** @param name the connection to be removed

        @throw MONITOR-REMOVE-ERROR thrown if the named connection is not being monitored

        @see removeConnection()
    */
    removeConnectionEx(string name) {
        if (!removeConnection(name)) {
            throw "MONITOR-REMOVE-ERROR", sprintf("no connection %y is being monitored", name);
        }
    }

    #! Removes the given connection without canceling in-flight pings
    /** @param name the connection to be removed

        @return @ref True if the timer thread was signaled to stop

        Unlike removeConnection(), this method does not cancel in-flight ping operations via the
        controller.  This is useful during shutdown sequences where the caller holds locks and cannot
        afford to wait for cancellation.

        @see removeConnection()
    */
    *bool removeConnectionNoWait(string name) {
        m.lock();
        on_exit m.unlock();
        if (remove cache{name}) {
            if (autostart && !cache) {
                quit = True;
                timer_cond.signal();
                return True;
            }
            timer_cond.signal();
        }
    }

    #! Adds or updates an existing connection that is already being monitored
    /** @param conn the connection to be monitored; must support the connection polling API
        @param other a free-form hash to be stored alongside the connection
        @param start_pending wait until the next cycle to poll
    */
    private addOrUpdateIntern(AbstractConnection conn, *hash<auto> other, *bool start_pending) {
        string name = conn.getName();
        cache{name} = <PollInfo>{
            "conn": conn,
            "other": other,
        };
        # Clear quit so the timer thread continues if it hasn't exited yet, but
        # not during an explicit stop() — stopping prevents quit from being rescinded
        if (!stopping) {
            quit = False;
            if (autostart && !tid) {
                startIntern();
            }
        }
        if (start_pending) {
            cache{name}.start = now_us();
        }
        timer_cond.signal();
    }

    private startIntern() {
        quit = False;
        mcnt.add();
        on_error mcnt.done();
        tid = background timerThread();
    }

    #! Timer thread: manages scheduling of ping operations
    private timerThread() {
        log(LoggerLevel::INFO, "started polling connection monitor timer thread");
        on_exit {
            {
                AutoLock al(m);
                remove tid;
                # If quit was rescinded (add() set quit=False) and there are
                # connections to monitor, restart the timer thread
                if (!quit && autostart && cache) {
                    startIntern();
                }
            }
            log(LoggerLevel::INFO, "exiting polling connection monitor timer thread");
            mcnt.done();
        }

        while (True) {
            list<string> due_names = ();
            {
                AutoLock al(m);
                if (quit) {
                    break;
                }
                date now = now_us();
                *date next_deadline;

                foreach string name in (keys cache) {
                    hash<PollInfo> info = cache{name};
                    if (info.ping_active) {
                        continue;  # in-flight with controller
                    }
                    date delta = info.spop ? ping_timeout : ping_repeat;
                    date trigger = info.start + delta;
                    if (trigger <= now) {
                        due_names += name;
                    } else if (!next_deadline || trigger < next_deadline) {
                        next_deadline = trigger;
                    }
                }

                if (!due_names && !quit) {
                    int wait_ms = next_deadline
                        ? max(0, (next_deadline - now_us()).durationMilliseconds())
                        : -1;
                    timer_cond.wait(m, wait_ms);
                    continue;
                }
                if (quit) {
                    break;
                }
            }

            # Submit pings outside the lock
            for (int i = 0; i < due_names.size(); ++i) {
                submitPing(due_names[i]);
            }
        }
    }

    #! Starts and submits a ping operation to the async I/O controller
    private submitPing(string name) {
        AutoLock al(m);
        if (!cache{name} || cache{name}.ping_active) {
            return;
        }

        AbstractPollOperation spop;
        try {
            spop = cache{name}.conn.startPollConnect(getLogger());
            log(LoggerLevel::DEBUG, "%y: starting connect ping to %y; poll state: %y", name,
                cache{name}.conn.getUrl(), spop.getState());
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "%y: error handling ping start: %s", name, get_exception_string(ex));
            failedToStartPing(name, ex);
            cache{name}.start = now_us();
            return;
        }

        *hash<SocketPollInfo> poll_info;
        try {
            poll_info = spop.continuePoll();
        } catch (hash<ExceptionInfo> ex) {
            cache{name}.conn.last_check = now_us();
            cache{name}.spop = spop;
            handlePingFailed(name, now_us() - cache{name}.start, ex);
            cache{name}.start = now_us();
            return;
        }

        cache{name}.spop = spop;
        cache{name}.start = now_us();

        if (!poll_info) {
            # Completed immediately
            cache{name}.conn.last_check = cache{name}.start;
            if (spop.goalReached()) {
                handlePingSuccess(name, 0us, cache{name}.conn.up);
            } else {
                hash<ExceptionInfo> ex = <ExceptionInfo>{
                    "err": "PING-FAILED",
                    "desc": sprintf("ping operation ended in state %y without reaching goal",
                        spop.getState()),
                };
                handlePingFailed(name, 0us, ex);
            }
            return;
        }

        # Submit to controller — do NOT pass poll_info; the controller must call
        # continuePoll() itself on the first cycle to register the socket in EventLoop.
        # Passing poll_info would cause the controller to skip the first continuePoll
        # (it assumes the socket is already registered when poll_info is set).
        #
        # Design note: submit() is safe to call while holding `m` — it never invokes
        # callbacks inline (stores in cache, notifies I/O thread). Lock ordering is
        # PCM.m -> controller.m (never reverse), so no deadlock is possible.
        cache{name}.ping_active = True;
        int gen = ++cache{name}.ping_gen;
        string conn_name = name;
        controller.submit(<SocketPollOperationInfo>{
            "sock": poll_info.socket,
            "spop": spop,
            "to": ping_timeout,
            "owner": "pcm:" + name,
            "callback": sub (hash<SocketPollResultInfo> result) {
                handlePingResult(conn_name, gen, result);
            },
        });
    }

    #! Handles the result of a ping operation from the async I/O controller
    /** Runs in the controller's I/O thread.

        Design note: this callback acquires `m` while running on the I/O thread.
        Contention is bounded by ping_repeat intervals and the critical section is
        short. Lock ordering is PCM.m -> controller.m (controller callbacks are
        delivered without holding the controller lock), so no deadlock is possible.
    */
    private handlePingResult(string name, int gen, hash<SocketPollResultInfo> result) {
        AutoLock al(m);
        if (!cache{name} || cache{name}.ping_gen != gen) {
            return;  # connection removed or replaced; stale result
        }

        date delta = now_us() - cache{name}.start;
        cache{name}.ping_active = False;
        cache{name}.conn.last_check = cache{name}.start;

        if (result.canceled) {
            delete cache{name}.spop;
            cache{name}.start = now_us();
            timer_cond.signal();
            return;
        }

        # Ensure spop is set for handler methods that access cache{name}.spop.getState()
        if (!cache{name}.spop) {
            cache{name}.spop = result.spop;
        }

        if (result.ex) {
            if (result.ex.err == "SOCKET-TIMEOUT") {
                handlePingTimeoutIntern(name, delta);
                # handlePingTimeoutIntern does not clean up spop/start
                delete cache{name}.spop;
                cache{name}.start = now_us();
            } else {
                # handlePingFailed cleans up spop and sets start
                handlePingFailed(name, delta, result.ex);
            }
        } else if (result.spop.goalReached()) {
            # handlePingSuccess cleans up spop and sets start
            handlePingSuccess(name, delta, cache{name}.conn.up);
        } else {
            # poll operation ended without reaching goal and without exception
            hash<ExceptionInfo> ex = <ExceptionInfo>{
                "err": "PING-FAILED",
                "desc": sprintf("ping operation ended in state %y without reaching goal",
                    result.spop.getState()),
            };
            # handlePingFailed cleans up spop and sets start
            handlePingFailed(name, delta, ex);
        }

        timer_cond.signal();
    }

    #! Could not start ping
    private failedToStartPing(string name, hash<ExceptionInfo> ex) {
        # error already logged
        cache{name}.conn.handlePingFailed(0s, ex);
    }

    #! Handles a successful ping
    private handlePingSuccess(string name, date delta, *bool oldok) {
        log(LoggerLevel::INFO, "%y (poll state %y was up: %y): successful ping (duration: %y)", name,
            cache{name}.spop.getState(), oldok ?? False, delta);
        cache{name}.conn.handlePingSuccess(delta);
        delete cache{name}.spop;
        cache{name}.start = now_us();
    }

    #! Handles a failed ping
    private handlePingFailed(string name, date delta, hash<ExceptionInfo> ex) {
        AbstractConnection c = cache{name}.conn;
        log(LoggerLevel::ERROR, "%y (poll state %y was up: %y): error in ping (duration: %y): %s", name,
            cache{name}.spop.getState(), c.up, delta, get_exception_string(ex));
        c.handlePingFailed(delta, ex);
        delete cache{name}.spop;
        cache{name}.start = now_us();
    }

    #! Handles a poll timeout
    private handlePingTimeout(string name, date delta) {
        if (cache{name}.spop) {
            handlePingTimeoutIntern(name, delta);
        }
        delete cache{name}.spop;
        cache{name}.start = now_us();
    }

    #! Handles a ping timeout
    private handlePingTimeoutIntern(string name, date delta) {
        AbstractConnection c = cache{name}.conn;
        string err = sprintf("(poll state %s): %s connection ping timed out after: %y",
            cache{name}.spop.getState(), c.getType(), delta);
        c.handlePingFailed(delta, err);
        log(LoggerLevel::ERROR, "%y %s; restarting poll", name, err);
    }
}
}

# private symbols
namespace Priv {
hashdecl PollInfo {
    # poll start timestamp
    date start;

    # connection
    AbstractConnection conn;

    # poll operation (set while in-flight)
    *AbstractPollOperation spop;

    # whether a ping is currently submitted to the controller
    bool ping_active = False;

    # generation counter for invalidating stale async results
    int ping_gen = 0;

    # other data stored alongside the connection
    *hash<auto> other;
}
}
