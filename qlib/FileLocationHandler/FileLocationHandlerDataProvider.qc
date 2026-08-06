# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FileLocationHandlerDataProvider.qc file location handler driving a data provider file API

/*  FileLocationHandlerDataProvider Copyright 2021 - 2026 Qore Technologies, s.r.o.

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
#! Reads and writes single files through a data provider application's declared file API
/** Applications registered in the data provider action catalog expose file operations under their own names and
    with their own option and response field names.  An application that declares a
    \c DataProvider::DataProviderFileApiInfo mapping can be driven generically: this handler resolves the declared
    read or write action, invokes it through the connection's data provider, and extracts the file data from the
    response.

    Instances are created by
    @ref FileLocationHandler::FileLocationHandlerConnection "FileLocationHandlerConnection" during resolution of a
    \c "conn://" location; this class is not registered for a scheme of its own.

    @note This handler is request/response based.  Streams are synthesized by transferring the whole file in a
    background thread, and non-blocking I/O pollers are not supported.  Connections that can stream natively should
    provide a handler through \c ConnectionProvider::AbstractConnection::getFileLocationHandler() instead, which
    takes precedence over this handler.

    @since FileLocationHandler 3.0
*/
class FileLocationHandlerDataProvider inherits AbstractFileLocationHandler {
    private {
        #! The connection serving the application
        object conn;

        #! The connection name; used in error messages
        string conn_name;

        #! The application name
        string app;

        #! The file API declared by the application
        hash<auto> file_api;
    }

    #! Creates the object
    /** @param conn the connection serving the application
        @param conn_name the connection name, used in error messages
        @param app the application name
        @param file_api the \c DataProviderFileApiInfo hash declared by the application
    */
    constructor(object conn, string conn_name, string app, hash<auto> file_api) {
        self.conn = conn;
        self.conn_name = conn_name;
        self.app = app;
        self.file_api = file_api;
    }

    #! Retrieves a text file from the given location
    private string getTextFileImpl(string scheme, string location, *hash<auto> opts) {
        data rv = read(location, opts);
        string str = rv.typeCode() == NT_STRING ? rv : rv.toString(opts.encoding ?? "UTF-8");
        return AbstractFileLocationHandler::forceEncoding(str, opts.encoding);
    }

    #! Retrieves a binary file from the given location
    private binary getBinaryFileImpl(string scheme, string location, *hash<auto> opts) {
        data rv = read(location, opts);
        return rv.typeCode() == NT_BINARY ? rv : binary(rv);
    }

    #! Returns an I/O poller for the file's data at the given location
    /** Only supported when the application's read action returns a URL rather than the data itself: the URL is
        resolved with one blocking action call, and the poller for the URL's own scheme is then returned, so the
        transfer itself - which is the part with the volume - is non-blocking.

        An application whose read action returns the data inline has nothing to poll: the data arrives with the
        action's response, and there is no separate transfer to overlap with other work.

        @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return an I/O poller retrieving the file's data from the URL returned by the read action

        @throw UNIMPLEMENTED the read action returns the data inline, or \c follow_url is set to @ref False
    */
    private Qore::AbstractPollOperation getIoPollerForLocationImpl(string scheme, string location,
            *hash<auto> opts) {
        if (file_api.read_mode != "url") {
            throw "UNIMPLEMENTED", sprintf("connection %y (application %y) reads files through a request/response "
                "data provider action returning the data itself; non-blocking I/O is not supported for such "
                "connections", conn_name, app);
        }
        if (exists opts.follow_url && !opts.follow_url) {
            throw "UNIMPLEMENTED", sprintf("connection %y (application %y): \"follow_url\" is set to False, so the "
                "read returns the URL itself rather than its contents; there is nothing to poll", conn_name, app);
        }
        # the URL is resolved with a blocking call; only the transfer it points at is polled
        return FileLocationHandler::getIoPollerForLocation(getReadUrl(location, opts));
    }

    #! Returns a stream reader for the file's data at the given location
    /** @note the file is transferred in full in a background thread
    */
    private Qore::StreamReader getStreamReaderImpl(string scheme, string location, *hash<auto> opts) {
        return new StreamReader(getBinaryStreamImpl(scheme, location, opts), opts.encoding);
    }

    #! Returns an input stream for the file's data at the given location
    /** @note the file is transferred in full in a background thread
    */
    private Qore::InputStream getBinaryStreamImpl(string scheme, string location, *hash<auto> opts) {
        # the read is performed before the stream is returned so that errors reach the caller directly rather
        # than silently terminating the stream
        binary content = getBinaryFileImpl(scheme, location, opts);
        return new BinaryInputStream(content);
    }

    #! Writes data to a file at the given location
    private writeFileImpl(string scheme, string location, data contents, *hash<auto> opts) {
        write(location, contents, opts);
    }

