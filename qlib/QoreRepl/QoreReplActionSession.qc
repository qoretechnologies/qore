# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore REPL action session class definition

/** QoreReplActionSession.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Action-based REPL session for Creator WebSocket integration
/** This class provides a transport-agnostic action API for REPL use and emits
    a list of events for each action.

    With an emitter set with @ref QoreRepl::QoreReplActionSession::setEmitter() "setEmitter()", the session also
    emits asynchronous events about the background threads that the session's code starts and the Program's
    activity (see @ref qorerepl_thread_events):
    - \c repl-thread: a background thread started, cancellation was requested for it, or it ended; \c data has
      \c kind (\c "started", \c "cancel-requested" or \c "finished"), \c tid, \c thread_id, \c parent_tid,
      \c parent_thread_id, \c name, \c start, \c running (the number of background threads running after the
      event) and \c notices (the notices of the event at the session's level, see
      @ref QoreRepl::QoreRepl::formatThreadNotices() "QoreRepl::formatThreadNotices()"); for \c "finished" also
      \c outcome, \c duration_ms, \c error and \c cleanup_error (\c err, \c desc, \c file and \c line of the
      exception), and for \c "cancel-requested" \c cancel_reason
    - \c repl-activity: the Program became idle or active; \c data has \c idle and \c running; emitted only when
      enabled (see @ref QoreRepl::QoreReplActionSession::setActivityEvents() "setActivityEvents()"), as every call
      into the Program produces them
    - \c repl-program-replaced: @c /reset replaced the Program; \c data has \c program_id, \c running and
      \c threads (the new Program's background threads)
*/
public class QoreReplActionSession {
    public {
        #! Protocol version
        const ProtocolVersion = "1.0";
    }

    private {
        #! REPL instance
        QoreRepl repl;

        #! Session ID
        string session_id;

        #! guards the emitter
        Mutex emitter_lock();

        #! the emitter of asynchronous events, if any
        *code<nothing(hash<auto>)> emitter;

        #! the ID of the REPL listener that calls the emitter, if any
        *int listener_id;

        #! True if the emitter receives activity events
        bool activity = False;
    }

    #! Creates a new action session
    constructor(*hash<auto> options) {
        repl = new QoreRepl(options);
        session_id = get_random_string(16);
    }

    #! Sets or removes the callback for the session's asynchronous events
    /** @param cb called with each event, a hash with \c event and \c data keys (see the class description), on a
        thread of the session that delivers the events in order; @ref nothing removes the callback, which ends that
        thread; an exception that the callback raises is ignored
        @param activity @ref True to emit \c repl-activity events as well; see
        @ref QoreRepl::QoreReplActionSession::setActivityEvents() "setActivityEvents()"

        @par Example:
        @code{.py}
QoreReplActionSession session();
session.setEmitter(sub (hash<auto> ev) { connection.send(make_json(ev)); });
        @endcode

        @throw REPL-ERROR the session has been closed

        @note the callback is called until it is removed or the session is closed with
        @ref QoreRepl::QoreReplActionSession::close() "close()"; a process waits for the thread that calls it
        before it ends
    */
    setEmitter(*code<nothing(hash<auto>)> cb, bool activity = False) {
        *int old_id;
        {
            emitter_lock.lock();
            on_exit emitter_lock.unlock();
            emitter = cb;
            self.activity = activity;
            if (cb && !exists listener_id) {
                listener_id = repl.addEventListener(\replEvent(), activity);
            } else if (cb) {
                repl.setEventListenerActivity(listener_id, activity);
            } else if (exists listener_id) {
                old_id = remove listener_id;
            }
        }
        # removed outside the lock: the delivering thread may be calling replEvent(), which takes it
        if (exists old_id) {
            repl.removeEventListener(old_id);
        }
    }

