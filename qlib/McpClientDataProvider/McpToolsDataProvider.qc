# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore McpToolsDataProvider class definition

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
#! MCP tools container data provider
/** Provides access to MCP server tools.

    Children:
    - \c list: list available tools
    - \c call: call a tool
*/
public class McpToolsDataProvider inherits McpClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "tools",
            "desc": "MCP tools data provider; provides access to server tools",
            "type": "McpToolsDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
        };
    }

    #! Creates the object with the given MCP client
    constructor(McpClient mcp) : McpClientDataProviderBase(mcp) {
    }

    #! Returns the data provider name
    string getName() {
        return "tools";
    }

    #! Returns a list of child data provider names
    private *list<string> getChildProviderNamesImpl() {
        return ("list", "call");
    }

    #! Returns the given child provider or \c NOTHING if the given child is unknown
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        switch (name) {
            case "list":
                return new McpToolsListDataProvider(mcp);
            case "call":
                return new McpToolCallDataProvider(mcp);
        }
        return NOTHING;
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! MCP tools list data provider
/** Lists all available tools on the MCP server.
*/
public class McpToolsListDataProvider inherits McpClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "list",
            "desc": "Lists all available tools on the MCP server",
            "type": "McpToolsListDataProvider",
            "supports_request": True,
        };
    }

    #! Creates the object with the given MCP client
    constructor(McpClient mcp) : McpClientDataProviderBase(mcp) {
    }

    #! Returns the data provider name
    string getName() {
        return "list";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Lists all available tools on the MCP server";
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return NOTHING;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return McpToolsListResultDataType;
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @debug("Listing tools");
        list<auto> tools = mcp.listTools();
        @debug {
            printf("Found %d tools\n", tools.size());
        }
        return {
            "tools": tools,
        };
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! MCP tool call data provider
/** Calls an MCP tool by name with arguments.
*/
public class McpToolCallDataProvider inherits McpClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "call",
            "desc": "Calls an MCP tool by name with the provided arguments",
            "type": "McpToolCallDataProvider",
            "supports_request": True,
        };
    }

    #! Creates the object with the given MCP client
    constructor(McpClient mcp) : McpClientDataProviderBase(mcp) {
    }

    #! Returns the data provider name
    string getName() {
        return "call";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Calls an MCP tool by name with the provided arguments";
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return McpToolCallRequestDataType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return McpToolResultDataType;
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.name);
        @debug("Calling tool %y with arguments: %y", req.name, req.arguments);
        hash<auto> result = mcp.callTool(req.name, req.arguments);
        @debug {
            printf("Tool %y returned: %y\n", req.name, result);
        }
        return result;
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
