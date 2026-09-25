# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore HttpClientDataProviderBase class definition

/** HttpClientDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the HttpClientDataProvider module
public namespace HttpClientDataProvider {
#! Base class for the HTTP client data providers; requests are made with HttpClientIo
/** Requests are made with a
    <a href="../../HttpClientIo/html/index.html">HttpClientIo::HttpClientConnectionManager</a>; the data provider
    keeps the target URL and the request options that the connection manager does not handle itself (default
    headers, basic authentication, error handling, and response body handling).

    @see @ref httpclientdataprovider_client

    @since HttpClientDataProvider 1.2
*/
public class HttpClientDataProviderBase inherits AbstractDataProvider {
    public {
        #! The encoding assumed for text responses without a \c charset if the \c assume_encoding option is not set
        /** ISO-8859-1, the default character set of text media types in HTTP/1.1 (RFC 2616), as assumed by
            @ref Qore::HTTPClient "HTTPClient"
        */
        const DefaultAssumedEncoding = "ISO-8859-1";

        #! The synchronous HTTP client that the data provider was created with, if any
        /** @deprecated not used for requests, which are made with the connection manager returned by
            @ref getConnectionManager(); only set if the data provider was created with an
            @ref Qore::HTTPClient "HTTPClient" object
        */
        *HTTPClient http;

        #! Maps \c http_version option values to HttpClientIo protocol values
        const HttpVersionProtocolMap = {
            "auto": "auto",
            "1.0": "h1",
            "1.1": "h1",
            "2.0": "h2",
            "3.0": "h3",
        };

        #! Request options accepted by the constructor taking a connection manager and a URL
        const RequestOptions = {
            "assume_encoding": True,
            "error_passthru": True,
            "headers": True,
            "password": True,
            "pre_encoded_urls": True,
            "timeout": True,
            "username": True,
        };
    }

    private {
        #! The connection manager used for requests
        HttpClientIo::HttpClientConnectionManager mgr;

        #! The target URL without user information
        string url;

        #! The origin (scheme, host, and port) of the target URL, passed to the connection manager
        string origin;

        #! The path of the target URL, used for requests without a path
        string url_path;

        #! Default request headers, including any \c Authorization header for basic authentication
        hash<auto> default_headers = {};

        #! If error responses are returned instead of raising an exception
        bool error_passthru;

        #! If request paths are already percent-encoded
        bool pre_encoded_urls;

        #! The encoding to assume for text responses without a \c charset
        /** ISO-8859-1 by default, the default character set of text media types in HTTP/1.1 (RFC 2616), as assumed by
            @ref Qore::HTTPClient "HTTPClient"
        */
        string assume_encoding = DefaultAssumedEncoding;

        #! The request timeout, if not the connection manager's default
        *int request_timeout;

        #! Characters percent-encoded in request paths, like @ref Qore::HTTPClient "HTTPClient" encodes them
        const EncodeChars = {
            " ": True,
            "<": True,
            ">": True,
            "\"": True,
            "#": True,
            "%": True,
            "{": True,
            "}": True,
            "|": True,
            "\\": True,
            "^": True,
            "~": True,
            "[": True,
            "]": True,
            "`": True,
        };

        #! Characters that must not appear in a request path when the \c pre_encoded_urls option is set
        const PreEncodedInvalidChars = {
            "{": True,
            "}": True,
            "|": True,
            "\\": True,
            "^": True,
            "[": True,
            "]": True,
            "`": True,
        };
    }

