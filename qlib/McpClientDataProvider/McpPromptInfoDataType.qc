# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore McpPromptInfoDataType class definition

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
#! MCP prompt argument data type
public class McpPromptArgumentDataType inherits HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "Argument name",
                "desc": "The name of the prompt argument",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Argument description",
                "desc": "Description of the argument",
            },
            "required": {
                "type": SoftBoolOrNothingType,
                "display_name": "Required",
                "short_desc": "Is required",
                "desc": "Whether the argument is required",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! MCP prompt argument data type constant
public const McpPromptArgumentDataType = new McpPromptArgumentDataType();

#! MCP prompt information data type
public class McpPromptInfoDataType inherits HashDataType {
    private {
        #! Field descriptions for simple types
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "Prompt name",
                "desc": "The unique name of the prompt",
                "example_value": "code_review",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Prompt description",
                "desc": "Human-readable description of the prompt",
            },
        };

        #! Arguments field description
        const ArgumentsField = {
            "display_name": "Arguments",
            "short_desc": "Prompt arguments",
            "desc": "List of arguments the prompt accepts",
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
        addField(new QoreDataField("arguments", ArgumentsField.desc,
            new ListDataType("arguments", McpPromptArgumentDataType, True)));
    }
}

#! MCP prompt info data type constant
public const McpPromptInfoDataType = new McpPromptInfoDataType();

#! MCP prompt message data type
public class McpPromptMessageDataType inherits HashDataType {
    private {
        #! Field descriptions for simple types
        const Fields = {
            "role": {
                "type": StringType,
                "display_name": "Role",
                "short_desc": "Message role",
                "desc": "The role of the message sender: user or assistant",
                "example_value": McpPromptMessageRole::User,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "User",
                        "short_desc": "User message",
                        "desc": "A message authored by the user",
                        "value": McpPromptMessageRole::User,
                    },
                    <AllowedValueInfo>{
                        "display_name": "Assistant",
                        "short_desc": "Assistant message",
                        "desc": "A message authored by the assistant",
                        "value": McpPromptMessageRole::Assistant,
                    },
                ),
            },
        };

        #! Content field description
        const ContentField = {
            "display_name": "Content",
            "short_desc": "Message content",
            "desc": "The content of the message",
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
        addField(new QoreDataField("content", ContentField.desc, McpContentItemDataType));
    }
}

#! MCP prompt message data type constant
public const McpPromptMessageDataType = new McpPromptMessageDataType();

#! MCP prompt result data type
public class McpPromptResultDataType inherits HashDataType {
    private {
        #! Field descriptions for simple types
        const Fields = {
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Prompt description",
                "desc": "Optional description of the prompt result",
            },
        };

        #! Messages field description
        const MessagesField = {
            "display_name": "Messages",
            "short_desc": "Prompt messages",
            "desc": "List of prompt messages",
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
        addField(new QoreDataField("messages", MessagesField.desc,
            new ListDataType("messages", McpPromptMessageDataType, True)));
    }
}

#! MCP prompt result data type constant
public const McpPromptResultDataType = new McpPromptResultDataType();
}
