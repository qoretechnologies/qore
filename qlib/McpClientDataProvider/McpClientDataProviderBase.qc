# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore McpClientDataProviderBase class definition

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
#! Base class for MCP client data providers
public class McpClientDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Constructor options
        const ConstructorOptions = {
            "mcp": <DataProviderOptionInfo>{
                "display_name": "MCP Client",
                "short_desc": "The MCP client object",
                "type": AbstractDataProviderType::get(new Type("McpClient")),
                "desc": "The MCP client object to use for API calls",
            },
            "mcpconnection": <DataProviderOptionInfo>{
                "display_name": "MCP Connection",
                "short_desc": "The name of the MCP connection",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The name of the MCP connection to use",
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "URL",
                "short_desc": "The URL to the MCP server",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The URL to the MCP server (`mcp://` or `mcps://`)",
            },
            "client_name": <DataProviderOptionInfo>{
                "display_name": "Client Name",
                "short_desc": "Client name for MCP initialization",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The client name to report to the MCP server when using the URL option",
            },
            "client_version": <DataProviderOptionInfo>{
                "display_name": "Client Version",
                "short_desc": "Client version for MCP initialization",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The client version to report to the MCP server when using the URL option",
            },
        };
    }

    private {
        #! The MCP client
        McpClient mcp;
    }

    #! Creates the object with the given MCP client
    constructor(McpClient mcp) {
        @assert(mcp);
        self.mcp = mcp;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *McpClient mcp = McpClientDataProviderBase::getMcpClient(options);
        if (!mcp) {
            mcp = McpClientDataProviderBase::getMcpClientFromConnection(options);
        }
        @assert(mcp);
        self.mcp = mcp;
    }

    #! Returns the MCP client
    McpClient getMcpClient() {
        return mcp;
    }

    #! Returns the MCP client from options
    static *McpClient getMcpClient(*hash<auto> options) {
        if (options.mcp) {
            return options.mcp;
        }
        if (options.url) {
            @debug("Creating MCP client from URL: %y", options.url);
            return new McpClient(options.url, {
                "name": options.client_name ?? "QoreDataProvider",
                "version": options.client_version ?? "1.0",
            });
        }
        return NOTHING;
    }

    #! Returns the MCP client from a connection
    static *McpClient getMcpClientFromConnection(*hash<auto> options) {
        if (options.mcpconnection) {
            @debug("Getting MCP client from connection: %y", options.mcpconnection);
            AbstractConnection conn = get_connection(options.mcpconnection);
            return conn.get();
        }
        return NOTHING;
    }
}
}
