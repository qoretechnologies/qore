# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AgUiEmitter.qc AG-UI protocol event emitter

/*  AgUiEmitter.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! The %AgUi namespace contains all public definitions in the %AgUi module
public namespace AgUi {
#! Emits AG-UI protocol events into a caller-supplied sink
/** The emitter does not own a socket.  It formats events and hands each one to the \c sink
    closure the caller supplied, exactly as
    @ref A2aServerHandler::A2aServerConnection "A2aServerConnection" does, so the same emitter
    works over an HTTP/SSE response stream, a WebSocket, a test harness that collects events in
    a list, or anything else — the transport stays where it already is.

    @par What the emitter guarantees
    - a run emits @ref AGUI_RunStarted first and exactly one of @ref AGUI_RunFinished or
      @ref AGUI_RunError last
    - \c timestamp is set on every event, from a monotonic-ordered counter so that two events
      emitted in the same millisecond still order correctly
    - a state delta is computed against the state the emitter last sent, so a client that
      applies the deltas in order reconstructs exactly what the server has
    - @ref nothing -valued members are dropped before serialization, matching the reference
      SDK encoder, so an optional member the caller did not set is absent rather than \c null

    @par What it does not do
    It does not validate that the caller's event sequence is well-formed beyond the run
    lifecycle (for example, it does not check that a \c TEXT_MESSAGE_CONTENT event names a
    message that was started).  Sequencing is the caller's, since the caller is the only one
    that knows the shape of its own output.

    @par Example:
    @code{.py}
AgUiEmitter emitter(sub (string frame) { stream.queueRawString(frame); });
emitter.runStarted("thread-1", "run-1");
on_error emitter.runError($1.desc);

string mid = emitter.startTextMessage();
emitter.textMessageContent(mid, "Hello, ");
emitter.textMessageContent(mid, "world");
emitter.endTextMessage(mid);

emitter.stateSnapshot({"progress": 0});
emitter.stateDelta({"progress": 100});   # emits the JSON Patch, not the whole state

emitter.runFinished();
    @endcode
*/
public class AgUiEmitter {
    public {
        #! The SSE content type an AG-UI HTTP endpoint responds with
        const ContentType = MimeTypeSse;
    }

    private {
        #! Receives each formatted event; see @ref setSink()
        *code<nothing(string)> sink;

        #! Encodes an event hash into whatever the sink expects
        code<string(hash<auto>)> encoder;

        #! The thread this run belongs to
        *string thread_id;

        #! The current run ID
        *string run_id;

        #! Set once @ref runStarted() has been emitted
        bool run_started = False;

        #! Set once @ref runFinished() or @ref runError() has been emitted
        bool run_ended = False;

        #! The state as the client last saw it, used as the base for @ref stateDelta()
        auto last_state;

        #! Set once a state snapshot has been sent
        bool have_state = False;

        #! Monotonically increasing per-emitter sequence for generated IDs
        int id_seq = 0;

        #! Protects the emitter's mutable state
        Mutex lck();
    }

    #! Creates the emitter
    /** @param sink receives each formatted event; may be @ref nothing if it is set later with
            @ref setSink()
        @param encoder formats an event hash; defaults to
            @ref AgUiEmitter::encodeSse() "SSE framing"

        @throw AGUI-EMITTER-ERROR the encoder is not callable
    */
    constructor(*code<nothing(string)> sink, *code<string(hash<auto>)> encoder) {
        self.sink = sink;
        self.encoder = encoder ?? \AgUiEmitter::encodeSse();
    }

    #! Sets or replaces the sink
    /** @param sink receives each formatted event
    */
    setSink(*code<nothing(string)> sink) {
        AutoLock al(lck);
        self.sink = sink;
    }

    #! Returns the current run ID, if a run is in progress
    *string getRunId() {
        AutoLock al(lck);
        return run_id;
    }

    #! Returns the current thread ID, if a run is in progress
    *string getThreadId() {
        AutoLock al(lck);
        return thread_id;
    }

