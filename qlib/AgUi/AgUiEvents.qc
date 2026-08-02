# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AgUiEvents.qc AG-UI protocol event types

/*  AgUiEvents.qc Copyright 2026 Qore Technologies, s.r.o.

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
/** @defgroup agui_event_types AG-UI Event Types
    The \c type discriminator of every AG-UI event.

    @note the wire values are \c SCREAMING_SNAKE_CASE.  The protocol documentation names the
    event *classes* in PascalCase (\c RunStarted, \c StateDelta), which is not what goes on the
    wire — a peer sent \c "RunStarted" rejects it as an unknown event.
*/
#/@{
#! A run has started
public const AGUI_RunStarted = "RUN_STARTED";

#! A run has finished successfully
public const AGUI_RunFinished = "RUN_FINISHED";

#! A run has failed
public const AGUI_RunError = "RUN_ERROR";

#! A named step within a run has started
public const AGUI_StepStarted = "STEP_STARTED";

#! A named step within a run has finished
public const AGUI_StepFinished = "STEP_FINISHED";

#! A text message has started
public const AGUI_TextMessageStart = "TEXT_MESSAGE_START";

#! A chunk of text message content
public const AGUI_TextMessageContent = "TEXT_MESSAGE_CONTENT";

#! A text message has ended
public const AGUI_TextMessageEnd = "TEXT_MESSAGE_END";

#! A tool call has started
public const AGUI_ToolCallStart = "TOOL_CALL_START";

#! A chunk of the tool call's JSON arguments
public const AGUI_ToolCallArgs = "TOOL_CALL_ARGS";

#! A tool call's arguments are complete
public const AGUI_ToolCallEnd = "TOOL_CALL_END";

#! The result of a tool call
public const AGUI_ToolCallResult = "TOOL_CALL_RESULT";

#! A complete snapshot of the shared state
public const AGUI_StateSnapshot = "STATE_SNAPSHOT";

#! An RFC 6902 JSON Patch against the last state snapshot
public const AGUI_StateDelta = "STATE_DELTA";

#! A complete snapshot of the conversation messages
public const AGUI_MessagesSnapshot = "MESSAGES_SNAPSHOT";

#! A complete snapshot of an activity (plan, progress list, etc.)
public const AGUI_ActivitySnapshot = "ACTIVITY_SNAPSHOT";

#! An RFC 6902 JSON Patch against the last activity snapshot
public const AGUI_ActivityDelta = "ACTIVITY_DELTA";

#! Reasoning output has started
public const AGUI_ReasoningStart = "REASONING_START";

#! A reasoning message has started
public const AGUI_ReasoningMessageStart = "REASONING_MESSAGE_START";

#! A chunk of reasoning message content
public const AGUI_ReasoningMessageContent = "REASONING_MESSAGE_CONTENT";

#! A reasoning message has ended
public const AGUI_ReasoningMessageEnd = "REASONING_MESSAGE_END";

#! Reasoning output has ended
public const AGUI_ReasoningEnd = "REASONING_END";

#! A provider-specific event passed through unchanged
public const AGUI_Raw = "RAW";

#! An application-specific event with a name and a free-form value
public const AGUI_Custom = "CUSTOM";
#/@}

#! Every AG-UI event type this module emits or recognizes
public const AgUiEventTypes = (
    AGUI_RunStarted,
    AGUI_RunFinished,
    AGUI_RunError,
    AGUI_StepStarted,
    AGUI_StepFinished,
    AGUI_TextMessageStart,
    AGUI_TextMessageContent,
    AGUI_TextMessageEnd,
    AGUI_ToolCallStart,
    AGUI_ToolCallArgs,
    AGUI_ToolCallEnd,
    AGUI_ToolCallResult,
    AGUI_StateSnapshot,
    AGUI_StateDelta,
    AGUI_MessagesSnapshot,
    AGUI_ActivitySnapshot,
    AGUI_ActivityDelta,
    AGUI_ReasoningStart,
    AGUI_ReasoningMessageStart,
    AGUI_ReasoningMessageContent,
    AGUI_ReasoningMessageEnd,
    AGUI_ReasoningEnd,
    AGUI_Raw,
    AGUI_Custom,
);

#! Message roles used in AG-UI text and reasoning messages
public enum AgUiRole : string {
    #! The end user
    User = "user",
    #! The agent
    Assistant = "assistant",
    #! The system prompt
    System = "system",
    #! The developer
    Developer = "developer",
    #! A tool result message
    Tool = "tool",
}

#! Base members shared by every AG-UI event
/** Every concrete event hashdecl inherits this one, so a caller can accept
    \c hash<AgUiEvent> and discriminate on @ref AgUiEvent::type "type".
*/
public hashdecl AgUiEvent {
    #! The event type; see @ref agui_event_types
    string type;

    #! Epoch milliseconds when the event was produced
    /** Optional in the protocol; @ref AgUi::AgUiEmitter "AgUiEmitter" always sets it.
    */
    *int timestamp;

    #! The provider event this one was derived from, for debugging
    *auto rawEvent;
}

#! A run has started
public hashdecl AgUiRunStartedEvent inherits AgUiEvent {
    #! The conversation thread this run belongs to
    string threadId;

    #! The run identifier
    string runId;
}

#! A run has finished successfully
public hashdecl AgUiRunFinishedEvent inherits AgUiEvent {
    #! The conversation thread this run belongs to
    string threadId;

    #! The run identifier
    string runId;

    #! The run's final result, if any
    *auto result;
}

