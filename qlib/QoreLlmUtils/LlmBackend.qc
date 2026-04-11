# -*- mode: qore; indent-tabs-mode: nil -*-

/*  LlmBackend.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see LlmBackend
public namespace QoreLlmUtils {

#! Configuration for the LLM backend
/** Specifies how to connect to the LLM — via an OpenAI-compatible HTTP
    endpoint, the Anthropic Messages API, or the Qorus AI Gateway.
*/
public hashdecl LlmBackendConfig {
    #! Mode: "http" for OpenAI-compatible, "anthropic" for Anthropic Messages API, "qorus" for Qorus AI Gateway
    string mode;
    #! Base URL of the LLM service (required for "http" and "anthropic" modes)
    *string url;
    #! Model name to pass in the request
    *string model;
    #! API key for authentication — this is the secret key from your LLM provider (e.g., from the OpenAI or Anthropic dashboard)
    *string api_key;
    #! Maximum tokens to generate; default 4096
    int max_tokens = 4096;
    #! Sampling temperature; default 0.0 (deterministic)
    float temperature = 0.0;
    #! HTTP connect timeout in milliseconds; default 10000
    int connect_timeout_ms = 10000;
    #! HTTP request timeout in milliseconds; default 120000
    int timeout_ms = 120000;
    #! Anthropic API version header; default "2023-06-01"
    string anthropic_version = "2023-06-01";
}

#! Result from an LLM completion call
public hashdecl LlmCompletionResult {
    #! The generated text content
    string content;
    #! Token usage information (provider-specific)
    hash<auto> usage;
}

#! Pluggable LLM backend for chat completions
/** Provides a unified interface for making LLM chat completion calls across
    multiple providers.  Supports three backends:

    - \b http: OpenAI-compatible \c /v1/chat/completions with optional
      \c response_format for JSON schema enforcement.
    - \b anthropic: Anthropic Messages API (\c /v1/messages) with tool-use-based
      structured extraction when an output schema is provided.
    - \b qorus: Qorus AI Gateway — delegates to \c UserApi::callRestApi().

    Thread-safe: the underlying REST client is lazily initialized behind a mutex.

    @since QoreLlmUtils 1.0

    @par Example: Create an HTTP backend and get a completion
    @code{.py}
%modern
%requires QoreLlmUtils

LlmBackend llm(<LlmBackendConfig>{
    "mode": "http",
    "url": "http://localhost:8080",
    "model": "gpt-4o",
});
hash<LlmCompletionResult> result = llm.complete(
    "You are a helpful assistant.",
    "What are the main benefits of cloud computing?",
);
printf("Response: %s\n", result.content);
    @endcode
*/
public class LlmBackend {
    private {
        #! The backend mode
        string mode;
        #! Base URL
        *string url;
        #! Model name
        *string model_name;
        #! API key
        *string api_key;
        #! Max tokens to generate
        int max_tokens;
        #! Sampling temperature
        float temperature;
        #! Connect timeout
        int connect_timeout_ms;
        #! Request timeout
        int timeout_ms;
        #! Anthropic API version
        string anthropic_version;
%ifndef NoRestClient
        #! REST client for HTTP/Anthropic modes (lazy-initialized)
        *RestClient rest_client;
        #! Mutex for thread-safe lazy initialization
        Mutex rest_mutex();
%endif
    }

    #! Creates an LlmBackend with the given configuration
    /** @param config an @ref LlmBackendConfig hash specifying the backend and parameters

        @throw LLM-CONFIG-ERROR if the mode is invalid or required parameters are missing
        @throw LLM-DEPENDENCY-ERROR if required modules are not available
    */
    constructor(hash<LlmBackendConfig> config) {
        mode = config.mode;
        if (mode != "http" && mode != "anthropic" && mode != "qorus") {
            throw "LLM-CONFIG-ERROR",
                sprintf("invalid mode %y; expected \"http\", \"anthropic\", or \"qorus\"", mode);
        }

        if (mode == "http" || mode == "anthropic") {
            if (!config.url.val()) {
                throw "LLM-CONFIG-ERROR",
                    sprintf("url is required for %y mode", mode);
            }
%ifdef NoRestClient
            throw "LLM-DEPENDENCY-ERROR",
                "RestClient module is required for HTTP/Anthropic mode but is not available";
%endif
%ifdef NoJson
            throw "LLM-DEPENDENCY-ERROR",
                "json module is required for HTTP/Anthropic mode but is not available";
%endif
            url = config.url;
        }

        model_name = config.model;
        api_key = config.api_key;
        max_tokens = config.max_tokens;
        temperature = config.temperature;
        connect_timeout_ms = config.connect_timeout_ms;
        timeout_ms = config.timeout_ms;
        anthropic_version = config.anthropic_version;
    }

    #! Makes a chat completion call and returns the result
    /** @param system_prompt the system prompt that defines the LLM's role
        @param user_prompt the user message / input text
        @param output_schema optional JSON schema to enforce structured output;
               for HTTP mode this uses \c response_format, for Anthropic mode
               this uses a tool-use extraction pattern
        @return an @ref LlmCompletionResult hash with the content and usage

        @throw LLM-ERROR if the completion request fails
    */
    hash<LlmCompletionResult> complete(string system_prompt, string user_prompt,
            *hash<auto> output_schema) {
        switch (mode) {
            case "http":
                return completeHttp(system_prompt, user_prompt, output_schema);
            case "anthropic":
                return completeAnthropic(system_prompt, user_prompt, output_schema);
            case "qorus":
                return completeQorus(system_prompt, user_prompt, output_schema);
        }
        # unreachable — mode validated in constructor
        throw "LLM-ERROR", sprintf("unsupported mode %y", mode);
    }

