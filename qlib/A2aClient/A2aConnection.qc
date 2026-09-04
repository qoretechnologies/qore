# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file A2aConnection.qc @brief A2A connection provider implementation

/*  A2aConnection.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! the %A2aClient namespace holds all public definitions in the %A2aClient module
public namespace A2aClient {

#! Class for A2A client connections; returns @ref A2aClient objects
/** supports the following options:
    - \c "connect_timeout": connection timeout to use in milliseconds (default: 30000)
    - \c "timeout": transfer timeout to use in milliseconds (default: 60000)

    @see @ref A2aClient::A2aClient "A2aClient::constructor()" for more information on the above options
*/
public class A2aConnection inherits ConnectionProvider::HttpBasedConnection {
    public {
        #! Real options for creating the A2A client
        hash<auto> real_opts;

        #! Connection entry info
        const ConnectionScheme = <ConnectionSchemeInfo>{
            "display_name": "A2A Connection",
            "short_desc": "A connection to an A2A (Agent-to-Agent) agent",
            "desc": "A connection to an A2A agent for agent-to-agent collaboration via JSON-RPC 2.0",
            "cls": Class::forName("A2aClient::A2aConnection"),
            "options": HttpConnection::ConnectionScheme.options,
            "schemes": {
                "a2as": True,
                "a2a": True,
            },
            "base_scheme_map": {
                "https": "a2as",
                "http": "a2a",
            },
        };
    }

    #! Creates the A2aConnection object
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

    #! returns \c "a2a"
    string getType() {
        return "a2a";
    }

    #! returns an @ref A2aClient object
    /** @param connect if \c True, then the agent card is fetched on connection
        @param rtopts this connection type does not accept any runtime options, so this parameter is ignored

        @return an @ref A2aClient object
    */
    private A2aClient getImpl(bool connect = True, *hash<auto> rtopts) {
        # Convert a2a/a2as URL to http/https
        string http_url = real_opts.url;
        if (http_url =~ /^a2a:/) {
            http_url = regex_subst(http_url, "^a2a:", "http:");
        } else if (http_url =~ /^a2as:/) {
            http_url = regex_subst(http_url, "^a2as:", "https:");
        }

        hash<auto> client_opts = real_opts - "url";
        client_opts.url = http_url;

        A2aClient client(http_url, client_opts);

        if (connect) {
            client.getAgentCard();
        }

        return client;
    }

    #! Returns an A2A client backed by HttpClientIo
    /** @param connect if \c True, then the agent card is fetched before returning
        @param rtopts this connection type does not accept runtime options

        @return an async-I/O-capable @ref A2aClient object
    */
    private A2aClient getAsyncImpl(bool connect = True, *hash<auto> rtopts) {
        return getImpl(connect, rtopts);
    }

    #! Returns the ConnectionSchemeInfo hash for this object
    private hash<ConnectionSchemeInfo> getConnectionSchemeInfoImpl() {
        return ConnectionScheme;
    }
}

} # namespace A2aClient
