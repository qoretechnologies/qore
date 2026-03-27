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
                "desc": "The parameters the function accepts, described as a JSON Schema object",
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

#! The responses API request type
public class GrokResponseCreateRequestType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "input": {
                "display_name": "Input",
                "short_desc": "Text or message array input",
                "desc": "The input content for the response. Can be a string or an array of message objects",
                "type": AbstractDataProviderTypeMap."any",
                "example_value": "Tell me about quantum computing",
            },
            "model": {
                "display_name": "Model ID",
                "short_desc": "Model to use for the response",
                "desc": "Model to use for the response (e.g., grok-3, grok-4)",
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
                "desc": "Controls which tools the model may use",
                "type": AbstractDataProviderTypeMap."any",
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
                "short_desc": "Additional metadata",
                "desc": "Additional metadata to attach to the response",
                "type": AbstractDataProviderTypeMap."*hash",
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
                "desc": "The object type (always 'response')",
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
                "desc": "The generated response messages and content",
                "type": AbstractDataProviderTypeMap."*list",
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
                "desc": "Extracted tool calls from the output",
                "type": AbstractDataProviderTypeMap."*list",
            },
            "tool_results": {
                "display_name": "Tool Results",
                "short_desc": "Tool results from output",
                "desc": "Extracted tool results from the output",
                "type": AbstractDataProviderTypeMap."*list",
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
