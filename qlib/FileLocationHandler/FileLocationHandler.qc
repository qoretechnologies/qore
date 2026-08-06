# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FileLocationHandler.qc module for returning file data from a URL-like location string

/*  FileLocationHandler Copyright 2021 - 2025 Qore Technologies, s.r.o.

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

    #! Returns a string tagged with the given encoding, if any
    private static string forceEncoding(string str, *string encoding) {
        if (encoding && encoding.lwr() != str.encoding().lwr()) {
            return force_encoding(str, encoding);
        }
        return str;
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

        @throw LOCATION-ERROR unsupported or invalid location
    */
    private static hash<InternalLocationInfo> getInfo(string location, bool read) {
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
}

namespace Priv {
hashdecl InternalLocationInfo {
    string scheme;
    string location;
    *hash<auto> opts;
    AbstractFileLocationHandler handler;
}
}