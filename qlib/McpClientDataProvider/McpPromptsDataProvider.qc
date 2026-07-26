# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore McpPromptsDataProvider class definition

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
#! MCP prompts container data provider
/** Provides access to MCP server prompts.

    Children:
    - \c list: list available prompts
    - \c get: get a prompt by name
*/
public class McpPromptsDataProvider inherits McpClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "prompts",
            "desc": "MCP prompts data provider; provides access to server prompts",
            "type": "McpPromptsDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
        };
    }

    #! Creates the object with the given MCP client
    constructor(McpClient mcp) : McpClientDataProviderBase(mcp) {
    }

    #! Returns the data provider name
    string getName() {
        return "prompts";
    }

    #! Returns a list of child data provider names
    private *list<string> getChildProviderNamesImpl() {
        return ("list", "get");
    }

    #! Returns the given child provider or \c NOTHING if the given child is unknown
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        switch (name) {
            case "list":
                return new McpPromptsListDataProvider(mcp);
            case "get":
                return new McpPromptGetDataProvider(mcp);
        }
        return NOTHING;
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! MCP prompts list data provider
/** Lists all available prompts on the MCP server.
*/
public class McpPromptsListDataProvider inherits McpClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "list",
            "desc": "Lists all available prompts on the MCP server",
            "type": "McpPromptsListDataProvider",
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
        return "Lists all available prompts on the MCP server";
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return NOTHING;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return McpPromptsListResultDataType;
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @debug("Listing prompts");
        list<auto> prompts = mcp.listPrompts();
        @debug {
            printf("Found %d prompts\n", prompts.size());
        }
        return {
            "prompts": prompts,
        };
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! MCP prompt get data provider
/** Gets a prompt from the MCP server by name with optional arguments.
*/
public class McpPromptGetDataProvider inherits McpClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get",
            "desc": "Gets a prompt from the MCP server by name with optional arguments",
            "type": "McpPromptGetDataProvider",
            "supports_request": True,
        };
    }

    #! Creates the object with the given MCP client
    constructor(McpClient mcp) : McpClientDataProviderBase(mcp) {
    }

    #! Returns the data provider name
    string getName() {
        return "get";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Gets a prompt from the MCP server by name with optional arguments";
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return McpPromptGetRequestDataType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return McpPromptResultDataType;
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.name);
        @debug("Getting prompt %y with arguments: %y", req.name, req.arguments);
        hash<auto> result = mcp.getPrompt(req.name, req.arguments);
        @debug {
            printf("Prompt %y returned %d messages\n", req.name, result.messages.size());
        }
        return result;
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
