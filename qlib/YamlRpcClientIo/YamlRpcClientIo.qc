# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file YamlRpcClientIo.qc @brief Pure-Qore YAML-RPC client with async I/O

/*  YamlRpcClientIo.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Pure-Qore YAML-RPC client with async I/O via HttpClientIo
/** This class provides a YAML-RPC client implementation using
    \c HttpClientIo::HttpClientConnectionManager for async I/O with automatic protocol
    negotiation (HTTP/3 &rarr; HTTP/2 &rarr; HTTP/1.1) and connection pooling.

    It matches the API of the \c YamlRpcClient class.

    @par Example:
    @code{.py}
YamlRpcClientIo client("http://localhost:8080/YAML");
auto result = client.call("system.listMethods");
printf("methods: %N\n", result);
client.close();
    @endcode

    @since yaml 1.3
*/
public class YamlRpcClientIo {
    public {
        #! Default connect timeout (45 seconds, matches native YamlRpcClient)
        const DefaultConnectTimeout = 45000;

        #! Default I/O timeout (45 seconds, matches native YamlRpcClient)
        const DefaultTimeout = 45000;

        #! Default Content-Type header value
        const DefaultContentType = MimeTypeYamlRpc + ";charset=utf-8";

        #! Default Accept header value
        const DefaultAccept = MimeTypeYamlRpc;

        #! Default User-Agent header value
        const DefaultUserAgent = "Qore-YAML-RPC-Client-Io/1.0";
    }

    private {
        #! The server URL (with http/https scheme)
        string url;

        #! The request path extracted from the URL
        string url_path;

        #! Async I/O connection manager for all HTTP requests
        HttpClientIo::HttpClientConnectionManager mgr;

        #! Connect timeout in milliseconds
        int connect_timeout;

        #! Transfer timeout in milliseconds
        int io_timeout;

        #! Protocol for the connection manager ("h1", "h2", "h3", or "auto")
        *string http_protocol;

        #! YAML serialization flags
        int yaml_flags = YAML::None;

        #! Logger
        *LoggerInterface logger;

        #! Default headers for all requests
        hash<string, string> default_headers;
    }

    #! Creates the YamlRpcClientIo with the given URL and options
    /** @param url the URL of the YAML-RPC server; supports \c http://, \c https://,
        \c yamlrpc://, \c yamlrpcs://, \c yamlrpcio://, and \c yamlrpcios:// schemes
        @param opts optional client options:
        - \c connect_timeout: connection timeout in milliseconds (default: 45000)
        - \c timeout: transfer timeout in milliseconds (default: 45000)
        - \c default_path: default URL path if none in URL (default: \c "YAML")
        - \c http_protocol: HTTP protocol to use (\c "auto", \c "h1", \c "h2", \c "h3"; default: \c "auto")
        - \c logger: a \c LoggerInterface object for logging
        - \c headers: extra default headers as a hash
        - \c flags: YAML serialization flags (default: \c YAML::None)

        @par Example:
        @code{.py}
YamlRpcClientIo client("http://localhost:8080/YAML", {"timeout": 30000});
        @endcode
    */
    constructor(string url, *hash<auto> opts) {
        # Map custom schemes to http/https
        self.url = mapUrl(url);
        *string path = parse_url(self.url).path;
        string default_path = opts.default_path ?? DefaultYamlRpcPath;
        if (!path.val()) {
            # Append default path if none in URL
            url_path = "/" + default_path;
            self.url += url_path;
        } else {
            url_path = path;
        }

        connect_timeout = opts.connect_timeout ?? DefaultConnectTimeout;
        io_timeout = opts.timeout ?? DefaultTimeout;
        http_protocol = opts.http_protocol;
        logger = opts.logger;
        yaml_flags |= opts.flags ?? YAML::None;

        default_headers = {
            "Content-Type": DefaultContentType,
            "Accept": DefaultAccept,
            "User-Agent": DefaultUserAgent,
        };
        if (opts.headers.typeCode() == NT_HASH) {
            default_headers += cast<hash<string, string>>(opts.headers);
        }

        mgr = createConnectionManager();
    }

