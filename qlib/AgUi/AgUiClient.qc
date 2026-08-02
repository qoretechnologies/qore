# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AgUiClient.qc AG-UI protocol event stream consumer

/*  AgUiClient.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The state an @ref AgUiClient has reconstructed from an AG-UI event stream
public hashdecl AgUiRunState {
    #! The conversation thread, once @ref AGUI_RunStarted has been seen
    *string threadId;

    #! The run identifier, once @ref AGUI_RunStarted has been seen
    *string runId;

    #! Whether the run has ended (successfully or not)
    bool finished = False;

    #! The failure, if the run ended with @ref AGUI_RunError
    *hash<auto> error;

    #! The run result, if the run ended with a result
    *auto result;

    #! The shared state, rebuilt from snapshots and deltas
    auto state;

    #! Completed and in-progress text messages, keyed by message ID
    /** Each value has \c role, \c text and \c complete members.
    */
    hash<string, hash<auto>> messages = {};

    #! The order in which message IDs were first seen
    list<string> messageOrder = ();

    #! Tool calls, keyed by tool call ID
    /** Each value has \c name, \c args (the concatenated argument text), \c complete and,
        once a result arrives, \c result.
    */
    hash<string, hash<auto>> toolCalls = {};

    #! The order in which tool call IDs were first seen
    list<string> toolCallOrder = ();

    #! Step names that have started but not finished, in order
    list<string> activeSteps = ();
}

#! Consumes an AG-UI event stream into typed events and reconstructed state
/** The client parses AG-UI frames, applies @ref AGUI_StateDelta patches on top of the last
    @ref AGUI_StateSnapshot, accumulates streamed text messages and tool call arguments, and
    reports each event to an optional callback.

    Like @ref AgUiEmitter it owns no socket: the caller feeds it bytes or parsed events from
    whatever transport it already has.

    @par Forward compatibility
    Unknown event types and unknown members within a known event are preserved and reported,
    never rejected: AG-UI ships SDK releases rather than numbered protocol revisions, so a
    peer using a newer release will send events this module has never heard of.  Only a frame
    that is not valid JSON, or whose \c type is missing or not a string, is an error.

    @par Example:
    @code{.py}
AgUiClient client(sub (hash<auto> event) {
    printf("%s\n", event.type);
});
client.feed(sse_bytes);
hash<AgUiRunState> st = client.getRunState();
printf("assistant said: %s\n", client.getMessageText(st.messageOrder[0]));
    @endcode
*/
public class AgUiClient {
    private {
        #! Called with each parsed event
        *code<nothing(hash<auto>)> callback;

        #! Bytes received but not yet forming a complete frame
        string buf = "";

        #! The reconstructed run state
        hash<AgUiRunState> run_state();

        #! Set once a state snapshot has been received
        /** Distinguishes "the server sent an empty state" from "the server has sent no state",
            which the state value alone cannot.
        */
        bool have_state = False;

        #! Every event seen, in order, when recording is enabled
        list<hash<auto>> events = ();

        #! Whether to record every event in @ref events
        bool record;

        #! Protects the client's mutable state
        Mutex lck();
    }

    #! Creates the client
    /** @param callback called with each parsed event, in stream order
        @param record whether to keep every event for later retrieval with @ref getEvents();
            off by default, since a long-lived stream would grow without bound
    */
    constructor(*code<nothing(hash<auto>)> callback, bool record = False) {
        self.callback = callback;
        self.record = record;
    }

    #! Feeds raw SSE bytes to the client
    /** Partial frames are buffered until the terminating blank line arrives, so the caller can
        pass whatever the transport delivered without aligning to frame boundaries.

        @param data the bytes received

        @return the events parsed from the data, in order

        @throw AGUI-PARSE-ERROR a frame is not valid JSON or is not a valid AG-UI event
    */
    list<hash<auto>> feed(string data) {
        list<string> frames;
        {
            AutoLock al(lck);
            buf += data;
            frames = takeCompleteFramesLocked();
        }
        list<hash<auto>> rv = ();
        foreach string frame in (frames) {
            *hash<auto> event = parseSseFrame(frame);
            if (event) {
                rv += processEvent(event);
            }
        }
        return rv;
    }

