# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GrokDataProvider module definition

/** GrokModelCreateResponseDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the GrokDataProvider module
public namespace GrokDataProvider {
#! Grok model create response data provider - creates a response via the Responses API
public class GrokModelCreateResponseDataProvider inherits GrokDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-response",
            "type": "GrokModelCreateResponseDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = GrokResponseCreateRequestType;

        #! Response type
        const ResponseType = GrokResponseType;
    }

    private {
        #! Model info
        *hash<auto> model;
    }

    #! Creates the object from the arguments
    constructor(*RestClientIo::RestClientIo rest) : GrokDataProviderCommon(rest) {
    }

    #! Creates the object with model info
    constructor(*RestClientIo::RestClientIo rest, hash<auto> model) : GrokDataProviderCommon(rest) {
        self.model = model;
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> ctx_info = {};
        *GrokConversationContextStore ctx_store = getConversationContextStore();
        req = prepareRequest(req, model, ctx_store, \ctx_info);

        hash<auto> rv = doRestCommand("POST", "responses", req).body;

        # Post-process response
        if (rv.output) {
            rv.output_text = getOutputText(rv.output);
            rv.tool_calls = getToolCalls(rv.output);
            rv.tool_results = getToolResults(rv.output);
        }

        # Persist conversation context
        if (rv.id) {
            hash<auto> new_ctx;
            new_ctx.previous_response_id = rv.id;
            if (rv.conversation_id) {
                new_ctx.conversation_id = rv.conversation_id;
            }
            *hash<auto> effective_metadata = ctx_info.ctx.metadata ?? req.metadata;
            if (effective_metadata) {
                new_ctx.metadata = effective_metadata;
            }
            rv.conversation_context = new_ctx;
            if (ctx_info.ctx_key && ctx_store) {
                ctx_store.saveContext(ctx_info.ctx_key, new_ctx);
            }
        }
        return rv;
    }

    #! Prepares the request with model and context
    static hash<auto> prepareRequest(hash<auto> req, *hash<auto> model, *GrokConversationContextStore ctx_store,
            reference<hash<auto>> ctx_info) {
        # Set model from context if available
        if (model && !req.model) {
            req.model = model.id;
        }

        # Transform typed input to API format
        if (req.input.typeCode() == NT_LIST) {
            req.input = map getInput($1), req.input;
        }

        # Load conversation context
        if (ctx_store && req.context_key) {
            ctx_info.ctx_key = remove req.context_key;
            ctx_info.ctx = ctx_store.loadContext(ctx_info.ctx_key);
            if (ctx_info.ctx.previous_response_id && !req.previous_response_id) {
                req.previous_response_id = ctx_info.ctx.previous_response_id;
            }
        }

        return req;
    }

    #! Converts a typed input item to API format
    private static auto getInput(auto input) {
        # Plain string — pass through
        if (input.typeCode() == NT_STRING) {
            return input;
        }
        if (input.typeCode() != NT_HASH) {
            return input;
        }
        # Build API message from flat fields
        # NOTE: hash<auto!> prevents type narrowing — the Qorus UI may pass richtext (list)
        # values for string fields inside list element types
        hash<auto!> msg = {
            "role": string(input.role ?? "user"),
        };
        # Build content from text and/or image_url
        if (input.text && input.image_url) {
            # Both provided — multimodal content array
            msg.content = (
                {"type": "input_text", "text": input.text},
                {"type": "input_image", "image_url": input.image_url},
            );
        } else if (input.image_url) {
            # Image only
            msg.content = (
                {"type": "input_image", "image_url": input.image_url},
            );
        } else if (input.text) {
            # Text only — use plain string
            msg.content = input.text;
        }
        return msg;
    }

    #! Extracts text output from the response output array
    static *string getOutputText(list<auto> output) {
        string text = "";
        foreach hash<auto> item in (output) {
            if (item.type == "message" && item.content) {
                foreach hash<auto> content in (item.content) {
                    if (content.type == "output_text" && content.text) {
                        text += content.text;
                    }
                }
            }
        }
        return text.val() ? text : NOTHING;
    }

    #! Extracts tool calls from the response output array
    static *list<hash<auto>> getToolCalls(list<auto> output) {
        list<hash<auto>> rv;
        foreach hash<auto> item in (output) {
            if (item.type == "function_call") {
                rv += item;
            }
        }
        return rv ?? NOTHING;
    }

    #! Extracts tool results from the response output array
    static *list<hash<auto>> getToolResults(list<auto> output) {
        list<hash<auto>> rv;
        foreach hash<auto> item in (output) {
            if (item.type == "function_call_output") {
                rv += item;
            }
        }
        return rv ?? NOTHING;
    }

    #! Returns the description of a successful request message, if any
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Reasoning configuration data type for the Responses API
public class GrokReasoningConfigDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "effort": {
                "display_name": "Effort",
                "short_desc": "How much reasoning effort the model should use",
                "desc": "Controls how much time the model spends thinking. Only supported by reasoning-capable "
                    "models (e.g., `grok-3-mini`)",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Low",
                        "short_desc": "Minimal thinking",
                        "desc": "Minimal thinking",
                        "value": "low",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Medium",
                        "short_desc": "Moderate thinking",
                        "desc": "Moderate thinking",
                        "value": "medium",
                    },
                    <AllowedValueInfo>{
                        "display_name": "High",
                        "short_desc": "Maximum thinking",
                        "desc": "Maximum thinking",
                        "value": "high",
                    },
                ),
            },
            "summary": {
                "display_name": "Summary",
                "short_desc": "Type of reasoning summary to include",
                "desc": "Controls the type of reasoning summary included in the response",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Auto",
                        "short_desc": "Automatic summary",
                        "desc": "Automatic summary",
                        "value": "auto",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Concise",
                        "short_desc": "Brief summary of reasoning",
                        "desc": "Brief summary of reasoning",
                        "value": "concise",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Detailed",
                        "short_desc": "Detailed summary of reasoning",
                        "desc": "Detailed summary of reasoning",
                        "value": "detailed",
                    },
                ),
            },
            "generate_summary": {
                "display_name": "Generate Summary",
                "short_desc": "Summary generation control",
                "desc": "Controls summary generation for reasoning output",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Auto",
                        "short_desc": "Automatic summary generation",
                        "desc": "Automatic summary generation",
                        "value": "auto",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Concise",
                        "short_desc": "Generate concise summary",
                        "desc": "Generate concise summary",
                        "value": "concise",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Detailed",
                        "short_desc": "Generate detailed summary",
                        "desc": "Generate detailed summary",
                        "value": "detailed",
                    },
                ),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("GrokReasoningConfig") {
        addQoreFields(Fields);
    }

    #! Creates the type with a specific base type
    constructor(Type base_type) : HashDataType(base_type, "GrokReasoningConfig") {
        addQoreFields(Fields);
    }
}

#! Constant for reasoning config data type
public const GrokReasoningConfigDataType = new GrokReasoningConfigDataType();

#! Response tool data type for the Responses API (different format from chat completions)
public class GrokResponseToolDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "type": {
                "display_name": "Type",
                "short_desc": "The type of tool",
                "desc": "The type of tool. `function` for custom functions, `web_search` for built-in web search",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Function",
                        "short_desc": "Custom function tool",
                        "desc": "Custom function tool",
                        "value": "function",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Web Search",
                        "short_desc": "Built-in web search tool",
                        "desc": "Built-in web search tool",
                        "value": "web_search",
                    },
                ),
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The name of the function",
                "desc": "The name of the function to be called. Required for `function` type tools",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "A description of what the function does",
                "desc": "A description of what the function does, used by the model to choose when and how to "
                    "call the function",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "parameters": {
                "display_name": "Parameters",
                "short_desc": "The function parameters as a JSON Schema object",
                "desc": "The parameters the function accepts, described as a JSON Schema object.\n\n"
                    "**Example**: `{\"type\": \"object\", \"properties\": {\"location\": {\"type\": \"string\", "
                    "\"description\": \"City name\"}, \"unit\": {\"type\": \"string\", \"enum\": [\"celsius\", "
                    "\"fahrenheit\"]}}, \"required\": [\"location\"]}`",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("GrokResponseTool") {
        addQoreFields(Fields);
    }
}

#! Constant for response tool data type
public const GrokResponseToolDataType = new GrokResponseToolDataType();

#! Token details data type for input tokens
public class GrokInputTokenDetailsDataType inherits HashDataType {
    public {
        const Fields = {
            "cached_tokens": {
                "display_name": "Cached Tokens",
                "short_desc": "Number of cached tokens",
                "desc": "Number of input tokens served from cache",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "audio_tokens": {
                "display_name": "Audio Tokens",
                "short_desc": "Number of audio input tokens",
                "desc": "Number of audio input tokens",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "image_tokens": {
                "display_name": "Image Tokens",
                "short_desc": "Number of image input tokens",
                "desc": "Number of image input tokens",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "text_tokens": {
                "display_name": "Text Tokens",
                "short_desc": "Number of text input tokens",
                "desc": "Number of text input tokens",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    constructor() : HashDataType("GrokInputTokenDetails") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type, "GrokInputTokenDetails") {
        addQoreFields(Fields);
    }
}

public const GrokInputTokenDetailsDataType = new GrokInputTokenDetailsDataType();

#! Token details data type for output tokens
public class GrokOutputTokenDetailsDataType inherits HashDataType {
    public {
        const Fields = {
            "reasoning_tokens": {
                "display_name": "Reasoning Tokens",
                "short_desc": "Number of reasoning tokens",
                "desc": "Number of tokens used for reasoning",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "accepted_prediction_tokens": {
                "display_name": "Accepted Prediction Tokens",
                "short_desc": "Number of accepted prediction tokens",
                "desc": "Number of accepted prediction tokens",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "audio_tokens": {
                "display_name": "Audio Tokens",
                "short_desc": "Number of audio output tokens",
                "desc": "Number of audio output tokens",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "rejected_prediction_tokens": {
                "display_name": "Rejected Prediction Tokens",
                "short_desc": "Number of rejected prediction tokens",
                "desc": "Number of rejected prediction tokens",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    constructor() : HashDataType("GrokOutputTokenDetails") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type, "GrokOutputTokenDetails") {
        addQoreFields(Fields);
    }
}

public const GrokOutputTokenDetailsDataType = new GrokOutputTokenDetailsDataType();

#! Server-side tool usage details data type
public class GrokServerToolUsageDataType inherits HashDataType {
    public {
        const Fields = {
            "code_interpreter_calls": {
                "display_name": "Code Interpreter Calls",
                "short_desc": "Number of code interpreter calls",
                "desc": "Number of code interpreter calls made",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "document_search_calls": {
                "display_name": "Document Search Calls",
                "short_desc": "Number of document search calls",
                "desc": "Number of document search calls made",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "file_search_calls": {
                "display_name": "File Search Calls",
                "short_desc": "Number of file search calls",
                "desc": "Number of file search calls made",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "mcp_calls": {
                "display_name": "MCP Calls",
                "short_desc": "Number of MCP calls",
                "desc": "Number of MCP (Model Context Protocol) calls made",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "web_search_calls": {
                "display_name": "Web Search Calls",
                "short_desc": "Number of web search calls",
                "desc": "Number of web search calls made",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "x_search_calls": {
                "display_name": "X Search Calls",
                "short_desc": "Number of X search calls",
                "desc": "Number of X (formerly Twitter) search calls made",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    constructor() : HashDataType("GrokServerToolUsage") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type, "GrokServerToolUsage") {
        addQoreFields(Fields);
    }
}

public const GrokServerToolUsageDataType = new GrokServerToolUsageDataType();

#! Response usage data type for the Responses API
public class GrokResponseUsageDataType inherits HashDataType {
    public {
        const Fields = {
            "input_tokens": {
                "display_name": "Input Tokens",
                "short_desc": "Number of input tokens",
                "desc": "Number of tokens in the input",
                "type": AbstractDataProviderTypeMap."int",
            },
            "output_tokens": {
                "display_name": "Output Tokens",
                "short_desc": "Number of output tokens",
                "desc": "Number of tokens in the generated output",
                "type": AbstractDataProviderTypeMap."int",
            },
            "total_tokens": {
                "display_name": "Total Tokens",
                "short_desc": "Total number of tokens used",
                "desc": "Total number of tokens used in the request (input + output)",
                "type": AbstractDataProviderTypeMap."int",
            },
            "input_tokens_details": {
                "display_name": "Input Token Details",
                "short_desc": "Detailed breakdown of input tokens",
                "desc": "Detailed breakdown of input token usage including cached tokens",
                "type": new GrokInputTokenDetailsDataType(AutoHashOrNothingType),
            },
            "output_tokens_details": {
                "display_name": "Output Token Details",
                "short_desc": "Detailed breakdown of output tokens",
                "desc": "Detailed breakdown of output token usage including reasoning tokens",
                "type": new GrokOutputTokenDetailsDataType(AutoHashOrNothingType),
            },
            "server_side_tool_usage_details": {
                "display_name": "Server Tool Usage Details",
                "short_desc": "Breakdown of server-side tool usage",
                "desc": "Detailed breakdown of server-side tool calls made during the response",
                "type": new GrokServerToolUsageDataType(AutoHashOrNothingType),
            },
            "cost_in_nano_usd": {
                "display_name": "Cost in Nano USD",
                "short_desc": "Billing cost in nano USD",
                "desc": "Billing cost in nano USD (billionths of a dollar)",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "cost_in_usd_ticks": {
                "display_name": "Cost in USD Ticks",
                "short_desc": "Billing cost in ticks (10 billion per dollar)",
                "desc": "Billing cost in USD ticks (10 billion ticks per dollar)",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "num_server_side_tools_used": {
                "display_name": "Server Tools Used",
                "short_desc": "Number of server-side tools used",
                "desc": "Number of server-side tools used during the response",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "num_sources_used": {
                "display_name": "Sources Used",
                "short_desc": "Number of sources used",
                "desc": "Number of live search sources utilized",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    constructor() : HashDataType("GrokResponseUsage") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type, "GrokResponseUsage") {
        addQoreFields(Fields);
    }
}

public const GrokResponseUsageDataType = new GrokResponseUsageDataType();

#! Text format configuration data type for the Responses API
public class GrokTextFormatConfigDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "type": {
                "display_name": "Type",
                "short_desc": "The response format type",
                "desc": "The response format type",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Text",
                        "short_desc": "Standard text output (default)",
                        "desc": "Standard text output (default)",
                        "value": "text",
                    },
                    <AllowedValueInfo>{
                        "display_name": "JSON Schema",
                        "short_desc": "Structured output conforming to a JSON schema",
                        "desc": "Structured output conforming to a JSON schema",
                        "value": "json_schema",
                    },
                    <AllowedValueInfo>{
                        "display_name": "JSON Object",
                        "short_desc": "Valid JSON output without a specific schema",
                        "desc": "Valid JSON output without a specific schema",
                        "value": "json_object",
                    },
                ),
                "default_value": "text",
            },
            "name": {
                "display_name": "Schema Name",
                "short_desc": "Identifier for the JSON schema (required for json_schema type)",
                "desc": "Identifier for the JSON schema. Required when `type` is `json_schema`. "
                    "Must be alphanumeric with underscores/dashes, max 64 characters",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "movie_response",
            },
            "schema": {
                "display_name": "Schema",
                "short_desc": "JSON Schema object defining the output structure",
                "desc": "The JSON Schema object defining the output structure. Required when `type` is "
                    "`json_schema`.\n\n"
                    "**Example**: `{\"type\": \"object\", \"properties\": {\"title\": {\"type\": \"string\"}, "
                    "\"rating\": {\"type\": \"number\"}}, \"required\": [\"title\", \"rating\"], "
                    "\"additionalProperties\": false}`",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Human-readable description of the schema's purpose",
                "desc": "Human-readable description of the schema's purpose",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "strict": {
                "display_name": "Strict",
                "short_desc": "Strictly follow the exact schema",
                "desc": "When `true`, the model strictly follows the exact schema definition",
                "type": AbstractDataProviderTypeMap."*bool",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("GrokTextFormatConfig") {
        addQoreFields(Fields);
    }

    #! Creates the type with a specific base type
    constructor(Type base_type) : HashDataType(base_type, "GrokTextFormatConfig") {
        addQoreFields(Fields);
    }
}

#! Constant for text format config data type
public const GrokTextFormatConfigDataType = new GrokTextFormatConfigDataType();

#! Text format data type for the Responses API
public class GrokTextFormatDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "format": {
                "display_name": "Format",
                "short_desc": "Output format configuration",
                "desc": "Output format configuration. Set `type` to `json_schema` for structured outputs "
                    "or `json_object` for free-form JSON",
                "type": new GrokTextFormatConfigDataType(AutoHashOrNothingType),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("GrokTextFormat") {
        addQoreFields(Fields);
    }

    #! Creates the type with a specific base type
    constructor(Type base_type) : HashDataType(base_type, "GrokTextFormat") {
        addQoreFields(Fields);
    }
}

#! Constant for text format data type
public const GrokTextFormatDataType = new GrokTextFormatDataType();

#! Input item data type for the Responses API
/** Provides a flat structure with text and image fields. If both are provided, they are split into
    separate content parts in the API request. The content type is determined automatically.
*/
public class GrokResponseInputItemDataType inherits HashDataType {
    public {
        const Fields = {
            "role": {
                "display_name": "Role",
                "short_desc": "The role of the message author",
                "desc": "The role of the message author",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "User",
                        "short_desc": "User message",
                        "desc": "A message from the user",
                        "value": "user",
                    },
                    <AllowedValueInfo>{
                        "display_name": "System",
                        "short_desc": "System instructions",
                        "desc": "System instructions (must be first if present)",
                        "value": "system",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Developer",
                        "short_desc": "Developer instructions (alias for system)",
                        "desc": "Developer instructions (alias for system)",
                        "value": "developer",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Assistant",
                        "short_desc": "Model response for conversation continuation",
                        "desc": "A previous model response for conversation continuation",
                        "value": "assistant",
                    },
                ),
                "default_value": "user",
            },
            "text": {
                "display_name": "Text",
                "short_desc": "The text content of the message",
                "desc": "The text content of the message",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "Tell me about quantum computing",
                "attr": {
                    "required_groups": ("message_content",),
                },
            },
            "image_url": {
                "display_name": "Image URL",
                "short_desc": "URL or base64 data URL for an image",
                "desc": "URL or base64 data URL for an image input. Supported formats: JPEG, PNG. "
                    "Max size: 20 MiB. If both text and image are provided, they are sent together "
                    "as a multimodal message",
                "type": AbstractDataProviderTypeMap."*string",
                "attr": {
                    "required_groups": ("message_content",),
                },
            },
        };
    }

    constructor() : HashDataType("GrokResponseInputItem") {
        addQoreFields(Fields);
    }
}

