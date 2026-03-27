# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenAiDataProvider module definition

/** OpenAiRunCreateDataProvider.qc Copyright 2024 - 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the OpenAiDataProvider module
public namespace OpenAiDataProvider {
#! The OpenAi data provider class
public class OpenAiRunCreateDataProvider inherits OpenAiDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-run",
            "type": "OpenAiRunCreateDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = OpenAiRunCreateRequestType;

        #! Response type
        const ResponseType = OpenAiRunDataType;

        #! Optional date fields
        const OptDateFields = ("expires_at", "started_at", "canceled_at", "failed_at", "completed_at");
    }

    #! Creates the object from the arguments
    constructor(*RestClientIo::RestClientIo rest) : OpenAiDataProviderCommon(rest) {
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
        hash<auto> rv = doRestCommand("POST", "threads/" + (remove req.thread_id) + "/runs", req, AssistantApiHdr)
            .body;
        rv.created_at = localtime(rv.created_at);
        map rv{$1.key} = localtime($1.value), rv{OptDateFields}.pairIterator(), $1.value;
        return rv;
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

#! OpenAI truncation strategy type
public class OpenAiTrunctationStrategyDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "type": {
                "display_name": "Type",
                "short_desc": "The truncation strategy to use for the thread",
                "desc": "The truncation strategy to use for the thread. The default is `auto`. If set to "
                    "`last_messages`, the thread will be truncated to the `n` most recent messages in the thread. "
                    "When set to `auto`, messages in the middle of the thread will be dropped to fit the context "
                    "length of the model, `max_prompt_tokens`",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "auto",
                        "short_desc": "Messages in the middle of the thread will be dropped to fit the context "
                            "length of the model",
                        "desc": "Messages in the middle of the thread will be dropped to fit the context length of "
                            "the model, `max_prompt_tokens`",
                        "value": "auto",
                    },
                    <AllowedValueInfo>{
                        "display_name": "last_messages",
                        "short_desc": "The thread will be truncated to the 'n' most recent messages in the thread",
                        "desc": "The thread will be truncated to the `n` most recent messages in the thread",
                        "value": "last_messages",
                    },
                ),
                "default_value": "auto",
            },
            "last_messages": {
                "display_name": "Last Messages",
                "short_desc": "The number of most recent messages from the thread when constructing the context for "
                    "the run",
                "desc": "The number of most recent messages from the thread when constructing the context for the "
                    "run",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiTruncationStrategy") {
        addQoreFields(Fields);
    }
}

#! Constant for the truncation strategy type
public const OpenAiTrunctationStrategyDataType = new OpenAiTrunctationStrategyDataType();

#! The run request data type
public class OpenAiRunCreateRequestType inherits HashDataType {
    public {
        #! fields
        const Fields = OpenAiThreadIdRequestType::Fields{"thread_id",} + {
            "assistant_id": {
                "display_name": "Assistant ID",
                "short_desc": "The ID of the assistant to use to execute this run",
                "desc": "The ID of the assistant to use to execute this run",
                "type": AbstractDataProviderTypeMap."string",
                "attr": {
                    "ref_data": "assistants",
                },
            },
            "model": OpenAiChatCompletionRequestType::Fields.model + {
                "short_desc": "The model to override on the assistant to execute this run",
                "desc": "The ID of the Model to be used to execute this run. If a value is provided here, it will "
                    "override the model associated with the assistant. If not, the model associated with the "
                    "assistant will be used",
                "type": AbstractDataProviderTypeMap."*string",
                # for dynamic lookups of allowed values
                "attr": {
                    "ref_data": "models",
                },
            },
            "instructions": OpenAiAssistantCreateRequestType::Fields.instructions + {
                "short_desc": "Overrides the instructions of the assistant",
                "desc": "Overrides the instructions of the assistant. This is useful for modifying the behavior on a "
                    "per-run basis",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "additional_instructions": {
                "display_name": "Additional Instructions",
                "short_desc": "Appends additional instructions at the end of the instructions for the run",
                "desc": "Appends additional instructions at the end of the instructions for the run. This is useful "
                    "for modifying the behavior on a per-run basis without overriding other instructions",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "additional_messages": {
                "display_name": "Additional Messages",
                "short_desc": "Adds additional messages to the thread before creating the run",
                "desc": "Adds additional messages to the thread before creating the run",
                "type": new ListDataType("*Messages", OpenAiMessageDataType, True),
            },
            "tools": OpenAiAssistantCreateRequestType::Fields.tools + {
                "short_desc": "Override the tools the assistant can use for this run",
                "desc": "Override the tools the assistant can use for this run. This is useful for modifying the "
                    "behavior on a per-run basis",
            },
            "metadata": OpenAiAssistantCreateRequestType::Fields.metadata,
        } + OpenAiChatCompletionRequestType::Fields{"temperature", "top_p", "stream"} + {
            "max_prompt_tokens": {
                "display_name": "Max Prompt Tokens",
                "short_desc": "The maximum number of prompt tokens that may be used over the course of the run",
                "desc": "The maximum number of prompt tokens that may be used over the course of the run. The run "
                    "will make a best effort to use only the number of prompt tokens specified, across multiple "
                    "turns of the run. If the run exceeds the number of prompt tokens specified, the run will end "
                    "with status `incomplete`",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "max_completion_tokens": {
                "display_name": "Max Completion Tokens",
                "short_desc": "The maximum number of completion tokens that may be used over the course of the run",
                "desc": "The maximum number of completion tokens that may be used over the course of the run. The "
                    "run will make a best effort to use only the number of completion tokens specified, across "
                    "multiple turns of the run. If the run exceeds the number of completion tokens specified, the "
                    "run will end with status `incomplete`",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "truncation_strategy": {
                "display_name": "Truncation Strategy",
                "short_desc": "Controls for how a thread will be truncated prior to the run",
                "desc": "Controls for how a thread will be truncated prior to the run. Use this to control the "
                    "initial context window of the run",
                "type": OpenAiTrunctationStrategyDataType.getOrNothingType(),
            },
        } + OpenAiChatCompletionRequestType::Fields{"tool_choice", "response_format"};
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiRunCreateRequest") {
        addQoreFields(Fields);
    }
}

#! Constant for the run request data type
public const OpenAiRunCreateRequestType = new OpenAiRunCreateRequestType();

#! Function call data type
public class OpenAiFunctionCallDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "name": {
                "display_name": "Name",
                "short_desc": "The name of the function",
                "desc": "The name of the function",
                "type": AbstractDataProviderTypeMap."string",
            },
            "arguments": {
                "display_name": "Arguments",
                "short_desc": "The arguments that the model expects you to pass to the function",
                "desc": "The arguments that the model expects you to pass to the function",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiFunctionCall") {
        addQoreFields(Fields);
    }
}

#! Function call data type constant
public const OpenAiFunctionCallDataType = new OpenAiFunctionCallDataType();

#! Tool call data type
public class OpenAiRunToolCallDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "The ID of the tool call",
                "desc": "The ID of the tool call. This ID must be referenced when you submit the tool outputs in "
                    "using the `Submit tool outputs to run` endpoint",
                "type": AbstractDataProviderTypeMap."string",
            },
            "type": {
                "display_name": "Type",
                "short_desc": "The type of tool call the output is required for",
                "desc": "The type of tool call the output is required for. For now, this is always `function`",
                "type": AbstractDataProviderTypeMap."string",
                "example_value": "function",
            },
            "function": {
                "display_name": "Function",
                "short_desc": "The function call definition",
                "desc": "The function call definition",
                "type": OpenAiFunctionCallDataType,
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiToolCall") {
        addQoreFields(Fields);
    }
}

#! Tool call type constant
public const OpenAiRunToolCallDataType = new OpenAiRunToolCallDataType();

#! Submit tool outputs data type
public class OpenAiSubmitToolOutputsDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "tool_calls": {
                "display_name": "Tool Calls",
                "short_desc": "A list of the relevant tool calls",
                "desc": "A list of the relevant tool calls",
                "type": new ListDataType("ToolCalls", OpenAiRunToolCallDataType),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiSubmitToolOutputs") {
        addQoreFields(Fields);
    }
}

#! Required action data type constant
public const OpenAiSubmitToolOutputsDataType = new OpenAiSubmitToolOutputsDataType();

#! Required action data type
public class OpenAiRequiredActionDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "type": {
                "display_name": "Type",
                "short_desc": "The type of action required",
                "desc": "The type of action required. For now, this is always `submit_tool_outputs`",
                "type": AbstractDataProviderTypeMap."string",
                "example_value": "submit_tool_outputs",
            },
            "submit_tool_outputs": {
                "display_name": "Submit Tool Outputs",
                "short_desc": "Details on the tool outputs needed for this run to continue",
                "desc": "Details on the tool outputs needed for this run to continue",
                "type": OpenAiSubmitToolOutputsDataType,
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiRunRequest") {
        addQoreFields(Fields);
    }
}

#! Required action data type constant
public const OpenAiRequiredActionDataType = new OpenAiRequiredActionDataType();

#! Run error data type
public class OpenAiRunErrorDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "code": {
                "display_name": "Code",
                "short_desc": "The error code",
                "desc": "One of `server_error`, `rate_limit_exceeded`, or `invalid_prompt`",
                "type": AbstractDataProviderTypeMap."string",
                "example_value": "server_error",
            },
            "message": {
                "display_name": "Error Message",
                "short_desc": "A human-readable description of the error",
                "desc": "A human-readable description of the error",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiRunRequest") {
        addQoreFields(Fields);
    }
}

#! Run error data type constant
public const OpenAiRunErrorDataType = new OpenAiRunErrorDataType();

#! Incomplete details data type
public class OpenAiIncompleteDetails inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "reason": {
                "display_name": "Reason",
                "short_desc": "The reason why the data is incomplete",
                "desc": "The reason why the data is incomplete. This will point to which specific token limit was "
                    "reached over the course of the run",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiIncompleteDetails") {
        addQoreFields(Fields);
    }
}

#! Incomplete details data type constant
public const OpenAiIncompleteDetails = new OpenAiIncompleteDetails();

#! Run type
public class OpenAiRunDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "id": {
                "display_name": "Run ID",
                "short_desc": "Run ID",
                "desc": "Run ID",
                "type": AbstractDataProviderTypeMap."string",
            },
            "object": {
                "display_name": "Object Type",
                "short_desc": "The type of object",
                "desc": "The type of object",
                "type": AbstractDataProviderTypeMap."string",
            },
            "created_at": {
                "display_name": "Creation Timestamp",
                "short_desc": "The date and time the run was created",
                "desc": "The date and time the run was created",
                "type": AbstractDataProviderTypeMap."date",
            },
            "assistant_id": OpenAiRunCreateRequestType::Fields.assistant_id,
            "status": {
                "display_name": "Status",
                "short_desc": "The status of the run",
                "desc": "The status of the run, which can be either `queued`, `in_progress`, `requires_action`, "
                    "`cancelling`, `cancelled`, `failed`, `completed`, `incomplete`, or `expired`",
                "type": AbstractDataProviderTypeMap."string",
            },
            "required_action": {
                "display_name": "Required Action",
                "short_desc": "Details on the action required to continue the run",
                "desc": "Details on the action required to continue the run. Will be `null` if no action is required",
                "type": OpenAiRequiredActionDataType.getOrNothingType(),
            },
            "last_error": {
                "display_name": "Last Error",
                "short_desc": "The last error associated with this run",
                "desc": "The last error associated with this run. Will be `null` if there are no errors",
                "type": OpenAiRunErrorDataType.getOrNothingType(),
            },
            "expires_at": {
                "display_name": "Expires At",
                "short_desc": "When the run will expire",
                "desc": "When the run will expire",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "started_at": {
                "display_name": "Started At",
                "short_desc": "When the run was started",
                "desc": "When the run was started",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "canceled_at": {
                "display_name": "Canceled At",
                "short_desc": "When the run was canceled",
                "desc": "When the run was canceled",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "failed_at": {
                "display_name": "Failed At",
                "short_desc": "When the run failed",
                "desc": "When the run failed",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "completed_at": {
                "display_name": "Completed At",
                "short_desc": "When the run completed",
                "desc": "When the run completed",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "incomplete_details": {
                "display_name": "Incomplete Details",
                "short_desc": "Details on why the run is incomplete",
                "desc": "Details on why the run is incomplete. Will be `null` if the run is not incomplete",
                "type": OpenAiIncompleteDetails.getOrNothingType(),
            },
            "model": OpenAiChatCompletionRequestType::Fields.model + {
                "short_desc": "The model that the assistant used for this run",
                "desc": "The model that the assistant used for this run",
            },
            "instructions": OpenAiAssistantCreateRequestType::Fields.instructions + {
                "short_desc": "The instructions that the assistant used for this run",
                "desc": "The instructions that the assistant used for this run",
            },
            "tools": OpenAiAssistantCreateRequestType::Fields.tools + {
                "short_desc": "The list of tools that the assistant used for this run",
                "desc": "The list of tools that the assistant used for this run",
            },
            "metadata": OpenAiAssistantCreateRequestType::Fields.metadata,
            "usage": OpenAiChatCompletionResponseType::Fields.usage + {
                "short_desc": "Usage statistics related to the run",
                "desc": "Usage statistics related to the run. This value will be `null` if the run is not in a "
                    "terminal state (i.e. `in_progress`, `queued`, etc.)",
            },
        } + OpenAiChatCompletionRequestType::Fields{"temperature", "top_p"} +
        OpenAiRunCreateRequestType::Fields{"max_prompt_tokens", "max_completion_tokens", "truncation_strategy",
            "tool_choice", "response_format"};
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiRunCreateRequest") {
        addQoreFields(Fields);
    }
}

#! Run type constant
public const OpenAiRunDataType = new OpenAiRunDataType();
}