#! A run has failed
/** A run emits either @ref AgUiRunFinishedEvent or this event, never both.
*/
public hashdecl AgUiRunErrorEvent inherits AgUiEvent {
    #! A human-readable description of the failure
    string message;

    #! An application-specific error code
    *string code;
}

#! A named step within a run has started
public hashdecl AgUiStepStartedEvent inherits AgUiEvent {
    #! The step name; must match the name in the matching @ref AgUiStepFinishedEvent
    string stepName;
}

#! A named step within a run has finished
public hashdecl AgUiStepFinishedEvent inherits AgUiEvent {
    #! The step name
    string stepName;
}

#! A text message has started
public hashdecl AgUiTextMessageStartEvent inherits AgUiEvent {
    #! Identifies the message across its start / content / end events
    string messageId;

    #! Who is speaking; see @ref AgUiRole
    string role;
}

#! A chunk of text message content
public hashdecl AgUiTextMessageContentEvent inherits AgUiEvent {
    #! The message this chunk belongs to
    string messageId;

    #! The text to append to the message
    string delta;
}

#! A text message has ended
public hashdecl AgUiTextMessageEndEvent inherits AgUiEvent {
    #! The message that ended
    string messageId;
}

#! A tool call has started
public hashdecl AgUiToolCallStartEvent inherits AgUiEvent {
    #! Identifies the tool call across its start / args / end / result events
    string toolCallId;

    #! The name of the tool being called
    string toolCallName;

    #! The message the tool call belongs to, if any
    *string parentMessageId;
}

#! A chunk of the tool call's JSON arguments
/** Arguments stream as text so that a front end can render them as they are produced;
    concatenating every delta for a tool call yields the complete JSON argument object.
*/
public hashdecl AgUiToolCallArgsEvent inherits AgUiEvent {
    #! The tool call these arguments belong to
    string toolCallId;

    #! The argument text to append
    string delta;
}

#! A tool call's arguments are complete
public hashdecl AgUiToolCallEndEvent inherits AgUiEvent {
    #! The tool call whose arguments are complete
    string toolCallId;

    #! The tool call's result content, when it is known at the same time
    *auto content;
}

#! The result of a tool call
public hashdecl AgUiToolCallResultEvent inherits AgUiEvent {
    #! The identifier of the tool result message
    string messageId;

    #! The tool call this result belongs to
    string toolCallId;

    #! The result content
    auto content;

    #! The role of the result message; defaults to @ref AgUiRole::Tool "tool"
    *string role;
}

#! A complete snapshot of the shared state
/** Sent at the start of a run and whenever the deltas would be larger than the state itself.
    A client replaces its state with the snapshot and applies subsequent
    @ref AgUiStateDeltaEvent "deltas" on top of it.
*/
public hashdecl AgUiStateSnapshotEvent inherits AgUiEvent {
    #! The complete state
    auto snapshot;
}

#! An RFC 6902 JSON Patch against the last state snapshot
public hashdecl AgUiStateDeltaEvent inherits AgUiEvent {
    #! The JSON Patch operations, in the order they must be applied
    list<auto> delta;
}

#! A complete snapshot of the conversation messages
public hashdecl AgUiMessagesSnapshotEvent inherits AgUiEvent {
    #! The messages
    list<auto> messages;
}

#! A complete snapshot of an activity
public hashdecl AgUiActivitySnapshotEvent inherits AgUiEvent {
    #! The message the activity is attached to
    string messageId;

    #! The kind of activity, e.g. \c "PLAN"
    string activityType;

    #! The activity content
    auto content;

    #! Whether the content replaces the previous snapshot rather than extending it
    *bool replace;
}

#! An RFC 6902 JSON Patch against the last activity snapshot
public hashdecl AgUiActivityDeltaEvent inherits AgUiEvent {
    #! The message the activity is attached to
    string messageId;

    #! The kind of activity
    string activityType;

    #! The JSON Patch operations, in the order they must be applied
    list<auto> patch;
}

#! Reasoning output has started
public hashdecl AgUiReasoningStartEvent inherits AgUiEvent {
    #! The message the reasoning belongs to
    string messageId;
}

#! A reasoning message has started
public hashdecl AgUiReasoningMessageStartEvent inherits AgUiEvent {
    #! The reasoning message identifier
    string messageId;

    #! The role; always \c "reasoning"
    string role = "reasoning";
}

#! A chunk of reasoning message content
public hashdecl AgUiReasoningMessageContentEvent inherits AgUiEvent {
    #! The reasoning message this chunk belongs to
    string messageId;

    #! The reasoning text to append
    string delta;
}

#! A reasoning message has ended
public hashdecl AgUiReasoningMessageEndEvent inherits AgUiEvent {
    #! The reasoning message that ended
    string messageId;
}

#! Reasoning output has ended
public hashdecl AgUiReasoningEndEvent inherits AgUiEvent {
    #! The message whose reasoning ended
    string messageId;
}

#! A provider-specific event passed through unchanged
public hashdecl AgUiRawEvent inherits AgUiEvent {
    #! The provider event
    auto event;

    #! Where the event came from
    *string source;
}

#! An application-specific event with a name and a free-form value
/** The extension point for anything the protocol has no event for; a front end that does not
    recognize \a name ignores the event.
*/
public hashdecl AgUiCustomEvent inherits AgUiEvent {
    #! The application-defined event name
    string name;

    #! The event payload
    *auto value;
}
}
