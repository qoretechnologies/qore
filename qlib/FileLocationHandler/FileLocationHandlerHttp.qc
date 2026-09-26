# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FileLocationHandlerHttp.qc module for returning file data from a URL-like location string

/*  FileLocationHandlerHttp Copyright 2021 - 2026 Qore Technologies, s.r.o.

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
#! The class for handling http(s):// locations
/** Requests are made with the \c HttpClientIo module, which supports HTTP/1.1, HTTP/2, and HTTP/3; see
    @ref filelocationhandler_http.

    A handler normally makes each request from the location URL.  It can instead be bound to an
    already-configured client, in which case the location is a URI path on that client's target rather than a URL;
    this is how a connection provides a native file location handler without reimplementing the transport.

    @see @ref FileLocationHandler::FileLocationHandler::getHttpHandlerForClient() "FileLocationHandler::getHttpHandlerForClient()"
*/
class FileLocationHandlerHttp inherits AbstractFileLocationHandler {
    private {
        #! The client this handler was bound to, if any
        /** A @ref Qore::HTTPClient "HTTPClient", whose configuration is used for requests, or an
            \c HttpClientIo::HttpClientConnectionManager used for requests
        */
        *object bound_client;

        #! The connection manager of a bound handler
        *object bound_mgr;

        #! The request settings of a bound handler
        *hash<HttpIoRequestInfo> bound_info;
    }

    #! Creates a handler that makes each request from the location URL
    constructor() {
    }

    #! Creates a handler bound to an already-configured client
    /** @param client the client to use for every request: a @ref Qore::HTTPClient "HTTPClient" (or a subclass),
        whose configuration is used to make requests with a new \c HttpClientIo::HttpClientConnectionManager, or an
        \c HttpClientIo::HttpClientConnectionManager, in which case \a url is required
        @param url the target URL for requests made with a connection manager; any user information is sent with
        HTTP basic authentication, and any path is ignored, as locations are URI paths
        @param opts request options for a connection manager: \c headers (default request headers),
        \c error_passthru, \c redirect_passthru, \c max_redirects, \c timeout (in milliseconds), and
        \c encoding_passthru

        @throw LOCATION-ERROR \a client is neither an \c HTTPClient nor a connection manager, or no URL was given
        for a connection manager

        @note When bound, the location is a URI path on the client's target rather than a URL, and the handler
        neither changes nor closes the client
    */
    constructor(object client, *string url, *hash<auto> opts) {
        bound_client = client;
        if (client instanceof HTTPClient) {
            HTTPClient hc = cast<HTTPClient>(client);
            hash<auto> config = hc.getHttpConfig();
            hash<auto> mgr_opts = config{"http_version", "ssl_verify_cert", "ssl_cert_data", "ssl_key_data",
                "ssl_key_password", "max_response_body_size"} + {
                "timeout": hc.getTimeout(),
                "connect_timeout": hc.getConnectTimeout(),
                "proxy": hc.getProxyURL(),
            };
            bound_mgr = HttpIoSupport::createManager(mgr_opts);
            bound_info = HttpIoSupport::getRequestInfo(config.url ?? hc.getURL(), <HttpIoRequestInfo>{
                "headers": config.headers ?? {},
                "error_passthru": hc.getErrorPassthru(),
                "redirect_passthru": hc.getRedirectPassthru(),
                "encoding_passthru": hc.getEncodingPassthru(),
                "assume_encoding": hc.getAssumedEncoding(),
                "max_redirects": hc.getMaxRedirects(),
                "timeout": hc.getTimeout(),
            }, config.username, config.password);
            return;
        }
        if (!url.val() || !HttpIoSupport::isConnectionManager(client)) {
            throw "LOCATION-ERROR", sprintf("cannot bind an HTTP location handler to an object of class %y; "
                "expecting an HTTPClient, or an HttpClientIo::HttpClientConnectionManager with a URL",
                client.className());
        }
        if (*hash<auto> unknown = opts - (keys BoundManagerOpts)) {
            throw "LOCATION-ERROR", sprintf("unsupported request option(s) %y; supported options: %y", keys unknown,
                keys BoundManagerOpts);
        }
        bound_mgr = client;
        hash<UrlInfo> url_info = parse_url(url);
        bound_info = HttpIoSupport::getRequestInfo(url, <HttpIoRequestInfo>{
            "headers": opts.headers ?? {},
            "error_passthru": opts.error_passthru.toBool(),
            "redirect_passthru": opts.redirect_passthru.toBool(),
            "encoding_passthru": opts.encoding_passthru.toBool(),
            "max_redirects": opts.max_redirects ?? HttpIoSupport::DefaultMaxRedirects,
            "timeout": opts.timeout ?? client.getRequestTimeout(),
        }, url_info.username, url_info.password);
    }

    #! Returns @ref True if this handler is bound to an already-configured client
    private bool isBoundImpl() {
        return exists bound_client;
    }