    #! Feeds one already-parsed event to the client
    /** @param event the event

        @return the event

        @throw AGUI-PARSE-ERROR the event has no string \c type member
    */
    hash<auto> processEvent(hash<auto> event) {
        if (event.type.typeCode() != NT_STRING) {
            throw "AGUI-PARSE-ERROR", sprintf("AG-UI event %y has no string \"type\" member",
                event);
        }
        *code<nothing(hash<auto>)> cb;
        {
            AutoLock al(lck);
            applyEventLocked(event);
            if (record) {
                events += event;
            }
            cb = callback;
        }
        if (cb) {
            cb(event);
        }
        return event;
    }

    #! Returns the reconstructed run state
    hash<AgUiRunState> getRunState() {
        AutoLock al(lck);
        return run_state;
    }

    #! Returns the shared state as rebuilt from snapshots and deltas
    auto getState() {
        AutoLock al(lck);
        return run_state.state;
    }

    #! Returns the accumulated text of a message
    /** @param message_id the message ID

        @return the text, or @ref nothing if the message was never seen
    */
    *string getMessageText(string message_id) {
        AutoLock al(lck);
        return run_state.messages{message_id}.text;
    }

    #! Returns every event seen so far
    /** Empty unless the client was created with recording enabled.
    */
    list<hash<auto>> getEvents() {
        AutoLock al(lck);
        return events;
    }

    #! Returns @ref True if the run has ended
    bool isFinished() {
        AutoLock al(lck);
        return run_state.finished;
    }

    #! Discards buffered bytes and resets the reconstructed state
    reset() {
        AutoLock al(lck);
        buf = "";
        run_state = new hash<AgUiRunState>();
        have_state = False;
        events = ();
    }

    #! Parses one SSE frame into an event
    /** @param frame the frame text, without its terminating blank line

        @return the event, or @ref nothing for a frame that carries no data (a comment or
            keep-alive)

        @throw AGUI-PARSE-ERROR the frame's data is not valid JSON, or is not a JSON object
    */
    static *hash<auto> parseSseFrame(string frame) {
        list<string> data_lines = ();
        foreach string line in (frame.split("\n")) {
            # tolerate CRLF line endings
            if (line.val() && line[line.length() - 1] == "\r") {
                line = line.substr(0, line.length() - 1);
            }
            if (!line.val() || line[0] == ":") {
                # comment / keep-alive
                continue;
            }
            int colon = line.find(":");
            if (colon == -1) {
                continue;
            }
            string field = line.substr(0, colon);
            if (field != "data") {
                # AG-UI sets no event: or id: field, but a proxy may; ignore rather than fail
                continue;
            }
            string value = line.substr(colon + 1);
            # a single leading space after the colon is part of the framing, not the data
            if (value.val() && value[0] == " ") {
                value = value.substr(1);
            }
            data_lines += value;
        }
        if (!data_lines) {
            return;
        }
        string json = data_lines.join("\n");
        if (!json.val()) {
            return;
        }
        auto v;
        try {
            v = parse_json(json);
        } catch (hash<ExceptionInfo> ex) {
            throw "AGUI-PARSE-ERROR", sprintf("AG-UI event frame is not valid JSON: %s: %s",
                ex.err, ex.desc);
        }
        if (v.typeCode() != NT_HASH) {
            throw "AGUI-PARSE-ERROR", sprintf("AG-UI event frame decoded to type %y; expecting "
                "a JSON object", v.fullType());
        }
        return v;
    }

    #! Removes and returns every complete frame in the buffer; called with the lock held
    private list<string> takeCompleteFramesLocked() {
        list<string> rv = ();
        while (True) {
            int pos = buf.find("\n\n");
            int seplen = 2;
            int crpos = buf.find("\r\n\r\n");
            if (crpos != -1 && (pos == -1 || crpos < pos)) {
                pos = crpos;
                seplen = 4;
            }
            if (pos == -1) {
                break;
            }
            rv += buf.substr(0, pos);
            buf = buf.substr(pos + seplen);
        }
        return rv;
    }

