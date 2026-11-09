# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore REPL session events: background thread lifecycle, Program activity and evaluation

/** QoreReplEvents.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace QoreRepl {
#! the levels of thread notices, from the most to the least verbose; see @ref qorerepl_thread_events
public const ThreadNoticeLevels = ("all", "summary", "failures", "off");

#! the default level of thread notices
public const DefaultThreadNoticeLevel = "summary";

#! an event of a REPL session, delivered to the listeners added with @ref QoreRepl::QoreRepl::addEventListener() "QoreRepl::addEventListener()"
/** Events:
    - \c "thread-started": a background thread started in the session's Program; \c thread has the Program's
      \c STARTED event
    - \c "thread-cancel-requested": cancellation was requested for a background thread; \c thread has the Program's
      \c CANCEL_REQUESTED event
    - \c "thread-finished": a background thread ended; \c thread has the Program's \c FINISHED event, with its
      outcome and error
    - \c "program-busy", \c "program-idle": the Program became active or idle: code runs in it (an evaluation, a
      background thread, a callback), or none does; parsing, @c /load, @c /type and @c /reset need an idle Program
    - \c "eval-started", \c "eval-finished": the session started and finished a request that runs code in the
      Program (foreground evaluation); \c input has the request
    - \c "program-replaced": @c /reset replaced the Program; \c threads has the background threads of the new
      Program, and later events are about it

    @since QoreRepl 1.0
*/
public hashdecl ReplEvent {
    #! the event
    string event;

    #! the ID of the session's Program that the event is about
    int program_id;

    #! the number of background threads of the Program running after the event
    int running;

    #! @ref True if the Program is idle after the event
    bool idle;

    #! the Program's thread event (thread events)
    *hash<ProgramThreadEvent> thread;

    #! the background threads of the new Program (\c "program-replaced")
    *list<hash<ProgramThreadInfo>> threads;

    #! the request evaluated (\c "eval-started", \c "eval-finished")
    *string input;
}

#! A message of a session to its delivering thread, in the queue of the Program's thread events
hashdecl ReplDeliveryMessage {
    #! True to end the delivering thread
    bool stop = False;

    #! the \c "program-replaced" event to deliver
    *hash<ReplEvent> replaced;

    #! an event of the session itself: \c "eval-started" or \c "eval-finished"
    *string session;

    #! the request of an event of the session
    *string input;
}

#! Delivers the session events of a REPL to its listeners and keeps the state of the Program's background threads
/** While the session has listeners, the dispatcher subscribes to the thread events of the session's Program (see
    @ref Qore::Program::subscribeThreadEvents() "Program::subscribeThreadEvents()") and runs one thread that reads the
    subscription's queue, keeps the state of the Program's background threads and calls the listeners.  When the
    last listener is removed, the subscription is removed and the thread ends, so a session without listeners runs
    no thread (a script that ends waits for its background threads); its state is then taken from a snapshot.

    The thread holds the dispatcher, not the REPL, so the REPL can be destroyed while it runs: its destructor stops
    the dispatcher.
*/
class ReplEventDispatcher {
    private {
        #! guards the members below
        Mutex m();

        #! the session's Program
        Program pgm;

        #! the delivery, while the session has listeners
        *ReplEventDelivery delivery;

        #! the listeners, by ID
        hash<string, code<nothing(hash<ReplEvent>)>> listeners;

        #! the last listener ID handed out
        int listener_id = 0;

        #! True once the dispatcher has been stopped
        bool stopped = False;
    }

    #! creates the dispatcher for the given Program; nothing is subscribed until a listener is added
    constructor(Program pgm) {
        self.pgm = pgm;
    }

    #! adds a listener and starts the delivery if it has not started; returns the listener's ID
    int addListener(code<nothing(hash<ReplEvent>)> listener) {
        m.lock();
        on_exit m.unlock();
        checkStoppedIntern();
        if (!delivery) {
            delivery = new ReplEventDelivery(self, pgm);
        }
        listeners{++listener_id} = listener;
        return listener_id;
    }

    #! removes a listener; the delivery ends with the last one; returns @ref False if there is no such listener
    bool removeListener(int id) {
        *ReplEventDelivery ended;
        {
            m.lock();
            on_exit m.unlock();
            if (!exists listeners{id}) {
                return False;
            }
            remove listeners{id};
            if (!listeners && delivery) {
                ended = remove delivery;
            }
        }
        if (ended) {
            ended.stop();
        }
        return True;
    }

