# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore McpContentItemDataType class definition

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
#! MCP content item data type
public class McpContentItemDataType inherits HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "type": {
                "type": StringType,
                "display_name": "Type",
                "short_desc": "Content type",
                "desc": "Content type: `text`, `image`, or `resource`",
                "example_value": McpContentItemType::Text,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Text",
                        "short_desc": "Text content",
                        "desc": "Text content item",
                        "value": McpContentItemType::Text,
                    },
                    <AllowedValueInfo>{
                        "display_name": "Image",
                        "short_desc": "Image content",
                        "desc": "Image content item with base64 data and MIME type",
                        "value": McpContentItemType::Image,
                    },
                    <AllowedValueInfo>{
                        "display_name": "Resource",
                        "short_desc": "Resource content",
                        "desc": "Content item referencing a resource URI",
                        "value": McpContentItemType::Resource,
                    },
                ),
            },
            "text": {
                "type": StringOrNothingType,
                "display_name": "Text",
                "short_desc": "Text content",
                "desc": "Text content (for `type=text`)",
            },
            "mimeType": {
                "type": StringOrNothingType,
                "display_name": "MIME Type",
                "short_desc": "Content MIME type",
                "desc": "MIME type of the content",
                "example_value": "text/plain",
            },
            "data": {
                "type": StringOrNothingType,
                "display_name": "Data",
                "short_desc": "Binary data",
                "desc": "Base64-encoded binary data (for `type=image`)",
            },
            "uri": {
                "type": UriOrNothingType,
                "display_name": "URI",
                "short_desc": "Resource URI",
                "desc": "Resource URI (for `type=resource`)",
            },
            "isError": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Error",
                "short_desc": "Error indicator",
                "desc": "Whether this content item represents an error",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! MCP content item data type constant
public const McpContentItemDataType = new McpContentItemDataType();
}
