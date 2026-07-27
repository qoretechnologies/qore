# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore McpResourcesDataProvider class definition

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
#! MCP resources container data provider
/** Provides access to MCP server resources.

    Children:
    - \c list: list available resources
    - \c read: read a resource by URI
*/
public class McpResourcesDataProvider inherits McpClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "resources",
            "desc": "MCP resources data provider; provides access to server resources",
            "type": "McpResourcesDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
        };
    }

    #! Creates the object with the given MCP client
    constructor(McpClient mcp) : McpClientDataProviderBase(mcp) {
    }

    #! Returns the data provider name
    string getName() {
        return "resources";
    }

    #! Returns a list of child data provider names
    private *list<string> getChildProviderNamesImpl() {
        return ("list", "read");
    }

    #! Returns the given child provider or \c NOTHING if the given child is unknown
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        switch (name) {
            case "list":
                return new McpResourcesListDataProvider(mcp);
            case "read":
                return new McpResourceReadDataProvider(mcp);
        }
        return NOTHING;
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! MCP resources list data provider
/** Lists all available resources on the MCP server.
*/
public class McpResourcesListDataProvider inherits McpClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "list",
            "desc": "Lists all available resources on the MCP server",
            "type": "McpResourcesListDataProvider",
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
        return "Lists all available resources on the MCP server";
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return NOTHING;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return McpResourcesListResultDataType;
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @debug("Listing resources");
        list<auto> resources = mcp.listResources();
        @debug {
            printf("Found %d resources\n", resources.size());
        }
        return {
            "resources": resources,
        };
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! MCP resource read data provider
/** Reads a resource from the MCP server by URI.
*/
public class McpResourceReadDataProvider inherits McpClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "read",
            "desc": "Reads a resource from the MCP server by URI",
            "type": "McpResourceReadDataProvider",
            "supports_request": True,
        };
    }

    #! Creates the object with the given MCP client
    constructor(McpClient mcp) : McpClientDataProviderBase(mcp) {
    }

    #! Returns the data provider name
    string getName() {
        return "read";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Reads a resource from the MCP server by URI";
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return McpResourceReadRequestDataType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return McpResourceContentDataType;
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.uri);
        @debug("Reading resource: %y", req.uri);
        hash<auto> result = mcp.readResource(req.uri);
        @debug {
            printf("Resource %y returned %d content items\n", req.uri, result.contents.size());
        }
        return result;
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
