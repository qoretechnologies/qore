# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FileLocationHandlerHttp.qc module for returning file data from a URL-like location string

/*  FileLocationHandlerHttp Copyright 2021 - 2024 Qore Technologies, s.r.o.

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
/** A handler normally builds its own client for each request from the location URL.  It can instead be bound to
    an already-authenticated client, in which case the location is a URI path on that client's target rather than
    a URL; this is how a connection provides a native file location handler without reimplementing the transport.

    @see @ref FileLocationHandler::FileLocationHandler::getHttpHandlerForClient() "FileLocationHandler::getHttpHandlerForClient()"
*/
class FileLocationHandlerHttp inherits AbstractFileLocationHandler {
    private {
        #! The already-authenticated client this handler is bound to, if any
        *object bound_client;
    }

    #! Creates a handler that builds a client for each request from the location URL
    constructor() {
    }

    #! Creates a handler bound to an already-authenticated client
    /** @param client the client to use for every request; must be a
        @ref Qore::HTTPClient "HTTPClient" or a subclass of it

        @note When bound, the location is a URI path on the client's target rather than a URL, and the handler
        neither configures nor closes the client
    */
    constructor(object client) {
        bound_client = client;
    }

    #! Returns @ref True if this handler is bound to an already-authenticated client
    bool isBound() {
        return exists bound_client;
    }

    public {
        #! Valid HTTP location options
        /** @see @ref Qore::HTTPClient::constructor() "HTTPClient::constructor()" for supported options
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
                "desc": "Either `1.0` or `1.1` for the claimed HTTP protocol version compliancy in outgoing message "
                    "headers",
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
    }

    #! Retrieves a text file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return the string of the file's contents, if it can be retrieved

        @note This method can throw any exception related to making an HTTP call
    */
    private string getTextFileImpl(string scheme, string location, *hash<auto> opts) {
        string path;
        # make sure we can accept any Content-Type
        if (!opts.headers.Accept) {
            opts.headers.Accept = "*/*";
        }
        HTTPClient hc = getHttpClient(scheme, location, opts, \path);
        return hc.send(NOTHING, opts.method ? opts.method.upr() : "GET", path, opts.headers).body ?? "";
    }

    #! Retrieves a binary file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return the binary data of the file's contents, if it can be retrieved

        @note This method can throw any exception related to making an HTTP call
    */
    private binary getBinaryFileImpl(string scheme, string location, *hash<auto> opts) {
        string path;
        HTTPClient hc = getHttpClient(scheme, location, opts, \path);
        *string body = hc.send(NOTHING, opts.method ? opts.method.upr() : "GET", path, opts.headers).body;
        return exists body ? body.toBinary() : binary();
    }

    #! Retrieves an I/O poller to retrieve a binary file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return an I/O poller to retrieve the file's data
    */
    private Qore::AbstractPollOperation getIoPollerForLocationImpl(string scheme, string location,
            *hash<auto> opts) {
        string path;
        object client = getHttpClient(scheme, location, opts, \path, False);
        return new HttpPollWrapper(client.startPollSendRecv(opts.method ? opts.method.upr() : "GET", path, NOTHING,
            opts.headers));
    }

    #! Returns a stream reader for the file's data at the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return a stream reader for the file's contents, if it can be retrieved

        @note This method can throw any exception related to making an HTTP call
    */
    private Qore::StreamReader getStreamReaderImpl(string scheme, string location, *hash<auto> opts) {
        string path;
        HTTPClient hc = getHttpClient(scheme, location, opts, \path);

        StreamPipe pipe(sync_close: False);

        # run the I/O operation in the background
        background sub () {
            try {
                OutputStream os = pipe.getOutputStream();
                hc.send(os, NOTHING, opts.method ? opts.method.upr() : "GET", path, opts.headers);
                os.close();
            } catch (hash<ExceptionInfo> ex) {
            }
        }();

        return new StreamReader(pipe.getInputStream(), opts.encoding);
    }

    #! Retrieves a binary file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return an input stream of the file's contents, if it can be retrieved

        @note This method can throw any exception related to making an HTTP call
    */
    private Qore::InputStream getBinaryStreamImpl(string scheme, string location, *hash<auto> opts) {
        string path;
        HTTPClient hc = getHttpClient(scheme, location, opts, \path);

        StreamPipe pipe(sync_close: False);

        # run the I/O operation in the background
        background sub () {
            try {
                OutputStream os = pipe.getOutputStream();
                hc.send(os, NOTHING, opts.method ? opts.method.upr() : "GET", path, opts.headers);
                os.close();
            } catch (hash<ExceptionInfo> ex) {
            }
        }();

        return pipe.getInputStream();
    }

    #! Writes data to a file at the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param contents the file's contents
        @param opts write options

        @note This method can throw any exception related to making an HTTP call
    */
    private writeFileImpl(string scheme, string location, data contents, *hash<auto> opts) {
        string path;
        HTTPClient hc = getHttpClient(scheme, location, opts, \path);
        hc.send(contents, opts.method ? opts.method.upr() : "POST", path, opts.headers);
    }

    #! Returns an output stream for writing data to the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts write options

        @return an output stream for writing to the file

        @note
        - OutputStream::close() must be called to close the stream after writing terminates in order to avoid a
          deadlock
        - The data written to the OutputStreamWrapper object is written in the background; to ensure that the data has
          been written to the target, call OutputStreamWrapper::waitForIo()
        - This method can throw any exception related to making an HTTP call
    */
    private OutputStreamWrapper getOutputStreamImpl(string scheme, string location, *hash<auto> opts) {
        string path;
        HTTPClient hc = getHttpClient(scheme, location, opts, \path);

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
            try {
                Qore::InputStream is = pipe.getInputStream();
                hc.sendChunked(new BinaryOutputStream(), is, opts.method ? opts.method.upr() : "POST", NOTHING, path,
                    opts.headers);
            } catch (hash<ExceptionInfo> ex) {
                # Record the error so OutputStreamWrapper::waitForIo()
                # can rethrow it to the caller — see FTP handler comment.
                rv.setIoError(ex);
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

    #! Returns an HTTP client for the given location
    /**
        @note This method can throw any exception related to making an HTTP connection
    */
    private HTTPClient getHttpClient(string scheme, string location, *hash<auto> opts, reference<string> path,
            bool do_connect = True) {
        if (bound_client) {
            # the client already targets the right host; the location is a URI path on it
            path = AbstractFileLocationHandler::getUriPath(location);
            return cast<HTTPClient>(bound_client);
        }

        location = scheme + "://" + location;

        hash<UrlInfo> url_info = parse_url(location);
        if (url_info.path) {
            path = url_info.path;
        }
        if (!exists path) {
            throw "LOCATION-ERROR", sprintf("HTTP location %y missing URI path", location);
        }
        HTTPClient h({
            "url": location,
        } + opts);
        if (do_connect) {
            h.connect();
        }
        return h;
    }
}
}