public const GrokResponseInputItemDataType = new GrokResponseInputItemDataType();

#! The responses API request type
public class GrokResponseCreateRequestType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "input": {
                "display_name": "Input",
                "short_desc": "Text or message input to the model",
                "desc": "The input content for the response. Each item has a role, text, and optional image. "
                    "For a simple text prompt, a single item with text is sufficient",
                "type": new SoftListDataType("GrokInputList", GrokResponseInputItemDataType),
            },
            "model": {
                "display_name": "Model ID",
                "short_desc": "Model to use for the response",
                "desc": "Model to use for the response (e.g., `grok-3`, `grok-4`)",
                "type": AbstractDataProviderTypeMap."string",
                "attr": {
                    "ref_data": "models",
                },
            },
            "instructions": {
                "display_name": "Instructions",
                "short_desc": "System prompt instructions",
                "desc": "A system (or developer) message inserted at the top of the model's context",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "previous_response_id": {
                "display_name": "Previous Response ID",
                "short_desc": "Prior response ID for continuation",
                "desc": "The unique ID of a previous response to use as context for continuation without "
                    "repeating input",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "context_key": {
                "display_name": "Context Key",
                "short_desc": "Key for conversation context store",
                "desc": "An opaque identifier used to load and save conversation context. Only used when a "
                    "conversation context store is configured",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "max_output_tokens": {
                "display_name": "Max Output Tokens",
                "short_desc": "Maximum tokens to generate including reasoning",
                "desc": "Maximum number of tokens that can be generated in a response, including both output "
                    "and reasoning tokens",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "max_turns": {
                "display_name": "Max Turns",
                "short_desc": "Maximum agentic tool-calling rounds",
                "desc": "Maximum number of agentic tool-calling iterations",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "temperature": {
                "display_name": "Sampling Temperature",
                "short_desc": "Sampling temperature (0-2)",
                "desc": "Sampling temperature between 0 and 2. Higher values make output more random",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "top_p": {
                "display_name": "Top P",
                "short_desc": "Nucleus sampling threshold (0-1)",
                "desc": "Nucleus sampling threshold where the model considers tokens comprising the top_p "
                    "probability mass",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "reasoning": {
                "display_name": "Reasoning",
                "short_desc": "Reasoning configuration",
                "desc": "Reasoning configuration with effort level and summary type. Only supported by "
                    "reasoning-capable models (e.g., `grok-3-mini`)",
                "type": new GrokReasoningConfigDataType(AutoHashOrNothingType),
            },
            "stream": {
                "display_name": "Stream?",
                "short_desc": "Enable server-sent events streaming",
                "desc": "If `true`, the response will be streamed using server-sent events",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "store": {
                "display_name": "Store?",
                "short_desc": "Persist input and response for 30 days",
                "desc": "If `true`, the input and response will be stored for 30 days before permanent deletion",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "tools": {
                "display_name": "Tools",
                "short_desc": "Available tools (max 128)",
                "desc": "A list of tools available to the model, including functions and web search",
                "type": new ListDataType("*GrokResponseToolList", GrokResponseToolDataType, True),
            },
            "tool_choice": {
                "display_name": "Tool Choice",
                "short_desc": "Tool invocation control",
                "desc": "Controls which tools the model may use.\n\n"
                    "Use one of the predefined values, or enter a custom value to force a specific function "
                    "(e.g., a function name)",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "None",
                        "short_desc": "The model will not call any tool",
                        "desc": "The model will not call any tool and instead generates a message",
                        "value": "none",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Auto",
                        "short_desc": "The model decides whether to call tools",
                        "desc": "The model can pick between generating a message or calling one or more tools",
                        "value": "auto",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Required",
                        "short_desc": "The model must call one or more tools",
                        "desc": "The model must call one or more tools",
                        "value": "required",
                    },
                ),
                "allowed_values_creatable": True,
            },
            "parallel_tool_calls": {
                "display_name": "Parallel Tool Calls",
                "short_desc": "Allow concurrent tool execution",
                "desc": "Whether to allow parallel tool execution",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "search_parameters": {
                "display_name": "Search Parameters",
                "short_desc": "Web search configuration",
                "desc": "Configuration for web search integration",
                "type": GrokSearchParametersType.getOrNothingType(),
            },
            "user": {
                "display_name": "User",
                "short_desc": "End-user identifier",
                "desc": "A unique identifier representing your end-user for abuse monitoring",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "metadata": {
                "display_name": "Metadata",
                "short_desc": "Additional metadata key-value pairs",
                "desc": "Set of key-value pairs to attach to the response. Maximum 16 pairs; keys up to 64 "
                    "characters, values up to 512 characters",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "text": {
                "display_name": "Text Format",
                "short_desc": "Configuration for text response format",
                "desc": "Configuration for text response format from the model. Use to specify structured output "
                    "via JSON schema",
                "type": new GrokTextFormatDataType(AutoHashOrNothingType),
            },
            "include": {
                "display_name": "Include",
                "short_desc": "Additional output data to include in the response",
                "desc": "Additional output data to include in the model response",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "prompt_cache_key": {
                "display_name": "Prompt Cache Key",
                "short_desc": "A key for caching prompts to optimize cache hit rates",
                "desc": "Used by xAI to cache responses for similar requests to optimize cache hit rates",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "logprobs": {
                "display_name": "Log Probabilities",
                "short_desc": "Whether to return log probabilities of output tokens",
                "desc": "Whether to return log probabilities of the output tokens. If `true`, returns the log "
                    "probabilities of each output token",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "top_logprobs": {
                "display_name": "Top Log Probabilities",
                "short_desc": "Number of most likely tokens to return at each position",
                "desc": "An integer between `0` and `20` specifying the number of most likely tokens to return "
                    "at each token position, each with an associated log probability",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "background": {
                "display_name": "Background",
                "short_desc": "Process the response asynchronously in the background",
                "desc": "If `true`, the response will be processed asynchronously in the background. "
                    "Use the `get-response` action with the returned response ID to poll for the result",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "service_tier": {
                "display_name": "Service Tier",
                "short_desc": "Specifies the latency tier to use for processing",
                "desc": "Specifies the latency tier to use for processing the request",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Auto",
                        "short_desc": "Automatic tier selection",
                        "desc": "Automatic tier selection based on load",
                        "value": "auto",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Default",
                        "short_desc": "Default processing tier",
                        "desc": "Default processing tier",
                        "value": "default",
                    },
                ),
            },
            "truncation": {
                "display_name": "Truncation",
                "short_desc": "Truncation strategy for long conversations",
                "desc": "Truncation strategy when the context exceeds the model's maximum context length. "
                    "`auto` truncates from the middle of the conversation to fit within the context window",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Auto",
                        "short_desc": "Automatically truncate long conversations",
                        "desc": "Automatically truncate from the middle of the conversation",
                        "value": "auto",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Disabled",
                        "short_desc": "Do not truncate; return error if too long",
                        "desc": "Do not truncate; return an error if the context exceeds the limit",
                        "value": "disabled",
                    },
                ),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("GrokResponseCreateRequest") {
        addQoreFields(Fields);
    }
}

#! Constant for the response create request type
public const GrokResponseCreateRequestType = new GrokResponseCreateRequestType();

#! Incomplete details data type for the Responses API
public class GrokIncompleteDetailsDataType inherits HashDataType {
    public {
        const Fields = {
            "reason": {
                "display_name": "Reason",
                "short_desc": "Why the response was incomplete",
                "desc": "The reason the response was incomplete: `max_output_tokens` or `content_filter`",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Max Output Tokens",
                        "short_desc": "Response truncated due to token limit",
                        "desc": "Response truncated due to token limit",
                        "value": "max_output_tokens",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Content Filter",
                        "short_desc": "Response truncated due to content filter",
                        "desc": "Response truncated due to content filter",
                        "value": "content_filter",
                    },
                ),
            },
        };
    }

    constructor() : HashDataType("GrokIncompleteDetails") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type, "GrokIncompleteDetails") {
        addQoreFields(Fields);
    }
}

public const GrokIncompleteDetailsDataType = new GrokIncompleteDetailsDataType();

#! Conversation context data type for follow-up requests
public class GrokConversationContextDataType inherits HashDataType {
    public {
        const Fields = {
            "previous_response_id": {
                "display_name": "Previous Response ID",
                "short_desc": "ID of the previous response for continuation",
                "desc": "The unique ID of the previous response, used for conversation continuation",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "conversation_id": {
                "display_name": "Conversation ID",
                "short_desc": "Conversation identifier",
                "desc": "An identifier for the conversation session",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "metadata": {
                "display_name": "Metadata",
                "short_desc": "Additional conversation metadata",
                "desc": "Additional metadata associated with the conversation",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    constructor() : HashDataType("GrokConversationContext") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type, "GrokConversationContext") {
        addQoreFields(Fields);
    }
}

public const GrokConversationContextDataType = new GrokConversationContextDataType();

#! Output content data type for the Responses API
public class GrokResponseOutputContentDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "display_name": "Content Type",
                "short_desc": "The output content type",
                "desc": "The output content type",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Output Text",
                        "short_desc": "Text output from the model",
                        "desc": "Text output from the model",
                        "value": "output_text",
                    },
                ),
                "allowed_values_creatable": True,
            },
            "text": {
                "display_name": "Text",
                "short_desc": "Text output from the model",
                "desc": "Text output from the model",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "annotations": {
                "display_name": "Annotations",
                "short_desc": "Annotations for output text",
                "desc": "Annotations for output text such as URL citations from web search results",
                "type": new ListDataType("*GrokOutputAnnotations", GrokUrlCitationAnnotationDataType, True),
            },
        };
    }

    constructor() : HashDataType("GrokResponseOutputContent") {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

public const GrokResponseOutputContentDataType = new GrokResponseOutputContentDataType();

#! URL citation annotation data type for the Responses API
public class GrokUrlCitationAnnotationDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "display_name": "Type",
                "short_desc": "The annotation type",
                "desc": "The annotation type (e.g., `url_citation`)",
                "type": AbstractDataProviderTypeMap."string",
            },
            "title": {
                "display_name": "Title",
                "short_desc": "Citation label or title",
                "desc": "Citation label or title for the annotation",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "url": {
                "display_name": "URL",
                "short_desc": "The source URL",
                "desc": "The source URL for the citation",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "start_index": {
                "display_name": "Start Index",
                "short_desc": "Start character position in the text",
                "desc": "Character position where the citation reference starts in the text",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "end_index": {
                "display_name": "End Index",
                "short_desc": "End character position in the text",
                "desc": "Character position where the citation reference ends in the text",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    constructor() : HashDataType("GrokUrlCitationAnnotation") {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

public const GrokUrlCitationAnnotationDataType = new GrokUrlCitationAnnotationDataType();

#! Output item data type for the Responses API
public class GrokResponseOutputItemDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "display_name": "Output Type",
                "short_desc": "The type of output item",
                "desc": "The type of output item",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Message",
                        "short_desc": "A message output item",
                        "desc": "A message output item",
                        "value": "message",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Function Call",
                        "short_desc": "A function call output item",
                        "desc": "A function call output item",
                        "value": "function_call",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Function Call Output",
                        "short_desc": "A function call result output item",
                        "desc": "A function call result output item",
                        "value": "function_call_output",
                    },
                ),
                "allowed_values_creatable": True,
            },
            "id": {
                "display_name": "ID",
                "short_desc": "Output item ID",
                "desc": "Output item ID",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Output item status",
                "desc": "Output item status",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "role": {
                "display_name": "Role",
                "short_desc": "Role for message outputs",
                "desc": "Role for message outputs",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "content": {
                "display_name": "Content",
                "short_desc": "Output content parts",
                "desc": "Output content parts",
                "type": new ListDataType("*GrokResponseOutputContent", GrokResponseOutputContentDataType, True),
            },
            "name": {
                "display_name": "Function Name",
                "short_desc": "Function name for function call outputs",
                "desc": "Function name for function call outputs",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "arguments": {
                "display_name": "Function Arguments",
                "short_desc": "Function call arguments as JSON string",
                "desc": "Function call arguments serialized as a JSON string",
                "type": AbstractDataProviderTypeMap."any",
            },
            "call_id": {
                "display_name": "Call ID",
                "short_desc": "Function call ID",
                "desc": "Function call ID",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "output": {
                "display_name": "Function Output",
                "short_desc": "Output from a function call",
                "desc": "Output from a function call",
                "type": AbstractDataProviderTypeMap."any",
            },
        };
    }

    constructor() : HashDataType("GrokResponseOutputItem") {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

public const GrokResponseOutputItemDataType = new GrokResponseOutputItemDataType();

#! The responses API response type
public class GrokResponseType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "The unique response identifier",
                "desc": "The unique response identifier",
                "type": AbstractDataProviderTypeMap."string",
            },
            "object": {
                "display_name": "Object",
                "short_desc": "The object type (always 'response')",
                "desc": "The object type (always `response`)",
                "type": AbstractDataProviderTypeMap."string",
            },
            "created_at": {
                "display_name": "Created At",
                "short_desc": "Unix timestamp of creation",
                "desc": "Unix timestamp in seconds when the response was created",
                "type": AbstractDataProviderTypeMap."int",
            },
            "completed_at": {
                "display_name": "Completed At",
                "short_desc": "Completion timestamp",
                "desc": "Unix timestamp when the response completed",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "model": {
                "display_name": "Model",
                "short_desc": "Model used for the response",
                "desc": "Model used to generate the response",
                "type": AbstractDataProviderTypeMap."string",
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Response status",
                "desc": "Response status: `completed`, `in_progress`, or `incomplete`",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Completed",
                        "short_desc": "Response completed successfully",
                        "desc": "Response completed successfully",
                        "value": "completed",
                    },
                    <AllowedValueInfo>{
                        "display_name": "In Progress",
                        "short_desc": "Response still being generated",
                        "desc": "Response still being generated",
                        "value": "in_progress",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Incomplete",
                        "short_desc": "Response generation was incomplete",
                        "desc": "Response generation was incomplete",
                        "value": "incomplete",
                    },
                ),
            },
            "output": {
                "display_name": "Output",
                "short_desc": "Generated response messages",
                "desc": "The generated response output items including messages, function calls, and results",
                "type": new ListDataType("*GrokResponseOutputItems", GrokResponseOutputItemDataType, True),
            },
            "output_text": {
                "display_name": "Output Text",
                "short_desc": "Extracted text from output",
                "desc": "Extracted text content from the output messages",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tool_calls": {
                "display_name": "Tool Calls",
                "short_desc": "Tool calls from output",
                "desc": "Extracted function call items from the output",
                "type": new ListDataType("*GrokExtractedToolCalls", GrokResponseOutputItemDataType, True),
            },
            "tool_results": {
                "display_name": "Tool Results",
                "short_desc": "Tool results from output",
                "desc": "Extracted function call output items from the output",
                "type": new ListDataType("*GrokExtractedToolResults", GrokResponseOutputItemDataType, True),
            },
            "usage": {
                "display_name": "Usage",
                "short_desc": "Token usage statistics",
                "desc": "Token and cost accounting for the response",
                "type": new GrokResponseUsageDataType(AutoHashOrNothingType),
            },
            "incomplete_details": {
                "display_name": "Incomplete Details",
                "short_desc": "Reason for incomplete response",
                "desc": "Details about why the response was incomplete, if applicable",
                "type": new GrokIncompleteDetailsDataType(AutoHashOrNothingType),
            },
            "conversation_context": {
                "display_name": "Conversation Context",
                "short_desc": "Context for follow-up requests",
                "desc": "Conversation context including `previous_response_id` for continuation",
                "type": new GrokConversationContextDataType(AutoHashOrNothingType),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("GrokResponse") {
        addQoreFields(Fields);
    }
}

#! Constant for the response type
public const GrokResponseType = new GrokResponseType();
}