    #! Creates the YamlRpcClientIo from a hash of options (legacy-compatible form)
    /** @param opts options hash; supports the same keys as the primary constructor \c opts parameter,
        plus a required \c url key
        @param no_connect this parameter is accepted for compatibility but ignored; connections
        are established on demand

        @par Example:
        @code{.py}
YamlRpcClientIo client({"url": "http://localhost:8080/YAML", "timeout": 30000});
        @endcode
    */
    constructor(hash<auto> opts, softbool no_connect = False) {
        string input_url = opts.url ?? "";
        if (!input_url.val()) {
            throw "YAMLRPCCLIENTIO-ERROR", "missing required 'url' option";
        }

        self.url = mapUrl(input_url);
        *string path = parse_url(self.url).path;
        string default_path = opts.default_path ?? DefaultYamlRpcPath;
        if (!path.val()) {
            url_path = "/" + default_path;
            self.url += url_path;
        } else {
            url_path = path;
        }

        connect_timeout = opts.connect_timeout ?? DefaultConnectTimeout;
        io_timeout = opts.timeout ?? DefaultTimeout;
        http_protocol = opts.http_protocol;
        logger = opts.logger;
        yaml_flags |= opts.flags ?? YAML::None;

        default_headers = {
            "Content-Type": DefaultContentType,
            "Accept": DefaultAccept,
            "User-Agent": DefaultUserAgent,
        };
        if (opts.headers.typeCode() == NT_HASH) {
            default_headers += cast<hash<string, string>>(opts.headers);
        }

        mgr = createConnectionManager();
    }

    #! Destroys the client and cleans up resources
    destructor() {
        close();
    }

    #! Calls a remote method taking all arguments after the method name for the method arguments
    /** @param method the YAML-RPC method name to call
        @param ... the arguments to the call

        @return the parsed YAML-RPC response; a hash with a \c result key on success or an \c error
        key on error

        @throw YAML-SERIALIZATION-ERROR cannot serialize value passed
        @throw YAML-PARSE-ERROR syntax error parsing YAML response
        @throw YAMLRPCCLIENTIO-HTTP-ERROR HTTP error from server
        @throw YAMLRPCCLIENTIO-RESPONSE-ERROR empty or invalid response

        @par Example:
        @code{.py}
auto result = client.call("method.name", arg1, arg2);
        @endcode
    */
    auto call(string method, ...) {
        string body = makeRequest(method, argv, yaml_flags);
        return doRequest(body);
    }

    #! Calls a remote method using a single value after the method name for the method arguments
    /** @param method the YAML-RPC method name to call
        @param args an optional list of arguments (or single argument) for the method

        @return the parsed YAML-RPC response; a hash with a \c result key on success or an \c error
        key on error

        @throw YAML-SERIALIZATION-ERROR cannot serialize value passed
        @throw YAML-PARSE-ERROR syntax error parsing YAML response
        @throw YAMLRPCCLIENTIO-HTTP-ERROR HTTP error from server
        @throw YAMLRPCCLIENTIO-RESPONSE-ERROR empty or invalid response

        @par Example:
        @code{.py}
auto result = client.callArgs("method.name", arg_list);
        @endcode
    */
    auto callArgs(string method, auto args) {
        string body = makeRequest(method, args, yaml_flags);
        return doRequest(body);
    }

    #! Calls a remote method taking all arguments after the method name; populates a reference hash with debug info
    /** @param info a reference to a hash that provides the following keys on output:
        - \c request: the literal outgoing request body sent
        - \c request_headers: a hash of HTTP headers in the outgoing request
        - \c response_code: the HTTP response status code
        - \c response: the literal response body received from the server
        - \c response_headers: a hash of headers received in the response
        @param method the YAML-RPC method name to call
        @param ... the arguments to the call

        @return the parsed YAML-RPC response; a hash with a \c result key on success or an \c error
        key on error

        @throw YAML-SERIALIZATION-ERROR cannot serialize value passed
        @throw YAML-PARSE-ERROR syntax error parsing YAML response
        @throw YAMLRPCCLIENTIO-HTTP-ERROR HTTP error from server
        @throw YAMLRPCCLIENTIO-RESPONSE-ERROR empty or invalid response

        @par Example:
        @code{.py}
hash<auto> info;
auto result = client.callWithInfo(\info, "method.name", arg1, arg2);
        @endcode
    */
    auto callWithInfo(reference<hash<auto>> info, string method, ...) {
        string body = makeRequest(method, argv, yaml_flags);
        return doRequestWithInfo(body, \info);
    }

    #! Calls a remote method using a single value for the arguments; populates a reference hash with debug info
    /** @param info a reference to a hash that provides the following keys on output:
        - \c request: the literal outgoing request body sent
        - \c request_headers: a hash of HTTP headers in the outgoing request
        - \c response_code: the HTTP response status code
        - \c response: the literal response body received from the server
        - \c response_headers: a hash of headers received in the response
        @param method the YAML-RPC method name to call
        @param args an optional list of arguments (or single argument) for the method

        @return the parsed YAML-RPC response; a hash with a \c result key on success or an \c error
        key on error

        @throw YAML-SERIALIZATION-ERROR cannot serialize value passed
        @throw YAML-PARSE-ERROR syntax error parsing YAML response
        @throw YAMLRPCCLIENTIO-HTTP-ERROR HTTP error from server
        @throw YAMLRPCCLIENTIO-RESPONSE-ERROR empty or invalid response

        @par Example:
        @code{.py}
hash<auto> info;
auto result = client.callArgsWithInfo(\info, "method.name", arg_list);
        @endcode
    */
    auto callArgsWithInfo(reference<hash<auto>> info, string method, auto args) {
        string body = makeRequest(method, args, yaml_flags);
        return doRequestWithInfo(body, \info);
    }

