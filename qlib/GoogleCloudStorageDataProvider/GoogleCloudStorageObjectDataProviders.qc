# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GoogleCloudStorageDataProvider module definition

/** GoogleCloudStorageObjectDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Static helpers shared by the object providers
public class GoogleCloudStorageObjectUtil {
    public {
        #! The writable metadata fields of an object, mapped from this module's names to the API's
        const WritableFieldMap = {
            "content_type": "contentType",
            "cache_control": "cacheControl",
            "content_disposition": "contentDisposition",
            "content_encoding": "contentEncoding",
            "content_language": "contentLanguage",
            "metadata": "metadata",
            "custom_time": "customTime",
        };

        #! The UTC time zone, used to normalize dates before they are written as RFC 3339
        static TimeZone UtcZone("UTC");
    }

    #! Converts a Google Cloud Storage Object resource to this module's object metadata hash
    /** \c size, \c generation and \c metageneration arrive as strings because they are 64-bit
        values, and the timestamps as RFC 3339 strings; all are converted to %Qore types here.  The
        \c md5Hash and \c crc32c values are deliberately left base64-encoded, which is how Google
        Cloud Storage publishes them and how they must be sent back on a conditional write.

        @param obj the Object resource as returned by the JSON API

        @return the object metadata hash matching @ref GoogleCloudStorageObjectDataType
    */
    static hash<auto> normalizeObject(hash<auto> obj) {
        hash<auto!> rv = {
            "name": obj.name,
            "bucket": obj.bucket,
            "id": obj.id,
            "content_type": obj.contentType,
            "storage_class": obj.storageClass,
            "md5_hash": obj.md5Hash,
            "crc32c": obj.crc32c,
            "etag": obj.etag,
            "cache_control": obj.cacheControl,
            "content_disposition": obj.contentDisposition,
            "content_encoding": obj.contentEncoding,
            "content_language": obj.contentLanguage,
            "metadata": obj.metadata,
            "kms_key_name": obj.kmsKeyName,
            "media_link": obj.mediaLink,
            "self_link": obj.selfLink,
        };
        foreach hash<auto> i in ({"size": "size", "generation": "generation",
                "metageneration": "metageneration"}.pairIterator()) {
            if (exists obj{i.key}) {
                rv{i.value} = obj{i.key}.toInt();
            }
        }
        foreach hash<auto> i in ({"timeCreated": "time_created", "updated": "updated",
                "timeDeleted": "time_deleted", "customTime": "custom_time"}.pairIterator()) {
            if (obj{i.key}.val()) {
                rv{i.value} = date(obj{i.key});
            }
        }
        return rv;
    }

    #! Returns the JSON API path addressing one object in a bucket
    /** @param bucket the bucket name
        @param object the object name, used verbatim

        @return the path \c "/storage/v1/b/BUCKET/o/OBJECT" with both components encoded

        @note the object name is encoded as a *single* path segment, so a \c "/" in it becomes
        \c "%2F" and the flat key space is addressed exactly; see
        @ref GoogleCloudStorageRestClient::gcsEncodeObject() "gcsEncodeObject()"
    */
    static string objectPath(string bucket, string object) {
        return sprintf("%s/b/%s/o/%s", GoogleCloudStorageRestClient::GcsApiPrefix,
            encode_url(bucket, True), GoogleCloudStorageRestClient::gcsEncodeObject(object));
    }

    #! Builds an Object resource body from this module's writable field names
    /** @param req the request hash; only the keys named in @ref WritableFieldMap are considered, so
        the request's own \c bucket and \c object never reach the body

        @return the API-shaped body, which is empty when the request set none of the writable fields
    */
    static hash<auto> buildMetadataBody(hash<auto> req) {
        hash<auto> body = {};
        foreach hash<auto> i in (WritableFieldMap.pairIterator()) {
            if (!exists req{i.key}) {
                continue;
            }
            auto value = req{i.key};
            # a date has to go back as RFC 3339 rather than as a Qore date
            body{i.value} = value.typeCode() == NT_DATE
                ? GoogleCloudStorageObjectUtil::formatRfc3339(value)
                : value;
        }
        return body;
    }

    #! Formats a date as the UTC RFC 3339 form Google Cloud Storage publishes and accepts
    /** The date is converted to UTC first and the zone is then written as a literal \c "Z".  Both
        steps matter: \c format() with a \c "Z" specifier writes the *offset* (\c "+02:00" for a
        date in a positive zone), so writing \c "Z" onto an unconverted local date would claim UTC
        while carrying local wall-clock time — an error of exactly the zone's offset, silently
        accepted by the service.

        @param d the date to format

        @return the date as \c "YYYY-MM-DDTHH:MM:SSZ" in UTC
    */
    static string formatRfc3339(date d) {
        return UtcZone.date(d).format("YYYY-MM-DDTHH:mm:SS") + "Z";
    }
}

