# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DropboxFileLocationHandler.qc native file location handler for Dropbox connections

/*  DropboxFileLocationHandler.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the DropboxDataProvider module
public namespace DropboxDataProvider {
#! Reads and writes files directly through a Dropbox connection's own client
/** Returned by \c DropboxRestClient::DropboxRestConnection::getFileLocationHandler(), so that a \c "conn://"
    location naming a Dropbox connection transfers files through the Dropbox content endpoints rather than
    through the application's request/response actions.

    Two things this gains over the declarative application file API:
    - **writing does not buffer the file.**
      @ref getOutputStreamImpl() "getOutputStreamForLocation()" drives a Dropbox upload session from the stream
      itself, sending each chunk as it is written.  Going through the \c upload-file action instead requires the
      whole file in memory, base64-encoded, before the first byte is sent
    - **reading does not inflate the file.**  The bytes arrive as the response body rather than base64-encoded
      into a JSON field, so nothing is encoded, buffered, and decoded again

    @since DropboxDataProvider 1.0
*/
public class DropboxFileLocationHandler inherits FileLocationHandler::AbstractFileLocationHandler {
    public {
        #! Location options accepted by this handler
        const LocationOpts = {
            "autorename": <FileHandlerOptionInfo>{
                "type": "bool",
                "default_value": False,
                "desc": "choose a name that does not conflict rather than failing when a file already exists at "
                    "the destination (write)",
            },
            "encoding": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "the encoding of the file's data (read / write)",
            },
            "mode": <FileHandlerOptionInfo>{
                "type": "string",
                "default_value": "overwrite",
                "desc": "what to do when a file already exists at the path; either `add` or `overwrite` (write)",
            },
            "mute": <FileHandlerOptionInfo>{
                "type": "bool",
                "default_value": False,
                "desc": "do not show the user a desktop notification about this change (write)",
            },
        };