    #! Returns an output stream for writing data to the given location
    /** The data is buffered and written when the stream is closed; OutputStreamWrapper::waitForIo() rethrows any
        error raised by the write
    */
    private OutputStreamWrapper getOutputStreamImpl(string scheme, string location, *hash<auto> opts) {
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
                write(location, content ?? binary(), opts);
            } catch (hash<ExceptionInfo> ex) {
                rv.setIoError(ex);
            }
        }();

        return rv;
    }

    #! Gets supported read options
    private hash<string, hash<FileHandlerOptionInfo>> getReadOptionsImpl() {
        return FileLocationHandlerConnection::ConnectionLocationOpts;
    }

    #! Gets supported write options
    private hash<string, hash<FileHandlerOptionInfo>> getWriteOptionsImpl() {
        return FileLocationHandlerConnection::ConnectionLocationOpts;
    }

    #! Returns @ref True; options were already validated by the connection handler
    private bool selfValidatesOptionsImpl() {
        return True;
    }

    #! Invokes the application's declared read action and returns the value of its data field
    /** @param path the path within the connection
        @param opts read options

        @return the value of the field the file API declares as holding the data or the URL

        @throw LOCATION-ERROR the action's response has no such field
    */
    private auto readValue(string path, *hash<auto> opts) {
        hash<auto> req = {
            file_api.read_path_option: path,
        } + file_api.options_add;
        if (file_api.read_container_option.val() && opts.container.val()) {
            req{file_api.read_container_option} = opts.container;
        }

        auto resp = doAction(file_api.read_action, req, opts);

        if (!resp.hasKey(file_api.read_data_field)) {
            throw "LOCATION-ERROR", sprintf("connection %y (application %y): action %y returned no %y field; "
                "returned fields: %y", conn_name, app, file_api.read_action, file_api.read_data_field,
                resp.typeCode() == NT_HASH ? keys resp : resp.type());
        }
        return resp{file_api.read_data_field};
    }

    #! Returns the URL the application's read action reports for the file at the given path
    /** @param path the path within the connection
        @param opts read options

        @return the URL the file can be retrieved from

        @throw LOCATION-ERROR the action returned an empty URL
    */
    private string getReadUrl(string path, *hash<auto> opts) {
        auto value = readValue(path, opts);
        if (!value.val()) {
            throw "LOCATION-ERROR", sprintf("connection %y (application %y): action %y returned an empty URL "
                "in %y", conn_name, app, file_api.read_action, file_api.read_data_field);
        }
        return value.toString();
    }

    #! Reads the file at the given path through the application's declared read action
    private data read(string path, *hash<auto> opts) {
        if (file_api.read_mode == "url") {
            if (exists opts.follow_url && !opts.follow_url) {
                # the caller asked for the URL itself rather than its contents
                return getReadUrl(path, opts);
            }
            return FileLocationHandler::getBinaryFileFromLocation(getReadUrl(path, opts));
        }

        auto value = readValue(path, opts);
        if (!exists value) {
            throw "LOCATION-ERROR", sprintf("connection %y (application %y): action %y returned no data in %y",
                conn_name, app, file_api.read_action, file_api.read_data_field);
        }
        if (file_api.read_data_encoding == "base64") {
            # already-binary data is passed through; some providers return decoded data despite declaring base64
            return value.typeCode() == NT_BINARY ? value : parse_base64_string(value.toString());
        }
        return value.typeCode() == NT_BINARY || value.typeCode() == NT_STRING ? value : value.toString();
    }

    #! Writes the given data to the given path through the application's declared write action
    private write(string path, data contents, *hash<auto> opts) {
        if (!file_api.write_action.val()) {
            throw "LOCATION-ERROR", sprintf("connection %y (application %y) is read-only for file locations; the "
                "application declares no write action", conn_name, app);
        }

        # applications that address a new file by parent plus name take only the parent here; the name travels
        # in the file value
        string path_value = path;
        if (file_api.write_path_is_parent) {
            int pos = path.rfind("/");
            path_value = pos == -1 ? "" : path.substr(0, pos);
        }

        hash<auto> req = {
            file_api.write_path_option: path_value,
            file_api.write_data_option: getWriteValue(path, contents, opts),
        } + file_api.options_add;
        if (file_api.write_container_option.val() && opts.container.val()) {
            req{file_api.write_container_option} = opts.container;
        }

        doAction(file_api.write_action, req, opts);
    }

    #! Returns the value to pass in the write action's data option, in the declared format
    private auto getWriteValue(string path, data contents, *hash<auto> opts) {
        if (opts.encoding && contents.typeCode() == NT_STRING && opts.encoding != contents.encoding()) {
            contents = convert_encoding(contents, opts.encoding);
        }
        switch (file_api.write_data_format) {
            case "raw":
                return contents;
            case "base64":
                return make_base64_string(contents);
        }
        # DPFW_FILE: a FileDataType hash
        return {
            "name": basename(path),
            "content": make_base64_string(contents),
            "mime_type": "auto",
        };
    }

    #! Resolves and invokes the named action on the connection's data provider
    private auto doAction(string action_name, hash<auto> req, *hash<auto> opts) {
        load_module("DataProvider");

        hash<auto> action;
        try {
            action = call_static_method("DataProvider::DataProviderActionCatalog", "getAppActionEx", app,
                action_name);
        } catch (hash<ExceptionInfo> ex) {
            throw "LOCATION-ERROR", sprintf("connection %y (application %y): the file API declares action %y, "
                "which is not registered: %s: %s", conn_name, app, action_name, ex.err, ex.desc), ex.arg;
        }

        object provider;
        try {
            *string subtype = opts.subtype ?? action.subtype;
            provider = subtype.val()
                ? conn.getDataProvider(subtype, opts.conn_rtopts)
                : conn.getDataProvider(opts.conn_rtopts);
            if (action.path.val()) {
                # action paths are relative to the application's data provider and start with "/"
                string rel = action.path;
                rel =~ s/^\///;
                if (rel.val()) {
                    provider = provider.getChildProviderPath(rel);
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            throw "LOCATION-ERROR", sprintf("connection %y (application %y): cannot resolve the data provider for "
                "action %y (path %y): %s: %s", conn_name, app, action_name, action.path, ex.err, ex.desc), ex.arg;
        }

        return provider.doRequest(req);
    }
}
}
