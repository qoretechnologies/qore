# -*- mode: qore; indent-tabs-mode: nil -*-

/*  VisionLlmClient.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see VisionLlmClient
public namespace QoreVisionExtraction {

#! Configuration for VisionLlmClient
/** Specifies how to connect to the vision LLM. Supports all modes from
    @ref QoreLlmUtils::LlmBackend "LlmBackend": HTTP/OpenAI, Anthropic,
    and custom API gateways via callback.

    For backward compatibility, \c "openai" is accepted as an alias for \c "http".
*/
public hashdecl VisionLlmConfig {
    #! Mode: "http"/"openai" for OpenAI-compatible, "anthropic" for Anthropic,
    #! "callback" for a custom API dispatch function
    string mode;
    #! Base URL of the LLM service (required for "http" and "anthropic" modes)
    *string url;
    #! Model name (e.g., "gpt-4o", "claude-sonnet-4-20250514")
    string model;
    #! API key for authentication
    *string api_key;
    #! Maximum tokens to generate; default 4096
    int max_tokens = 4096;
    #! HTTP connect timeout in milliseconds; default 10000
    int connect_timeout_ms = 10000;
    #! HTTP request timeout in milliseconds; default 120000
    int timeout_ms = 120000;
    #! Anthropic API version; default "2023-06-01"
    string anthropic_version = "2023-06-01";
    #! Custom system prompt — overrides the default prompt for each operation
    *string system_prompt;
    #! Callback mode: a callable for custom API dispatch
    *code<hash<auto>(string, string, hash<auto>)> api_callback;
}

#! Result from a vision extraction operation
public hashdecl VisionExtractionResult {
    #! Extracted text, description, or structured data as text
    string text;
    #! Output format ("text", "markdown_table", "description", "structured")
    string format;
    #! Token usage from the LLM call
    hash<auto> usage;
    #! Parsed structured output when an output schema was used
    *hash<auto> structured;
}

#! Multimodal LLM client for image-based extraction
/** Sends images to vision-capable LLMs and extracts text, descriptions,
    tables, or structured data.  Built on
    @ref QoreLlmUtils::LlmBackend "LlmBackend" — supports all backend modes
    including HTTP/OpenAI-compatible endpoints, Anthropic Messages API, and
    custom API gateways.

    @par Example: Describe an image using a callback API
    @code{.py}
%modern
%requires QoreVisionExtraction

VisionLlmClient client(<VisionLlmConfig>{
    "mode": "callback",
    "model": "gpt-4o",
});
binary img = ReadOnlyFile::readBinaryFile("photo.jpg");
hash<VisionExtractionResult> result = client.describeImage(img, "image/jpeg");
printf("Description: %s\n", result.text);
printf("Tokens used: %y\n", result.usage);
    @endcode

    @par Example: Extract structured data from an image
    @code{.py}
%modern
%requires QoreVisionExtraction

VisionLlmClient client(<VisionLlmConfig>{
    "mode": "http",
    "url": "https://api.openai.com",
    "model": "gpt-4o",
    "api_key": ENV.OPENAI_API_KEY,
});

hash<auto> schema = {
    "type": "object",
    "name": "receipt_data",
    "properties": {
        "store_name": {"type": "string"},
        "total": {"type": "number"},
        "items": {"type": "array", "items": {"type": "object",
            "properties": {"name": {"type": "string"}, "price": {"type": "number"}}}},
    },
    "required": ("store_name", "total"),
};

binary img = ReadOnlyFile::readBinaryFile("receipt.jpg");
hash<VisionExtractionResult> result = client.extractStructured(img, "image/jpeg", schema);
printf("Store: %s, Total: %y\n", result.structured.store_name, result.structured.total);
    @endcode
*/
public class VisionLlmClient {
    private {
        #! The underlying LLM backend
        QoreLlmUtils::LlmBackend llm;
        #! Custom system prompt override
        *string custom_system_prompt;
    }

    #! Creates a VisionLlmClient
    /** @param config a @ref VisionLlmConfig hash

        @throw VISION-CONFIG-ERROR if mode is invalid or required params missing
    */
    constructor(hash<VisionLlmConfig> config) {
        # Map "openai" -> "http" for backward compatibility
        string backend_mode = config.mode;
        if (backend_mode == "openai") {
            backend_mode = "http";
        }

        if (backend_mode != "http" && backend_mode != "anthropic"
                && backend_mode != "callback") {
            throw "VISION-CONFIG-ERROR",
                sprintf("invalid mode %y; expected \"http\", \"openai\", "
                    "\"anthropic\", or \"callback\"", config.mode);
        }

        hash<QoreLlmUtils::LlmBackendConfig> llm_config(<QoreLlmUtils::LlmBackendConfig>{
            "mode": backend_mode,
            "url": config.url,
            "model": config.model,
            "api_key": config.api_key,
            "max_tokens": config.max_tokens,
            "connect_timeout_ms": config.connect_timeout_ms,
            "timeout_ms": config.timeout_ms,
            "anthropic_version": config.anthropic_version,
        });
        if (config.api_callback) {
            llm_config.api_callback = config.api_callback;
        }
        llm = new QoreLlmUtils::LlmBackend(llm_config);

        custom_system_prompt = config.system_prompt;
    }

