# -*- mode: qore; indent-tabs-mode: nil -*-
# Qore HttpConnection class definition

/*  HttpConnection.qc Copyright 2016 - 2026 Qore Technologies, s.r.o.

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

%modern

#! The ConnectionProvider namespace
/** All public declarations in the %ConnectionProvider module are defined in this namespace
*/
public namespace ConnectionProvider {
#! class for HTTP connections; returns @ref Qore::HTTPClient objects
/** Supports the following options:
    - \c "assume_encoding": assumes the given encoding if the server does not send a charset value
    - \c "connect_timeout": connection timeout to use in milliseconds (default: 45 seconds)
    - \c "error_passthru": if @ref True "True" then HTTP status codes indicating errors will not cause an
      \c HTTP-CLIENT-RECEIVE-ERROR exception to be raised, rather such responses will be passed through to the
      caller like any other response
    - \c "headers": headers to add in each outgoing request
    - \c "http_version": HTTP version to use (\c "auto", \c "1.0", \c "1.1", \c "2.0", or \c "3.0", defaults to
      \c "auto"); HTTP/2 (\c "2.0" or \c "auto") requires TLS and is negotiated via ALPN; HTTP/3 (\c "3.0")
      requires TLS and uses QUIC as the transport protocol
    - \c "max_redirects": maximum redirects to support
    - \c "max_response_body_size": the maximum size in bytes of a response body received into memory, as received
      and after its content encoding is decoded (0, the default, means no limit)
    - \c "negotiate_auth": enables Kerberos/SPNEGO HTTP Negotiate authentication for REST clients that support it
    - \c "negotiate_credential_cache": credential cache to use for Kerberos/SPNEGO HTTP Negotiate authentication
    - \c "negotiate_delegation_mode": delegation mode for Kerberos/SPNEGO HTTP Negotiate authentication
    - \c "negotiate_keytab": client keytab to use for Kerberos/SPNEGO HTTP Negotiate authentication
    - \c "negotiate_preemptive": if \c True, send the initial Negotiate token before receiving a 401 challenge
    - \c "negotiate_service_principal": Kerberos service principal to use; if omitted, REST clients derive a
      host-based \c HTTP service name from the URL
    - \c "password": The password for authentication; only used if no username or password is set in the URL and if
      the \c username option is also used
    - \c "pre_encoded_urls": If @ref True "True" then all URI paths in URLs are assumed to be already
      <a href="https://en.wikipedia.org/wiki/Percent-encoding">percent encoded</a>; if this flag is set and any
      unencoded characters are sent in a URL with a new request, an exception is raised
    - \c "proxy": proxy URL to use
    - \c "redirect_passthru": if @ref True "True" then redirect responses will be passed to the caller instead of
      processed
    - \c "ssl_cert_path": a path to an X.509 client certificate file in PEM format
    - \c "ssl_key_path": a path to a private key file in PEM format for the X.509 client certificate
    - \c "ssl_key_password": the password to the private key given with \c "ssl_key_path"
    - \c "ssl_verify_cert": if @ref True "True" then the server's certificate will only be accepted if it's verified
    - \c "timeout": transfer timeout to use in milliseconds (default: 45 seconds)
    - \c "username": The username for HTTP basic authentication; only used if no username or password is set in the
      URL and if the \c password option is also used

    @see @ref Qore::HTTPClient::constructor(hash<auto>) "HTTPClient::constructor(hash<auto> opts)" for more
    information on the above options

    @note The raw @ref Qore::HTTPClient "HTTPClient" object returned by this connection does not implement the
    SPNEGO retry exchange. If \c negotiate_auth is enabled, raw \c HttpConnection paths such as \c get(),
    \c startPollConnect(), and \c startPollRequest() raise \c HTTP-NEGOTIATE-AUTH-ERROR. Use
    \c RestConnection, \c RestClient, or \c RestClientIo for Kerberos/SPNEGO HTTP Negotiate authentication.
*/
public class HttpConnection inherits AbstractConnectionWithInfo {
    public {
        #! HTTP Negotiate delegation modes
        const list<hash<AllowedValueInfo>> NegotiateDelegationModeAllowedValues = (
            <AllowedValueInfo>{
                "value": "none",
                "desc": "Do not delegate credentials",
            },
            <AllowedValueInfo>{
                "value": "s4u",
                "desc": "Use constrained delegation with S4U where supported",
            },
            <AllowedValueInfo>{
                "value": "direct",
                "desc": "Delegate the initiator credential directly where supported",
            },
        );

        #! HTTP Negotiate option names
        const list<string> NegotiateOptionNames = (
            "negotiate_auth",
            "negotiate_service_principal",
            "negotiate_keytab",
            "negotiate_credential_cache",
            "negotiate_preemptive",
            "negotiate_delegation_mode",
        );

        #! HTTP Negotiate option metadata
        const hash<string, hash<ConnectionOptionInfo>> NegotiateOptions = {
            "negotiate_auth": <ConnectionOptionInfo>{
                "display_name": "Kerberos Negotiate Auth?",
                "short_desc": "Enables Kerberos/SPNEGO HTTP Negotiate authentication",
                "type": "bool",
                "desc": "Enables Kerberos/SPNEGO HTTP Negotiate authentication for REST clients that support it; "
                    "raw HTTPClient paths reject this option with HTTP-NEGOTIATE-AUTH-ERROR",
                "default_value": False,
            },
            "negotiate_service_principal": <ConnectionOptionInfo>{
                "display_name": "Kerberos Service Principal",
                "short_desc": "The Kerberos HTTP service principal",
                "type": "string",
                "desc": "The Kerberos service principal to use for HTTP Negotiate authentication; if omitted, "
                    "REST clients derive a host-based HTTP service name from the URL",
            },
            "negotiate_keytab": <ConnectionOptionInfo>{
                "display_name": "Kerberos Client Keytab",
                "short_desc": "Client keytab for Kerberos HTTP Negotiate authentication",
                "type": "string",
                "desc": "Client keytab to use for Kerberos/SPNEGO HTTP Negotiate authentication",
                "subst_env_vars": True,
            },
            "negotiate_credential_cache": <ConnectionOptionInfo>{
                "display_name": "Kerberos Credential Cache",
                "short_desc": "Credential cache for Kerberos HTTP Negotiate authentication",
                "type": "string",
                "desc": "Credential cache to use for Kerberos/SPNEGO HTTP Negotiate authentication",
                "subst_env_vars": True,
            },
            "negotiate_preemptive": <ConnectionOptionInfo>{
                "display_name": "Preemptive Kerberos Negotiate?",
                "short_desc": "Sends the initial Negotiate token without waiting for a 401 challenge",
                "type": "bool",
                "desc": "If True, send the initial Negotiate token before receiving a 401 challenge",
                "default_value": False,
            },
            "negotiate_delegation_mode": <ConnectionOptionInfo>{
                "display_name": "Kerberos Delegation Mode",
                "short_desc": "Delegation mode for Kerberos HTTP Negotiate authentication",
                "type": "string",
                "desc": "Delegation mode for Kerberos/SPNEGO HTTP Negotiate authentication",
                "default_value": "none",
                "allowed_values": NegotiateDelegationModeAllowedValues,
            },
        };

        #! Connection options understood by the \c FileLocationHandler HTTP location handler
        /** Forwarded by @ref getFileLocationRedirect() so that \c "conn://" locations resolved through this
            connection honor the TLS settings, timeouts, and proxy configured on the connection

            @since ConnectionProvider 3.1
        */
        const HttpFileLocationOptions = (
            "connect_timeout",
            "proxy",
            "ssl_cert_data",
            "ssl_cert_path",
            "ssl_key_data",
            "ssl_key_password",
            "ssl_key_path",
            "ssl_verify_cert",
            "timeout",
        );

        #! Connection entry info
        const hash<ConnectionSchemeInfo> ConnectionScheme = <ConnectionSchemeInfo>{
            "display_name": "HTTP Connection",
            "short_desc": "A connection to a HTTP server",
            "desc": "A connectioas to a HTTP server",
            "cls": Class::forName("HttpConnection"),
            "schemes": {
                "https": True,
                "http": True,
            },
            "options": NegotiateOptions + {
                "assume_encoding": <ConnectionOptionInfo>{
                    "display_name": "Assume Character Encoding",
                    "short_desc": "Sets the character encoding to assume if 'charset' is missing",
                    "type": "string",
                    "desc": "Assumes the given encoding if the server does not send a `charset` value",
                },
                "connect_timeout": <ConnectionOptionInfo>{
                    "display_name": "Connect Timeout",
                    "short_desc": "The connect timeout in milliseconds",
                    "type": "timeout",
                    "desc": "The connection timeout to use in milliseconds (default: 45 seconds)",
                    "default_value": 45000,
                },
                "disable_automatic_pings": <ConnectionOptionInfo>{
                    "display_name": "Disable Automatic Pings?",
                    "short_desc": "Disables automatic pings",
                    "type": "bool",
                    "desc": "Disables automatic pings; useful for rate-limited or metered connections",
                },
                "encode_chars": <ConnectionOptionInfo>{
                    "display_name": "Encode URL Characters",
                    "short_desc": "A set of additional characters to subject to percent encoding in URLs",
                    "type": "string",
                    "desc": "A set of additional characters to subject to percent encoding in URLs",
                },
                "error_passthru": <ConnectionOptionInfo>{
                    "display_name": "Error Passthrough",
                    "short_desc": "If errors should be returned like normal successful responses",
                    "type": "bool",
                    "desc": "If `True` then HTTP status codes indicating errors will not cause an "
                        "`HTTP-CLIENT-RECEIVE-ERROR` exception to be raised, rather such responses will be "
                        "passed through to the caller like any other response",
                    "default_value": False,
                },
                "headers": <ConnectionOptionInfo>{
                    "display_name": "HTTP Headers",
                    "short_desc": "Sets HTTP headers to include in each request",
                    "type": "hash",
                    "desc": "Sets HTTP headers to include in each outgoing request",
                },
                "ping_method": <ConnectionOptionInfo>{
                    "display_name": "Ping Method",
                    "short_desc": "The HTTP method to use for pings",
                    "type": "string",
                    "desc": "The HTTP method to use for an advanced ping; this and `ping_path` must be set to make "
                        "an HTTP request as a part of the socket polling ping operation; must be a valid HTTP method "
                        "name",
                },
                "ping_path": <ConnectionOptionInfo>{
                    "display_name": "Ping Path",
                    "short_desc": "The URI path to use for pings",
                    "type": "string",
                    "desc": "The URI path to use for an advanced ping; this and `ping_method` must be set to make "
                        "an HTTP request as a part of the socket polling ping operation.  If the value for this "
                        "option begins with a `/` character, then it replaces any connection path for the HTTP "
                        "client, otherwise the value is appended to the connection path for the HTTP client.",
                },
                "ping_headers": <ConnectionOptionInfo>{
                    "display_name": "Ping Headers",
                    "short_desc": "Any HTTP headers to send with pings",
                    "type": "hash",
                    "desc": "Any HTTP headers to send when performing an advanced ping operation; ignored if either "
                        "one of `ping_method` and `ping_path` are not set",
                },
                "ping_body": <ConnectionOptionInfo>{
                    "display_name": "Ping Body",
                    "short_desc": "The message body to send with pings",
                    "type": "auto",
                    "desc": "Any message body to send when performing an advanced ping operation; ignored if either "
                        "one of `ping_method` and `ping_path` are not set or if `ping_method` is `GET`",
                },
                "http_version": <ConnectionOptionInfo>{
                    "display_name": "HTTP Version",
                    "short_desc": "The HTTP version to use",
                    "type": "string",
                    "desc": "The HTTP version to use: `auto` (HTTP/3-first happy eyeballs for HTTPS, default), "
                        "`1.0`, `1.1`, `2.0`, or `3.0`. HTTP/2 requires TLS and is negotiated via ALPN. HTTP/3 requires "
                        "TLS and uses QUIC as the transport protocol",
                    "default_value": "auto",
                    "allowed_values": (
                        <AllowedValueInfo>{
                            "value": "auto",
                            "desc": "Use HTTP/3-first happy eyeballs for HTTPS; fall back to HTTP/2 if needed (default)",
                        },
                        <AllowedValueInfo>{
                            "value": "1.0",
                            "desc": "HTTP/1.0 only",
                        },
                        <AllowedValueInfo>{
                            "value": "1.1",
                            "desc": "HTTP/1.1 only",
                        },
                        <AllowedValueInfo>{
                            "value": "2.0",
                            "desc": "Require HTTP/2, fail if the server doesn't support it (TLS required)",
                        },
                        <AllowedValueInfo>{
                            "value": "3.0",
                            "desc": "Require HTTP/3 (QUIC), fail if unavailable (TLS required)",
                        },
                    ),
                },
                "max_redirects": <ConnectionOptionInfo>{
                    "display_name": "Max Redirects",
                    "short_desc": "Sets the maximum HTTP redirects to support",
                    "type": "int",
                    "desc": "Sets the maximum HTTP redirects to support in a single request",
                },
                "max_response_body_size": <ConnectionOptionInfo>{
                    "display_name": "Max Response Body Size",
                    "short_desc": "The maximum size of a response body received into memory",
                    "type": "int",
                    "desc": "The maximum size in bytes of a response body that is received into memory, as "
                        "received and after its content encoding (such as `gzip`) is decoded; `0` (the default) "
                        "means no limit.\n\n"
                        "A larger response fails the request, so a huge response, or a small compressed response "
                        "that expands to a huge size, cannot exhaust memory.",
                },
                "password": <ConnectionOptionInfo>{
                    "display_name": "Password",
                    "short_desc": "The password for HTTP basic authentication",
                    "type": "string",
                    "desc": "The password for HTTP basic authentication; only used if no username or password is set "
                        "in the URL and if the `username` option is also used",
                    "sensitive": True,
                },
                "pre_encoded_urls": <ConnectionOptionInfo>{
                    "display_name": "Pre-Encoded URLs",
                    "short_desc": "If true then URLs are assumed to be preencoded",
                    "type": "bool",
                    "desc": "If `true` then all URI paths in URLs are assumed to be already "
                        "[percent encoded](https://en.wikipedia.org/wiki/Percent-encoding); if this flag is set and "
                        "any unencoded characters are sent in a URL with a new request, an exception is raised",
                },
                "proxy": <ConnectionOptionInfo>{
                    "display_name": "HTTP Proxy",
                    "short_desc": "The HTTP proxy to use",
                    "type": "string",
                    "desc": "The HTTP proxy URL to use",
                },
                "redirect_passthru": <ConnectionOptionInfo>{
                    "display_name": "Redirect Passthrough",
                    "short_desc": "If true then redirect responses are returned to the caller",
                    "type": "bool",
                    "desc": "If `True` then redirect responses will be passed to the caller instead of processed",
                    "default_value": False,
                },
                "ssl_cert_data": <ConnectionOptionInfo>{
                    "display_name": "SSL Certificate Data",
                    "short_desc": "X.509 certificate data",
                    "type": "data",
                    "desc": "The X.509 certificate data in PEM format (string) or in DER format (binary); if this "
                        "option is set, then `ssl_cert_path` is ignored",
                },
                "ssl_cert_path": <ConnectionOptionInfo>{
                    "display_name": "SSL Certificate Location",
                    "short_desc": "The location of the X.509 client certificate to use",
                    "type": "file-as-string",
                    "desc": "The location of a X.509 client certificate file in PEM format; "
                        "processed with `FileLocationHandler`, so it may be a bare path or "
                        "any registered scheme (`file://`, `http(s)://`, `sftp://`, ...)",
                    "subst_env_vars": True,
                    "freeform": True,
                },
                "ssl_key_password": <ConnectionOptionInfo>{
                    "display_name": "SSL Key Password",
                    "short_desc": "The password to the private key",
                    "type": "string",
                    "desc": "The password to the private key given with `ssl_key_path`",
                    "sensitive": True,
                },
                "ssl_key_data": <ConnectionOptionInfo>{
                    "display_name": "SSL Key Data",
                    "short_desc": "X.509 private key data",
                    "type": "data",
                    "desc": "The X.509 private key data in PEM format (string) or in DER format (binary); if this "
                        "option is set, then `ssl_key_path` is ignored",
                    "sensitive": True,
                },
                "ssl_key_path": <ConnectionOptionInfo>{
                    "display_name": "SSL Key Location",
                    "short_desc": "The location of the private key file",
                    "type": "file-as-string",
                    "desc": "The location of a private key file in PEM format for the X.509 "
                        "client certificate; processed with `FileLocationHandler`, so it may "
                        "be a bare path or any registered scheme (`file://`, `http(s)://`, "
                        "`sftp://`, ...)",
                    "subst_env_vars": True,
                    "freeform": True,
                },
                "ssl_verify_cert": <ConnectionOptionInfo>{
                    "display_name": "SSL Verify Certificate",
                    "short_desc": "If true then server certificates will be verified",
                    "type": "bool",
                    "desc": "if `True` then the server's certificate will only be accepted if it's verified",
                    "default_value": False,
                },
                "timeout": <ConnectionOptionInfo>{
                    "display_name": "I/O Timeout",
                    "short_desc": "The maximum time an individual I/O operation can take in milliseconds",
                    "type": "timeout",
                    "desc": "The transfer timeout to use in milliseconds (default: 45 seconds)",
                    "default_value": 45000,
                },
                "username": <ConnectionOptionInfo>{
                    "display_name": "Username",
                    "short_desc": "Sets the username for HTTP basic authentication",
                    "type": "string",
                    "desc": "The username for authentication; only used if no username or password is set in the URL "
                        "and if the `password` option is also used",
                },
            },
        };

        #! HttpConnection object connection options
        const hash<string, bool> Options = map {$1: True}, keys ConnectionScheme.options;

        #! Connection options passed as request options to the data provider returned by getDataProvider()
        /** @since ConnectionProvider 3.1
        */
        const list<string> DataProviderRequestOptions = (
            "assume_encoding",
            "error_passthru",
            "headers",
            "pre_encoded_urls",
        );

        #! Maps HTTPClient HTTP version options to HttpClientIo protocol modes
        /** HttpClientIo implements HTTP/1.1 for both legacy HTTP/1.x selections.

            @since ConnectionProvider 3.1
        */
        const hash<string, string> AsyncHttpVersionMap = {
            "auto": "auto",
            "1.0": "h1",
            "1.1": "h1",
            "2.0": "h2",
            "3.0": "h3",
        };
    }

