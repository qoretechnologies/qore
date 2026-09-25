# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FileLocationHandlerRest.qc module for returning file data from a URL-like location string

/*  FileLocationHandlerRest Copyright 2021 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FileLocationHandler module
public namespace FileLocationHandler {
#! The class for handling "rest(s)://"" locations
/** Requests are made with a \c RestClientIo::RestClientIo client; see @ref filelocationhandler_http.

    A handler normally builds its own client for each request from the location URL.  It can instead be bound to
    an already-authenticated client, in which case the location is a URI path on that client's target rather than
    a URL; this is how a REST connection provides a native file location handler without reimplementing
    authentication, connection pooling, or token refresh.

    @note An unbound handler does not support writing to output streams; a bound handler does, by buffering the
    data written and sending it when the stream is closed

    @see @ref FileLocationHandler::FileLocationHandler::getRestHandlerForClient() "FileLocationHandler::getRestHandlerForClient()"
*/
class FileLocationHandlerRest inherits AbstractFileLocationHandler {
    private {
        #! The already-authenticated client this handler is bound to, if any
        *object bound_client;

        #! The asynchronous client used for requests by a bound handler, if the bound client is supported
        *object bound_rest;
    }

    #! Creates a handler that builds a client for each request from the location URL
    constructor() {
    }

    #! Creates a handler bound to an already-authenticated client
    /** @param client the client to use for every request: a \c RestClientIo::RestClientIo object (or a subclass),
        which is used for requests, or a synchronous \c RestClient::RestClient object, in which case requests are
        made with a \c RestClientIo::RestClientIo object created with its constructor options and connection path

        @note
        - When bound, the location is a URI path on the client's target rather than a URL, and the handler
          neither configures nor closes the client
        - The operations of a handler bound to another kind of object raise \c UNIMPLEMENTED
    */
    constructor(object client) {
        bound_client = client;
        bound_rest = RestIoSupport::getAsyncClient(client);
    }

    #! Returns @ref True if this handler is bound to an already-authenticated client
    private bool isBoundImpl() {
        return exists bound_client;
    }

