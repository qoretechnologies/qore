# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore McpToolInfoDataType class definition

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
#! MCP tool information data type
public class McpToolInfoDataType inherits HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "Tool name",
                "desc": "The unique name of the tool",
                "example_value": "get_weather",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Tool description",
                "desc": "Human-readable description of what the tool does",
            },
            "inputSchema": {
                "type": AutoHashOrNothingType,
                "display_name": "Input Schema",
                "short_desc": "JSON Schema for input",
                "desc": "JSON Schema defining the tool's input parameters; properties may "
                    "carry an \"x-mcp-header\" annotation naming an HTTP header that the "
                    "client mirrors the value into",
            },
            "outputSchema": {
                "type": AutoHashOrNothingType,
                "display_name": "Output Schema",
                "short_desc": "JSON Schema for structured output",
                "desc": "JSON Schema describing the tool's structured content, if any",
            },
            "execution": {
                "type": AutoHashOrNothingType,
                "display_name": "Execution",
                "short_desc": "Execution metadata",
                "desc": "Execution metadata such as \"taskSupport\", which tells the client "
                    "whether the server may answer a call with a task handle instead of a "
                    "result",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! MCP tool info data type constant
public const McpToolInfoDataType = new McpToolInfoDataType();
}