    #! Returns the server URL
    /** @return the full URL including path
    */
    string getUrl() {
        return url;
    }

    #! Closes the client and releases resources
    /** The client can be reused after calling close(); a new connection will be established
        on the next request.
    */
    close() {
        mgr.closeAll();
        # Recreate the manager so the client can be reused after close
        mgr = createConnectionManager();
    }

    #! Makes a YAML-RPC request string
    /** @param method the remote method name
        @param arg the argument(s) for the method
        @param flags YAML serialization flags (default: \c YAML::None)

        @return the YAML-serialized request string
    */
    static string makeRequest(string method, auto arg, int flags = YAML::None) {
        return make_yaml({"method": method, "params": arg}, flags);
    }

    #! Internal: Sends a YAML-RPC request via HTTP POST and returns the parsed response
    /** @param body the YAML-RPC request body as a string

        @return the parsed YAML-RPC response

        @throw YAMLRPCCLIENTIO-HTTP-ERROR HTTP error from server
        @throw YAMLRPCCLIENTIO-RESPONSE-ERROR empty response body
    */
    private auto doRequest(string body) {
        hash<auto> resp = mgr.request(url, "POST", url_path, default_headers, body, io_timeout);

        if (resp.status_code < 200 || resp.status_code >= 300) {
            throw "YAMLRPCCLIENTIO-HTTP-ERROR",
                sprintf("HTTP error %d from %s", resp.status_code, url);
        }

        if (!resp.body) {
            throw "YAMLRPCCLIENTIO-RESPONSE-ERROR", "empty response body from server";
        }

        return parse_yaml(resp.body);
    }

    #! Internal: Sends a YAML-RPC request via HTTP POST, populates debug info, and returns the parsed response
    /** @param body the YAML-RPC request body as a string
        @param info a reference to a hash that will be populated with debug info

        @return the parsed YAML-RPC response

        @throw YAMLRPCCLIENTIO-HTTP-ERROR HTTP error from server
        @throw YAMLRPCCLIENTIO-RESPONSE-ERROR empty response body
    */
    private auto doRequestWithInfo(string body, reference<hash<auto>> info) {
        info.request = body;
        info.request_headers = {} + default_headers;

        hash<auto> resp = mgr.request(url, "POST", url_path, default_headers, body, io_timeout);

        info.response_code = resp.status_code;
        info.response = resp.body;
        info.response_headers = resp.headers;

        if (resp.status_code < 200 || resp.status_code >= 300) {
            throw "YAMLRPCCLIENTIO-HTTP-ERROR",
                sprintf("HTTP error %d from %s", resp.status_code, url);
        }

        if (!resp.body) {
            throw "YAMLRPCCLIENTIO-RESPONSE-ERROR", "empty response body from server";
        }

        return parse_yaml(resp.body);
    }

    #! Internal: Creates a new HttpClientConnectionManager
    private HttpClientIo::HttpClientConnectionManager createConnectionManager() {
        return new HttpClientIo::HttpClientConnectionManager(
            <HttpClientIo::HttpClientConnectionManagerOptions>{
                "connect_timeout": connect_timeout,
                "request_timeout": io_timeout,
                "protocol": http_protocol ?? "auto",
            });
    }

    #! Internal: Maps custom URL schemes to http/https
    /** @param input_url the input URL potentially with yamlrpc[s]://, yamlrpcio[s]://, etc.
        @return the URL with http:// or https:// scheme
    */
    private static string mapUrl(string input_url) {
        if (input_url =~ /^yamlrpcios:/) {
            return regex_subst(input_url, "^yamlrpcios:", "https:");
        }
        if (input_url =~ /^yamlrpcio:/) {
            return regex_subst(input_url, "^yamlrpcio:", "http:");
        }
        if (input_url =~ /^yamlrpcs:/) {
            return regex_subst(input_url, "^yamlrpcs:", "https:");
        }
        if (input_url =~ /^yamlrpc:/) {
            return regex_subst(input_url, "^yamlrpc:", "http:");
        }
        return input_url;
    }
}

} # namespace YamlRpcClientIo
