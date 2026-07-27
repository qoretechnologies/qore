# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file McpConnection.qc @brief MCP connection provider implementation

/*  McpConnection.qc Copyright (C) 2025 Qore Technologies, s.r.o.

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

#! the %McpClient namespace holds all public definitions in the %McpClient module
public namespace McpClient {

#! Class for MCP client connections; returns @ref McpClient objects
/** supports the following options:
    - \c "connect_timeout": connection timeout to use in milliseconds (default: 30000)
    - \c "timeout": transfer timeout to use in milliseconds (default: 60000)
    - \c "client_name": the client name to use for initialization (default: "QoreMcpClient")
    - \c "client_version": the client version to use for initialization (default: "1.0")

    @see @ref McpClient::McpClient "McpClient::constructor()" for more information on the above options
*/
public class McpConnection inherits ConnectionProvider::HttpBasedConnection {
    public {
        #! Real options for creating the MCP client
        hash<auto> real_opts;

        #! Connection entry info
        const ConnectionScheme = <ConnectionSchemeInfo>{
            "display_name": "MCP Connection",
            "short_desc": "A connection to an MCP (Model Context Protocol) server",
            "desc": "A connection to an MCP server for accessing AI tools, resources, and prompts",
            "cls": Class::forName("McpClient::McpConnection"),
            "options": HttpConnection::ConnectionScheme.options + {
                "client_name": <ConnectionOptionInfo>{
                    "display_name": "Client Name",
                    "short_desc": "The client name to use for MCP initialization",
                    "type": "string",
                    "desc": "The name of this client to report to the MCP server during initialization",
                    "default_value": McpClientName,
                },
                "client_version": <ConnectionOptionInfo>{
                    "display_name": "Client Version",
                    "short_desc": "The client version to use for MCP initialization",
                    "type": "string",
                    "desc": "The version of this client to report to the MCP server during initialization",
                    "default_value": McpClientVersion,
                },
            },
            "schemes": {
                "mcps": True,
                "mcp": True,
            },
            "base_scheme_map": {
                "https": "mcps",
                "http": "mcp",
            },
        };
    }

    #! Creates the McpConnection object
    /** @param name the name of the connection
        @param description connection description
        @param url connection URL (potentially with password info)
        @param attributes various attributes. See below
        @param options connection options

        See \c AbstractConnection::constructor() for \c attributes and \c options reference.
    */
    constructor(string name, string description, string url, hash<auto> attributes = {}, hash<auto> options = {})
            : HttpBasedConnection(name, description, url, attributes, options) {
        real_opts = {"url": real_url} + opts;
    }

    #! Creates the object from the arguments
    /** @param config with the following keys:
        - name (required string): the connection name
        - display_name (optional string): the display name
        - short_desc (optional string): a short description in plain text
        - desc (optional string): a long description with markdown formatting
        - url (required string): the connection URL
        - opts (optional hash): connection options
        - logger (optional LoggerInterface object): logger for the connection
        @param attr optional connection attributes
        - monitor (optional bool): should the connection be monitored? Default: True
        - enabled (optional bool): is the connection enabled? Default: True
        - locked (optional bool): is the connection locked? Default: False
        - debug_data (optional bool): debug data? Default: False
        - tags (optional hash): tags for the connection; no default value

        @throw CONNECTION-OPTION-ERROR missing or invalid connection option or attribute
    */
    constructor(hash<auto> config, *hash<auto> attr) : HttpBasedConnection(config, attr) {
        real_opts = {"url": real_url} + opts;
    }

    #! returns \c "mcp"
    string getType() {
        return "mcp";
    }

    #! returns a @ref McpClient object
    /** @param connect if \c True, then the connection is returned already initialized
        @param rtopts this connection type does not accept any runtime options, so this parameter is ignored

        @return a @ref McpClient object
    */
    private McpClient getImpl(bool connect = True, *hash<auto> rtopts) {
        # Convert mcp/mcps URL to http/https
        string http_url = real_opts.url;
        if (http_url =~ /^mcp:/) {
            http_url = regex_subst(http_url, "^mcp:", "http:");
        } else if (http_url =~ /^mcps:/) {
            http_url = regex_subst(http_url, "^mcps:", "https:");
        }

        hash<auto> client_opts = real_opts - "url";
        client_opts.url = http_url;

        McpClient client(http_url, client_opts);

        if (connect) {
            string client_name = opts.client_name ?? McpClientName;
            string client_version = opts.client_version ?? McpClientVersion;

            client.initialize(<McpClientInfo>{
                "name": client_name,
                "version": client_version,
            });
        }

        return client;
    }

    #! Returns the ConnectionSchemeInfo hash for this object
    private hash<ConnectionSchemeInfo> getConnectionSchemeInfoImpl() {
        return ConnectionScheme;
    }
}

} # namespace McpClient
