# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AwsS3ObjectDataProviders.qc object-level action providers for the AwsS3DataProvider module

/*  AwsS3ObjectDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Lists the objects in a bucket
/** Supports the standard S3 continuation-token pagination; a truncated response carries the token needed to
    retrieve the following page
*/
public class AwsS3ListObjectsDataProvider inherits AwsS3DataProviderBase {
    public {
        #! Search options for reading a bucket as a record source
        /** These are the arguments Amazon S3 can apply itself.  A \c where clause beyond them is evaluated
            over the records as they arrive, because S3 has no query language to push it into: a listing can be
            narrowed by key prefix and grouped by delimiter, and that is all.
        */
        const SearchOptions = {
            "bucket": <DataProviderOptionInfo>{
                "display_name": "Bucket",
                "short_desc": "The bucket to read",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The bucket to read objects from",
                "required": True,
                "example_value": "my-application-data",
                "ref_data": "buckets",
                "preselected": True,
            },
            "prefix": <DataProviderOptionInfo>{
                "display_name": "Prefix",
                "short_desc": "Read only keys beginning with this string",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Read only keys beginning with this string.\n\n"
                    "The prefix is applied by Amazon S3 rather than after the listing arrives, so it reduces "
                    "what is transferred and paid for, not only what is examined",
                "example_value": "invoices/2026/",
                "preselected": True,
            },
            "delimiter": <DataProviderOptionInfo>{
                "display_name": "Delimiter",
                "short_desc": "Group keys sharing a prefix up to this character",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Group keys that share a prefix up to the first occurrence of this character; set to "
                    "`/` together with `prefix` to read one level of a key hierarchy rather than every key "
                    "below it",
                "example_value": "/",
            },
            "start_after": <DataProviderOptionInfo>{
                "display_name": "Start After",
                "short_desc": "Begin reading after this key",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Begin reading after this key; Amazon S3 returns keys in lexical order, so this "
                    "resumes a walk from where an earlier one stopped",
            },
            "page_size": <DataProviderOptionInfo>{
                "display_name": "Page Size",
                "short_desc": "How many keys to fetch per request",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "How many keys to fetch per request; Amazon S3 caps this at `1000`.\n\n"
                    "This affects how the walk is split into requests, not how many records are returned - "
                    "the iterator fetches further pages as they are needed",
                "default_value": MaxKeys,
            },
        };

        const ProviderInfo = <DataProviderInfo>{
            "name": "list-objects",
            "type": "AwsS3ListObjectsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
            # the action and the record source are the same provider: the action answers one page with an
            # explicit continuation token, while the record source walks the whole listing itself
            "supports_read": True,
            "has_record": True,
            "search_options": SearchOptions,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = AwsS3ListObjectsRequestDataType;
        const ResponseType = AwsS3ListObjectsResponseDataType;
    }

    constructor(AwsRestClient::AwsRestClient rest, bool path_style = True)
            : AwsS3DataProviderBase(rest, path_style) {
    }
    constructor(*hash<auto> options) : AwsS3DataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int max_keys = req.max_keys ?? DefaultMaxKeys;
        if (max_keys > MaxKeys) {
            max_keys = MaxKeys;
        }
        # NOTE: "hash<auto!>", not "hash<auto>": a literal whose values are all integers narrows to
        # hash<string, int> at runtime, and every option added below is a string -- so setting any of them
        # raised RUNTIME-TYPE-ERROR and no listing with a prefix, delimiter, start_after, or continuation
        # token could succeed
        hash<auto!> query = {
            "list-type": 2,
            "max-keys": max_keys,
        };
        if (req.prefix.val()) {
            query.prefix = req.prefix;
        }
        if (req.delimiter.val()) {
            query.delimiter = req.delimiter;
        }
        if (req.start_after.val()) {
            query."start-after" = req.start_after;
        }
        if (req.continuation_token.val()) {
            query."continuation-token" = req.continuation_token;
        }

        hash<auto> resp = doS3Request("GET", req.bucket, NOTHING, query);
        *hash<auto> doc = AwsS3DataProviderBase::parseXml(resp.body).ListBucketResult;

        # NOTE: built with an explicit loop rather than "map"; the hash-map form produces a single hash rather
        # than one hash per element, and the list form is ambiguous to the parser in a hash value position
        list<auto> objects = ();
        foreach hash<auto> obj in (AwsS3DataProviderBase::forceList(doc.Contents)) {
            objects += {
                "key": obj.Key,
                "size": obj.Size.toInt(),
                "last_modified": obj.LastModified.val() ? date(obj.LastModified) : NOTHING,
                "etag": AwsS3ListObjectsDataProvider::stripEtag(obj.ETag),
                "storage_class": obj.StorageClass,
            };
        }

        return {
            "objects": objects,
            # NOTE: the map expression must be parenthesized here; its own comma would otherwise terminate this
            # hash entry
            "prefixes": (map $1.Prefix, AwsS3DataProviderBase::forceList(doc.CommonPrefixes)),
            "truncated": doc.IsTruncated == "true" || doc.IsTruncated === True,
            "next_continuation_token": doc.NextContinuationToken,
            "key_count": doc.KeyCount.val() ? doc.KeyCount.toInt() : NOTHING,
        };
    }

