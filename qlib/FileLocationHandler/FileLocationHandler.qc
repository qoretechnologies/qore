# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FileLocationHandler.qc module for returning file data from a URL-like location string

/*  FileLocationHandler Copyright 2021 - 2026 Qore Technologies, s.r.o.

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
#! File handler option info
public hashdecl FileHandlerOptionInfo {
    #! The option value type: \c "string", \c "bool", or \c "int"
    string type;

    #! Required flag
    bool required = False;

    #! The option description
    string desc;

    #! Default value, if any
    auto default_value;
}

#! File handler info
public hashdecl FileHandlerInfo {
    #! The schema handled
    string scheme;
    #! Read options, if any
    *hash<string, hash<FileHandlerOptionInfo>> read_options;
    #! Write options, if any
    *hash<string, hash<FileHandlerOptionInfo>> write_options;
}

#! Information about an HTTP redirect followed while retrieving a resource
/** @see @ref filelocationhandler_resources

    @since FileLocationHandler 3.0
*/
public hashdecl HttpFileRedirectInfo {
    #! The absolute URL of the request that received the redirect response
    string request_url;

    #! The HTTP status code of the redirect response
    int status_code;

    #! The \c Location header value exactly as received
    string location;

    #! The absolute URL that \c location was resolved to and that was requested next; it never has a fragment
    string target_url;

    #! The headers of the redirect response with lower-case names, each with the list of values received
    hash<string, list<string>> headers = {};
}

#! HTTP information about a retrieved resource
/** @see @ref filelocationhandler_resources

    @since FileLocationHandler 3.0
*/
public hashdecl HttpFileResourceInfo {
    #! The HTTP status code of the final response
    int status_code;

    #! The headers of the final response with lower-case names, each with the list of values received
    /** Repeated headers such as \c Set-Cookie keep each value separately.  The headers describe the response
        as sent: \c Content-Length and \c Content-Encoding describe the transferred representation, not
        @ref FileResourceResult::data "data", which has any content encoding decoded
    */
    hash<string, list<string>> headers = {};

    #! The redirects followed, from the first response to the last redirect followed
    list<hash<HttpFileRedirectInfo>> redirects = ();
}

#! Information about a retrieved resource
/** @see @ref filelocationhandler_resources

    @since FileLocationHandler 3.0
*/
public hashdecl FileResourceInfo {
    #! The location as requested, without any location options and without user information
    /** This is a location string as accepted by this module, not necessarily an absolute URI; for example, a
        local file can be requested with a relative path
    */
    string requested_location;

    #! The absolute URI that produced the returned data, without a fragment, or @ref nothing if not known
    /** For HTTP locations this is the URL of the final response after any redirects; for local files it is an
        absolute \c file:// URI.  Resolve relative references in the returned data against this URI (RFC 3986
        section 5.1.3).  Handlers that cannot observe where the data came from, such as custom handlers that
        only provide data, leave it unset rather than report the requested location
    */
    *string effective_location;

    #! The media type of the data declared by the transport, in lower case and without parameters, if any
    *string media_type;

    #! The character encoding of the data declared by the transport, if any
    /** For HTTP locations this is the \c charset parameter of the \c Content-Type header; the data is never
        converted, so consumers decide how to interpret it
    */
    *string charset;

    #! HTTP information about the retrieval; set only for HTTP-based locations
    *hash<HttpFileResourceInfo> http;

    #! Handler-specific information, if any
    /** Each handler documents the keys it sets; keys are namespaced by handler to avoid collisions
    */
    *hash<auto> extra;
}

#! A retrieved resource with its metadata
/** @see @ref filelocationhandler_resources

    @since FileLocationHandler 3.0
*/
public hashdecl FileResourceResult {
    #! The data retrieved, after any transfer or content decoding and before any character decoding
    /** Empty data is an empty binary value
    */
    binary data = binary();

    #! Information about the resource
    hash<FileResourceInfo> info;
}

#! The abstract file location handler class
public class AbstractFileLocationHandler {
    #! Retrieves a text file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme or options
        @param opts read options

        @return the string of the file's contents, if it can be retrieved
    */
    string getTextFile(string scheme, string location, *hash<auto> opts) {
        return getTextFileImpl(scheme, location, opts);
    }

    #! Retrieves a binary file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return the binary data of the file's contents, if it can be retrieved
    */
    binary getBinaryFile(string scheme, string location, *hash<auto> opts) {
        return getBinaryFileImpl(scheme, location, opts);
    }

    #! Retrieves an I/O poller to retrieve a binary file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return an I/O poller to retrieve the file's data
    */
    Qore::AbstractPollOperation getIoPollerForLocation(string scheme, string location, *hash<auto> opts) {
        return getIoPollerForLocationImpl(scheme, location, opts);
    }

    #! Retrieves a resource and its metadata from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme or options
        @param opts read options

        @return the resource's data and metadata; see @ref filelocationhandler_resources

        @since FileLocationHandler 3.0
    */
    hash<FileResourceResult> getResource(string scheme, string location, *hash<auto> opts) {
        return getResourceImpl(scheme, location, opts);
    }

    #! Returns an I/O poller to retrieve a resource and its metadata from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme or options
        @param opts read options

        @return an I/O poller whose output is the resource's data and metadata; see
        @ref filelocationhandler_resources

        @since FileLocationHandler 3.0
    */
    Qore::AbstractPollOperation<hash<FileResourceResult>> getResourcePoller(string scheme, string location,
            *hash<auto> opts) {
        return getResourcePollerImpl(scheme, location, opts);
    }

    #! Returns a string stream for the file's data at the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return a stream reader for the file's contents, if it can be retrieved

        @note The data is read in the background and added to the input stream backing the stream reader
    */
    Qore::StreamReader getStreamReader(string scheme, string location, *hash<auto> opts) {
        return getStreamReaderImpl(scheme, location, opts);
    }