    #! Returns @ref True if a run has started and has not yet ended
    bool isRunning() {
        AutoLock al(lck);
        return run_started && !run_ended;
    }

    #! Returns the state as the client last saw it
    auto getState() {
        AutoLock al(lck);
        return last_state;
    }

    #! Emits @ref AGUI_RunStarted and begins a run
    /** @param thread_id the conversation thread; generated if not supplied
        @param run_id the run identifier; generated if not supplied

        @return the run ID

        @throw AGUI-STATE-ERROR a run is already in progress
    */
    string runStarted(*string thread_id, *string run_id) {
        hash<AgUiRunStartedEvent> event;
        {
            AutoLock al(lck);
            if (run_started && !run_ended) {
                throw "AGUI-STATE-ERROR", sprintf("run %y is already in progress; finish it "
                    "before starting another", self.run_id);
            }
            self.thread_id = thread_id ?? makeIdLocked("thread");
            self.run_id = run_id ?? makeIdLocked("run");
            run_started = True;
            run_ended = False;
            # a new run means the client starts from nothing again
            have_state = False;
            remove last_state;
            event = <AgUiRunStartedEvent>{
                "type": AGUI_RunStarted,
                "timestamp": nowMs(),
                "threadId": self.thread_id,
                "runId": self.run_id,
            };
        }
        emitEvent(event);
        return event.runId;
    }

    #! Emits @ref AGUI_RunFinished and ends the run
    /** Idempotent with @ref runError(): whichever is called first ends the run, and the
        second call is a no-op.  That is what makes
        \c on_error emitter.runError(...) safe to write next to a normal
        \c emitter.runFinished() at the end of a handler.

        @param result the run's final result, if any

        @throw AGUI-STATE-ERROR no run is in progress
    */
    runFinished(*auto result) {
        hash<AgUiRunFinishedEvent> event;
        {
            AutoLock al(lck);
            if (!run_started) {
                throw "AGUI-STATE-ERROR", "runFinished() called with no run in progress";
            }
            if (run_ended) {
                return;
            }
            run_ended = True;
            event = <AgUiRunFinishedEvent>{
                "type": AGUI_RunFinished,
                "timestamp": nowMs(),
                "threadId": thread_id,
                "runId": run_id,
                "result": result,
            };
        }
        emitEvent(event);
    }

    #! Emits @ref AGUI_RunError and ends the run
    /** @param message a human-readable description of the failure
        @param code an application-specific error code

        @throw AGUI-STATE-ERROR no run is in progress
    */
    runError(string message, *string code) {
        hash<AgUiRunErrorEvent> event;
        {
            AutoLock al(lck);
            if (!run_started) {
                throw "AGUI-STATE-ERROR", "runError() called with no run in progress";
            }
            if (run_ended) {
                return;
            }
            run_ended = True;
            event = <AgUiRunErrorEvent>{
                "type": AGUI_RunError,
                "timestamp": nowMs(),
                "message": message,
                "code": code,
            };
        }
        emitEvent(event);
    }

    #! Emits @ref AGUI_StepStarted
    /** @param step_name the step name
    */
    stepStarted(string step_name) {
        emitEvent(<AgUiStepStartedEvent>{
            "type": AGUI_StepStarted,
            "timestamp": nowMs(),
            "stepName": step_name,
        });
    }

    #! Emits @ref AGUI_StepFinished
    /** @param step_name the step name; must match the matching @ref stepStarted() call
    */
    stepFinished(string step_name) {
        emitEvent(<AgUiStepFinishedEvent>{
            "type": AGUI_StepFinished,
            "timestamp": nowMs(),
            "stepName": step_name,
        });
    }