    #! Describes an image using a vision LLM
    /** @param image_data raw image bytes
        @param mime_type MIME type (e.g., "image/png", "image/jpeg")
        @param prompt optional custom user prompt (default: "Describe this image
            in detail")
        @return a @ref VisionExtractionResult hash

        @throw LLM-ERROR if the request fails
    */
    hash<VisionExtractionResult> describeImage(binary image_data, string mime_type,
            *string prompt) {
        string sys = custom_system_prompt
            ?? "You are a visual analysis assistant. Describe images accurately "
               "and comprehensively.";
        string user_prompt = prompt ?? "Describe this image in detail.";
        return callVision(sys, user_prompt, image_data, mime_type, "description");
    }

    #! Extracts tables from an image as markdown
    /** @param image_data raw image bytes containing a table
        @param mime_type MIME type
        @return a @ref VisionExtractionResult with markdown table text

        @throw LLM-ERROR if the request fails
    */
    hash<VisionExtractionResult> extractTable(binary image_data, string mime_type) {
        string sys = custom_system_prompt
            ?? "You are a table extraction engine. Extract tables from images "
               "and output them as markdown tables.";
        string user_prompt = "Extract all tables from this image. Output each table "
            "as a markdown table with headers. If no table is found, say "
            "\"No table found.\"";
        return callVision(sys, user_prompt, image_data, mime_type, "markdown_table");
    }

    #! Extracts visible text from an image
    /** @param image_data raw image bytes
        @param mime_type MIME type
        @return a @ref VisionExtractionResult with extracted text

        @throw LLM-ERROR if the request fails
    */
    hash<VisionExtractionResult> extractText(binary image_data, string mime_type) {
        string sys = custom_system_prompt
            ?? "You are a text extraction engine. Extract all visible text from "
               "images exactly as it appears, preserving layout where possible.";
        string user_prompt = "Extract all text visible in this image. Preserve the "
            "original layout and formatting as much as possible.";
        return callVision(sys, user_prompt, image_data, mime_type, "text");
    }

    #! Extracts structured data from an image using a JSON schema
    /** @param image_data raw image bytes
        @param mime_type MIME type
        @param schema JSON Schema defining the output structure
        @param prompt optional custom user prompt
        @return a @ref VisionExtractionResult with structured data in the
            \c structured field

        @throw LLM-ERROR if the request fails
        @throw VISION-CONFIG-ERROR if no schema is provided
    */
    hash<VisionExtractionResult> extractStructured(binary image_data, string mime_type,
            hash<auto> schema, *string prompt) {
        string sys = custom_system_prompt
            ?? "You are a structured data extraction engine. Extract data from "
               "images according to the provided schema.";
        string user_prompt = prompt
            ?? "Extract structured data from this image according to the schema.";
        return callVision(sys, user_prompt, image_data, mime_type, "structured",
            schema);
    }

    #! Core vision call using LlmBackend.chat()
    private hash<VisionExtractionResult> callVision(string system_prompt,
            string user_prompt, binary image_data, string mime_type, string format,
            *hash<auto> schema) {
        list<hash<QoreLlmUtils::ChatMessage>> messages = (
            <QoreLlmUtils::ChatMessage>{"role": "system", "content": system_prompt},
            <QoreLlmUtils::ChatMessage>{
                "role": "user",
                "content_parts": (
                    <QoreLlmUtils::ContentPart>{
                        "type": "image",
                        "data": image_data,
                        "mime_type": mime_type,
                    },
                    <QoreLlmUtils::ContentPart>{
                        "type": "text",
                        "text": user_prompt,
                    },
                ),
            },
        );

        # For structured extraction, use tools to enforce the schema
        *list<hash<QoreLlmUtils::ToolDefinition>> tools;
        if (schema) {
            tools = (
                <QoreLlmUtils::ToolDefinition>{
                    "name": schema.name ?? "extract",
                    "description": "Extract structured data from the image",
                    "parameters": schema,
                },
            );
        }

        hash<QoreLlmUtils::ChatResponse> resp = llm.chat(messages, tools);

        string text = resp.content ?? "";
        *hash<auto> structured;

        # If tool calls returned (structured extraction), extract the arguments
        if (resp.tool_calls) {
            structured = resp.tool_calls[0].arguments;
%ifndef NoJson
            text = make_json(structured);
%else
            text = sprintf("%y", structured);
%endif
        }

        return <VisionExtractionResult>{
            "text": text,
            "format": format,
            "usage": resp.usage,
            "structured": structured,
        };
    }
}

} # namespace QoreVisionExtraction