    #! Creates the HttpConnection object
    /** @param name the name of the connection
        @param desc connection description
        @param url connection URL (potentially with password info)
        @param attributes various attributes. See below
        @param options connection options
        @param logger optional logger interface for the connection

        See @ref ConnectionProvider::AbstractConnection::constructor() for \c attributes and \c options reference.

        @throw CONNECTION-OPTION-ERROR missing or invalid connection option
    */
    constructor(string name, string desc, string url, hash<auto> attributes = {}, hash<auto> options = {},
            *Logger logger) : AbstractConnectionWithInfo(name, desc, url, attributes, options, logger) {
    }

    #! Creates the HttpConnection object
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
    constructor(hash<auto> config, *hash<auto> attr) : AbstractConnectionWithInfo(config, attr) {
    }

    #! Returns \c "http"
    string getType() {
        return "http";
    }

    #! Returns an HttpClientDataProvider object for this connection
    /** The data provider makes its requests with the connection manager returned by \c getAsync() and this
        connection's URL; the \c headers, \c error_passthru, \c pre_encoded_urls, and \c assume_encoding options and
        the \c username and \c password options are used as its request options.

        @param constructor_options request options for the data provider, overriding the connection's options;
        the headers given are merged with the connection's headers; see the
        <a href="../../HttpClientDataProvider/html/index.html">HttpClientDataProvider</a> module for the supported
        options

        @return an \c HttpClientDataProvider::HttpClientDataProvider object for this connection

        @throw CONSTRUCTOR-ERROR an unsupported option was given in \a constructor_options
        @throw HTTP-NEGOTIATE-AUTH-ERROR the \c negotiate_auth option is set; the data provider does not support
        Kerberos/SPNEGO authentication

        @note Before %ConnectionProvider 3.1, this method loaded a nonexistent \c HttpDataProvider module and always
        failed

        @see hasDataProvider()
    */
    DataProvider::AbstractDataProvider getDataProvider(*hash<auto> constructor_options) {
        # to avoid circular dependencies, this object loads the HttpClientDataProvider module and creates the data
        # provider object dynamically
        load_module("HttpClientDataProvider");
        hash<auto> req_opts = (map {$1: opts{$1}}, DataProviderRequestOptions, exists opts{$1}) ?? {};
        if (opts.username.val() && opts.password.val() && (!urlh.username.val() || !urlh.password.val())) {
            req_opts.username = opts.username;
            req_opts.password = opts.password;
        }
        if (constructor_options.headers) {
            req_opts.headers = (req_opts.headers ?? {}) + constructor_options.headers;
        }
        req_opts += constructor_options - "headers";
        return create_object("HttpClientDataProvider::HttpClientDataProvider", getAsync(), url, req_opts);
    }