#! Request type for listing the objects in a bucket
public class GoogleCloudStorageListObjectsRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "bucket": {
                "display_name": "Bucket",
                "short_desc": "The bucket to list",
                "desc": "The name of the bucket whose objects should be listed",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "my-bucket",
                "ref_data": "buckets",
            },
            "prefix": {
                "display_name": "Prefix",
                "short_desc": "List only objects whose name begins with this string",
                "desc": "List only objects whose name begins with this string.  Because object names "
                    "are a flat key space, a prefix is how a subset is selected; it is matched "
                    "literally and byte for byte",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "reports/2026/",
            },
            "delimiter": {
                "display_name": "Delimiter",
                "short_desc": "Roll up names containing this delimiter into common prefixes",
                "desc": "Objects whose name contains this string after `prefix` are omitted from "
                    "`objects` and rolled up into `prefixes` instead.  Setting it to `/` together "
                    "with `prefix` makes a bucket browsable one level at a time, as though it had "
                    "folders — which it does not",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "/",
            },
            "include_trailing_delimiter": {
                "display_name": "Include Trailing Delimiter",
                "short_desc": "Also return objects whose name ends with the delimiter",
                "desc": "When set, objects whose name ends with `delimiter` are returned in `objects` "
                    "as well as being rolled up into `prefixes`; this is how the zero-byte folder "
                    "placeholder objects written by some tools become visible",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "start_offset": {
                "display_name": "Start Offset",
                "short_desc": "List only objects at or after this name",
                "desc": "List only objects whose name is lexicographically greater than or equal to "
                    "this value",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "reports/2026/m",
            },
            "end_offset": {
                "display_name": "End Offset",
                "short_desc": "List only objects before this name",
                "desc": "List only objects whose name is lexicographically less than this value",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "reports/2026/n",
            },
            "versions": {
                "display_name": "Include Versions",
                "short_desc": "Include noncurrent object generations",
                "desc": "Include noncurrent generations of objects, which exist only in a bucket with "
                    "versioning enabled; each generation is returned as a separate entry",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "max_results": {
                "display_name": "Max Results",
                "short_desc": "Maximum objects to return per page",
                "desc": "The maximum number of objects to return in one page (max `1000`)",
                "type": AbstractDataProviderTypeMap."*int",
                "example_value": 100,
            },
            "page_token": {
                "display_name": "Page Token",
                "short_desc": "Token identifying the page to return",
                "desc": "The `next_page_token` from a previous call, to retrieve the following page",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleCloudStorageListObjectsRequestDataType =
    new GoogleCloudStorageListObjectsRequestDataType();

#! Data provider for listing the objects in a bucket
public class GoogleCloudStorageListObjectsDataProvider inherits GoogleCloudStorageDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-objects",
            "type": "GoogleCloudStorageListObjectsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = GoogleCloudStorageListObjectsRequestDataType;
        const ResponseType = GoogleCloudStorageObjectListDataType;
    }

    constructor(GoogleCloudStorageRestClient::GoogleCloudStorageRestClientIo rest, hash<GcsAuthContext> auth)
            : GoogleCloudStorageDataProviderBase(rest, auth) {}
    constructor(*hash<auto> options) : GoogleCloudStorageDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string path = sprintf("%s/b/%s/o", GoogleCloudStorageRestClient::GcsApiPrefix,
            encode_url(req.bucket, True));
        hash<auto> query = {
            "projection": "noAcl",
            "prefix": req.prefix,
            "delimiter": req.delimiter,
            "includeTrailingDelimiter": exists req.include_trailing_delimiter
                ? (req.include_trailing_delimiter ? "true" : "false")
                : NOTHING,
            "startOffset": req.start_offset,
            "endOffset": req.end_offset,
            "versions": exists req.versions ? (req.versions ? "true" : "false") : NOTHING,
            "maxResults": req.max_results ?? MaxPageSize,
            "pageToken": req.page_token,
        };
        hash<auto> resp = doGcsRequest("GET", path, query);
        return {
            "objects": (map GoogleCloudStorageObjectUtil::normalizeObject($1), resp.body.items ?? ()),
            "prefixes": resp.body.prefixes,
            "next_page_token": resp.body.nextPageToken,
        };
    }
}

