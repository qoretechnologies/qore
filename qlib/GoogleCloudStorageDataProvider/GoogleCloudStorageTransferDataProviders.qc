# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GoogleCloudStorageDataProvider module definition

/** GoogleCloudStorageTransferDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the GoogleCloudStorageDataProvider module
public namespace GoogleCloudStorageDataProvider {
#! Request type for uploading an object
public class GoogleCloudStorageUploadObjectRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "bucket": {
                "display_name": "Bucket",
                "short_desc": "The bucket to upload into",
                "desc": "The name of the bucket to upload the object into",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "my-bucket",
                "ref_data": "buckets",
            },
            "file": {
                "display_name": "File",
                "short_desc": "The content to upload",
                "desc": "The content to upload, including the file name, the content, and the MIME "
                    "type",
                "type": FileDataType,
                "required": True,
            },
            "object": {
                "display_name": "Object Name",
                "short_desc": "The object name to write",
                "desc": "The name to store the object under, used verbatim: `/` is an ordinary "
                    "character in a Google Cloud Storage object name, so `reports/2026/q1.csv` is a "
                    "single key and not a path.  When omitted the file's name is used",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "reports/2026/q1.csv",
            },
            "content_type": {
                "display_name": "Content Type",
                "short_desc": "The MIME type to store",
                "desc": "The MIME type to store the object with.  When omitted the file's MIME type is "
                    "used; if that is unset or `auto`, it is derived from the object name, falling "
                    "back to `application/octet-stream`",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "text/csv",
            },
            "cache_control": {
                "display_name": "Cache Control",
                "short_desc": "The Cache-Control header to store",
                "desc": "The `Cache-Control` header to serve the object's content with",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "public, max-age=3600",
            },
            "content_disposition": {
                "display_name": "Content Disposition",
                "short_desc": "The Content-Disposition header to store",
                "desc": "The `Content-Disposition` header to serve the object's content with",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "attachment; filename=\"q1.csv\"",
            },
            "content_encoding": {
                "display_name": "Content Encoding",
                "short_desc": "The Content-Encoding header to store",
                "desc": "The `Content-Encoding` header to serve the object's content with (e.g. "
                    "`gzip`).  This declares how the bytes being uploaded are already encoded; it "
                    "does not compress them",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "gzip",
            },
            "content_language": {
                "display_name": "Content Language",
                "short_desc": "The Content-Language header to store",
                "desc": "The `Content-Language` header to serve the object's content with",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "en-US",
            },
            "storage_class": {
                "display_name": "Storage Class",
                "short_desc": "The storage class to store the object with",
                "desc": "The storage class for the new object: `STANDARD`, `NEARLINE`, `COLDLINE` or "
                    "`ARCHIVE`.  When omitted the bucket's default is used",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "NEARLINE",
            },
            "metadata": {
                "display_name": "Custom Metadata",
                "short_desc": "User-defined metadata to store",
                "desc": "User-defined metadata to store on the object as key/value pairs",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "custom_time": {
                "display_name": "Custom Time",
                "short_desc": "The user-specified timestamp to store",
                "desc": "A user-specified date and time on the object, usable in lifecycle rules",
                "type": DateOrNothingType,
            },
            "if_generation_match": {
                "display_name": "If Generation Match",
                "short_desc": "Only write if the live generation matches",
                "desc": "Only perform the write if the object's current generation matches this "
                    "value; pass `0` to write only if the object does not exist yet, which makes the "
                    "upload a safe create rather than an overwrite",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleCloudStorageUploadObjectRequestDataType =
    new GoogleCloudStorageUploadObjectRequestDataType();

#! Data provider for uploading an object
/** Small content with no metadata to set is sent in a single \c uploadType=media request.  Anything
    larger, or any upload that also sets metadata, uses a resumable upload: the initiating request
    carries the metadata as JSON and returns a session URI, and the content follows in chunks, each of
    which Google Cloud Storage commits as it arrives.  That means a large upload does not have to
    start again from the beginning if a chunk fails, and it is the only shape that can set metadata and
    content in one operation.
*/
public class GoogleCloudStorageUploadObjectDataProvider inherits GoogleCloudStorageDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "upload-object",
            "type": "GoogleCloudStorageUploadObjectDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = GoogleCloudStorageUploadObjectRequestDataType;
        const ResponseType = GoogleCloudStorageObjectDataType;
    }

    constructor(GoogleCloudStorageRestClient::GoogleCloudStorageRestClientIo rest, hash<GcsAuthContext> auth)
            : GoogleCloudStorageDataProviderBase(rest, auth) {}
    constructor(*hash<auto> options) : GoogleCloudStorageDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> file = req.file;
        # the object name is authoritative when given: the file API passes the whole location path in
        # "object" while the FileDataType value it builds carries only the last segment in "name", so
        # preferring "name" would silently write the wrong key for "reports/2026/q1.csv"
        string object = req.object.val() ? req.object : (file.name ?? "");
        if (!object.val()) {
            throw "GCS-UPLOAD-ERROR", sprintf("neither 'object' nor a file name was supplied, so the "
                "object name to write to bucket %y cannot be determined", req.bucket);
        }
        binary content = GoogleCloudStorageUploadObjectDataProvider::getContent(file);
        string content_type = GoogleCloudStorageUploadObjectDataProvider::getContentType(req, file, object);

        hash<auto> metadata = GoogleCloudStorageObjectUtil::buildMetadataBody(
            req + {"content_type": content_type});
        if (req.storage_class.val()) {
            metadata.storageClass = req.storage_class;
        }

        # only "contentType" is set, which a single-request media upload carries in its Content-Type
        # header anyway, so no metadata body is needed
        bool metadata_only_content_type = (keys metadata) == ("contentType",);

        if (content.size() <= ResumableUploadThreshold && metadata_only_content_type) {
            return uploadSingleRequest(req, object, content, content_type);
        }
        return uploadResumable(req, object, content, content_type, metadata);
    }

    #! Uploads content in a single \c uploadType=media request
    private hash<auto> uploadSingleRequest(hash<auto> req, string object, binary content,
            string content_type) {
        string path = sprintf("%s/b/%s/o", GoogleCloudStorageRestClient::GcsUploadPrefix,
            encode_url(req.bucket, True));
        # the object name travels as a query parameter here and is url-encoded strictly by addQuery(),
        # so "/" in it reaches the service as "%2F" and the flat key is written verbatim
        hash<auto> query = {
            "uploadType": "media",
            "name": object,
            "ifGenerationMatch": req.if_generation_match,
        };
        # the bytes must reach the service unchanged; the connection's "json" serialization would
        # base64-encode them into a JSON string and Google Cloud Storage would store that and report
        # success
        hash<auto> resp = doGcsRawRequest("POST", path, query, content, content_type);
        return GoogleCloudStorageObjectUtil::normalizeObject(resp.body);
    }

    #! Uploads content as a resumable upload, sending the metadata first and then the content in chunks
    private hash<auto> uploadResumable(hash<auto> req, string object, binary content,
            string content_type, hash<auto> metadata) {
        string session = startResumableSession(req, object, content, content_type, metadata);
        return uploadResumableChunks(session, content, content_type, object, req.bucket);
    }

    #! Initiates a resumable upload session and returns the session path to send the content to
    private string startResumableSession(hash<auto> req, string object, binary content,
            string content_type, hash<auto> metadata) {
        string path = sprintf("%s/b/%s/o", GoogleCloudStorageRestClient::GcsUploadPrefix,
            encode_url(req.bucket, True));
        hash<auto> query = {
            "uploadType": "resumable",
            "ifGenerationMatch": req.if_generation_match,
        };
        # the name goes in the metadata body rather than the query, since this request already carries
        # a JSON body; it is a JSON string value, so it needs no encoding and is stored verbatim
        hash<auto> body = metadata + {"name": object};
        hash<auto> resp = doGcsRequest("POST", path, query, body, {
            "X-Upload-Content-Type": content_type,
            "X-Upload-Content-Length": content.size(),
        });

        *string location = resp.headers.location ?? resp.headers.Location;
        if (!location.val()) {
            throw "GCS-UPLOAD-ERROR", sprintf("the resumable upload session for object %y in bucket %y "
                "returned HTTP %d with no Location header, so there is no session to send the content "
                "to", object, req.bucket, resp.status_code);
        }
        return resumableSessionPath(location, object, req.bucket);
    }

    #! Converts a resumable session URI into a request path for this connection's client
    /** Google Cloud Storage returns the session as an absolute URI on the same host the upload was
        initiated against, so only its path and query are needed.  A different host would have to be
        reached with a different client, and silently sending the request to this connection's host
        instead would address the wrong endpoint — so that case is rejected explicitly rather than
        guessed at.

        @param location the \c Location header value
        @param object the object name, for the error message
        @param bucket the bucket name, for the error message

        @return the path and query to send the content chunks to
    */
    private string resumableSessionPath(string location, string object, string bucket) {
        hash<auto> url = parse_url(location);
        if (!url.path.val()) {
            throw "GCS-UPLOAD-ERROR", sprintf("the resumable upload session URI %y for object %y in "
                "bucket %y has no path", location, object, bucket);
        }
        # a relative Location is already a path on this host
        if (url.host.val()) {
            hash<auto> own = parse_url(rest.getUrl());
            if (url.host != own.host) {
                throw "GCS-UPLOAD-ERROR", sprintf("the resumable upload session for object %y in "
                    "bucket %y is hosted at %y, but this connection is bound to %y; the content "
                    "cannot be sent through this client", object, bucket, url.host, own.host);
            }
        }
        return url.path;
    }

    #! Sends the content of a resumable upload in chunks and returns the stored object
    private hash<auto> uploadResumableChunks(string session, binary content, string content_type,
            string object, string bucket) {
        int total = content.size();
        # a zero-length object still needs one request to commit the (empty) content
        if (!total) {
            hash<auto> resp = putResumableChunk(session, binary(), content_type,
                sprintf("bytes */%d", total), object, bucket);
            return GoogleCloudStorageObjectUtil::normalizeObject(resp.body);
        }

        int offset = 0;
        while (offset < total) {
            int len = total - offset;
            if (len > ResumableChunkSize) {
                len = ResumableChunkSize;
            }
            binary chunk = content.substr(offset, len);
            string range = sprintf("bytes %d-%d/%d", offset, offset + len - 1, total);
            hash<auto> resp = putResumableChunk(session, chunk, content_type, range, object, bucket);
            offset += len;
            if (offset >= total) {
                return GoogleCloudStorageObjectUtil::normalizeObject(resp.body);
            }
        }
        # unreachable: the loop returns on the final chunk
        throw "GCS-UPLOAD-ERROR", sprintf("the resumable upload of object %y in bucket %y sent all %d "
            "byte(s) without receiving a final response", object, bucket, total);
    }

    #! Sends one chunk of a resumable upload, treating HTTP 308 as the expected "keep going" answer
    /** Google Cloud Storage answers a chunk that does not complete the upload with HTTP 308
        (Resume Incomplete).  That is a success for this protocol, but it is an error status as far as
        the REST client is concerned, so it arrives as an exception and is recognized here.
    */
    private hash<auto> putResumableChunk(string session, binary chunk, string content_type,
            string range, string object, string bucket) {
        try {
            return doGcsRawRequest("PUT", session, NOTHING, chunk, content_type, {
                "Content-Range": range,
            });
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "REST-RESPONSE-ERROR" && ex.arg.status_code == 308) {
                debug("GCS resumable chunk %y accepted (HTTP 308 Resume Incomplete)", range);
                return ex.arg;
            }
            rethrow;
        }
    }

    #! Returns the binary content of a FileDataType value
    /** The content is declared as base64 but may already have been decoded upstream, so both forms
        are accepted.
    */
    static binary getContent(hash<auto> file) {
        if (file.content.typeCode() == NT_BINARY) {
            return file.content;
        }
        if (!exists file.content) {
            return binary();
        }
        return parse_base64_string(file.content.toString());
    }

    #! Returns the content type to store the object with
    /** \c "auto" is the @ref DataProvider::FileDataType "FileDataType" placeholder meaning "work it
        out from the name", so it is never sent as a MIME type.
    */
    static string getContentType(hash<auto> req, hash<auto> file, string object) {
        foreach *string candidate in ((req.content_type, file.mime_type)) {
            if (candidate.val() && candidate != "auto") {
                return candidate;
            }
        }
        return get_mime_type_from_ext(object) ?? MimeTypeOctetStream;
    }
}

