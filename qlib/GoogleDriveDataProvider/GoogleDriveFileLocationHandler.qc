# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GoogleDriveFileLocationHandler.qc native file location handler for the GoogleDriveDataProvider module

/*  GoogleDriveFileLocationHandler.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the GoogleDriveDataProvider module
public namespace GoogleDriveDataProvider {
#! Reads and writes files directly through a Google Drive connection's own client
/** Returned by \c GoogleRestClient::GoogleDriveRestConnection::getFileLocationHandler(), so that a
    \c "conn://" location naming a Drive connection transfers files through Drive itself rather than through
    the application's request/response actions.

    Two things this gains over the declarative application file API:
    - **writing does not buffer the file.**
      @ref getOutputStreamImpl() "getOutputStreamForLocation()" drives a Drive resumable upload from the
      stream itself, sending each chunk as it is written, so a file larger than memory can be written
    - **reading does not inflate the file.**  The bytes arrive as the response body rather than base64-encoded
      into a JSON field, so nothing is encoded, buffered, and decoded again

    @par Addressing a file
    Drive addresses a file by an immutable ID rather than by path, so a read location is a file ID.  A write
    location is a **name**, since the file being written does not exist yet; the folder to create it in comes
    from the \c container option, and without one the user's \c "My Drive" root is used.
*/
public class GoogleDriveFileLocationHandler inherits FileLocationHandler::AbstractFileLocationHandler {
    public {
        #! Location options accepted by this handler
        const LocationOpts = {
            "container": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "the ID of the folder to create the file in; if not set, the user's `My Drive` root is "
                    "used (write)",
            },
            "content_type": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "the MIME type to store with the file; if not set, it is inferred from the name "
                    "(write)",
            },
            "encoding": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "the encoding of the file's data (read / write)",
            },
            "export_mime_type": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "the format to export a Google-native document to; such a document has no byte content "
                    "of its own and cannot be read without one (read)",
            },
        };
    }

    private {
        #! The provider used to reach Drive, built on the connection's own client
        GoogleDriveFileTransferProvider prov;
    }

    #! Creates the object from a Drive client
    /** @param rest the connection's own client, already authenticated
    */
    constructor(RestClientIo::RestClientIo rest) {
        prov = new GoogleDriveFileTransferProvider(rest);
    }

    #! Retrieves a text file from the given location
    private string getTextFileImpl(string scheme, string location, *hash<auto> opts) {
        string str = prov.get(location, opts.export_mime_type).toString(opts.encoding ?? "UTF-8");
        return AbstractFileLocationHandler::forceEncoding(str, opts.encoding);
    }

    #! Retrieves a binary file from the given location
    private binary getBinaryFileImpl(string scheme, string location, *hash<auto> opts) {
        return prov.get(location, opts.export_mime_type);
    }

    #! Returns a poll operation that reads the file with non-blocking I/O
    /** @param scheme the scheme being used
        @param location the Drive file ID
        @param opts read options

        @return a poll operation whose output is the file's content

        @note A file whose content has to be exported -- a Google-native document -- is read in one request
        when \c export_mime_type is given, and in two otherwise: the metadata is fetched first so that a
        document with no byte content is reported precisely rather than as the API's generic
        <em>"Only files with binary content can be downloaded"</em>.  Both requests are polled
    */
    private Qore::AbstractPollOperation getIoPollerForLocationImpl(string scheme, string location,
            *hash<auto> opts) {
        return prov.startPollGet(location, opts.export_mime_type);
    }

    #! Returns a stream reader for the file's data at the given location
    private Qore::StreamReader getStreamReaderImpl(string scheme, string location, *hash<auto> opts) {
        return new StreamReader(getBinaryStreamImpl(scheme, location, opts), opts.encoding);
    }

    #! Returns an input stream for the file's data at the given location
    /** @note the read is performed before the stream is returned, so that an error reaches the caller directly
        rather than silently terminating the stream
    */
    private Qore::InputStream getBinaryStreamImpl(string scheme, string location, *hash<auto> opts) {
        return new BinaryInputStream(prov.get(location, opts.export_mime_type));
    }

    #! Writes data to a file at the given location
    /** @note the location is the **name** for the new file rather than a file ID: a write creates a file, and
        the ID only exists afterwards
    */
    private writeFileImpl(string scheme, string location, data contents, *hash<auto> opts) {
        if (opts.encoding && contents.typeCode() == NT_STRING && opts.encoding != contents.encoding()) {
            contents = convert_encoding(contents, opts.encoding);
        }
        string name = GoogleDriveFileLocationHandler::getName(location);
        prov.put(name, opts.container, binary(contents),
            opts.content_type ?? (get_mime_type_from_ext(name) ?? MimeTypeOctetStream));
    }

    #! Returns an output stream that uploads what is written to it through a Drive resumable upload
    /** The upload runs in the background as the data is written, so a file larger than memory can be written
        through this stream.

        @param scheme the scheme being used
        @param location the name for the new file
        @param opts write options

        @return an output stream for writing to the file

        @note
        - OutputStream::close() must be called to close the stream after writing terminates, or the upload
          never completes and the background thread is held open
        - call OutputStreamWrapper::waitForIo() to be sure the data reached Drive; it rethrows any error the
          upload raised
    */
    private FileLocationHandler::OutputStreamWrapper getOutputStreamImpl(string scheme, string location,
            *hash<auto> opts) {
        string name = GoogleDriveFileLocationHandler::getName(location);
        string mime = opts.content_type ?? (get_mime_type_from_ext(name) ?? MimeTypeOctetStream);
        *string folder = opts.container;

        StreamPipe pipe(sync_close: False);

        WaitGroup io_counter();
        io_counter.add();
        FileLocationHandler::OutputStreamWrapper rv(pipe.getOutputStream(), io_counter);

        background sub () {
            # NOTE: done() must fire after the catch block records the error, so that waitForIo() observes the
            # error before the wait group drops to zero
            on_exit io_counter.done();
            try {
                prov.putStream(name, folder, pipe.getInputStream(), mime);
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

    #! Returns the name for a new file from a write location
    /** A location is accepted with or without a leading separator, and only its last segment is used: Drive
        has no directory structure to address, so anything before it would be meaningless.

        @param location the location string without the scheme

        @return the file name

        @throw LOCATION-ERROR the location names no file
    */
    static string getName(string location) {
        location =~ s/^\///;
        if (!location.val()) {
            throw "LOCATION-ERROR", "a Google Drive file location must name a file";
        }
        int i = location.rfind("/");
        return i >= 0 ? location.substr(i + 1) : location;
    }
}

#! Gives the file location handler access to the module's Drive request plumbing
/** The request plumbing -- retries, error unpacking, and the resumable upload -- is private to the provider
    hierarchy, and the file location handler is not part of it.  Rather than duplicate any of it, this exposes
    exactly the three operations the handler needs.
*/
public class GoogleDriveFileTransferProvider inherits GoogleDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "file-transfer",
            "type": "GoogleDriveFileTransferProvider",
            "constructor_options": ConstructorOptions,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object from a Drive client
    /** @param rest the connection's own client, already authenticated
    */
    constructor(RestClientIo::RestClientIo rest) : GoogleDriveDataProviderBase(rest) {}

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the content of a file
    /** @param file_id the Drive file ID
        @param export_mime_type the format to export a Google-native document to, if any

        @return the file content

        @throw GOOGLE-DRIVE-ERROR the file is a Google-native document and no export format was given
    */
    binary get(string file_id, *string export_mime_type) {
        if (export_mime_type.val()) {
            return getRawContent(sprintf("/drive/v3/files/%s/export?mimeType=%s", encode_url(file_id, True),
                encode_url(export_mime_type, True)));
        }
        # a Google-native document has no byte content of its own, so the failure is reported precisely rather
        # than as the API's generic "only files with binary content can be downloaded"
        hash<auto> meta = GoogleDriveDataProviderBase::getFileInfo(getFileMetadata(file_id));
        if (meta.is_google_doc) {
            throw "GOOGLE-DRIVE-ERROR", sprintf("file %y (%y) is a Google-native document of type %y, which "
                "has no byte content of its own; set the \"export_mime_type\" location option to a format to "
                "convert it to", meta.name, file_id, meta.mime_type);
        }
        return getRawContent(sprintf("/drive/v3/files/%s?alt=media", encode_url(file_id, True)));
    }

    #! Returns a poll operation that reads a file with non-blocking I/O
    /** @param file_id the Drive file ID
        @param export_mime_type the format to export a Google-native document to, if any

        @return a poll operation whose output is the file content
    */
    Qore::AbstractPollOperation startPollGet(string file_id, *string export_mime_type) {
        return new GoogleDrivePollOperation(rest, file_id, export_mime_type);
    }

    #! Creates a file from content that is already in memory
    put(string name, *string folder_id, binary content, string mime) {
        upload("POST", "/upload/drive/v3/files", GoogleDriveFileTransferProvider::getMetadata(name, folder_id),
            content, mime);
    }

    #! Creates a file from the content of a stream, sending each chunk as it is read
    putStream(string name, *string folder_id, Qore::InputStream is, string mime) {
        uploadResumable("POST", "/upload/drive/v3/files",
            GoogleDriveFileTransferProvider::getMetadata(name, folder_id), is, mime);
    }

    #! Returns the file metadata for a new file
    static hash<auto> getMetadata(string name, *string folder_id) {
        # NOTE: "hash<auto!>"; a single-string literal narrows to hash<string, string>, and "parents" is a list
        hash<auto!> rv = {"name": name};
        if (folder_id.val()) {
            rv.parents = (folder_id,);
        }
        return rv;
    }
}
}
