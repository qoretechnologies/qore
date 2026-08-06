# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AwsS3ExtendedDataProviders.qc further action providers for the AwsS3DataProvider module

/*  AwsS3ExtendedDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Creates a presigned URL granting one operation on one object
/** A presigned URL is the answer to "how do I give this file to a system that cannot authenticate to AWS?".
    It carries its own authorization in the query string, so the holder needs no credentials of their own, and
    it stops working when it expires.

    @note The URL is signed rather than requested: no call is made to Amazon S3 at all, so this succeeds even
    for an object that does not exist -- the URL simply returns a not-found error when it is used.
*/
public class AwsS3PresignedUrlDataProvider inherits AwsS3DataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "presigned-url",
            "type": "AwsS3PresignedUrlDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = AwsS3PresignedUrlRequestDataType;
        const ResponseType = AwsS3PresignedUrlResponseDataType;
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
        checkAddressing(req.bucket);
        int secs = req.expiration_seconds ?? DefaultPresignedUrlLifetime;

        # NOTE: "hash<auto!>"; the query is built up from values of different types
        hash<auto!> query = {};
        if (req.version_id.val()) {
            query.versionId = req.version_id;
        }
        if (req.response_content_type.val()) {
            query."response-content-type" = req.response_content_type;
        }
        if (req.response_content_disposition.val()) {
            query."response-content-disposition" = req.response_content_disposition;
        }

        string method = req.method ?? "GET";
        string url = rest.getPresignedUrl(method, getPath(req.bucket, req.object_key), seconds(secs),
            query ?: NOTHING);
        return {
            "url": url,
            "method": method,
            "expires": now_us() + seconds(secs),
            "bucket": req.bucket,
            "object_key": req.object_key,
        };
    }
}

#! Deletes several objects in one call
/** Amazon S3 charges per request, so removing a thousand objects one at a time costs a thousand requests;
    this does it in one.

    @note A batch delete does not fail as a whole.  A request can succeed while individual keys are refused --
    by an object-lock retention, or by a policy that allows some prefixes and not others -- so the response
    reports the failures rather than raising, and the caller must inspect them.
*/
public class AwsS3DeleteObjectsDataProvider inherits AwsS3DataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-objects",
            "type": "AwsS3DeleteObjectsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = AwsS3DeleteObjectsRequestDataType;
        const ResponseType = AwsS3DeleteObjectsResponseDataType;
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
        list<auto> object_keys = req.object_keys.typeCode() == NT_LIST ? req.object_keys : (req.object_keys,);
        if (!object_keys) {
            throw "S3-DELETE-ERROR", "no object keys were given to delete";
        }
        if (object_keys.size() > MaxDeleteKeys) {
            throw "S3-DELETE-ERROR", sprintf("Amazon S3 accepts at most %d keys in one batch delete; %d were "
                "given. Split the keys into batches of %d or fewer", MaxDeleteKeys, object_keys.size(),
                MaxDeleteKeys);
        }

        string body = AwsS3DeleteObjectsDataProvider::getDeleteBody(object_keys, req.quiet ?? False);
        hash<auto> hdr = {
            "Content-Type": MimeTypeXml,
            # Amazon S3 requires the integrity check on this operation specifically, and rejects the request
            # outright without it
            "Content-MD5": make_base64_string(digest(CRYPTO_DIGEST_MD5, body)),
        };
        if (req.bypass_governance_retention) {
            hdr."x-amz-bypass-governance-retention" = "true";
        }

        hash<auto> resp = doS3Request("POST", req.bucket, NOTHING, {"delete": True}, body, hdr);
        *hash<auto> doc = AwsS3DataProviderBase::parseXml(resp.body).DeleteResult;

        list<auto> deleted = ();
        foreach hash<auto> d in (AwsS3DataProviderBase::forceList(doc.Deleted)) {
            deleted += {
                "key": d.Key,
                "version_id": d.VersionId,
                "delete_marker": d.DeleteMarker == "true",
            };
        }
        list<auto> errors = ();
        foreach hash<auto> e in (AwsS3DataProviderBase::forceList(doc.Error)) {
            errors += {
                "key": e.Key,
                "code": e.Code,
                "message": e.Message,
            };
        }
        return {
            "deleted": deleted,
            "errors": errors,
            "deleted_count": deleted.size(),
            "error_count": errors.size(),
        };
    }

    #! Builds the XML document naming the keys to delete
    /** @param object_keys the keys to delete
        @param quiet if only failures should be reported

        @return the request body
    */
    static string getDeleteBody(list<auto> object_keys, bool quiet) {
        string body = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Delete>";
        foreach auto key in (object_keys) {
            body += "<Object><Key>" + AwsS3ExtendedXml::escape(key.toString()) + "</Key></Object>";
        }
        if (quiet) {
            body += "<Quiet>true</Quiet>";
        }
        return body + "</Delete>";
    }
}

