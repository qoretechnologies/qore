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
public hashdecl VisionLlmConfig {
    #! Mode: "openai" for OpenAI-compatible, "anthropic" for Anthropic Messages API
    string mode;
    #! Base URL of the LLM service
    string url;
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
}

#! Result from a vision extraction operation
public hashdecl VisionExtractionResult {
    #! Extracted text or description
    string text;
    #! Output format ("text", "markdown_table", "description")
    string format;
    #! Token usage from the LLM call
    hash<auto> usage;
}

#! Multimodal LLM client for image-based extraction
/** Sends images as base64-encoded content blocks to vision-capable LLMs
    and extracts text, descriptions, or structured data.

    Supports two API formats:
    - \b openai: OpenAI-compatible \c /v1/chat/completions with image_url content blocks
    - \b anthropic: Anthropic Messages API with base64 image content blocks

    @since QoreVisionExtraction 1.0
*/
public class VisionLlmClient {
    private {
        #! Backend mode
        string mode;
        #! Model name
        string model_name;
        #! API key
        *string api_key;
        #! Max tokens
        int max_tokens;
        #! Anthropic API version
        string anthropic_version;
%ifndef NoRestClient
        #! REST client (lazy-initialized)
        *RestClient rest_client;
        #! Mutex for thread-safe lazy initialization
        Mutex rest_mutex();
        #! Base URL
        string url;
        #! Timeouts
        int connect_timeout_ms;
        int timeout_ms;
%endif
    }

    #! Creates a VisionLlmClient
    /** @param config a @ref VisionLlmConfig hash

        @throw VISION-CONFIG-ERROR if mode is invalid or required params missing
        @throw VISION-DEPENDENCY-ERROR if required modules are not available
    */
    constructor(hash<VisionLlmConfig> config) {
        mode = config.mode;
        if (mode != "openai" && mode != "anthropic") {
            throw "VISION-CONFIG-ERROR",
                sprintf("invalid mode %y; expected \"openai\" or \"anthropic\"", mode);
        }
        if (!config.url.val()) {
            throw "VISION-CONFIG-ERROR", "url is required";
        }
%ifdef NoRestClient
        throw "VISION-DEPENDENCY-ERROR",
            "RestClient module is required but is not available";
%endif
%ifdef NoJson
        throw "VISION-DEPENDENCY-ERROR",
            "json module is required but is not available";
%endif
        model_name = config.model;
        api_key = config.api_key;
        max_tokens = config.max_tokens;
        anthropic_version = config.anthropic_version;
%ifndef NoRestClient
        url = config.url;
        connect_timeout_ms = config.connect_timeout_ms;
        timeout_ms = config.timeout_ms;
%endif
    }

    #! Describes an image using a vision LLM
    /** @param image_data raw image bytes
        @param mime_type MIME type (e.g., "image/png", "image/jpeg")
        @param prompt optional custom prompt (default: "Describe this image in detail")
        @return a @ref VisionExtractionResult hash

        @throw VISION-ERROR if the request fails
    */
    hash<VisionExtractionResult> describeImage(binary image_data, string mime_type,
            *string prompt) {
        string sys = "You are a visual analysis assistant. Describe images accurately "
            "and comprehensively.";
        string user_prompt = prompt ?? "Describe this image in detail.";
        string text = callVisionLlm(sys, user_prompt, image_data, mime_type);
        return <VisionExtractionResult>{
            "text": text,
            "format": "description",
            "usage": {},
        };
    }

    #! Extracts a table from an image as a markdown table
    /** @param image_data raw image bytes containing a table
        @param mime_type MIME type
        @return a @ref VisionExtractionResult with markdown table text

        @throw VISION-ERROR if the request fails
    */
    hash<VisionExtractionResult> extractTable(binary image_data, string mime_type) {
        string sys = "You are a table extraction engine. Extract tables from images "
            "and output them as markdown tables.";
        string user_prompt = "Extract all tables from this image. Output each table "
            "as a markdown table with headers. If no table is found, say \"No table found.\"";
        string text = callVisionLlm(sys, user_prompt, image_data, mime_type);
        return <VisionExtractionResult>{
            "text": text,
            "format": "markdown_table",
            "usage": {},
        };
    }

