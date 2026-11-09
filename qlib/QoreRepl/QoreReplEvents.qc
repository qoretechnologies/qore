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
      background thread, a callback), or none does; parsing, @c /load, @c /type and @c /reset need an idle Program;
      delivered only to the listeners added with \a activity set, as every call into the Program makes it active
      and idle
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

    #! @ref True if the Program is idle after the event; set only while a listener receives the activity events, and
    #! for \c "program-replaced"
    *bool idle;

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
    no thread; its state is then taken from a snapshot.  The Program's activity (\c ACTIVITY events) is subscribed to
    only while a listener asked for it, as every call into the Program makes it active and idle.

    The thread holds the dispatcher, not the REPL, so the REPL can be destroyed while it runs: its destructor stops
    the dispatcher.  The module's exit hook stops every delivery before the process waits for its threads
    (ReplDeliveryRegistry), so a session kept until the process ends does not keep it from ending.
*/
class ReplEventDispatcher {
    private {
        #! guards the members below
        Mutex m();

        #! the session's Program
        Program pgm;

        #! the delivery, while the session has listeners
        *ReplEventDelivery delivery;

        #! the listeners, by ID, in the order they were added
        hash<string, code<nothing(hash<ReplEvent>)>> listeners;

        #! the IDs of the listeners that receive the Program's activity events
        hash<string, bool> activity_listeners;

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
    /** @param listener the listener
        @param activity @ref True if the listener receives the Program's activity events
    */
    int addListener(code<nothing(hash<ReplEvent>)> listener, bool activity) {
        m.lock();
        on_exit m.unlock();
        checkStoppedIntern();
        if (!delivery) {
            delivery = new ReplEventDelivery(self, pgm);
        }
        string id = string(++listener_id);
        listeners{id} = listener;
        if (activity) {
            activity_listeners{id} = True;
            delivery.setActivity(True);
        }
        return listener_id;
    }

    #! sets whether a listener receives the Program's activity events; returns @ref False if there is no such
    #! listener
    bool setListenerActivity(int id, bool activity) {
        m.lock();
        on_exit m.unlock();
        if (!exists listeners{id}) {
            return False;
        }
        if (activity) {
            activity_listeners{id} = True;
        } else {
            remove activity_listeners{id};
        }
        delivery.setActivity(!!activity_listeners);
        return True;
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
            remove activity_listeners{id};
            if (!listeners) {
                ended = remove delivery;
            } else if (!activity_listeners) {
                delivery.setActivity(False);
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

    #! returns @ref True if the Program is idle: as of the last event processed while a listener receives activity
    #! events, otherwise from a snapshot
    bool isIdle() {
        m.lock();
        on_exit m.unlock();
        checkStoppedIntern();
        if (delivery && activity_listeners) {
            return delivery.isIdle();
        }
        return snapshotIntern().idle;
    }

    #! returns the listeners to call with an event, in the order they were added
    /** @param activity @ref True for an activity event, which only the listeners that asked for them receive
    */
    list<code<nothing(hash<ReplEvent>)>> getListeners(bool activity) {
        m.lock();
        on_exit m.unlock();
        return (map listeners{$1}, keys listeners, !activity || activity_listeners{$1}) ?? ();
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
            remove activity_listeners;
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

#! The live deliveries of the module, which its exit hook stops before the process waits for its threads
/** A delivery's thread runs until it is told to stop, and a process waits for its threads before it destroys its
    global variables and the local variables of its top-level code, whose destructors would stop it: a session kept
    in one with a listener would keep the process from ending.
*/
class ReplDeliveryRegistry {
    private {
        #! guards the members below
        static Mutex m();

        #! the live deliveries, by ID
        static hash<string, ReplEventDelivery> deliveries;

        #! the last ID handed out
        static int last_id = 0;

        #! True once the exit hook has run: deliveries started afterwards are stopped at once
        static bool exiting = False;
    }

    #! registers the module's exit hook; called by the module's init closure
    /** A Program whose code may not register exit hooks (a sandbox governs it) loads the module without the hook:
        its sessions then keep the process from ending while they have listeners, until they are closed
    */
    static init() {
        try {
            register_exit_hook(\ReplDeliveryRegistry::stopAll());
        } catch (hash<ExceptionInfo> ex) {
            # no hook: see above
        }
    }

    #! registers a delivery; returns its ID, or @ref nothing if the exit hook has run, in which case the delivery
    #! stops at once
    static *int register(ReplEventDelivery delivery) {
        m.lock();
        on_exit m.unlock();
        if (exiting) {
            return;
        }
        deliveries{++last_id} = delivery;
        return last_id;
    }

    #! deregisters a delivery that has stopped
    static deregister(int id) {
        m.lock();
        on_exit m.unlock();
        remove deliveries{id};
    }

    #! the exit hook: tells every delivery to stop, without waiting for it
    static stopAll() {
        list<ReplEventDelivery> l;
        {
            m.lock();
            on_exit m.unlock();
            exiting = True;
            l = (map $1, deliveries.values()) ?? ();
            remove deliveries;
        }
        map $1.requestStop(), l;
    }
}

#! The delivery of the events of a session while it has listeners: subscriptions and the thread that reads them
class ReplEventDelivery {
    private {
        #! guards the members below
        Mutex m();

        #! the dispatcher whose listeners are called
        ReplEventDispatcher dispatcher;

        #! the queue of the subscriptions and of the session's own events
        Queue queue();

        #! the Program subscribed to
        Program pgm;

        #! the ID of the subscription to the thread events, until the delivery is stopped
        *int sub_id;

        #! the ID of the subscription to the activity events, while a listener receives them
        *int activity_sub_id;

        #! the ID of the Program
        int program_id;

        #! the background threads of the Program, by thread ID, in the order they started
        hash<string, hash<ProgramThreadInfo>> threads;

        #! True if the Program is idle, while subscribed to its activity
        bool idle = True;

        #! the TID of the delivering thread
        int tid;

        #! the ID of the delivery in ReplDeliveryRegistry, if registered
        *int registry_id;

        #! signalled when the delivering thread ends
        Counter done(1);
    }

    #! subscribes to the Program's thread events and starts the delivering thread
    constructor(ReplEventDispatcher dispatcher, Program pgm) {
        self.dispatcher = dispatcher;
        self.pgm = pgm;
        subscribeIntern();
        tid = background run();
        registry_id = ReplDeliveryRegistry::register(self);
        if (!exists registry_id) {
            # the process is ending
            requestStop();
        }
    }

    #! returns the background threads, in the order they started
    list<hash<ProgramThreadInfo>> getThreads() {
        m.lock();
        on_exit m.unlock();
        return threads ? threads.values() : ();
    }

    #! returns True if the Program is idle
    bool isIdle() {
        m.lock();
        on_exit m.unlock();
        return idle;
    }

    #! subscribes to or unsubscribes from the Program's activity events
    setActivity(bool activity) {
        m.lock();
        on_exit m.unlock();
        if (!exists sub_id || activity == exists activity_sub_id) {
            return;
        }
        if (activity) {
            subscribeActivityIntern();
        } else {
            pgm.unsubscribeThreadEvents(remove activity_sub_id);
        }
    }

    #! subscribes to a new Program and queues \c "program-replaced"
    setProgram(Program new_pgm) {
        m.lock();
        on_exit m.unlock();
        if (!exists sub_id) {
            return;
        }
        bool activity = exists activity_sub_id;
        unsubscribeIntern();
        pgm = new_pgm;
        hash<ProgramThreadSubscription> s = subscribeIntern();
        if (activity) {
            subscribeActivityIntern();
        }
        # processed after any event of the old subscriptions still in the queue, which are discarded
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

    #! removes the subscriptions and tells the delivering thread to end, without waiting for it
    requestStop() {
        m.lock();
        on_exit m.unlock();
        if (!exists sub_id) {
            return;
        }
        try {
            unsubscribeIntern();
        } catch (hash<ExceptionInfo> ex) {
            # the Program has been destroyed: its subscriptions have ended
        }
        remove sub_id;
        remove activity_sub_id;
        queue.push(<ReplDeliveryMessage>{"stop": True});
    }

    #! removes the subscriptions and ends the delivering thread; waits for it to end unless called by it
    stop() {
        requestStop();
        if (tid != gettid()) {
            done.waitForZero();
        }
    }

    #! removes the subscriptions; called with the lock held
    private unsubscribeIntern() {
        pgm.unsubscribeThreadEvents(sub_id);
        if (exists activity_sub_id) {
            pgm.unsubscribeThreadEvents(activity_sub_id);
        }
    }

    #! subscribes to the current Program's thread events; called with the lock held, or by the constructor
    private hash<ProgramThreadSubscription> subscribeIntern() {
        hash<ProgramThreadSubscription> s = pgm.subscribeThreadEvents(queue, <ProgramThreadEventOptions>{
            "events": PTE_STARTED | PTE_CANCEL_REQUESTED | PTE_FINISHED | PTE_CLOSED,
        });
        sub_id = s.id;
        program_id = s.program_id;
        threads = map {$1.thread_id: $1}, s.threads;
        return s;
    }

    #! subscribes to the current Program's activity events; called with the lock held
    private subscribeActivityIntern() {
        hash<ProgramThreadSubscription> s = pgm.subscribeThreadEvents(queue,
            <ProgramThreadEventOptions>{"events": PTE_ACTIVITY});
        activity_sub_id = s.id;
        idle = s.idle;
    }

    #! the delivering thread: reads the queue and calls the listeners
    private run() {
        on_exit {
            if (exists registry_id) {
                ReplDeliveryRegistry::deregister(registry_id);
            }
            done.dec();
        }
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
            bool activity = ev.event == "program-idle" || ev.event == "program-busy";
            foreach code<nothing(hash<ReplEvent>)> listener in (dispatcher.getListeners(activity)) {
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
            if (pev.event == "ACTIVITY") {
                # an event of a removed or replaced subscription
                if (pev.subscription_id != activity_sub_id) {
                    return;
                }
                idle = pev.idle;
                return <ReplEvent>{"event": idle ? "program-idle" : "program-busy", "program_id": program_id,
                    "running": threads.size(), "idle": idle};
            }
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
                default:
                    # CLOSED: the Program has been destroyed
                    return;
            }
            return <ReplEvent>{"event": event, "program_id": program_id, "running": threads.size(),
                "idle": currentIdleIntern(), "thread": pev};
        }
        hash<ReplDeliveryMessage> dmsg = cast<hash<ReplDeliveryMessage>>(msg);
        if (dmsg.replaced) {
            return dmsg.replaced;
        }
        return <ReplEvent>{"event": dmsg.session, "program_id": program_id, "running": threads.size(),
            "idle": currentIdleIntern(), "input": dmsg.input};
    }

    #! returns the Program's activity state if it is known (subscribed to its activity); called with the lock held
    private *bool currentIdleIntern() {
        return exists activity_sub_id ? idle : NOTHING;
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