#! Deletes an empty bucket
public class AwsS3DeleteBucketDataProvider inherits AwsS3DataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-bucket",
            "type": "AwsS3DeleteBucketDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = AwsS3DeleteBucketRequestDataType;
        const ResponseType = AwsS3BucketResponseDataType;
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
        doS3Request("DELETE", req.bucket, NOTHING);
        return {
            "bucket": req.bucket,
            "location": "/" + req.bucket,
        };
    }
}

#! Moves an object within a bucket or between buckets
/** Amazon S3 has no move operation, so this copies the object and then deletes the original.  The copy is
    performed by Amazon S3 itself, so the content is not transferred through this client.

    @note The delete runs only after the copy has been confirmed, so a failure part-way through leaves the
    original in place rather than losing it.
*/
public class AwsS3MoveObjectDataProvider inherits AwsS3DataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "move-object",
            "type": "AwsS3MoveObjectDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = AwsS3MoveObjectRequestDataType;
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
        if (req.source_bucket == req.bucket && req.source_key == req.object_key) {
            throw "S3-MOVE-ERROR", sprintf("the source and the target are the same object (%s/%s); a move to "
                "the same key would copy the object over itself and then delete it",
                req.source_bucket, req.source_key);
        }

        hash<auto!> hdr = {
            "x-amz-copy-source": "/" + req.source_bucket + "/"
                + (foldl $1 + "/" + $2, (map encode_url($1, True), req.source_key.split("/"))),
        };
        if (req.storage_class.val()) {
            hdr."x-amz-storage-class" = req.storage_class;
        }

        hash<auto> resp = doS3Request("PUT", req.bucket, req.object_key, NOTHING, NOTHING, hdr);
        # a copy can fail with a 200 status and an error document in the body, so the result has to be
        # inspected before the original is deleted rather than trusted
        *hash<auto> doc = AwsS3DataProviderBase::parseXml(resp.body);
        if (doc.Error.Code.val()) {
            throw "S3-API-ERROR", sprintf("move %s/%s to %s/%s failed during the copy, so nothing was "
                "deleted: %s: %s", req.source_bucket, req.source_key, req.bucket, req.object_key,
                doc.Error.Code, doc.Error.Message);
        }

        # the copy is confirmed, so the original can be removed
        doS3Request("DELETE", req.source_bucket, req.source_key);
        return {
            "bucket": req.bucket,
            "object_key": req.object_key,
            "etag": AwsS3ListObjectsDataProvider::stripEtag(doc.CopyObjectResult.ETag),
            "version_id": resp.hdr."x-amz-version-id",
        };
    }
}

#! Returns the tags stored with an object
public class AwsS3GetObjectTaggingDataProvider inherits AwsS3DataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-object-tagging",
            "type": "AwsS3GetObjectTaggingDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = AwsS3GetObjectTaggingRequestDataType;
        const ResponseType = AwsS3ObjectTaggingResponseDataType;
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
        hash<auto!> query = {"tagging": True};
        if (req.version_id.val()) {
            query.versionId = req.version_id;
        }
        hash<auto> resp = doS3Request("GET", req.bucket, req.object_key, query);
        *hash<auto> doc = AwsS3DataProviderBase::parseXml(resp.body).Tagging;

        hash<auto> tags = {};
        foreach hash<auto> t in (AwsS3DataProviderBase::forceList(doc.TagSet.Tag)) {
            if (t.Key.val()) {
                tags{t.Key} = t.Value;
            }
        }
        return {
            "bucket": req.bucket,
            "object_key": req.object_key,
            "tags": tags,
            "version_id": resp.hdr."x-amz-version-id" ?? req.version_id,
        };
    }
}