    #! Retrieves a binary file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return an input stream of the file's contents, if it can be retrieved

        @note The data is read in the background and added to the input stream
    */
    Qore::InputStream getBinaryStream(string scheme, string location, *hash<auto> opts) {
        return getBinaryStreamImpl(scheme, location, opts);
    }

    #! Writes data to a file at the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param contents the file's contents
        @param opts write options
    */
    writeFile(string scheme, string location, data contents, *hash<auto> opts) {
        return writeFileImpl(scheme, location, contents, opts);
    }

    #! Returns an output stream for writing data to the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts write options

        @return an output stream for writing to the file

        @note
        - OutputStreamWrapper::close() must be called to close the stream after writing terminates in order to avoid a
          deadlock
        - The data written to the OutputStreamWrapper object may be written in the background; to ensure that the data
          has been written to the target, call OutputStreamWrapper::waitForIo()
    */
    OutputStreamWrapper getOutputStream(string scheme, string location, *hash<auto> opts) {
        return getOutputStreamImpl(scheme, location, opts);
    }

    #! Gets supported read options
    /** @return supported read options
    */
    hash<string, hash<FileHandlerOptionInfo>> getReadOptions() {
        return getReadOptionsImpl();
    }

    #! Gets supported write options
    /** @return supported write options
    */
    hash<string, hash<FileHandlerOptionInfo>> getWriteOptions() {
        return getWriteOptionsImpl();
    }

    #! Returns @ref True if the handler validates location options itself
    /** Handlers that return @ref True are responsible for validating location options themselves;
        @ref FileLocationHandler::FileLocationHandler::getInfo() "FileLocationHandler::getInfo()" will not reject
        unknown options before dispatching to the handler.

        This is required by handlers that cannot know the effective set of supported options until the location has
        been resolved; @ref FileLocationHandler::FileLocationHandlerConnection "FileLocationHandlerConnection" is the
        motivating case, as the options accepted depend on the connection named in the location string.

        Default value processing and required option checks are performed for all handlers regardless of this value.

        @return @ref True if the handler validates location options itself

        @since FileLocationHandler 3.0
    */
    bool selfValidatesOptions() {
        return selfValidatesOptionsImpl();
    }

    #! Returns @ref True if the handler is bound to an already-authenticated client
    /** A bound handler treats the location as a URI path on its client's target rather than as a URL, and therefore
        accepts only the options that still apply once there is no client to construct; see
        @ref FileLocationHandler::FileLocationHandler::getHttpHandlerForClient() "FileLocationHandler::getHttpHandlerForClient()".

        @return @ref True if the handler is bound to an already-authenticated client

        @since FileLocationHandler 3.0
    */
    bool isBound() {
        return isBoundImpl();
    }

    #! Returns a string tagged with the given encoding, if any
    private static string forceEncoding(string str, *string encoding) {
        if (encoding && encoding.lwr() != str.encoding().lwr()) {
            return force_encoding(str, encoding);
        }
        return str;
    }

    #! Returns a location as an absolute URI path
    /** Used by handlers bound to an already-authenticated client, where the location is a path on that client's
        target rather than a URL.  A leading slash is added if missing, since HTTP/2 and HTTP/3 require the
        \c :path pseudo-header to be absolute and a relative path would otherwise be rejected at the transport
        layer rather than reported here.

        @param location the location string

        @return the location as an absolute URI path

        @since FileLocationHandler 3.0
    */
    private static string getUriPath(string location) {
        return location =~ /^\// ? location : "/" + location;
    }

    #! Retrieves a resource and its metadata from the given location
    /** The default implementation reads the data with @ref getBinaryFileImpl() and reports only the requested
        location; handlers that can observe more override this method

        @param scheme the scheme being used
        @param location the location string without the scheme or options
        @param opts read options

        @return the resource's data and metadata

        @since FileLocationHandler 3.0
    */
    private hash<FileResourceResult> getResourceImpl(string scheme, string location, *hash<auto> opts) {
        return <FileResourceResult>{
            "data": getBinaryFileImpl(scheme, location, opts),
            "info": <FileResourceInfo>{
                "requested_location": AbstractFileLocationHandler::getRequestedLocation(scheme, location),
            },
        };
    }

    #! Returns an I/O poller to retrieve a resource and its metadata from the given location
    /** The default implementation wraps the poller returned by @ref getIoPollerForLocationImpl() and reports
        only the requested location; handlers that can observe more override this method

        @param scheme the scheme being used
        @param location the location string without the scheme or options
        @param opts read options

        @return an I/O poller whose output is the resource's data and metadata

        @since FileLocationHandler 3.0
    */
    private Qore::AbstractPollOperation<hash<FileResourceResult>> getResourcePollerImpl(string scheme,
            string location, *hash<auto> opts) {
        hash<FileResourceInfo> info = <FileResourceInfo>{
            "requested_location": AbstractFileLocationHandler::getRequestedLocation(scheme, location),
        };
        return new FileResourcePollOperation(getIoPollerForLocationImpl(scheme, location, opts),
            hash<FileResourceResult> sub (auto output) {
                return <FileResourceResult>{
                    "data": FileResourcePollOperation::getBinaryOutput(output),
                    "info": info,
                };
            });
    }

    #! Returns the requested location for a location string without its scheme
    /** @param scheme the scheme being used
        @param location the location string without the scheme or options

        @return the location with its scheme and without any user information

        @since FileLocationHandler 3.0
    */
    static string getRequestedLocation(string scheme, string location) {
        return AbstractFileLocationHandler::removeUserInfo(scheme + "://" + location);
    }

