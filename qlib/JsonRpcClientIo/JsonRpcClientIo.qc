# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file JsonRpcClientIo.qc @brief Pure-Qore JSON-RPC client with async I/O

/*  JsonRpcClientIo.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! the %JsonRpcClientIo namespace holds all public definitions in the %JsonRpcClientIo module
public namespace JsonRpcClientIo {

#! Pure-Qore JSON-RPC client with async I/O via HttpClientIo
/** This class provides a JSON-RPC client implementation using
    \c HttpClientIo::HttpClientConnectionManager for async I/O with automatic protocol
    negotiation (HTTP/3 &rarr; HTTP/2 &rarr; HTTP/1.1) and connection pooling.

    It supports JSON-RPC versions 1.0, 1.1, and 2.0, matching the API of the native
    @ref Qore::Json::JsonRpcClient "JsonRpcClient" class.

    @par Example:
    @code{.py}
JsonRpcClientIo client("http://localhost:8080/JSON");
hash<auto> result = client.call("system.listMethods");
printf("methods: %N\n", result);
client.close();
    @endcode

    @since json 1.12
*/
public class JsonRpcClientIo {
    public {
        #! Default connect timeout (45 seconds, matches native JsonRpcClient)
        const DefaultConnectTimeout = 45000;

        #! Default I/O timeout (45 seconds, matches native JsonRpcClient)
        const DefaultTimeout = 45000;

        #! Default Content-Type header value
        const DefaultContentType = "application/json;charset=utf-8";

        #! Default Accept header value
        const DefaultAccept = "application/json";

        #! Default User-Agent header value
        const DefaultUserAgent = "Qore-JSON-RPC-Client-Io/1.0";
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

        #! JSON-RPC protocol version
        string jsonrpc_version = DefaultJsonRpcVersion;

        #! JSON-RPC request ID counter
        int request_id = 0;

        #! Mutex for thread-safe access to request_id and jsonrpc_version
        Mutex lck();

        #! Logger
        *LoggerInterface logger;

        #! Default headers for all requests
        hash<string, string> default_headers;
    }

