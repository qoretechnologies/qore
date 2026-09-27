# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConnectConnection.qc Connect connection provider

/*  ConnectConnection.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace ConnectDataProvider {

#! Connect connection provider
/** Provides \c connect:// and \c connects:// URL schemes for the
    ConnectionProvider framework.

    - \c connect:// — plaintext HTTP (maps to \c http://)
    - \c connects:// — TLS-encrypted HTTPS (maps to \c https://)

    @par Example
    @code{.py}
ConnectConnection conn("myconn", "connects://example.com:8080",
    {"codec": "yaml"});
auto provider = conn.getDataProvider();
    @endcode
*/
public class ConnectConnection inherits ConnectionProvider::AbstractConnectionWithInfo {
    public {
        #! Connection scheme definition
        const ConnectionScheme = <ConnectionSchemeInfo>{
            "display_name": "Connect RPC",
            "short_desc": "Connect RPC connection",
            "desc": "Connect RPC protocol connection for DataStream 2.0",
            "cls": Class::forName("ConnectDataProvider::ConnectConnection"),
            "schemes": {
                "connect": True,
                "connects": True,
            },
            "base_scheme_map": {
                "connect": "http",
                "connects": "https",
            },
            "options": ConnectionOptions,
        };

        #! Connection options
        const ConnectionOptions = {
            "codec": <ConnectionOptionInfo>{
                "display_name": "Codec",
                "short_desc": "Serialization codec",
                "type": "string",
                "desc": "The codec for message serialization: `yaml` (default), `json`, or `cbor`",
                "default_value": "yaml",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "yaml", "display_name": "YAML",
                        "desc": "YAML codec with full Qore type fidelity"},
                    <AllowedValueInfo>{"value": "json", "display_name": "JSON",
                        "desc": "JSON codec (`json` module required)"},
                    <AllowedValueInfo>{"value": "cbor", "display_name": "CBOR",
                        "desc": "CBOR binary codec (`json` module >= 1.12 required)"},
                ),
            },
            "encoding": <ConnectionOptionInfo>{
                "display_name": "Compression",
                "short_desc": "Compression encoding",
                "type": "string",
                "desc": "Per-message compression encoding: `gzip` or `deflate`",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "gzip", "display_name": "Gzip",
                        "desc": "Standard `gzip` compression (RFC 1952)"},
                    <AllowedValueInfo>{"value": "deflate", "display_name": "Deflate",
                        "desc": "Raw `deflate` compression (RFC 1951)"},
                ),
            },
            "timeout_ms": <ConnectionOptionInfo>{
                "display_name": "Timeout",
                "short_desc": "Default timeout (ms)",
                "type": "int",
                "desc": "Default request timeout in milliseconds",
            },
            "protocol": <ConnectionOptionInfo>{
                "display_name": "Protocol",
                "short_desc": "HTTP protocol version",
                "type": "string",
                "desc": "HTTP protocol: `auto` (default), `h2` (HTTP/2), or `h3` (HTTP/3)",
                "default_value": "auto",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "auto", "display_name": "Auto",
                        "desc": "Auto-detect protocol (tries HTTP/3, falls back to HTTP/2)"},
                    <AllowedValueInfo>{"value": "h2", "display_name": "HTTP/2",
                        "desc": "HTTP/2 over TLS (`h2` ALPN negotiation)"},
                    <AllowedValueInfo>{"value": "h3", "display_name": "HTTP/3",
                        "desc": "HTTP/3 over QUIC (requires QUIC support in the Qore runtime)"},
                ),
            },
            "accept_all_certs": <ConnectionOptionInfo>{
                "display_name": "Accept All Certs",
                "short_desc": "Accept all TLS certificates",
                "type": "bool",
                "desc": "Accept all server TLS certificates without validation (for testing only; "
                    "default: True)",
                "default_value": True,
            },
            "use_discovery": <ConnectionOptionInfo>{
                "display_name": "Use Discovery",
                "short_desc": "Enable service discovery",
                "type": "bool",
                "desc": "Enable automatic service discovery via Connect reflection (default: True)",
                "default_value": True,
            },
        };
    }

    #! Creates the connection
    /** @param name connection name
        @param desc connection description
        @param url connection URL (connect:// or connects://)
        @param attributes connection attributes
        @param options connection options
    */
    constructor(string name, string desc, string url, hash<auto> attributes = {},
            hash<auto> options = {})
            : AbstractConnectionWithInfo(name, desc, url, attributes, options) {
    }

    #! Returns a data provider for this connection
    /** @return a ConnectDataProvider configured for this connection
    */
    DataProvider::AbstractDataProvider getDataProvider() {
        return new ConnectDataProvider(<ConnectDataProviderOptions>{
            "url": getHttpUrl(),
            "codec": opts.codec ?? "yaml",
            "encoding": opts.encoding,
            "timeout_ms": opts.timeout_ms,
            "accept_all_certs": opts.accept_all_certs ?? True,
            "protocol": opts.protocol,
            "use_discovery": opts.use_discovery,
        });
    }

    #! Returns the connection type
    /** @return \c "connect"
    */
    string getType() {
        return "connect";
    }

    #! Returns the underlying connection object
    /** @return a Connect client configured for this connection
    */
    private Connect::ConnectClient getImpl(bool connect = True, *hash<auto> rtopts) {
        return new Connect::ConnectClient(getHttpUrl(), NOTHING,
            <Connect::ConnectClientOptions>{
                "accept_all_certs": opts.accept_all_certs ?? True,
                "protocol": opts.protocol,
            });
    }

    #! Pings the connection by verifying HTTP reachability
    /** Throws an exception if the connection cannot be established; the parent
        class catches this and sets \c ok to \c False in the ping result.
    */
    private pingImpl() {
        HTTPClient client({"url": getHttpUrl(), "timeout": 5s});
        client.connect();
    }

    #! Returns the connection scheme info
    private hash<ConnectionSchemeInfo> getConnectionSchemeInfoImpl() {
        return ConnectionScheme;
    }

    #! Converts connect:// to http:// and connects:// to https://
    string getHttpUrl() {
        string result = url;
        if (result =~ /^connects:/) {
            result = regex_subst(result, "^connects:", "https:");
        } else if (result =~ /^connect:/) {
            result = regex_subst(result, "^connect:", "http:");
        }
        return result;
    }
}

} # namespace ConnectDataProvider