    #! Folds one event into the reconstructed state; called with the lock held
    private applyEventLocked(hash<auto> event) {
        switch (event.type) {
            case AGUI_RunStarted: {
                run_state.threadId = event.threadId;
                run_state.runId = event.runId;
                run_state.finished = False;
                remove run_state.error;
                break;
            }
            case AGUI_RunFinished: {
                run_state.finished = True;
                run_state.result = event.result;
                break;
            }
            case AGUI_RunError: {
                run_state.finished = True;
                run_state.error = {
                    "message": event.message,
                    "code": event.code,
                };
                break;
            }
            case AGUI_StepStarted: {
                run_state.activeSteps += event.stepName;
                break;
            }
            case AGUI_StepFinished: {
                # remove the first matching entry: steps may nest, and a finished inner step
                # must not remove the outer step of the same name
                foreach string step in (run_state.activeSteps) {
                    if (step == event.stepName) {
                        splice run_state.activeSteps, $#, 1;
                        break;
                    }
                }
                break;
            }
            case AGUI_TextMessageStart: {
                startMessageLocked(event.messageId, event.role ?? AgUiRole::Assistant);
                break;
            }
            case AGUI_TextMessageContent: {
                if (!run_state.messages.hasKey(event.messageId)) {
                    # tolerate a stream that starts mid-message (a resumed connection)
                    startMessageLocked(event.messageId, AgUiRole::Assistant);
                }
                run_state.messages{event.messageId}.text += event.delta ?? "";
                break;
            }
            case AGUI_TextMessageEnd: {
                if (run_state.messages.hasKey(event.messageId)) {
                    run_state.messages{event.messageId}.complete = True;
                }
                break;
            }
            case AGUI_ToolCallStart: {
                if (!run_state.toolCalls.hasKey(event.toolCallId)) {
                    run_state.toolCallOrder += event.toolCallId;
                }
                run_state.toolCalls{event.toolCallId} = {
                    "name": event.toolCallName,
                    "parentMessageId": event.parentMessageId,
                    "args": "",
                    "complete": False,
                };
                break;
            }
            case AGUI_ToolCallArgs: {
                if (!run_state.toolCalls.hasKey(event.toolCallId)) {
                    run_state.toolCallOrder += event.toolCallId;
                    run_state.toolCalls{event.toolCallId} = {"args": "", "complete": False};
                }
                run_state.toolCalls{event.toolCallId}.args += event.delta ?? "";
                break;
            }
            case AGUI_ToolCallEnd: {
                if (run_state.toolCalls.hasKey(event.toolCallId)) {
                    run_state.toolCalls{event.toolCallId}.complete = True;
                }
                break;
            }
            case AGUI_ToolCallResult: {
                if (!run_state.toolCalls.hasKey(event.toolCallId)) {
                    run_state.toolCallOrder += event.toolCallId;
                    run_state.toolCalls{event.toolCallId} = {"args": "", "complete": True};
                }
                run_state.toolCalls{event.toolCallId}.result = event.content;
                break;
            }
            case AGUI_StateSnapshot: {
                run_state.state = event.snapshot;
                have_state = True;
                break;
            }
            case AGUI_StateDelta: {
                if (event.delta.typeCode() != NT_LIST) {
                    throw "AGUI-PARSE-ERROR", sprintf("%s event has \"delta\" of type %y; "
                        "expecting a list of RFC 6902 operations", AGUI_StateDelta,
                        event.delta.fullType());
                }
                if (!have_state) {
                    # a patch against a document the client does not have cannot be applied,
                    # and guessing would leave the client silently out of sync with the server
                    throw "AGUI-STATE-ERROR", sprintf("received a %s event before any %s "
                        "event; the client has no state to apply the patch to", AGUI_StateDelta,
                        AGUI_StateSnapshot);
                }
                run_state.state = json_patch_apply(run_state.state, event.delta);
                break;
            }
        }
    }

    #! Starts tracking a message; called with the lock held
    private startMessageLocked(auto message_id, auto role) {
        string mid = message_id.toString();
        if (!run_state.messages.hasKey(mid)) {
            run_state.messageOrder += mid;
        }
        run_state.messages{mid} = {
            "role": role,
            "text": "",
            "complete": False,
        };
    }
}
}
