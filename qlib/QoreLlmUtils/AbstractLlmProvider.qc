# -*- mode: qore; indent-tabs-mode: nil -*-

/*  AbstractLlmProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Abstract base class for LLM providers
/** Concrete subclasses translate the provider-neutral
    @ref LlmBackend interface (\c complete, \c chat, \c chatStream) into a
    specific wire protocol (OpenAI chat completions, Anthropic Messages,
    Google Gemini generateContent, etc.).

    @par Provider responsibilities
    - Build provider-native request bodies from @ref ChatMessage and
      @ref ToolDefinition lists
    - Dispatch the request over the appropriate transport
    - Translate provider-native responses back into @ref ChatResponse /
      @ref LlmCompletionResult / @ref ChatStreamEvent shapes
    - Manage any provider-specific connection or REST client lifecycle

    @par Provider-neutral helpers
    Subclasses inherit @ref withRetry() (retry-on-transient-errors loop),
    @ref getRequestModel() / @ref getRequestMaxTokens() /
    @ref getRequestTemperature() (per-call override resolution), and
    @ref mergeRequestBodyOptions() (provider-specific body-options merge).

    @par OpenAI-compatible facade hooks
    @ref LlmBackend::chatCompletions() and
    @ref LlmBackend::chatCompletionsStream() expose an OpenAI-shaped wire
    contract regardless of the underlying provider.  Providers that need to
    adapt OpenAI-shaped inputs (tool choice, response format) or interpret
    provider-native stream chunks override the \c applyOpenAi*() and
    \c updateOpenAiStreamStateFromRaw() / \c getOpenAiToolCallDeltas() hooks.
    Default implementations target the OpenAI wire shape directly.

    Thread-safe: implementations must allow concurrent calls.
*/
public class AbstractLlmProvider {
    private {
        #! The backend configuration passed at construction
        hash<LlmBackendConfig> config_h;
    }

    #! Stores the backend configuration for use by helpers
    /** Concrete subclasses call this from their own constructor (typically
        via \c : AbstractLlmProvider(config)) and then perform any
        provider-specific validation or REST client setup.
    */
    constructor(hash<LlmBackendConfig> config) {
        config_h = config;
    }

    #! Returns the mode string this provider services (e.g. \c "gemini")
    abstract string getMode();

    #! Returns the configured model name, or @ref nothing if none was set
    *string getModel() {
        return config_h.model;
    }

    #! Returns the underlying configuration hash
    hash<LlmBackendConfig> getConfig() {
        return config_h;
    }

    #! Makes a single-turn completion call
    /** @param system_prompt the system prompt
        @param user_prompt the user prompt
        @param output_schema optional JSON schema for structured output
        @param opts per-call request overrides

        @return a @ref LlmCompletionResult with the generated content and usage
    */
    abstract hash<LlmCompletionResult> complete(string system_prompt,
            string user_prompt, *hash<auto> output_schema,
            *hash<LlmRequestOptions> opts);

    #! Makes a multi-turn chat call with optional tool calling
    /** @param messages conversation history including system, user,
            assistant and tool messages
        @param tools optional tool definitions the LLM may call
        @param opts per-call request overrides

        @return a @ref ChatResponse describing the model's response
    */
    abstract hash<ChatResponse> chat(list<hash<ChatMessage>> messages,
            *list<hash<ToolDefinition>> tools,
            *hash<LlmRequestOptions> opts);

    #! Streams a chat completion via Server-Sent Events
    /** Default implementation throws \c LLM-ERROR; concrete providers
        override this when the underlying API supports streaming.

        @param messages conversation history
        @param callback receives each @ref ChatStreamEvent as it arrives
        @param tools optional tool definitions
        @param opts per-call overrides

        @throw LLM-ERROR streaming is not supported by this provider
    */
    chatStream(list<hash<ChatMessage>> messages,
            code<nothing(hash<ChatStreamEvent>)> callback,
            *list<hash<ToolDefinition>> tools,
            *hash<LlmRequestOptions> opts) {
        throw "LLM-ERROR",
            sprintf("streaming is not supported by the %y provider; use "
                "chat() instead", getMode());
    }

    #! Resolves the model name for a single call (per-call override > config default)
    *string getRequestModel(*hash<LlmRequestOptions> opts) {
        return opts.model ?? config_h.model;
    }

    #! Resolves the max-token budget for a single call
    int getRequestMaxTokens(*hash<LlmRequestOptions> opts) {
        return opts.max_tokens ?? config_h.max_tokens;
    }

    #! Resolves the sampling temperature for a single call
    float getRequestTemperature(*hash<LlmRequestOptions> opts) {
        return opts.temperature ?? config_h.temperature;
    }

    #! Merges provider-specific body options after generic fields
    /** Default semantics: keys in \a opts.body_options overwrite top-level
        keys in \a body verbatim.  Providers that need recursive / nested
        merge semantics (e.g. Gemini \c generationConfig) override this.
    */
    hash<auto> mergeRequestBodyOptions(hash<auto> body,
            *hash<LlmRequestOptions> opts) {
        if (!opts.body_options) {
            return body;
        }
        hash<auto!> rv;
        rv += body;
        foreach string key in (keys opts.body_options) {
            rv{key} = opts.body_options{key};
        }
        return rv;
    }