    #! returns the background threads of the Program, in the order they started
    /** While events are delivered, the state is that of the last event processed; otherwise it is a snapshot
    */
    list<hash<ProgramThreadInfo>> getThreads() {
        m.lock();
        on_exit m.unlock();
        checkStoppedIntern();
        if (delivery) {
            return delivery.getThreads();
        }
        return snapshotIntern().threads;
    }

    #! returns @ref True if the Program is idle; see getThreads() for the state reported
    bool isIdle() {
        m.lock();
        on_exit m.unlock();
        checkStoppedIntern();
        if (delivery) {
            return delivery.isIdle();
        }
        return snapshotIntern().idle;
    }

    #! returns the listeners to call, in the order they were added
    list<code<nothing(hash<ReplEvent>)>> getListeners() {
        m.lock();
        on_exit m.unlock();
        return (map listeners{$1}, sort(map $1.toInt(), keys listeners)) ?? ();
    }

    #! replaces the Program of the session; delivers \c "program-replaced" while events are delivered
    setProgram(Program new_pgm) {
        m.lock();
        on_exit m.unlock();
        pgm = new_pgm;
        if (delivery) {
            delivery.setProgram(new_pgm);
        }
    }

    #! delivers an event of the session itself (\c "eval-started", \c "eval-finished") while events are delivered
    pushSessionEvent(string event, string input) {
        m.lock();
        on_exit m.unlock();
        if (delivery) {
            delivery.pushSessionEvent(event, input);
        }
    }

    #! stops the delivery and removes the listeners; waits for the delivering thread to end unless called by it
    stop() {
        *ReplEventDelivery ended;
        {
            m.lock();
            on_exit m.unlock();
            if (stopped) {
                return;
            }
            stopped = True;
            remove listeners;
            ended = remove delivery;
        }
        if (ended) {
            ended.stop();
        }
    }

    #! raises an exception if the dispatcher has been stopped
    private checkStoppedIntern() {
        if (stopped) {
            throw "REPL-ERROR", "the REPL session has been closed";
        }
    }

    #! returns a snapshot of the Program's background threads and activity; called with the lock held
    private hash<ProgramThreadSubscription> snapshotIntern() {
        Queue q();
        hash<ProgramThreadSubscription> s = pgm.subscribeThreadEvents(q,
            <ProgramThreadEventOptions>{"events": PTE_CLOSED});
        pgm.unsubscribeThreadEvents(s.id);
        return s;
    }
}

#! The delivery of the events of a session while it has listeners: a subscription and the thread that reads it
class ReplEventDelivery {
    private {
        #! guards the members below
        Mutex m();

        #! the dispatcher whose listeners are called
        ReplEventDispatcher dispatcher;

        #! the queue of the subscription and of the session's own events
        Queue queue();

        #! the Program subscribed to
        Program pgm;

        #! the ID of the current subscription, until the delivery is stopped
        *int sub_id;

        #! the ID of the Program
        int program_id;

        #! the background threads of the Program, by thread ID
        hash<string, hash<ProgramThreadInfo>> threads;

        #! True if the Program is idle
        bool idle = True;

        #! the TID of the delivering thread
        int tid;

        #! signalled when the delivering thread ends
        Counter done(1);
    }

    #! subscribes to the Program's events and starts the delivering thread
    constructor(ReplEventDispatcher dispatcher, Program pgm) {
        self.dispatcher = dispatcher;
        self.pgm = pgm;
        subscribeIntern();
        tid = background run();
    }

    #! returns the background threads, in the order they started
    list<hash<ProgramThreadInfo>> getThreads() {
        m.lock();
        on_exit m.unlock();
        return (map threads{$1}, sort(map $1.toInt(), keys threads)) ?? ();
    }

    #! returns True if the Program is idle
    bool isIdle() {
        m.lock();
        on_exit m.unlock();
        return idle;
    }

    #! subscribes to a new Program and queues \c "program-replaced"
    setProgram(Program new_pgm) {
        m.lock();
        on_exit m.unlock();
        if (!exists sub_id) {
            return;
        }
        pgm.unsubscribeThreadEvents(sub_id);
        pgm = new_pgm;
        hash<ProgramThreadSubscription> s = subscribeIntern();
        # processed after any event of the old subscription still in the queue, which is discarded
        queue.push(<ReplDeliveryMessage>{"replaced": <ReplEvent>{"event": "program-replaced",
            "program_id": s.program_id, "running": s.threads.size(), "idle": s.idle, "threads": s.threads}});
    }

    #! queues an event of the session itself
    pushSessionEvent(string event, string input) {
        m.lock();
        on_exit m.unlock();
        if (exists sub_id) {
            queue.push(<ReplDeliveryMessage>{"session": event, "input": input});
        }
    }