    #! Sets whether the emitter receives \c repl-activity events
    /** @param activity @ref True to emit them; they are emitted for every call into the Program

        @return @ref True if an emitter is set, @ref False if not (the setting applies to the next emitter set with
        @ref QoreRepl::QoreReplActionSession::setEmitter() "setEmitter()" only through its own argument)
    */
    bool setActivityEvents(bool activity) {
        emitter_lock.lock();
        on_exit emitter_lock.unlock();
        self.activity = activity;
        if (!exists listener_id) {
            return False;
        }
        repl.setEventListenerActivity(listener_id, activity);
        return True;
    }

    #! Ends the session's asynchronous events and closes its REPL
    /** Background threads of the session's Program keep running.
    */
    close() {
        {
            emitter_lock.lock();
            on_exit emitter_lock.unlock();
            remove emitter;
            remove listener_id;
        }
        repl.close();
    }

    #! Returns session info
    hash<auto> getSessionInfo() {
        return {
            "sessionId": session_id,
            "version": ProtocolVersion,
            "qoreVersion": Qore::VersionString,
        };
    }

    #! Returns the REPL instance
    QoreRepl getRepl() {
        return repl;
    }

    #! Returns the session ID
    string getSessionId() {
        return session_id;
    }

    #! Returns prompt events without evaluation
    list<hash<auto>> getPromptEvents(*string request_id) {
        return (
            makePromptEvent(request_id),
        );
    }

    #! Handles a REPL action
    list<hash<auto>> handleAction(string action, *hash<auto> args) {
        switch (action) {
            case "repl-eval":
                return handleEval(args);

            case "repl-command":
                return handleCommand(args);

            case "repl-interrupt":
                return handleInterrupt(args);

            case "repl-complete":
                return handleComplete(args);

            case "repl-close":
                return handleClose(args);
        }

        throw "INVALID-ACTION", sprintf("Unsupported action %y", action);
    }

    private list<hash<auto>> handleEval(*hash<auto> args) {
        string code = getRequiredStringArg(args, "code");
        *string request_id = args.request_id ?? args.id;
        return evalWithCode(code, request_id);
    }

    private list<hash<auto>> handleCommand(*hash<auto> args) {
        string command = getRequiredStringArg(args, "command");
        *string request_id = args.request_id ?? args.id;
        if (command !~ /^\//) {
            command = "/" + command;
        }
        return evalWithCode(command, request_id);
    }

    private list<hash<auto>> handleInterrupt(*hash<auto> args) {
        *string request_id = args.request_id ?? args.id;
        return (
            makeErrorEvent(request_id, "NOT-IMPLEMENTED", "Interrupt is not currently supported"),
        );
    }

    private list<hash<auto>> handleComplete(*hash<auto> args) {
        string input = getRequiredStringArg(args, "input");
        *string request_id = args.request_id ?? args.id;
        list<hash<CompletionItem>> items = repl.getCompletionDetails(input);
        return (
            {
                "event": "repl-completion",
                "data": {
                    "request_id": request_id,
                    "items": items,
                },
            },
        );
    }

    private list<hash<auto>> handleClose(*hash<auto> args) {
        *string request_id = args.request_id ?? args.id;
        return (
            makeExitEvent("user_request", request_id),
        );
    }

    private list<hash<auto>> evalWithCode(string code, *string request_id) {
        date start = now_us();
        *hash<auto> result = repl.eval(code);
        int duration_ms = (now_us() - start).durationMicroseconds() / 1000;

        if (!exists result) {
            return (
                makePromptEvent(request_id),
            );
        }

        if (result.exit) {
            return (
                makeExitEvent("user_request", request_id),
            );
        }

        list<hash<auto>> events = ();
        if (result.error) {
            events += makeErrorEvent(request_id, result.error.err, result.error.desc, result.error);
        } else {
            events += makeResultEvent(request_id, result.value, duration_ms);
        }
        events += makePromptEvent(request_id);
        return events;
    }