    #! Returns the record type for a bucket listing
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return AwsS3ObjectRecordDataType.getFields();
    }

    #! Reads a bucket as a record source, walking the whole listing rather than one page of it
    /** The \c list-objects action answers one page and hands back a continuation token, which is right for a
        single API call; a record source must not make its consumer do the paginating, so this returns an
        iterator that fetches each page as it is reached.

        @param where_cond any further conditions, evaluated over the records as they arrive
        @param search_options the search options

        @return an iterator over every matching object

        @throw SEARCH-OPTION-ERROR no bucket was given
    */
    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        *string bucket = search_options.bucket;
        if (!bucket.val()) {
            throw "SEARCH-OPTION-ERROR", "cannot read objects: no bucket was given; set the 'bucket' search "
                "option to the bucket to read";
        }
        checkAddressing(bucket);

        int page_size = search_options.page_size ?? MaxKeys;
        if (page_size > MaxKeys) {
            page_size = MaxKeys;
        }
        # NOTE: "hash<auto!>"; the string options below cannot go into an int-narrowed literal
        hash<auto!> query = {
            "list-type": 2,
            "max-keys": page_size,
        };
        if (search_options.prefix.val()) {
            query.prefix = search_options.prefix;
        }
        if (search_options.delimiter.val()) {
            query.delimiter = search_options.delimiter;
        }
        if (search_options.start_after.val()) {
            query."start-after" = search_options.start_after;
        }

        # the request plumbing is private to the provider hierarchy, so the iterator is given a closure
        # rather than the provider itself
        code<hash<auto>(hash<auto> q)> fetch = hash<auto> sub (hash<auto> q) {
            return doS3Request("GET", bucket, NOTHING, q);
        };
        return new DefaultRecordIterator<hash<auto>>(
            new AwsS3ObjectPageIterator(fetch, bucket, query), where_cond, search_options,
            getRecordTypeImpl(search_options));
    }

    #! Removes the quotes S3 wraps around entity tags
    /** S3 returns the ETag as a quoted string, quotes included; leaving them in place means every caller that
        compares or stores the value has to strip them

        @param etag the raw value from the response

        @return the value without surrounding quotes
    */
    static *string stripEtag(*string etag) {
        if (!etag.val()) {
            return;
        }
        etag =~ s/^"//;
        etag =~ s/"$//;
        return etag;
    }
}

#! Retrieves an object, optionally including its content
public class AwsS3GetObjectDataProvider inherits AwsS3DataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-object",
            "type": "AwsS3GetObjectDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = AwsS3GetObjectRequestDataType;
        const ResponseType = AwsS3GetObjectResponseDataType;
    }

    constructor(AwsRestClient::AwsRestClient rest, bool path_style = True)
            : AwsS3DataProviderBase(rest, path_style) {
    }
    constructor(*hash<auto> options) : AwsS3DataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        *hash<auto> query = req.version_id.val() ? {"versionId": req.version_id} : NOTHING;
        bool include = req.include_content ?? True;

        # HEAD returns every metadata header without transferring the body, which matters for large objects
        hash<auto> resp = doS3Request(include ? "GET" : "HEAD", req.bucket, req.object_key, query);
        hash<auto> rv = AwsS3GetObjectDataProvider::getMetadata(resp, req.object_key);
        if (include) {
            binary content = resp.body.typeCode() == NT_BINARY ? resp.body : binary(resp.body ?? "");
            rv.content = make_base64_string(content);
            if (!exists rv.size) {
                rv.size = content.size();
            }
        }
        return rv;
    }

    #! Extracts object metadata from the response headers
    static hash<auto> getMetadata(hash<auto> resp, string key) {
        return {
            "name": key,
            "mime_type": resp.hdr."content-type",
            "size": resp.hdr."content-length".val() ? resp.hdr."content-length".toInt() : NOTHING,
            "etag": AwsS3ListObjectsDataProvider::stripEtag(resp.hdr.etag),
            "last_modified": resp.hdr."last-modified".val() ? date(resp.hdr."last-modified") : NOTHING,
            "version_id": resp.hdr."x-amz-version-id",
        };
    }
}