    #! removes the subscription and ends the delivering thread; waits for it to end unless called by it
    stop() {
        {
            m.lock();
            on_exit m.unlock();
            if (!exists sub_id) {
                return;
            }
            try {
                pgm.unsubscribeThreadEvents(sub_id);
            } catch (hash<ExceptionInfo> ex) {
                # the Program has been destroyed: its subscriptions have ended
            }
            remove sub_id;
            queue.push(<ReplDeliveryMessage>{"stop": True});
        }
        if (tid != gettid()) {
            done.waitForZero();
        }
    }

    #! subscribes to the current Program's thread events; called with the lock held, or by the constructor
    private hash<ProgramThreadSubscription> subscribeIntern() {
        hash<ProgramThreadSubscription> s = pgm.subscribeThreadEvents(queue,
            <ProgramThreadEventOptions>{"events": PTE_ALL});
        sub_id = s.id;
        program_id = s.program_id;
        idle = s.idle;
        threads = map {$1.thread_id: $1}, s.threads;
        return s;
    }

    #! the delivering thread: reads the queue and calls the listeners
    private run() {
        on_exit done.dec();
        while (True) {
            # a Program's thread event or a message of the session
            hash<auto> msg = queue.get();
            if (msg instanceof hash<ReplDeliveryMessage> && msg.stop) {
                break;
            }
            *hash<ReplEvent> ev;
            {
                m.lock();
                on_exit m.unlock();
                ev = processIntern(msg);
            }
            if (!ev) {
                continue;
            }
            foreach code<nothing(hash<ReplEvent>)> listener in (dispatcher.getListeners()) {
                try {
                    listener(ev);
                } catch (hash<ExceptionInfo> ex) {
                    # a listener's exception does not stop the delivery to other listeners or of other events
                }
            }
        }
    }

    #! updates the state with a message from the queue and returns the event to deliver, if any; called with the
    #! lock held
    private *hash<ReplEvent> processIntern(hash<auto> msg) {
        # the delivery has been stopped
        if (!exists sub_id) {
            return;
        }
        if (msg instanceof hash<ProgramThreadEvent>) {
            hash<ProgramThreadEvent> pev = cast<hash<ProgramThreadEvent>>(msg);
            # an event of a replaced Program's subscription
            if (pev.subscription_id != sub_id) {
                return;
            }
            string event;
            switch (pev.event) {
                case "STARTED":
                    threads{pev.thread_id} = <ProgramThreadInfo>{
                        "tid": pev.tid,
                        "thread_id": pev.thread_id,
                        "parent_tid": pev.parent_tid,
                        "parent_thread_id": pev.parent_thread_id,
                        "name": pev.name,
                        "start": pev.start,
                        "cancel_requested": False,
                    };
                    event = "thread-started";
                    break;
                case "CANCEL_REQUESTED":
                    if (threads{pev.thread_id}) {
                        threads{pev.thread_id}.cancel_requested = True;
                    }
                    event = "thread-cancel-requested";
                    break;
                case "FINISHED":
                    remove threads{pev.thread_id};
                    event = "thread-finished";
                    break;
                case "ACTIVITY":
                    idle = pev.idle;
                    event = idle ? "program-idle" : "program-busy";
                    break;
                default:
                    # CLOSED: the Program has been destroyed
                    return;
            }
            hash<ReplEvent> rv = <ReplEvent>{"event": event, "program_id": program_id, "running": threads.size(),
                "idle": idle};
            # only thread events carry the Program's event
            if (pev.event != "ACTIVITY") {
                rv.thread = pev;
            }
            return rv;
        }
        hash<ReplDeliveryMessage> dmsg = cast<hash<ReplDeliveryMessage>>(msg);
        if (dmsg.replaced) {
            return dmsg.replaced;
        }
        return <ReplEvent>{"event": dmsg.session, "program_id": program_id, "running": threads.size(),
            "idle": idle, "input": dmsg.input};
    }
}

#! formats the notices of a batch of session events; see @ref QoreRepl::QoreRepl::formatThreadNotices()
class ReplThreadNoticeFormatter {
    public {
        #! the outcomes reported as failures
        const FailureOutcomes = {
            "EXCEPTION": "failed",
            "INTERRUPTED": "interrupted",
            "START_FAILED": "failed to start",
        };
    }

