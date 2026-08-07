# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FileLocationHandlerFtp.qc module for returning file data from a URL-like location string

/*  FileLocationHandlerFtp Copyright 2021 - 2026 Qore Technologies, s.r.o.

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
class FileLocationHandlerFtpOutputStreamWrapper inherits OutputStreamWrapper {
    private {
        FtpClient client;
        string file;
        bool closed = False;
    }

    constructor(FtpClient client, string file) : OutputStreamWrapper(new BinaryOutputStream(), new WaitGroup()) {
        self.client = client;
        self.file = file;
    }

    close() {
        if (closed) {
            return;
        }

        BinaryOutputStream bos = cast<BinaryOutputStream>(getStream());
        client.put(input_stream: new BinaryInputStream(source: bos.getData()), remote_path: file);
        bos.close();
        closed = True;
    }
}

#! Non-blocking retrieval of a single file over FTP
/** Adapts the composite @ref FtpClientIo::FtpClientPollOperation "FtpClientPollOperation" -- which is driven
    a phase at a time -- to the single @ref Qore::AbstractPollOperation "AbstractPollOperation" contract the
    file location handlers expose, and follows the same path as the blocking implementation: log in, change
    to the file's directory, then retrieve it by name.
*/
class FileLocationHandlerFtpPollOperation inherits Qore::AbstractPollOperation<auto> {
    public {
        #! The default FTP control port
        const DefaultFtpPort = 21;

        #! Logging in to the server
        const SPS_LOGIN = "login";
        #! Changing to the file's directory
        const SPS_CWD = "cwd";
        #! Retrieving the file
        const SPS_GET = "get";
        #! The file has been retrieved
        const SPS_COMPLETE = "complete";
    }

    private {
        FtpClientIo::FtpClientPollOperation op;
        string remote_dir;
        string remote_file;
        string url;
        string state = SPS_LOGIN;
        *binary output;
    }

    #! Creates the operation; performs no I/O
    /** @param scheme the scheme being used ("ftp" or "ftps")
        @param location the location string without the scheme

        @throw LOCATION-ERROR the location does not name a file
    */
    constructor(string scheme, string location) {
        url = scheme + "://" + location;
        hash<UrlInfo> url_info = parse_url(url);
        if (url_info.path.val()) {
            remote_file = basename(url_info.path);
            remote_dir = dirname(url_info.path);
        }
        if (!remote_file.val()) {
            throw "LOCATION-ERROR", sprintf("FTP location %y missing file path", url);
        }
        bool secure = scheme == "ftps";
        # NOTE: positional arguments are required here; the named form fails with
        # NAMED-CALL-NOT-SUPPORTED, as the signature is not resolved at parse time
        op = new FtpClientIo::FtpClientPollOperation(url_info.host, url_info.port ?? DefaultFtpPort, secure,
            secure, url_info.username, url_info.password);
    }

    #! Returns the goal of the operation
    string getGoal() {
        return sprintf("FTP retrieve %y", url);
    }

    #! Returns the current state
    string getState() {
        return state;
    }

    #! Returns @ref True when the file has been retrieved
    bool goalReached() {
        return state == SPS_COMPLETE;
    }

    #! Continues the poll operation
    /** @return a @ref Qore::SocketPollInfo "SocketPollInfo" hash if more I/O is needed, or nothing when the
        transfer is complete
    */
    *hash<SocketPollInfo> continuePoll() {
        while (True) {
            AbstractPollOperation cur = op.getCurrentPollOp();
            *hash<SocketPollInfo> rv = cur.continuePoll();
            # NOTE: readiness is checked before \a rv is returned, and not after; the poll cycle that
            # finishes establishing the data connection also starts the transfer, so it returns a
            # descriptor to wait on that nothing will ever arrive on until the transfer command is sent
            if (!op.currentOpReady()) {
                if (!rv) {
                    throw "FTP-POLL-ERROR", sprintf("FTP operation for %y cannot continue in state %y and "
                        "has nothing to wait for", url, cur.getState());
                }
                return rv;
            }
            if (!op.advancePhase()) {
                continue;
            }
            switch (state) {
                case SPS_LOGIN:
                    if (remote_dir.val()) {
                        state = SPS_CWD;
                        op.submitCommand("CWD", remote_dir);
                        continue;
                    }
                    state = SPS_GET;
                    op.submitGet(remote_file);
                    continue;

                case SPS_CWD: {
                    *hash<FtpClientIo::FtpCommandResult> resp = op.getCommandResult();
                    if ((resp.code / 100) != 2) {
                        throw "LOCATION-ERROR", sprintf("cannot change to directory %y for FTP location "
                            "%y: %s", remote_dir, url, resp.message);
                    }
                    state = SPS_GET;
                    op.submitGet(remote_file);
                    continue;
                }
            }
            output = op.getTransferResult().data;
            state = SPS_COMPLETE;
            return;
        }
    }

