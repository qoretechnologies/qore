# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MCP utility data providers

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
#! MCP completion data provider
/** Provides autocompletion suggestions for prompts or resources.
*/
public class McpCompletionDataProvider inherits McpClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "completion",
            "desc": "Provides autocompletion suggestions for prompts or resources",
            "type": "McpCompletionDataProvider",
            "supports_request": True,
        };
    }

    #! Creates the object with the given MCP client
    constructor(McpClient mcp) : McpClientDataProviderBase(mcp) {
    }

    #! Returns the data provider name
    string getName() {
        return "completion";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Provides autocompletion suggestions for prompts or resources";
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return McpCompletionRequestDataType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return McpCompletionResultDataType;
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.ref_type);
        @assert(req.ref_name);
        @assert(req.argument_name);
        @assert(exists req.argument_value);

        @debug("Getting completion for %y/%y: %y=%y", req.ref_type, req.ref_name,
            req.argument_name, req.argument_value);

        hash<auto> ref = {
            "type": req.ref_type,
        };
        if (req.ref_type == McpCompletionRefType::Prompt) {
            ref.name = req.ref_name;
        } else {
            ref.uri = req.ref_name;
        }

        hash<auto> argument = {
            "name": req.argument_name,
            "value": req.argument_value,
        };

        hash<auto> result = mcp.complete(ref, argument);
        @debug {
            printf("Completion returned %d values\n", result.completion.values.size());
        }
        return result.completion;
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! MCP ping data provider
/** Pings the MCP server to verify connectivity.
*/
public class McpPingDataProvider inherits McpClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "ping",
            "desc": "Pings the MCP server to verify connectivity",
            "type": "McpPingDataProvider",
            "supports_request": True,
        };
    }

    #! Creates the object with the given MCP client
    constructor(McpClient mcp) : McpClientDataProviderBase(mcp) {
    }

    #! Returns the data provider name
    string getName() {
        return "ping";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Pings the MCP server to verify connectivity";
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return NOTHING;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return McpPingResultDataType;
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @debug("Pinging MCP server");
        mcp.ping();
        @debug {
            printf("Ping successful\n");
        }
        return {};
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! MCP server discovery data provider
/** Reports the server's supported protocol versions, capabilities, and identity in a single
    request.

    @note introduced in MCP 2026-07-28; with a handshake-based server the same information
        comes from the \c initialize handshake and this provider is not advertised
*/
public class McpDiscoverDataProvider inherits McpClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "discover",
            "desc": "Reports the MCP server's protocol versions, capabilities, and identity",
            "type": "McpDiscoverDataProvider",
            "supports_request": True,
        };
    }

    #! Creates the object with the given MCP client
    constructor(McpClient mcp) : McpClientDataProviderBase(mcp) {
    }

    #! Returns the data provider name
    string getName() {
        return "discover";
    }

    #! Returns the data provider description
    *string getDesc() {
        return ProviderInfo.desc;
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return NOTHING;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return NOTHING;
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @debug("Discovering MCP server capabilities");
        return mcp.discover();
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
