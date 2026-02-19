# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FileLocationHandlerFile.qc module for returning file data from a URL-like location string

/*  FileLocationHandlerFile Copyright 2021 - 2024 Qore Technologies, s.r.o.

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
#! The class for handling "file://" locations
class FileLocationHandlerFile inherits AbstractFileLocationHandler {
    public {
        #! Valid common file location options
        /**
        */
        const FileLocationOpts = {
            "encoding": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "the encoding of the file's data (read / write)",
            },
            "max_file_len": <FileHandlerOptionInfo>{
                "type": "int",
                "default_value": -1,
                "desc": "maximum length of files (read / write)",
            },
        };

        #! Valid common file location options
        /**
        */
        const FileLocationReadOpts = FileLocationOpts + {
            "stream_read_io_timeout": <FileHandlerOptionInfo>{
                "type": "int",
                "default_value": 60000,
                "desc": "the default I/O timeout for stream operations, default is 60 seconds",
            },
            "stream_nonblock_open": <FileHandlerOptionInfo>{
                "type": "bool",
                "default_value": False,
                "desc": "if `True` then the stream will be opened in nonblocking mode (default `False`)",
            },
        };

        #! Valid file location write options
        /**
        */
        const FileLocationWriteOpts = FileLocationOpts + {
            "append": <FileHandlerOptionInfo>{
                "type": "bool",
                "default_value": False,
                "desc": "if true the file will be appended, otherwise it will be truncated (the default)",
            },
            "file_permissions": <FileHandlerOptionInfo>{
                "type": "int",
                "default_value": 0644,
                "desc": "file permissions (default: 0664)",
            },
        };
    }

    #! Retrieves a text file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return the string of the file's contents, if it can be retrieved

        @throw FILE-READ-ERROR an I/O or other error occurred when reading the file

        @note Supports environment variable substitution in \a location
    */
    private string getTextFileImpl(string scheme, string location, *hash<auto> opts) {
        return ReadOnlyFile::readTextFile(substitute_env_vars(location), opts.encoding, opts.max_file_len);
    }

    #! Retrieves a binary file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return the binary data of the file's contents, if it can be retrieved

        @throw FILE-READ-ERROR an I/O or other error occurred when reading the file

        @note Supports environment variable substitution in \a location
    */
    private binary getBinaryFileImpl(string scheme, string location, *hash<auto> opts) {
        return ReadOnlyFile::readBinaryFile(substitute_env_vars(location), opts.max_file_len);
    }

    #! Retrieves an I/O poller to retrieve a binary file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return an I/O poller to retrieve the file's data
    */
    private Qore::AbstractPollOperation getIoPollerForLocationImpl(string scheme, string location,
            *hash<auto> opts) {
        File f();
        return f.startPollRead(substitute_env_vars(location));
    }

    #! Returns a stream reader for the file's data at the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return a stream reader for the file's contents, if it can be retrieved

        @note Supports environment variable substitution in \a location
    */
    private Qore::StreamReader getStreamReaderImpl(string scheme, string location, *hash<auto> opts) {
        return new StreamReader(getBinaryStreamImpl(scheme, location, opts), opts.encoding);
    }

    #! Retrieves a binary file from the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts read options

        @return an input stream of the file's contents, if it can be retrieved

        @note Supports environment variable substitution in \a location
    */
    private Qore::InputStream getBinaryStreamImpl(string scheme, string location, *hash<auto> opts) {
        return new FileInputStream(substitute_env_vars(location), opts.stream_read_io_timeout,
            opts.stream_nonblock_open);
    }

    #! Writes data to a file at the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param contents the file's contents
        @param opts write options

        @note Supports environment variable substitution in \a location
    */
    private writeFileImpl(string scheme, string location, data contents, *hash<auto> opts) {
        if (opts.max_file_len > 0 && contents.size() > opts.max_file_len) {
            throw "FILE-WRITE-ERROR", sprintf("the file's size (%d bytes) exceeds max_file_len = %d", contents.size(),
                opts.max_file_len);
        }
        File f();
        int open_flags = O_CREAT | O_WRONLY | (opts.append ? O_APPEND : O_TRUNC);
        int file_permissions = opts.file_permissions;
        f.open2(substitute_env_vars(location), open_flags, file_permissions, opts.encoding);
        f.write(contents);
    }

    #! Returns an output stream for writing data to the given location
    /** @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts write options

        @return an output stream for writing to the file

        @note
        - OutputStream::close() must be called to close the stream after writing terminates in order to avoid a
          deadlock
        - Supports environment variable substitution in \a location
    */
    private OutputStreamWrapper getOutputStreamImpl(string scheme, string location, *hash<auto> opts) {
        return new OutputStreamWrapper(new FileOutputStream(substitute_env_vars(location), opts.append,
            opts.file_permissions, opts.encoding), new Counter());
    }

    #! Gets supported read options
    /** @return supported read options
    */
    private hash<string, hash<FileHandlerOptionInfo>> getReadOptionsImpl() {
        return FileLocationReadOpts;
    }

    #! Gets supported write options
    /** @return supported write options
    */
    private hash<string, hash<FileHandlerOptionInfo>> getWriteOptionsImpl() {
        return FileLocationWriteOpts;
    }
}
}