    #! Emits @ref AGUI_TextMessageStart and returns the message ID
    /** @param role who is speaking; see @ref AgUiRole
        @param message_id the message ID; generated if not supplied

        @return the message ID, for the matching @ref textMessageContent() and
            @ref endTextMessage() calls
    */
    string startTextMessage(string role = AgUiRole::Assistant, *string message_id) {
        string mid = message_id ?? makeId("msg");
        emitEvent(<AgUiTextMessageStartEvent>{
            "type": AGUI_TextMessageStart,
            "timestamp": nowMs(),
            "messageId": mid,
            "role": role,
        });
        return mid;
    }

    #! Emits @ref AGUI_TextMessageContent
    /** @param message_id the message the chunk belongs to
        @param delta the text to append
    */
    textMessageContent(string message_id, string delta) {
        emitEvent(<AgUiTextMessageContentEvent>{
            "type": AGUI_TextMessageContent,
            "timestamp": nowMs(),
            "messageId": message_id,
            "delta": delta,
        });
    }

    #! Emits @ref AGUI_TextMessageEnd
    /** @param message_id the message that ended
    */
    endTextMessage(string message_id) {
        emitEvent(<AgUiTextMessageEndEvent>{
            "type": AGUI_TextMessageEnd,
            "timestamp": nowMs(),
            "messageId": message_id,
        });
    }

    #! Emits a complete text message as start / content / end
    /** @param text the message text
        @param role who is speaking
        @param message_id the message ID; generated if not supplied

        @return the message ID
    */
    string textMessage(string text, string role = AgUiRole::Assistant, *string message_id) {
        string mid = startTextMessage(role, message_id);
        textMessageContent(mid, text);
        endTextMessage(mid);
        return mid;
    }

    #! Emits @ref AGUI_ToolCallStart and returns the tool call ID
    /** @param tool_name the name of the tool being called
        @param tool_call_id the tool call ID; generated if not supplied
        @param parent_message_id the message the tool call belongs to, if any

        @return the tool call ID
    */
    string startToolCall(string tool_name, *string tool_call_id, *string parent_message_id) {
        string tid = tool_call_id ?? makeId("tool");
        emitEvent(<AgUiToolCallStartEvent>{
            "type": AGUI_ToolCallStart,
            "timestamp": nowMs(),
            "toolCallId": tid,
            "toolCallName": tool_name,
            "parentMessageId": parent_message_id,
        });
        return tid;
    }

    #! Emits @ref AGUI_ToolCallArgs with a chunk of the tool call's JSON arguments
    /** @param tool_call_id the tool call
        @param delta the argument text to append; concatenating every delta for a tool call
            must yield a complete JSON object
    */
    toolCallArgs(string tool_call_id, string delta) {
        emitEvent(<AgUiToolCallArgsEvent>{
            "type": AGUI_ToolCallArgs,
            "timestamp": nowMs(),
            "toolCallId": tool_call_id,
            "delta": delta,
        });
    }

    #! Emits the tool call's arguments as a single @ref AGUI_ToolCallArgs event
    /** Serializes \a args to JSON.  Use @ref toolCallArgs() instead when the arguments are
        produced incrementally and the front end should render them as they arrive.

        @param tool_call_id the tool call
        @param args the complete arguments
    */
    toolCallArgsComplete(string tool_call_id, auto args) {
        toolCallArgs(tool_call_id, make_json(args));
    }

    #! Emits @ref AGUI_ToolCallEnd
    /** @param tool_call_id the tool call whose arguments are complete
    */
    endToolCall(string tool_call_id) {
        emitEvent(<AgUiToolCallEndEvent>{
            "type": AGUI_ToolCallEnd,
            "timestamp": nowMs(),
            "toolCallId": tool_call_id,
        });
    }

    #! Emits @ref AGUI_ToolCallResult
    /** @param tool_call_id the tool call this result belongs to
        @param content the result content
        @param message_id the result message ID; generated if not supplied

        @return the result message ID
    */
    string toolCallResult(string tool_call_id, auto content, *string message_id) {
        string mid = message_id ?? makeId("msg");
        emitEvent(<AgUiToolCallResultEvent>{
            "type": AGUI_ToolCallResult,
            "timestamp": nowMs(),
            "messageId": mid,
            "toolCallId": tool_call_id,
            "content": content,
            "role": AgUiRole::Tool,
        });
        return mid;
    }

