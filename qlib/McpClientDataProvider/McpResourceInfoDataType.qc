# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore McpResourceInfoDataType class definition

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
#! MCP resource information data type
public class McpResourceInfoDataType inherits HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "uri": {
                "type": StringType,
                "display_name": "URI",
                "short_desc": "Resource URI",
                "desc": "The unique URI identifying the resource",
                "example_value": "file:///config.json",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "Resource name",
                "desc": "Human-readable name for the resource",
                "example_value": "Configuration File",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Resource description",
                "desc": "Human-readable description of the resource",
            },
            "mimeType": {
                "type": StringOrNothingType,
                "display_name": "MIME Type",
                "short_desc": "Content MIME type",
                "desc": "MIME type of the resource content",
                "example_value": "application/json",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! MCP resource info data type constant
public const McpResourceInfoDataType = new McpResourceInfoDataType();

#! MCP resource content data type
public class McpResourceContentDataType inherits HashDataType {
    private {
        #! Contents field description
        const ContentsField = {
            "display_name": "Contents",
            "short_desc": "Resource contents",
            "desc": "List of resource content items",
        };
    }

    #! Creates the object
    constructor() {
        addField(new QoreDataField("contents", ContentsField.desc,
            new ListDataType("contents", McpContentItemDataType, True)));
    }
}

#! MCP resource content data type constant
public const McpResourceContentDataType = new McpResourceContentDataType();
}
