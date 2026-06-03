# -*- mode: qore; indent-tabs-mode: nil -*-

/*  LlmTelemetry.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace QoreLlmUtils {

#! Provider-neutral helpers for LLM observability
/** The helpers in this class deliberately avoid raw prompt / completion
    capture.  They produce safe summaries and normalized usage counters that
    applications can persist, price, filter, or export according to their own
    security policy.

    @note This class is policy-free: it does not calculate costs, make access
        control decisions, or persist telemetry.

    @par Example
    @code{.py}
hash<LlmTokenUsage> usage = LlmTelemetry::normalizeUsage(provider_usage);
    @endcode
*/
public class LlmTelemetry {
    #! Normalizes provider usage counters into a stable token accounting shape
    /** @param raw provider-specific usage hash or @ref NOTHING

        @return normalized token counters
    */
    static hash<LlmTokenUsage> normalizeUsage(auto raw) {
        if (raw.typeCode() != NT_HASH) {
            return <LlmTokenUsage>{};
        }
        hash<auto> usage = cast<hash>(raw);
        int input_tokens = firstInt(usage, (
            "input_tokens", "prompt_tokens", "inputTokens",
            "promptTokenCount", "prompt_token_count", "tokens_in"));
        int output_tokens = firstInt(usage, (
            "output_tokens", "completion_tokens", "outputTokens",
            "candidatesTokenCount", "completion_token_count",
            "tokens_out"));
        int cached_input_tokens = firstInt(usage, (
            "cached_input_tokens", "cache_read_input_tokens",
            "cachedInputTokens", "cacheReadInputTokens",
            "cached_content_token_count", "cachedContentTokenCount"));
        int cache_creation_input_tokens = firstInt(usage, (
            "cache_creation_input_tokens", "cache_write_input_tokens",
            "cacheCreationInputTokens", "cacheWriteInputTokens"));
        int reasoning_tokens = firstInt(usage, (
            "reasoning_tokens", "thinking_tokens", "reasoningTokens",
            "thoughtsTokenCount"));
        int embedding_tokens = firstInt(usage, (
            "embedding_tokens", "embeddingTokens", "total_tokens",
            "totalTokens"));
        int reranker_tokens = firstInt(usage, (
            "reranker_tokens", "rerank_tokens", "rerankerTokens",
            "rerankTokens"));

        if (usage.prompt_tokens_details.typeCode() == NT_HASH) {
            hash<auto> details = cast<hash>(usage.prompt_tokens_details);
            cached_input_tokens = max(cached_input_tokens,
                firstInt(details, ("cached_tokens", "cachedTokens")));
        }
        if (usage.input_token_details.typeCode() == NT_HASH) {
            hash<auto> details =
                cast<hash>(usage.input_token_details);
            cached_input_tokens = max(cached_input_tokens,
                firstInt(details, ("cache_read", "cache_read_input_tokens",
                    "cached_tokens")));
            cache_creation_input_tokens = max(cache_creation_input_tokens,
                firstInt(details, ("cache_creation",
                    "cache_creation_input_tokens", "cache_write")));
        }
        if (usage.completion_tokens_details.typeCode() == NT_HASH) {
            hash<auto> details =
                cast<hash>(usage.completion_tokens_details);
            reasoning_tokens = max(reasoning_tokens,
                firstInt(details, ("reasoning_tokens", "thinking_tokens")));
        }
        int total_tokens = firstInt(usage, (
            "total_tokens", "totalTokens", "totalTokenCount"));
        if (!total_tokens) {
            total_tokens = input_tokens + output_tokens + reasoning_tokens;
        }

        return <LlmTokenUsage>{
            "input_tokens": input_tokens,
            "output_tokens": output_tokens,
            "total_tokens": total_tokens,
            "cached_input_tokens": cached_input_tokens,
            "cache_creation_input_tokens": cache_creation_input_tokens,
            "reasoning_tokens": reasoning_tokens,
            "embedding_tokens": embedding_tokens,
            "reranker_tokens": reranker_tokens,
        };
    }