#! Request type for retrieving one object's metadata
public class GoogleCloudStorageGetObjectMetadataRequestDataType inherits HashDataType {
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
                "desc": "The name of the object.  Object names are a flat key space in which `/` is "
                    "an ordinary character, so the value is used verbatim: it is not split, "
                    "normalized, or resolved against anything",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "reports/2026/q1.csv",
            },
            "generation": {
                "display_name": "Generation",
                "short_desc": "A specific content generation to address",
                "desc": "The content generation to retrieve; when omitted the live version is used",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleCloudStorageGetObjectMetadataRequestDataType =
    new GoogleCloudStorageGetObjectMetadataRequestDataType();

#! Data provider for retrieving one object's metadata
public class GoogleCloudStorageGetObjectMetadataDataProvider inherits GoogleCloudStorageDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-object-metadata",
            "type": "GoogleCloudStorageGetObjectMetadataDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = GoogleCloudStorageGetObjectMetadataRequestDataType;
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
        hash<auto> resp = doGcsRequest("GET",
            GoogleCloudStorageObjectUtil::objectPath(req.bucket, req.object), {
                "projection": "noAcl",
                "generation": req.generation,
            });
        return GoogleCloudStorageObjectUtil::normalizeObject(resp.body);
    }
}

#! Request type for updating one object's metadata
public class GoogleCloudStorageUpdateObjectMetadataRequestDataType inherits HashDataType {
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
                "desc": "The name of the object, used verbatim",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "reports/2026/q1.csv",
            },
            "content_type": {
                "display_name": "Content Type",
                "short_desc": "The MIME type to set",
                "desc": "The MIME type served with the object's content",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "text/csv",
            },
            "cache_control": {
                "display_name": "Cache Control",
                "short_desc": "The Cache-Control header to set",
                "desc": "The `Cache-Control` header served with the object's content",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "public, max-age=3600",
            },
            "content_disposition": {
                "display_name": "Content Disposition",
                "short_desc": "The Content-Disposition header to set",
                "desc": "The `Content-Disposition` header served with the object's content",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "attachment; filename=\"q1.csv\"",
            },
            "content_encoding": {
                "display_name": "Content Encoding",
                "short_desc": "The Content-Encoding header to set",
                "desc": "The `Content-Encoding` header served with the object's content (e.g. `gzip`)",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "gzip",
            },
            "content_language": {
                "display_name": "Content Language",
                "short_desc": "The Content-Language header to set",
                "desc": "The `Content-Language` header served with the object's content",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "en-US",
            },
            "metadata": {
                "display_name": "Custom Metadata",
                "short_desc": "User-defined metadata to set",
                "desc": "User-defined metadata as key/value pairs.  A `PATCH` merges these with the "
                    "existing metadata; set a key to `null` to remove it",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "custom_time": {
                "display_name": "Custom Time",
                "short_desc": "The user-specified timestamp to set",
                "desc": "A user-specified date and time on the object, usable in lifecycle rules",
                "type": DateOrNothingType,
            },
            "generation": {
                "display_name": "Generation",
                "short_desc": "A specific content generation to address",
                "desc": "The content generation whose metadata should be updated; when omitted the "
                    "live version is used",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleCloudStorageUpdateObjectMetadataRequestDataType =
    new GoogleCloudStorageUpdateObjectMetadataRequestDataType();

#! Data provider for updating one object's metadata
public class GoogleCloudStorageUpdateObjectMetadataDataProvider inherits GoogleCloudStorageDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-object-metadata",
            "type": "GoogleCloudStorageUpdateObjectMetadataDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = GoogleCloudStorageUpdateObjectMetadataRequestDataType;
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
        hash<auto> body = GoogleCloudStorageObjectUtil::buildMetadataBody(req);
        if (!body) {
            throw "GCS-UPDATE-ERROR", sprintf("no metadata fields were supplied for object %y in bucket "
                "%y; set at least one of %y", req.object, req.bucket,
                keys GoogleCloudStorageObjectUtil::WritableFieldMap);
        }
        hash<auto> resp = doGcsRequest("PATCH",
            GoogleCloudStorageObjectUtil::objectPath(req.bucket, req.object), {
                "generation": req.generation,
            }, body);
        return GoogleCloudStorageObjectUtil::normalizeObject(resp.body);
    }
}