    /** @return the configured backend mode ("http", "anthropic", or "qorus") */
    string getMode() {
        return mode;
    }

    /** @return the configured model name, or @ref nothing if none was set */
    *string getModel() {
        return model_name;
    }

    #! Ensures the REST client is initialized
    private ensureRestClient() {
%ifndef NoRestClient
        if (!rest_client) {
            rest_mutex.lock();
            on_exit rest_mutex.unlock();
            if (!rest_client) {
                rest_client = new RestClient({
                    "url": url,
                    "connect_timeout": connect_timeout_ms,
                    "timeout": timeout_ms,
                });
            }
        }
%endif
    }

    #! Completes via OpenAI-compatible /v1/chat/completions endpoint
    private hash<LlmCompletionResult> completeHttp(string system_prompt, string user_prompt,
            *hash<auto> output_schema) {
%ifndef NoRestClient
        ensureRestClient();

        hash<auto> body = {
            "messages": (
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": user_prompt},
            ),
            "max_tokens": max_tokens,
            "temperature": temperature,
        };
        if (model_name.val()) {
            body.model = model_name;
        }
        if (output_schema) {
            body.response_format = {
                "type": "json_schema",
                "json_schema": {
                    "name": output_schema.name ?? "output",
                    "strict": True,
                    "schema": output_schema,
                },
            };
        }

        hash<auto> hdr = {"Content-Type": "application/json"};
        if (api_key.val()) {
            hdr."Authorization" = "Bearer " + api_key;
        }

        hash<auto> resp;
        try {
            resp = rest_client.post("/v1/chat/completions", body, NOTHING, \hdr);
        } catch (hash<ExceptionInfo> ex) {
            throw "LLM-ERROR", sprintf("HTTP completion request failed: %s", ex.desc), ex;
        }

        if (resp.body.typeCode() == NT_STRING) {
            resp.body = parse_json(resp.body);
        }

        string content = resp.body.choices[0].message.content ?? "";
        hash<auto> usage = resp.body.usage ?? {};

        return <LlmCompletionResult>{
            "content": content,
            "usage": usage,
        };
%endif
        throw "LLM-ERROR", "RestClient not available";
    }

    #! Completes via Anthropic Messages API /v1/messages
    private hash<LlmCompletionResult> completeAnthropic(string system_prompt, string user_prompt,
            *hash<auto> output_schema) {
%ifndef NoRestClient
        ensureRestClient();

        hash<auto> body = {
            "model": model_name ?? "claude-sonnet-4-20250514",
            "max_tokens": max_tokens,
            "system": system_prompt,
            "messages": (
                {"role": "user", "content": user_prompt},
            ),
        };

        # When an output schema is provided, use tool-use extraction pattern:
        # define a tool whose input_schema is the desired output, then force
        # the model to call it via tool_choice
        if (output_schema) {
            string tool_name = output_schema.name ?? "extract";
            body.tools = (
                {
                    "name": tool_name,
                    "description": "Extract structured data matching the schema",
                    "input_schema": output_schema,
                },
            );
            body.tool_choice = {"type": "tool", "name": tool_name};
        }

        hash<auto> hdr = {
            "Content-Type": "application/json",
            "anthropic-version": anthropic_version,
        };
        if (api_key.val()) {
            hdr."x-api-key" = api_key;
        }

        hash<auto> resp;
        try {
            resp = rest_client.post("/v1/messages", body, NOTHING, \hdr);
        } catch (hash<ExceptionInfo> ex) {
            throw "LLM-ERROR", sprintf("Anthropic completion request failed: %s", ex.desc), ex;
        }

        if (resp.body.typeCode() == NT_STRING) {
            resp.body = parse_json(resp.body);
        }

        # Extract content from response
        string content = "";
        list<auto> content_blocks = resp.body.content ?? ();
        foreach hash<auto> block in (content_blocks) {
            if (block.type == "text") {
                content = block.text;
                break;
            } else if (block.type == "tool_use") {
                # For tool-use extraction, the structured output is in the input field
                content = make_json(block.input);
                break;
            }
        }

        hash<auto> usage = resp.body.usage ?? {};

        return <LlmCompletionResult>{
            "content": content,
            "usage": usage,
        };
%endif
        throw "LLM-ERROR", "RestClient not available";
    }

    #! Completes via Qorus AI Gateway
    /** Calls \c UserApi::callRestApi() to route through the Qorus AI Gateway.
        Requires running inside a Qorus context where \c UserApi is available.

        @throw LLM-ERROR if the Qorus API call fails
    */
    private hash<LlmCompletionResult> completeQorus(string system_prompt, string user_prompt,
            *hash<auto> output_schema) {
        hash<auto> body = {
            "messages": (
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": user_prompt},
            ),
            "max_tokens": max_tokens,
            "temperature": temperature,
        };
        if (model_name.val()) {
            body.model = model_name;
        }
        if (output_schema) {
            body.response_format = {
                "type": "json_schema",
                "json_schema": {
                    "name": output_schema.name ?? "output",
                    "strict": True,
                    "schema": output_schema,
                },
            };
        }

        hash<auto> resp;
        try {
            # Use dynamic dispatch — UserApi is only available in Qorus context
            resp = call_static_method("UserApi", "callRestApi", "PUT",
                "ai/v1/chat/completions", body);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "UNKNOWN-CLASS") {
                throw "LLM-ERROR",
                    "Qorus mode requires a Qorus runtime context (UserApi not available)";
            }
            throw "LLM-ERROR", sprintf("Qorus AI Gateway call failed: %s", ex.desc), ex;
        }

        string content = resp.choices[0].message.content ?? "";
        hash<auto> usage = resp.usage ?? {};

        return <LlmCompletionResult>{
            "content": content,
            "usage": usage,
        };
    }
}

} # namespace QoreLlmUtils