    #! Returns @ref True, as the connection returns a data provider with the \c getDataProvider() method
    /** @return @ref True, as the connection returns a data provider with the \c getDataProvider() method

        @see @ref getDataProvider()
    */
    bool hasDataProvider() {
        return True;
    }

    #! returns @ref True, since this connection type supports the %Qore @ref Qore::Socket "Socket-based" polling API
    /** @return @ref True

        @since ConnectionProvider 1.7
    */
    bool supportsPollingApi() {
        return True;
    }

    #! Called to start the connection if the connection supports the polling API
    /** If the \c ping_method and \c ping_path options are set, an @ref HttpPingPollOperation is returned that sends
        a ping request with non-blocking I/O using \c HttpClientIo (supporting HTTP/1.1, HTTP/2, and HTTP/3),
        otherwise the operation returned by @ref Qore::HTTPClient::startPollConnect() "HTTPClient::startPollConnect()"
        is returned, which establishes a connection to the server

        @param logger an optional logger for the poll operation

        @return a socket poll operation object that will allow the connection goal to be reached with polling

        @throw HTTP-NEGOTIATE-AUTH-ERROR the \c negotiate_auth option is set

        @see supportsPollingApi()

        @since ConnectionProvider 1.7
    */
    Qore::AbstractPollOperation startPollConnect(*Logger::LoggerInterface logger) {
        checkRawHttpNegotiateUnsupported();
        if (opts.ping_method && opts.ping_path) {
            hash<auto> headers;
            headers += opts.headers;
            headers += opts.ping_headers;
            return new HttpPingPollOperation(url, opts.ping_method, opts.ping_path,
                opts{keys ConnectionScheme.options} + {"url": url}, headers,
                (opts.ping_method == "GET" ? NOTHING : opts.ping_body), logger, "HTTP-PING-ERROR",
                "HttpConnection");
        }
        return getPollImpl().startPollConnect();
    }

