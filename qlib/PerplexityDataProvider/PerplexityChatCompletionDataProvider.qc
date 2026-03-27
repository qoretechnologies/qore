# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore PerplexityDataProvider module definition

/** PerplexityChatCompletionDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the PerplexityDataProvider module
public namespace PerplexityDataProvider {
#! The Perplexity chat completion data provider class
public class PerplexityChatCompletionDataProvider inherits PerplexityDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "multi-chat",
            "type": "PerplexityChatCompletionDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = PerplexityChatCompletionRequestType;

        #! Response type
        const ResponseType = PerplexityChatCompletionResponseType;
    }

    #! Creates the object from the arguments
    constructor(*RestClient rest) : PerplexityDataProviderCommon(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("POST", "chat/completions", req).body;
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Chat completion request message
public class PerplexityMessageDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "role": {
                "display_name": "Role",
                "short_desc": "The role of the author of this message",
                "desc": "The role of the author of this message",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "User",
                        "short_desc": "The author of the message is the user",
                        "desc": "The author of the message is the user",
                        "value": "user",
                    },
                    <AllowedValueInfo>{
                        "display_name": "System",
                        "short_desc": "The author of the message is the system",
                        "desc": "The author of the message is the system",
                        "value": "system",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Assistant",
                        "short_desc": "The author of the message is an assistant",
                        "desc": "The author of the message is an assistant",
                        "value": "assistant",
                    },
                ),
                "default_value": "user",
            },
            "content": {
                "display_name": "Content",
                "short_desc": "The contents of the message",
                "desc": "The contents of the message",
                "type": AbstractDataProviderTypeMap."string",
                "example_value": "What is the weather today?",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("PerplexityMessage") {
        addQoreFields(Fields);
    }
}

#! Chat completion message constant
public const PerplexityMessageDataType = new PerplexityMessageDataType();

#! The web search options data type
public class PerplexityWebSearchOptionsDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "search_context_size": {
                "display_name": "Search Context Size",
                "short_desc": "Amount of web data retrieved per query",
                "desc": "Controls amount of web data retrieved per query. Higher values provide more context but use "
                    "more tokens",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Low",
                        "short_desc": "Minimal web context",
                        "desc": "Minimal web context for faster, cheaper responses",
                        "value": "low",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Medium",
                        "short_desc": "Moderate web context",
                        "desc": "Balanced web context for most use cases",
                        "value": "medium",
                    },
                    <AllowedValueInfo>{
                        "display_name": "High",
                        "short_desc": "Maximum web context",
                        "desc": "Maximum web context for comprehensive responses",
                        "value": "high",
                    },
                ),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("PerplexityWebSearchOptions") {
        addQoreFields(Fields);
    }
}

#! Constant for the web search options data type
public const PerplexityWebSearchOptionsDataType = new PerplexityWebSearchOptionsDataType();

#! The JSON schema options data type for structured output
public class PerplexityJsonSchemaDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "name": {
                "display_name": "Schema Name",
                "short_desc": "A name for the JSON schema",
                "desc": "A name for the JSON schema used to identify the output structure",
                "type": AbstractDataProviderTypeMap."string",
            },
            "schema": {
                "display_name": "JSON Schema",
                "short_desc": "The JSON schema object defining the output structure",
                "desc": "The JSON schema object defining the structure of the output. This follows standard JSON Schema "
                    "specification",
                "type": AbstractDataProviderTypeMap."hash",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("PerplexityJsonSchema") {
        addQoreFields(Fields);
    }
}

#! Constant for the JSON schema data type
public const PerplexityJsonSchemaDataType = new PerplexityJsonSchemaDataType();

#! The response format data type for structured output
public class PerplexityResponseFormatDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "type": {
                "display_name": "Format Type",
                "short_desc": "The format type for the response",
                "desc": "The format type for the response. Use `json_schema` for structured JSON output or `text` for "
                    "plain text",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "JSON Schema",
                        "short_desc": "Structured JSON output",
                        "desc": "Enable structured JSON output conforming to a schema",
                        "value": "json_schema",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Text",
                        "short_desc": "Plain text output",
                        "desc": "Plain text output (default behavior)",
                        "value": "text",
                    },
                ),
            },
            "json_schema": {
                "display_name": "JSON Schema",
                "short_desc": "Schema definition for structured output",
                "desc": "The JSON schema definition when `type` is set to `json_schema`. Defines the structure that "
                    "the model output must conform to",
                "type": PerplexityJsonSchemaDataType.getOrNothingType(),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("PerplexityResponseFormat") {
        addQoreFields(Fields);
    }
}

#! Constant for the response format data type
public const PerplexityResponseFormatDataType = new PerplexityResponseFormatDataType();

#! The chat completion request type
public class PerplexityChatCompletionRequestType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "model": {
                "display_name": "Model ID",
                "short_desc": "Model to use for the chat completion",
                "desc": "Model to use for the chat completion",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Sonar",
                        "short_desc": "Lightweight, cost-effective search model",
                        "desc": "Lightweight, cost-effective search model based on Llama 3.3 70B",
                        "value": "sonar",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Sonar Pro",
                        "short_desc": "Advanced search with deeper retrieval",
                        "desc": "Advanced search with deeper retrieval and double citations",
                        "value": "sonar-pro",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Sonar Reasoning Pro",
                        "short_desc": "Chain of Thought reasoning model",
                        "desc": "Chain of Thought reasoning model based on DeepSeek-R1 with multi-query search",
                        "value": "sonar-reasoning-pro",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Sonar Deep Research",
                        "short_desc": "Expert-level autonomous research",
                        "desc": "Expert-level research with autonomous source evaluation and report generation",
                        "value": "sonar-deep-research",
                    },
                ),
            },
            "messages": {
                "display_name": "Messages",
                "short_desc": "A list of messages comprising the conversation so far",
                "desc": "A list of messages comprising the conversation so far",
                "type": new ListDataType("Messages", PerplexityMessageDataType),
            },
            "max_tokens": {
                "display_name": "Max Tokens",
                "short_desc": "The maximum number of tokens to generate",
                "desc": "The maximum number of tokens that can be generated in the chat completion. Up to 128,000 "
                    "tokens supported",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "temperature": {
                "display_name": "Sampling Temperature",
                "short_desc": "The sampling temperature to use, between 0 and 2",
                "desc": "What sampling temperature to use, between `0` and `2`. Higher values like `0.8` will make "
                    "the output more random, while lower values like `0.2` will make it more focused and "
                    "deterministic",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "top_p": {
                "display_name": "Top P",
                "short_desc": "Nucleus sampling value",
                "desc": "An alternative to sampling with temperature, called nucleus sampling, where the model "
                    "considers the results of the tokens with `top_p` probability mass. So `0.1` means only the "
                    "tokens comprising the top 10% probability mass are considered",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "stream": {
                "display_name": "Stream?",
                "short_desc": "Enable streaming via Server-Sent Events",
                "desc": "If set to `True`, partial message deltas will be sent as data-only server-sent events as "
                    "they become available, with the stream terminated by a `data: [DONE]` message",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "search_mode": {
                "display_name": "Search Mode",
                "short_desc": "The type of search to perform",
                "desc": "The type of search to perform for grounding the response",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Web",
                        "short_desc": "General web search",
                        "desc": "General web search for grounding",
                        "value": "web",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Academic",
                        "short_desc": "Academic and research papers",
                        "desc": "Search academic and research papers",
                        "value": "academic",
                    },
                    <AllowedValueInfo>{
                        "display_name": "SEC",
                        "short_desc": "SEC filings",
                        "desc": "Search US SEC regulatory filings",
                        "value": "sec",
                    },
                ),
            },
            "disable_search": {
                "display_name": "Disable Search?",
                "short_desc": "Restrict response to training data only",
                "desc": "If set to `True`, the model will not search the web and will only use its training data to "
                    "generate a response",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "return_images": {
                "display_name": "Return Images?",
                "short_desc": "Include image results from search",
                "desc": "If set to `True`, image results from the web search will be included in the response",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "return_related_questions": {
                "display_name": "Return Related Questions?",
                "short_desc": "Generate follow-up question suggestions",
                "desc": "If set to `True`, the response will include suggested follow-up questions",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "search_domain_filter": {
                "display_name": "Search Domain Filter",
                "short_desc": "Domain allowlist or denylist for search (max 20)",
                "desc": "A list of domains to include or exclude from search results. Prefix with `-` to exclude. "
                    "Maximum of 20 domains",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "search_language_filter": {
                "display_name": "Search Language Filter",
                "short_desc": "ISO 639-1 language codes to filter search results (max 10)",
                "desc": "A list of ISO 639-1 language codes to filter search results. Maximum of 10 languages",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "search_recency_filter": {
                "display_name": "Search Recency Filter",
                "short_desc": "Filter search results by recency",
                "desc": "Filter search results to only include sources from a specific time period",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Last Hour",
                        "short_desc": "Sources from the last hour",
                        "desc": "Only include sources from the last hour",
                        "value": "hour",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Last Day",
                        "short_desc": "Sources from the last day",
                        "desc": "Only include sources from the last day",
                        "value": "day",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Last Week",
                        "short_desc": "Sources from the last week",
                        "desc": "Only include sources from the last week",
                        "value": "week",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Last Month",
                        "short_desc": "Sources from the last month",
                        "desc": "Only include sources from the last month",
                        "value": "month",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Last Year",
                        "short_desc": "Sources from the last year",
                        "desc": "Only include sources from the last year",
                        "value": "year",
                    },
                ),
            },
            "response_format": {
                "display_name": "Response Format",
                "short_desc": "Specify the output format (e.g., JSON schema)",
                "desc": "An object specifying the format that the model must output. Set `type` to `json_schema` and "
                    "provide a `json_schema` object to enable structured JSON output",
                "type": PerplexityResponseFormatDataType.getOrNothingType(),
            },
            "web_search_options": {
                "display_name": "Web Search Options",
                "short_desc": "Advanced web search configuration",
                "desc": "Advanced web search configuration options such as search context size",
                "type": PerplexityWebSearchOptionsDataType.getOrNothingType(),
            },
            "stop": {
                "display_name": "Stop Sequences",
                "short_desc": "Sequences where the API will stop generating tokens",
                "desc": "Up to 4 sequences where the API will stop generating further tokens",
                "type": AbstractDataProviderTypeMap."any",
            },
            "reasoning_effort": {
                "display_name": "Reasoning Effort",
                "short_desc": "Control the reasoning effort for reasoning models",
                "desc": "Controls the depth of reasoning for `sonar-reasoning` and `sonar-reasoning-pro` models",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Minimal",
                        "short_desc": "Minimal reasoning effort",
                        "desc": "Minimal reasoning effort for fast responses",
                        "value": "minimal",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Low",
                        "short_desc": "Low reasoning effort",
                        "desc": "Low reasoning effort",
                        "value": "low",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Medium",
                        "short_desc": "Medium reasoning effort",
                        "desc": "Medium reasoning effort (default)",
                        "value": "medium",
                    },
                    <AllowedValueInfo>{
                        "display_name": "High",
                        "short_desc": "High reasoning effort",
                        "desc": "Maximum reasoning effort for thorough analysis",
                        "value": "high",
                    },
                ),
            },
            "stream_mode": {
                "display_name": "Stream Mode",
                "short_desc": "Controls the streaming format",
                "desc": "Controls the streaming format. `full` includes metadata inline with no separate reasoning "
                    "events. `concise` provides separate reasoning events",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Full",
                        "short_desc": "Metadata inline, no reasoning events",
                        "desc": "Metadata inline with no separate reasoning events",
                        "value": "full",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Concise",
                        "short_desc": "Separate reasoning events",
                        "desc": "Provides separate reasoning events during streaming",
                        "value": "concise",
                    },
                ),
            },
            "search_after_date_filter": {
                "display_name": "Search After Date",
                "short_desc": "Only include sources published after this date",
                "desc": "Only include sources published after this date. Format: `MM/DD/YYYY`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "search_before_date_filter": {
                "display_name": "Search Before Date",
                "short_desc": "Only include sources published before this date",
                "desc": "Only include sources published before this date. Format: `MM/DD/YYYY`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "last_updated_after_filter": {
                "display_name": "Last Updated After",
                "short_desc": "Only include sources updated after this date",
                "desc": "Only include sources that were last updated after this date. Format: `MM/DD/YYYY`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "last_updated_before_filter": {
                "display_name": "Last Updated Before",
                "short_desc": "Only include sources updated before this date",
                "desc": "Only include sources that were last updated before this date. Format: `MM/DD/YYYY`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "enable_search_classifier": {
                "display_name": "Enable Search Classifier?",
                "short_desc": "Use a classifier to determine if search is needed",
                "desc": "If set to `True`, uses a classifier to determine whether a web search is needed for the "
                    "query before performing one",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "image_format_filter": {
                "display_name": "Image Format Filter",
                "short_desc": "Filter image results by format",
                "desc": "A list of image formats to include in image results (e.g., `png`, `jpg`). Only applies "
                    "when `return_images` is set to `True`",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "image_domain_filter": {
                "display_name": "Image Domain Filter",
                "short_desc": "Limit image sources to specific domains",
                "desc": "A list of domains to limit image search results to. Only applies when `return_images` is "
                    "set to `True`",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "language_preference": {
                "display_name": "Language Preference",
                "short_desc": "ISO 639-1 language code for the response",
                "desc": "An ISO 639-1 language code specifying the preferred language for the response",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("PerplexityChatCompletionRequest") {
        addQoreFields(Fields);
    }
}

#! The chat completion request type constant
public const PerplexityChatCompletionRequestType = new PerplexityChatCompletionRequestType();

#! Chat completion response message
public class PerplexityCompletionMessageDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "role": {
                "display_name": "Role",
                "short_desc": "The role of the author of this message",
                "desc": "The role of the author of this message",
                "type": AbstractDataProviderTypeMap."string",
            },
            "content": {
                "display_name": "Content",
                "short_desc": "The contents of the message",
                "desc": "The contents of the message",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("PerplexityCompletionMessage") {
        addQoreFields(Fields);
    }
}

#! Chat completion response message constant
public const PerplexityCompletionMessageDataType = new PerplexityCompletionMessageDataType();

#! Chat completion response choice type
public class PerplexityChoiceDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "index": {
                "display_name": "Index",
                "short_desc": "The index of the chat completion choice",
                "desc": "The index of the chat completion choice",
                "type": AbstractDataProviderTypeMap."int",
            },
            "message": {
                "display_name": "Message",
                "short_desc": "Information comprising a message",
                "desc": "Information comprising a message",
                "type": PerplexityCompletionMessageDataType,
            },
            "finish_reason": {
                "display_name": "Finish Reason",
                "short_desc": "The reason the model stopped generating tokens",
                "desc": "The reason the model stopped generating tokens. This will be `stop` if the model hit a "
                    "natural stop point or a provided stop sequence, `length` if the maximum number of tokens "
                    "specified in the request was reached, or `content_filter` if content was omitted due to a "
                    "content filter",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("PerplexityChoice") {
        addQoreFields(Fields);
    }
}

#! Constant for the chat completion response choice type
public const PerplexityChoiceDataType = new PerplexityChoiceDataType();

#! The chat completion usage type
public class PerplexityUsageDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "prompt_tokens": {
                "display_name": "Prompt Tokens",
                "short_desc": "Number of tokens in the prompt",
                "desc": "Number of tokens in the prompt",
                "type": AbstractDataProviderTypeMap."int",
            },
            "completion_tokens": {
                "display_name": "Completion Tokens",
                "short_desc": "Number of tokens in the generated completion",
                "desc": "Number of tokens in the generated completion",
                "type": AbstractDataProviderTypeMap."int",
            },
            "total_tokens": {
                "display_name": "Total Tokens",
                "short_desc": "Total number of tokens used in the request",
                "desc": "Total number of tokens used in the request (prompt + completion)",
                "type": AbstractDataProviderTypeMap."int",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("PerplexityUsage") {
        addQoreFields(Fields);
    }
}

#! Constant for the chat completion usage type
public const PerplexityUsageDataType = new PerplexityUsageDataType();

#! Chat completion search result data type
public class PerplexityChatSearchResultDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "title": {
                "display_name": "Title",
                "short_desc": "The title of the search result",
                "desc": "The title of the search result page",
                "type": AbstractDataProviderTypeMap."string",
            },
            "url": {
                "display_name": "URL",
                "short_desc": "The URL of the search result",
                "desc": "The URL of the search result page",
                "type": AbstractDataProviderTypeMap."string",
            },
            "snippet": {
                "display_name": "Snippet",
                "short_desc": "A text snippet from the search result",
                "desc": "A text snippet extracted from the search result page",
                "type": AbstractDataProviderTypeMap."string",
            },
            "date": {
                "display_name": "Date",
                "short_desc": "The publication date of the result",
                "desc": "The publication date of the search result",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "last_updated": {
                "display_name": "Last Updated",
                "short_desc": "When the result was last updated",
                "desc": "The date when the search result page was last updated",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "source": {
                "display_name": "Source",
                "short_desc": "The source type of the search result",
                "desc": "The source type indicating the origin of the search result",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Web",
                        "short_desc": "Result from web search",
                        "desc": "The search result came from a web search",
                        "value": "web",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Attachment",
                        "short_desc": "Result from a user-uploaded attachment",
                        "desc": "The search result came from a user-uploaded attachment",
                        "value": "attachment",
                    },
                ),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("PerplexityChatSearchResult") {
        addQoreFields(Fields);
    }
}

#! Constant for the chat search result data type
public const PerplexityChatSearchResultDataType = new PerplexityChatSearchResultDataType();

#! The chat completion response type
public class PerplexityChatCompletionResponseType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "The ID of the response",
                "desc": "The ID of the response",
                "type": AbstractDataProviderTypeMap."string",
            },
            "object": {
                "display_name": "Object",
                "short_desc": "The type of response",
                "desc": "The type of response (always `chat.completion`)",
                "type": AbstractDataProviderTypeMap."string",
            },
            "created": {
                "display_name": "Creation Timestamp",
                "short_desc": "Unix epoch timestamp when the response was created",
                "desc": "Unix epoch timestamp when the response was created",
                "type": AbstractDataProviderTypeMap."int",
            },
            "model": {
                "display_name": "Model ID",
                "short_desc": "Model used to create the response",
                "desc": "Model used to create the response",
                "type": AbstractDataProviderTypeMap."string",
            },
            "choices": {
                "display_name": "Choices",
                "short_desc": "A list of chat completion choices",
                "desc": "A list of chat completion choices",
                "type": new ListDataType("ChoicesList", PerplexityChoiceDataType),
            },
            "usage": {
                "display_name": "Usage",
                "short_desc": "Usage statistics for the completion request",
                "desc": "Usage statistics for the completion request",
                "type": PerplexityUsageDataType,
            },
            "citations": {
                "display_name": "Citations",
                "short_desc": "Source URLs used to ground the response",
                "desc": "A list of source URLs that were used to ground the response. Perplexity automatically "
                    "provides web-grounded citations with every response",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "search_results": {
                "display_name": "Search Results",
                "short_desc": "Detailed search result objects",
                "desc": "Detailed search result objects from the web search that grounded the response",
                "type": new ListDataType("ChatSearchResultsList", PerplexityChatSearchResultDataType)
                    .getOrNothingType(),
            },
            "images": {
                "display_name": "Images",
                "short_desc": "Image results from the search",
                "desc": "Image URLs returned from the web search. Only present when `return_images` is set to "
                    "`True` in the request",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "related_questions": {
                "display_name": "Related Questions",
                "short_desc": "Suggested follow-up questions",
                "desc": "Suggested follow-up questions based on the response. Only present when "
                    "`return_related_questions` is set to `True` in the request",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("PerplexityChatCompletionResponse") {
        addQoreFields(Fields);
    }
}

#! The chat completion response type constant
public const PerplexityChatCompletionResponseType = new PerplexityChatCompletionResponseType();
}
