# -*- mode: qore; indent-tabs-mode: nil -*-

/*  AbstractAgentStrategy.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see AbstractAgentStrategy
public namespace QoreAgentStrategies {

#! Context hash passed to agent strategies
public hashdecl AgentContext {
    #! The user message to process
    string message;
    #! Conversation history (list of ChatMessage hashes); if provided, these
    #! are prepended to the message list sent to the LLM
    *list<hash<QoreLlmUtils::ChatMessage>> history;
    #! Available tools (list of ToolDefinition hashes)
    *list<hash<QoreLlmUtils::ToolDefinition>> tools;
    #! Tool dispatch callable: auto sub (string tool_name, hash<auto> args)
    *code tool_dispatch;
    #! System prompt for the agent; default is a generic assistant prompt
    *string system_prompt;
    #! Additional user context (e.g., user_id, session_id)
    *hash<auto> user_context;
}

#! Result from an agent strategy execution
public hashdecl AgentResult {
    #! The final response text
    string response;
    #! Tool calls made during execution (list of {tool, args, result, *error} hashes)
    list<hash<auto>> tool_calls;
    #! Total tokens consumed
    int total_tokens;
    #! Number of LLM calls made
    int llm_calls;
    #! Strategy-specific metadata
    *hash<auto> metadata;
}

#! An event emitted during strategy execution for observability
/** Consumers (e.g., Qorus) can register a callback to receive these events
    for audit logging, UI updates, or debugging.
*/
public hashdecl AgentEvent {
    #! Event type: "strategy_started", "llm_called", "tool_dispatched",
    #! "tool_completed", "iteration_completed", "strategy_completed", "error"
    string event_type;
    #! Strategy name: "simple", "planning", "reflexion"
    string strategy;
    #! Current iteration number (for loop-based strategies)
    *int iteration;
    #! Tool name (for tool_dispatched/tool_completed events)
    *string tool_name;
    #! Tool arguments (for tool_dispatched events)
    *hash<auto> tool_args;
    #! Tool result (for tool_completed events)
    *auto tool_result;
    #! Error message (for error events)
    *string error;
    #! Additional metadata
    *hash<auto> metadata;
    #! When the event occurred
    date timestamp;
}

#! Abstract base class for agent execution strategies
/** Subclasses define how the agent's reasoning loop operates — how tools
    are dispatched, whether to plan first, whether to reflect on results, etc.

    All strategies support:
    - Token budget tracking with @ref AGENT-BUDGET-EXCEEDED enforcement
    - Event callbacks for observability (audit, logging, UI)
    - Multi-turn conversation via @ref AgentContext::history

    @par Subclassing
    Override the \c execute() method to implement your strategy. Use
    \c callChat() for multi-turn LLM calls with tool support, or
    \c callLlm() for simple single-shot completions.
*/
public class AbstractAgentStrategy {
    private {
        #! The LLM backend for making completion calls
        QoreLlmUtils::LlmBackend llm;
        #! Total token budget (0 = unlimited)
        int token_budget;
        #! Tokens consumed so far
        int tokens_consumed = 0;
        #! Optional event callback for observability
        *code event_callback;
    }

    #! Creates the strategy with an LLM backend
    /** @param llm_backend the LLM backend to use
        @param options strategy options:
            - \c token_budget (int): total token budget; default 0 (unlimited)
            - \c event_callback (code): callable receiving @ref AgentEvent hashes
    */
    constructor(QoreLlmUtils::LlmBackend llm_backend, *hash<auto> options) {
        llm = llm_backend;
        token_budget = options.token_budget ?? 0;
        event_callback = options.event_callback;
    }

    #! Executes the strategy
    /** @param ctx the agent context with message, tools, and dispatch callable
        @return an @ref AgentResult hash

        @throw AGENT-ERROR if execution fails
        @throw AGENT-BUDGET-EXCEEDED if the token budget is exhausted
    */
    abstract hash<AgentResult> execute(hash<AgentContext> ctx);

    #! Sets or clears the event callback
    /** @param cb callable receiving @ref AgentEvent hashes, or NOTHING to clear
    */
    setEventCallback(*code cb) {
        event_callback = cb;
    }

    #! Makes a multi-turn chat call with tool support and tracks token usage
    /** @param messages list of ChatMessage hashes
        @param tools optional list of ToolDefinition hashes
        @return a ChatResponse hash

        @throw AGENT-BUDGET-EXCEEDED if the budget would be exceeded
    */
    hash<QoreLlmUtils::ChatResponse> callChat(
            list<hash<QoreLlmUtils::ChatMessage>> messages,
            *list<hash<QoreLlmUtils::ToolDefinition>> tools) {
        checkBudget();
        hash<QoreLlmUtils::ChatResponse> result = llm.chat(messages, tools);
        int used = result.usage.total_tokens ?? 0;
        tokens_consumed += used;
        return result;
    }