    public {
        #! Valid HTTP location options
        /** @see @ref filelocationhandler_http
        */
        const HttpClientLocationOpts = {
            "connect_timeout": <FileHandlerOptionInfo>{
                "type": "int",
                "desc": "connection timeout in milliseconds",
            },
            "encoding": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "the string encoding for any strings returned or sent",
            },
            "encoding_passthru": <FileHandlerOptionInfo>{
                "type": "bool",
                "desc": "do not decode known content-encodings but rather pass the body through as-is",
            },
            "error_passthru": <FileHandlerOptionInfo>{
                "type": "bool",
                "desc": "HTTP status codes indicating errors will not cause an `HTTP-CLIENT-RECEIVE-ERROR` exception "
                    "to be raised",
            },
            "http_version": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "The HTTP version to use: `auto` (the default), `1.0` or `1.1` (HTTP/1.1), `2.0`, or `3.0`",
            },
            "method": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "the HTTP method to use; if not given then `GET` is used for reads, and `POST` is used for "
                    "writes",
            },
            "proxy": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "The proxy URL for connecting through a proxy",
            },
            "redirect_passthru": <FileHandlerOptionInfo>{
                "type": "bool",
                "desc": "redirect responses will be passed to the caller instead of followed",
            },
            "ssl_cert_data": <FileHandlerOptionInfo>{
                "type": "data",
                "desc": "the X.509 certificate data in PEM format (string) or in DER format (binary); if this "
                    "option is set, then `ssl_cert_path` is ignored",
            },
            "ssl_cert_path": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "a path to an X.509 client certificate file in PEM format; if this option is used, then the "
                    "calling context must not be restricted with sandbox restriction `PO_NO_FILESYSTEM` which is "
                    "checked at runtime",
            },
            "ssl_key_data": <FileHandlerOptionInfo>{
                "type": "data",
                "desc": "the X.509 private key data in PEM format (string) or in DER format (binary); if this "
                    "option is set, then `ssl_key_path` is ignored",
            },
            "ssl_key_path": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "a path to a private key file in PEM format; if this option is used, then the "
                    "calling context must not be restricted with sandbox restriction `PO_NO_FILESYSTEM` which is "
                    "checked at runtime",
            },
            "ssl_key_password": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "the password to the private key given with `ssl_key_path`",
            },
            "ssl_verify_cert": <FileHandlerOptionInfo>{
                "type": "bool",
                "desc": "the server's certificate will only be accepted if it's verified",
            },
            "timeout": <FileHandlerOptionInfo>{
                "type": "int",
                "desc": "I/O timeout in milliseconds",
            },
        };

        #! Location options supported when the handler is bound to a client
        /** Every other option in @ref HttpClientLocationOpts configures the construction of a client, which a
            bound handler does not do; accepting one would silently ignore it
        */
        const BoundLocationOpts = {
            "encoding": HttpClientLocationOpts.encoding,
            "headers": <FileHandlerOptionInfo>{
                "type": "hash",
                "desc": "an optional hash of headers to send with the request",
            },
            "method": HttpClientLocationOpts.method,
        };

        #! Request options accepted when binding a handler to an \c HttpClientIo::HttpClientConnectionManager
        /** @since FileLocationHandler 3.0
        */
        const BoundManagerOpts = {
            "encoding_passthru": True,
            "error_passthru": True,
            "headers": True,
            "max_redirects": True,
            "redirect_passthru": True,
            "timeout": True,
        };
    }

    #! Retrieves a text file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return the string of the file's contents, if it can be retrieved

        @note This method can throw any exception related to making an HTTP call
    */
    private string getTextFileImpl(string scheme, string location, *hash<auto> opts) {
        hash<auto> resp = request(scheme, location, opts, "GET");
        return HttpIoSupport::getStringBody(resp, opts.encoding ?? HttpIoSupport::DefaultAssumedEncoding);
    }

    #! Retrieves a binary file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return the binary data of the file's contents, if it can be retrieved

        @note This method can throw any exception related to making an HTTP call
    */
    private binary getBinaryFileImpl(string scheme, string location, *hash<auto> opts) {
        # the data of a binary file are the octets received
        hash<auto> resp = request(scheme, location, opts, "GET", NOTHING, False);
        return FileResourcePollOperation::getBinaryOutput(resp.body);
    }

    #! Retrieves an I/O poller to retrieve a binary file from the given location
    /** The poller's output is the response body: the octets received, after any content encoding is decoded; a
        text body is not converted, so any byte order mark is kept.  Error responses are returned like any other
        response

        @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return an I/O poller to retrieve the file's data
    */
    private Qore::AbstractPollOperation getIoPollerForLocationImpl(string scheme, string location,
            *hash<auto> opts) {
        hash<HttpIoRequestInfo> info = getRequestInfo(scheme, location, opts, "GET");
        info.decode_text = False;
        return new HttpIoBodyPollOperation(new HttpIoRequestPollOperation(getManager(info), info));
    }

    #! Retrieves a resource and its metadata from the given location
    /** The data is the octets of the response body after any content encoding is decoded: a text body is not
        converted, so any byte order mark is kept, and its content type in the information tells how to decode it;
        the information includes the effective URL,
        the final response headers, and the redirects followed.  An HTTP error status raises the same
        \c HTTP-CLIENT-RECEIVE-ERROR exception as the other read methods, unless the \c error_passthru option (or
        the bound client's setting) is enabled.

        @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return the resource's data and metadata

        @note This method can throw any exception related to making an HTTP call

        @since FileLocationHandler 3.0
    */
    private hash<FileResourceResult> getResourceImpl(string scheme, string location, *hash<auto> opts) {
        hash<auto> resp = request(scheme, location, opts, getReadMethod(opts), NOTHING, False);
        return FileLocationHandlerHttp::getIoResourceResult(getResourceRequestedLocation(scheme, location), resp);
    }

    #! Retrieves an I/O poller to retrieve a resource and its metadata from the given location
    /** The poller follows redirects and reports the same result and errors as @ref getResourceImpl() without
        blocking; see @ref filelocationhandler_resources

        @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return an I/O poller whose output is the resource's data and metadata

        @since FileLocationHandler 3.0
    */
    private Qore::AbstractPollOperation<hash<FileResourceResult>> getResourcePollerImpl(string scheme,
            string location, *hash<auto> opts) {
        hash<HttpIoRequestInfo> info = getRequestInfo(scheme, location, opts, getReadMethod(opts));
        info.decode_text = False;
        *string requested_location = getResourceRequestedLocation(scheme, location);
        bool error_passthru = info.error_passthru;
        return new FileResourcePollOperation(new HttpIoRequestPollOperation(getManager(info), info),
            hash<FileResourceResult> sub (auto output) {
                if (!error_passthru) {
                    HttpIoSupport::checkResponse(output);
                }
                return FileLocationHandlerHttp::getIoResourceResult(requested_location, output);
            });
    }

    #! Returns a resource result from the response of a request made with \c HttpClientIo
    /** @param requested_location the requested location, or @ref nothing to report the URL of the first request
        made, as for a handler bound to a client, whose locations are paths that the client resolves
        @param resp the response: \c status_code, \c headers, \c headers_raw, \c body, \c effective_url, and
        \c redirects

        @return the resource result

        @since FileLocationHandler 3.0
    */
    static hash<FileResourceResult> getIoResourceResult(*string requested_location, hash<auto> resp) {
        hash<FileResourceInfo> rinfo = <FileResourceInfo>{
            # the request URLs never have user information
            "requested_location": requested_location ?? resp.redirects[0].request_url ?? resp.effective_url,
            "effective_location": resp.effective_url,
            "http": <HttpFileResourceInfo>{
                "status_code": resp.status_code,
                "headers": AbstractFileLocationHandler::getHeaderValues(resp.headers_raw ?? resp.headers),
                "redirects": FileLocationHandlerHttp::getRedirects(resp.redirects),
            },
        };
        AbstractFileLocationHandler::setContentType(\rinfo, rinfo.http.headers."content-type"[0]);
        return <FileResourceResult>{
            "data": FileResourcePollOperation::getBinaryOutput(resp.body),
            "info": rinfo,
        };
    }

    #! Returns the redirects followed from the redirect information of an HTTP request
    /** @param redirects the \c redirects key of an @ref HttpIoRequestPollOperation output hash

        @return the redirects followed with normalized headers

        @since FileLocationHandler 3.0
    */
    static list<hash<HttpFileRedirectInfo>> getRedirects(*list<auto> redirects) {
        list<hash<HttpFileRedirectInfo>> rv = ();
        foreach hash<auto> redirect in (redirects) {
            rv += <HttpFileRedirectInfo>{
                "request_url": redirect.request_url,
                "status_code": redirect.status_code,
                "location": redirect.location,
                "target_url": redirect.target_url,
                "headers": AbstractFileLocationHandler::getHeaderValues(redirect.headers),
            };
        }
        return rv;
    }

    #! Returns the requested location of a resource, or @ref nothing for a bound handler
    /** For a bound handler the location is a path that the client resolves against its own URL, so the requested
        location is the URL of the first request made; see @ref getIoResourceResult()
    */
    private *string getResourceRequestedLocation(string scheme, string location) {
        return bound_client ? NOTHING : AbstractFileLocationHandler::getRequestedLocation(scheme, location);
    }

    #! Returns the read method
    private static string getReadMethod(*hash<auto> opts) {
        return opts.method ? opts.method.upr() : "GET";
    }

    #! Returns a stream reader for the file's data at the given location
    /** The response headers are received before this method returns, so an error response or a failed
        connection is raised here; the body is read from the network as the stream is read.

        @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return a stream reader for the file's contents, if it can be retrieved

        @note This method can throw any exception related to making an HTTP call
    */
    private Qore::StreamReader getStreamReaderImpl(string scheme, string location, *hash<auto> opts) {
        hash<auto> response = getBinaryStreamIntern(scheme, location, opts);
        Qore::InputStream stream = response.stream;
        if (!response.decoded) {
            # the octets of a body that is not decoded are not text
            return new StreamReader(stream, opts.encoding);
        }

        # the encoding of the text is determined by the rules shared by all HTTP clients (see
        # get_http_body_encoding()) from the headers and the start of the body, with the encoding option as the
        # assumed encoding
        binary start;
        while (start.size() < HttpIoSupport::EncodingSniffSize) {
            *binary b = stream.read(HttpIoSupport::EncodingSniffSize - start.size());
            if (!b) {
                break;
            }
            start += b;
        }
        auto content_type = response.headers."content-type";
        if (content_type.typeCode() == NT_LIST) {
            content_type = content_type[0];
        }
        string assumed = opts.encoding ?? HttpIoSupport::DefaultAssumedEncoding;
        *string enc = get_http_body_encoding(content_type, start, assumed);
        if (enc && start) {
            # a byte order mark is not part of the text: the decoded start of the body is shorter by its size
            data decoded = decode_http_body(content_type, start, assumed);
            int bom_len = start.size() - decoded.size();
            if (bom_len) {
                start = start.substr(bom_len);
            }
        }
        if (start) {
            stream = new HttpIoPrefixInputStream(start, stream);
        }
        # a body that is not text is read in the encoding option, as before
        return new StreamReader(stream, enc ?? opts.encoding);
    }

    #! Retrieves a binary file from the given location
    /** The response headers are received before this method returns, so an error response or a failed
        connection is raised here; the body is read from the network as the stream is read, and any content
        encoding is decoded as it is read unless the \c encoding_passthru option is set.

        @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return an input stream of the file's contents, if it can be retrieved

        @note This method can throw any exception related to making an HTTP call
    */
    private Qore::InputStream getBinaryStreamImpl(string scheme, string location, *hash<auto> opts) {
        return getBinaryStreamIntern(scheme, location, opts).stream;
    }

    #! Retrieves a file from the given location as a stream
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return a hash with the following keys:
        - \c stream: an input stream of the file's contents
        - \c headers: the response headers
        - \c decoded: @ref True if the stream delivers the decoded body (not a body with a content encoding that
          is passed through)
    */
    private hash<auto> getBinaryStreamIntern(string scheme, string location, *hash<auto> opts) {
        hash<HttpIoRequestInfo> info = getRequestInfo(scheme, location, opts, "GET");
        object mgr = getManager(info);
        HttpIoRequestPollOperation op(mgr, info, True);
        hash<auto> resp;
        try {
            resp = HttpIoSupport::runSync(op, info.timeout);
            if (!info.error_passthru && (resp.status_code < 100 || resp.status_code >= 300)) {
                # the error body is read to be reported in the exception
                HttpIoInputStream error_stream(resp.stream, resp.initial_body, resp.complete, info.timeout);
                binary body;
                while (*binary b = error_stream.read(HttpIoSupport::MaxErrorBodySize - body.size())) {
                    body += b;
                    if (body.size() >= HttpIoSupport::MaxErrorBodySize) {
                        break;
                    }
                }
                HttpIoSupport::checkResponse(resp + {"body": body});
            }
        } catch (hash<ExceptionInfo> ex) {
            op.abort();
            if (!bound_mgr) {
                mgr.closeAll();
            }
            rethrow;
        }
        Qore::InputStream rv = new HttpIoInputStream(resp.stream, resp.initial_body, resp.complete, info.timeout,
            bound_mgr ? NOTHING : mgr);
        *string coding = HttpIoSupport::getContentCoding(resp.headers."content-encoding");
        if (coding && !info.encoding_passthru) {
            rv = new TransformInputStream(rv, get_decompressor(HttpIoSupport::CodingAlgorithms{coding}));
        }
        return {
            "stream": rv,
            "headers": resp.headers,
            "decoded": !coding || !info.encoding_passthru,
        };
    }

    #! Writes data to a file at the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param contents the file's contents
        @param opts write options

        @note This method can throw any exception related to making an HTTP call
    */
    private writeFileImpl(string scheme, string location, data contents, *hash<auto> opts) {
        if (opts.encoding && contents.typeCode() == NT_STRING) {
            contents = convert_encoding(contents, opts.encoding);
        }
        request(scheme, location, opts, "POST", contents);
    }

    #! Returns an output stream for writing data to the given location
    /** The data written is sent with chunked transfer encoding (HTTP/1.1) or as the request body stream (HTTP/2
        and HTTP/3) as it is written; the request is started when this method returns, and the response is
        received once the stream is closed

        @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts write options

        @return an output stream for writing to the file

        @note
        - OutputStream::close() must be called to close the stream after writing terminates in order to avoid a
          deadlock
        - The data written to the OutputStreamWrapper object is written in the background; to ensure that the data has
          been written to the target, call OutputStreamWrapper::waitForIo(), which also raises any error, including
          \c HTTP-CLIENT-RECEIVE-ERROR for an error response unless the \c error_passthru option is set
        - This method can throw any exception related to making an HTTP call
    */
    private OutputStreamWrapper getOutputStreamImpl(string scheme, string location, *hash<auto> opts) {
        hash<HttpIoRequestInfo> info = getRequestInfo(scheme, location, opts, "POST");
        object mgr = getManager(info);
        bool own_mgr = !bound_mgr;
        hash<auto> hdr = info.headers;
        if (!HttpIoSupport::hasHeader(hdr, "content-type")) {
            hdr."Content-Type" = "application/octet-stream";
        }
        object stream;
        try {
            stream = mgr.acquireAndStartStreaming(info.origin, info.method, info.path, hdr, True);
        } catch (hash<ExceptionInfo> ex) {
            if (own_mgr) {
                mgr.closeAll();
            }
            HttpIoSupport::rethrowError(ex);
        }

        StreamPipe pipe(sync_close: False);

        WaitGroup io_counter();
        io_counter.add();
        OutputStreamWrapper rv(pipe.getOutputStream(), io_counter);

        # run the I/O operation in the background
        background sub () {
            # on_exit outside try: done() must fire AFTER catch records
            # the error so waitForIo() observes the error before the
            # waitgroup drops to zero.
            on_exit io_counter.done();
            on_exit {
                if (own_mgr) {
                    mgr.closeAll();
                }
            }
            try {
                Qore::InputStream is = pipe.getInputStream();
                while (*binary chunk = is.read(HttpIoSupport::StreamChunkSize)) {
                    stream.sendData(chunk);
                }
                stream.sendData(binary(), True);
                # read the response
                HttpIoInputStream response_stream(stream, (), False, info.timeout);
                binary body;
                while (*binary b = response_stream.read(HttpIoSupport::MaxErrorBodySize)) {
                    if (body.size() < HttpIoSupport::MaxErrorBodySize) {
                        body += b;
                    }
                }
                *hash<auto> resp = stream.getResponse();
                if (!info.error_passthru) {
                    HttpIoSupport::checkResponse(resp + {"body": body});
                }
            } catch (hash<ExceptionInfo> ex) {
                try {
                    stream.cancel();
                } catch () {
                }
                # Record the error so OutputStreamWrapper::waitForIo()
                # can rethrow it to the caller — see FTP handler comment.
                try {
                    HttpIoSupport::rethrowError(ex);
                } catch (hash<ExceptionInfo> ex1) {
                    rv.setIoError(ex1);
                }
            }
        }();

        return rv;
    }

    #! Gets supported read options
    /** @return supported read options
    */
    private hash<string, hash<FileHandlerOptionInfo>> getReadOptionsImpl() {
        return bound_client ? BoundLocationOpts : HttpClientLocationOpts;
    }

    #! Gets supported write options
    /** @return supported write options
    */
    private hash<string, hash<FileHandlerOptionInfo>> getWriteOptionsImpl() {
        return bound_client ? BoundLocationOpts : HttpClientLocationOpts;
    }

    #! Makes a request and returns the response; the response status is checked unless errors are passed through
    /** @param decode_text if @ref False, the response body is the octets of the resource: binary data after its
        content encoding is decoded, including any byte order mark of text
    */
    private hash<auto> request(string scheme, string location, *hash<auto> opts, string default_method,
            *data body, bool decode_text = True) {
        hash<HttpIoRequestInfo> info = getRequestInfo(scheme, location, opts, default_method, body);
        info.decode_text = decode_text;
        object mgr = getManager(info);
        on_exit {
            if (!bound_mgr) {
                mgr.closeAll();
            }
        }
        hash<auto> resp = HttpIoSupport::runSync(new HttpIoRequestPollOperation(mgr, info), info.timeout);
        if (!info.error_passthru) {
            HttpIoSupport::checkResponse(resp);
        }
        return resp;
    }

    #! Returns the connection manager for a request: the bound manager or a new one for the request's options
    private object getManager(hash<HttpIoRequestInfo> info) {
        return bound_mgr ?? HttpIoSupport::createManager(info.mgr_opts);
    }

    #! Returns the request settings for the given location
    /**
        @throw LOCATION-ERROR the location has no host or no URI path
    */
    private hash<HttpIoRequestInfo> getRequestInfo(string scheme, string location, *hash<auto> opts,
            string default_method, *data body) {
        hash<HttpIoRequestInfo> info;
        if (bound_info) {
            # the client already targets the right host; the location is a URI path on it
            info = bound_info;
            info.path = HttpIoSupport::encodeRequestTarget(AbstractFileLocationHandler::getUriPath(location));
            if (opts.headers) {
                # per-request headers are sent without changing the client's defaults
                info.headers = HttpIoSupport::mergeHeaders(info.headers, opts.headers);
            }
        } else {
            string url = scheme + "://" + location;
            hash<UrlInfo> url_info = parse_url(url);
            if (!url_info.path) {
                throw "LOCATION-ERROR", sprintf("HTTP location %y missing URI path",
                    AbstractFileLocationHandler::removeUserInfo(url));
            }
            info = HttpIoSupport::getRequestInfo(url, <HttpIoRequestInfo>{
                "headers": {},
                "error_passthru": opts.error_passthru.toBool(),
                "redirect_passthru": opts.redirect_passthru.toBool(),
                "encoding_passthru": opts.encoding_passthru.toBool(),
                "max_redirects": HttpIoSupport::DefaultMaxRedirects,
                "timeout": opts.timeout ?? HttpIoSupport::DefaultTimeout,
                "mgr_opts": opts{"connect_timeout", "timeout", "http_version", "proxy", "ssl_verify_cert",
                    "ssl_cert_data", "ssl_cert_path", "ssl_key_data", "ssl_key_path", "ssl_key_password"},
            }, url_info.username, url_info.password);
            info.path = HttpIoSupport::encodeRequestTarget(info.path);
        }
        info.method = opts.method ? opts.method.upr() : default_method;
        # the encoding option is assumed for text response bodies whose encoding is not determined otherwise
        if (opts.encoding) {
            info.assume_encoding = opts.encoding;
        }
        if (exists body) {
            info.body = body;
            if (!HttpIoSupport::hasHeader(info.headers, "content-type")) {
                info.headers."Content-Type" = body.typeCode() == NT_STRING
                    ? sprintf("text/plain;charset=%s", body.encoding())
                    : "application/octet-stream";
            }
        }
        return info;
    }
}

