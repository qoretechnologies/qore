# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AwsS3FileLocationHandler.qc native file location handler for the AwsS3DataProvider module

/*  AwsS3FileLocationHandler.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the AwsS3DataProvider module
public namespace AwsS3DataProvider {
#! Reads and writes objects directly through an Amazon S3 connection's own client
/** Returned by \c AwsRestClient::AwsS3RestConnectionBase::getFileLocationHandler(), so that a \c "conn://"
    location naming an S3 connection transfers objects through S3 itself rather than through the application's
    request/response actions.

    Two things this gains over the declarative application file API:
    - **writing does not buffer the object.**
      @ref getOutputStreamImpl() "getOutputStreamForLocation()" drives a multipart upload from the stream
      itself, sending each part as it is written, so an object larger than memory can be written.  Going
      through the \c upload-file action instead requires the whole object in memory, base64-encoded, before
      the first byte is sent
    - **reading does not inflate the object.**  The bytes arrive as the response body rather than
      base64-encoded into a JSON field, so nothing is encoded, buffered, and decoded again

    @par Addressing the bucket
    S3 has no path: a key is a flat string in which \c "/" is an ordinary character.  The bucket is therefore
    taken from the \c container option when it is given, and otherwise from the first segment of the location,
    which is the form <tt>conn://s3/bucket/key</tt>.

    @since AwsS3DataProvider 1.0
*/
public class AwsS3FileLocationHandler inherits FileLocationHandler::AbstractFileLocationHandler {
    public {
        #! Location options accepted by this handler
        const LocationOpts = {
            "container": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "the bucket holding the object; if not set, the first segment of the location is used "
                    "as the bucket name (read / write)",
            },
            "content_type": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "the MIME type to store with the object; if not set, it is inferred from the key "
                    "(write)",
            },
            "encoding": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "the encoding of the file's data (read / write)",
            },
            "storage_class": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "the S3 storage class to store the object under (write)",
            },
            "version_id": <FileHandlerOptionInfo>{
                "type": "string",
                "desc": "a specific object version to read; only meaningful on a version-enabled bucket "
                    "(read)",
            },
        };
    }

    private {
        #! The provider used to reach S3, built on the connection's own client
        AwsS3FileTransferProvider prov;
    }

    #! Creates the object from an S3 client
    /** @param rest the connection's own client, already authenticated
        @param path_style if buckets are addressed in the URI path rather than in the hostname
    */
    constructor(AwsRestClient::AwsRestClient rest, bool path_style = True) {
        prov = new AwsS3FileTransferProvider(rest, path_style);
    }

    #! Retrieves a text file from the given location
    private string getTextFileImpl(string scheme, string location, *hash<auto> opts) {
        string str = download(location, opts).toString(opts.encoding ?? "UTF-8");
        return AbstractFileLocationHandler::forceEncoding(str, opts.encoding);
    }

    #! Retrieves a binary file from the given location
    private binary getBinaryFileImpl(string scheme, string location, *hash<auto> opts) {
        return download(location, opts);
    }

    #! Not supported; S3 requests are driven through the REST client's request API
    private Qore::AbstractPollOperation getIoPollerForLocationImpl(string scheme, string location,
            *hash<auto> opts) {
        throw "UNIMPLEMENTED", sprintf("Amazon S3 file location %y cannot be read with non-blocking I/O; S3 "
            "requests are signed and driven through the REST client's request API", location);
    }

    #! Returns a stream reader for the object's data at the given location
    private Qore::StreamReader getStreamReaderImpl(string scheme, string location, *hash<auto> opts) {
        return new StreamReader(getBinaryStreamImpl(scheme, location, opts), opts.encoding);
    }

    #! Returns an input stream for the object's data at the given location
    /** @note the read is performed before the stream is returned, so that an error reaches the caller directly
        rather than silently terminating the stream
    */
    private Qore::InputStream getBinaryStreamImpl(string scheme, string location, *hash<auto> opts) {
        return new BinaryInputStream(download(location, opts));
    }

    #! Writes data to an object at the given location
    private writeFileImpl(string scheme, string location, data contents, *hash<auto> opts) {
        if (opts.encoding && contents.typeCode() == NT_STRING && opts.encoding != contents.encoding()) {
            contents = convert_encoding(contents, opts.encoding);
        }
        hash<auto> t = AwsS3FileLocationHandler::split(location, opts);
        prov.put(t.bucket, t.key, binary(contents), AwsS3FileLocationHandler::getHeaders(t.key, opts));
    }

    #! Returns an output stream that uploads what is written to it through an S3 multipart upload
    /** The upload runs in the background as the data is written, so an object larger than memory can be
        written through this stream.  Only when the whole object turns out to fit in a single part is a plain
        single-request upload used instead, which saves two round trips on a small object.

        @param scheme the scheme being used
        @param location the location string without the scheme
        @param opts write options

        @return an output stream for writing to the object

        @note
        - OutputStream::close() must be called to close the stream after writing terminates, or the upload
          never completes and the background thread is held open
        - call OutputStreamWrapper::waitForIo() to be sure the data reached S3; it rethrows any error the
          upload raised
    */
    private FileLocationHandler::OutputStreamWrapper getOutputStreamImpl(string scheme, string location,
            *hash<auto> opts) {
        hash<auto> t = AwsS3FileLocationHandler::split(location, opts);
        hash<auto> hdr = AwsS3FileLocationHandler::getHeaders(t.key, opts);

        StreamPipe pipe(sync_close: False);

        WaitGroup io_counter();
        io_counter.add();
        FileLocationHandler::OutputStreamWrapper rv(pipe.getOutputStream(), io_counter);

        background sub () {
            # NOTE: done() must fire after the catch block records the error, so that waitForIo() observes the
            # error before the wait group drops to zero
            on_exit io_counter.done();
            try {
                prov.putStream(t.bucket, t.key, pipe.getInputStream(), hdr);
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

    #! Downloads the object at the given location
    private binary download(string location, *hash<auto> opts) {
        hash<auto> t = AwsS3FileLocationHandler::split(location, opts);
        return prov.get(t.bucket, t.key, opts.version_id);
    }

    #! Splits a location into a bucket and a key
    /** S3 has no directory structure: a key is a flat string in which \c "/" is an ordinary character, so the
        bucket cannot be derived from the key and has to be named.  It is taken from the \c container option
        when one is given, and otherwise from the first segment of the location.

        @param location the location string without the scheme
        @param opts the location options

        @return a hash with the \c bucket and the \c key

        @throw LOCATION-ERROR the location names no bucket, or no key within it
    */
    static hash<auto> split(string location, *hash<auto> opts) {
        # a leading separator is accepted and ignored, since a location is otherwise written without one
        location =~ s/^\///;
        if (opts.container.val()) {
            if (!location.val()) {
                throw "LOCATION-ERROR", sprintf("the Amazon S3 location names bucket %y but no object key "
                    "within it", opts.container);
            }
            return {"bucket": opts.container, "key": location};
        }
        int i = location.find("/");
        if (i <= 0 || i == location.length() - 1) {
            throw "LOCATION-ERROR", sprintf("Amazon S3 file location %y does not name both a bucket and an "
                "object key; use \"<bucket>/<key>\", or give the bucket in the \"container\" option and the "
                "key as the location", location);
        }
        return {
            "bucket": location.substr(0, i),
            "key": location.substr(i + 1),
        };
    }

    #! Returns the request headers for a write
    static hash<auto> getHeaders(string key, *hash<auto> opts) {
        hash<auto> hdr = {
            "Content-Type": opts.content_type ?? (get_mime_type_from_ext(key) ?? MimeTypeOctetStream),
        };
        if (opts.storage_class.val()) {
            hdr."x-amz-storage-class" = opts.storage_class;
        }
        return hdr;
    }
}

#! Gives the file location handler access to the module's S3 request plumbing
/** The request plumbing -- signing, retries, XML error unpacking, and the multipart upload -- is private to
    the provider hierarchy, and the file location handler is not part of it.  Rather than duplicate any of it,
    this exposes exactly the three operations the handler needs.

    @since AwsS3DataProvider 1.0
*/
public class AwsS3FileTransferProvider inherits AwsS3DataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "file-transfer",
            "type": "AwsS3FileTransferProvider",
            "constructor_options": ConstructorOptions,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object from an S3 client
    /** @param rest the connection's own client, already authenticated
        @param path_style whether the bucket is addressed as the first path segment rather than as a
            subdomain of the endpoint
    */
    constructor(AwsRestClient::AwsRestClient rest, bool path_style = True)
            : AwsS3DataProviderBase(rest, path_style) {
    }

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the content of an object
    /** @param bucket the bucket holding the object
        @param key the object key
        @param version_id a specific version to read, if any

        @return the object content
    */
    binary get(string bucket, string key, *string version_id) {
        *hash<auto> query = version_id.val() ? {"versionId": version_id} : NOTHING;
        hash<auto> resp = doS3Request("GET", bucket, key, query);
        return resp.body.typeCode() == NT_BINARY ? resp.body : binary(resp.body ?? "");
    }

    #! Stores content that is already in memory, using a multipart upload when it is large
    put(string bucket, string key, binary content, hash<auto> hdr) {
        uploadContent(bucket, key, content, hdr);
    }

    #! Stores the content of a stream, sending each part as it is read
    putStream(string bucket, string key, Qore::InputStream is, hash<auto> hdr) {
        uploadMultipart(bucket, key, is, hdr);
    }
}
}