    #! Makes a streaming multi-turn chat call, accumulating into a ChatResponse
    /** Bridge from streaming-aware strategies (e.g.
        @ref SimpleAgentStrategy::executeStream()) to
        @ref QoreLlmUtils::LlmBackend::chatStream().  Accumulates the per-chunk
        events into a final @ref ChatResponse so the strategy loop sees the
        same shape as @ref callChat() — the only difference is that each
        chunk is also forwarded to @a on_event for live consumption (e.g.,
        SSE forwarding to a chat client).

        Tool-call deltas (the OpenAI streaming convention of splitting
        each tool call across multiple chunks: partial function name in
        one, partial arguments JSON in subsequent chunks) are accumulated
        in-method.  The returned @ref ChatResponse carries the rebuilt
        @ref ChatToolCall list when @c finish_reason is \c tool_calls.

        @param messages list of @ref ChatMessage hashes for this turn
        @param on_event called once per @ref ChatStreamEvent; callers
            decide what to do with each event (content delta forwarding,
            tool-call delta surfacing, etc.).  May be @ref nothing to
            silence the live channel and use this purely as a buffering
            adapter
        @param tools optional list of @ref ToolDefinition hashes

        @return a @ref ChatResponse with @c content (accumulated text),
            @c tool_calls (rebuilt from streamed deltas, present when
            @c finish_reason is \c tool_calls), @c stop_reason, @c usage,
            and @c raw fields populated from the terminal chunk

        @throw AGENT-BUDGET-EXCEEDED if the budget would be exceeded

        @par Example: stream content deltas to a live UI while still
        accumulating the full @ref ChatResponse for the strategy loop

        @code{.py}
hash<ChatResponse> resp = callChatStream(messages,
    sub (hash<ChatStreamEvent> ev) {
        if (ev.delta.val()) {
            sse_queue.push(sprintf("data: %s\n\n",
                make_json({"delta": ev.delta})));
        }
    }, ctx.tools);
if (resp.stop_reason == "tool_use") {
    # Dispatch the rebuilt tool calls; the accumulator already merged
    # the multi-chunk OpenAI delta convention into resp.tool_calls
    dispatchTools(ctx, resp.tool_calls);
}
        @endcode
    */
    hash<QoreLlmUtils::ChatResponse> callChatStream(
            list<hash<QoreLlmUtils::ChatMessage>> messages,
            *code on_event,
            *list<hash<QoreLlmUtils::ToolDefinition>> tools) {
        checkBudget();

        # Accumulator state.  `tool_call_buf` is a hash keyed by
        # OpenAI's per-chunk `index` field (used to associate fragments
        # across delta chunks).  Each entry holds id, function name (may
        # arrive split across chunks), and the partial-arguments string
        # we concatenate as fragments arrive.
        string content_buf = "";
        hash<auto> tool_call_buf = {};
        list<int> tool_call_indices = ();
        *string finish_reason;
        *hash<auto> usage_acc;
        *string response_id;
        *string response_model;
        *hash<auto> last_raw;

        llm.chatStream(messages,
            sub (hash<QoreLlmUtils::ChatStreamEvent> event) {
                if (event.delta.val()) {
                    content_buf += event.delta;
                }
                *hash<auto> choice = event.raw.choices[0];
                if (choice.delta.tool_calls.typeCode() == NT_LIST) {
                    foreach hash<auto> tcd in (choice.delta.tool_calls) {
                        int idx = tcd.index ?? 0;
                        if (!exists tool_call_buf{idx}) {
                            tool_call_buf{idx} = {"args_buf": ""};
                            push tool_call_indices, idx;
                        }
                        if (tcd.id.val()) {
                            tool_call_buf{idx}.id = tcd.id;
                        }
                        if (tcd."function".name.val()) {
                            tool_call_buf{idx}.name =
                                (tool_call_buf{idx}.name ?? "")
                                + tcd."function".name;
                        }
                        if (tcd."function".arguments.val()) {
                            tool_call_buf{idx}.args_buf +=
                                tcd."function".arguments;
                        }
                    }
                }
                if (event.raw.id.val()) {
                    response_id = event.raw.id;
                }
                if (event.raw.model.val()) {
                    response_model = event.raw.model;
                }
                if (event.raw.usage.typeCode() == NT_HASH) {
                    usage_acc = event.raw.usage;
                }
                if (event.done) {
                    last_raw = event.raw;
                    if (choice.finish_reason.val()) {
                        finish_reason = choice.finish_reason;
                    }
                }
                if (on_event) {
                    on_event(event);
                }
            }, tools);

        # Map OpenAI finish_reason → QoreLlmUtils ChatResponse stop_reason
        string stop_reason;
        switch (finish_reason) {
            case "stop":
                stop_reason = "end_turn";
                break;
            case "tool_calls":
                stop_reason = "tool_use";
                break;
            case "length":
                stop_reason = "max_tokens";
                break;
            default:
                stop_reason = finish_reason ?? "end_turn";
        }

        *list<hash<QoreLlmUtils::ChatToolCall>> parsed_tool_calls;
        if (tool_call_buf) {
            parsed_tool_calls = ();
            foreach int idx in (tool_call_indices) {
                hash<auto> raw = tool_call_buf{idx};
                *hash<auto> args;
                if (raw.args_buf.val()) {
                    try {
                        args = parse_json(raw.args_buf);
                    } catch () {
                        # Tolerate malformed JSON: surface raw text via
                        # a `_raw` field so callers can debug provider
                        # output without the strategy crashing.
                        args = {"_raw": raw.args_buf};
                    }
                }
                push parsed_tool_calls, <QoreLlmUtils::ChatToolCall>{
                    "id": raw.id ?? sprintf("tc_%d", idx),
                    "name": raw.name ?? "",
                    "arguments": args,
                };
            }
        }

        hash<QoreLlmUtils::ChatResponse> result = <QoreLlmUtils::ChatResponse>{
            "id": response_id,
            "model": response_model,
            "stop_reason": stop_reason,
            "content": content_buf,
            "tool_calls": parsed_tool_calls,
            "usage": usage_acc ?? {},
            "raw": last_raw,
        };

        int used = result.usage.total_tokens ?? 0;
        tokens_consumed += used;
        return result;
    }

