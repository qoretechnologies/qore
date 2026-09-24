# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DeepSeekApiDataTypes class definition

/** DeepSeekApiDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Public DeepSeek API contracts
public namespace DeepSeekDataProvider {

#! ResponseContent data contract
public class DeepSeekResponseContentDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "type": {
                "display_name": "Content Type",
                "short_desc": "Content Type",
                "desc": "Content Type.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "input_text", "display_name": "Input Text"},
                    <AllowedValueInfo>{"value": "output_text", "display_name": "Output Text"},
                    <AllowedValueInfo>{"value": "input_image", "display_name": "Input Image"},
                    <AllowedValueInfo>{"value": "reasoning_text", "display_name": "Reasoning Text"},
                ),
            },
            "text": {
                "display_name": "Text",
                "short_desc": "Text",
                "desc": "Text for a text or reasoning part.",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "Describe the product.",
            },
            "image_url": {
                "display_name": "Image URL",
                "short_desc": "Image URL",
                "desc": "Public image URL or base64 data URL. Mutually exclusive with `file_id`.",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "https://example.com/product.png",
            },
            "file_id": {
                "display_name": "File ID",
                "short_desc": "File ID",
                "desc": "Uploaded image identifier. Mutually exclusive with `image_url`.",
                "type": AbstractDataProviderTypeMap."*string",
                "ref_data": "files",
                "allowed_values_creatable": True,
            },
            "detail": {
                "display_name": "Image Detail",
                "short_desc": "Image Detail",
                "desc": "Image processing quality; ignored when `file_id` is supplied.",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "low", "display_name": "Low"},
                    <AllowedValueInfo>{"value": "high", "display_name": "High"},
                    <AllowedValueInfo>{"value": "original", "display_name": "Original"},
                    <AllowedValueInfo>{"value": "auto", "display_name": "Automatic"},
                ),
            },
            "annotations": {
                "display_name": "Annotations",
                "short_desc": "Annotations",
                "desc": "Preserved when reusing content from an earlier response.",
                "type": AbstractDataProviderTypeMap."*list",
            },
            "logprobs": {
                "display_name": "Log Probabilities",
                "short_desc": "Log Probabilities",
                "desc": "Preserved when reusing content from an earlier response.",
                "type": AbstractDataProviderTypeMap."*list",
            },
        };
    }

    #! Creates the typed fields
    /** @param optional whether the object can be absent */
    constructor(bool optional = False) : HashDataType(optional ? AutoHashOrNothingType : AutoHashType) {
        addQoreFields(Fields);
        # Responses metadata is extensible; input items must preserve returned items verbatim.
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

#! Shared DeepSeekResponseContentDataType instance
public const DeepSeekResponseContentDataType = new DeepSeekResponseContentDataType();


#! FimRequest data contract
public class DeepSeekFimRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "Model",
                "desc": "Select a model supporting Fill-in-the-Middle completion. Currently `deepseek-v4-pro`.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "fim-models",
                "allowed_values_creatable": True,
            },
            "prompt": {
                "display_name": "Prompt",
                "short_desc": "Prompt",
                "desc": "Text before the insertion point.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "def calculate_total(items):\n    ",
            },
            "suffix": {
                "display_name": "Suffix",
                "short_desc": "Suffix",
                "desc": "Text after the insertion point.",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "\n    return total",
                "preselected": True,
            },
            "max_tokens": {
                "display_name": "Max Tokens",
                "short_desc": "Max Tokens",
                "desc": "Maximum generated tokens, including reasoning. Check the selected model limits.",
                "type": AbstractDataProviderTypeMap."*softint",
                "example_value": 2048,
                "preselected": True,
            },
            "temperature": {
                "display_name": "Temperature",
                "short_desc": "Temperature",
                "desc": "Sampling temperature from `0` to `2`. Has no effect in thinking mode.",
                "type": AbstractDataProviderTypeMap."*softfloat",
                "example_value": 1.0,
            },
            "top_p": {
                "display_name": "Top-P",
                "short_desc": "Top-P",
                "desc": "Nucleus sampling cutoff from `0` to `1`. Has no effect in thinking mode.",
                "type": AbstractDataProviderTypeMap."*softfloat",
            },
            "stop": {
                "display_name": "Stop Sequences",
                "short_desc": "Stop Sequences",
                "desc": "Up to 16 strings that stop generation before the matching sequence.",
                "type": new QoreUnionDataType((AbstractDataProviderTypeMap."*string", new ListDataType("DeepSeekStopList", StringType),)),
                "example_value": ("END",),
            },
            "echo": {
                "display_name": "Echo Prompt",
                "short_desc": "Echo Prompt",
                "desc": "Include the prompt in the returned text.",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "logprobs": {
                "display_name": "Log Probabilities",
                "short_desc": "Log Probabilities",
                "desc": "Number of alternative token probabilities from `0` to `20`.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "stream": {
                "display_name": "Stream",
                "short_desc": "Stream",
                "desc": "Completed-response actions accept `False`. Use `DeepSeekLlmProvider` for streaming chat.",
                "type": AbstractDataProviderTypeMap."*bool",
                "allowed_values": (
                    <AllowedValueInfo>{"value": False, "display_name": "Complete Response"},
                ),
            },
        };
    }

    #! Creates the typed fields
    /** @param optional whether the object can be absent */
    constructor(bool optional = False) : HashDataType(optional ? AutoHashOrNothingType : AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Shared DeepSeekFimRequestDataType instance
public const DeepSeekFimRequestDataType = new DeepSeekFimRequestDataType();

#! Reasoning data contract
public class DeepSeekReasoningDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "effort": {
                "display_name": "Reasoning Effort",
                "short_desc": "Reasoning Effort",
                "desc": "Choose `none` to disable thinking; other values control reasoning effort.",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "none", "display_name": "None"},
                    <AllowedValueInfo>{"value": "minimal", "display_name": "Minimal"},
                    <AllowedValueInfo>{"value": "low", "display_name": "Low"},
                    <AllowedValueInfo>{"value": "medium", "display_name": "Medium"},
                    <AllowedValueInfo>{"value": "high", "display_name": "High"},
                    <AllowedValueInfo>{"value": "xhigh", "display_name": "Extra High"},
                    <AllowedValueInfo>{"value": "max", "display_name": "Maximum"},
                ),
            },
        };
    }

    #! Creates the typed fields
    /** @param optional whether the object can be absent */
    constructor(bool optional = False) : HashDataType(optional ? AutoHashOrNothingType : AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Shared DeepSeekReasoningDataType instance
public const DeepSeekReasoningDataType = new DeepSeekReasoningDataType();

#! TextFormat data contract
public class DeepSeekTextFormatDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "type": {
                "display_name": "Format",
                "short_desc": "Format",
                "desc": "Format.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "text", "display_name": "Text"},
                    <AllowedValueInfo>{"value": "json_object", "display_name": "JSON Object"},
                    <AllowedValueInfo>{"value": "json_schema", "display_name": "JSON Schema"},
                ),
            },
            "name": {
                "display_name": "Schema Name",
                "short_desc": "Schema Name",
                "desc": "Required for `json_schema` output.",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "invoice",
            },
            "schema": {
                "display_name": "Schema",
                "short_desc": "Schema",
                "desc": "Required JSON Schema for `json_schema` output.",
                "type": AbstractDataProviderTypeMap."*hash",
                "example_value": {
                    "type": "object",
                    "properties": {
                        "total": {
                            "type": "number",
                        },
                    },
                    "required": ("total",),
                },
            },
        };
    }

    #! Creates the typed fields
    /** @param optional whether the object can be absent */
    constructor(bool optional = False) : HashDataType(optional ? AutoHashOrNothingType : AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Shared DeepSeekTextFormatDataType instance
public const DeepSeekTextFormatDataType = new DeepSeekTextFormatDataType();

#! TextConfig data contract
public class DeepSeekTextConfigDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "format": {
                "display_name": "Output Format",
                "short_desc": "Output Format",
                "desc": "Output Format.",
                "type": new DeepSeekTextFormatDataType(True),
            },
        };
    }

    #! Creates the typed fields
    /** @param optional whether the object can be absent */
    constructor(bool optional = False) : HashDataType(optional ? AutoHashOrNothingType : AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Shared DeepSeekTextConfigDataType instance
public const DeepSeekTextConfigDataType = new DeepSeekTextConfigDataType();

#! ResponseTool data contract
public class DeepSeekResponseToolDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "type": {
                "display_name": "Tool Type",
                "short_desc": "Tool Type",
                "desc": "Tool Type.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "function", "display_name": "Function"},
                    <AllowedValueInfo>{"value": "custom", "display_name": "Custom Patch Tool"},
                    <AllowedValueInfo>{"value": "web_search", "display_name": "Web Search"},
                    <AllowedValueInfo>{"value": "web_search_2025_08_26", "display_name": "Web Search (2025-08-26)"},
                ),
            },
            "name": {
                "display_name": "Function Name",
                "short_desc": "Function Name",
                "desc": "Required for `function` tools; at most 128 letters, digits, underscores, or dashes. Use `apply_patch` for `custom` tools.",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "lookup_order",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Description",
                "desc": "Explain when to invoke the function.",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "Look up an order.",
            },
            "parameters": {
                "display_name": "Parameters",
                "short_desc": "Parameters",
                "desc": "JSON Schema describing function arguments.",
                "type": AbstractDataProviderTypeMap."*hash",
                "example_value": {
                    "type": "object",
                    "properties": {
                    },
                },
            },
        };
    }

    #! Creates the typed fields
    /** @param optional whether the object can be absent */
    constructor(bool optional = False) : HashDataType(optional ? AutoHashOrNothingType : AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Shared DeepSeekResponseToolDataType instance
public const DeepSeekResponseToolDataType = new DeepSeekResponseToolDataType();

#! ResponseInput data contract
public class DeepSeekResponseInputDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "type": {
                "display_name": "Item Type",
                "short_desc": "Item Type",
                "desc": "Item Type.",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "message", "display_name": "Message"},
                    <AllowedValueInfo>{"value": "function_call", "display_name": "Function Call"},
                    <AllowedValueInfo>{"value": "function_call_output", "display_name": "Function Call Output"},
                    <AllowedValueInfo>{"value": "custom_tool_call", "display_name": "Custom Tool Call"},
                    <AllowedValueInfo>{"value": "custom_tool_call_output", "display_name": "Custom Tool Call Output"},
                    <AllowedValueInfo>{"value": "reasoning", "display_name": "Reasoning"},
                    <AllowedValueInfo>{"value": "web_search_call", "display_name": "Web Search Call"},
                ),
            },
            "role": {
                "display_name": "Role",
                "short_desc": "Role",
                "desc": "Role.",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "user", "display_name": "User"},
                    <AllowedValueInfo>{"value": "assistant", "display_name": "Assistant"},
                    <AllowedValueInfo>{"value": "system", "display_name": "System"},
                    <AllowedValueInfo>{"value": "developer", "display_name": "Developer"},
                ),
            },
            "content": {
                "display_name": "Content",
                "short_desc": "Content",
                "desc": "Text or content parts such as `input_text`, `output_text`, `input_image`, or `reasoning_text`.",
                "type": new QoreUnionDataType((AbstractDataProviderTypeMap."*string", new ListDataType("DeepSeekResponseContentList", new DeepSeekResponseContentDataType()),)),
            },
            "call_id": {
                "display_name": "Call ID",
                "short_desc": "Call ID",
                "desc": "Pair a tool call with its output.",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "call_123",
            },
            "id": {
                "display_name": "Item ID",
                "short_desc": "Item ID",
                "desc": "Identifier returned by a previous response item.",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "fc_123",
            },
            "name": {
                "display_name": "Function Name",
                "short_desc": "Function Name",
                "desc": "Function associated with a tool call.",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "lookup_order",
            },
            "arguments": {
                "display_name": "Arguments",
                "short_desc": "Arguments",
                "desc": "JSON-encoded function arguments.",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "{\"order_id\":\"123\"}",
            },
            "output": {
                "display_name": "Tool Output",
                "short_desc": "Tool Output",
                "desc": "Tool result text or image/text content parts.",
                "type": new QoreUnionDataType((AbstractDataProviderTypeMap."*string", new ListDataType("DeepSeekResponseContentList", new DeepSeekResponseContentDataType()),)),
            },
            "input": {
                "display_name": "Patch Input",
                "short_desc": "Patch Input",
                "desc": "Patch text from an earlier `custom_tool_call`.",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "*** Begin Patch\n*** End Patch",
            },
            "status": {
                "display_name": "Item Status",
                "short_desc": "Item Status",
                "desc": "Status returned by an earlier response item.",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "in_progress", "display_name": "In Progress"},
                    <AllowedValueInfo>{"value": "completed", "display_name": "Completed"},
                    <AllowedValueInfo>{"value": "incomplete", "display_name": "Incomplete"},
                ),
            },
            "action": {
                "display_name": "Search Action",
                "short_desc": "Search Action",
                "desc": "Pass back the complete `web_search_call` action to restore search results.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "summary": {
                "display_name": "Reasoning Summary",
                "short_desc": "Reasoning Summary",
                "desc": "May be carried over from a response. DeepSeek reads reasoning from `content` and ignores `summary`.",
                "type": AbstractDataProviderTypeMap."*list",
            },
        };
    }

    #! Creates the typed fields
    /** @param optional whether the object can be absent */
    constructor(bool optional = False) : HashDataType(optional ? AutoHashOrNothingType : AutoHashType) {
        addQoreFields(Fields);
        # Responses metadata is extensible; input items must preserve returned items verbatim.
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

#! Shared DeepSeekResponseInputDataType instance
public const DeepSeekResponseInputDataType = new DeepSeekResponseInputDataType();

#! CreateResponseRequest data contract
public class DeepSeekCreateResponseRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "Model",
                "desc": "Select an available model for this request.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "models",
                "allowed_values_creatable": True,
            },
            "input": {
                "display_name": "Input",
                "short_desc": "Input",
                "desc": "Text or conversation items. Supply `input` or `instructions`. Include the full history for each turn.",
                "type": new QoreUnionDataType((AbstractDataProviderTypeMap."*string", new ListDataType("DeepSeekResponseInputDataTypeList", new DeepSeekResponseInputDataType(), False),)),
                "example_value": "Summarize our delivery policy.",
                "preselected": True,
            },
            "instructions": {
                "display_name": "Instructions",
                "short_desc": "Instructions",
                "desc": "System instructions; required when `input` is absent.",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "Respond as a customer support assistant.",
            },
            "max_output_tokens": {
                "display_name": "Max Output Tokens",
                "short_desc": "Max Output Tokens",
                "desc": "Maximum generated tokens, including reasoning. Check the selected model limits.",
                "type": AbstractDataProviderTypeMap."*softint",
                "example_value": 2048,
                "preselected": True,
            },
            "reasoning": {
                "display_name": "Reasoning",
                "short_desc": "Reasoning",
                "desc": "Control how much reasoning the model performs before answering. Set `effort` to `none` to disable thinking.",
                "type": new DeepSeekReasoningDataType(True),
            },
            "temperature": {
                "display_name": "Temperature",
                "short_desc": "Temperature",
                "desc": "Sampling temperature from `0` to `2`. Has no effect in thinking mode.",
                "type": AbstractDataProviderTypeMap."*softfloat",
                "example_value": 1.0,
            },
            "top_p": {
                "display_name": "Top-P",
                "short_desc": "Top-P",
                "desc": "Nucleus sampling cutoff from `0` to `1`. Has no effect in thinking mode.",
                "type": AbstractDataProviderTypeMap."*softfloat",
            },
            "text": {
                "display_name": "Text Output",
                "short_desc": "Text Output",
                "desc": "Choose plain text, JSON, or output matching a JSON Schema.",
                "type": new DeepSeekTextConfigDataType(True),
            },
            "tools": {
                "display_name": "Tools",
                "short_desc": "Tools",
                "desc": "Custom functions or server-side web search.",
                "type": new ListDataType("DeepSeekResponseToolDataTypeList", new DeepSeekResponseToolDataType(), True),
            },
            "tool_choice": {
                "display_name": "Tool Choice",
                "short_desc": "Tool Choice",
                "desc": "Choose a tool policy or provide a named function object to force a specific tool.",
                "type": new QoreUnionDataType((AbstractDataProviderTypeMap."*string", AbstractDataProviderTypeMap."hash",)),
                "allowed_values": (
                    <AllowedValueInfo>{"value": "none", "display_name": "None"},
                    <AllowedValueInfo>{"value": "auto", "display_name": "Automatic"},
                    <AllowedValueInfo>{"value": "required", "display_name": "Required"},
                ),
                "allowed_values_creatable": True,
                "example_value": "auto",
            },
            "top_logprobs": {
                "display_name": "Top Log Probabilities",
                "short_desc": "Top Log Probabilities",
                "desc": "Number of token alternatives from `0` to `20`.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "user": {
                "display_name": "User ID",
                "short_desc": "User ID",
                "desc": "Pseudonymous identifier for cache and scheduling isolation. Use letters, digits, underscores, or dashes; at most 512 characters. Exclude personal information.",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "customer-123",
            },
            "stream": {
                "display_name": "Stream",
                "short_desc": "Stream",
                "desc": "Completed-response actions accept `False`. Use `DeepSeekLlmProvider` for streaming chat.",
                "type": AbstractDataProviderTypeMap."*bool",
                "allowed_values": (
                    <AllowedValueInfo>{"value": False, "display_name": "Complete Response"},
                ),
            },
        };
    }

    #! Creates the typed fields
    /** @param optional whether the object can be absent */
    constructor(bool optional = False) : HashDataType(optional ? AutoHashOrNothingType : AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Shared DeepSeekCreateResponseRequestDataType instance
public const DeepSeekCreateResponseRequestDataType = new DeepSeekCreateResponseRequestDataType();

#! FileRequest data contract
public class DeepSeekFileRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "file_id": {
                "display_name": "File ID",
                "short_desc": "File ID",
                "desc": "Select an uploaded image file.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "files",
                "allowed_values_creatable": True,
                "example_value": "file-api-0123456789",
            },
        };
    }

    #! Creates the typed fields
    /** @param optional whether the object can be absent */
    constructor(bool optional = False) : HashDataType(optional ? AutoHashOrNothingType : AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Shared DeepSeekFileRequestDataType instance
public const DeepSeekFileRequestDataType = new DeepSeekFileRequestDataType();

#! ListFilesRequest data contract
public class DeepSeekListFilesRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "after": {
                "display_name": "After File",
                "short_desc": "After File",
                "desc": "Return files after this cursor.",
                "type": AbstractDataProviderTypeMap."*string",
                "required": False,
                "ref_data": "files",
                "allowed_values_creatable": True,
                "example_value": "file-api-0123456789",
            },
            "limit": {
                "display_name": "Limit",
                "short_desc": "Limit",
                "desc": "Maximum files to return, from `1` to `1000`.",
                "type": AbstractDataProviderTypeMap."*softint",
                "example_value": 100,
            },
            "order": {
                "display_name": "Order",
                "short_desc": "Order",
                "desc": "Sort files by creation time.",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "asc", "display_name": "Oldest First"},
                    <AllowedValueInfo>{"value": "desc", "display_name": "Newest First"},
                ),
            },
            "purpose": {
                "display_name": "Purpose",
                "short_desc": "Purpose",
                "desc": "Purpose.",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "user_data", "display_name": "User Data"},
                ),
            },
        };
    }

    #! Creates the typed fields
    /** @param optional whether the object can be absent */
    constructor(bool optional = False) : HashDataType(optional ? AutoHashOrNothingType : AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Shared DeepSeekListFilesRequestDataType instance
public const DeepSeekListFilesRequestDataType = new DeepSeekListFilesRequestDataType();

#! UploadFileRequest data contract
public class DeepSeekUploadFileRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "file": {
                "display_name": "Image File",
                "short_desc": "Image File",
                "desc": "Named image with `name`, binary or base64 `content`, and optional `mime_type`. JPEG, PNG, GIF, or WebP; at most 64 MiB.",
                "type": FileDataType,
                "required": True,
            },
            "purpose": {
                "display_name": "Purpose",
                "short_desc": "Purpose",
                "desc": "Intended use of this image.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "default_value": "user_data",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "user_data", "display_name": "User Data"},
                ),
            },
            "expires_after_seconds": {
                "display_name": "Expires After (Seconds)",
                "short_desc": "Expires After (Seconds)",
                "desc": "Delete the file automatically after `3600` to `2592000` seconds. Omit to retain it.",
                "type": AbstractDataProviderTypeMap."*softint",
                "example_value": 86400,
            },
        };
    }

    #! Creates the typed fields
    /** @param optional whether the object can be absent */
    constructor(bool optional = False) : HashDataType(optional ? AutoHashOrNothingType : AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Shared DeepSeekUploadFileRequestDataType instance
public const DeepSeekUploadFileRequestDataType = new DeepSeekUploadFileRequestDataType();

#! Model data contract
public class DeepSeekModelDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "Model ID",
                "short_desc": "Model ID",
                "desc": "Model ID.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "object": {
                "display_name": "Object Type",
                "short_desc": "Object Type",
                "desc": "Object Type.",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "model", "display_name": "Model"},
                ),
            },
            "owned_by": {
                "display_name": "Owner",
                "short_desc": "Owner",
                "desc": "Owner.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "name": {
                "display_name": "Model Name",
                "short_desc": "The display name of the model",
                "desc": "The display name of the model, for example `DeepSeek-V4-Pro`.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "context_window": {
                "display_name": "Context Window",
                "short_desc": "The maximum number of tokens in the context",
                "desc": "The maximum number of tokens the model accepts in its context, including the input and "
                    "the output.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "max_output_tokens": {
                "display_name": "Maximum Output Tokens",
                "short_desc": "The maximum number of tokens in a response",
                "desc": "The maximum number of tokens the model can generate in a response.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "input_modalities": {
                "display_name": "Input Modalities",
                "short_desc": "The kinds of input the model accepts",
                "desc": "The kinds of input the model accepts, for example `text` and `image`.",
                "type": AbstractDataProviderTypeMap."*list",
            },
            "output_modalities": {
                "display_name": "Output Modalities",
                "short_desc": "The kinds of output the model generates",
                "desc": "The kinds of output the model generates, for example `text`.",
                "type": AbstractDataProviderTypeMap."*list",
            },
            "effort": {
                "display_name": "Reasoning Effort",
                "short_desc": "The reasoning effort levels the model supports",
                "desc": "The reasoning effort levels the model supports: `supported_levels` lists the levels and "
                    "`default_level` gives the level used when a request does not set one.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "api_capabilities": {
                "display_name": "API Capabilities",
                "short_desc": "Model behavior specific to each supported API",
                "desc": "Model behavior specific to each supported API, keyed by the API name (for example "
                    "`anthropic_messages`).",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    #! Creates the typed fields
    /** @param optional whether the object can be absent */
    constructor(bool optional = False) : HashDataType(optional ? AutoHashOrNothingType : AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Shared DeepSeekModelDataType instance
public const DeepSeekModelDataType = new DeepSeekModelDataType();

#! ListModelsResponse data contract
public class DeepSeekListModelsResponseDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "object": {
                "display_name": "Object Type",
                "short_desc": "Object Type",
                "desc": "Object Type.",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "list", "display_name": "List"},
                ),
            },
            "data": {
                "display_name": "Models",
                "short_desc": "Models",
                "desc": "Models.",
                "type": new ListDataType("DeepSeekModelDataTypeList", new DeepSeekModelDataType(), False),
            },
        };
    }

    #! Creates the typed fields
    /** @param optional whether the object can be absent */
    constructor(bool optional = False) : HashDataType(optional ? AutoHashOrNothingType : AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Shared DeepSeekListModelsResponseDataType instance
public const DeepSeekListModelsResponseDataType = new DeepSeekListModelsResponseDataType();

#! BalanceInfo data contract
public class DeepSeekBalanceInfoDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "currency": {
                "display_name": "Currency",
                "short_desc": "Currency",
                "desc": "Currency.",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "CNY", "display_name": "Chinese Yuan"},
                    <AllowedValueInfo>{"value": "USD", "display_name": "US Dollar"},
                ),
            },
            "total_balance": {
                "display_name": "Total Balance",
                "short_desc": "Total Balance",
                "desc": "Available credit as a decimal string.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "granted_balance": {
                "display_name": "Granted Balance",
                "short_desc": "Granted Balance",
                "desc": "Unexpired promotional credit as a decimal string.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "topped_up_balance": {
                "display_name": "Purchased Balance",
                "short_desc": "Purchased Balance",
                "desc": "Purchased credit as a decimal string.",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }

    #! Creates the typed fields
    /** @param optional whether the object can be absent */
    constructor(bool optional = False) : HashDataType(optional ? AutoHashOrNothingType : AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Shared DeepSeekBalanceInfoDataType instance
public const DeepSeekBalanceInfoDataType = new DeepSeekBalanceInfoDataType();

#! BalanceResponse data contract
public class DeepSeekBalanceResponseDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "is_available": {
                "display_name": "Credit Available",
                "short_desc": "Credit Available",
                "desc": "Whether sufficient credit is available for inference.",
                "type": AbstractDataProviderTypeMap."bool",
            },
            "balance_infos": {
                "display_name": "Balances",
                "short_desc": "Balances",
                "desc": "Balances.",
                "type": new ListDataType("DeepSeekBalanceInfoDataTypeList", new DeepSeekBalanceInfoDataType(), False),
            },
        };
    }

    #! Creates the typed fields
    /** @param optional whether the object can be absent */
    constructor(bool optional = False) : HashDataType(optional ? AutoHashOrNothingType : AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Shared DeepSeekBalanceResponseDataType instance
public const DeepSeekBalanceResponseDataType = new DeepSeekBalanceResponseDataType();

#! File data contract
public class DeepSeekFileDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "File ID",
                "short_desc": "File ID",
                "desc": "File ID.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "object": {
                "display_name": "Object Type",
                "short_desc": "Object Type",
                "desc": "Object Type.",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "file", "display_name": "File"},
                ),
            },
            "bytes": {
                "display_name": "Size (Bytes)",
                "short_desc": "Size (Bytes)",
                "desc": "Size (Bytes).",
                "type": AbstractDataProviderTypeMap."int",
            },
            "created_at": {
                "display_name": "Created",
                "short_desc": "Created",
                "desc": "Created.",
                "type": AbstractDataProviderTypeMap."date",
            },
            "filename": {
                "display_name": "Filename",
                "short_desc": "Filename",
                "desc": "Filename.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "purpose": {
                "display_name": "Purpose",
                "short_desc": "Purpose",
                "desc": "Purpose.",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "user_data", "display_name": "User Data"},
                ),
            },
            "expires_at": {
                "display_name": "Expires",
                "short_desc": "Expires",
                "desc": "Expires.",
                "type": AbstractDataProviderTypeMap."*date",
            },
        };
    }

    #! Creates the typed fields
    /** @param optional whether the object can be absent */
    constructor(bool optional = False) : HashDataType(optional ? AutoHashOrNothingType : AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Shared DeepSeekFileDataType instance
public const DeepSeekFileDataType = new DeepSeekFileDataType();

#! ListFilesResponse data contract
public class DeepSeekListFilesResponseDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "object": {
                "display_name": "Object Type",
                "short_desc": "Object Type",
                "desc": "Object Type.",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "list", "display_name": "List"},
                ),
            },
            "data": {
                "display_name": "Files",
                "short_desc": "Files",
                "desc": "Files.",
                "type": new ListDataType("DeepSeekFileDataTypeList", new DeepSeekFileDataType(), False),
            },
            "first_id": {
                "display_name": "First File ID",
                "short_desc": "First File ID",
                "desc": "First File ID.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "last_id": {
                "display_name": "Last File ID",
                "short_desc": "Last File ID",
                "desc": "Last File ID.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "has_more": {
                "display_name": "More Files Available",
                "short_desc": "More Files Available",
                "desc": "More Files Available.",
                "type": AbstractDataProviderTypeMap."bool",
            },
        };
    }

    #! Creates the typed fields
    /** @param optional whether the object can be absent */
    constructor(bool optional = False) : HashDataType(optional ? AutoHashOrNothingType : AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Shared DeepSeekListFilesResponseDataType instance
public const DeepSeekListFilesResponseDataType = new DeepSeekListFilesResponseDataType();

#! DeleteFileResponse data contract
public class DeepSeekDeleteFileResponseDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "File ID",
                "short_desc": "File ID",
                "desc": "File ID.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "object": {
                "display_name": "Object Type",
                "short_desc": "Object Type",
                "desc": "Object Type.",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "file", "display_name": "File"},
                ),
            },
            "deleted": {
                "display_name": "Deleted",
                "short_desc": "Deleted",
                "desc": "Deleted.",
                "type": AbstractDataProviderTypeMap."bool",
            },
        };
    }

    #! Creates the typed fields
    /** @param optional whether the object can be absent */
    constructor(bool optional = False) : HashDataType(optional ? AutoHashOrNothingType : AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Shared DeepSeekDeleteFileResponseDataType instance
public const DeepSeekDeleteFileResponseDataType = new DeepSeekDeleteFileResponseDataType();

#! FimChoice data contract
public class DeepSeekFimChoiceDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "index": {
                "display_name": "Index",
                "short_desc": "Index",
                "desc": "Index.",
                "type": AbstractDataProviderTypeMap."int",
            },
            "text": {
                "display_name": "Text",
                "short_desc": "Text",
                "desc": "Text.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "finish_reason": {
                "display_name": "Finish Reason",
                "short_desc": "Finish Reason",
                "desc": "Finish Reason.",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "stop", "display_name": "Stop"},
                    <AllowedValueInfo>{"value": "length", "display_name": "Length"},
                    <AllowedValueInfo>{"value": "content_filter", "display_name": "Content Filter"},
                    <AllowedValueInfo>{"value": "insufficient_system_resource", "display_name": "Insufficient System Resource"},
                ),
            },
            "logprobs": {
                "display_name": "Log Probabilities",
                "short_desc": "Log Probabilities",
                "desc": "Log Probabilities.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    #! Creates the typed fields
    /** @param optional whether the object can be absent */
    constructor(bool optional = False) : HashDataType(optional ? AutoHashOrNothingType : AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Shared DeepSeekFimChoiceDataType instance
public const DeepSeekFimChoiceDataType = new DeepSeekFimChoiceDataType();

#! FimResponse data contract
public class DeepSeekFimResponseDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "Completion ID",
                "short_desc": "Completion ID",
                "desc": "Completion ID.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "object": {
                "display_name": "Object Type",
                "short_desc": "Object Type",
                "desc": "Object Type.",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "text_completion", "display_name": "Text Completion"},
                ),
            },
            "created": {
                "display_name": "Created",
                "short_desc": "Created",
                "desc": "Created.",
                "type": AbstractDataProviderTypeMap."date",
            },
            "model": {
                "display_name": "Model",
                "short_desc": "Model",
                "desc": "Model.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "choices": {
                "display_name": "Choices",
                "short_desc": "Choices",
                "desc": "Choices.",
                "type": new ListDataType("DeepSeekFimChoiceDataTypeList", new DeepSeekFimChoiceDataType(), False),
            },
            "usage": {
                "display_name": "Usage",
                "short_desc": "Usage",
                "desc": "Usage.",
                "type": new DeepSeekUsageDataType(True),
            },
            "system_fingerprint": {
                "display_name": "System Fingerprint",
                "short_desc": "System Fingerprint",
                "desc": "System Fingerprint.",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the typed fields
    /** @param optional whether the object can be absent */
    constructor(bool optional = False) : HashDataType(optional ? AutoHashOrNothingType : AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Shared DeepSeekFimResponseDataType instance
public const DeepSeekFimResponseDataType = new DeepSeekFimResponseDataType();


#! ResponseOutputContent data contract
public class DeepSeekResponseOutputContentDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "type": {
                "display_name": "Content Type",
                "short_desc": "Content Type",
                "desc": "Content Type.",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "output_text", "display_name": "Output Text"},
                    <AllowedValueInfo>{"value": "reasoning_text", "display_name": "Reasoning Text"},
                ),
            },
            "text": {
                "display_name": "Text",
                "short_desc": "Text",
                "desc": "Text.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "annotations": {
                "display_name": "Annotations",
                "short_desc": "Annotations",
                "desc": "Annotations.",
                "type": AbstractDataProviderTypeMap."*list",
            },
            "logprobs": {
                "display_name": "Log Probabilities",
                "short_desc": "Log Probabilities",
                "desc": "Log Probabilities.",
                "type": AbstractDataProviderTypeMap."*list",
            },
        };
    }

    #! Creates the typed fields
    /** @param optional whether the object can be absent */
    constructor(bool optional = False) : HashDataType(optional ? AutoHashOrNothingType : AutoHashType) {
        addQoreFields(Fields);
        # Responses metadata is extensible; input items must preserve returned items verbatim.
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

#! Shared DeepSeekResponseOutputContentDataType instance
public const DeepSeekResponseOutputContentDataType = new DeepSeekResponseOutputContentDataType();

#! ResponseOutput data contract
public class DeepSeekResponseOutputDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "type": {
                "display_name": "Item Type",
                "short_desc": "Item Type",
                "desc": "Item Type.",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "message", "display_name": "Message"},
                    <AllowedValueInfo>{"value": "reasoning", "display_name": "Reasoning"},
                    <AllowedValueInfo>{"value": "function_call", "display_name": "Function Call"},
                    <AllowedValueInfo>{"value": "custom_tool_call", "display_name": "Custom Tool Call"},
                    <AllowedValueInfo>{"value": "web_search_call", "display_name": "Web Search Call"},
                ),
            },
            "id": {
                "display_name": "Item ID",
                "short_desc": "Item ID",
                "desc": "Item ID.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Status",
                "desc": "Status.",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "in_progress", "display_name": "In Progress"},
                    <AllowedValueInfo>{"value": "completed", "display_name": "Completed"},
                    <AllowedValueInfo>{"value": "incomplete", "display_name": "Incomplete"},
                ),
            },
            "role": {
                "display_name": "Role",
                "short_desc": "Role",
                "desc": "Role.",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "assistant", "display_name": "Assistant"},
                ),
            },
            "content": {
                "display_name": "Content",
                "short_desc": "Content",
                "desc": "Content.",
                "type": new ListDataType("DeepSeekResponseOutputContentDataTypeList", new DeepSeekResponseOutputContentDataType(), True),
            },
            "call_id": {
                "display_name": "Call ID",
                "short_desc": "Call ID",
                "desc": "Call ID.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "name": {
                "display_name": "Function Name",
                "short_desc": "Function Name",
                "desc": "Function Name.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "arguments": {
                "display_name": "Arguments",
                "short_desc": "Arguments",
                "desc": "JSON-encoded function arguments. Validate before calling the function.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "action": {
                "display_name": "Search Action",
                "short_desc": "Search Action",
                "desc": "Server-side web search details.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "summary": {
                "display_name": "Reasoning Summary",
                "short_desc": "Reasoning Summary",
                "desc": "Reasoning Summary.",
                "type": AbstractDataProviderTypeMap."*list",
            },
            "input": {
                "display_name": "Patch Input",
                "short_desc": "Patch Input",
                "desc": "Patch text generated by the `apply_patch` custom tool.",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the typed fields
    /** @param optional whether the object can be absent */
    constructor(bool optional = False) : HashDataType(optional ? AutoHashOrNothingType : AutoHashType) {
        addQoreFields(Fields);
        # Responses metadata is extensible; input items must preserve returned items verbatim.
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

#! Shared DeepSeekResponseOutputDataType instance
public const DeepSeekResponseOutputDataType = new DeepSeekResponseOutputDataType();

#! ResponseUsage data contract
public class DeepSeekResponseUsageDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "input_tokens": {
                "display_name": "Input Tokens",
                "short_desc": "Input Tokens",
                "desc": "Input Tokens.",
                "type": AbstractDataProviderTypeMap."int",
            },
            "output_tokens": {
                "display_name": "Output Tokens",
                "short_desc": "Output Tokens",
                "desc": "Output Tokens.",
                "type": AbstractDataProviderTypeMap."int",
            },
            "total_tokens": {
                "display_name": "Total Tokens",
                "short_desc": "Total Tokens",
                "desc": "Total Tokens.",
                "type": AbstractDataProviderTypeMap."int",
            },
            "input_tokens_details": {
                "display_name": "Input Token Details",
                "short_desc": "Input Token Details",
                "desc": "Includes the number of `cached_tokens`.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "output_tokens_details": {
                "display_name": "Output Token Details",
                "short_desc": "Output Token Details",
                "desc": "Includes the number of `reasoning_tokens`.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    #! Creates the typed fields
    /** @param optional whether the object can be absent */
    constructor(bool optional = False) : HashDataType(optional ? AutoHashOrNothingType : AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Shared DeepSeekResponseUsageDataType instance
public const DeepSeekResponseUsageDataType = new DeepSeekResponseUsageDataType();

#! CreateResponse data contract
public class DeepSeekCreateResponseDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "Response ID",
                "short_desc": "Response ID",
                "desc": "Response ID.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "object": {
                "display_name": "Object Type",
                "short_desc": "Object Type",
                "desc": "Object Type.",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "response", "display_name": "Response"},
                ),
            },
            "created_at": {
                "display_name": "Created",
                "short_desc": "Created",
                "desc": "Created.",
                "type": AbstractDataProviderTypeMap."date",
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Status",
                "desc": "Status.",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "in_progress", "display_name": "In Progress"},
                    <AllowedValueInfo>{"value": "completed", "display_name": "Completed"},
                    <AllowedValueInfo>{"value": "incomplete", "display_name": "Incomplete"},
                    <AllowedValueInfo>{"value": "failed", "display_name": "Failed"},
                ),
            },
            "error": {
                "display_name": "Error",
                "short_desc": "Error",
                "desc": "Error.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "incomplete_details": {
                "display_name": "Incomplete Details",
                "short_desc": "Incomplete Details",
                "desc": "Incomplete Details.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "model": {
                "display_name": "Model",
                "short_desc": "Model",
                "desc": "Model.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "output": {
                "display_name": "Output",
                "short_desc": "Output",
                "desc": "Output messages, reasoning items, and tool calls.",
                "type": new ListDataType("DeepSeekResponseOutputList", new DeepSeekResponseOutputDataType()),
            },
            "usage": {
                "display_name": "Usage",
                "short_desc": "Usage",
                "desc": "Input, output, and reasoning token usage.",
                "type": new DeepSeekResponseUsageDataType(True),
            },
        };
    }

    #! Creates the typed fields
    /** @param optional whether the object can be absent */
    constructor(bool optional = False) : HashDataType(optional ? AutoHashOrNothingType : AutoHashType) {
        addQoreFields(Fields);
        # Responses metadata is extensible; input items must preserve returned items verbatim.
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

#! Shared DeepSeekCreateResponseDataType instance
public const DeepSeekCreateResponseDataType = new DeepSeekCreateResponseDataType();
}