    public {
        #! Valid REST location options
        /** @see <a href="../../RestClient/html/index.html">RestClient::constructor()</a> for supported options
        */
        const RestClientLocationOpts = {
            "connect_timeout": <FileHandlerOptionInfo>{
                "type": "int",
                "desc": "Connection timeout in milliseconds",
            },
            "content_encoding": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "The content encoding (compression algorithm) for sending (if `send_encoding` is not set) "
                    "and receiving",
            },
            "data": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "The data serialization option to use",
                "default_value": "auto",
            },
            "error_passthru": <FileHandlerOptionInfo>{
                "type": "bool",
                "desc": "Error responses will be passed to the caller instead of causing an exception to be raised",
            },
            "headers": <FileHandlerOptionInfo>{
                "type": "hash",
                "desc": "An optional hash of headers to send with the request",
            },
            "http_version": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "The HTTP version to use",
                "default_value": "1.1",
            },
            "max_redirects": <FileHandlerOptionInfo>{
                "type": "int",
                "desc": "The maximum number of redirect responses to process",
                "default_value": 5,
            },
            "method": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "The HTTP method to use; if not given then `GET` is used for reads, and `POST` is used for "
                    "writes",
            },
            "proxy": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "The proxy URL for connecting through a proxy",
            },
            "redirect_passthru": <FileHandlerOptionInfo>{
                "type": "bool",
                "desc": "Redirect responses will be passed to the caller instead of followed",
            },
            "send_encoding": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "The content encoding (compression algorithm) to use when sending data",
            },
            "ssl_cert_data": <FileHandlerOptionInfo>{
                "type": "data",
                "desc": "The X.509 certificate data in PEM format (string) or in DER format (binary); if this "
                    "option is set, then `ssl_cert_path` is ignored",
            },
            "ssl_cert_path": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "A path to an X.509 client certificate file in PEM format; if this option is used, then the "
                    "calling context must not be restricted with sandbox restriction `PO_NO_FILESYSTEM` which is "
                    "checked at runtime",
            },
            "ssl_key_data": <FileHandlerOptionInfo>{
                "type": "data",
                "desc": "The X.509 private key data in PEM format (string) or in DER format (binary); if this "
                    "option is set, then `ssl_key_path` is ignored",
            },
            "ssl_key_path": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "A path to a private key file in PEM format; if this option is used, then the "
                    "calling context must not be restricted with sandbox restriction `PO_NO_FILESYSTEM` which is "
                    "checked at runtime",
            },
            "ssl_key_password": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "The password to the private key given with `ssl_key_path`",
            },
            "ssl_verify_cert": <FileHandlerOptionInfo>{
                "type": "bool",
                "desc": "The server's certificate will only be accepted if it's verified",
            },
            "swagger": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "The location of a Swagger schema to use for message validation",
            },
            "swagger_base_path": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "In case a REST validator is used, the base path in the schema can be overridden "
                    "with this option (applies to any REST validator; not just Swagger validators)",
            },
            "timeout": <FileHandlerOptionInfo>{
                "type": "int",
                "desc": "I/O timeout in milliseconds",
            },
        };

        #! Location options supported when the handler is bound to a client
        /** Every other option in @ref RestClientLocationOpts configures the construction of a client, which a
            bound handler does not do; accepting one would silently ignore it
        */
        const BoundLocationOpts = {
            "encoding": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "The string encoding for any strings returned or sent",
            },
            "headers": RestClientLocationOpts.headers,
            "method": RestClientLocationOpts.method,
        };
    }

    #! Retrieves a text file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return the string of the file's contents, if it can be retrieved

        @throw REST-DATA-ERROR The REST call returned a data type that cannot be converted to a string

        @note This method can throw any exception related to making a REST call as well
    */
    private string getTextFileImpl(string scheme, string location, *hash<auto> opts) {
        # make sure we can accept any Content-Type
        if (!opts.headers.Accept) {
            opts.headers.Accept = "*/*";
        }
        auto val = request(scheme, location, opts, "GET").body;
        if (!exists val) {
            return "";
        }
        if (val.typeCode() == NT_STRING) {
            return val;
        }
        if (!val.strp()) {
            throw "REST-DATA-ERROR", sprintf("%s://%s returned data of type %y; expecting \"string\"",
                scheme, AbstractFileLocationHandler::removeUserInfo(location), val.fullType());
        }
        return val.toString();
    }

    #! Retrieves a binary file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @throw REST-DATA-ERROR The REST call returned a data type that cannot be converted to a binary

        @note This method can throw any exception related to making a REST call as well
    */
    private binary getBinaryFileImpl(string scheme, string location, *hash<auto> opts) {
        auto val = request(scheme, location, opts, "GET").body;
        if (!exists val) {
            return binary();
        }
        if (val.typeCode() == NT_BINARY) {
            return val;
        }
        if (!val.strp()) {
            throw "REST-DATA-ERROR", sprintf("%s://%s returned data of type %y; expecting \"binary\" or \"string\"",
                scheme, AbstractFileLocationHandler::removeUserInfo(location), val.fullType());
        }
        return val.toString().toBinary();
    }

    #! Retrieves an I/O poller to retrieve a binary file from the given location
    /** The poller's output is the response body before deserialization; error responses are returned like any
        other response

        @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return an I/O poller to retrieve the file's data
    */
    private Qore::AbstractPollOperation getIoPollerForLocationImpl(string scheme, string location,
            *hash<auto> opts) {
        return new RestIoBodyPollOperation(startRequest(scheme, location, opts, "GET"));
    }

    #! Retrieves a resource and its metadata from the given location
    /** The request is made with the REST client's request processing (authentication and token refresh); the
        data is the response body before deserialization.  An HTTP error status raises
        \c HTTP-CLIENT-RECEIVE-ERROR, as for \c http:// locations, unless the \c error_passthru option is enabled.

        @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return the resource's data and metadata

        @note This method can throw any exception related to making a REST call

        @since FileLocationHandler 3.0
    */
    private hash<FileResourceResult> getResourceImpl(string scheme, string location, *hash<auto> opts) {
        hash<auto> resp = request(scheme, location, opts, getReadMethod(opts));
        return FileLocationHandlerHttp::getIoResourceResult(getResourceRequestedLocation(scheme, location),
            resp + {"body": resp.response_body_raw});
    }

    #! Retrieves an I/O poller to retrieve a resource and its metadata from the given location
    /** The poller uses the REST client's asynchronous request processing and reports the same result and errors
        as @ref getResourceImpl() without blocking

        @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return an I/O poller whose output is the resource's data and metadata

        @since FileLocationHandler 3.0
    */
    private Qore::AbstractPollOperation<hash<FileResourceResult>> getResourcePollerImpl(string scheme,
            string location, *hash<auto> opts) {
        RestIoRequestPollOperation op = startRequest(scheme, location, opts, getReadMethod(opts));
        *string requested_location = getResourceRequestedLocation(scheme, location);
        bool error_passthru = op.getErrorPassthru();
        return new FileResourcePollOperation(op, hash<FileResourceResult> sub (auto output) {
            if (!error_passthru) {
                RestIoSupport::checkResponse(output);
            }
            return FileLocationHandlerHttp::getIoResourceResult(requested_location,
                output + {"body": output.response_body_raw});
        });
    }

    #! Returns the requested location of a resource, or @ref nothing for a bound handler
    /** For a bound handler the location is a path that the client prepares with its connection and base paths, so
        the requested location is the URL of the first request made; see
        @ref FileLocationHandlerHttp::getIoResourceResult()
    */
    private *string getResourceRequestedLocation(string scheme, string location) {
        return bound_client ? NOTHING : AbstractFileLocationHandler::getRequestedLocation(scheme, location);
    }

    #! Returns the read method
    private static string getReadMethod(*hash<auto> opts) {
        return opts.method ? opts.method.upr() : "GET";
    }

    #! Returns a stream reader for the file's data at the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return a stream reader for the file's contents, if it can be retrieved

        @throw REST-DATA-ERROR The REST call returned a data type that cannot be converted to a binary

        @note This method can throw any exception related to making a REST call as well
    */
    private Qore::StreamReader getStreamReaderImpl(string scheme, string location, *hash<auto> opts) {
        return new StreamReader(getBinaryStreamImpl(scheme, location, opts));
    }

    #! Retrieves a binary file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return an input stream of the file's contents, if it can be retrieved

        @throw REST-DATA-ERROR The REST call returned a data type that cannot be converted to a binary

        @note This method can throw any exception related to making a REST call as well
    */
    private Qore::InputStream getBinaryStreamImpl(string scheme, string location, *hash<auto> opts) {
        return new BinaryInputStream(source: getBinaryFileImpl(scheme, location, opts));
    }

    #! Writes data to a file at the given location
    /** The data is serialized according to the client's data serialization setting

        @param scheme the scheme being used
        @param location the location string without the scheme
        @param contents the file's contents
        @param opts write options

        @note This method can throw any exception related to making a REST call
    */
    private writeFileImpl(string scheme, string location, data contents, *hash<auto> opts) {
        request(scheme, location, opts, "POST", contents);
    }

    #! Returns an output stream for writing data to the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts write options

        @return an output stream for writing to the file

        @throw STREAM-WRITE-ERROR an unbound \c rest(s):// location does not support writing to an output stream

        @note
        - Only a handler bound to a client supports this; an unbound \c rest(s):// location has no authenticated
          client to send to and throws \c STREAM-WRITE-ERROR, directing the caller to \c http(s):// instead
        - REST is request/response, so the data written is buffered and sent when the stream is closed;
          OutputStreamWrapper::close() must be called, and OutputStreamWrapper::waitForIo() rethrows any error
          raised by the request
    */
    private OutputStreamWrapper getOutputStreamImpl(string scheme, string location, *hash<auto> opts) {
        if (!bound_client) {
            throw "STREAM-WRITE-ERROR", sprintf("%s:// locations do not support writing with an output stream; use "
                "http(s):// locations instead", scheme);
        }

        StreamPipe pipe(sync_close: False);

        WaitGroup io_counter();
        io_counter.add();
        OutputStreamWrapper rv(pipe.getOutputStream(), io_counter);

        background sub () {
            # NOTE: done() must fire after the catch block records the error, so that waitForIo() observes the
            # error before the wait group drops to zero
            on_exit io_counter.done();
            try {
                StreamReader reader(pipe.getInputStream());
                *binary content = reader.readBinary();
                writeFileImpl(scheme, location, content ?? binary(), opts);
            } catch (hash<ExceptionInfo> ex) {
                rv.setIoError(ex);
            }
        }();

        return rv;
    }

    #! Gets supported read options
    /** @return supported read options
    */
    private hash<string, hash<FileHandlerOptionInfo>> getReadOptionsImpl() {
        return bound_client ? BoundLocationOpts : RestClientLocationOpts;
    }

    #! Gets supported write options
    /** @return supported write options
    */
    private hash<string, hash<FileHandlerOptionInfo>> getWriteOptionsImpl() {
        return bound_client ? BoundLocationOpts : RestClientLocationOpts;
    }

    #! Makes a request and returns the response; the response status is checked unless errors are passed through
    private hash<auto> request(string scheme, string location, *hash<auto> opts, string default_method,
            *data body) {
        RestIoRequestPollOperation op = startRequest(scheme, location, opts, default_method, body);
        hash<auto> resp = HttpIoSupport::runSync(op, op.getTimeout());
        if (!op.getErrorPassthru()) {
            RestIoSupport::checkResponse(resp);
        }
        return resp;
    }

    #! Starts a request
    /** @throw UNIMPLEMENTED the handler is bound to an unsupported client
        @throw LOCATION-ERROR the location has no URI path
    */
    private RestIoRequestPollOperation startRequest(string scheme, string location, *hash<auto> opts,
            string default_method, *data body) {
        string method = opts.method ? opts.method.upr() : default_method;
        if (bound_client) {
            if (!bound_rest) {
                throw "UNIMPLEMENTED", sprintf("REST client class %y does not provide the asynchronous request API "
                    "required by this handler; expecting a RestClientIo or a RestClient object",
                    bound_client.className());
            }
            # the client already targets the right host; the location is a URI path on it
            string path = HttpIoSupport::encodeRequestTarget(AbstractFileLocationHandler::getUriPath(location));
            return new RestIoRequestPollOperation(bound_rest, method, path, body, opts.headers, False,
                HttpIoSupport::DefaultMaxRedirects);
        }

        string url = (scheme == "rests" ? "https" : "http") + "://" + location;
        hash<UrlInfo> url_info = parse_url(url);
        if (!url_info.path) {
            throw "LOCATION-ERROR", sprintf("REST location %y missing URI path",
                AbstractFileLocationHandler::removeUserInfo(url));
        }
        # the request path is everything after the authority, including any query
        *list<*string> parts = (url =~ x/^([a-z]+:\/\/[^\/?#]*)([^#]*)/);
        string path = HttpIoSupport::encodeRequestTarget(parts[1]);
        hash<auto> client_opts = opts - ("method", "max_redirects", "error_passthru", "redirect_passthru") + {
            "url": parts[0],
            # the handler follows and reports redirects and checks the response status itself
            "error_passthru": True,
            "allow_any_response": True,
        };
        if (!exists client_opts.ssl_verify_cert) {
            # like the synchronous client, server certificates are only verified if requested
            client_opts.ssl_verify_cert = False;
        }
        object rest = RestIoSupport::createClient(client_opts);
        return new RestIoRequestPollOperation(rest, method, path, body, opts.headers, False,
            opts.max_redirects ?? HttpIoSupport::DefaultMaxRedirects, opts.error_passthru.toBool(),
            opts.redirect_passthru.toBool());
    }
}

#! Supporting functions for REST requests made with \c RestClientIo
/** This module cannot declare a dependency on \c RestClientIo, which depends on this module; \c RestClientIo is
    loaded when a REST location is used, and its objects are used through the \c object type.

    @since FileLocationHandler 3.0
*/
class RestIoSupport {
    public {
        #! The module providing the REST client
        const ModuleName = "RestClientIo";

        #! The REST client class
        const ClientClass = "RestClientIo::RestClientIo";
    }

    #! Creates a REST client
    static object createClient(hash<auto> opts, *object mgr) {
        load_module(ModuleName);
        return exists mgr ? create_object(ClientClass, opts, mgr) : create_object(ClientClass, opts);
    }

    #! Returns @ref True if the object provides the asynchronous request API of \c RestClientIo::RestClientIo
    static bool isAsyncClient(object client) {
        return client.hasCallableMethod("startPollRawRequest") && client.hasCallableMethod("startPollRawRetry")
            && client.hasCallableMethod("getConnectionManager");
    }

    #! Returns the asynchronous client to use for a bound handler, or @ref nothing if the client is not supported
    /** @param client a \c RestClientIo::RestClientIo object, used as-is, or a synchronous
        \c RestClient::RestClient object, for which a \c RestClientIo::RestClientIo object is created with its
        constructor options and connection path
    */
    static *object getAsyncClient(object client) {
        if (RestIoSupport::isAsyncClient(client)) {
            return client;
        }
        if (!client.hasCallableMethod("getConstructorOptions") || !client.hasCallableMethod("getConnectionPath")) {
            return;
        }
        object rest = RestIoSupport::createClient(client.getConstructorOptions());
        rest.setConnectionPath(client.getConnectionPath());
        return rest;
    }

    #! Returns the origin (scheme, host, and port) of a URL, without user information
    static string getOrigin(string url) {
        *list<*string> parts = (url =~ x/^([a-zA-Z][a-zA-Z0-9+.-]*:\/\/)(?:[^@\/?#]*@)?([^\/?#]*)/s);
        return parts ? parts[0].lwr() + parts[1] : url;
    }

    #! Raises \c HTTP-CLIENT-RECEIVE-ERROR if a REST response has an error status
    /** @throw HTTP-CLIENT-RECEIVE-ERROR the response has an error status
    */
    static checkResponse(hash<auto> resp) {
        HttpIoSupport::checkResponse(resp + {"body": resp.response_body_raw ?? resp.body});
    }
}

#! A non-blocking REST request that follows and reports redirects
/** The first request is made with the REST client's request processing (authentication and token refresh, body
    serialization, and its connection path); redirects are followed with requests on the same connection manager,
    sending the headers of the first request to its origin only.  The output is the
    \c RestClient::RestResponseInfo hash of the final response with the \c effective_url and \c redirects keys.

    @since FileLocationHandler 3.0
*/
class RestIoRequestPollOperation inherits Qore::AbstractPollOperation<auto> {
    private {
        #! The client of the first request
        object rest;
        #! The current request operation
        object op;
        #! The origin of the current request
        string origin;
        #! The method of the current request
        string method;
        #! The path of the current request, once known
        *string path;
        #! Follows and records redirects
        *HttpIoRedirectState redirects;
        #! The maximum number of redirects to follow
        int max_redirects;
        #! If error responses are returned instead of raising an exception
        bool error_passthru;
        #! If redirects are returned instead of followed
        bool redirect_passthru;
        #! The output
        *hash<auto> output;
        #! True once aborted
        bool aborted;
        #! Serializes the caller and an async I/O controller worker
        Mutex m();
    }

    #! Creates the operation and prepares the first request
    /** @param rest the \c RestClientIo::RestClientIo client
        @param method the request method
        @param path the encoded request path
        @param body the request body, if any; serialized according to the client's data serialization setting
        @param hdr request headers, if any
        @param raw if @ref True, the body is sent as-is
        @param max_redirects the maximum number of redirects to follow
        @param error_passthru if @ref True, error responses are returned by the handler instead of raising an
        exception
        @param redirect_passthru if @ref True, redirects are returned instead of followed
    */
    constructor(object rest, string method, string path, auto body, *hash<auto> hdr, bool raw, int max_redirects,
            bool error_passthru = False, bool redirect_passthru = False) {
        self.rest = rest;
        self.method = method;
        self.max_redirects = max_redirects;
        self.error_passthru = error_passthru;
        self.redirect_passthru = redirect_passthru;
        origin = RestIoSupport::getOrigin(rest.getUrl());
        op = raw || !exists body
            ? rest.startPollRawRequest(method, path, body, hdr)
            : rest.startPollRequest(method, path, body, hdr);
    }

    #! Returns @ref True if error responses are returned instead of raising an exception
    bool getErrorPassthru() {
        return error_passthru;
    }

    #! Returns the I/O timeout of the client in milliseconds
    int getTimeout() {
        return rest.getTimeout();
    }

    #! Returns the goal
    string getGoal() {
        return sprintf("REST %s %s%s", method, origin, path ?? op.getPath());
    }

    #! Returns the state
    string getState() {
        AutoLock al(m);
        return exists output ? "complete" : (aborted ? "aborted" : op.getState());
    }

    #! Returns @ref True once the output is available
    bool goalReached() {
        AutoLock al(m);
        return exists output;
    }

    #! Returns the output, or @ref nothing if the request is not complete
    auto getOutput() {
        AutoLock al(m);
        return output;
    }

    #! Continues the request
    /** @return a poll hash while the request needs more I/O, @ref nothing once the output is available

        @throw HTTP-CLIENT-MAXIMUM-REDIRECTS-EXCEEDED the maximum number of redirects was exceeded
        @throw HTTP-CLIENT-REDIRECT-ERROR a redirect cannot be followed
        @throw HTTP-CLIENT-UNKNOWN-PROTOCOL a redirect location has an unsupported scheme

        @note Other errors of the client are raised as well
    */
    *hash<SocketPollInfo> continuePoll() {
        AutoLock al(m);
        while (!exists output && !aborted) {
            *hash<auto> resp;
            try {
                *hash<SocketPollInfo> info = op.continuePoll();
                if (info) {
                    return info;
                }
                if (!op.goalReached()) {
                    return;
                }
                resp = op.getOutput();
            } catch (hash<ExceptionInfo> ex) {
                # a client that does not pass errors through reports an error response with its response info
                if (ex.err != "REST-RESPONSE-ERROR" || ex.arg.typeCode() != NT_HASH || !ex.arg.status_code.intp()) {
                    aborted = True;
                    HttpIoSupport::rethrowError(ex);
                }
                resp = ex.arg;
            }
            string request_url = origin + (resp.request_path ?? op.getPath());
            if (!redirects) {
                # the redirect state is created with the headers of the first request as sent
                path = resp.request_path ?? op.getPath();
                redirects = new HttpIoRedirectState(origin, op.getRequestHeaders(), max_redirects,
                    redirect_passthru);
            }
            if (!redirects.isFollowed(resp.status_code)) {
                output = resp + {
                    "effective_url": request_url,
                    "redirects": redirects.getRedirects(),
                };
                return;
            }
            hash<auto> hdr = op.getRequestHeaders() ?? {};
            *data body = op.getRequestBody();
            hash<auto> next;
            try {
                next = redirects.follow(resp + {"headers": resp.headers ?? {}}, request_url, \method, \body, \hdr);
            } catch (hash<ExceptionInfo> ex) {
                aborted = True;
                rethrow;
            }
            origin = next.origin;
            path = next.path;
            # a redirect is sent with a client for its origin on the same connection manager; the request was
            # prepared already, so the token check is skipped
            object hop = RestIoSupport::createClient({
                "url": next.origin,
                "error_passthru": True,
                "allow_any_response": True,
            }, rest.getConnectionManager());
            op = hop.startPollRawRetry(method, next.path, body, hdr);
        }
    }

    #! Aborts the request
    abort() {
        AutoLock al(m);
        if (!exists output) {
            aborted = True;
            op.abort();
        }
    }

    #! Aborts the request if it was canceled or failed in an async I/O controller
    nothing onComplete(hash<SocketPollResultInfo> result) {
        if (result.canceled || result.ex) {
            abort();
        }
    }
}

#! A poller returning the response body of a @ref RestIoRequestPollOperation before deserialization
/** @since FileLocationHandler 3.0
*/
class RestIoBodyPollOperation inherits Qore::AbstractPollOperation<auto> {
    private {
        #! The request
        RestIoRequestPollOperation op;
    }

    #! Creates the object
    constructor(RestIoRequestPollOperation op) {
        self.op = op;
    }

    #! Returns the goal of the request
    string getGoal() {
        return op.getGoal();
    }

    #! Returns the state of the request
    string getState() {
        return op.getState();
    }

    #! Returns @ref True once the response has been received
    bool goalReached() {
        return op.goalReached();
    }

    #! Continues the request
    *hash<SocketPollInfo> continuePoll() {
        return op.continuePoll();
    }

    #! Returns the response body before deserialization, or @ref nothing if the request is not complete
    auto getOutput() {
        return op.getOutput().response_body_raw;
    }

    #! Aborts the request
    abort() {
        op.abort();
    }

    #! Forwards completion to the request
    nothing onComplete(hash<SocketPollResultInfo> result) {
        op.onComplete(result);
    }
}

#! Wraps an HTTP poller so that the response body is always returned as the output
public class HttpPollWrapper inherits AbstractPollOperation<auto> {
    private {
        #! The HTTP poller to execute
        AbstractPollOperation op;
    }

    #! Creates the object wrapping the HTTP poller passed as an argument
    constructor(AbstractPollOperation op) {
        self.op = op;
    }

    #! Calls the corresponding method in the wrapped poller and returns the return value
    string getGoal() {
        return op.getGoal();
    }

    #! Calls the corresponding method in the wrapped poller and returns the return value
    string getState() {
        return op.getState();
    }

    #! Calls the corresponding method in the wrapped poller and returns the return value
    bool goalReached() {
        return op.goalReached();
    }

    #! Calls the corresponding method in the wrapped poller and returns the return value
    *hash<SocketPollInfo> continuePoll() {
        return op.continuePoll();
    }

    #! Returns the value of the \c "response-body" key in the wrapped HTTP poller's output data
    auto getOutput() {
        return op.getOutput()."response-body";
    }

    #! Aborts the current operation
    abort() {
        op.abort();
    }
}
}