#! Request type for downloading an object
public class GoogleCloudStorageDownloadObjectRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "bucket": {
                "display_name": "Bucket",
                "short_desc": "The bucket holding the object",
                "desc": "The name of the bucket holding the object",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "my-bucket",
                "ref_data": "buckets",
            },
            "object": {
                "display_name": "Object Name",
                "short_desc": "The object's name",
                "desc": "The name of the object to download, used verbatim: `/` is an ordinary "
                    "character in a Google Cloud Storage object name, so the value is not split or "
                    "normalized",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "reports/2026/q1.csv",
            },
            "generation": {
                "display_name": "Generation",
                "short_desc": "A specific content generation to download",
                "desc": "The content generation to download; when omitted the live version is used",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleCloudStorageDownloadObjectRequestDataType =
    new GoogleCloudStorageDownloadObjectRequestDataType();

#! Data provider for downloading an object's content
/** The content is returned as raw bytes in the \c content field, which is what the application's file
    API declares; it is never base64-encoded and never passed through a text codec.
*/
public class GoogleCloudStorageDownloadObjectDataProvider inherits GoogleCloudStorageDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "download-object",
            "type": "GoogleCloudStorageDownloadObjectDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = GoogleCloudStorageDownloadObjectRequestDataType;
        const ResponseType = GoogleCloudStorageDownloadResultDataType;
    }

    constructor(GoogleCloudStorageRestClient::GoogleCloudStorageRestClientIo rest, hash<GcsAuthContext> auth)
            : GoogleCloudStorageDataProviderBase(rest, auth) {}
    constructor(*hash<auto> options) : GoogleCloudStorageDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> resp = doGcsRequest("GET",
            GoogleCloudStorageObjectUtil::objectPath(req.bucket, req.object), {
                "alt": "media",
                "generation": req.generation,
            }, NOTHING, {
                # ask for the stored bytes rather than a transfer-encoded form, so that what arrives
                # is what is stored; Google Cloud Storage otherwise may serve a gzip-stored object
                # decompressed, which would not match the stored MD5
                "Accept-Encoding": "identity",
            });

        binary content = GoogleCloudStorageDownloadObjectDataProvider::getRawBody(resp);
        hash<auto> hashes = GoogleCloudStorageDownloadObjectDataProvider::parseGoogHash(
            resp.headers."x-goog-hash");

        hash<auto!> rv = {
            "name": req.object,
            "bucket": req.bucket,
            "content": content,
            "content_type": resp.headers."content-type",
            "size": content.size(),
            "etag": resp.headers.etag,
        } + hashes;
        *string generation = resp.headers."x-goog-generation";
        if (generation.val()) {
            rv.generation = generation.toInt();
        }
        return rv;
    }

    #! Returns the response body exactly as it arrived, before any codec ran
    /** An object's content type is whatever it was stored with, so a download can carry
        \c application/json, \c text/csv or anything else, and the REST client's response codec would
        parse or re-encode it: a stored JSON document would come back as a parsed
        @ref hash rather than as bytes, and content that is invalid in its declared type would raise a
        deserialization error and lose the bytes altogether.

        \c response_body_raw is the body before any of that happened, which is the only value a
        byte-exact download can use.  (The client is additionally constructed with
        \c allow_any_response so that a deserialization failure does not throw; see
        @ref GoogleCloudStorageRestClient::GoogleCloudStorageRestClientBase::DefaultOptions.)

        @param resp the REST response

        @return the body as raw bytes
    */
    static binary getRawBody(hash<auto> resp) {
        auto body = resp.response_body_raw ?? resp.body;
        switch (body.typeCode()) {
            case NT_BINARY:
                return body;
            case NT_NOTHING:
                return binary();
            case NT_STRING:
                # a Qore string is a byte buffer with an encoding tag, so this hands back the same
                # bytes; no encoding conversion is performed
                return binary(body);
        }
        # a body that reached here was parsed by a codec despite response_body_raw being available,
        # which would mean the bytes are already gone
        throw "GCS-DOWNLOAD-ERROR", sprintf("the object content arrived as type %y rather than as "
            "bytes, so it cannot be returned unchanged", body.type());
    }

    #! Parses the \c x-goog-hash response header into this module's hash fields
    /** The header is a comma-separated list of \c "name=base64value" pairs, for example
        \c "crc32c=n03xxA==,md5=Ojk9c3dhfxgoKVVHYwFbHQ==".  The values are base64 and are kept that
        way, matching how the JSON API publishes them.

        @param header the \c x-goog-hash header value, if any

        @return a hash with any \c md5_hash and \c crc32c values found
    */
    static hash<auto> parseGoogHash(*string header) {
        hash<auto> rv = {};
        if (!header.val()) {
            return rv;
        }
        foreach string part in (header.split(",")) {
            trim part;
            int pos = part.find("=");
            if (pos < 1) {
                continue;
            }
            string name = part.substr(0, pos);
            string value = part.substr(pos + 1);
            switch (name) {
                case "md5":
                    rv.md5_hash = value;
                    break;
                case "crc32c":
                    rv.crc32c = value;
                    break;
            }
        }
        return rv;
    }
}
}