#! Request type for deleting an object
public class GoogleCloudStorageDeleteObjectRequestDataType inherits HashDataType {
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
                "desc": "The name of the object to delete, used verbatim",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "reports/2026/q1.csv",
            },
            "generation": {
                "display_name": "Generation",
                "short_desc": "A specific content generation to delete",
                "desc": "The content generation to delete; when omitted the live version is deleted.  "
                    "In a bucket with versioning enabled, deleting the live version leaves the "
                    "earlier generations in place, and a specific generation must be named to remove "
                    "it permanently",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleCloudStorageDeleteObjectRequestDataType =
    new GoogleCloudStorageDeleteObjectRequestDataType();

#! Data provider for deleting an object
public class GoogleCloudStorageDeleteObjectDataProvider inherits GoogleCloudStorageDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-object",
            "type": "GoogleCloudStorageDeleteObjectDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = GoogleCloudStorageDeleteObjectRequestDataType;
        const ResponseType = GoogleCloudStorageDeleteResultDataType;
    }

    constructor(GoogleCloudStorageRestClient::GoogleCloudStorageRestClientIo rest, hash<GcsAuthContext> auth)
            : GoogleCloudStorageDataProviderBase(rest, auth) {}
    constructor(*hash<auto> options) : GoogleCloudStorageDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # a successful delete answers HTTP 204 with no body
        doGcsRequest("DELETE", GoogleCloudStorageObjectUtil::objectPath(req.bucket, req.object), {
            "generation": req.generation,
        });
        hash<auto!> rv = {
            "bucket": req.bucket,
            "object": req.object,
            "deleted": True,
        };
        if (exists req.generation) {
            rv.generation = req.generation;
        }
        return rv;
    }
}

