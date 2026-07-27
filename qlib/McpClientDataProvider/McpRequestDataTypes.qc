# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MCP request data types

/*  Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the McpClientDataProvider module
public namespace McpClientDataProvider {
#! MCP tool call request data type
public class McpToolCallRequestDataType inherits HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Tool Name",
                "short_desc": "Name of the tool to call",
                "desc": "The name of the MCP tool to execute",
                "example_value": "get_weather",
            },
            "arguments": {
                "type": AutoHashOrNothingType,
                "display_name": "Arguments",
                "short_desc": "Tool arguments",
                "desc": "Arguments to pass to the tool (structure depends on the tool's `inputSchema`)",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! MCP tool call request data type constant
public const McpToolCallRequestDataType = new McpToolCallRequestDataType();

#! MCP resource read request data type
public class McpResourceReadRequestDataType inherits HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "uri": {
                "type": StringType,
                "display_name": "URI",
                "short_desc": "Resource URI",
                "desc": "The URI of the resource to read",
                "example_value": "file:///config.json",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! MCP resource read request data type constant
public const McpResourceReadRequestDataType = new McpResourceReadRequestDataType();

#! MCP prompt get request data type
public class McpPromptGetRequestDataType inherits HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Prompt Name",
                "short_desc": "Name of the prompt",
                "desc": "The name of the prompt to retrieve",
                "example_value": "code_review",
            },
            "arguments": {
                "type": AutoHashOrNothingType,
                "display_name": "Arguments",
                "short_desc": "Prompt arguments",
                "desc": "Arguments to pass to the prompt template",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! MCP prompt get request data type constant
public const McpPromptGetRequestDataType = new McpPromptGetRequestDataType();

#! MCP completion request data type
public class McpCompletionRequestDataType inherits HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "ref_type": {
                "type": StringType,
                "display_name": "Reference Type",
                "short_desc": "Type of completion reference",
                "desc": "The type of completion reference: `ref/prompt` or `ref/resource`",
                "example_value": McpCompletionRefType::Prompt,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Prompt",
                        "short_desc": "Prompt reference",
                        "desc": "Use a prompt name as the reference",
                        "value": McpCompletionRefType::Prompt,
                    },
                    <AllowedValueInfo>{
                        "display_name": "Resource",
                        "short_desc": "Resource reference",
                        "desc": "Use a resource URI as the reference",
                        "value": McpCompletionRefType::Resource,
                    },
                ),
            },
            "ref_name": {
                "type": StringType,
                "display_name": "Reference Name",
                "short_desc": "Name or URI of the reference",
                "desc": "The name (for prompts) or URI (for resources) to complete",
            },
            "argument_name": {
                "type": StringType,
                "display_name": "Argument Name",
                "short_desc": "Argument to complete",
                "desc": "The name of the argument to get completions for",
            },
            "argument_value": {
                "type": StringType,
                "display_name": "Argument Value",
                "short_desc": "Partial value",
                "desc": "The partial value to complete",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! MCP completion request data type constant
public const McpCompletionRequestDataType = new McpCompletionRequestDataType();

#! MCP completion result data type
public class McpCompletionResultDataType inherits HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "values": {
                "type": AutoListType,
                "display_name": "Values",
                "short_desc": "Completion values",
                "desc": "List of completion suggestions",
            },
            "total": {
                "type": SoftIntOrNothingType,
                "display_name": "Total",
                "short_desc": "Total count",
                "desc": "Total number of available completions (if known)",
            },
            "hasMore": {
                "type": SoftBoolOrNothingType,
                "display_name": "Has More",
                "short_desc": "More available",
                "desc": "Whether there are more completions available",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! MCP completion result data type constant
public const McpCompletionResultDataType = new McpCompletionResultDataType();

#! MCP ping result data type (empty response)
public class McpPingResultDataType inherits HashDataType {
    #! Creates the object
    constructor() {
    }
}

#! MCP ping result data type constant
public const McpPingResultDataType = new McpPingResultDataType();

#! MCP tools list result data type
public class McpToolsListResultDataType inherits HashDataType {
    private {
        #! Tools field description
        const ToolsField = {
            "display_name": "Tools",
            "short_desc": "Available tools",
            "desc": "List of tools available on the server",
        };
    }

    #! Creates the object
    constructor() {
        addField(new QoreDataField("tools", ToolsField.desc,
            new ListDataType("tools", McpToolInfoDataType, True)));
    }
}

#! MCP tools list result data type constant
public const McpToolsListResultDataType = new McpToolsListResultDataType();

#! MCP resources list result data type
public class McpResourcesListResultDataType inherits HashDataType {
    private {
        #! Resources field description
        const ResourcesField = {
            "display_name": "Resources",
            "short_desc": "Available resources",
            "desc": "List of resources available on the server",
        };
    }

    #! Creates the object
    constructor() {
        addField(new QoreDataField("resources", ResourcesField.desc,
            new ListDataType("resources", McpResourceInfoDataType, True)));
    }
}

#! MCP resources list result data type constant
public const McpResourcesListResultDataType = new McpResourcesListResultDataType();

#! MCP prompts list result data type
public class McpPromptsListResultDataType inherits HashDataType {
    private {
        #! Prompts field description
        const PromptsField = {
            "display_name": "Prompts",
            "short_desc": "Available prompts",
            "desc": "List of prompts available on the server",
        };
    }

    #! Creates the object
    constructor() {
        addField(new QoreDataField("prompts", PromptsField.desc,
            new ListDataType("prompts", McpPromptInfoDataType, True)));
    }
}

#! MCP prompts list result data type constant
public const McpPromptsListResultDataType = new McpPromptsListResultDataType();
}