        #! Write modes Dropbox accepts
        const WriteModes = {
            "add": True,
            "overwrite": True,
        };
    }

    private {
        #! The connection's own client
        DropboxRestClient::DropboxRestClientIo rest;
    }

    #! Creates the object from a Dropbox client
    /** @param rest the connection's own client, already authenticated
    */
    constructor(DropboxRestClient::DropboxRestClientIo rest) {
        self.rest = rest;
    }

    #! Retrieves a text file from the given location
    private string getTextFileImpl(string scheme, string location, *hash<auto> opts) {
        string str = download(location).toString(opts.encoding ?? "UTF-8");
        return AbstractFileLocationHandler::forceEncoding(str, opts.encoding);
    }

    #! Retrieves a binary file from the given location
    private binary getBinaryFileImpl(string scheme, string location, *hash<auto> opts) {
        return download(location);
    }

    #! Not supported; the Dropbox content endpoints are driven through the REST client's request API
    private Qore::AbstractPollOperation getIoPollerForLocationImpl(string scheme, string location,
            *hash<auto> opts) {
        throw "UNIMPLEMENTED", sprintf("Dropbox file location %y cannot be read with non-blocking I/O; the "
            "content endpoint is driven through the REST client's request API", location);
    }

    #! Returns a stream reader for the file's data at the given location
    /** @note the response body arrives whole, so the file is held in memory; it is not base64-inflated on the
        way, as it is when the same file is read through the application's \c download-file action
    */
    private Qore::StreamReader getStreamReaderImpl(string scheme, string location, *hash<auto> opts) {
        return new StreamReader(getBinaryStreamImpl(scheme, location, opts), opts.encoding);
    }

    #! Returns an input stream for the file's data at the given location
    /** @note the read is performed before the stream is returned, so that an error reaches the caller directly
        rather than silently terminating the stream
    */
    private Qore::InputStream getBinaryStreamImpl(string scheme, string location, *hash<auto> opts) {
        return new BinaryInputStream(download(location));
    }

    #! Writes data to a file at the given location
    private writeFileImpl(string scheme, string location, data contents, *hash<auto> opts) {
        if (opts.encoding && contents.typeCode() == NT_STRING && opts.encoding != contents.encoding()) {
            contents = convert_encoding(contents, opts.encoding);
        }
        uploadWhole(DropboxDataProviderBase::normalizePath(location), binary(contents), opts);
    }

    #! Returns an output stream that uploads what is written to it through a Dropbox upload session
    /** The upload runs in the background as the data is written, so a file larger than memory can be written
        through this stream.  Only when the whole file turns out to fit in a single chunk is a plain single-shot
        upload used instead, which saves two round trips on a small file.

        @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts write options

        @return an output stream for writing to the file

        @note
        - OutputStream::close() must be called to close the stream after writing terminates, or the upload never
          completes and the background thread is held open
        - call OutputStreamWrapper::waitForIo() to be sure the data reached Dropbox; it rethrows any error the
          upload raised
    */
    private FileLocationHandler::OutputStreamWrapper getOutputStreamImpl(string scheme, string location,
            *hash<auto> opts) {
        string path = DropboxDataProviderBase::normalizePath(location);
        checkPath(path);

        StreamPipe pipe(sync_close: False);

        WaitGroup io_counter();
        io_counter.add();
        FileLocationHandler::OutputStreamWrapper rv(pipe.getOutputStream(), io_counter);

        background sub () {
            # NOTE: done() must fire after the catch block records the error, so that waitForIo() observes the
            # error before the wait group drops to zero
            on_exit io_counter.done();
            try {
                uploadStream(path, pipe.getInputStream(), opts);
            } catch (hash<ExceptionInfo> ex) {
                rv.setIoError(ex);
            }
        }();

        return rv;
    }

    #! Gets supported read options
    private hash<string, hash<FileHandlerOptionInfo>> getReadOptionsImpl() {
        return LocationOpts;
    }

    #! Gets supported write options
    private hash<string, hash<FileHandlerOptionInfo>> getWriteOptionsImpl() {
        return LocationOpts;
    }

    #! Downloads the file at the given location
    /** @param location the location string without the scheme

        @return the file content

        @throw DROPBOX-ERROR the download failed
    */
    private binary download(string location) {
        string path = DropboxDataProviderBase::normalizePath(location);
        checkPath(path);
        hash<auto> resp = rest.contentRequest("/2/files/download", {"path": path});
        return DropboxDownloadFileDataProvider::getBinaryBody(resp.body);
    }

    #! Uploads content that is already in memory
    private uploadWhole(string path, binary content, *hash<auto> opts) {
        checkPath(path);
        hash<auto> commit = getCommit(path, opts);
        if (content.size() <= UploadSessionThreshold) {
            rest.contentRequest("/2/files/upload", commit, content, MimeTypeOctetStream);
            return;
        }
        int offset = 0;
        *string session_id;
        while (offset < content.size()) {
            int len = content.size() - offset;
            if (len > UploadChunkSize) {
                len = UploadChunkSize;
            }
            session_id = sendChunk(session_id, offset, content.substr(offset, len),
                offset + len >= content.size(), commit);
            offset += len;
        }
    }

    #! Uploads the content of an input stream, sending each chunk as it is read
    /** The total size is not known in advance, so the first chunk decides the transfer method: a file that ends
        within it is sent in a single request, and anything larger opens an upload session.

        @param path the destination path
        @param is the stream to read the content from
        @param opts write options
    */
    private uploadStream(string path, Qore::InputStream is, *hash<auto> opts) {
        hash<auto> commit = getCommit(path, opts);

        binary buf = DropboxFileLocationHandler::readExactly(is, UploadChunkSize);
        if (buf.size() < UploadChunkSize) {
            # the whole file arrived within the first chunk; a session would cost two extra round trips
            rest.contentRequest("/2/files/upload", commit, buf, MimeTypeOctetStream);
            return;
        }

        *string session_id = sendChunk(NOTHING, 0, buf, False, commit);
        int offset = buf.size();
        while (True) {
            buf = DropboxFileLocationHandler::readExactly(is, UploadChunkSize);
            bool last = buf.size() < UploadChunkSize;
            # a final chunk of zero bytes is legal and commits the session
            sendChunk(session_id, offset, buf, last, commit);
            offset += buf.size();
            if (last) {
                break;
            }
        }
    }

    #! Sends one chunk of an upload session and returns the session ID
    /** @param session_id the session to append to, or @ref nothing to open one
        @param offset the offset the chunk starts at
        @param content the chunk
        @param last @ref True if this chunk completes the upload
        @param commit the commit arguments, used when the session is closed

        @return the session ID

        @throw DROPBOX-ERROR the server opened a session without giving it an ID
    */
    private *string sendChunk(*string session_id, int offset, binary content, bool last, hash<auto> commit) {
        if (!session_id.val()) {
            hash<auto> start = rest.contentRequest("/2/files/upload_session/start", {"close": False}, content,
                MimeTypeOctetStream).body;
            session_id = start.session_id;
            if (!session_id.val()) {
                throw "DROPBOX-ERROR", sprintf("upload session for %y was not given a session ID by the server",
                    commit.path);
            }
            return session_id;
        }
        hash<auto> cursor = {"session_id": session_id, "offset": offset};
        if (last) {
            rest.contentRequest("/2/files/upload_session/finish", {"cursor": cursor, "commit": commit}, content,
                MimeTypeOctetStream);
        } else {
            rest.contentRequest("/2/files/upload_session/append_v2", {"cursor": cursor, "close": False}, content,
                MimeTypeOctetStream);
        }
        return session_id;
    }

    #! Returns the Dropbox commit arguments for a write
    private hash<auto> getCommit(string path, *hash<auto> opts) {
        string mode = opts.mode ?? "overwrite";
        if (!WriteModes{mode}) {
            throw "LOCATION-ERROR", sprintf("Dropbox write mode %y is unknown; known modes: %y", mode,
                keys WriteModes);
        }
        return {
            "path": path,
            "mode": mode,
            "autorename": opts.autorename ?? False,
            "mute": opts.mute ?? False,
        };
    }

    #! Rejects a location that addresses the account root rather than a file
    private checkPath(string path) {
        if (path == RootPath) {
            throw "LOCATION-ERROR", "a Dropbox file location must name a file; the account root is not a file";
        }
    }

    #! Reads exactly the given number of bytes from a stream, or fewer if the stream ends first
    /** A stream read may return fewer bytes than asked for without being at the end, so a single read cannot be
        used to decide that the content is finished; that decision is what selects a single-shot upload over an
        upload session, and getting it wrong would truncate the file

        @param is the stream to read from
        @param n the number of bytes wanted

        @return the bytes read; shorter than \a n only when the stream ended
    */
    static binary readExactly(Qore::InputStream is, int n) {
        binary rv = binary();
        while (rv.size() < n) {
            *binary b = is.read(n - rv.size());
            if (!b) {
                break;
            }
            rv += b;
        }
        return rv;
    }
}
}