    #! Creates the JsonRpcClientIo with the given URL and options
    /** @param url the URL of the JSON-RPC server; supports \c http://, \c https://,
        \c jsonrpc://, \c jsonrpcs://, \c jsonrpcio://, and \c jsonrpcios:// schemes
        @param opts optional client options:
        - \c connect_timeout: connection timeout in milliseconds (default: 45000)
        - \c timeout: transfer timeout in milliseconds (default: 45000)
        - \c version: JSON-RPC protocol version (\c "1.0", \c "1.1", or \c "2.0"; default: \c "2.0")
        - \c default_path: default URL path if none in URL (default: \c "JSON")
        - \c http_protocol: HTTP protocol to use (\c "auto", \c "h1", \c "h2", \c "h3"; default: \c "auto")
        - \c logger: a \c LoggerInterface object for logging
        - \c headers: extra default headers as a hash

        @par Example:
        @code{.py}
JsonRpcClientIo client("http://localhost:8080/JSON", {"timeout": 30000});
        @endcode
    */
    constructor(string url, *hash<auto> opts) {
        # Map custom schemes to http/https
        self.url = mapUrl(url);
        *string path = parse_url(self.url).path;
        string default_path = opts.default_path ?? DefaultJsonRpcPath;
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

        if (opts.version.val()) {
            jsonrpc_version = opts.version;
        }

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

    #! Creates the JsonRpcClientIo from a hash of options (legacy-compatible form)
    /** @param opts options hash; supports the same keys as the primary constructor \c opts parameter,
        plus a required \c url key
        @param no_connect this parameter is accepted for compatibility but ignored; connections
        are established on demand

        @par Example:
        @code{.py}
JsonRpcClientIo client({"url": "http://localhost:8080/JSON", "timeout": 30000});
        @endcode
    */
    constructor(hash<auto> opts, softbool no_connect = False) {
        string input_url = opts.url ?? "";
        if (!input_url.val()) {
            throw "JSONRPCCLIENTIO-ERROR", "missing required 'url' option";
        }

        self.url = mapUrl(input_url);
        *string path = parse_url(self.url).path;
        string default_path = opts.default_path ?? DefaultJsonRpcPath;
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

        if (opts.version.val()) {
            jsonrpc_version = opts.version;
        }

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
    /** @param method the JSON-RPC method name to call
        @param ... the arguments to the call

        @return a hash corresponding to the JSON-RPC response from the server

        @throw JSON-SERIALIZATION-ERROR cannot serialize value passed
        @throw JSON-PARSE-ERROR syntax error parsing JSON response
        @throw JSONRPCCLIENTIO-HTTP-ERROR HTTP error from server
        @throw JSONRPCCLIENTIO-RESPONSE-ERROR empty or invalid response

        @par Example:
        @code{.py}
hash<auto> result = client.call("method.name", arg1, arg2);
        @endcode
    */
    hash<auto> call(string method, ...) {
        int id = getNextId();
        string body = make_jsonrpc_request(method, jsonrpc_version, id, argv);
        return doRequest(body);
    }

    #! Calls a remote method using a single value after the method name for the method arguments
    /** @param method the JSON-RPC method name to call
        @param args an optional list of arguments (or single argument) for the method

        @return a hash corresponding to the JSON-RPC response from the server

        @throw JSON-SERIALIZATION-ERROR cannot serialize value passed
        @throw JSON-PARSE-ERROR syntax error parsing JSON response
        @throw JSONRPCCLIENTIO-HTTP-ERROR HTTP error from server
        @throw JSONRPCCLIENTIO-RESPONSE-ERROR empty or invalid response

        @par Example:
        @code{.py}
hash<auto> result = client.callArgs("method.name", arg_list);
        @endcode
    */
    hash<auto> callArgs(string method, auto args) {
        int id = getNextId();
        string body = make_jsonrpc_request(method, jsonrpc_version, id, args);
        return doRequest(body);
    }

    #! Calls a remote method with an explicit ID, taking all arguments after the method name
    /** @param id the \c id value for the call
        @param method the JSON-RPC method name to call
        @param ... the arguments to the call

        @return a hash corresponding to the JSON-RPC response from the server

        @throw JSON-SERIALIZATION-ERROR cannot serialize value passed
        @throw JSON-PARSE-ERROR syntax error parsing JSON response
        @throw JSONRPCCLIENTIO-HTTP-ERROR HTTP error from server
        @throw JSONRPCCLIENTIO-RESPONSE-ERROR empty or invalid response

        @par Example:
        @code{.py}
hash<auto> result = client.callId(42, "method.name", arg1, arg2);
        @endcode

        @since json 1.12
    */
    hash<auto> callId(auto id, string method, ...) {
        string body = make_jsonrpc_request(method, jsonrpc_version, id, argv);
        return doRequest(body);
    }

    #! Calls a remote method taking all arguments after the method name; populates a reference hash with debug info
    /** @param info a reference to a hash that provides the following keys on output:
        - \c request: the literal outgoing request body sent
        - \c request_headers: a hash of HTTP headers in the outgoing request
        - \c response_code: the HTTP response status code
        - \c response: the literal response body received from the server
        - \c response_headers: a hash of headers received in the response
        @param method the JSON-RPC method name to call
        @param ... the arguments to the call

        @return a hash corresponding to the JSON-RPC response from the server

        @throw JSON-SERIALIZATION-ERROR cannot serialize value passed
        @throw JSON-PARSE-ERROR syntax error parsing JSON response
        @throw JSONRPCCLIENTIO-HTTP-ERROR HTTP error from server
        @throw JSONRPCCLIENTIO-RESPONSE-ERROR empty or invalid response

        @par Example:
        @code{.py}
hash<auto> info;
hash<auto> result = client.callWithInfo(\info, "method.name", arg1, arg2);
        @endcode
    */
    hash<auto> callWithInfo(reference<hash<auto>> info, string method, ...) {
        int id = getNextId();
        string body = make_jsonrpc_request(method, jsonrpc_version, id, argv);
        return doRequestWithInfo(body, \info);
    }

    #! Calls a remote method using a single value for the arguments; populates a reference hash with debug info
    /** @param info a reference to a hash that provides the following keys on output:
        - \c request: the literal outgoing request body sent
        - \c request_headers: a hash of HTTP headers in the outgoing request
        - \c response_code: the HTTP response status code
        - \c response: the literal response body received from the server
        - \c response_headers: a hash of headers received in the response
        @param method the JSON-RPC method name to call
        @param args an optional list of arguments (or single argument) for the method

        @return a hash corresponding to the JSON-RPC response from the server

        @throw JSON-SERIALIZATION-ERROR cannot serialize value passed
        @throw JSON-PARSE-ERROR syntax error parsing JSON response
        @throw JSONRPCCLIENTIO-HTTP-ERROR HTTP error from server
        @throw JSONRPCCLIENTIO-RESPONSE-ERROR empty or invalid response

        @par Example:
        @code{.py}
hash<auto> info;
hash<auto> result = client.callArgsWithInfo(\info, "method.name", arg_list);
        @endcode
    */
    hash<auto> callArgsWithInfo(reference<hash<auto>> info, string method, auto args) {
        int id = getNextId();
        string body = make_jsonrpc_request(method, jsonrpc_version, id, args);
        return doRequestWithInfo(body, \info);
    }

    #! Calls a remote method with an explicit ID; populates a reference hash with debug info
    /** @param info a reference to a hash that provides the following keys on output:
        - \c request: the literal outgoing request body sent
        - \c request_headers: a hash of HTTP headers in the outgoing request
        - \c response_code: the HTTP response status code
        - \c response: the literal response body received from the server
        - \c response_headers: a hash of headers received in the response
        @param id the \c id value for the call
        @param method the JSON-RPC method name to call
        @param ... the arguments to the call

        @return a hash corresponding to the JSON-RPC response from the server

        @throw JSON-SERIALIZATION-ERROR cannot serialize value passed
        @throw JSON-PARSE-ERROR syntax error parsing JSON response
        @throw JSONRPCCLIENTIO-HTTP-ERROR HTTP error from server
        @throw JSONRPCCLIENTIO-RESPONSE-ERROR empty or invalid response

        @par Example:
        @code{.py}
hash<auto> info;
hash<auto> result = client.callIdWithInfo(\info, 42, "method.name", arg1, arg2);
        @endcode

        @since json 1.12
    */
    hash<auto> callIdWithInfo(reference<hash<auto>> info, auto id, string method, ...) {
        string body = make_jsonrpc_request(method, jsonrpc_version, id, argv);
        return doRequestWithInfo(body, \info);
    }

    #! Calls a remote method with an explicit ID and array arguments; populates a reference hash with debug info
    /** @param info a reference to a hash that provides the following keys on output:
        - \c request: the literal outgoing request body sent
        - \c request_headers: a hash of HTTP headers in the outgoing request
        - \c response_code: the HTTP response status code
        - \c response: the literal response body received from the server
        - \c response_headers: a hash of headers received in the response
        @param id the \c id value for the call
        @param method the JSON-RPC method name to call
        @param args an optional list of arguments (or single argument) for the method

        @return a hash corresponding to the JSON-RPC response from the server

        @throw JSON-SERIALIZATION-ERROR cannot serialize value passed
        @throw JSON-PARSE-ERROR syntax error parsing JSON response
        @throw JSONRPCCLIENTIO-HTTP-ERROR HTTP error from server
        @throw JSONRPCCLIENTIO-RESPONSE-ERROR empty or invalid response

        @par Example:
        @code{.py}
hash<auto> info;
hash<auto> result = client.callIdArgsWithInfo(\info, 42, "method.name", arg_list);
        @endcode

        @since json 1.12
    */
    hash<auto> callIdArgsWithInfo(reference<hash<auto>> info, auto id, string method, auto args) {
        string body = make_jsonrpc_request(method, jsonrpc_version, id, args);
        return doRequestWithInfo(body, \info);
    }

    #! Sends a JSON-RPC 2.0 notification (no response expected)
    /** A notification is a request without an \c id field; the server must not return a response.

        @param method the JSON-RPC method name
        @param ... the arguments to the notification

        @throw JSON-SERIALIZATION-ERROR cannot serialize value passed
        @throw JSONRPCCLIENTIO-VERSION-ERROR notifications require JSON-RPC 2.0

        @note notifications are only supported in JSON-RPC 2.0

        @par Example:
        @code{.py}
client.notify("log.message", "hello");
        @endcode

        @since json 1.12
    */
    nothing notify(string method, ...) {
        if (jsonrpc_version != "2.0") {
            throw "JSONRPCCLIENTIO-VERSION-ERROR",
                sprintf("notifications require JSON-RPC 2.0 (current version: %y)", jsonrpc_version);
        }
        hash request = {
            "jsonrpc": "2.0",
            "method": method,
        };
        if (argv) {
            request.params = argv;
        }
        doNotify(make_json(request));
    }

    #! Sends a JSON-RPC 2.0 notification with array-style arguments (no response expected)
    /** @param method the JSON-RPC method name
        @param args optional parameters for the notification

        @throw JSON-SERIALIZATION-ERROR cannot serialize value passed
        @throw JSONRPCCLIENTIO-VERSION-ERROR notifications require JSON-RPC 2.0

        @note notifications are only supported in JSON-RPC 2.0

        @par Example:
        @code{.py}
client.notifyArgs("log.message", ("hello",));
        @endcode

        @since json 1.12
    */
    nothing notifyArgs(string method, auto args) {
        if (jsonrpc_version != "2.0") {
            throw "JSONRPCCLIENTIO-VERSION-ERROR",
                sprintf("notifications require JSON-RPC 2.0 (current version: %y)", jsonrpc_version);
        }
        hash request = {
            "jsonrpc": "2.0",
            "method": method,
        };
        if (exists args) {
            request.params = args;
        }
        doNotify(make_json(request));
    }

    #! Sets the JSON-RPC protocol version
    /** @param version the JSON-RPC protocol version (\c "1.0", \c "1.1", or \c "2.0")

        @par Example:
        @code{.py}
client.setVersion("1.0");
        @endcode
    */
    nothing setVersion(string version) {
        lck.lock();
        on_exit lck.unlock();
        jsonrpc_version = version;
    }

    #! Returns the JSON-RPC protocol version
    /** @return the current JSON-RPC protocol version

        @par Example:
        @code{.py}
string ver = client.getVersion();
        @endcode
    */
    string getVersion() {
        lck.lock();
        on_exit lck.unlock();
        return jsonrpc_version;
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

    #! Internal: Returns the next request ID, incrementing the counter
    private int getNextId() {
        lck.lock();
        on_exit lck.unlock();
        return ++request_id;
    }

    #! Internal: Sends a JSON-RPC request via HTTP POST and returns the parsed response
    /** @param body the JSON-RPC request body as a string

        @return the parsed JSON-RPC response hash

        @throw JSONRPCCLIENTIO-HTTP-ERROR HTTP error from server
        @throw JSONRPCCLIENTIO-RESPONSE-ERROR empty response body
    */
    private hash<auto> doRequest(string body) {
        hash<auto> resp = mgr.request(url, "POST", url_path, default_headers, body, io_timeout);

        if (resp.status_code < 200 || resp.status_code >= 300) {
            throw "JSONRPCCLIENTIO-HTTP-ERROR",
                sprintf("HTTP error %d from %s", resp.status_code, url);
        }

        if (!resp.body) {
            throw "JSONRPCCLIENTIO-RESPONSE-ERROR", "empty response body from server";
        }

        return parse_json(resp.body);
    }

    #! Internal: Sends a JSON-RPC request via HTTP POST, populates debug info, and returns the parsed response
    /** @param body the JSON-RPC request body as a string
        @param info a reference to a hash that will be populated with debug info

        @return the parsed JSON-RPC response hash

        @throw JSONRPCCLIENTIO-HTTP-ERROR HTTP error from server
        @throw JSONRPCCLIENTIO-RESPONSE-ERROR empty response body
    */
    private hash<auto> doRequestWithInfo(string body, reference<hash<auto>> info) {
        info.request = body;
        info.request_headers = {} + default_headers;

        hash<auto> resp = mgr.request(url, "POST", url_path, default_headers, body, io_timeout);

        info.response_code = resp.status_code;
        info.response = resp.body;
        info.response_headers = resp.headers;

        if (resp.status_code < 200 || resp.status_code >= 300) {
            throw "JSONRPCCLIENTIO-HTTP-ERROR",
                sprintf("HTTP error %d from %s", resp.status_code, url);
        }

        if (!resp.body) {
            throw "JSONRPCCLIENTIO-RESPONSE-ERROR", "empty response body from server";
        }

        return parse_json(resp.body);
    }

    #! Internal: Sends a JSON-RPC notification via HTTP POST (no response expected)
    /** @param body the JSON-RPC notification body as a string
    */
    private nothing doNotify(string body) {
        mgr.request(url, "POST", url_path, default_headers, body, io_timeout);
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
    /** @param input_url the input URL potentially with jsonrpc[s]://, jsonrpcio[s]://, etc.
        @return the URL with http:// or https:// scheme
    */
    private static string mapUrl(string input_url) {
        if (input_url =~ /^jsonrpcios:/) {
            return regex_subst(input_url, "^jsonrpcios:", "https:");
        }
        if (input_url =~ /^jsonrpcio:/) {
            return regex_subst(input_url, "^jsonrpcio:", "http:");
        }
        if (input_url =~ /^jsonrpcs:/) {
            return regex_subst(input_url, "^jsonrpcs:", "https:");
        }
        if (input_url =~ /^jsonrpc:/) {
            return regex_subst(input_url, "^jsonrpc:", "http:");
        }
        return input_url;
    }
}

} # namespace JsonRpcClientIo
