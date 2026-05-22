# -*- mode: qore; indent-tabs-mode: nil -*-

/*  SimpleAgentStrategy.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see SimpleAgentStrategy
public namespace QoreAgentStrategies {

#! Core tool-calling agent loop — the default strategy for AI endpoints
/** Implements the standard agent execution pattern:

    1. Send the user message (with conversation history) and tool definitions
       to the LLM
    2. If the LLM returns tool calls: dispatch them, add results to the
       conversation, and repeat from step 1
    3. If the LLM returns text: return it as the response
    4. Cap at \c max_iterations to prevent runaway loops

    This is the strategy that replaces hardcoded tool dispatch loops in AI
    endpoints.  It supports optional parallel tool dispatch when the LLM
    requests multiple tools at once.

    @par Example: Agent with tool calling
    @code{.py}
%modern
%requires QoreAgentStrategies

LlmBackend llm(<LlmBackendConfig>{"mode": "http", "url": "http://localhost:8080",
    "model": "gpt-4o"});

code dispatch = auto sub (string name, hash<auto> args) {
    if (name == "get_weather") {
        return {"temp": 22, "condition": "sunny"};
    }
    return {"error": "unknown tool"};
};

SimpleAgentStrategy agent(llm, {"max_iterations": 5});
hash<AgentResult> result = agent.execute(<AgentContext>{
    "message": "What's the weather in Prague?",
    "tools": (<ToolDefinition>{"name": "get_weather", "description": "Get weather",
        "parameters": {"type": "object", "properties": {"city": {"type": "string"}}}},),
    "tool_dispatch": dispatch,
});
printf("Response: %s\nTools called: %d\n", result.response, result.tool_calls.size());
    @endcode
*/
public class SimpleAgentStrategy inherits AbstractAgentStrategy {
    private {
        #! Maximum tool-calling iterations
        int max_iterations;
        #! Optional parallel tool dispatcher
        *ParallelToolDispatcher parallel_dispatcher;
    }

    #! Creates a SimpleAgentStrategy
    /** @param llm_backend the LLM backend to use
        @param options strategy options:
            - \c max_iterations (int): max tool-calling loop iterations; default 5
            - \c parallel_dispatch (bool): enable parallel tool dispatch; default False
            - \c max_concurrent (int): max concurrent tools when parallel; default 5
            - \c tool_timeout_ms (int): per-tool timeout in ms; default 30000
            - \c token_budget (int): total token budget; default 0 (unlimited)
            - \c event_callback (code): event callback for observability
    */
    constructor(QoreLlmUtils::LlmBackend llm_backend, *hash<auto> options)
            : AbstractAgentStrategy(llm_backend, options) {
        max_iterations = options.max_iterations ?? 5;
        if (options.parallel_dispatch) {
            parallel_dispatcher = new ParallelToolDispatcher({
                "max_concurrent": options.max_concurrent ?? 5,
                "tool_timeout_ms": options.tool_timeout_ms ?? 30000,
            });
        }
    }