#! The settings of a request made with \c HttpClientIo
/** @since FileLocationHandler 3.0
*/
hashdecl HttpIoRequestInfo {
    #! The origin (scheme, host, and port) of the target, without user information
    string origin;
    #! The request method
    string method = "GET";
    #! The encoded request path
    string path = "/";
    #! The request headers, including any \c Authorization header for basic authentication
    hash<auto> headers = {};
    #! The request body, if any
    *data body;
    #! If error responses are returned instead of raising an exception
    bool error_passthru = False;
    #! If redirect responses are returned instead of followed
    bool redirect_passthru = False;
    #! If content encodings are not decoded
    bool encoding_passthru = False;
    #! If a text response body is decoded to a string; if @ref False, the body is the octets of the resource
    /** @since FileLocationHandler 3.0
    */
    bool decode_text = True;
    #! The encoding assumed for text response bodies whose encoding is not determined otherwise
    string assume_encoding = "ISO-8859-1";
    #! The maximum number of redirects to follow
    int max_redirects = 5;
    #! The I/O timeout in milliseconds
    int timeout = 45000;
    #! Options for a connection manager created for the request
    *hash<auto> mgr_opts;
}

#! Supporting functions for HTTP requests made with \c HttpClientIo
/** This module cannot declare a dependency on \c HttpClientIo, which depends on this module through
    \c AsyncSocketIo and \c HttpServerUtil; \c HttpClientIo is loaded when an HTTP location is used, and its objects
    are used through the \c object type.

    @since FileLocationHandler 3.0
*/
class HttpIoSupport {
    public {
        #! The encoding assumed for text responses without a \c charset if no encoding is given
        /** ISO-8859-1, the default character set of text media types in HTTP/1.1 (RFC 2616), as assumed by
            @ref Qore::HTTPClient "HTTPClient"
        */
        const DefaultAssumedEncoding = "ISO-8859-1";

        #! The number of bytes examined at the start of a text body to determine its encoding
        /** The size of the WHATWG MIME Sniffing resource header; see @ref Qore::get_http_body_encoding()
            "get_http_body_encoding()"
        */
        const EncodingSniffSize = 1445;

        #! The module providing the HTTP transport
        const ModuleName = "HttpClientIo";

        #! The connection manager class
        const ManagerClass = "HttpClientIo::HttpClientConnectionManager";

        #! The connection manager options type
        const ManagerOptionsType = "HttpClientIo::HttpClientConnectionManagerOptions";

        #! Maps \c http_version location option values to connection manager protocol values
        const ProtocolMap = {
            "auto": "auto",
            "1.0": "h1",
            "1.1": "h1",
            "2.0": "h2",
            "3.0": "h3",
        };

        #! The default maximum number of redirects followed
        const DefaultMaxRedirects = 5;

        #! The default I/O timeout in milliseconds
        const DefaultTimeout = 45000;

        #! The poll timeout for event notifiers in milliseconds
        const PollTimeoutMs = 1000;

        #! The maximum size of an error response body read for an exception
        const MaxErrorBodySize = 64 * 1024;

        #! The size of chunks sent from an output stream
        const StreamChunkSize = 64 * 1024;

        #! The content encodings accepted in responses
        const AcceptEncoding = "gzip,deflate,br,zstd,bzip2";

        #! Maps content codings to decompression algorithms
        const CodingAlgorithms = {
            "gzip": COMPRESSION_ALG_GZIP,
            "deflate": COMPRESSION_ALG_ZLIB,
            "bzip2": COMPRESSION_ALG_BZIP2,
            "br": COMPRESSION_ALG_BROTLI,
            "zstd": COMPRESSION_ALG_ZSTD,
        };

        #! Redirect status codes that are followed
        const FollowedRedirects = {
            "301": True,
            "302": True,
            "303": True,
            "307": True,
            "308": True,
        };

        #! Headers sent only to the origin of the original request
        const OriginBoundHeaders = {
            "authorization": True,
            "cookie": True,
            "host": True,
        };

        #! Reason phrases for responses that do not carry one (HTTP/2 and HTTP/3)
        const ReasonPhrases = {
            "100": "Continue",
            "101": "Switching Protocols",
            "200": "OK",
            "201": "Created",
            "202": "Accepted",
            "203": "Non-Authoritative Information",
            "204": "No Content",
            "205": "Reset Content",
            "206": "Partial Content",
            "300": "Multiple Choices",
            "301": "Moved Permanently",
            "302": "Found",
            "303": "See Other",
            "304": "Not Modified",
            "307": "Temporary Redirect",
            "308": "Permanent Redirect",
            "400": "Bad Request",
            "401": "Unauthorized",
            "402": "Payment Required",
            "403": "Forbidden",
            "404": "Not Found",
            "405": "Method Not Allowed",
            "406": "Not Acceptable",
            "407": "Proxy Authentication Required",
            "408": "Request Timeout",
            "409": "Conflict",
            "410": "Gone",
            "411": "Length Required",
            "412": "Precondition Failed",
            "413": "Content Too Large",
            "414": "URI Too Long",
            "415": "Unsupported Media Type",
            "416": "Range Not Satisfiable",
            "417": "Expectation Failed",
            "421": "Misdirected Request",
            "422": "Unprocessable Content",
            "425": "Too Early",
            "426": "Upgrade Required",
            "428": "Precondition Required",
            "429": "Too Many Requests",
            "431": "Request Header Fields Too Large",
            "451": "Unavailable For Legal Reasons",
            "500": "Internal Server Error",
            "501": "Not Implemented",
            "502": "Bad Gateway",
            "503": "Service Unavailable",
            "504": "Gateway Timeout",
            "505": "HTTP Version Not Supported",
        };
    }