#! Replaces the tags stored with an object
public class AwsS3PutObjectTaggingDataProvider inherits AwsS3DataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "put-object-tagging",
            "type": "AwsS3PutObjectTaggingDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = AwsS3PutObjectTaggingRequestDataType;
        const ResponseType = AwsS3ObjectTaggingResponseDataType;
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
        string body = AwsS3PutObjectTaggingDataProvider::getTaggingBody(req.tags ?? {});
        hash<auto!> query = {"tagging": True};
        if (req.version_id.val()) {
            query.versionId = req.version_id;
        }
        hash<auto> resp = doS3Request("PUT", req.bucket, req.object_key, query, body, {
            "Content-Type": MimeTypeXml,
            # as with a batch delete, Amazon S3 requires the integrity check on this operation
            "Content-MD5": make_base64_string(digest(CRYPTO_DIGEST_MD5, body)),
        });
        return {
            "bucket": req.bucket,
            "object_key": req.object_key,
            "tags": req.tags ?? {},
            "version_id": resp.hdr."x-amz-version-id" ?? req.version_id,
        };
    }

    #! Builds the XML document carrying an object's tag set
    /** @param tags the tags, as a hash of name to value

        @return the request body
    */
    static string getTaggingBody(hash<auto> tags) {
        string body = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Tagging><TagSet>";
        foreach hash<auto> i in (tags.pairIterator()) {
            body += "<Tag><Key>" + AwsS3ExtendedXml::escape(i.key) + "</Key><Value>"
                + AwsS3ExtendedXml::escape(i.value.toString()) + "</Value></Tag>";
        }
        return body + "</TagSet></Tagging>";
    }
}

#! Lists every version of the objects in a bucket
/** On a bucket with versioning enabled, deleting an object does not remove the data: it adds a delete marker
    that hides it.  This lists what is actually there, so an earlier version can be retrieved or a deletion
    undone by removing its marker.
*/
public class AwsS3ListObjectVersionsDataProvider inherits AwsS3DataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-object-versions",
            "type": "AwsS3ListObjectVersionsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = AwsS3ListObjectVersionsRequestDataType;
        const ResponseType = AwsS3ListObjectVersionsResponseDataType;
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
        # NOTE: "hash<auto!>"; the string markers below cannot go into an int-narrowed literal
        hash<auto!> query = {
            "versions": True,
            "max-keys": max_keys,
        };
        if (req.prefix.val()) {
            query.prefix = req.prefix;
        }
        if (req.key_marker.val()) {
            query."key-marker" = req.key_marker;
        }
        if (req.version_id_marker.val()) {
            query."version-id-marker" = req.version_id_marker;
        }

        hash<auto> resp = doS3Request("GET", req.bucket, NOTHING, query);
        *hash<auto> doc = AwsS3DataProviderBase::parseXml(resp.body).ListVersionsResult;

        list<auto> versions = ();
        foreach hash<auto> v in (AwsS3DataProviderBase::forceList(doc.Version)) {
            versions += {
                "key": v.Key,
                "version_id": v.VersionId,
                "is_latest": v.IsLatest == "true",
                "size": v.Size.val() ? v.Size.toInt() : NOTHING,
                "last_modified": v.LastModified.val() ? date(v.LastModified) : NOTHING,
                "etag": AwsS3ListObjectsDataProvider::stripEtag(v.ETag),
                "storage_class": v.StorageClass,
            };
        }
        list<auto> markers = ();
        foreach hash<auto> m in (AwsS3DataProviderBase::forceList(doc.DeleteMarker)) {
            markers += {
                "key": m.Key,
                "version_id": m.VersionId,
                "is_latest": m.IsLatest == "true",
                "last_modified": m.LastModified.val() ? date(m.LastModified) : NOTHING,
            };
        }

        return {
            "versions": versions,
            "delete_markers": markers,
            "truncated": doc.IsTruncated == "true" || doc.IsTruncated === True,
            "next_key_marker": doc.NextKeyMarker,
            "next_version_id_marker": doc.NextVersionIdMarker,
        };
    }
}

#! XML helpers shared by the operations that send documents rather than headers
/** Amazon S3 takes the arguments to a batch delete and to a tag set as an XML document, so the values have to
    be escaped: an object key may contain \c & or \c <, both of which are ordinary characters in a key and
    would otherwise produce a malformed request or, worse, one that means something else.
*/
public class AwsS3ExtendedXml {
    #! Escapes a value for inclusion in XML character data
    /** @param str the value to escape

        @return the escaped value
    */
    static string escape(string str) {
        str = replace(str, "&", "&amp;");
        str = replace(str, "<", "&lt;");
        str = replace(str, ">", "&gt;");
        str = replace(str, "\"", "&quot;");
        return replace(str, "'", "&apos;");
    }
}
}