    hash<AgentResult> execute(hash<AgentContext> ctx) {
        resetTokenCounter();
        list<hash<auto>> all_tool_calls = ();
        int llm_calls = 0;

        emitEvent(<AgentEvent>{
            "event_type": "strategy_started",
            "strategy": "simple",
            "timestamp": now_us(),
        });

        # Build initial messages from context (history + user message)
        list<hash<QoreLlmUtils::ChatMessage>> messages = buildInitialMessages(ctx);

        int iteration = 0;
        while (iteration < max_iterations) {
            ++iteration;

            # Call LLM with tools
            hash<QoreLlmUtils::ChatResponse> response = callChat(messages, ctx.tools);
            ++llm_calls;

            emitEvent(<AgentEvent>{
                "event_type": "llm_called",
                "strategy": "simple",
                "iteration": iteration,
                "timestamp": now_us(),
            });

            # If no tool calls requested, return the text response
            if (response.stop_reason != "tool_use" || !response.tool_calls) {
                emitEvent(<AgentEvent>{
                    "event_type": "strategy_completed",
                    "strategy": "simple",
                    "iteration": iteration,
                    "timestamp": now_us(),
                    "metadata": {"reason": "end_turn"},
                });
                return <AgentResult>{
                    "response": response.content ?? "",
                    "tool_calls": all_tool_calls,
                    "total_tokens": getTokensConsumed(),
                    "llm_calls": llm_calls,
                    "metadata": {
                        "strategy": "simple",
                        "iterations": iteration,
                    },
                };
            }

            # Add assistant message with tool_calls to conversation
            push messages, <QoreLlmUtils::ChatMessage>{
                "role": "assistant",
                "content": response.content,
                "tool_calls": response.tool_calls,
            };

            # Dispatch tools
            list<hash<auto>> dispatch_results = dispatchTools(ctx, response.tool_calls);

            # Add tool results to conversation and tracking
            foreach hash<auto> dr in (dispatch_results) {
                string result_content;
                if (dr.error.val()) {
                    result_content = dr.error;
                } else if (dr.result.typeCode() == NT_STRING) {
                    result_content = dr.result;
                } else {
%ifndef NoJson
                    result_content = make_json(dr.result ?? {});
%else
                    result_content = sprintf("%y", dr.result ?? {});
%endif
                }
                push messages, <QoreLlmUtils::ChatMessage>{
                    "role": "tool",
                    "content": result_content,
                    "tool_call_id": dr.id,
                    "name": dr.name,
                };
                push all_tool_calls, {
                    "tool": dr.name,
                    "args": dr.args ?? {},
                    "result": dr.result,
                    "error": dr.error,
                };
            }

            emitEvent(<AgentEvent>{
                "event_type": "iteration_completed",
                "strategy": "simple",
                "iteration": iteration,
                "timestamp": now_us(),
                "metadata": {"tool_count": dispatch_results.size()},
            });
        }

        # Max iterations reached — make one final call without tools for a text response
        hash<QoreLlmUtils::ChatResponse> final_response = callChat(messages);
        ++llm_calls;

        emitEvent(<AgentEvent>{
            "event_type": "strategy_completed",
            "strategy": "simple",
            "iteration": max_iterations,
            "timestamp": now_us(),
            "metadata": {"reason": "max_iterations"},
        });

        return <AgentResult>{
            "response": final_response.content
                ?? "Maximum iterations reached without a final response.",
            "tool_calls": all_tool_calls,
            "total_tokens": getTokensConsumed(),
            "llm_calls": llm_calls,
            "metadata": {
                "strategy": "simple",
                "iterations": max_iterations,
                "max_iterations_reached": True,
            },
        };
    }