#! Request type for copying an object
public class GoogleCloudStorageCopyObjectRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "source_bucket": {
                "display_name": "Source Bucket",
                "short_desc": "The bucket holding the source object",
                "desc": "The name of the bucket holding the object to copy",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "my-bucket",
                "ref_data": "buckets",
            },
            "source_object": {
                "display_name": "Source Object Name",
                "short_desc": "The object to copy",
                "desc": "The name of the object to copy, used verbatim",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "reports/2026/q1.csv",
            },
            "destination_bucket": {
                "display_name": "Destination Bucket",
                "short_desc": "The bucket to copy the object into",
                "desc": "The name of the bucket to copy the object into; may be the same as the source "
                    "bucket",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "my-archive-bucket",
                "ref_data": "buckets",
            },
            "destination_object": {
                "display_name": "Destination Object Name",
                "short_desc": "The name to copy the object to",
                "desc": "The name the copy should have, used verbatim",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "archive/2026/q1.csv",
            },
            "source_generation": {
                "display_name": "Source Generation",
                "short_desc": "A specific source content generation to copy",
                "desc": "The content generation of the source to copy; when omitted the live version "
                    "is copied",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "destination_storage_class": {
                "display_name": "Destination Storage Class",
                "short_desc": "The storage class for the copy",
                "desc": "The storage class the copy should have: `STANDARD`, `NEARLINE`, `COLDLINE` or "
                    "`ARCHIVE`.  When omitted the destination bucket's default is used.  Rewriting an "
                    "object onto itself with a new storage class is how an object's class is changed",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "COLDLINE",
            },
            "content_type": {
                "display_name": "Content Type",
                "short_desc": "The MIME type for the copy",
                "desc": "The MIME type the copy should have; when omitted the source's is kept",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "text/csv",
            },
            "metadata": {
                "display_name": "Custom Metadata",
                "short_desc": "User-defined metadata for the copy",
                "desc": "User-defined metadata for the copy as key/value pairs; when omitted the "
                    "source's metadata is kept",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleCloudStorageCopyObjectRequestDataType =
    new GoogleCloudStorageCopyObjectRequestDataType();

#! Data provider for copying an object
/** Uses the \c rewriteTo method rather than \c copyTo.  A rewrite handles an object of any size and a
    copy between different locations, storage classes or encryption keys, none of which \c copyTo
    supports beyond a limited size; where \c copyTo would work, \c rewriteTo completes in a single
    call and behaves identically.

    A rewrite that does not finish in one call returns \c done \c false and a \c rewriteToken, which is
    passed back to continue; each call performs work and returns immediately, so this is a
    continuation rather than polling.
*/
public class GoogleCloudStorageCopyObjectDataProvider inherits GoogleCloudStorageDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "copy-object",
            "type": "GoogleCloudStorageCopyObjectDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = GoogleCloudStorageCopyObjectRequestDataType;
        const ResponseType = GoogleCloudStorageObjectDataType;

        #! Maximum rewrite continuations before giving up
        /** A rewrite makes guaranteed forward progress, so this is only a backstop against an
            unexpected service response that would otherwise loop forever.
        */
        const MaxRewriteContinuations = 10000;
    }

    constructor(GoogleCloudStorageRestClient::GoogleCloudStorageRestClientIo rest, hash<GcsAuthContext> auth)
            : GoogleCloudStorageDataProviderBase(rest, auth) {}
    constructor(*hash<auto> options) : GoogleCloudStorageDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string path = sprintf("%s/b/%s/o/%s/rewriteTo/b/%s/o/%s",
            GoogleCloudStorageRestClient::GcsApiPrefix,
            encode_url(req.source_bucket, True),
            GoogleCloudStorageRestClient::gcsEncodeObject(req.source_object),
            encode_url(req.destination_bucket, True),
            GoogleCloudStorageRestClient::gcsEncodeObject(req.destination_object));

        hash<auto> body = {};
        if (req.destination_storage_class.val()) {
            body.storageClass = req.destination_storage_class;
        }
        if (req.content_type.val()) {
            body.contentType = req.content_type;
        }
        if (exists req.metadata) {
            body.metadata = req.metadata;
        }

        *string rewrite_token;
        int calls = 0;
        while (True) {
            hash<auto> resp = doGcsRequest("POST", path, {
                "sourceGeneration": req.source_generation,
                "rewriteToken": rewrite_token,
            }, body);
            if (resp.body.done.toBool()) {
                return GoogleCloudStorageObjectUtil::normalizeObject(resp.body.resource);
            }
            rewrite_token = resp.body.rewriteToken;
            if (!rewrite_token.val()) {
                throw "GCS-COPY-ERROR", sprintf("the rewrite of %y in bucket %y to %y in bucket %y "
                    "reported that it was not done but returned no rewriteToken to continue with; "
                    "%y of %y byte(s) were rewritten", req.source_object, req.source_bucket,
                    req.destination_object, req.destination_bucket,
                    resp.body.totalBytesRewritten, resp.body.objectSize);
            }
            if (++calls >= MaxRewriteContinuations) {
                throw "GCS-COPY-ERROR", sprintf("the rewrite of %y in bucket %y to %y in bucket %y did "
                    "not complete in %d continuation(s); %y of %y byte(s) were rewritten",
                    req.source_object, req.source_bucket, req.destination_object,
                    req.destination_bucket, calls, resp.body.totalBytesRewritten,
                    resp.body.objectSize);
            }
            debug("GCS rewrite continuation %d: %y of %y byte(s)", calls,
                resp.body.totalBytesRewritten, resp.body.objectSize);
        }
    }
}