    #! Emits @ref AGUI_StateSnapshot and records the state as the client's new baseline
    /** @param state the complete state
    */
    stateSnapshot(auto state) {
        hash<AgUiStateSnapshotEvent> event;
        {
            AutoLock al(lck);
            last_state = state;
            have_state = True;
            event = <AgUiStateSnapshotEvent>{
                "type": AGUI_StateSnapshot,
                "timestamp": nowMs(),
                "snapshot": state,
            };
        }
        emitEvent(event);
    }

    #! Emits the difference between the client's state and \a state
    /** Computes the RFC 6902 patch from the state the client last saw to \a state and emits it
        as @ref AGUI_StateDelta, then records \a state as the new baseline.

        Nothing is emitted when the state is unchanged: an empty patch would cost a round trip
        and tell the client nothing.

        No snapshot has been sent yet on the first call, so there is nothing to diff against
        and a full @ref AGUI_StateSnapshot is emitted instead — a client cannot apply a patch
        to a document it does not have.

        @param state the new state

        @return @ref True if an event was emitted
    */
    bool stateDelta(auto state) {
        hash<auto> event;
        {
            AutoLock al(lck);
            if (!have_state) {
                last_state = state;
                have_state = True;
                event = <AgUiStateSnapshotEvent>{
                    "type": AGUI_StateSnapshot,
                    "timestamp": nowMs(),
                    "snapshot": state,
                };
            } else {
                list<hash<auto>> ops = json_patch_diff(last_state, state);
                if (!ops) {
                    return False;
                }
                last_state = state;
                event = <AgUiStateDeltaEvent>{
                    "type": AGUI_StateDelta,
                    "timestamp": nowMs(),
                    "delta": ops,
                };
            }
        }
        emitEvent(event);
        return True;
    }

    #! Emits @ref AGUI_MessagesSnapshot
    /** @param messages the complete message list
    */
    messagesSnapshot(list<auto> messages) {
        emitEvent(<AgUiMessagesSnapshotEvent>{
            "type": AGUI_MessagesSnapshot,
            "timestamp": nowMs(),
            "messages": messages,
        });
    }

    #! Emits @ref AGUI_ActivitySnapshot
    /** @param message_id the message the activity is attached to
        @param activity_type the kind of activity, e.g. \c "PLAN"
        @param content the activity content
        @param replace whether the content replaces the previous snapshot
    */
    activitySnapshot(string message_id, string activity_type, auto content,
            *bool replace) {
        emitEvent(<AgUiActivitySnapshotEvent>{
            "type": AGUI_ActivitySnapshot,
            "timestamp": nowMs(),
            "messageId": message_id,
            "activityType": activity_type,
            "content": content,
            "replace": replace,
        });
    }

    #! Emits @ref AGUI_ActivityDelta
    /** @param message_id the message the activity is attached to
        @param activity_type the kind of activity
        @param patch the RFC 6902 operations
    */
    activityDelta(string message_id, string activity_type, list<auto> patch) {
        emitEvent(<AgUiActivityDeltaEvent>{
            "type": AGUI_ActivityDelta,
            "timestamp": nowMs(),
            "messageId": message_id,
            "activityType": activity_type,
            "patch": patch,
        });
    }

    #! Emits @ref AGUI_ReasoningStart
    /** @param message_id the message the reasoning belongs to; generated if not supplied

        @return the message ID
    */
    string reasoningStart(*string message_id) {
        string mid = message_id ?? makeId("reasoning");
        emitEvent(<AgUiReasoningStartEvent>{
            "type": AGUI_ReasoningStart,
            "timestamp": nowMs(),
            "messageId": mid,
        });
        return mid;
    }