    #! Creates a connection manager from HTTP location options
    /** Redirects are always returned by the connection manager, so that they are followed and reported by
        @ref HttpIoRequestPollOperation, and cookies are not stored

        @param opts location options: \c connect_timeout, \c timeout, \c http_version, \c proxy,
        \c ssl_verify_cert, and the client certificate and key options

        @return an \c HttpClientIo::HttpClientConnectionManager object

        @throw LOCATION-ERROR invalid \c http_version value
    */
    static object createManager(*hash<auto> opts) {
        load_module(ModuleName);
        *string protocol = ProtocolMap{opts.http_version ?? "auto"};
        if (!protocol) {
            throw "LOCATION-ERROR", sprintf("invalid http_version %y; expecting one of: %y", opts.http_version,
                keys ProtocolMap);
        }
        hash<auto> mgr_opts = {
            "protocol": protocol,
            # like HTTPClient, server certificates are only verified if requested
            "ssl_verify_mode": opts.ssl_verify_cert ? SSL_VERIFY_PEER : SSL_VERIFY_NONE,
            "redirect_passthru": True,
            "enable_cookies": False,
        };
        if (opts.connect_timeout) {
            mgr_opts.connect_timeout = opts.connect_timeout;
        }
        if (opts.timeout) {
            mgr_opts.request_timeout = opts.timeout;
        }
        if (opts.proxy.val()) {
            mgr_opts.proxy_url = opts.proxy;
        }
        if (opts.max_response_body_size) {
            mgr_opts.max_response_body_size = opts.max_response_body_size;
        }
        *data cert = exists opts.ssl_cert_data
            ? opts.ssl_cert_data
            : (opts.ssl_cert_path.val() ? ReadOnlyFile::readTextFile(opts.ssl_cert_path) : NOTHING);
        if (exists cert) {
            mgr_opts.client_cert = new SSLCertificate(cert);
        }
        *data key = exists opts.ssl_key_data
            ? opts.ssl_key_data
            : (opts.ssl_key_path.val() ? ReadOnlyFile::readTextFile(opts.ssl_key_path) : NOTHING);
        if (exists key) {
            mgr_opts.client_pk = new SSLPrivateKey(key, opts.ssl_key_password);
        }
        TypedHash th(ManagerOptionsType);
        return create_object(ManagerClass, th.doCast(mgr_opts));
    }