    #! Builds a safe summary for @ref LlmBackend::complete()
    /** @param system_prompt system prompt text; only length-derived counts are returned
        @param user_prompt user prompt text; only length-derived counts are returned
        @param output_schema optional structured-output schema
        @param opts optional request options

        @return safe request summary with no raw prompt content
    */
    static hash<auto> completionRequestSummary(string system_prompt,
            string user_prompt, *hash<auto> output_schema,
            *hash<LlmRequestOptions> opts) {
        hash<auto!> rv = {
            "message_count": 2,
            "system_prompt_bytes": system_prompt.length(),
            "user_prompt_bytes": user_prompt.length(),
            "estimated_input_tokens": estimateTextTokens(system_prompt)
                + estimateTextTokens(user_prompt),
            "structured_output": output_schema ? True : False,
        };
        addRequestOptions(\rv, opts);
        return rv;
    }

    #! Builds a safe summary for chat requests
    /** @param messages chat messages; only aggregate counts are returned
        @param tools optional tool definitions
        @param opts optional request options
        @param streaming True when summarizing a streaming request

        @return safe request summary with no raw message content
    */
    static hash<auto> chatRequestSummary(list<hash<ChatMessage>> messages,
            *list<hash<ToolDefinition>> tools,
            *hash<LlmRequestOptions> opts, bool streaming) {
        hash<auto!> role_counts = {};
        int text_bytes = 0;
        int content_part_count = 0;
        int image_part_count = 0;
        int file_part_count = 0;
        int tool_call_count = 0;
        foreach hash<ChatMessage> msg in (messages) {
            role_counts{msg.role} = (role_counts{msg.role} ?? 0) + 1;
            if (msg.content.val()) {
                text_bytes += msg.content.length();
            }
            if (msg.content_parts) {
                foreach hash<ContentPart> part in (msg.content_parts) {
                    ++content_part_count;
                    if (part.type == "text" && part.text.val()) {
                        text_bytes += part.text.length();
                    } else if (part.type == "image") {
                        ++image_part_count;
                    } else if (part.type == "file") {
                        ++file_part_count;
                    }
                }
            }
            if (msg.tool_calls) {
                tool_call_count += msg.tool_calls.size();
            }
        }

        list<string> tool_names = ();
        if (tools) {
            foreach hash<ToolDefinition> tool in (tools) {
                push tool_names, tool.name;
            }
        }

        hash<auto!> rv = {
            "message_count": messages.size(),
            "role_counts": role_counts,
            "text_bytes": text_bytes,
            "estimated_input_tokens": estimateTextTokensByBytes(text_bytes),
            "content_part_count": content_part_count,
            "image_part_count": image_part_count,
            "file_part_count": file_part_count,
            "tool_count": tools ? tools.size() : 0,
            "tool_names": tool_names,
            "assistant_tool_call_count": tool_call_count,
            "streaming": streaming,
        };
        addRequestOptions(\rv, opts);
        return rv;
    }

    #! Builds a safe summary for completion responses
    /** @param res completion response

        @return safe response summary with no raw response content
    */
    static hash<auto> completionResponseSummary(hash<LlmCompletionResult> res) {
        return {
            "content_bytes": res.content.length(),
            "estimated_output_tokens": estimateTextTokens(res.content),
        };
    }

    #! Builds a safe summary for chat responses
    /** @param res chat response

        @return safe response summary with no raw response content
    */
    static hash<auto> chatResponseSummary(hash<ChatResponse> res) {
        hash<auto!> rv = {
            "stop_reason": res.stop_reason,
            "content_bytes": res.content.val() ? res.content.length() : 0,
            "estimated_output_tokens": res.content.val()
                ? estimateTextTokens(res.content) : 0,
            "tool_call_count": res.tool_calls ? res.tool_calls.size() : 0,
        };
        if (res.id.val()) {
            rv.id = res.id;
        }
        if (res.model.val()) {
            rv.model = res.model;
        }
        return rv;
    }