    #! Emits @ref AGUI_ReasoningMessageStart
    /** @param message_id the reasoning message
    */
    reasoningMessageStart(string message_id) {
        emitEvent(<AgUiReasoningMessageStartEvent>{
            "type": AGUI_ReasoningMessageStart,
            "timestamp": nowMs(),
            "messageId": message_id,
            "role": "reasoning",
        });
    }

    #! Emits @ref AGUI_ReasoningMessageContent
    /** @param message_id the reasoning message
        @param delta the reasoning text to append
    */
    reasoningMessageContent(string message_id, string delta) {
        emitEvent(<AgUiReasoningMessageContentEvent>{
            "type": AGUI_ReasoningMessageContent,
            "timestamp": nowMs(),
            "messageId": message_id,
            "delta": delta,
        });
    }

    #! Emits @ref AGUI_ReasoningMessageEnd
    /** @param message_id the reasoning message
    */
    reasoningMessageEnd(string message_id) {
        emitEvent(<AgUiReasoningMessageEndEvent>{
            "type": AGUI_ReasoningMessageEnd,
            "timestamp": nowMs(),
            "messageId": message_id,
        });
    }

    #! Emits @ref AGUI_ReasoningEnd
    /** @param message_id the message whose reasoning ended
    */
    reasoningEnd(string message_id) {
        emitEvent(<AgUiReasoningEndEvent>{
            "type": AGUI_ReasoningEnd,
            "timestamp": nowMs(),
            "messageId": message_id,
        });
    }

    #! Emits @ref AGUI_Raw
    /** @param event the provider event to pass through
        @param source where the event came from
    */
    raw(auto event, *string source) {
        emitEvent(<AgUiRawEvent>{
            "type": AGUI_Raw,
            "timestamp": nowMs(),
            "event": event,
            "source": source,
        });
    }

    #! Emits @ref AGUI_Custom
    /** The extension point for anything the protocol has no event for; a front end that does
        not recognize \a name ignores the event.

        @param name the application-defined event name
        @param value the event payload
    */
    custom(string name, *auto value) {
        emitEvent(<AgUiCustomEvent>{
            "type": AGUI_Custom,
            "timestamp": nowMs(),
            "name": name,
            "value": value,
        });
    }

    #! Formats and delivers one event
    /** All the typed emit methods funnel through here, so a subclass can log, filter or
        rewrite every event by overriding this one method.

        @param event the event to emit
    */
    emitEvent(hash<auto> event) {
        *code<nothing(string)> s;
        code<string(hash<auto>)> e;
        {
            AutoLock al(lck);
            s = sink;
            e = encoder;
        }
        if (!s) {
            return;
        }
        s(e(agui_strip_nothing(event)));
    }

    #! Formats an event as an SSE frame
    /** AG-UI names no SSE event type: every frame is a bare \c data: line carrying the JSON
        event, terminated by a blank line.  A client dispatches on the JSON \c type member, so
        adding an SSE \c event: field would be ignored at best.

        @param event the event

        @return the SSE frame
    */
    static string encodeSse(hash<auto> event) {
        return "data: " + make_json(event) + "\n\n";
    }

    #! Formats an event as a single line of JSON followed by a newline (NDJSON)
    /** For transports that frame messages themselves — a WebSocket, or a log file.

        @param event the event

        @return the JSON text plus a newline
    */
    static string encodeJsonLine(hash<auto> event) {
        return make_json(event) + "\n";
    }

    #! Returns the current time in epoch milliseconds
    private static int nowMs() {
        return (get_duration_microseconds(now_us() - 1970-01-01Z)) / 1000;
    }

    #! Generates an identifier unique within this emitter
    private string makeId(string prefix) {
        AutoLock al(lck);
        return makeIdLocked(prefix);
    }

    #! Generates an identifier; called with the lock held
    private string makeIdLocked(string prefix) {
        return sprintf("%s-%s-%d", prefix, get_random_string(8), ++id_seq);
    }
}
}