    #! Extracts text content from an image
    /** @param image_data raw image bytes
        @param mime_type MIME type
        @return a @ref VisionExtractionResult with extracted text

        @throw VISION-ERROR if the request fails
    */
    hash<VisionExtractionResult> extractText(binary image_data, string mime_type) {
        string sys = "You are a text extraction engine. Extract all visible text from "
            "images exactly as it appears, preserving layout where possible.";
        string user_prompt = "Extract all text visible in this image. Preserve the "
            "original layout and formatting as much as possible.";
        string text = callVisionLlm(sys, user_prompt, image_data, mime_type);
        return <VisionExtractionResult>{
            "text": text,
            "format": "text",
            "usage": {},
        };
    }

    #! Calls the vision LLM with an image
    private string callVisionLlm(string system_prompt, string user_prompt,
            binary image_data, string mime_type) {
%ifndef NoRestClient
        ensureRestClient();

        string b64 = make_base64_string(image_data);

        if (mode == "openai") {
            return callOpenAi(system_prompt, user_prompt, b64, mime_type);
        }
        return callAnthropic(system_prompt, user_prompt, b64, mime_type);
%endif
        throw "VISION-ERROR", "RestClient not available";
    }

%ifndef NoRestClient
    #! Calls OpenAI-compatible vision API
    private string callOpenAi(string system_prompt, string user_prompt,
            string b64_image, string mime_type) {
        hash<auto> body = {
            "model": model_name,
            "max_tokens": max_tokens,
            "messages": (
                {"role": "system", "content": system_prompt},
                {
                    "role": "user",
                    "content": (
                        {
                            "type": "image_url",
                            "image_url": {
                                "url": sprintf("data:%s;base64,%s", mime_type, b64_image),
                            },
                        },
                        {
                            "type": "text",
                            "text": user_prompt,
                        },
                    ),
                },
            ),
        };

        hash<auto> hdr = {"Content-Type": "application/json"};
        if (api_key.val()) {
            hdr."Authorization" = "Bearer " + api_key;
        }

        hash<auto> resp;
        try {
            resp = rest_client.post("/v1/chat/completions", body, NOTHING, \hdr);
        } catch (hash<ExceptionInfo> ex) {
            throw "VISION-ERROR", sprintf("OpenAI vision request failed: %s", ex.desc), ex;
        }

        if (resp.body.typeCode() == NT_STRING) {
            resp.body = parse_json(resp.body);
        }

        return resp.body.choices[0].message.content ?? "";
    }

    #! Calls Anthropic Messages API with vision
    private string callAnthropic(string system_prompt, string user_prompt,
            string b64_image, string mime_type) {
        hash<auto> body = {
            "model": model_name,
            "max_tokens": max_tokens,
            "system": system_prompt,
            "messages": (
                {
                    "role": "user",
                    "content": (
                        {
                            "type": "image",
                            "source": {
                                "type": "base64",
                                "media_type": mime_type,
                                "data": b64_image,
                            },
                        },
                        {
                            "type": "text",
                            "text": user_prompt,
                        },
                    ),
                },
            ),
        };

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
            throw "VISION-ERROR",
                sprintf("Anthropic vision request failed: %s", ex.desc), ex;
        }

        if (resp.body.typeCode() == NT_STRING) {
            resp.body = parse_json(resp.body);
        }

        # Extract text from content blocks
        list<auto> content_blocks = resp.body.content ?? ();
        foreach hash<auto> block in (content_blocks) {
            if (block.type == "text") {
                return block.text;
            }
        }

        return "";
    }

    #! Ensures the REST client is initialized
    private ensureRestClient() {
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
    }
%endif
}

} # namespace QoreVisionExtraction
