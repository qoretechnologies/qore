# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file OneRecordConnection.qc @brief ONE Record connection provider implementation

/*  OneRecordConnection.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! the %OneRecordClient namespace holds all public definitions in the %OneRecordClient module
public namespace OneRecordClient {

#! Class for ONE Record client connections; returns @ref OneRecordClient objects
/** supports the following options:
    - \c "connect_timeout": connection timeout to use in milliseconds (default: 30000)
    - \c "timeout": transfer timeout to use in milliseconds (default: 60000)

    @see @ref OneRecordClient::OneRecordClient "OneRecordClient::constructor()" for more
    information on the above options

    @since json 1.12
*/
public class OneRecordConnection inherits ConnectionProvider::HttpBasedConnection {
    public {
        #! Real options for creating the ONE Record client
        hash<auto> real_opts;

        #! Connection entry info
        const ConnectionScheme = <ConnectionSchemeInfo>{
            "display_name": "ONE Record Connection",
            "short_desc": "A connection to an IATA ONE Record server",
            "desc": "A connection to an IATA ONE Record server for air cargo data sharing via "
                "REST/JSON-LD",
            "cls": Class::forName("OneRecordClient::OneRecordConnection"),
            "options": HttpConnection::ConnectionScheme.options,
            "schemes": {
                "1rs": True,
                "1r": True,
            },
            "base_scheme_map": {
                "https": "1rs",
                "http": "1r",
            },
        };
    }

    #! Creates the OneRecordConnection object
    /** @param name the name of the connection
        @param description connection description
        @param url connection URL (potentially with password info)
        @param attributes various attributes. See below
        @param options connection options

        See \c AbstractConnection::constructor() for \c attributes and \c options reference.
    */
    constructor(string name, string description, string url, hash<auto> attributes = {},
            hash<auto> options = {})
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

        @throw CONNECTION-OPTION-ERROR missing or invalid connection option or attribute
    */
    constructor(hash<auto> config, *hash<auto> attr) : HttpBasedConnection(config, attr) {
        real_opts = {"url": real_url} + opts;
    }

    #! returns \c "1r"
    string getType() {
        return "1r";
    }

    #! returns a @ref OneRecordClient object
    /** @param connect if \c True, then the server info is fetched on connection
        @param rtopts this connection type does not accept any runtime options, so this parameter
        is ignored

        @return a @ref OneRecordClient object
    */
    private OneRecordClient getImpl(bool connect = True, *hash<auto> rtopts) {
        # Convert 1r/1rs URL to http/https
        string http_url = real_opts.url;
        if (http_url =~ /^1r:/) {
            http_url = regex_subst(http_url, "^1r:", "http:");
        } else if (http_url =~ /^1rs:/) {
            http_url = regex_subst(http_url, "^1rs:", "https:");
        }

        hash<auto> client_opts = real_opts - "url";
        client_opts.url = http_url;

        OneRecordClient client(http_url, <OneRecordClientOptions>{
            "connect_timeout": client_opts.connect_timeout,
            "timeout": client_opts.timeout,
        });

        if (connect) {
            client.getServerInfo();
        }

        return client;
    }

    #! Returns the ConnectionSchemeInfo hash for this object
    private hash<ConnectionSchemeInfo> getConnectionSchemeInfoImpl() {
        return ConnectionScheme;
    }
}

} # namespace OneRecordClient