    #! Creates the object from constructor options
    /** @param options the constructor options; see @ref HttpClientDataProvider::ConstructorOptions

        @throw CONSTRUCTOR-ERROR invalid or missing constructor option
    */
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", HttpClientDataProvider::ConstructorOptions, options);
        setup(HttpClientDataProviderBase::getHttpConnectionManager(copts), copts.url, copts);
    }

    #! Creates the object from a synchronous HTTP client
    /** Requests are made with a new
        <a href="../../HttpClientIo/html/index.html">HttpClientIo::HttpClientConnectionManager</a> created from
        the configuration of \a http; see @ref httpclientdataprovider_client for the settings that are carried over.
        The client itself is not used for requests, and it is not changed.

        @param http the synchronous HTTP client whose configuration is used
    */
    constructor(HTTPClient http) {
        self.http = http;
        hash<auto> opts = HttpClientDataProviderBase::getOptionsFromClient(http);
        setup(HttpClientDataProviderBase::getHttpConnectionManager(opts), opts.url, opts);
    }

    #! Creates the object from an HttpClientIo connection manager
    /** @param mgr the connection manager for requests; it can be shared with other objects, and it is never closed
        by the data provider; its own options (timeouts, TLS, proxy, protocol, redirects, response body limit)
        apply to all requests
        @param url the target URL; user information in the URL is sent with HTTP basic authentication, and the path
        is used for requests without a path
        @param options request options, as follows:
        - \c assume_encoding: not used for decoding responses, which are decoded by the connection manager with its
          own \c assume_encoding option; kept in the request options of the data provider
        - \c error_passthru: if @ref True, error responses are returned instead of raising
          \c HTTP-CLIENT-RECEIVE-ERROR
        - \c headers: default headers for each request
        - \c password: the password for HTTP basic authentication; only used with \c username
        - \c pre_encoded_urls: if @ref True, request paths must already be percent-encoded
        - \c timeout: the request timeout in milliseconds, overriding the connection manager's request timeout
        - \c username: the username for HTTP basic authentication; overrides any user information in \a url

        @throw CONSTRUCTOR-ERROR the URL has no host, or an unsupported option was given
    */
    constructor(HttpClientIo::HttpClientConnectionManager mgr, string url, *hash<auto> options) {
        if (*hash<auto> unknown = options - (keys RequestOptions)) {
            throw "CONSTRUCTOR-ERROR", sprintf("unsupported request option(s) %y; supported options: %y",
                keys unknown, keys RequestOptions);
        }
        setup(mgr, url, options);
    }

    #! Returns the connection manager used for requests
    /** @return the connection manager used for requests
    */
    HttpClientIo::HttpClientConnectionManager getConnectionManager() {
        return mgr;
    }

    #! Returns the target URL without user information
    /** @return the target URL without user information
    */
    string getUrl() {
        return url;
    }

    #! Returns the request options passed to a child data provider
    /** @return the request options passed to a child data provider, including any default headers and the basic
        authentication header
    */
    hash<auto> getHttpRequestOptions() {
        hash<auto> rv();
        rv.error_passthru = error_passthru;
        rv.pre_encoded_urls = pre_encoded_urls;
        if (default_headers) {
            rv.headers = default_headers;
        }
        if (assume_encoding) {
            rv.assume_encoding = assume_encoding;
        }
        if (exists request_timeout) {
            rv.timeout = request_timeout;
        }
        return rv;
    }

    #! Makes an HTTP request and returns the response
    /** @param meth the HTTP method
        @param body the message body to send, if any
        @param path the request path; if not set, the path of the target URL (or \c "/") is used
        @param hdr headers to send in addition to the default headers

        @return the response hash; see @ref HttpClientCallResponseDataType

        @throw HTTP-CLIENT-RECEIVE-ERROR an error status code was received and the \c error_passthru option is not
        set; the exception argument has the lower-case response headers and the \c status_code,
        \c status_message, and \c body keys
        @throw HTTP-CLIENT-MAXIMUM-REDIRECTS-EXCEEDED the maximum number of redirects was exceeded
        @throw HTTP-CLIENT-REDIRECT-ERROR a redirect response cannot be followed
        @throw SOCKET-TIMEOUT the request timed out
        @throw URL-ENCODING-ERROR the \c pre_encoded_urls option is set and the path has a character that must be
        percent-encoded

        @note Other errors are raised by the connection manager, for example \c HTTPCLIENT-CONNECT-ERROR if the
        server cannot be reached
    */
    hash<auto> doHttpRequest(string meth, *data body, *string path, *hash<auto> hdr) {
        meth = meth.upr();
        string req_path = getRequestPath(path);

        # merge headers case-insensitively; headers given for the request override the default headers
        hash<auto> req_hdr = default_headers;
        foreach hash<auto> i in (hdr.pairIterator()) {
            req_hdr -= (map $1, keys req_hdr, $1.lwr() == i.key.lwr());
            req_hdr{i.key} = i.value;
        }
        if (exists body && !(map 1, keys req_hdr, $1.lwr() == "content-type")) {
            req_hdr."Content-Type" = body.typeCode() == NT_STRING
                ? sprintf("text/plain;charset=%s", body.encoding())
                : "application/octet-stream";
        }

        hash<auto> resp;
        try {
            resp = mgr.request(origin, meth, req_path, req_hdr, body, request_timeout);
        } catch (hash<ExceptionInfo> ex) {
            HttpClientDataProviderBase::rethrowRequestError(ex);
        }

        string version = resp.http_version ?? (resp.protocol == "h2" ? "2" : (resp.protocol == "h3" ? "3" : "1.1"));
        *string msg = resp.status_message;
        # the connection manager decodes the body by the rules shared by all HTTP clients, with the assumed encoding
        # of the data provider (see getHttpConnectionManager())
        auto resp_body = resp.body;

        if (!error_passthru && (resp.status_code < 100 || resp.status_code >= 300)) {
            hash<auto> arg = resp.headers + {
                "status_code": resp.status_code,
                "status_message": msg,
                "body": resp_body,
            };
            throw "HTTP-CLIENT-RECEIVE-ERROR", sprintf("HTTP status code %d received: message: %s",
                resp.status_code, msg ?? "<no message>"), arg;
        }

        return {
            "request-headers": req_hdr,
            "request-uri": sprintf("%s %s HTTP/%s", meth, req_path, version),
            "response-body": resp_body,
            "response-headers": resp.headers,
            "response-headers-raw": resp.headers_raw ?? resp.headers,
            "response-uri": sprintf("HTTP/%s %d%s", version, resp.status_code, msg ? " " + msg : ""),
            "status-code": resp.status_code,
            "status-message": msg,
        };
    }

    #! Returns a connection manager created from data provider constructor options
    /** @param options data provider constructor options; see @ref HttpClientDataProvider::ConstructorOptions;
        the transport options are used: \c connect_timeout, \c timeout, \c http_version, \c max_redirects,
        \c redirect_passthru, \c proxy, \c ssl_verify_cert, and the client certificate and key options

        @return a new connection manager

        @throw CONSTRUCTOR-ERROR invalid \c http_version value
    */
    static HttpClientIo::HttpClientConnectionManager getHttpConnectionManager(hash<auto> options) {
        *string protocol = HttpVersionProtocolMap{options.http_version ?? "auto"};
        if (!protocol) {
            throw "CONSTRUCTOR-ERROR", sprintf("invalid http_version %y; expecting one of: %y", options.http_version,
                keys HttpVersionProtocolMap);
        }
        hash<HttpClientIo::HttpClientConnectionManagerOptions> mgr_opts({
            "protocol": protocol,
            # HTTPClient does not verify server certificates by default, whereas HttpClientIo does
            "ssl_verify_mode": options.ssl_verify_cert ? SSL_VERIFY_PEER : SSL_VERIFY_NONE,
            "redirect_passthru": options.redirect_passthru.toBool(),
        });
        if (options.connect_timeout) {
            mgr_opts.connect_timeout = options.connect_timeout;
        }
        if (options.timeout) {
            mgr_opts.request_timeout = options.timeout;
        }
        if (exists options.max_redirects) {
            mgr_opts.max_redirects = options.max_redirects;
        }
        if (exists options.max_response_body_size) {
            mgr_opts.max_response_body_size = options.max_response_body_size;
        }
        mgr_opts.assume_encoding = options.assume_encoding.val() ? options.assume_encoding : DefaultAssumedEncoding;
        if (options.proxy.val()) {
            mgr_opts.proxy_url = options.proxy;
        }
        *data cert = HttpClientDataProviderBase::getTlsData(options, "cert");
        if (exists cert) {
            mgr_opts.client_cert = new SSLCertificate(cert);
        }
        *data key = HttpClientDataProviderBase::getTlsData(options, "key");
        if (exists key) {
            mgr_opts.client_pk = new SSLPrivateKey(key, options.ssl_key_password);
        }
        return new HttpClientIo::HttpClientConnectionManager(mgr_opts);
    }

    #! Returns data provider constructor options from the configuration of a synchronous HTTP client
    /** @param http the synchronous HTTP client

        @return data provider constructor options with the client's URL (including user information), timeouts,
        protocol version, redirect handling, proxy, TLS settings and client certificate, default headers, and
        response handling options
    */
    static hash<auto> getOptionsFromClient(HTTPClient http) {
        hash<auto> config = http.getHttpConfig();
        string url = config.url ?? http.getURL();
        hash<auto> rv = {
            "url": url,
            "timeout": http.getTimeout(),
            "connect_timeout": http.getConnectTimeout(),
            "http_version": HttpVersionProtocolMap{config.http_version} ? config.http_version : "auto",
            "max_redirects": http.getMaxRedirects(),
            "redirect_passthru": http.getRedirectPassthru(),
            "error_passthru": http.getErrorPassthru(),
            "pre_encoded_urls": http.getPreEncodedUrls(),
            "assume_encoding": http.getAssumedEncoding(),
            "ssl_verify_cert": config.ssl_verify_cert.toBool(),
        };
        if (config.username.val()) {
            rv.username = config.username;
            rv.password = config.password;
        }
        if (*string proxy = http.getProxyURL()) {
            rv.proxy = proxy;
        }
        if (config.headers) {
            rv.headers = config.headers;
        }
        if (config.max_response_body_size) {
            rv.max_response_body_size = config.max_response_body_size;
        }
        if (exists config.ssl_cert_data) {
            rv.ssl_cert_data = config.ssl_cert_data;
        }
        if (exists config.ssl_key_data) {
            rv.ssl_key_data = config.ssl_key_data;
        }
        if (exists config.ssl_key_password) {
            rv.ssl_key_password = config.ssl_key_password;
        }
        return rv;
    }

    #! Translates the errors of the connection manager to the errors of the synchronous HTTP client
    /** @param ex the exception raised by the connection manager

        @throw SOCKET-TIMEOUT the request timed out
        @throw HTTP-CLIENT-MAXIMUM-REDIRECTS-EXCEEDED the maximum number of redirects was exceeded
        @throw HTTP-CLIENT-REDIRECT-ERROR a redirect response cannot be followed
    */
    static rethrowRequestError(hash<ExceptionInfo> ex) {
        switch (ex.err) {
            case "HTTPCLIENT-TIMEOUT":
                throw "SOCKET-TIMEOUT", ex.desc, ex.arg;
            case "HTTP-REDIRECT-ERROR":
                if (ex.desc =~ /^maximum redirects/) {
                    throw "HTTP-CLIENT-MAXIMUM-REDIRECTS-EXCEEDED", ex.desc, ex.arg;
                }
                throw "HTTP-CLIENT-REDIRECT-ERROR", ex.desc, ex.arg;
        }
        throw ex.err, ex.desc, ex.arg;
    }

    #! Percent-encodes a request path like @ref Qore::HTTPClient "HTTPClient" does
    /** @param path the request path
        @param pre_encoded if @ref True, the path must already be percent-encoded

        @return the encoded path: control characters, non-ASCII octets, and the characters
        <tt>space &lt; &gt; &quot; # % { } | \\ ^ ~ [ ] &#96;</tt> are percent-encoded; with \a pre_encoded, the path is
        returned unchanged

        @throw URL-ENCODING-ERROR \a pre_encoded is set and the path has a space, a control or non-ASCII character,
        or one of the characters <tt>{ } | \\ ^ [ ] &#96;</tt>
    */
    static string encodePath(string path, bool pre_encoded = False) {
        binary octets = path.toBinary();
        string rv;
        for (int i = 0; i < octets.size(); ++i) {
            int c = octets[i];
            string ch = chr(c);
            if (pre_encoded) {
                if (c <= 0x20 || c >= 0x7f || PreEncodedInvalidChars{ch}) {
                    throw "URL-ENCODING-ERROR", sprintf("URI path %y contains at least one unencoded character "
                        "('%%%02X'), when the 'pre_encoded_urls' option is set, URLs must be already encoded with "
                        "percent encoding", path, c);
                }
                continue;
            }
            if (c < 0x20 || c >= 0x7f || EncodeChars{ch}) {
                rv += sprintf("%%%02X", c);
            } else {
                rv += ch;
            }
        }
        return pre_encoded ? path : rv;
    }

    #! Sets up the object
    private setup(HttpClientIo::HttpClientConnectionManager mgr, string url, *hash<auto> options) {
        self.mgr = mgr;
        # like HTTPClient, a URL without a scheme is an http:// URL
        if (url !~ /^[a-zA-Z][a-zA-Z0-9+.-]*:\/\//) {
            url = "http://" + url;
        }
        *list<*string> parts = (url =~ x/^([a-zA-Z][a-zA-Z0-9+.-]*:\/\/)(?:([^@\/?#]*)@)?([^\/?#]*)(.*)$/s);
        if (!parts || !parts[2].val()) {
            # the URL is not included in the message, as it can contain credentials
            throw "CONSTRUCTOR-ERROR", "the target URL has no host";
        }
        origin = parts[0].lwr() + parts[2];
        url_path = parts[3];
        url_path =~ s/#.*//s;
        self.url = origin + url_path;

        default_headers = options.headers ?? {};
        if (!(map 1, keys default_headers, $1.lwr() == "authorization")) {
            *string username;
            *string password;
            if (options.username.val()) {
                username = options.username;
                password = options.password;
            } else if (parts[1].val()) {
                # user information in the URL is sent with HTTP basic authentication
                hash<UrlInfo> url_info = parse_url(url);
                username = url_info.username;
                password = url_info.password;
            }
            if (exists username) {
                default_headers.Authorization = "Basic " + make_base64_string(sprintf("%s:%s", username,
                    password ?? ""));
            }
        }
        error_passthru = options.error_passthru.toBool();
        pre_encoded_urls = options.pre_encoded_urls.toBool();
        if (options.assume_encoding.val()) {
            assume_encoding = options.assume_encoding;
        }
        if (options.timeout) {
            request_timeout = options.timeout;
        }
    }

    #! Returns the encoded request path
    private string getRequestPath(*string path) {
        if (!path.val()) {
            path = url_path.val() ? url_path : "/";
        } else if (path !~ /^\//) {
            path = "/" + path;
        }
        return HttpClientDataProviderBase::encodePath(path, pre_encoded_urls);
    }

    #! Returns the client certificate or private key data from the options, if any
    private static *data getTlsData(hash<auto> options, string type) {
        if (exists options{"ssl_" + type + "_data"}) {
            return options{"ssl_" + type + "_data"};
        }
        *string loc = options{"ssl_" + type + "_location"} ?? options{"ssl_" + type + "_path"};
        if (!loc.val()) {
            return;
        }
        # a location without a scheme is a file path
        return options{"ssl_" + type + "_der"}
            ? FileLocationHandler::getBinaryFileFromLocation(loc)
            : FileLocationHandler::getTextFileFromLocation(loc);
    }
}
}