    #! Returns the file's data
    auto getOutput() {
        return output ?? binary();
    }

    #! Aborts the operation
    abort() {
        op.getCurrentPollOp().abort();
    }
}

#! The class for handling "ftp(s)://"" locations
class FileLocationHandlerFtp inherits AbstractFileLocationHandler {
    public {
        #! Valid common file location options
        /**
        */
        const FtpClientLocationOpts = {
            "encoding": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "the encoding of the file's data (read / write)",
            },
        };
    }

    #! Retrieves a text file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return the string of the file's contents, if it can be retrieved

        @note This method can throw any exception related to making an FTP transfer
    */
    private string getTextFileImpl(string scheme, string location, *hash<auto> opts) {
        string file;
        FtpClient f = getFtpClient(scheme, location, opts, \file);
        return f.getAsString(file, opts.encoding);
    }

    #! Retrieves a binary file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return the binary data of the file's contents, if it can be retrieved

        @note This method can throw any exception related to making an FTP transfer
    */
    private binary getBinaryFileImpl(string scheme, string location, *hash<auto> opts) {
        string file;
        FtpClient f = getFtpClient(scheme, location, opts, \file);
        return f.getAsBinary(remote_path: file);
    }

    #! Retrieves an I/O poller to retrieve a binary file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return an I/O poller to retrieve the file's data

        @throw LOCATION-ERROR the location does not name a file

        @note No I/O is performed until the returned operation is driven
    */
    private Qore::AbstractPollOperation getIoPollerForLocationImpl(string scheme, string location,
            *hash<auto> opts) {
        return new FileLocationHandlerFtpPollOperation(scheme, location);
    }

    #! Returns a stream reader for the file's data at the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return a stream reader for the file's contents, if it can be retrieved

        @note This method can throw any exception related to making an FTP transfer
    */
    private Qore::StreamReader getStreamReaderImpl(string scheme, string location, *hash<auto> opts) {
        return new StreamReader(getBinaryStreamImpl(scheme, location, opts), opts.encoding);
    }

    #! Retrieves a binary file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return an input stream of the file's contents, if it can be retrieved

        @note This method can throw any exception related to making an FTP transfer
    */
    private Qore::InputStream getBinaryStreamImpl(string scheme, string location, *hash<auto> opts) {
        return new BinaryInputStream(source: getBinaryFileImpl(scheme, location, opts));
    }

    #! Writes data to a file at the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param contents the file's contents
        @param opts write options

        @note This method can throw any exception related to making an FTP transfer
    */
    private writeFileImpl(string scheme, string location, data contents, *hash<auto> opts) {
        # convert encoding if necessary
        if (opts.encoding && contents.typeCode() == NT_STRING && opts.encoding != contents.encoding()) {
            contents = convert_encoding(contents, opts.encoding);
        }
        string file;
        FtpClient f = getFtpClient(scheme, location, opts, \file);
        return f.put(contents.typeCode() == NT_STRING
            ? new StringInputStream(contents)
            : new BinaryInputStream(contents), file);
    }

    #! Returns an output stream for writing data to the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts write options

        @return an output stream for writing to the file

        @note
        - OutputStream::close() must be called to close the stream after writing terminates in order to avoid a
          deadlock
        - The data written to the OutputStreamWrapper object is uploaded when OutputStreamWrapper::close() is called;
          OutputStreamWrapper::waitForIo() is retained for API compatibility
        - This method can throw any exception related to making an FTP transfer
    */
    private OutputStreamWrapper getOutputStreamImpl(string scheme, string location, *hash<auto> opts) {
        string file;
        FtpClient f = getFtpClient(scheme, location, opts, \file);
        return new FileLocationHandlerFtpOutputStreamWrapper(f, file);
    }

    #! Gets supported read options
    /** @return supported read options
    */
    private hash<string, hash<FileHandlerOptionInfo>> getReadOptionsImpl() {
        return FtpClientLocationOpts;
    }

    #! Gets supported write options
    /** @return supported write options
    */
    private hash<string, hash<FileHandlerOptionInfo>> getWriteOptionsImpl() {
        return FtpClientLocationOpts;
    }

    #! Returns an FTP client for the given location
    /*
        @note This method can throw any exception related to making an FTP connection
    */
    private FtpClient getFtpClient(string scheme, string location, *hash<auto> opts, reference<string> file) {
        location = scheme + "://" + location;

        hash<UrlInfo> url_info = parse_url(location);
        if (url_info.path) {
            file = basename(url_info.path);
        }
        if (!file) {
            throw "LOCATION-ERROR", sprintf("FTP location %y missing file path", location);
        }
        FtpClient f(url: location);
        f.connect();
        string dir = dirname(url_info.path);
        if (dir) {
            f.cwd(path: dir);
        }
        return f;
    }
}
}