    #! Streaming variant of @ref execute() that surfaces per-chunk events
    /** Mirrors the @ref execute() loop but uses
        @ref AbstractAgentStrategy::callChatStream() so each LLM iteration
        emits its content deltas, tool-call deltas, and per-iteration
        terminal event to @a on_event in real time.  Terminal \c done
        events from intermediate iterations are SUPPRESSED so the
        caller's wire-level SSE stream sees a continuous conversation
        across tool dispatches — a single terminal \c done is emitted
        after the loop exits.

        Tool-dispatch progress is surfaced via the @ref AgentEvent
        callback (@ref tool_dispatched / @ref tool_completed) so clients
        can render dispatch indicators between LLM bursts.

        @param ctx the agent context
        @param on_event called once per @ref ChatStreamEvent reaching the
            caller; suppression of intermediate terminals is already
            applied before @a on_event runs

        @return an @ref AgentResult with the same shape @ref execute()
            returns; \c metadata carries the per-iteration count and a
            \c streamed flag set to @ref True

        @par Example: drive a chat endpoint's SSE response from a
        streaming agent loop while still receiving the final accumulated
        @ref AgentResult for post-processing (audit logging, conversation
        persistence, token accounting)

        @code{.py}
SimpleAgentStrategy strat(backend, {"max_iterations": 5});
hash<AgentResult> res = strat.executeStream(<AgentContext>{
    "message": user_input,
    "tools": tool_defs,
    "tool_dispatch": tool_dispatcher,
}, sub (hash<ChatStreamEvent> ev) {
    if (ev.delta.val()) {
        sse_queue.push(sprintf("data: %s\n\n",
            make_json({"delta": ev.delta})));
    }
    if (ev.done) {
        sse_queue.push("data: [DONE]\n\n");
    }
});
audit.recordTurn(res);
        @endcode
    */
    hash<AgentResult> executeStream(hash<AgentContext> ctx, code on_event) {
        resetTokenCounter();
        list<hash<auto>> all_tool_calls = ();
        int llm_calls = 0;

        emitEvent(<AgentEvent>{
            "event_type": "strategy_started",
            "strategy": "simple",
            "timestamp": now_us(),
        });

        list<hash<QoreLlmUtils::ChatMessage>> messages = buildInitialMessages(ctx);

        int iteration = 0;
        # Suppress the upstream-emitted terminal `done` until the strategy
        # decides the conversation is actually complete (i.e. no more tool
        # calls).  The caller sees ONE terminal `done` at the end of the
        # final iteration.
        while (iteration < max_iterations) {
            ++iteration;

            int this_iteration = iteration;
            hash<QoreLlmUtils::ChatResponse> response = callChatStream(messages,
                sub (hash<QoreLlmUtils::ChatStreamEvent> event) {
                    # Strip terminal `done` from intermediate iterations.
                    # The loop emits its own final `done` after exit.
                    # An intermediate `done` always corresponds to an
                    # OpenAI `finish_reason` chunk; whether it carries
                    # `tool_calls` or `stop` is settled here by the
                    # strategy's loop guard below.  Surfacing it now
                    # would cause clients to terminate prematurely.
                    if (event.done) {
                        return;
                    }
                    on_event(event);
                }, ctx.tools);
            ++llm_calls;

            emitEvent(<AgentEvent>{
                "event_type": "llm_called",
                "strategy": "simple",
                "iteration": iteration,
                "timestamp": now_us(),
            });

            if (response.stop_reason != "tool_use" || !response.tool_calls) {
                # Final iteration — surface the terminal `done` to the
                # caller now.  Forward the upstream's terminal `raw`
                # chunk so callers that parse `event.raw.usage` /
                # `finish_reason` see the right shape.
                on_event(<QoreLlmUtils::ChatStreamEvent>{
                    "done": True,
                    "raw": response.raw,
                });
                emitEvent(<AgentEvent>{
                    "event_type": "strategy_completed",
                    "strategy": "simple",
                    "iteration": iteration,
                    "timestamp": now_us(),
                    "metadata": {"reason": "end_turn"},
                });
                return <AgentResult>{
                    "response": response.content ?? "",
                    "tool_calls": all_tool_calls,
                    "total_tokens": getTokensConsumed(),
                    "llm_calls": llm_calls,
                    "metadata": {
                        "strategy": "simple",
                        "iterations": iteration,
                        "streamed": True,
                    },
                };
            }

            # Tool dispatch round.  Append the assistant message that
            # requested the tool calls (same shape as the non-streaming
            # execute() loop) so the next LLM call has the full thread.
            push messages, <QoreLlmUtils::ChatMessage>{
                "role": "assistant",
                "content": response.content,
                "tool_calls": response.tool_calls,
            };

            list<hash<auto>> dispatch_results = dispatchTools(ctx,
                response.tool_calls);

            foreach hash<auto> dr in (dispatch_results) {
                string result_content;
                if (dr.error.val()) {
                    result_content = dr.error;
                } else if (dr.result.typeCode() == NT_STRING) {
                    result_content = dr.result;
                } else {
                    result_content = make_json(dr.result ?? {});
                }
                push messages, <QoreLlmUtils::ChatMessage>{
                    "role": "tool",
                    "content": result_content,
                    "tool_call_id": dr.id,
                    "name": dr.name,
                };
                push all_tool_calls, {
                    "tool": dr.name,
                    "args": dr.args ?? {},
                    "result": dr.result,
                    "error": dr.error,
                };
            }

            emitEvent(<AgentEvent>{
                "event_type": "iteration_completed",
                "strategy": "simple",
                "iteration": iteration,
                "timestamp": now_us(),
                "metadata": {"tool_count": dispatch_results.size()},
            });
        }

        # Max iterations exhausted — make a final non-streaming call for a
        # text response (same fallback execute() does).  Surface the final
        # text as a single content delta + terminal `done` so the caller
        # still sees a coherent stream.
        hash<QoreLlmUtils::ChatResponse> final_response = callChat(messages);
        ++llm_calls;
        if (final_response.content.val()) {
            on_event(<QoreLlmUtils::ChatStreamEvent>{
                "delta": final_response.content,
                "done": False,
            });
        }
        on_event(<QoreLlmUtils::ChatStreamEvent>{
            "done": True,
            "raw": final_response.raw,
        });

        emitEvent(<AgentEvent>{
            "event_type": "strategy_completed",
            "strategy": "simple",
            "iteration": max_iterations,
            "timestamp": now_us(),
            "metadata": {"reason": "max_iterations"},
        });

        return <AgentResult>{
            "response": final_response.content
                ?? "Maximum iterations reached without a final response.",
            "tool_calls": all_tool_calls,
            "total_tokens": getTokensConsumed(),
            "llm_calls": llm_calls,
            "metadata": {
                "strategy": "simple",
                "iterations": max_iterations,
                "max_iterations_reached": True,
                "streamed": True,
            },
        };
    }