    #! Removes any user information (a user name and password) from a URL-like location string
    /** The user information is found as @ref Qore::parse_url() "parse_url()" and the HTTP client find it, so a
        password can contain unencoded \c "/", \c "?", \c "#", \c ":", and \c "@" characters.  If the location
        cannot be parsed, everything up to the last \c "@" is removed.

        @param location a location string

        @return the location string without user information

        @since FileLocationHandler 3.0
    */
    static string removeUserInfo(string location) {
        *list<*string> parts = (location =~ x/^([A-Za-z][A-Za-z0-9+.-]*:\/\/)(.*)$/s);
        if (!parts || parts[1] !~ /@/) {
            return location;
        }
        string rest = parts[1];
        *hash<UrlInfo> url;
        try {
            url = parse_url(location);
        } catch (hash<ExceptionInfo> ex) {
            # the location cannot be used; never report anything that could be user information
        }
        if (url) {
            if (!exists url.username && !exists url.password) {
                return location;
            }
            string user_info = (url.username ?? "") + (exists url.password ? ":" + url.password : "") + "@";
            if (rest.startsWith(user_info)) {
                return parts[0] + rest.substr(user_info.length());
            }
        }
        rest =~ s/^.*@//s;
        return parts[0] + rest;
    }

    #! Returns an absolute \c file:// URI for a local path
    /** Relative paths are resolved against the current working directory, and \c "." and \c ".." segments are
        removed; symbolic links are not resolved, so relative references resolved against the URI behave as
        they would for the path as given.  Each path segment is percent-encoded.

        @par Example:
        @code{.py}
string uri = AbstractFileLocationHandler::getFileUri("/var/data/my file#1.xml");
# uri: "file:///var/data/my%20file%231.xml"
        @endcode

        @param path the local path

        @return an absolute \c file:// URI for the path

        @since FileLocationHandler 3.0
    */
    static string getFileUri(string path) {
        string abs = normalize_dir(path);
        if (PlatformOS == "Windows") {
            abs =~ s/\\/\//g;
        }
        list<string> segments = ();
        foreach string segment in (abs.split("/")) {
            # keep a Windows drive letter as is
            segments += (!$# && segment =~ /^[A-Za-z]:$/) ? segment : encode_url(segment, True);
        }
        string rv = segments.join("/");
        return "file://" + (rv =~ /^\// ? "" : "/") + rv;
    }

    #! Returns the local path named by an absolute \c file:// URI
    /** This is the inverse of @ref getFileUri(): use it to read a resource whose URI was obtained by resolving a
        reference against a \c file:// @ref FileLocationHandler::FileResourceInfo::effective_location
        "effective location", as the \c file:// scheme of this module takes a literal path rather than a
        percent-encoded URI.

        The URI must have one of the forms \c file:/path, \c file:///path, or \c file://localhost/path (RFC 8089);
        the scheme and \c localhost are compared case-insensitively.  Any fragment is removed, and the path is then
        percent-decoded once (\c "+" is not a space).  On Windows, the slash before a drive letter is removed.

        @par Example:
        @code{.py}
string path = AbstractFileLocationHandler::getPathFromFileUri("file:///var/data/my%20file%231.xml#part");
# path: "/var/data/my file#1.xml"
        @endcode

        @param uri the absolute \c file:// URI

        @return the local path named by the URI

        @throw LOCATION-ERROR the URI is not an absolute \c file URI of a supported form: it has another scheme, an
        authority other than \c localhost, a query (even an empty one), a relative path, a malformed
        percent-encoded octet, or an encoded NUL character

        @since FileLocationHandler 3.0
    */
    static string getPathFromFileUri(string uri) {
        *list<*string> parts = (uri =~ x/^[Ff][Ii][Ll][Ee]:(\/\/([^\/?#]*))?([^?#]*)(\?[^#]*)?(#.*)?$/s);
        if (!parts) {
            throw "LOCATION-ERROR", sprintf("%y is not a file URI", uri);
        }
        if (exists parts[0] && parts[1].val() && parts[1].lwr() != "localhost") {
            throw "LOCATION-ERROR", sprintf("file URI %y names host %y; only local files can be read", uri,
                parts[1]);
        }
        if (exists parts[3]) {
            throw "LOCATION-ERROR", sprintf("file URI %y has a query, which cannot name a local file", uri);
        }
        string path = parts[2] ?? "";
        if (path !~ /^\//) {
            throw "LOCATION-ERROR", sprintf("file URI %y does not have an absolute path", uri);
        }
        if (path =~ /%(?![0-9A-Fa-f]{2})/) {
            throw "LOCATION-ERROR", sprintf("file URI %y has a malformed percent-encoded octet", uri);
        }
        if (path =~ /%00/) {
            throw "LOCATION-ERROR", sprintf("file URI %y has an encoded NUL character", uri);
        }
        path = decode_url(path);
        if (PlatformOS == "Windows" && path =~ /^\/[A-Za-z]:/) {
            path = path.substr(1);
        }
        return path;
    }

    #! Returns HTTP headers with lower-case names, each with the list of values received
    /** @param headers headers as returned by the HTTP client, with string values or lists of string values

        @return the headers with lower-case names; headers whose names differ only in case are merged in order

        @since FileLocationHandler 3.0
    */
    static hash<string, list<string>> getHeaderValues(*hash<auto> headers) {
        hash<string, list<string>> rv = {};
        foreach hash<auto> i in (headers.pairIterator()) {
            string name = i.key.lwr();
            list<string> values = i.value.typeCode() == NT_LIST
                ? (map $1.toString(), i.value)
                : (i.value.toString(),);
            if (rv{name}) {
                rv{name} += values;
            } else {
                rv{name} = values;
            }
        }
        return rv;
    }

    #! Sets the media type and character encoding of a resource from a \c Content-Type value
    /** @param info the resource information to update
        @param content_type the \c Content-Type value, if any

        @since FileLocationHandler 3.0
    */
    static setContentType(reference<hash<FileResourceInfo>> info, *string content_type) {
        if (!content_type.val()) {
            return;
        }
        list<string> params = content_type.split(";");
        string media_type = trim(params[0]).lwr();
        if (media_type.val()) {
            info.media_type = media_type;
        }
        for (int i = 1; i < params.size(); ++i) {
            *list<*string> kv = (params[i] =~ x/^\s*([^=\s]+)\s*=\s*(.*?)\s*$/s);
            if (!kv || kv[0].lwr() != "charset") {
                continue;
            }
            string charset = kv[1] ?? "";
            # a quoted-string value (RFC 9110 section 5.6.4)
            if (charset =~ /^".*"$/s) {
                charset = charset.substr(1, -1);
                charset =~ s/\\(.)/$1/g;
            }
            if (charset.val()) {
                info.charset = charset;
            }
            break;
        }
    }

    #! Retrieves a text file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return the string of the file's contents, if it can be retrieved
    */
    private abstract string getTextFileImpl(string scheme, string location, *hash<auto> opts);

    #! Retrieves a binary file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return the binary data of the file's contents, if it can be retrieved
    */
    private abstract binary getBinaryFileImpl(string scheme, string location, *hash<auto> opts);

    #! Retrieves an I/O poller to retrieve a binary file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return an I/O poller to retrieve the file's data
    */
    private abstract Qore::AbstractPollOperation getIoPollerForLocationImpl(string scheme, string location,
            *hash<auto> opts);

    #! Returns a string stream for the file's data at the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return a stream reader for the file's contents, if it can be retrieved
    */
    private abstract Qore::StreamReader getStreamReaderImpl(string scheme, string location, *hash<auto> opts);

    #! Retrieves a binary file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return an input stream of the file's contents, if it can be retrieved
    */
    private abstract Qore::InputStream getBinaryStreamImpl(string scheme, string location, *hash<auto> opts);

    #! Writes data to a file at the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param contents the file's contents
        @param opts write options
    */
    private abstract writeFileImpl(string scheme, string location, data contents, *hash<auto> opts);

    #! Returns an output stream for writing data to the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts write options

        @return an output stream for writing to the file

        @note
        - OutputStreamWrapper::close() must be called to close the stream after writing terminates in order to avoid a
          deadlock
        - The data written to the OutputStreamWrapper object may be written in the background; to ensure that the data
          has been written to the target, call OutputStreamWrapper::waitForIo()
    */
    private abstract OutputStreamWrapper getOutputStreamImpl(string scheme, string location, *hash<auto> opts);

    #! Gets supported read options
    /** @return supported read options
    */
    private abstract hash<string, hash<FileHandlerOptionInfo>> getReadOptionsImpl();

    #! Gets supported write options
    /** @return supported write options
    */
    private abstract hash<string, hash<FileHandlerOptionInfo>> getWriteOptionsImpl();

    #! Returns @ref True if the handler validates location options itself
    /** The default implementation returns @ref False; override in handlers that must validate options after
        resolving the location

        @return @ref False in the base class

        @since FileLocationHandler 3.0
    */
    private bool selfValidatesOptionsImpl() {
        return False;
    }

    #! Returns @ref True if the handler is bound to an already-authenticated client
    /** The default implementation returns @ref False; override in handlers that can be bound to a client

        @return @ref False in the base class

        @since FileLocationHandler 3.0
    */
    private bool isBoundImpl() {
        return False;
    }
}

#! The main class
public class FileLocationHandler {
    private {
        #! The location handler cache; scheme -> handler;
        /**
        */
        static hash<string, AbstractFileLocationHandler> cache;

        #! The location handler lock to ensure atomic operations
        static Mutex handler_lock();

        #! Flag for dynamic initialization
        static bool dynamic_init;

        #! The connection filter, if any; see setConnectionFilter()
        static *code<bool(string, bool)> connection_filter;

        #! Flag for connection filter initialization from the environment
        static bool connection_filter_init;
    }

    #! Initializes default handlers
    static init() {
        # register default handlers
        FileLocationHandler::registerHandler("conn", new FileLocationHandlerConnection());
        FileLocationHandler::registerHandler("connection", new FileLocationHandlerConnection());
        FileLocationHandler::registerHandler("data", new FileLocationHandlerData());
        FileLocationHandler::registerHandler("file", new FileLocationHandlerFile());
        FileLocationHandler::registerHandler("ftp", new FileLocationHandlerFtp());
        FileLocationHandler::registerHandler("ftps", new FileLocationHandlerFtp());
        FileLocationHandler::registerHandler("http", new FileLocationHandlerHttp());
        FileLocationHandler::registerHandler("https", new FileLocationHandlerHttp());
        FileLocationHandler::registerHandler("rest", new FileLocationHandlerRest());
        FileLocationHandler::registerHandler("rests", new FileLocationHandlerRest());
        FileLocationHandler::registerHandler("sftp", new FileLocationHandlerSftp());
    }

    #! Register a new location handler
    /** @param scheme the scheme for the location
        @param handler the handler for the location, must have the signature:
        @code{.py} data sub (string scheme, bool text, string location) {} @endcode

        @throw LOCATION-HANDLER-ERROR the given location handler scheme has already been registered
    */
    static registerHandler(string scheme, AbstractFileLocationHandler handler) {
        bool has_lock = handler_lock.lockOwner();
        if (!has_lock) {
            handler_lock.lock();
        }
        on_exit if (!has_lock) {
            handler_lock.unlock();
        }

        if (cache{scheme}) {
            throw "LOCATION-HANDLER-ERROR", sprintf("a location handler for scheme %y has already been registered",
                scheme);
        }

        cache{scheme} = handler;
    }

    #! Tries to register a new location handler; if the scheme is already registered, the method returns @ref False
    /** @param scheme the scheme for the location
        @param handler the handler for the location, must have the signature:

        @return @ref True if the handler was registered, @ref False if not (scheme already has a handler)

        @code{.py} data sub (string scheme, bool text, string location) {} @endcode
    */
    static bool tryRegisterHandler(string scheme, AbstractFileLocationHandler handler) {
        bool has_lock = handler_lock.lockOwner();
        if (!has_lock) {
            handler_lock.lock();
        }
        on_exit if (!has_lock) {
            handler_lock.unlock();
        }

        if (cache{scheme}) {
            return False;
        }

        cache{scheme} = handler;
        return True;
    }

    #! Retuns a list of supported schemes
    /** @return a list of supported schemes
    */
    static list<string> getSupportedSchemes() {
        FileLocationHandler::checkDynamicInit();
        return keys cache;
    }

    #! Returns True if the scheme is supported
    /** @param scheme the scheme to check

        @return True if the scheme is supported
    */
    static bool isSchemeSupported(string scheme) {
        FileLocationHandler::checkDynamicInit();
        return exists cache{scheme};
    }

    #! Returns string data for the given file location
    /** @param location the location of the file data

        @return the resource data as a string

        @throw LOCATION-ERROR the location string cannotbe parsed; unknown location scheme; invalid location option
    */
    static string getTextFileFromLocation(string location) {
        hash<InternalLocationInfo> info = FileLocationHandler::getInfo(location, True);
        return info.handler.getTextFile(info.scheme, info.location, info.opts);
    }

    #! Returns binary data for the given file location
    /** @param location the location of the file data

        @return the resource data as a binary value

        @throw LOCATION-ERROR the location string cannotbe parsed; unknown location scheme; invalid location option
    */
    static binary getBinaryFileFromLocation(string location) {
        hash<InternalLocationInfo> info = FileLocationHandler::getInfo(location, True);
        return info.handler.getBinaryFile(info.scheme, info.location, info.opts);
    }

    #! Returns the file data from the given location
    /** @param text a flag if the data should be returned as text or not
        @param location the location string

        @throw LOCATION-ERROR invalid location or empty location string
    */
    static data getFileFromLocation(bool text, string location) {
        return text
            ? FileLocationHandler::getTextFileFromLocation(location)
            : FileLocationHandler::getBinaryFileFromLocation(location);
    }

    #! Returns a non-blocking I/O poller object to retrieve a binary file from the given location
    /** @param location the location string

        @throw LOCATION-ERROR invalid location or empty location string
    */
    static Qore::AbstractPollOperation getIoPollerForLocation(string location) {
        hash<InternalLocationInfo> info = FileLocationHandler::getInfo(location, True);
        return info.handler.getIoPollerForLocation(info.scheme, info.location, info.opts);
    }

    #! Returns a resource and its metadata from the given location
    /** @par Example:
        @code{.py}
hash<FileResourceResult> res = FileLocationHandler::getResourceFromLocation("https://example.com/svc?wsdl");
# resolve relative references in the document against the URI the data actually came from
string base = res.info.effective_location ?? res.info.requested_location;
        @endcode

        @param location the location of the resource; options can be given in the location string as for the other
        methods in this class
        @param read_options read options for the location's handler; these are validated like options given in the
        location string and take precedence over them

        @return the resource's data and metadata; see @ref filelocationhandler_resources

        @throw LOCATION-ERROR the location string cannot be parsed; unknown location scheme; invalid location option

        @note This method can throw any exception raised by the location's handler when retrieving the data

        @since FileLocationHandler 3.0
    */
    static hash<FileResourceResult> getResourceFromLocation(string location, *hash<auto> read_options) {
        hash<InternalLocationInfo> info = FileLocationHandler::getInfo(location, True, read_options);
        hash<FileResourceResult> rv = info.handler.getResource(info.scheme, info.location, info.opts);
        # a location given without a scheme is reported as given
        if (!info.scheme_given) {
            rv.info.requested_location = info.location;
        }
        return rv;
    }

    #! Returns a non-blocking I/O poller to retrieve a resource and its metadata from the given location
    /** No I/O is performed until the poller is driven; see @ref filelocationhandler_nonblocking.  The poller's
        output is the same as the return value of @ref getResourceFromLocation(), and it raises the same errors.

        @par Example:
        @code{.py}
AbstractPollOperation<hash<FileResourceResult>> op =
    FileLocationHandler::getResourcePollerForLocation("https://example.com/svc?wsdl");
while (!op.goalReached()) {
    *hash<SocketPollInfo> info = op.continuePoll();
    if (!info) {
        break;
    }
    Socket::poll((info,), 30s);
}
hash<FileResourceResult> res = op.getOutput();
        @endcode

        @param location the location of the resource; options can be given in the location string as for the other
        methods in this class
        @param read_options read options for the location's handler; these are validated like options given in the
        location string and take precedence over them

        @return an I/O poller whose output is the resource's data and metadata

        @throw LOCATION-ERROR the location string cannot be parsed; unknown location scheme; invalid location option
        @throw UNIMPLEMENTED the location's handler does not support non-blocking reads

        @since FileLocationHandler 3.0
    */
    static Qore::AbstractPollOperation<hash<FileResourceResult>> getResourcePollerForLocation(string location,
            *hash<auto> read_options) {
        hash<InternalLocationInfo> info = FileLocationHandler::getInfo(location, True, read_options);
        AbstractPollOperation<hash<FileResourceResult>> op = info.handler.getResourcePoller(info.scheme,
            info.location, info.opts);
        if (info.scheme_given) {
            return op;
        }
        # a location given without a scheme is reported as given
        string requested_location = info.location;
        return new FileResourcePollOperation(op, hash<FileResourceResult> sub (auto output) {
            hash<FileResourceResult> rv = output;
            rv.info.requested_location = requested_location;
            return rv;
        });
    }

    #! Returns a string stream for the file's data at the given location
    /** @param location the location of the file data

        @return a stream reader for the file's data
    */
    static Qore::StreamReader getStreamReaderFromLocation(string location) {
        hash<InternalLocationInfo> info = FileLocationHandler::getInfo(location, True);
        return info.handler.getStreamReader(info.scheme, info.location, info.opts);
    }

    #! Returns an input stream for file data at the given location
    /** @param location the location of the file data

        @return an input stream for the file's data
    */
    static Qore::InputStream getBinaryStreamFromLocation(string location) {
        hash<InternalLocationInfo> info = FileLocationHandler::getInfo(location, True);
        return info.handler.getBinaryStream(info.scheme, info.location, info.opts);
    }

    #! Writes data to a file at the given location
    /** @param location the location of the file data
        @param contents the file contents to write to the location
    */
    static writeFileToLocation(string location, data contents) {
        hash<InternalLocationInfo> info = FileLocationHandler::getInfo(location, False);
        info.handler.writeFile(info.scheme, info.location, contents, info.opts);
    }

    #! Returns an output stream for writing data to the given location
    /** @param location the location of the file data

        @return an output stream for writing to the file
getHandlerNames
        - OutputStreamWrapper::close() must be called to close the stream after writing terminates in order to avoid a
          deadlock
        - The data written to the OutputStreamWrapper object may be written in the background; to ensure that the data has
          been written to the target, call OutputStreamWrapper::waitForIo()
    */
    static OutputStreamWrapper getOutputStreamForLocation(string location) {
        hash<InternalLocationInfo> info = FileLocationHandler::getInfo(location, False);
        return info.handler.getOutputStream(info.scheme, info.location, info.opts);
    }

    #! Returns a list of all handler names
    /** @return a list of all handler names

        @since v2.2.1
    */
    static list<string> getHandlerNames() {
        return keys cache;
    }

    #! Returns info about all cached handlers
    /** @param write if information for write handler options should be returned (default: return read handler
        options)

        @return a hash keyed by scheme with handler information for each scheme

        @since v2.2.1
    */
    static hash<string, hash<FileHandlerInfo>> getHandlers(*bool write) {
        return map {
            $1.key: <FileHandlerInfo>{
                "scheme": $1.key,
                "read_options": $1.value.getReadOptions(),
                "write_options": $1.value.getWriteOptions(),
            }
        }, cache.pairIterator();
    }

    #! Returns info about the location handler for he given scheme
    /** @param scheme the scheme to provide info for

        @throw SCHEME-ERROR unsupported or invalid scheme

        @since v2.2.1
    */
    static hash<FileHandlerInfo> getHandlerInfo(string scheme) {
        *AbstractFileLocationHandler handler = cache{scheme};
        if (!handler) {
            throw "SCHEME-ERROR", sprintf("unknown location scheme %y; known location schemes: %y", scheme,
                keys cache);
        }
        return <FileHandlerInfo>{
            "scheme": scheme,
            "read_options": handler.getReadOptions(),
            "write_options": handler.getWriteOptions(),
        };
    }

    #! Returns the location handler registered for the given scheme
    /** @param scheme the scheme to return the handler for

        @return the location handler registered for the given scheme

        @throw SCHEME-ERROR unsupported or invalid scheme

        @note Primarily for handlers that delegate to another scheme's handler; see
        @ref FileLocationHandler::FileLocationHandlerConnection "FileLocationHandlerConnection"

        @since FileLocationHandler 3.0
    */
    static AbstractFileLocationHandler getHandler(string scheme) {
        FileLocationHandler::checkDynamicInit();
        *AbstractFileLocationHandler handler = cache{scheme};
        if (!handler) {
            throw "SCHEME-ERROR", sprintf("unknown location scheme %y; known location schemes: %y", scheme,
                keys cache);
        }
        return handler;
    }

    #! Returns a file location handler bound to an already-authenticated HTTP client
    /** The returned handler treats a location as a URI path on the client's target rather than as a URL, and
        neither changes nor closes the client.  Intended for
        \c ConnectionProvider::AbstractConnection::getFileLocationHandler() implementations, so that a connection
        supplies a native file location handler without reimplementing the transport.

        Requests are made with the \c HttpClientIo module; see @ref filelocationhandler_http.

        @param client the client to bind: a @ref Qore::HTTPClient "HTTPClient" (or a subclass), whose configuration
        (URL and credentials, timeouts, protocol version, proxy, TLS settings, default headers, redirect and error
        handling) is used for requests made with a new \c HttpClientIo::HttpClientConnectionManager, or an
        \c HttpClientIo::HttpClientConnectionManager, which is used for requests with \a url
        @param url the target URL for a connection manager; required for a connection manager and ignored
        otherwise; any user information is sent with HTTP basic authentication
        @param opts request options for a connection manager: \c headers (default request headers),
        \c error_passthru, \c redirect_passthru, \c max_redirects, \c timeout (in milliseconds), and
        \c encoding_passthru

        @return a file location handler bound to \a client

        @throw LOCATION-ERROR \a client is not supported, no URL was given for a connection manager, or an unsupported
        request option was given

        @note The client is shared with everything else using the connection, so the handler sends any request
        headers per request rather than changing the client's own defaults

        @since FileLocationHandler 3.0
    */
    static AbstractFileLocationHandler getHttpHandlerForClient(object client, *string url, *hash<auto> opts) {
        return new FileLocationHandlerHttp(client, url, opts);
    }

    #! Returns a file location handler bound to an already-authenticated REST client
    /** As @ref getHttpHandlerForClient(), but driving the client's asynchronous request API, so that the client's
        authentication, token refresh, and connection pooling apply to every request.

        @param client the client to bind: a \c RestClientIo::RestClientIo object (or a subclass), or a synchronous
        \c RestClient::RestClient object, in which case requests are made with a \c RestClientIo::RestClientIo
        object created with its constructor options and connection path; the operations of a handler bound to
        another kind of object raise \c UNIMPLEMENTED

        @return a file location handler bound to \a client

        @since FileLocationHandler 3.0
    */
    static AbstractFileLocationHandler getRestHandlerForClient(object client) {
        return new FileLocationHandlerRest(client);
    }

    #! Validates and normalizes location options for the given handler
    /** Performs the unknown-option check (optionally), converts option values to their declared types, applies
        default values, and enforces required options.

        @param location the location string with any option string already removed; used in error messages only
        @param handler the handler whose option declarations will be used
        @param opts the options parsed from the location string, if any
        @param read @ref True for read options, @ref False for write options
        @param validate_unknown if @ref True then unknown options cause an exception to be thrown

        @return the normalized options

        @throw LOCATION-ERROR unsupported option (only if \a validate_unknown is @ref True); missing required option

        @since FileLocationHandler 3.0
    */
    static *hash<auto> checkOptions(string location, AbstractFileLocationHandler handler, *hash<auto> opts,
            bool read, bool validate_unknown = True) {
        *hash<string, hash<FileHandlerOptionInfo>> handler_opts = read
            ? handler.getReadOptions()
            : handler.getWriteOptions();

        # check for unknown options
        if (validate_unknown && opts && (*hash<auto> erropts = opts - (keys handler_opts))) {
            throw "LOCATION-ERROR", sprintf("file location %y has unsupported %s option(s): %y; supported "
                "options: %y", location, read ? "read" : "write", keys erropts, keys handler_opts);
        }

        # check for required and default option values; fix types
        foreach hash<auto> i in (handler_opts.pairIterator()) {
            if (exists opts{i.key}) {
                switch (i.value.type) {
                    case "int":
                        opts{i.key} = opts{i.key}.toInt();
                        break;
                    case "bool":
                        opts{i.key} = parse_boolean(opts{i.key});
                        break;
                }
            } else {
                if (exists i.value.default_value) {
                    opts{i.key} = i.value.default_value;
                } else if (i.value.required) {
                    throw "LOCATION-ERROR", sprintf("file location %y is missing required option %y", location,
                        i.key);
                }
            }
        }

        return opts;
    }

    #! Sets a filter determining which connections may be used in \c "conn://" locations
    /** @param filter a closure or call reference taking the connection name and a boolean write flag and returning
        @ref True if the connection may be used; pass @ref nothing to remove any filter

        Named-connection locations widen what a location string can reach; location strings are frequently derived
        from user or partner data.  Applications that accept untrusted location strings should set a filter here.

        If no filter is set, the \c QORE_FILE_LOCATION_CONNECTIONS environment variable is used instead; when set, it
        provides a list of allowed connection names separated by @ref Qore::PathSep "PathSep" characters.  If neither
        is set, all connections are allowed.

        @since FileLocationHandler 3.0
    */
    static setConnectionFilter(*code<bool(string, bool)> filter) {
        handler_lock.lock();
        on_exit handler_lock.unlock();

        connection_filter = filter;
        # a filter set in code supersedes the environment variable
        connection_filter_init = True;
    }

    #! Returns the current connection filter, if any
    /** @return the current connection filter, if any

        @since FileLocationHandler 3.0
    */
    static *code<bool(string, bool)> getConnectionFilter() {
        handler_lock.lock();
        on_exit handler_lock.unlock();

        FileLocationHandler::checkConnectionFilterInit();
        return connection_filter;
    }

    #! Returns @ref True if the given connection may be used in a file location
    /** @param name the connection name
        @param write @ref True for write access, @ref False for read access

        @return @ref True if the given connection may be used in a file location

        @since FileLocationHandler 3.0
    */
    static bool connectionAllowed(string name, bool write) {
        *code<bool(string, bool)> filter;
        {
            handler_lock.lock();
            on_exit handler_lock.unlock();

            FileLocationHandler::checkConnectionFilterInit();
            filter = connection_filter;
        }
        # NOTE: the filter is called outside the lock; it is user code and may do I/O or call back into this class
        return filter ? filter(name, write) : True;
    }

    #! Initializes the connection filter from the environment; must be called with the handler lock held
    private static checkConnectionFilterInit() {
        if (connection_filter_init) {
            return;
        }
        connection_filter_init = True;

        *string allowed = ENV.QORE_FILE_LOCATION_CONNECTIONS;
        if (!allowed.val()) {
            return;
        }

        hash<string, bool> allow_map = map {$1: True}, allowed.split(PathSep), $1.val();
        connection_filter = bool sub (string name, bool write) {
            return allow_map{name} ?? False;
        };
    }

    #! Returns info about the requested location
    /** @param location the location string
        @param read if the location is meant for reading
        @param extra_opts options that are merged with any options in the location string and take precedence
        over them

        @throw LOCATION-ERROR unsupported or invalid location
    */
    private static hash<InternalLocationInfo> getInfo(string location, bool read, *hash<auto> extra_opts) {
        if (!location.val()) {
            throw "LOCATION-ERROR", "invalid empty location string";
        }

        *string scheme = (location =~ x/^(\w+):\/\//)[0];
        hash<InternalLocationInfo> info = <InternalLocationInfo>{
            "scheme": scheme ?? "file",
        };
        FileLocationHandler::checkDynamicInit();
        *AbstractFileLocationHandler handler = cache{info.scheme};
        if (!handler) {
            throw "LOCATION-ERROR", sprintf("unknown location scheme %y in location %y; known location schemes: %y",
                info.scheme, location, keys cache);
        }
        info.handler = handler;

        # NOTE: the option string must be stripped from the location before the location is used in any error
        # message, so this cannot be folded into the checkOptions() call below; argument evaluation order would
        # capture the unstripped location
        info.opts = FileLocationHandler::getOptions(\location);
        # options given by the caller take precedence over options in the location string
        if (extra_opts) {
            info.opts += extra_opts;
        }
        info.scheme_given = exists scheme;
        # NOTE: handlers that report selfValidatesOptions() perform the unknown-option check themselves after
        # resolving the location; they cannot know the effective option set until then.  Default value and required
        # option processing is always performed
        info.opts = FileLocationHandler::checkOptions(location, handler, info.opts, read,
            !handler.selfValidatesOptions());

        # remove the scheme from the location
        if (exists scheme) {
            splice location, 0, info.scheme.length() + 3;
        }
        info.location = location;

        return info;
    }

    #! Returns a hash of options from the location string
    private static *hash<auto> getOptions(reference<string> location) {
        *string opt_str = (location =~ x/((?:{).*(?:}))$/)[0];
        if (!opt_str) {
            return;
        }
        # remove option string from location string on exit
        on_exit {
            splice location, -opt_str.size();
        }
        auto v = parse_to_qore_value(opt_str);
        if (v.typeCode() != NT_HASH) {
            if (!v.strp()) {
                throw "LOCATION-ERROR", sprintf("option value in %y cannot be parsed", location);
            }
            v = {v: True};
        }
        # strip types before returning
        return {} + v;
    }

    #! Checks for dynamic initialization
    private static checkDynamicInit() {
        if (dynamic_init) {
            return;
        }

        handler_lock.lock();
        on_exit handler_lock.unlock();

        # must check again inside the lock
        if (dynamic_init) {
            return;
        }

        dynamic_init = True;

        # try to register handlers from the environment
        if (*string handlers = ENV.QORE_FILE_LOCATION_HANDLERS) {
            foreach string handler in (handlers.split(PathSep)) {
                try {
                    load_module(handler);
                } catch (hash<ExceptionInfo> ex) {
                    stderr.printf("error loading FileLocationHandler module %y: %s\n", handler, get_exception_string(ex));
                }
            }
        }
    }
}

#! An I/O poller that returns a retrieved resource with its metadata
/** Wraps the I/O poller that performs the retrieval and converts its output into a
    @ref FileLocationHandler::FileResourceResult "FileResourceResult" once the retrieval is complete.

    The conversion is performed in @ref continuePoll(), so an error found in the output, such as an HTTP error
    response, is raised there, exactly as the blocking retrieval raises it; @ref goalReached() returns @ref True
    only once the result is available.  No blocking I/O is performed: the wrapped poller drives all I/O.

    @see @ref filelocationhandler_resources

    @since FileLocationHandler 3.0
*/
public class FileResourcePollOperation inherits Qore::AbstractPollOperation<hash<FileResourceResult>> {
    private {
        #! The poller performing the retrieval
        Qore::AbstractPollOperation op;

        #! Converts the output of the wrapped poller into the result
        code<hash<FileResourceResult>(auto)> convert;

        #! The result, once available
        *hash<FileResourceResult> result;

        #! Serializes access from the caller and from an async I/O controller worker
        Mutex m();
    }

    #! Creates the poller
    /** @param op the poller performing the retrieval
        @param convert converts the output of \a op into the result once \a op has reached its goal; any exception
        it raises is raised by @ref continuePoll()
    */
    constructor(Qore::AbstractPollOperation op, code<hash<FileResourceResult>(auto)> convert) {
        self.op = op;
        self.convert = convert;
    }

    #! Returns the goal of the wrapped poller
    string getGoal() {
        return op.getGoal();
    }

    #! Returns the state of the wrapped poller
    string getState() {
        return op.getState();
    }

    #! Returns @ref True once the result is available
    bool goalReached() {
        AutoLock al(m);
        return exists result;
    }

    #! Continues the retrieval
    /** @return a poll hash while the retrieval needs more I/O, @ref nothing once the result is available

        @throw FILE-RESOURCE-ERROR the wrapped poller returned an unsupported value

        @note Any exception raised by the retrieval or by the conversion of its output is raised here
    */
    *hash<SocketPollInfo> continuePoll() {
        AutoLock al(m);
        if (exists result) {
            return;
        }
        *hash<SocketPollInfo> rv = op.continuePoll();
        if (rv || !op.goalReached()) {
            return rv;
        }
        result = convert(op.getOutput());
    }

    #! Returns the result, or @ref nothing if the retrieval is not complete
    *hash<FileResourceResult> getOutput() {
        AutoLock al(m);
        return result;
    }

    #! Aborts the retrieval
    abort() {
        op.abort();
    }

    #! Forwards completion to the wrapped poller
    onComplete(hash<SocketPollResultInfo> result) {
        op.onComplete(result);
    }

    #! Returns the binary data from the output of a poller that returns file data
    /** @param output the output of a poller returned by
        @ref FileLocationHandler::AbstractFileLocationHandler::getIoPollerForLocation() "getIoPollerForLocation()"

        @return the data as a binary value; @ref nothing is returned as an empty binary value

        @throw FILE-RESOURCE-ERROR \a output is not a binary value, a string, or @ref nothing
    */
    static binary getBinaryOutput(auto output) {
        switch (output.typeCode()) {
            case NT_NOTHING:
                return binary();
            case NT_BINARY:
                return output;
            case NT_STRING:
                return output.toBinary();
        }
        throw "FILE-RESOURCE-ERROR", sprintf("the I/O poller returned a value of type %y; expecting \"binary\", "
            "\"string\", or nothing", output.fullType());
    }
}
}

namespace Priv {
hashdecl InternalLocationInfo {
    string scheme;
    string location;
    *hash<auto> opts;
    AbstractFileLocationHandler handler;
    #! True if the location was given with a scheme
    bool scheme_given;
}
}