# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FileLocationHandlerFtp.qc module for returning file data from a URL-like location string

/*  FileLocationHandlerFtp Copyright 2021 - 2024 Qore Technologies, s.r.o.

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
#! The class for handling "ftp(s)://"" locations
/** Does not currently support non-blocking I/O operations
*/
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
        return f.getAsBinary(file);
    }

    #! Retrieves an I/O poller to retrieve a binary file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return an I/O poller to retrieve the file's data
    */
    private Qore::AbstractPollOperation getIoPollerForLocationImpl(string scheme, string location,
            *hash<auto> opts) {
        throw "UNIMPLEMENTED", "retrieving FTP file data with non-blocking operations is not yet supported";
    }

    #! Returns a stream reader for the file's data at the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return a stream reader for the file's contents, if it can be retrieved

        @note This method can throw any exception related to making an FTP transfer
    */
    private Qore::StreamReader getStreamReaderImpl(string scheme, string location, *hash<auto> opts) {
        string file;
        FtpClient f = getFtpClient(scheme, location, opts, \file);

        StreamPipe pipe(False);

        # run the I/O operation in the background
        background sub () {
            try {
                OutputStream os = pipe.getOutputStream();
                f.get(file, os);
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

        @note This method can throw any exception related to making an FTP transfer
    */
    private Qore::InputStream getBinaryStreamImpl(string scheme, string location, *hash<auto> opts) {
        string file;
        FtpClient f = getFtpClient(scheme, location, opts, \file);

        StreamPipe pipe(False);

        # run the I/O operation in the background
        background sub () {
            try {
                OutputStream os = pipe.getOutputStream();
                f.get(file, os);
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
        - The data written to the OutputStreamWrapper object is written in the background; to ensure that the data has
          been written to the target, call OutputStreamWrapper::waitForIo()
        - This method can throw any exception related to making an FTP transfer
    */
    private OutputStreamWrapper getOutputStreamImpl(string scheme, string location, *hash<auto> opts) {
        string file;
        FtpClient f = getFtpClient(scheme, location, opts, \file);

        StreamPipe pipe(False);

        WaitGroup io_counter();
        io_counter.add();
        OutputStreamWrapper rv(pipe.getOutputStream(), io_counter);

        # run the I/O operation in the background
        background sub () {
            try {
                on_exit io_counter.done();

                Qore::InputStream is = pipe.getInputStream();
                f.put(is, file);
                is.close();
            } catch (hash<ExceptionInfo> ex) {
            }
        }();

        return rv;
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
        FtpClient f(location);
        f.connect();
        string dir = dirname(url_info.path);
        if (dir) {
            f.cwd(dir);
        }
        return f;
    }
}
}