    #! Returns @ref True since HTTP connections support the async request API
    /** @return @ref True

        @since ConnectionProvider 3.0
    */
    bool supportsAsyncRequestApi() {
        return True;
    }

    #! Starts a non-blocking HTTP request on the connection
    /** @param method the HTTP method (GET, POST, PUT, DELETE, PATCH, etc.)
        @param path the request path
        @param body the request body
        @param headers optional request headers
        @param logger an optional logger interface

        @return a poll operation for the async request

        @since ConnectionProvider 3.0
    */
    Qore::AbstractPollOperation startPollRequest(string method, string path, auto body,
            *hash<auto> headers, *Logger::LoggerInterface logger) {
        checkRawHttpNegotiateUnsupported();
        hash<auto> req_headers;
        req_headers += opts.headers;
        req_headers += headers;
        return new HttpRequestPollOperation(url, method, path, body,
            opts{keys ConnectionScheme.options} + {"url": url}, req_headers, logger);
    }

    #! Returns options for creating a new connection
    *hash<auto> getConnectionOptions(*hash<auto> rtopts) {
        return {"url": url} + opts;
    }

    #! Sets child data provider capabilities
    private setChildCapabilities() {
        children_can_support_apis = True;
    }

    #! returns an @ref Qore::HTTPClient object
    /** @param connect if @ref True "True" then a connection is made by calling @ref Qore::HTTPClient::connect() before returning the object
        @param rtopts this connection type does not accept any runtime option, so this parameter is ignored
    */
    private Qore::HTTPClient getImpl(bool connect = True, *hash<auto> rtopts) {
        checkRawHttpNegotiateUnsupported();
        HTTPClient sc(getConnectionOptions());
        if (connect) {
            sc.connect();
        }

        return sc;
    }