    private hash<auto> makePromptEvent(*string request_id) {
        return {
            "event": "repl-prompt",
            "data": {
                "request_id": request_id,
                "value": repl.getPrompt(),
                "needs_more_input": repl.needsMoreInput(),
                "indent_level": repl.getIndentLevel(),
            },
        };
    }

    private hash<auto> makeResultEvent(*string request_id, auto value, int duration_ms) {
        hash<auto> data = {
            "request_id": request_id,
            "value": QoreRepl::formatValue(value),
            "duration_ms": duration_ms,
        };
        if (exists value) {
            data.value_type = value.fullType();
        }
        return {
            "event": "repl-result",
            "data": data,
        };
    }

    private hash<auto> makeExitEvent(string reason, *string request_id) {
        return {
            "event": "repl-exit",
            "data": {
                "request_id": request_id,
                "reason": reason,
            },
        };
    }

    private hash<auto> makeErrorEvent(*string request_id, string error, string description,
            *hash<ExceptionInfo> ex) {
        hash<auto!> data = {
            "request_id": request_id,
            "code": error,
            "message": description,
        };
        if (ex && ex.err) {
            data.details = ex;
        }
        return {
            "event": "repl-error",
            "data": data,
        };
    }

    #! Emits the action event of a REPL session event, if any; called on the REPL's delivering thread
    private replEvent(hash<ReplEvent> ev) {
        *hash<auto> out = makeAsyncEvent(ev);
        if (!out) {
            return;
        }
        *code<nothing(hash<auto>)> cb;
        {
            emitter_lock.lock();
            on_exit emitter_lock.unlock();
            cb = emitter;
        }
        if (cb) {
            cb(out);
        }
    }

    #! Returns the action event of a REPL session event, or @ref nothing for events that are not emitted
    private *hash<auto> makeAsyncEvent(hash<ReplEvent> ev) {
        switch (ev.event) {
            case "thread-started":
            case "thread-cancel-requested":
            case "thread-finished": {
                hash<ProgramThreadEvent> t = ev.thread;
                hash<auto> data = {
                    "kind": ev.event.substr(7),
                    "tid": t.tid,
                    "thread_id": t.thread_id,
                    "parent_tid": t.parent_tid,
                    "parent_thread_id": t.parent_thread_id,
                    "name": t.name,
                    "start": t.start,
                    "running": ev.running,
                    "notices": repl.getThreadNotices((ev,)),
                };
                if (ev.event == "thread-finished") {
                    data += {
                        "outcome": t.outcome,
                        "duration_ms": (t.duration_us ?? 0) / 1000,
                        "error": makeErrorInfo(t.error),
                        "cleanup_error": makeErrorInfo(t.cleanup_error),
                    };
                } else if (ev.event == "thread-cancel-requested") {
                    data.cancel_reason = t.cancel_reason;
                }
                return {"event": "repl-thread", "data": data};
            }

            case "program-idle":
            case "program-busy":
                return {"event": "repl-activity", "data": {"idle": ev.idle, "running": ev.running}};

            case "program-replaced":
                return {"event": "repl-program-replaced", "data": {
                    "program_id": ev.program_id,
                    "running": ev.running,
                    "threads": ev.threads,
                }};
        }
    }

    #! Returns the summary of an exception sent with thread events, or @ref nothing
    private static *hash<auto> makeErrorInfo(*hash<ExceptionInfo> ex) {
        if (!ex) {
            return;
        }
        return {
            "err": ex.err,
            "desc": ex.desc,
            "file": ex.file,
            "line": ex.line,
        };
    }

    private string getRequiredStringArg(*hash<auto> args, string name) {
        if (!args || !exists args{name}) {
            throw "MISSING-PARAMETER", sprintf("Missing '%s' parameter", name);
        }
        if (args{name}.typeCode() != NT_STRING) {
            throw "INVALID-PARAMETER", sprintf("Parameter '%s' must be a string", name);
        }
        return args{name};
    }
}

} # namespace QoreRepl