#! Downloads an object and returns its content
/** Distinct from @ref AwsS3GetObjectDataProvider in that the content is always retrieved; this is the action
    the file API maps onto
*/
public class AwsS3DownloadFileDataProvider inherits AwsS3DataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "download-file",
            "type": "AwsS3DownloadFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = AwsS3DownloadFileRequestDataType;
        const ResponseType = AwsS3GetObjectResponseDataType;
    }

    constructor(AwsRestClient::AwsRestClient rest, bool path_style = True)
            : AwsS3DataProviderBase(rest, path_style) {
    }
    constructor(*hash<auto> options) : AwsS3DataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        *hash<auto> query = req.version_id.val() ? {"versionId": req.version_id} : NOTHING;
        hash<auto> resp = doS3Request("GET", req.bucket, req.object_key, query);
        binary content = resp.body.typeCode() == NT_BINARY ? resp.body : binary(resp.body ?? "");
        return AwsS3GetObjectDataProvider::getMetadata(resp, req.object_key) + {
            "content": make_base64_string(content),
            "size": content.size(),
        };
    }
}

#! Uploads a file
public class AwsS3UploadFileDataProvider inherits AwsS3DataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "upload-file",
            "type": "AwsS3UploadFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = AwsS3UploadFileRequestDataType;
        const ResponseType = AwsS3ObjectResponseDataType;
    }

    constructor(AwsRestClient::AwsRestClient rest, bool path_style = True)
            : AwsS3DataProviderBase(rest, path_style) {
    }
    constructor(*hash<auto> options) : AwsS3DataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> file = req.file;
        string key = req.object_key.val() ? req.object_key : file.name;
        if (!key.val()) {
            throw "S3-UPLOAD-ERROR", "neither 'object_key' nor a file name was supplied; the object key cannot "
                "be determined";
        }
        binary content = AwsS3UploadFileDataProvider::getContent(file);

        hash<auto> hdr = AwsS3UploadFileDataProvider::getUploadHeaders(req, file.mime_type, key);
        # a large object is sent as a multipart upload rather than one request; Amazon S3 rejects a
        # single-request upload above 5 GB, and a large one has to start again from the beginning if the
        # connection drops
        hash<auto> stored = uploadContent(req.bucket, key, content, hdr);
        return {
            "bucket": req.bucket,
            "object_key": key,
        } + stored;
    }

    #! Returns the binary content of a FileDataType value
    /** The content is declared as base64 but may already have been decoded upstream, so both forms are accepted
    */
    static binary getContent(hash<auto> file) {
        if (file.content.typeCode() == NT_BINARY) {
            return file.content;
        }
        return parse_base64_string(file.content.toString());
    }

    #! Builds the request headers for an upload
    static hash<auto> getUploadHeaders(hash<auto> req, *string mime_type, string key) {
        hash<auto> hdr = {
            # "auto" is the FileDataType placeholder meaning "work it out from the name"
            "Content-Type": (mime_type.val() && mime_type != "auto")
                ? mime_type
                : (get_mime_type_from_ext(key) ?? MimeTypeOctetStream),
        };
        if (req.storage_class.val()) {
            hdr."x-amz-storage-class" = req.storage_class;
        }
        AwsS3UploadFileDataProvider::addEncryptionHeaders(req, \hdr);
        foreach hash<auto> i in (req.metadata.pairIterator()) {
            hdr{"x-amz-meta-" + i.key} = i.value.toString();
        }
        return hdr;
    }

    #! Adds the server-side encryption headers for a write request
    /** @param req the request
        @param hdr a reference to the request headers

        @note A KMS key is only sent alongside a KMS scheme; sending one with \c AES256 is rejected by Amazon
        S3 rather than ignored
    */
    static addEncryptionHeaders(hash<auto> req, reference<hash<auto>> hdr) {
        if (!req.server_side_encryption.val()) {
            return;
        }
        hdr."x-amz-server-side-encryption" = req.server_side_encryption;
        if (req.kms_key_id.val() && req.server_side_encryption =~ /^aws:kms/) {
            hdr."x-amz-server-side-encryption-aws-kms-key-id" = req.kms_key_id;
        }
    }
}