    #! Returns an async HTTP connection manager
    /** The manager uses the process-wide async I/O controller and establishes and pools connections lazily.
        Therefore \a connect is accepted for API compatibility but has no effect. Pass this connection's
        @ref url to the manager's request methods; the manager uses URLs per request so that one manager can pool
        connections for multiple endpoints.

        HTTPClient options that have HttpClientIo equivalents are preserved, including protocol selection,
        connect and request timeouts, proxy and redirect handling, TLS verification, and mutual TLS credentials.

        @param connect ignored; connections are established on demand
        @param rtopts this connection type does not accept runtime options

        @return a @ref HttpClientIo::HttpClientConnectionManager "HttpClientConnectionManager"

        @since ConnectionProvider 3.1
    */
    private HttpClientIo::HttpClientConnectionManager getAsyncImpl(bool connect = True, *hash<auto> rtopts) {
        checkRawHttpNegotiateUnsupported();
        return HttpConnection::getAsyncConnectionManager(opts, logger);
    }

    #! Returns an async HTTP connection manager for the given HTTP connection options
    /** HTTPClient options that have HttpClientIo equivalents are used, including protocol selection
        (\c http_version), connect and request timeouts (\c connect_timeout and \c timeout), proxy and redirect
        handling (\c proxy, \c max_redirects, and \c redirect_passthru), TLS verification (\c ssl_verify_cert),
        mutual TLS credentials (\c ssl_cert_data, \c ssl_cert_path, \c ssl_key_data, \c ssl_key_path, and
        \c ssl_key_password), and \c max_response_body_size; other options are ignored.

        @param opts @ref HttpConnection options
        @param logger an optional logger for the connection manager

        @return a new @ref HttpClientIo::HttpClientConnectionManager "HttpClientConnectionManager"; connections are
        established on demand

        @throw HTTPCLIENT-OPTION-ERROR an invalid \c http_version value was given

        @since ConnectionProvider 3.1
    */
    static HttpClientIo::HttpClientConnectionManager getAsyncConnectionManager(*hash<auto> opts,
            *Logger::LoggerInterface logger) {
        *string protocol = AsyncHttpVersionMap{opts.http_version ?? "auto"};
        if (!protocol) {
            throw "HTTPCLIENT-OPTION-ERROR", sprintf("invalid http_version %y; expecting one of: %y",
                opts.http_version, keys AsyncHttpVersionMap);
        }
        hash<HttpClientIo::HttpClientConnectionManagerOptions> async_opts =
            <HttpClientIo::HttpClientConnectionManagerOptions>{
                "protocol": protocol,
                # HTTPClient's ssl_verify_cert defaults to False, whereas HttpClientIo verifies by default
                "ssl_verify_mode": opts.ssl_verify_cert ? SSL_VERIFY_PEER : SSL_VERIFY_NONE,
                "redirect_passthru": opts.redirect_passthru.toBool(),
                "logger": logger,
            };
        # timeout options may be given as relative dates or as integer numbers of milliseconds
        if (opts.connect_timeout) {
            timeout connect_timeout = opts.connect_timeout;
            async_opts.connect_timeout = connect_timeout;
        }
        if (opts.timeout) {
            timeout request_timeout = opts.timeout;
            async_opts.request_timeout = request_timeout;
        }
        if (exists opts.max_redirects) {
            async_opts.max_redirects = opts.max_redirects;
        }
        if (exists opts.max_response_body_size) {
            async_opts.max_response_body_size = opts.max_response_body_size;
        }
        if (opts.proxy.val()) {
            async_opts.proxy_url = opts.proxy;
        }

        *SSLCertificate cert = HttpConnection::getAsyncSslCertificate(opts);
        if (cert) {
            async_opts.client_cert = cert;
        }
        *SSLPrivateKey key = HttpConnection::getAsyncSslPrivateKey(opts);
        if (key) {
            async_opts.client_pk = key;
        }

        return new HttpClientIo::HttpClientConnectionManager(async_opts);
    }