    #! returns the notices of a batch of events
    static list<string> format(list<hash<ReplEvent>> events, string level) {
        if (!inlist(level, ThreadNoticeLevels)) {
            throw "REPL-ERROR", sprintf("unknown thread notice level %y; expecting one of: %s", level,
                ThreadNoticeLevels.join(", "));
        }
        list<string> rv();
        if (level == "off") {
            return rv;
        }
        list<hash<ProgramThreadEvent>> l = (map $1.thread, events, $1.thread) ?? ();
        if (level == "all") {
            foreach hash<ProgramThreadEvent> ev in (l) {
                switch (ev.event) {
                    case "STARTED":
                        rv += sprintf("Thread %d started%s", ev.tid, ev.parent_thread_id
                            ? sprintf(" by thread %d", ev.parent_tid)
                            : "");
                        break;
                    case "CANCEL_REQUESTED":
                        rv += sprintf("Thread %d: cancellation requested%s", ev.tid, ev.cancel_reason
                            ? sprintf(" (%s)", ev.cancel_reason)
                            : "");
                        break;
                    case "FINISHED":
                        rv += finishedLines(ev);
                        break;
                }
            }
            return rv;
        }

        # summary and failures
        list<string> failures();
        foreach hash<ProgramThreadEvent> ev in (l) {
            if (ev.event == "FINISHED") {
                if (FailureOutcomes{ev.outcome} || ev.cleanup_error) {
                    failures += finishedLines(ev, True);
                }
            }
        }
        if (level == "failures") {
            return failures;
        }

        # the threads that finished in the batch
        hash<string, hash<ProgramThreadEvent>> finished = map {$1.thread_id: $1}, l, $1.event == "FINISHED";
        list<int> started = (map $1.tid, l, $1.event == "STARTED" && !finished{$1.thread_id}) ?? ();
        list<int> requested = (map $1.tid, l, $1.event == "CANCEL_REQUESTED"
            && finished{$1.thread_id}.outcome != "CANCELLED") ?? ();
        list<hash<ProgramThreadEvent>> completed = (map $1, l, $1.event == "FINISHED"
            && inlist($1.outcome, ("COMPLETED", "EXITED"))) ?? ();
        list<int> cancelled = (map $1.tid, l, $1.event == "FINISHED" && $1.outcome == "CANCELLED") ?? ();

        if (started) {
            rv += group(started, "started");
        }
        if (requested) {
            rv += requested.size() == 1
                ? sprintf("Thread %d: cancellation requested", requested[0])
                : sprintf("Cancellation requested for %d threads (%s)", requested.size(), requested.join(", "));
        }
        if (completed) {
            rv += completed.size() == 1
                ? sprintf("Thread %d finished (%s)", completed[0].tid, formatDuration(completed[0].duration_us))
                : group((map $1.tid, completed), "finished");
        }
        if (cancelled) {
            rv += group(cancelled, "cancelled");
        }
        return rv + failures;
    }

    #! returns the notice of threads grouped by what happened to them
    private static string group(list<int> tids, string what) {
        return tids.size() == 1
            ? sprintf("Thread %d %s", tids[0], what)
            : sprintf("%d threads %s (%s)", tids.size(), what, tids.join(", "));
    }

    #! returns the notices of a thread that finished
    /** @param ev the event
        @param failures_only only the notices of a failure or of an error in the thread's teardown
    */
    private static list<string> finishedLines(hash<ProgramThreadEvent> ev, *bool failures_only) {
        list<string> rv();
        if (*string failure = FailureOutcomes{ev.outcome}) {
            rv += sprintf("Thread %d %s: %s", ev.tid, failure, formatError(ev.error));
        } else if (!failures_only) {
            switch (ev.outcome) {
                case "CANCELLED":
                    rv += sprintf("Thread %d cancelled (%s)", ev.tid, formatDuration(ev.duration_us));
                    break;
                case "EXITED":
                    rv += sprintf("Thread %d exited (%s)", ev.tid, formatDuration(ev.duration_us));
                    break;
                default:
                    rv += sprintf("Thread %d finished (%s)", ev.tid, formatDuration(ev.duration_us));
                    break;
            }
        }
        if (ev.cleanup_error) {
            rv += sprintf("Thread %d: error in cleanup: %s", ev.tid, formatError(ev.cleanup_error));
        }
        return rv;
    }

    #! returns a summary of an exception
    private static string formatError(*hash<ExceptionInfo> ex) {
        if (!ex) {
            return "unknown error";
        }
        string rv = ex.err ?? "(error)";
        if (ex.desc) {
            rv += ": " + ex.desc;
        }
        if (ex.file) {
            rv += sprintf(" (%s:%d)", ex.file, ex.line);
        }
        return rv;
    }

    #! returns a duration in seconds with one decimal
    static string formatDuration(*int us) {
        return sprintf("%.1fs", (us ?? 0) / 1000000.0);
    }
}
}