#! Request type for composing objects
public class GoogleCloudStorageComposeRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "bucket": {
                "display_name": "Bucket",
                "short_desc": "The bucket holding the sources and the destination",
                "desc": "The name of the bucket; every source and the destination must be in the same "
                    "bucket, which is a Google Cloud Storage restriction on compose",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "my-bucket",
                "ref_data": "buckets",
            },
            "destination_object": {
                "display_name": "Destination Object Name",
                "short_desc": "The object to write the concatenation to",
                "desc": "The name of the object the sources are concatenated into, used verbatim; it "
                    "may be one of the sources, which is how content is appended",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "reports/2026/all.csv",
            },
            "source_objects": {
                "display_name": "Source Object Names",
                "short_desc": "The objects to concatenate, in order",
                "desc": "The names of the objects to concatenate, in the order they should appear; "
                    "between 1 and 32 names, used verbatim",
                "type": new ListDataType("GoogleCloudStorageComposeSources", SoftStringType),
                "required": True,
                "example_value": ("reports/2026/q1.csv", "reports/2026/q2.csv"),
            },
            "content_type": {
                "display_name": "Content Type",
                "short_desc": "The MIME type for the destination",
                "desc": "The MIME type the composed object should have.  Google Cloud Storage does "
                    "not infer it from the sources, so it should be set explicitly",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "text/csv",
            },
            "metadata": {
                "display_name": "Custom Metadata",
                "short_desc": "User-defined metadata for the destination",
                "desc": "User-defined metadata for the composed object as key/value pairs",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleCloudStorageComposeRequestDataType =
    new GoogleCloudStorageComposeRequestDataType();

#! Data provider for concatenating objects into one object
public class GoogleCloudStorageComposeDataProvider inherits GoogleCloudStorageDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "compose",
            "type": "GoogleCloudStorageComposeDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = GoogleCloudStorageComposeRequestDataType;
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
        list<auto> sources = req.source_objects;
        if (!sources) {
            throw "GCS-COMPOSE-ERROR", sprintf("no source objects were supplied for the composition of "
                "%y in bucket %y", req.destination_object, req.bucket);
        }
        if (sources.size() > MaxComposeSources) {
            throw "GCS-COMPOSE-ERROR", sprintf("%d source objects were supplied for the composition of "
                "%y in bucket %y; Google Cloud Storage composes at most %d sources in one call, so a "
                "larger set has to be composed in stages", sources.size(), req.destination_object,
                req.bucket, MaxComposeSources);
        }

        hash<auto> destination = {};
        if (req.content_type.val()) {
            destination.contentType = req.content_type;
        }
        if (exists req.metadata) {
            destination.metadata = req.metadata;
        }

        # built with a loop rather than "map": when a map expression yields a hash, %Qore merges the
        # results into a single hash instead of collecting them into a list, so every source would
        # collapse onto the one "name" key and only the last would be sent
        list<hash<auto>> source_objects = ();
        foreach auto source in (sources) {
            push source_objects, {"name": source.toString()};
        }

        hash<auto> body = {
            "sourceObjects": source_objects,
        } + (destination ? {"destination": destination} : {});

        string path = sprintf("%s/compose",
            GoogleCloudStorageObjectUtil::objectPath(req.bucket, req.destination_object));
        hash<auto> resp = doGcsRequest("POST", path, NOTHING, body);
        return GoogleCloudStorageObjectUtil::normalizeObject(resp.body);
    }
}
}
