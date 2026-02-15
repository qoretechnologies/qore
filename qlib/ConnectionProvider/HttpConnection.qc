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
    - \c "http_version": HTTP version to use (\c "auto", \c "1.0", \c "1.1", or \c "2.0", defaults to \c "auto");
      HTTP/2 (\c "2.0" or \c "auto") requires TLS and is negotiated via ALPN
    - \c "max_redirects": maximum redirects to support
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

    @note <b>HTTP/2 Limitations:</b> WebSocket over HTTP/2 (RFC 8441) and Server-Sent Events (SSE) over HTTP/2
    are not currently supported. WebSocket and SSE connections will use HTTP/1.x even when \c http_version is
    set to \c "auto" or \c "2.0".

    @see @ref Qore::HTTPClient::constructor(hash<auto>) "HTTPClient::constructor(hash<auto> opts)" for more
    information on the above options
*/
public class HttpConnection inherits AbstractConnectionWithInfo {
    public {
        #! Connection entry info
        const ConnectionScheme = <ConnectionSchemeInfo>{
            "display_name": "HTTP Connection",
            "short_desc": "A connection to a HTTP server",
            "desc": "A connectioas to a HTTP server",
            "cls": Class::forName("HttpConnection"),
            "schemes": {
                "https": True,
                "http": True,
            },
            "options": {
                "assume_encoding": <ConnectionOptionInfo>{
                    "display_name": "Assume Character Encoding",
                    "short_desc": "Sets the character encoding to assume if 'charset' is missing",
                    "type": "string",
                    "desc": "Assumes the given encoding if the server does not send a `charset` value",
                },
                "connect_timeout": <ConnectionOptionInfo>{
                    "display_name": "Connect Timeout",
                    "short_desc": "The connect timeout in milliseconds",
                    "type": "int",
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
                    "desc": "The HTTP version to use: `auto` (HTTP/2 if available, default), `1.0`, `1.1`, "
                        "or `2.0`. HTTP/2 requires TLS and is negotiated via ALPN. Note: WebSocket and SSE "
                        "connections will always use HTTP/1.x regardless of this setting",
                    "default_value": "auto",
                    "allowed_values": (
                        <AllowedValueInfo>{
                            "value": "auto",
                            "desc": "Use HTTP/2 if available via ALPN, fall back to HTTP/1.1 (default)",
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
                    ),
                },
                "max_redirects": <ConnectionOptionInfo>{
                    "display_name": "Max Redirects",
                    "short_desc": "Sets the maximum HTTP redirects to support",
                    "type": "int",
                    "desc": "Sets the maximum HTTP redirects to support in a single request",
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
                    "type": "string",
                    "desc": "The location of a X.509 client certificate file in PEM format",
                    "subst_env_vars": True,
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
                },
                "ssl_key_path": <ConnectionOptionInfo>{
                    "display_name": "SSL Key Location",
                    "short_desc": "The location of the private key file",
                    "type": "string",
                    "desc": "The location to a private key file in PEM format for the X.509 client certificate",
                    "subst_env_vars": True,
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
                    "type": "int",
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
        const Options = map {$1: True}, keys ConnectionScheme.options;
    }

    #! Creates the HttpConnection object
    /** @param name the name of the connection
        @param desc connection description
        @param url connection URL (potentially with password info)
        @param attributes various attributes. See below
        @param options connection options
        @param logger optional logger interface for the connection

        See \c AbstractConnection::constructor() for \c attributes and \c options reference.

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

    #! Returns an HttpDataProvider object for this connection
    /** @param constructor_options any additional constructor options for the data provider

        @return an HttpDataProvider object for this connection

        @see hasDataProvider()
    */
    DataProvider::AbstractDataProvider getDataProvider(*hash<auto> constructor_options) {
        # to avoid circular dependencies, this object loads the HttpDataProvider and creates the data provider
        # object dynamically
        load_module("HttpDataProvider");
        return create_object("HttpDataProvider", get());
    }

    #! Returns @ref True, as the connection returns a data provider with the \c getDataProvider() method
    /** @return @ref True, as the connection returns a data provider with the \c getDataProvider() method

        @see \c getDataProvider()
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
    /**
        @return a socket poll operation object that will allow the connection goal to be reached with polling

        @see supportsPollingApi()

        @since ConnectionProvider 1.7
    */
    Qore::AbstractPollOperation startPollConnect(*Logger::LoggerInterface logger) {
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
        HTTPClient sc(getConnectionOptions());
        if (connect) {
            sc.connect();
        }

        return sc;
    }

    #! Returns the ConnectionSchemeInfo hash for this object
    private hash<ConnectionSchemeInfo> getConnectionSchemeInfoImpl() {
        return ConnectionScheme;
    }
}
} # ConnectionProvider namespace