    #! Loads the configured client certificate for HttpClientIo
    private static *SSLCertificate getAsyncSslCertificate(*hash<auto> opts) {
        *data cert_data = opts.ssl_cert_data;
        if (!exists cert_data && opts.ssl_cert_path.val()) {
            cert_data = HttpConnection::getAsyncSslFileData(opts.ssl_cert_path);
        }
        return exists cert_data ? new SSLCertificate(cert_data) : NOTHING;
    }

    #! Loads the configured client private key for HttpClientIo
    private static *SSLPrivateKey getAsyncSslPrivateKey(*hash<auto> opts) {
        *data key_data = opts.ssl_key_data;
        if (!exists key_data && opts.ssl_key_path.val()) {
            key_data = HttpConnection::getAsyncSslFileData(opts.ssl_key_path);
        }
        return exists key_data ? new SSLPrivateKey(key_data, opts.ssl_key_password) : NOTHING;
    }

    #! Loads string data through FileLocationHandler without introducing a static module dependency
    private static string getAsyncSslFileData(string location) {
        load_module("FileLocationHandler");
        Class c = Class::forName("FileLocationHandler");
        StaticMethod m = c.getStaticMethod("getTextFileFromLocation");
        return m.call(location);
    }

    #! Rejects Kerberos/SPNEGO authentication for raw HTTPClient paths
    private checkRawHttpNegotiateUnsupported() {
        if (opts.negotiate_auth) {
            throw "HTTP-NEGOTIATE-AUTH-ERROR",
                "HttpConnection returns raw HTTPClient objects, which do not support negotiate_auth; use "
                "RestConnection for Kerberos/SPNEGO HTTP Negotiate authentication";
        }
    }

