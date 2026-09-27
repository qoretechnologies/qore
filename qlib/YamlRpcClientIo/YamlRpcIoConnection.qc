# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file YamlRpcIoConnection.qc @brief YAML-RPC async I/O connection provider implementation

/*  YamlRpcIoConnection.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! the %YamlRpcClientIo namespace holds all public definitions in the %YamlRpcClientIo module
public namespace YamlRpcClientIo {

#! Class for YAML-RPC async I/O client connections; returns @ref YamlRpcClientIo objects
/** supports the following options:
    - \c "connect_timeout": connection timeout to use in milliseconds (default: 45000)
    - \c "timeout": transfer timeout to use in milliseconds (default: 45000)

    @see @ref YamlRpcClientIo::YamlRpcClientIo "YamlRpcClientIo::constructor()" for more information
    on the above options
*/
public class YamlRpcIoConnection inherits ConnectionProvider::HttpBasedConnection {
    public {
        #! Real options for creating the YAML-RPC client
        hash<auto> real_opts;

        #! Connection entry info
        const ConnectionScheme = <ConnectionSchemeInfo>{
            "display_name": "YAML-RPC (Async I/O) Connection",
            "short_desc": "A connection to a YAML-RPC server using async I/O",
            "desc": "A connection to a YAML-RPC server using async I/O via HttpClientIo with automatic "
                "protocol negotiation (HTTP/3, HTTP/2, HTTP/1.1)",
            "cls": Class::forName("YamlRpcClientIo::YamlRpcIoConnection"),
            "options": HttpConnection::ConnectionScheme.options,
            "schemes": {
                "yamlrpcios": True,
                "yamlrpcio": True,
            },
            "base_scheme_map": {
                "https": "yamlrpcios",
                "http": "yamlrpcio",
            },
        };
    }

    #! Creates the YamlRpcIoConnection object
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

    #! returns \c "yamlrpcio"
    string getType() {
        return "yamlrpcio";
    }

    #! returns a @ref YamlRpcClientIo object
    /** @param connect if \c True, the connection is returned ready to use; this parameter is accepted
        for compatibility but connections are established on demand
        @param rtopts this connection type does not accept any runtime options, so this parameter is ignored

        @return a @ref YamlRpcClientIo object
    */
    private YamlRpcClientIo getImpl(bool connect = True, *hash<auto> rtopts) {
        # Convert yamlrpcio/yamlrpcios URL to http/https
        string http_url = real_opts.url;
        if (http_url =~ /^yamlrpcios:/) {
            http_url = regex_subst(http_url, "^yamlrpcios:", "https:");
        } else if (http_url =~ /^yamlrpcio:/) {
            http_url = regex_subst(http_url, "^yamlrpcio:", "http:");
        }

        hash<auto> client_opts = real_opts - "url";

        return new YamlRpcClientIo(http_url, client_opts);
    }

    #! Returns the ConnectionSchemeInfo hash for this object
    private hash<ConnectionSchemeInfo> getConnectionSchemeInfoImpl() {
        return ConnectionScheme;
    }
}

} # namespace YamlRpcClientIo