    #! Makes a simple LLM completion call and tracks token usage
    /** @param system_prompt the system prompt
        @param user_prompt the user message
        @param output_schema optional JSON schema for structured output
        @return an LlmCompletionResult hash

        @throw AGENT-BUDGET-EXCEEDED if the budget would be exceeded
    */
    hash<QoreLlmUtils::LlmCompletionResult> callLlm(string system_prompt,
            string user_prompt, *hash<auto> output_schema) {
        checkBudget();
        hash<QoreLlmUtils::LlmCompletionResult> result =
            llm.complete(system_prompt, user_prompt, output_schema);
        int used = result.usage.total_tokens ?? 0;
        tokens_consumed += used;
        return result;
    }

    #! Returns the total tokens consumed
    int getTokensConsumed() {
        return tokens_consumed;
    }

    #! Resets the token counter
    resetTokenCounter() {
        tokens_consumed = 0;
    }

    #! Dispatches a tool call via the context's tool_dispatch callable
    /** @param ctx the agent context
        @param tool_name name of the tool to call
        @param args arguments hash for the tool
        @return the tool's return value

        @throw AGENT-ERROR if no tool_dispatch callable is set
    */
    auto dispatchTool(hash<AgentContext> ctx, string tool_name, hash<auto> args) {
        if (!ctx.tool_dispatch) {
            throw "AGENT-ERROR",
                sprintf("no tool_dispatch callable provided for tool %y", tool_name);
        }
        return ctx.tool_dispatch(tool_name, args);
    }

    #! Emits an event to the callback (if set)
    /** Catches and silently ignores any exceptions from the callback to
        avoid disrupting the strategy execution loop.
    */
    emitEvent(hash<AgentEvent> event) {
        if (event_callback) {
            try {
                event_callback(event);
            } catch () {
                # Callback errors must not disrupt the strategy
            }
        }
    }

    #! Checks the token budget and throws if exceeded
    private checkBudget() {
        if (token_budget > 0 && tokens_consumed >= token_budget) {
            throw "AGENT-BUDGET-EXCEEDED",
                sprintf("token budget exhausted: %d/%d tokens consumed",
                    tokens_consumed, token_budget);
        }
    }

    #! Builds initial messages from an AgentContext
    /** If history is provided, uses it. Otherwise creates system + user messages.
        @param ctx the agent context
        @return list of ChatMessage hashes
    */
    list<hash<QoreLlmUtils::ChatMessage>> buildInitialMessages(hash<AgentContext> ctx) {
        if (ctx.history) {
            # Append user message to existing history
            list<hash<QoreLlmUtils::ChatMessage>> messages = ctx.history;
            push messages, <QoreLlmUtils::ChatMessage>{
                "role": "user",
                "content": ctx.message,
            };
            return messages;
        }

        # Build fresh conversation
        list<hash<QoreLlmUtils::ChatMessage>> messages = ();
        string sys = ctx.system_prompt ?? "You are a helpful assistant.";
        push messages, <QoreLlmUtils::ChatMessage>{
            "role": "system",
            "content": sys,
        };
        push messages, <QoreLlmUtils::ChatMessage>{
            "role": "user",
            "content": ctx.message,
        };
        return messages;
    }
}

} # namespace QoreAgentStrategies