    #! Returns an \c "http" or \c "https" scheme redirect for the given path
    /** @param path the path relative to any path in the connection URL
        @param write @ref True if the location will be written to

        @return a @ref FileLocationRedirect hash for the connection's HTTP scheme

        Connection options that the \c FileLocationHandler HTTP handler also understands are forwarded in the
        redirect so that TLS settings, timeouts, and any proxy configured on the connection are honored.

        @note
        - Kerberos/SPNEGO connections are not redirected; \c negotiate_auth cannot be expressed in a location string
          and raw \c HTTPClient paths reject it in any case.  Such connections fall through to the next resolution
          tier
        - The returned location may carry credentials; it must never appear in error messages or log output

        @since ConnectionProvider 3.1
    */
    private *hash<FileLocationRedirect> getFileLocationRedirectImpl(string path, bool write) {
        # negotiate_auth cannot be expressed in a location string; let a later tier handle these connections
        if (opts.negotiate_auth) {
            return;
        }

        string file_path = AbstractConnection::getConfinedFileLocationPath(name, urlh.path ?? "/", path);

        string authority = urlh.username.val() && urlh.password.val()
            ? sprintf("%s:%s@%s", urlh.username, urlh.password, urlh.host)
            : urlh.host;
        if (urlh.port) {
            authority += sprintf(":%d", urlh.port);
        }

        # forward only options that are actually set; a hash slice would include unset keys with no value, which
        # the target handler would then treat as explicitly-supplied values
        return <FileLocationRedirect>{
            "scheme": urlh.protocol.val() ? urlh.protocol : "http",
            "location": authority + file_path,
            "opts": map {$1: opts{$1}}, HttpFileLocationOptions, exists opts{$1},
        };
    }

    #! Returns connection features
    private *hash<string, bool> getFeaturesImpl() {
        return {
            AbstractConnection::CF_FILE_LOCATIONS: True,
        };
    }

    #! Returns the ConnectionSchemeInfo hash for this object
    private hash<ConnectionSchemeInfo> getConnectionSchemeInfoImpl() {
        return ConnectionScheme;
    }
}
} # ConnectionProvider namespace
