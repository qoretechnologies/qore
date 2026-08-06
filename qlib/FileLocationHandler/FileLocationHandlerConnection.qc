# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FileLocationHandlerConnection.qc module for returning file data from a named connection and a path

/*  FileLocationHandlerConnection Copyright 2021 - 2026 Qore Technologies, s.r.o.

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
#! The class for handling \c "conn://" and \c "connection://" locations
/** Locations have the form:
    @verbatim
conn://<connection-name>/<path>[{<options>}]
    @endverbatim

    The path is always relative to the connection's own base location; the connection supplies the host,
    the credentials, and the root, and the location string supplies only the leaf.  A leading slash in the path
    does not make it absolute, and \c ".." segments that would escape the connection's base are rejected.

    Connections are resolved through the \c ConnectionProvider module, which is loaded dynamically on first use;
    this module does not declare a dependency on it.

    Resolution proceeds in tiers:
    - the connection returns a \c ConnectionProvider::FileLocationRedirect hash naming an existing scheme, and
      dispatch re-enters that scheme's handler; this covers the local filesystem, FTP, SFTP, and plain HTTP,
      reusing those handlers' blocking, streaming, and polling implementations unchanged
    - the connection provides a native handler object through
      \c ConnectionProvider::AbstractConnection::getFileLocationHandler()

    @note Symbols in the \c ConnectionProvider module are referenced above as plain text rather than with
    <tt>\@ref</tt>: this module deliberately does not depend on \c ConnectionProvider (see the class notes on
    dynamic loading), so its documentation tag file is not available when these docs are generated and the
    references would not resolve

    @note Location strings are frequently derived from user or partner data, and a named connection can reach any
    configured system.  Use
    @ref FileLocationHandler::FileLocationHandler::setConnectionFilter() "FileLocationHandler::setConnectionFilter()"
    or the \c QORE_FILE_LOCATION_CONNECTIONS environment variable to restrict which connections are reachable.

    @since FileLocationHandler 3.0
*/
class FileLocationHandlerConnection inherits AbstractFileLocationHandler {
    public {
        #! Valid connection location options
        /** Options not declared here are passed through to the resolved target and validated against it
        */
        const ConnectionLocationOpts = {
            "container": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "the container (bucket, drive, or parent folder) holding the file, for connections that "
                    "require one",
            },
            "conn_rtopts": <FileHandlerOptionInfo>{
                "type": "hash",
                "desc": "runtime options passed to the connection when acquiring it",
            },
            "encoding": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "the encoding of the file's data (read / write)",
            },
            "follow_url": <FileHandlerOptionInfo>{
                "type": "bool",
                "default_value": True,
                "desc": "for connections whose read operation returns a URL rather than data, retrieve the URL "
                    "returned",
            },
            "path_is_id": <FileHandlerOptionInfo>{
                "type": "bool",
                "default_value": False,
                "desc": "the path is an opaque object ID rather than a hierarchical path",
            },
            "subtype": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "the data provider subtype to use, for connections providing more than one",
            },
        };

        #! The module providing connection resolution
        const ConnectionProviderModule = "ConnectionProvider";
    }

    #! Retrieves a text file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme or options
        @param opts read options

        @return the string of the file's contents, if it can be retrieved
    */
    private string getTextFileImpl(string scheme, string location, *hash<auto> opts) {
        hash<ResolvedLocation> r = resolve(scheme, location, opts, False);
        return r.handler.getTextFile(r.scheme, r.location, r.opts);
    }

    #! Retrieves a binary file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return the binary data of the file's contents, if it can be retrieved
    */
    private binary getBinaryFileImpl(string scheme, string location, *hash<auto> opts) {
        hash<ResolvedLocation> r = resolve(scheme, location, opts, False);
        return r.handler.getBinaryFile(r.scheme, r.location, r.opts);
    }

    #! Retrieves an I/O poller to retrieve a binary file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return an I/O poller to retrieve the file's data
    */
    private Qore::AbstractPollOperation getIoPollerForLocationImpl(string scheme, string location,
            *hash<auto> opts) {
        hash<ResolvedLocation> r = resolve(scheme, location, opts, False);
        return r.handler.getIoPollerForLocation(r.scheme, r.location, r.opts);
    }

    #! Returns a stream reader for the file's data at the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return a stream reader for the file's contents, if it can be retrieved
    */
    private Qore::StreamReader getStreamReaderImpl(string scheme, string location, *hash<auto> opts) {
        hash<ResolvedLocation> r = resolve(scheme, location, opts, False);
        return r.handler.getStreamReader(r.scheme, r.location, r.opts);
    }

    #! Retrieves a binary file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return an input stream of the file's contents, if it can be retrieved
    */
    private Qore::InputStream getBinaryStreamImpl(string scheme, string location, *hash<auto> opts) {
        hash<ResolvedLocation> r = resolve(scheme, location, opts, False);
        return r.handler.getBinaryStream(r.scheme, r.location, r.opts);
    }

    #! Writes data to a file at the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param contents the file's contents
        @param opts write options
    */
    private writeFileImpl(string scheme, string location, data contents, *hash<auto> opts) {
        hash<ResolvedLocation> r = resolve(scheme, location, opts, True);
        r.handler.writeFile(r.scheme, r.location, contents, r.opts);
    }

    #! Returns an output stream for writing data to the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts write options

        @return an output stream for writing to the file
    */
    private OutputStreamWrapper getOutputStreamImpl(string scheme, string location, *hash<auto> opts) {
        hash<ResolvedLocation> r = resolve(scheme, location, opts, True);
        return r.handler.getOutputStream(r.scheme, r.location, r.opts);
    }

    #! Gets supported read options
    /** @return supported read options
    */
    private hash<string, hash<FileHandlerOptionInfo>> getReadOptionsImpl() {
        return ConnectionLocationOpts;
    }

    #! Gets supported write options
    /** @return supported write options
    */
    private hash<string, hash<FileHandlerOptionInfo>> getWriteOptionsImpl() {
        return ConnectionLocationOpts;
    }

    #! Returns @ref True; options are validated against the resolved target
    /** The set of supported options is not known until the connection has been resolved and a target scheme
        selected, so the framework's own unknown-option check cannot be applied to this handler
    */
    private bool selfValidatesOptionsImpl() {
        return True;
    }

    #! Resolves a connection location to a target handler, location, and options
    /** @param scheme the scheme being used
        @param location the location string without the scheme; \c "<connection-name>/<path>"
        @param opts the location options
        @param write @ref True if the location will be written to

        @return the resolved target

        @throw LOCATION-ERROR the location cannot be parsed; the connection is not allowed, cannot be resolved, or
        does not support file locations; an option is not supported by the resolved target
    */
    private hash<ResolvedLocation> resolve(string scheme, string location, *hash<auto> opts, bool write) {
        string conn_name;
        string path;
        FileLocationHandlerConnection::parseLocation(scheme, location, \conn_name, \path);

        if (!FileLocationHandler::connectionAllowed(conn_name, write)) {
            throw "LOCATION-ERROR", sprintf("connection %y may not be used in file locations", conn_name);
        }

        object conn = getConnection(conn_name);

        # tier 1a: redirect to an existing scheme's handler
        *hash<auto> redirect = conn.getFileLocationRedirect(path, write);
        if (redirect) {
            AbstractFileLocationHandler handler = FileLocationHandler::getHandler(redirect.scheme);

            # options declared by this handler are consumed here; everything else is passed through to the target
            hash<auto> pass = opts - (keys ConnectionLocationOpts);
            # ... except options that this handler and the target both declare, which must be forwarded
            *hash<string, hash<FileHandlerOptionInfo>> target_opt_info = write
                ? handler.getWriteOptions()
                : handler.getReadOptions();
            if (exists opts.encoding && target_opt_info.encoding) {
                pass.encoding = opts.encoding;
            }

            # NOTE: options from the location string take precedence over options supplied by the connection; the
            # caller wrote the former explicitly
            # NOTE: the redirect location may carry credentials, so the original location is used to identify the
            # request in any option error raised here
            *hash<auto> target_opts = FileLocationHandler::checkOptions(
                sprintf("%s://%s/%s", scheme, conn_name, path), handler, redirect.opts + pass, !write);

            return <ResolvedLocation>{
                "handler": handler,
                "scheme": redirect.scheme,
                "location": redirect.location,
                "opts": target_opts,
            };
        }

        # tier 1b: the connection provides a native handler object
        if (conn.supportsFileLocations()) {
            object handler = conn.getFileLocationHandler();
            if (!(handler instanceof AbstractFileLocationHandler)) {
                throw "LOCATION-ERROR", sprintf("connection %y (type %y) returned a file location handler of class "
                    "%y, which does not inherit AbstractFileLocationHandler", conn_name, conn.getType(),
                    handler.className());
            }
            return <ResolvedLocation>{
                "handler": cast<AbstractFileLocationHandler>(handler),
                "scheme": scheme,
                "location": path,
                "opts": opts,
            };
        }

        throw "LOCATION-ERROR", sprintf("connection %y (type %y%s) does not support file locations; the connection "
            "provides neither a location redirect nor a native file location handler", conn_name, conn.getType(),
            (*string app = conn.getAppName()).val() ? sprintf(", app %y", app) : "");
    }

    #! Splits a connection location into the connection name and the path
    /** @param scheme the scheme being used; used in error messages
        @param location the location string without the scheme
        @param conn_name a reference to the connection name
        @param path a reference to the path

        @throw LOCATION-ERROR the location has no connection name or no path
    */
    private static parseLocation(string scheme, string location, reference<string> conn_name,
            reference<string> path) {
        int pos = location.find("/");
        if (pos == -1) {
            throw "LOCATION-ERROR", sprintf("%s location %y has no path; the format is "
                "\"%s://<connection-name>/<path>\"", scheme, scheme + "://" + location, scheme);
        }
        conn_name = location.substr(0, pos);
        if (!conn_name.val()) {
            throw "LOCATION-ERROR", sprintf("%s location %y has an empty connection name; the format is "
                "\"%s://<connection-name>/<path>\"", scheme, scheme + "://" + location, scheme);
        }
        path = location.substr(pos + 1);
        if (!path.val()) {
            throw "LOCATION-ERROR", sprintf("%s location %y has an empty path; the format is "
                "\"%s://<connection-name>/<path>\"", scheme, scheme + "://" + location, scheme);
        }
    }

    #! Returns the connection object for the given connection name
    /** The \c ConnectionProvider module is loaded dynamically here; this module declares no dependency on it, as
        \c FileLocationHandler is a leaf utility that sits below \c ConnectionProvider in the dependency graph

        @param conn_name the connection name

        @return the connection object

        @throw LOCATION-ERROR the \c ConnectionProvider module cannot be loaded, or the connection is unknown
    */
    private object getConnection(string conn_name) {
        try {
            load_module(ConnectionProviderModule);
        } catch (hash<ExceptionInfo> ex) {
            throw "LOCATION-ERROR", sprintf("cannot resolve connection %y; the %s module cannot be loaded: %s: %s",
                conn_name, ConnectionProviderModule, ex.err, ex.desc), ex.arg;
        }

        # NOTE: QORE_CONNECTION_PROVIDERS is deliberately not checked here.  ENV is a per-Program snapshot taken at
        # Program creation, so this module's ENV is not necessarily the one ConnectionProvider sees; checking here
        # could reject a resolvable connection.  get_connection() performs the check in the right context and its
        # message names the environment variable, which is preserved in the wrapped description below
        try {
            return call_function("ConnectionProvider::get_connection", conn_name);
        } catch (hash<ExceptionInfo> ex) {
            throw "LOCATION-ERROR", sprintf("cannot resolve connection %y: %s: %s", conn_name, ex.err, ex.desc),
                ex.arg;
        }
    }
}
}

namespace Priv {
#! A connection location resolved to a target handler
hashdecl ResolvedLocation {
    #! The handler that will service the request
    FileLocationHandler::AbstractFileLocationHandler handler;

    #! The scheme to pass to the handler
    string scheme;

    #! The location string to pass to the handler, without the scheme
    /** @note may contain credentials; never place this value in an error message or log entry
    */
    string location;

    #! The options to pass to the handler
    *hash<auto> opts;
}
}
