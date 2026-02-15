# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FileLocationHandlerRest.qc module for returning file data from a URL-like location string

/*  FileLocationHandlerRest Copyright 2021 - 2024 Qore Technologies, s.r.o.

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
/** @note This class does not support writing to output streams
*/
class FileLocationHandlerRest inherits AbstractFileLocationHandler {
    public {
        #! Valid REST location options
        /** @see \c RestClient::RestClient::constructor() "RestClient::constructor()" for supported options
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
        string path;
        # make sure we can accept any Content-Type
        if (!opts.headers.Accept) {
            opts.headers.Accept = "*/*";
        }
        object hc = getRestClient(scheme, location, opts, \path);
        auto val = hc.doRequest(opts.method ? opts.method.upr() : "GET", path).body;
        if (!exists val) {
            return "";
        }
        if (val.typeCode() == NT_STRING) {
            return val;
        }
        if (!val.strp()) {
            throw "REST-DATA-ERROR", sprintf("%s://%s returned data of type %y; expecting \"string\"",
                scheme, location, val.fullType());
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
        string path;
        object client = getRestClient(scheme, location, opts, \path);
        auto val = client.doRequest(opts.method ? opts.method.upr() : "GET", path).body;
        if (!exists val) {
            return binary();
        }
        if (val.typeCode() == NT_BINARY) {
            return val;
        }
        if (!val.strp()) {
            throw "REST-DATA-ERROR", sprintf("%s://%s returned data of type %y; expecting \"binary\" or \"string\"",
                scheme, location, val.fullType());
        }
        return val.toString().toBinary();
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
        object client = getRestClient(scheme, location, opts, \path, False);
        return new HttpPollWrapper(client.startPollSendRecv(opts.method ? opts.method.upr() : "GET", path));
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
        return new BinaryInputStream(getBinaryFileImpl(scheme, location, opts));
    }

    #! Writes data to a file at the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param contents the file's contents
        @param opts write options

        @note This method can throw any exception related to making a REST call
    */
    private writeFileImpl(string scheme, string location, data contents, *hash<auto> opts) {
        string path;
        object client = getRestClient(scheme, location, opts, \path);
        client.doRequest(opts.method ? opts.method.upr() : "POST", path, contents);
    }

    #! This method only throws a \c STREAM-WRITE-ERROR exception
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts write options

        @return always throws an exception

        @throw STREAM-WRITE-ERROR rest(s):// locations do not support writing to output streams
    */
    private OutputStreamWrapper getOutputStreamImpl(string scheme, string location, *hash<auto> opts) {
        throw "STREAM-WRITE-ERROR", sprintf("%s:// locations do not support writing with an output stream; use "
            "http(s):// locations instead", scheme);
    }

    #! Gets supported read options
    /** @return supported read options
    */
    private hash<string, hash<FileHandlerOptionInfo>> getReadOptionsImpl() {
        return RestClientLocationOpts;
    }

    #! Gets supported write options
    /** @return supported write options
    */
    private hash<string, hash<FileHandlerOptionInfo>> getWriteOptionsImpl() {
        return RestClientLocationOpts;
    }

    #! Returns a RestClient object for the given location
    /**
        @note This method can throw any exception related to making a REST connection
    */
    private object getRestClient(string scheme, string location, *hash<auto> opts, reference<string> path,
            bool do_connect = True) {
        # dynamically load the module to avoid a circular module dependency
        load_module("RestClient");

        location = (scheme == "rests" ? "https" : "http") + "://" + location;

        hash<UrlInfo> url_info = parse_url(location);
        if (url_info.path) {
            path = url_info.path;
        }
        if (!exists path) {
            throw "LOCATION-ERROR", sprintf("REST location %y missing URI path", location);
        }
        location = url_info.protocol + "://";
        if (url_info.username.val() && url_info.password.val()) {
            location += sprintf("%s:%s@", url_info.username, url_info.password);
        }
        location += url_info.host;
        if (url_info.port) {
            location += ":" + url_info.port;
        }
        object client = create_object("RestClient", {
                "url": location,
            } + opts
        );
        if (do_connect) {
            client.connect();
        }
        return client;
    }
}

#! Wraps an HTTP poller so that the response body is always returned as the output
public class HttpPollWrapper inherits AbstractPollOperation {
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
