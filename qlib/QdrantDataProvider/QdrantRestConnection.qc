# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QdrantRestConnection class definition

/** QdrantRestConnection.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the QdrantDataProvider module
public namespace QdrantDataProvider {
#! Qdrant REST connection
public class QdrantRestConnection inherits RestClient::RestConnection {
    public {
        #! Default ping method
        const DefaultPingMethod = "GET";

        #! Default ping path
        /** \c "/collections" and not \c "/": qdrant serves the root path (a title/version banner)
            outside its authentication layer, so a ping against \c "/" returns 200 even when the
            connection carries no credential at all - reporting a connection that cannot perform a
            single real operation as healthy.  \c "GET /collections" is cheap, read-only, always
            present, and answers 401 when the credential is missing or wrong, so it verifies
            authentication as well as reachability.

            Servers with no API key configured answer \c "GET /collections" with 200, so this
            default is correct for unauthenticated qdrant servers as well.
        */
        const DefaultPingPath = "/collections";

        #! Default ping headers
        /** \c "Connection: close" forces a fresh TCP connection on each ping, preventing stale
            keep-alive connection failures against qdrant's actix-web server, which closes idle
            keep-alive connections after a short timeout.
        */
        const DefaultPingHeaders = {"Connection": "close"};

        #! Connection entry info
        const ConnectionScheme = <ConnectionSchemeInfo>{
            "display_name": "Qdrant REST Connection",
            "short_desc": "A connection to a Qdrant vector database server",
            "desc": "A connection to a Qdrant vector database server",
            "cls": Class::forName("QdrantRestConnection"),
            "schemes": {
                "qdrantrest": True,
                "qdrantrests": True,
            },
            "base_scheme_map": {
                "https": "qdrantrests",
                "http": "qdrantrest",
            },
            "options": RestConnection::ConnectionScheme.options + {
                "api_key": <ConnectionOptionInfo>{
                    "display_name": "API Key",
                    "short_desc": "The Qdrant API key for authentication",
                    "type": "string",
                    "desc": "The API key for authenticating with the Qdrant server; sent as the `api-key` header",
                    "sensitive": True,
                },
                "ping_method": RestConnection::ConnectionScheme.options."ping_method"
                    + <ConnectionOptionInfo>{
                    "default_value": DefaultPingMethod,
                },
                "ping_path": RestConnection::ConnectionScheme.options."ping_path"
                    + <ConnectionOptionInfo>{
                    "default_value": DefaultPingPath,
                    "desc": "The URI path to use for pings; defaults to `/collections`, which requires "
                        "authentication and therefore verifies the credential as well as reachability.  "
                        "Do not use `/`: qdrant serves the root path outside its authentication layer, so "
                        "a ping against it succeeds even when the connection carries no credential at all "
                        "and no real operation can be performed",
                },
                "ping_headers": RestConnection::ConnectionScheme.options."ping_headers"
                    + <ConnectionOptionInfo>{
                    "default_value": DefaultPingHeaders,
                    "desc": "HTTP headers to send with ping requests; defaults to "
                        "`{\"Connection\": \"close\"}` to force a fresh TCP connection on "
                        "each ping, preventing stale keep-alive connection failures against "
                        "qdrant's actix-web server (which closes idle keep-alive connections "
                        "after a short timeout)",
                },
            },
        };
    }

    #! Creates the QdrantRestConnection object
    /** @param name the name of the connection
        @param description connection description
        @param url connection URL (potentially with password info)
        @param attributes various attributes. See below
        @param options connection options

        See \c AbstractConnection::constructor() for \c attributes and \c options reference.

        @throw CONNECTION-OPTION-ERROR missing or invalid connection option
    */
    constructor(string name, string description, string url, hash<auto> attributes = {},
            hash<auto> options = {})
            : RestConnection(name, description, url, attributes, options) {
    }

    #! Creates the QdrantRestConnection object
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
    constructor(hash<auto> config, *hash<auto> attr) : RestConnection(config, attr) {
    }

    #! Returns \c "qdrantrest"
    string getType() {
        return "qdrantrest";
    }

    #! Returns a data provider object for this connection
    /** @param constructor_options any additional constructor options for the data provider

        @return a data provider object for this connection; the data provider is @ref QdrantDataProvider
    */
    DataProvider::AbstractDataProvider getDataProvider(*hash<auto> constructor_options) {
        return new QdrantDataProvider({"restclient": getAsync()});
    }

    #! Returns the ConnectionSchemeInfo hash for this object
    private hash<ConnectionSchemeInfo> getConnectionSchemeInfoImpl() {
        return ConnectionScheme;
    }

    #! Returns connection options with API key header set if configured
    *hash<auto> getConnectionOptions(*hash<auto> rtopts) {
        hash<auto> opts = RestConnection::getConnectionOptions(rtopts);
        if (opts.api_key) {
            opts.headers."api-key" = remove opts.api_key;
        }
        return opts;
    }
}
}
