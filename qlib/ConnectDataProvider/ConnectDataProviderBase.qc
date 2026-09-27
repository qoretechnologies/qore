# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConnectDataProviderBase.qc Connect root data provider

/*  ConnectDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Options for creating a ConnectDataProvider
public hashdecl ConnectDataProviderOptions {
    #! Connect server URL (e.g., \c "https://example.com:8080")
    string url;
    #! Codec name (\c "yaml", \c "json", or \c "cbor"); default: \c "yaml"
    *string codec;
    #! Compression encoding (\c "gzip" or \c "deflate")
    *string encoding;
    #! Default timeout in milliseconds
    *int timeout_ms;
    #! Accept all server certificates (for testing)
    *bool accept_all_certs;
    #! HTTP protocol: \c "auto", \c "h2", or \c "h3"
    *string protocol;
    #! Enable service discovery via reflection (default: True)
    *bool use_discovery;
}

#! Connect RPC root data provider
/** Provides DataProvider integration for Connect RPC services.

    When \c use_discovery is enabled (the default), the provider attempts to
    discover available services via the server's built-in reflection service
    (\c qore.connect.reflection).  If reflection is available, child provider
    names are populated automatically; otherwise the provider silently falls
    back to accepting any service name dynamically.

    @par Example
    @code{.py}
auto provider = new ConnectDataProvider({"url": "https://example.com:8080"});
auto result = provider.getChildProvider("Greeter").getChildProvider("SayHello")
    .doRequest({"name": "World"});
    @endcode
*/
public class ConnectDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "connect",
            "desc": "Connect RPC data provider; children are service names",
            "type": "ConnectDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
            "children_can_support_records": False,
            "children_can_support_observers": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "url": <DataProviderOptionInfo>{
                "display_name": "URL",
                "short_desc": "Connect server URL",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The Connect server URL (e.g., `https://example.com:8080`)",
                "required": True,
                "example_value": "https://localhost:8080",
            },
            "codec": <DataProviderOptionInfo>{
                "display_name": "Codec",
                "short_desc": "Serialization codec",
                "type": AbstractDataProviderTypeMap."string",
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
            "encoding": <DataProviderOptionInfo>{
                "display_name": "Compression",
                "short_desc": "Compression encoding",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Per-message compression encoding for streaming: `gzip` or `deflate`",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "gzip", "display_name": "Gzip",
                        "desc": "Standard `gzip` compression (RFC 1952)"},
                    <AllowedValueInfo>{"value": "deflate", "display_name": "Deflate",
                        "desc": "Raw `deflate` compression (RFC 1951)"},
                ),
            },
            "timeout_ms": <DataProviderOptionInfo>{
                "display_name": "Timeout",
                "short_desc": "Default timeout (ms)",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "Default request timeout in milliseconds",
                "example_value": 30000,
            },
            "accept_all_certs": <DataProviderOptionInfo>{
                "display_name": "Accept All Certs",
                "short_desc": "Accept all TLS certificates",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "Accept all server TLS certificates without validation (for testing)",
                "default_value": True,
            },
            "protocol": <DataProviderOptionInfo>{
                "display_name": "Protocol",
                "short_desc": "HTTP protocol version",
                "type": AbstractDataProviderTypeMap."string",
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
            "use_discovery": <DataProviderOptionInfo>{
                "display_name": "Use Discovery",
                "short_desc": "Enable service discovery via reflection",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "Enable automatic service discovery via the Connect reflection service "
                    "(default: True)",
                "default_value": True,
            },
        };
    }

    private {
        #! Provider options
        hash<ConnectDataProviderOptions> m_opts;

        #! Discovered service map (service name -> service descriptor hash)
        *hash<string, hash<auto>> m_service_map;
    }

    #! Creates the provider from typed options
    /** @param options provider options

        @throw CONNECT-PROVIDER-ERROR if the URL is not provided
    */
    constructor(hash<ConnectDataProviderOptions> options) {
        m_opts = options;
        tryDiscover();
    }

    #! Creates the provider from untyped options (e.g., from factory)
    constructor(*hash<auto> options) {
        if (!options.url) {
            throw "CONNECT-PROVIDER-ERROR", "'url' option is required";
        }
        m_opts = cast<hash<ConnectDataProviderOptions>>(options);
        tryDiscover();
    }

    #! Returns the provider name
    string getName() {
        return m_opts.url;
    }

    #! Returns provider description
    *string getDesc() {
        return sprintf("Connect RPC provider for %s", m_opts.url);
    }

    #! Returns the options
    hash<ConnectDataProviderOptions> getOptions() {
        return m_opts;
    }

    #! Returns a child provider for the given service name
    /** If discovery succeeded and the service is known, passes the service
        descriptor to the child provider for type-aware navigation.
    */
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        if (m_service_map{name}) {
            return new ConnectServiceDataProvider(name, m_opts, m_service_map{name});
        }
        return new ConnectServiceDataProvider(name, m_opts);
    }

    #! Returns known child provider names
    /** Returns discovered service names if reflection succeeded, otherwise empty.
    */
    private *list<string> getChildProviderNamesImpl() {
        if (m_service_map) {
            return keys m_service_map;
        }
        return ();
    }

    #! Returns provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Attempts service discovery via reflection
    /** Silently falls back if reflection is not available.
    */
    private tryDiscover() {
        if (!(m_opts.use_discovery ?? True)) {
            return;
        }
        try {
            Connect::ConnectClient client = createClient();
            on_exit {
                client.shutdown();
            }
            Connect::ConnectReflectionClient refl(client);
            hash<string, hash<auto>> all = refl.discoverAll();
            m_service_map = all;
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "THREAD-CANCELLED" || ex.err == "PROGRAM-INTERRUPTED") {
                rethrow;
            }
            # Silently fall back — server may not support reflection
            m_service_map = NOTHING;
        }
    }

    #! Creates a Connect client from options
    /** @return a new ConnectClient instance
    */
    private Connect::ConnectClient createClient() {
        Connect::AbstractConnectCodec codec = ConnectMethodDataProvider::createCodecStatic(m_opts);
        return new Connect::ConnectClient(m_opts.url, codec,
            <Connect::ConnectClientOptions>{
                "accept_all_certs": m_opts.accept_all_certs ?? True,
                "protocol": m_opts.protocol,
            });
    }
}

} # namespace ConnectDataProvider