    #! Builds a safe summary for a streaming event
    /** @param event stream event; only aggregate delta metadata is returned
        @param delta_index zero-based stream delta index

        @return safe stream delta summary with no raw delta content
    */
    static hash<auto> streamDeltaSummary(hash<ChatStreamEvent> event,
            int delta_index) {
        hash<auto!> rv = {
            "delta_index": delta_index,
            "done": event.done,
            "delta_bytes": event.delta.val() ? event.delta.length() : 0,
        };
        if (event.event.val()) {
            rv.event = event.event;
        }
        if (event.raw.typeCode() == NT_HASH) {
            *string finish_reason = extractFinishReason(event.raw);
            if (finish_reason.val()) {
                rv.finish_reason = finish_reason;
            }
            if (event.raw.usage.typeCode() == NT_HASH) {
                rv.has_usage = True;
            }
        }
        return rv;
    }

    #! Dispatches a telemetry event to an observer
    /** @param observer optional observer callbacks
        @param event event to dispatch

        @note Observer exceptions are caught and ignored.
    */
    static notify(*hash<LlmTelemetryObserver> observer,
            hash<LlmTelemetryEvent> event) {
        if (!observer) {
            return;
        }
        callObserver(observer.event_callback, event);
        switch (event.type) {
            case "start":
                callObserver(observer.start_callback, event);
                break;
            case "stream_delta":
                callObserver(observer.stream_delta_callback, event);
                break;
            case "end":
                callObserver(observer.end_callback, event);
                break;
            case "error":
                callObserver(observer.error_callback, event);
                break;
            case "retry":
                callObserver(observer.retry_callback, event);
                break;
        }
    }

    #! Converts an exception to safe telemetry metadata
    /** @param ex caught exception info

        @return safe error metadata with selected non-sensitive exception
            argument fields
    */
    static hash<auto> errorInfo(hash<ExceptionInfo> ex) {
        hash<auto!> rv = {
            "err": ex.err,
            "desc": ex.desc,
        };
        if (ex.arg.typeCode() == NT_HASH) {
            hash<auto> arg = cast<hash>(ex.arg);
            list<string> arg_keys = keys arg;
            foreach string key in (("status_code", "code", "type",
                    "retryable", "provider", "backend", "model")) {
                if (inlist(key, arg_keys)) {
                    rv{key} = arg{key};
                }
            }
        }
        return rv;
    }

    private static callObserver(
            *code<nothing(hash<LlmTelemetryEvent>)> cb,
            hash<LlmTelemetryEvent> event) {
        if (!cb) {
            return;
        }
        try {
            cb(event);
        } catch () {
        }
    }

    private static addRequestOptions(reference<hash<auto!>> rv,
            *hash<LlmRequestOptions> opts) {
        if (!opts) {
            return;
        }
        foreach string key in (("model", "max_tokens", "max_output_tokens",
                "reasoning_token_budget", "temperature")) {
            if (exists opts{key}) {
                rv{key} = opts{key};
            }
        }
        rv.has_body_options = opts.body_options ? True : False;
        rv.has_output_schema = opts.output_schema ? True : False;
    }

    private static int estimateTextTokens(string text) {
        return (text.length() + 3) / 4;
    }

    private static int estimateTextTokensByBytes(int bytes) {
        return (bytes + 3) / 4;
    }

    private static int firstInt(hash<auto> h, list<string> names) {
        foreach string name in (names) {
            if (!exists h{name}) {
                continue;
            }
            try {
                int v = h{name}.toInt();
                if (v > 0) {
                    return v;
                }
            } catch () {
            }
        }
        return 0;
    }

    private static *string extractFinishReason(hash<auto> raw) {
        if (raw.choices.typeCode() == NT_LIST && raw.choices.size()) {
            auto choice = raw.choices[0];
            if (choice.typeCode() == NT_HASH) {
                hash<auto> choice_h = cast<hash>(choice);
                if (choice_h.finish_reason.val()) {
                    return choice_h.finish_reason;
                }
            }
        }
        if (raw.finish_reason.val()) {
            return raw.finish_reason;
        }
        if (raw.finishReason.val()) {
            return raw.finishReason;
        }
        return NOTHING;
    }
}

} # namespace QoreLlmUtils
