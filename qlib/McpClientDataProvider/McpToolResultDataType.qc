# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore McpToolResultDataType class definition

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
#! MCP tool result data type
public class McpToolResultDataType inherits HashDataType {
    private {
        #! Field descriptions for simple types
        const Fields = {
            "isError": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Error",
                "short_desc": "Error indicator",
                "desc": "Whether the tool call resulted in an error",
                "default_value": False,
            },
            "resultType": {
                "type": StringOrNothingType,
                "display_name": "Result Type",
                "short_desc": "Result discriminator",
                "desc": "The kind of result returned: \"complete\" for a final result, "
                    "\"input_required\" when the server needs more information, or \"task\" "
                    "when the call was accepted as a task.  Servers implementing protocol "
                    "revisions before 2026-07-28 omit it, which must be read as \"complete\"",
                "example_value": "complete",
            },
            "structuredContent": {
                "type": AutoType,
                "display_name": "Structured Content",
                "short_desc": "Structured tool output",
                "desc": "The tool's structured output, validated against its output schema; "
                    "may be any JSON value",
            },
        };

        #! Content field description
        const ContentField = {
            "display_name": "Content",
            "short_desc": "Result content",
            "desc": "List of content items returned by the tool",
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
        addField(new QoreDataField("content", ContentField.desc, new ListDataType("content",
            McpContentItemDataType, True)));
    }
}

#! MCP tool result data type constant
public const McpToolResultDataType = new McpToolResultDataType();
}