    #! Retries the given call with exponential backoff on transient errors
    /** Retries on HTTP 429 (rate limit) and 5xx (server error) responses up
        to @ref LlmBackendConfig::max_retries times.  Initial delay is
        @ref LlmBackendConfig::retry_delay_ms and doubles on each attempt;
        respects \c Retry-After when present on 429 responses.

        @param call a callable that performs the request and returns the response

        @return the response from the successful call

        @throw any non-retryable exception is rethrown; a retryable exception
        is rethrown after retries are exhausted
    */
    auto withRetry(code call) {
        int attempt = 0;
        while (True) {
            try {
                return call();
            } catch (hash<ExceptionInfo> ex) {
                bool retryable = False;
                if (ex.arg.typeCode() == NT_HASH) {
                    int status = ex.arg.status_code ?? 0;
                    if (status == 429 || status >= 500) {
                        retryable = True;
                    }
                }

                if (!retryable || attempt >= config_h.max_retries) {
                    rethrow;
                }

                ++attempt;
                int delay = config_h.retry_delay_ms * (1 << (attempt - 1));

                if (ex.arg.typeCode() == NT_HASH
                        && ex.arg.headers."retry-after".val()) {
                    int retry_after = ex.arg.headers."retry-after".toInt();
                    if (retry_after > 0) {
                        delay = retry_after * 1000;
                    }
                }

                usleep(delay * 1000);
            }
        }
    }

    #! OpenAI-facade hook: adapts an OpenAI tool_choice to provider-native form
    /** Called by @ref LlmBackend::chatCompletions() when translating an
        OpenAI-shaped request body for dispatch through this provider.
        The default writes the choice unchanged into
        \c body_options.tool_choice; providers with a different native
        encoding (e.g. Gemini \c toolConfig) override.

        @param body_options the hash being accumulated for
            @ref LlmRequestOptions::body_options
        @param choice the OpenAI \c tool_choice value (string or hash)
    */
    applyOpenAiToolChoice(reference<hash<auto!>> body_options, auto choice) {
        if (exists choice) {
            body_options.tool_choice = choice;
        }
    }

    #! OpenAI-facade hook: routes an OpenAI response_format to provider-native form
    /** Called when an OpenAI-shaped request specifies
        \c response_format=\c json_schema.  The default copies
        \c response_format into \c body_options so the OpenAI-compatible
        provider sees it verbatim; providers that prefer a structured-output
        side channel (e.g. Gemini \c responseSchema in
        @ref LlmRequestOptions::output_schema) override.

        @param body_options the body-options hash being built
        @param options the @ref LlmRequestOptions hash being built
        @param output_schema the parsed JSON schema (already validated by caller)
        @param response_format the raw OpenAI \c response_format hash from the request
    */
    applyOpenAiResponseFormat(reference<hash<auto!>> body_options,
            reference<hash<LlmRequestOptions>> options,
            hash<auto> output_schema, *hash<auto> response_format) {
        if (response_format) {
            body_options.response_format = response_format;
        }
    }

    #! OpenAI-facade hook: updates OpenAI stream state from a provider-native raw event
    /** Called once per streaming event from @ref chatStream() to populate
        the OpenAI-compatible stream-state hash with id, model, usage and
        finish-reason fields.  Default reads OpenAI shape (\c id, \c model,
        \c usage, \c choices[0].finish_reason); providers with native event
        shapes (e.g. Gemini \c responseId, \c usageMetadata,
        \c candidates[0].finishReason) override.

        @param state the OpenAI-compatible stream-state hash being accumulated
        @param raw the parsed provider-native event hash, or @ref nothing
    */
    updateOpenAiStreamStateFromRaw(reference<hash<auto>> state,
            *hash<auto> raw) {
        if (!raw) {
            return;
        }
        if (raw.id.val()) {
            state.id = raw.id;
        }
        if (raw.model.val()) {
            state.model = raw.model;
        }
        if (raw.usage.typeCode() == NT_HASH) {
            state.usage = raw.usage;
        }
        if (raw.choices[0].finish_reason.val()) {
            state.finish_reason = raw.choices[0].finish_reason;
        }
    }

    #! OpenAI-facade hook: extracts OpenAI tool-call deltas from a provider-native raw event
    /** Returns the list of OpenAI-shaped \c tool_calls deltas (index, id,
        type, function) that should be emitted to the caller for this event.
        Default returns an empty list (OpenAI streams emit tool calls
        progressively in \c choices[0].delta.tool_calls which the existing
        delta accumulator handles directly); Gemini overrides to harvest
        \c candidates[0].content.parts[].functionCall entries.

        @param state the OpenAI-compatible stream-state hash being accumulated
        @param raw the parsed provider-native event hash, or @ref nothing
    */
    list<hash<auto>> getOpenAiToolCallDeltas(reference<hash<auto>> state,
            *hash<auto> raw) {
        return ();
    }
}

} # namespace QoreLlmUtils