    #! Dispatches tool calls (parallel or sequential)
    private list<hash<auto>> dispatchTools(hash<AgentContext> ctx,
            list<hash<QoreLlmUtils::ChatToolCall>> tool_calls) {
        if (parallel_dispatcher && tool_calls.size() > 1 && ctx.tool_dispatch) {
            # Build dispatch list
            list<hash<auto>> tc_list = ();
            foreach hash<QoreLlmUtils::ChatToolCall> tc in (tool_calls) {
                push tc_list, {"name": tc.name, "args": tc.arguments ?? {}};
            }
            list<hash<auto>> results =
                parallel_dispatcher.dispatchToolsParallel(tc_list, ctx.tool_dispatch);

            # Add tool call IDs to results
            for (int i = 0; i < results.size(); ++i) {
                results[i].id = tool_calls[i].id;
            }
            return results;
        }

        # Sequential dispatch
        list<hash<auto>> results = ();
        foreach hash<QoreLlmUtils::ChatToolCall> tc in (tool_calls) {
            emitEvent(<AgentEvent>{
                "event_type": "tool_dispatched",
                "strategy": "simple",
                "tool_name": tc.name,
                "tool_args": tc.arguments,
                "timestamp": now_us(),
            });

            try {
                auto tool_result = dispatchTool(ctx, tc.name, tc.arguments ?? {});
                push results, {
                    "id": tc.id,
                    "name": tc.name,
                    "args": tc.arguments ?? {},
                    "result": tool_result,
                };
                emitEvent(<AgentEvent>{
                    "event_type": "tool_completed",
                    "strategy": "simple",
                    "tool_name": tc.name,
                    "tool_result": tool_result,
                    "timestamp": now_us(),
                });
            } catch (hash<ExceptionInfo> ex) {
                string err = sprintf("%s: %s", ex.err, ex.desc);
                push results, {
                    "id": tc.id,
                    "name": tc.name,
                    "args": tc.arguments ?? {},
                    "error": err,
                };
                emitEvent(<AgentEvent>{
                    "event_type": "error",
                    "strategy": "simple",
                    "tool_name": tc.name,
                    "error": err,
                    "timestamp": now_us(),
                });
            }
        }
        return results;
    }
}

} # namespace QoreAgentStrategies