    #! Returns @ref True if the object provides the API of an \c HttpClientIo::HttpClientConnectionManager
    static bool isConnectionManager(object obj) {
        return obj.hasCallableMethod("acquireConnectionAsync") && obj.hasCallableMethod("acquireStream")
            && obj.hasCallableMethod("acquireAndStartStreaming");
    }

    #! Returns request settings for the given URL
    /** @param url the target URL; the path is used as the request path
        @param info the settings to update
        @param username the username for HTTP basic authentication, if any
        @param password the password for HTTP basic authentication, if any

        @return the settings with the origin, path, and headers set

        @throw LOCATION-ERROR the URL has no host
    */
    static hash<HttpIoRequestInfo> getRequestInfo(string url, hash<HttpIoRequestInfo> info, *string username,
            *string password) {
        if (url !~ /^[a-zA-Z][a-zA-Z0-9+.-]*:\/\//) {
            url = "http://" + url;
        }
        *list<*string> parts = (url =~ x/^([a-zA-Z][a-zA-Z0-9+.-]*:\/\/)(?:[^@\/?#]*@)?([^\/?#]*)([^#]*)/s);
        if (!parts || !parts[1].val()) {
            throw "LOCATION-ERROR", sprintf("HTTP location %y has no host",
                AbstractFileLocationHandler::removeUserInfo(url));
        }
        info.origin = parts[0].lwr() + parts[1];
        info.path = parts[2].val() ? parts[2] : "/";
        if (info.path !~ /^\//) {
            info.path = "/" + info.path;
        }
        info.headers = mergeHeaders({
            "Accept": "*/*",
            "Accept-Encoding": AcceptEncoding,
        }, info.headers);
        if (username.val() && !hasHeader(info.headers, "authorization")) {
            info.headers.Authorization = "Basic " + make_base64_string(username + ":" + (password ?? ""));
        }
        return info;
    }

    #! Returns @ref True if the headers have a header with the given lower-case name
    static bool hasHeader(*hash<auto> hdr, string name) {
        foreach string key in (keys hdr) {
            if (key.lwr() == name) {
                return True;
            }
        }
        return False;
    }

    #! Merges headers case-insensitively; headers in \a hdr replace headers with the same name in \a base
    static hash<auto> mergeHeaders(*hash<auto> base, *hash<auto> hdr) {
        hash<auto> rv = base ?? {};
        foreach hash<auto> i in (hdr.pairIterator()) {
            rv -= (map $1, keys rv, $1.lwr() == i.key.lwr());
            rv{i.key} = i.value;
        }
        return rv;
    }

    #! Percent-encodes the characters that cannot be sent in a request target
    /** Percent-encoded octets and URI delimiters are kept exactly as given, so an encoded delimiter is never
        decoded
    */
    static string encodeRequestTarget(string target) {
        if (target !~ /[^\x21-\x7e]|["<>\\^`{|}]/) {
            return target;
        }
        binary octets = target.toBinary();
        string rv;
        for (int i = 0; i < octets.size(); ++i) {
            int c = octets[i];
            if (c <= 0x20 || c >= 0x7f || c == 0x22 || c == 0x3c || c == 0x3e || c == 0x5c || c == 0x5e
                    || c == 0x60 || c == 0x7b || c == 0x7c || c == 0x7d) {
                rv += sprintf("%%%02X", c);
            } else {
                rv += chr(c);
            }
        }
        return rv;
    }

    #! Returns the first content coding of a \c Content-Encoding value if the body is to be decoded
    static *string getContentCoding(auto content_encoding) {
        if (content_encoding.typeCode() == NT_LIST) {
            content_encoding = content_encoding[0];
        }
        if (content_encoding.typeCode() != NT_STRING) {
            return;
        }
        string coding = trim(content_encoding.split(",")[0]).lwr();
        coding =~ s/^x-//;
        return CodingAlgorithms{coding} ? coding : NOTHING;
    }

    #! Returns the reason phrase of a response
    static string getStatusMessage(hash<auto> resp) {
        return resp.status_message ?? ReasonPhrases{resp.status_code} ?? "<no message>";
    }

    #! Raises \c HTTP-CLIENT-RECEIVE-ERROR if the response has an error status, like @ref Qore::HTTPClient "HTTPClient"
    /** @param resp the response

        @throw HTTP-CLIENT-RECEIVE-ERROR the response has an error status; the argument has the response headers
        and the \c status_code, \c status_message, and \c body keys
    */
    static checkResponse(hash<auto> resp) {
        int code = resp.status_code;
        if (code >= 100 && code < 300) {
            return;
        }
        string msg = getStatusMessage(resp);
        throw "HTTP-CLIENT-RECEIVE-ERROR", sprintf("HTTP status code %d received: message: %s", code, msg),
            resp.headers + {
                "status_code": code,
                "status_message": msg,
                "body": resp.body,
            };
    }

    #! Rethrows an exception of the transport with the error code of @ref Qore::HTTPClient "HTTPClient"
    /** @throw SOCKET-TIMEOUT the request timed out
    */
    static rethrowError(hash<ExceptionInfo> ex) {
        if (ex.err == "HTTPCLIENT-TIMEOUT") {
            throw "SOCKET-TIMEOUT", ex.desc, ex.arg;
        }
        throw ex.err, ex.desc, ex.arg;
    }

    #! Returns the body of a response as a string
    /** A text body has already been decoded by the rules shared by all HTTP clients (see
        @ref Qore::decode_http_body() "decode_http_body()"); a body of a type that is not text is returned as a
        string in the assumed encoding

        @param resp the response
        @param assumed_encoding the encoding assumed for text whose encoding is not determined otherwise

        @return the body of the response as a string
    */
    static string getStringBody(hash<auto> resp, string assumed_encoding) {
        auto body = resp.body;
        switch (body.typeCode()) {
            case NT_NOTHING:
                return "";
            case NT_BINARY:
                return body.toString(assumed_encoding);
        }
        return body;
    }

    #! Drives a poll operation on the calling thread and returns its output
    /** @param op the poll operation
        @param timeout_ms the maximum time to wait for any I/O event

        @return the output of the poll operation

        @throw SOCKET-TIMEOUT no I/O event occurred within \a timeout_ms
    */
    static auto runSync(Qore::AbstractPollOperation op, int timeout_ms) {
        date last = now_us();
        while (!op.goalReached()) {
            *hash<SocketPollInfo> info = op.continuePoll();
            if (op.goalReached()) {
                break;
            }
            if (!info) {
                throw "HTTP-IO-ERROR", sprintf("the request %y ended without a response", op.getGoal());
            }
            int remaining = timeout_ms - (now_us() - last).durationMilliseconds();
            if (remaining <= 0) {
                op.abort();
                throw "SOCKET-TIMEOUT", sprintf("timeout waiting %d ms for a response to %s", timeout_ms,
                    op.getGoal());
            }
            if (Socket::poll((info,), remaining)) {
                last = now_us();
            }
        }
        return op.getOutput();
    }
}

#! Follows HTTP redirects like the \c HttpClientIo connection manager and records them
/** Redirect errors are raised with the error codes of @ref Qore::HTTPClient "HTTPClient"

    @since FileLocationHandler 3.0
*/
class HttpIoRedirectState {
    private {
        #! The origin of the original request
        string original_origin;
        #! Origin-bound headers of the original request
        hash<auto> origin_headers = {};
        #! The maximum number of redirects to follow
        int max_redirects;
        #! If redirects are returned instead of followed
        bool redirect_passthru;
        #! The redirects followed
        list<hash<auto>> redirects = ();
    }

    #! Creates the object
    /** @param origin the origin of the original request
        @param headers the headers of the original request
        @param max_redirects the maximum number of redirects to follow
        @param redirect_passthru if @ref True, redirects are returned instead of followed
    */
    constructor(string origin, *hash<auto> headers, int max_redirects, bool redirect_passthru) {
        original_origin = origin;
        origin_headers = map {$1.key: $1.value}, headers.pairIterator(),
            HttpIoSupport::OriginBoundHeaders{$1.key.lwr()};
        self.max_redirects = max_redirects;
        self.redirect_passthru = redirect_passthru;
    }

    #! Returns @ref True if a response with the given status code is a redirect to follow
    bool isFollowed(int status_code) {
        return !redirect_passthru && HttpIoSupport::FollowedRedirects{status_code};
    }

    #! Returns the redirects followed
    list<hash<auto>> getRedirects() {
        return redirects;
    }

    #! Follows a redirect
    /** @param resp the redirect response with the \c status_code and \c headers keys (lower-case header names),
        and optionally \c status_message and \c headers_raw
        @param request_url the URL of the request that received the response
        @param method the request method; updated for the next request
        @param body the request body; updated for the next request
        @param headers the request headers; updated for the next request

        @return a hash with the \c origin and \c path keys of the next request

        @throw HTTP-CLIENT-MAXIMUM-REDIRECTS-EXCEEDED the maximum number of redirects was exceeded
        @throw HTTP-CLIENT-REDIRECT-ERROR the redirect cannot be followed
        @throw HTTP-CLIENT-UNKNOWN-PROTOCOL the redirect location has an unsupported scheme
    */
    hash<auto> follow(hash<auto> resp, string request_url, reference<string> method, reference<*data> body,
            reference<hash<auto>> headers) {
        int code = resp.status_code;
        auto location = resp.headers.location;
        if (location.typeCode() == NT_LIST) {
            throw "HTTP-CLIENT-REDIRECT-ERROR", sprintf("redirect response %d from %y has %d Location headers; "
                "expecting one", code, request_url, location.size());
        }
        if (!location.val()) {
            throw "HTTP-CLIENT-REDIRECT-ERROR", sprintf("no redirect location given for status code %d from %y",
                code, request_url);
        }
        if (redirects.size() >= max_redirects) {
            throw "HTTP-CLIENT-MAXIMUM-REDIRECTS-EXCEEDED", sprintf("maximum redirects (%d) exceeded; redirect %d "
                "from %y to %y ignored", max_redirects, code, request_url, location);
        }

        # resolve the location against the URL of the request that received it (RFC 3986 section 5.2) and remove
        # any fragment, which is never sent
        string target = resolve_url(request_url, location);
        target =~ s/#.*//s;
        *list<*string> parts = (target =~ x/^([A-Za-z][A-Za-z0-9+.-]*):\/\/([^\/?#]*)(.*)$/s);
        if (!parts) {
            throw "HTTP-CLIENT-REDIRECT-ERROR", sprintf("redirect location %y from %y (code %d) has no host",
                location, request_url, code);
        }
        string scheme = parts[0].lwr();
        if (scheme != "http" && scheme != "https") {
            throw "HTTP-CLIENT-UNKNOWN-PROTOCOL", sprintf("redirect location %y from %y (code %d) has unsupported "
                "scheme %y", location, request_url, code, scheme);
        }
        # user information in a redirect location is never used as credentials
        string authority = parts[1];
        authority =~ s/^.*@//s;
        if (!authority.val() || authority =~ /[\x00-\x20\x7f"<>\\^`{|}]|%(?![0-9a-fA-F]{2})/) {
            throw "HTTP-CLIENT-REDIRECT-ERROR", sprintf("redirect location %y from %y (code %d) has an invalid "
                "host", location, request_url, code);
        }
        # a network server must never be able to send the request to a local UNIX domain socket
        if (authority =~ /^socket=/i && request_url !~ /^[a-z]+:\/\/socket=/i) {
            throw "HTTP-CLIENT-REDIRECT-ERROR", sprintf("redirect location %y from %y (code %d) names a UNIX "
                "domain socket; only a request on a UNIX domain socket can be redirected to one", location,
                request_url, code);
        }
        string next_origin = scheme + "://" + authority;
        string next_path = HttpIoSupport::encodeRequestTarget(parts[2] ?? "");
        if (next_path !~ /^\//) {
            next_path = "/" + next_path;
        }

        # RFC 9110 sections 15.4.2 - 15.4.4: 303 is retrieved with GET, and a POST redirected with 301 or 302 is
        # changed to GET; 307 and 308 repeat the request
        bool drop_body;
        if (code == 303) {
            if (method != "HEAD") {
                method = "GET";
            }
            drop_body = True;
        } else if ((code == 301 || code == 302) && method == "POST") {
            method = "GET";
            drop_body = True;
        }
        if (drop_body) {
            remove body;
            headers -= (map $1, keys headers, $1 =~ /^(content-|transfer-encoding$)/i);
        }

        # origin-bound headers are sent only to the origin of the original request
        headers -= (map $1, keys headers, HttpIoSupport::OriginBoundHeaders{$1.lwr()});
        if (next_origin.lwr() == original_origin.lwr()) {
            headers += origin_headers;
        }

        redirects += {
            "request_url": request_url,
            "status_code": code,
            "status_message": resp.status_message,
            "location": location,
            "target_url": next_origin + next_path,
            "headers": resp.headers_raw ?? resp.headers,
        };
        return {
            "origin": next_origin,
            "path": next_path,
        };
    }
}

#! A non-blocking HTTP request made with \c HttpClientIo; follows and reports redirects
/** The output is a hash with the \c status_code, \c status_message, \c http_version, \c protocol, \c headers
    (lower-case names), \c headers_raw, \c body, \c effective_url, and \c redirects keys; the body is decoded
    according to its content encoding unless the \c encoding_passthru option is set.  In stream mode the output is
    available once the final response headers have been received, and it has the \c stream, \c initial_body, and
    \c complete keys instead of \c body.

    No I/O is performed on the calling thread: connection readiness is waited for with the connection manager's
    event notifiers, and the response is read from the stream as it arrives.

    @since FileLocationHandler 3.0
*/
class HttpIoRequestPollOperation inherits Qore::AbstractPollOperation<auto> {
    public {
        #! Waiting for a connection
        const ST_CONNECTING = "connecting";
        #! Waiting for the response
        const ST_RECEIVING = "receiving";
        #! Complete
        const ST_COMPLETE = "complete";
        #! Aborted
        const ST_ABORTED = "aborted";
    }

    private {
        #! The connection manager
        object mgr;
        #! The request settings; updated for each redirect
        hash<HttpIoRequestInfo> info;
        #! If the output is available once the final response headers have been received
        bool stream_mode;

        #! Follows and records redirects
        HttpIoRedirectState redirects;

        #! The current state
        string state = ST_CONNECTING;
        #! The connection handed back by the last acquisition step
        *object tracked;
        #! Failed connection attempts
        int failures = 0;
        #! The connection readiness notifier
        *EventNotifier conn_notifier;
        #! True if @ref conn_notifier was signaled by this object
        bool self_signaled = False;
        #! The response notifier
        EventNotifier io_notifier();
        #! The current stream
        *object stream;
        #! The response headers of the current request
        *hash<auto> response;
        #! The body received for the current request
        list<binary> body_chunks = ();
        #! True if the current response is a redirect to follow
        bool redirecting = False;

        #! The output
        *hash<auto> output;

        #! Serializes the caller and an async I/O controller worker
        Mutex m();
    }

    #! Creates the operation
    /** @param mgr the \c HttpClientIo::HttpClientConnectionManager to use
        @param info the request settings
        @param stream_mode if @ref True, the output is available once the final response headers have been received
    */
    constructor(object mgr, hash<HttpIoRequestInfo> info, bool stream_mode = False) {
        self.mgr = mgr;
        self.info = info;
        self.stream_mode = stream_mode;
        redirects = new HttpIoRedirectState(info.origin, info.headers, info.max_redirects, info.redirect_passthru);
    }

    #! Returns the goal
    string getGoal() {
        return sprintf("HTTP %s %s%s", info.method, info.origin, info.path);
    }

    #! Returns the state
    string getState() {
        AutoLock al(m);
        return state;
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

        @throw SOCKET-TIMEOUT a request timed out
        @throw HTTP-CLIENT-MAXIMUM-REDIRECTS-EXCEEDED the maximum number of redirects was exceeded
        @throw HTTP-CLIENT-REDIRECT-ERROR a redirect cannot be followed
        @throw HTTP-CLIENT-UNKNOWN-PROTOCOL a redirect location has an unsupported scheme

        @note Other errors of the transport are raised as well, for example \c HTTPCLIENT-CONNECT-ERROR
    */
    *hash<SocketPollInfo> continuePoll() {
        AutoLock al(m);
        try {
            return continuePollIntern();
        } catch (hash<ExceptionInfo> ex) {
            cancelStream();
            state = ST_ABORTED;
            HttpIoSupport::rethrowError(ex);
        }
    }

    #! Aborts the request
    abort() {
        AutoLock al(m);
        cancelStream();
        if (!exists output) {
            state = ST_ABORTED;
        }
    }

    #! Aborts the request if it was canceled or failed in an async I/O controller
    nothing onComplete(hash<SocketPollResultInfo> result) {
        if (result.canceled || result.ex) {
            abort();
        }
    }

    #! Continues the request; called with the lock held
    private *hash<SocketPollInfo> continuePollIntern() {
        while (True) {
            switch (state) {
                case ST_COMPLETE:
                case ST_ABORTED:
                    return;

                case ST_CONNECTING: {
                    if (conn_notifier) {
                        # one acquisition step per readiness notification; see the HttpClientIo design
                        if (!self_signaled && connectionPending()) {
                            return getPollInfo(conn_notifier);
                        }
                        conn_notifier.acknowledge();
                        remove conn_notifier;
                    }
                    checkConnectionFailure();
                    self_signaled = False;
                    *EventNotifier notifier = mgr.acquireConnectionAsync(info.origin, \tracked);
                    if (!notifier && tracked && HttpIoRequestPollOperation::failed(tracked)
                        && !mgr.isUnresolvedRaceLeg(tracked)) {
                        # the connection closed synchronously; the next step counts the failure
                        notifier = new EventNotifier();
                        notifier.notify();
                    }
                    if (notifier) {
                        conn_notifier = notifier;
                        return getPollInfo(notifier);
                    }
                    try {
                        stream = mgr.acquireStream(info.origin);
                        stream.startStreaming(info.method, info.path, info.headers, False, info.body, io_notifier);
                    } catch (hash<ExceptionInfo> ex) {
                        if (ex.err == "HTTPCLIENT-NOT-READY") {
                            # the connection reported itself ready and refused the request; retry the acquisition
                            remove stream;
                            conn_notifier = new EventNotifier();
                            conn_notifier.notify();
                            self_signaled = True;
                            return getPollInfo(conn_notifier);
                        }
                        rethrow;
                    }
                    # a used connection that closes later is not a failed connection attempt
                    remove tracked;
                    state = ST_RECEIVING;
                    continue;
                }

                case ST_RECEIVING: {
                    io_notifier.acknowledge();
                    while (True) {
                        auto data = stream.readData(0);
                        if (!exists data) {
                            if (!response) {
                                response = stream.getResponse();
                            }
                            if (response && !processHeaders()) {
                                return;
                            }
                            if (stream.isDone()) {
                                # the end of the stream was delivered together with the last body chunk
                                if (processEnd()) {
                                    return;
                                }
                                break;
                            }
                            return getPollInfo(io_notifier);
                        }
                        bool end;
                        if (data.typeCode() == NT_HASH) {
                            if (data.error) {
                                throw data.error.err ?? "HTTP-IO-ERROR", data.error.desc ?? sprintf("%y", data.error),
                                    data.error;
                            }
                            if (!response && (data.status_code || data.headers)) {
                                response = data;
                            }
                            if (exists data.body) {
                                body_chunks += binary(data.body);
                            }
                            end = data.end_stream.toBool();
                        } else {
                            body_chunks += binary(data);
                        }
                        if (!response) {
                            response = stream.getResponse();
                        }
                        if (response && !processHeaders()) {
                            return;
                        }
                        if (end) {
                            if (processEnd()) {
                                return;
                            }
                            # a redirect was followed
                            break;
                        }
                    }
                    continue;
                }
            }
        }
    }

    #! Processes the response headers once; returns @ref False if the output is available
    private bool processHeaders() {
        if (redirecting || response.processed) {
            return True;
        }
        response.processed = True;
        if (redirects.isFollowed(response.status_code)) {
            redirecting = True;
            return True;
        }
        if (stream_mode) {
            output = getResponseOutput() + {
                "stream": stream,
                "initial_body": body_chunks,
                "complete": False,
            };
            state = ST_COMPLETE;
            return False;
        }
        return True;
    }

    #! Processes the end of a response; returns @ref True if the output is available, @ref False if a redirect is
    #! followed
    private bool processEnd() {
        if (redirecting) {
            followRedirect();
            return False;
        }
        hash<auto> rv = getResponseOutput();
        binary body;
        map body += $1, body_chunks;
        if (stream_mode) {
            # the whole response arrived with the headers
            rv += {
                "stream": stream,
                "initial_body": body_chunks,
                "complete": True,
            };
        } else if (body) {
            if (info.encoding_passthru) {
                rv.body = body;
            } else {
                hash<auto> decoded = call_static_method("HttpClientIo::HttpClientStreamHandle",
                    "normalizeResponseBody", {"headers": response.headers ?? {}, "body": body}, 0,
                    info.assume_encoding, info.decode_text);
                rv.body = decoded.body;
            }
        }
        output = rv;
        state = ST_COMPLETE;
        return True;
    }

    #! Returns the output without the body
    private hash<auto> getResponseOutput() {
        return {
            "status_code": response.status_code,
            "status_message": response.status_message,
            "http_version": response.http_version,
            "protocol": response.protocol,
            "headers": response.headers ?? {},
            "headers_raw": response.headers_raw,
            "effective_url": info.origin + info.path,
            "redirects": redirects.getRedirects(),
        };
    }

    #! Follows a redirect
    private followRedirect() {
        hash<auto> next = redirects.follow(response + {"headers": response.headers ?? {}}, info.origin + info.path,
            \info.method, \info.body, \info.headers);
        info.origin = next.origin;
        info.path = next.path;
        remove stream;
        remove response;
        body_chunks = ();
        redirecting = False;
        failures = 0;
        state = ST_CONNECTING;
    }

    #! Returns @ref True while the tracked connection is still connecting
    private bool connectionPending() {
        return exists tracked && !HttpIoRequestPollOperation::failed(tracked) && !tracked.isReady()
            && !mgr.isUnresolvedRaceLeg(tracked);
    }

    #! Counts the tracked connection as a failed attempt if it failed before it could be used
    /** @throw HTTPCLIENT-CONNECT-ERROR the connection attempts failed more often than the connection manager's
        \c max_request_retries option allows
    */
    private checkConnectionFailure() {
        *object conn = remove tracked;
        if (!conn || !HttpIoRequestPollOperation::failed(conn) || mgr.isUnresolvedRaceLeg(conn)) {
            return;
        }
        if (++failures <= mgr.getMaxRequestRetries()) {
            return;
        }
        *hash<auto> err = conn.getPollOperation().getError();
        throw "HTTPCLIENT-CONNECT-ERROR", sprintf("connection closed: %s: %s", err.err ?? "UNKNOWN",
            err.desc ?? "unknown"), err;
    }

    #! Returns @ref True if a pending connection failed
    private static bool failed(object conn) {
        return conn.isClosed() || conn.getPollOperation().hasError();
    }

    #! Cancels the current stream, unless it was handed over in the output
    private cancelStream() {
        if (stream && !exists output) {
            try {
                stream.cancel();
            } catch () {
            }
        }
    }

    #! Returns poll information for the given notifier
    private static hash<SocketPollInfo> getPollInfo(EventNotifier notifier) {
        return <SocketPollInfo>{
            "events": SOCK_POLLIN,
            "socket": notifier,
            "poll_timeout_ms": HttpIoSupport::PollTimeoutMs,
        };
    }
}

#! A poller returning the body of the response of an @ref HttpIoRequestPollOperation
/** @since FileLocationHandler 3.0
*/
class HttpIoBodyPollOperation inherits Qore::AbstractPollOperation<auto> {
    private {
        #! The request
        HttpIoRequestPollOperation op;
    }

    #! Creates the object
    constructor(HttpIoRequestPollOperation op) {
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

    #! Returns the response body, or @ref nothing if the request is not complete
    auto getOutput() {
        return op.getOutput().body;
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

#! An input stream reading the response body from an \c HttpClientIo stream
/** @since FileLocationHandler 3.0
*/
#! An input stream that returns the given data before the data of another stream
/** Used to read the start of a body to determine its character encoding before the body is read as text
*/
class HttpIoPrefixInputStream inherits Qore::InputStream {
    private {
        #! The data returned first
        binary prefix;
        #! The stream read after the prefix
        Qore::InputStream stream;
    }

    #! Creates the object
    /** @param prefix the data returned first
        @param stream the stream read after the prefix
    */
    constructor(binary prefix, Qore::InputStream stream) {
        self.prefix = prefix;
        self.stream = stream;
    }

    #! Reads data from the stream
    /** @param limit the maximum number of bytes to read

        @return the data read, or @ref nothing at the end of the stream
    */
    *binary read(int limit) {
        if (!prefix) {
            return stream.read(limit);
        }
        binary b;
        if (limit > 0 && prefix.size() > limit) {
            b = prefix.substr(0, limit);
            prefix = prefix.substr(limit);
        } else {
            b = prefix;
            prefix = binary();
        }
        return b;
    }

    #! Returns the next byte without reading it, or -1 at the end of the stream
    int peek() {
        return prefix ? prefix[0] : stream.peek();
    }
}

class HttpIoInputStream inherits Qore::InputStream {
    private {
        #! The stream
        object stream;
        #! Data received and not yet read
        list<binary> pending = ();
        #! True once the end of the stream has been received
        bool done;
        #! The I/O timeout in milliseconds
        int timeout_ms;
        #! A connection manager owned by the stream and closed at the end of the stream
        *object mgr;
    }

    #! Creates the object
    /** @param stream the \c HttpClientIo::HttpClientStreamHandle
        @param initial_body data already received
        @param done @ref True if the end of the stream has already been received
        @param timeout_ms the I/O timeout in milliseconds
        @param mgr a connection manager owned by the stream, closed at the end of the stream
    */
    constructor(object stream, list<binary> initial_body, bool done, int timeout_ms, *object mgr) {
        self.stream = stream;
        pending = select initial_body, $1.size();
        self.done = done;
        self.timeout_ms = timeout_ms;
        self.mgr = mgr;
        if (done) {
            release();
        }
    }

    #! Cancels the request if the stream was not read to the end
    destructor() {
        if (!done) {
            try {
                stream.cancel();
            } catch () {
            }
            release();
        }
    }

    #! Reads data from the stream
    /** @param limit the maximum number of bytes to read

        @return the data read, or @ref nothing at the end of the stream

        @throw SOCKET-TIMEOUT no data was received within the I/O timeout
    */
    *binary read(int limit) {
        while (!pending && !done) {
            fill();
        }
        if (!pending) {
            return;
        }
        binary b = shift pending;
        if (limit > 0 && b.size() > limit) {
            unshift pending, b.substr(limit);
            b = b.substr(0, limit);
        }
        return b;
    }

    #! Returns the next byte without reading it, or -1 at the end of the stream
    /** @throw SOCKET-TIMEOUT no data was received within the I/O timeout
    */
    int peek() {
        while (!pending && !done) {
            fill();
        }
        return pending ? pending[0][0] : -1;
    }

    #! Receives the next data from the stream
    private fill() {
        auto data;
        try {
            data = stream.readData(0);
            if (!exists data) {
                if (stream.isDone()) {
                    finish();
                    return;
                }
                data = stream.readData(timeout_ms);
                if (!exists data) {
                    if (stream.isDone()) {
                        finish();
                        return;
                    }
                    throw "SOCKET-TIMEOUT", sprintf("timeout waiting %d ms for response data", timeout_ms);
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            finish();
            HttpIoSupport::rethrowError(ex);
        }
        if (data.typeCode() == NT_HASH) {
            if (data.error) {
                finish();
                throw data.error.err ?? "HTTP-IO-ERROR", data.error.desc ?? sprintf("%y", data.error), data.error;
            }
            if (exists data.body && data.body.size()) {
                pending += binary(data.body);
            }
            if (data.end_stream) {
                finish();
            }
            return;
        }
        if (data.size()) {
            pending += binary(data);
        }
    }

    #! Marks the end of the stream
    private finish() {
        done = True;
        release();
    }

    #! Closes an owned connection manager
    private release() {
        if (mgr) {
            try {
                mgr.closeAll();
            } catch () {
            }
            remove mgr;
        }
    }
}
}