#! Creates an object from text content
public class AwsS3CreateTextFileDataProvider inherits AwsS3DataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-text-file",
            "type": "AwsS3CreateTextFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = AwsS3CreateTextFileRequestDataType;
        const ResponseType = AwsS3ObjectResponseDataType;
    }

    constructor(AwsRestClient::AwsRestClient rest, bool path_style = True)
            : AwsS3DataProviderBase(rest, path_style) {
    }
    constructor(*hash<auto> options) : AwsS3DataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> hdr = {
            "Content-Type": req.content_type ?? "text/plain",
        };
        if (req.storage_class.val()) {
            hdr."x-amz-storage-class" = req.storage_class;
        }
        AwsS3UploadFileDataProvider::addEncryptionHeaders(req, \hdr);
        foreach hash<auto> i in (req.metadata.pairIterator()) {
            hdr{"x-amz-meta-" + i.key} = i.value.toString();
        }

        hash<auto> stored = uploadContent(req.bucket, req.object_key, binary(req.content), hdr);
        return {
            "bucket": req.bucket,
            "object_key": req.object_key,
        } + stored;
    }
}

#! Deletes an object
public class AwsS3DeleteObjectDataProvider inherits AwsS3DataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-object",
            "type": "AwsS3DeleteObjectDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = AwsS3DeleteObjectRequestDataType;
        const ResponseType = AwsS3ObjectResponseDataType;
    }

    constructor(AwsRestClient::AwsRestClient rest, bool path_style = True)
            : AwsS3DataProviderBase(rest, path_style) {
    }
    constructor(*hash<auto> options) : AwsS3DataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        *hash<auto> query = req.version_id.val() ? {"versionId": req.version_id} : NOTHING;
        hash<auto> resp = doS3Request("DELETE", req.bucket, req.object_key, query);
        return {
            "bucket": req.bucket,
            "object_key": req.object_key,
            "version_id": resp.hdr."x-amz-version-id",
        };
    }
}

#! Copies an object
public class AwsS3CopyObjectDataProvider inherits AwsS3DataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "copy-object",
            "type": "AwsS3CopyObjectDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = AwsS3CopyObjectRequestDataType;
        const ResponseType = AwsS3ObjectResponseDataType;
    }

    constructor(AwsRestClient::AwsRestClient rest, bool path_style = True)
            : AwsS3DataProviderBase(rest, path_style) {
    }
    constructor(*hash<auto> options) : AwsS3DataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # the copy source is a single header naming bucket and key, URI-encoded except for the separators
        hash<auto> hdr = {
            "x-amz-copy-source": "/" + req.source_bucket + "/"
                + (foldl $1 + "/" + $2, (map encode_url($1, True), req.source_key.split("/"))),
        };
        if (req.storage_class.val()) {
            hdr."x-amz-storage-class" = req.storage_class;
        }

        hash<auto> resp = doS3Request("PUT", req.bucket, req.object_key, NOTHING, NOTHING, hdr);
        # a copy can fail with a 200 status and an error document in the body, so the result has to be inspected
        # rather than trusted
        *hash<auto> doc = AwsS3DataProviderBase::parseXml(resp.body);
        if (doc.Error.Code.val()) {
            throw "S3-API-ERROR", sprintf("copy %s/%s to %s/%s failed: %s: %s", req.source_bucket, req.source_key,
                req.bucket, req.object_key, doc.Error.Code, doc.Error.Message);
        }
        return {
            "bucket": req.bucket,
            "object_key": req.object_key,
            "etag": AwsS3ListObjectsDataProvider::stripEtag(doc.CopyObjectResult.ETag),
            "version_id": resp.hdr."x-amz-version-id",
        };
    }
}
}
