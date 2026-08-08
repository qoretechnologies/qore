# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GoogleCloudStorageDataProvider module definition

/** GoogleCloudStorageDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Google Cloud Storage bucket data type
public class GoogleCloudStorageBucketDataType inherits HashDataType {
    private {
        const Fields = {
            "name": {
                "display_name": "Bucket Name",
                "short_desc": "The name of the bucket",
                "desc": "The name of the bucket, which is globally unique across Google Cloud Storage",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "my-bucket",
            },
            "id": {
                "display_name": "Bucket ID",
                "short_desc": "The ID of the bucket",
                "desc": "The ID of the bucket; for buckets this is the same as the name",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "project_number": {
                "display_name": "Project Number",
                "short_desc": "The project number owning the bucket",
                "desc": "The numeric project number of the project that owns the bucket",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "location": {
                "display_name": "Location",
                "short_desc": "The bucket's location",
                "desc": "The location of the bucket: a region (`US-EAST1`), a dual-region, or a "
                    "multi-region (`US`, `EU`, `ASIA`); object data is stored here and cannot be "
                    "moved after creation",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "US",
            },
            "location_type": {
                "display_name": "Location Type",
                "short_desc": "The kind of location",
                "desc": "The type of location: `region`, `dual-region` or `multi-region`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "storage_class": {
                "display_name": "Storage Class",
                "short_desc": "The bucket's default storage class",
                "desc": "The default storage class for objects created in the bucket: `STANDARD`, "
                    "`NEARLINE`, `COLDLINE`, `ARCHIVE` or `AUTOCLASS`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "versioning_enabled": {
                "display_name": "Versioning Enabled",
                "short_desc": "If object versioning is enabled",
                "desc": "If object versioning is enabled on the bucket, so that overwritten and "
                    "deleted objects are retained as noncurrent generations",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "uniform_bucket_level_access": {
                "display_name": "Uniform Bucket-Level Access",
                "short_desc": "If uniform bucket-level access is enabled",
                "desc": "If uniform bucket-level access is enabled, in which case per-object ACLs are "
                    "disabled and access is governed entirely by IAM",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "requester_pays": {
                "display_name": "Requester Pays",
                "short_desc": "If the requester is billed for access",
                "desc": "If requester pays is enabled, in which case a billing project must be "
                    "supplied with every request (see the `gcp_quota_project_id` option)",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "labels": {
                "display_name": "Labels",
                "short_desc": "User-defined labels on the bucket",
                "desc": "User-defined labels on the bucket as key/value pairs",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "time_created": {
                "display_name": "Created",
                "short_desc": "When the bucket was created",
                "desc": "The date and time the bucket was created",
                "type": DateOrNothingType,
            },
            "updated": {
                "display_name": "Updated",
                "short_desc": "When the bucket was last modified",
                "desc": "The date and time the bucket's metadata was last modified",
                "type": DateOrNothingType,
            },
            "etag": {
                "display_name": "ETag",
                "short_desc": "HTTP 1.1 entity tag for the bucket",
                "desc": "The HTTP 1.1 entity tag for the bucket",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "self_link": {
                "display_name": "Self Link",
                "short_desc": "The URI of the bucket resource",
                "desc": "The URI of the bucket in the Google Cloud Storage JSON API",
                "type": AbstractDataProviderTypeMap."*uri",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleCloudStorageBucketDataType = new GoogleCloudStorageBucketDataType();

#! Google Cloud Storage object metadata data type
public class GoogleCloudStorageObjectDataType inherits HashDataType {
    private {
        const Fields = {
            "name": {
                "display_name": "Object Name",
                "short_desc": "The object's name",
                "desc": "The name of the object.  Google Cloud Storage object names are a flat key "
                    "space in which `/` is an ordinary character and not a path separator, so the "
                    "value is used verbatim and is never split or normalized",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "reports/2026/q1.csv",
            },
            "bucket": {
                "display_name": "Bucket",
                "short_desc": "The bucket holding the object",
                "desc": "The name of the bucket containing the object",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "id": {
                "display_name": "Object ID",
                "short_desc": "The object's ID",
                "desc": "The ID of the object, in the form `bucket/name/generation`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "generation": {
                "display_name": "Generation",
                "short_desc": "The object's content generation",
                "desc": "The content generation of the object, which changes on every overwrite; used "
                    "to address a specific version of the object",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "metageneration": {
                "display_name": "Metageneration",
                "short_desc": "The object's metadata generation",
                "desc": "The version of the object's metadata, which changes on every metadata update "
                    "and resets when the content generation changes",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "content_type": {
                "display_name": "Content Type",
                "short_desc": "The object's MIME type",
                "desc": "The MIME type of the object's content",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "text/csv",
            },
            "size": {
                "display_name": "Size",
                "short_desc": "The object's size in bytes",
                "desc": "The size of the object's content in bytes.  The JSON API reports this as a "
                    "string because it is a 64-bit value; it is converted to an integer here",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "storage_class": {
                "display_name": "Storage Class",
                "short_desc": "The object's storage class",
                "desc": "The storage class of the object: `STANDARD`, `NEARLINE`, `COLDLINE` or "
                    "`ARCHIVE`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "md5_hash": {
                "display_name": "MD5 Hash",
                "short_desc": "Base64-encoded MD5 digest of the content",
                "desc": "The base64-encoded MD5 digest of the object's content, as Google Cloud "
                    "Storage reports it (not hex).  Not present for composed objects, which have no "
                    "MD5",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "crc32c": {
                "display_name": "CRC32C",
                "short_desc": "Base64-encoded CRC32C checksum of the content",
                "desc": "The base64-encoded, big-endian CRC32C checksum of the object's content, as "
                    "Google Cloud Storage reports it (not hex).  Present for every object, including "
                    "composed objects",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "etag": {
                "display_name": "ETag",
                "short_desc": "HTTP 1.1 entity tag for the object",
                "desc": "The HTTP 1.1 entity tag for the object",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "cache_control": {
                "display_name": "Cache Control",
                "short_desc": "The object's Cache-Control header value",
                "desc": "The `Cache-Control` header served with the object's content",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "content_disposition": {
                "display_name": "Content Disposition",
                "short_desc": "The object's Content-Disposition header value",
                "desc": "The `Content-Disposition` header served with the object's content",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "content_encoding": {
                "display_name": "Content Encoding",
                "short_desc": "The object's Content-Encoding header value",
                "desc": "The `Content-Encoding` header served with the object's content (e.g. `gzip`)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "content_language": {
                "display_name": "Content Language",
                "short_desc": "The object's Content-Language header value",
                "desc": "The `Content-Language` header served with the object's content",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "metadata": {
                "display_name": "Custom Metadata",
                "short_desc": "User-defined metadata on the object",
                "desc": "User-defined metadata on the object as key/value pairs; values are always "
                    "strings",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "custom_time": {
                "display_name": "Custom Time",
                "short_desc": "A user-specified timestamp on the object",
                "desc": "A user-specified date and time on the object, usable in lifecycle rules",
                "type": DateOrNothingType,
            },
            "kms_key_name": {
                "display_name": "KMS Key Name",
                "short_desc": "The Cloud KMS key protecting the object",
                "desc": "The resource name of the Cloud KMS key used to encrypt the object, if it is "
                    "protected by a customer-managed encryption key",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "time_created": {
                "display_name": "Created",
                "short_desc": "When the object was created",
                "desc": "The date and time the object was created",
                "type": DateOrNothingType,
            },
            "updated": {
                "display_name": "Updated",
                "short_desc": "When the object was last modified",
                "desc": "The date and time the object's metadata was last modified",
                "type": DateOrNothingType,
            },
            "time_deleted": {
                "display_name": "Deleted",
                "short_desc": "When the object generation was deleted",
                "desc": "The date and time this object generation was deleted or overwritten; only "
                    "present for noncurrent generations in a versioned bucket",
                "type": DateOrNothingType,
            },
            "media_link": {
                "display_name": "Media Link",
                "short_desc": "The URI of the object's content",
                "desc": "The URI the object's content can be downloaded from; the request must be "
                    "authenticated, so this is not a public link",
                "type": AbstractDataProviderTypeMap."*uri",
            },
            "self_link": {
                "display_name": "Self Link",
                "short_desc": "The URI of the object resource",
                "desc": "The URI of the object's metadata in the Google Cloud Storage JSON API",
                "type": AbstractDataProviderTypeMap."*uri",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleCloudStorageObjectDataType = new GoogleCloudStorageObjectDataType();

#! Google Cloud Storage object download result data type
/** The object's bytes are returned in the \c content field as a @ref binary, which is what the
    application's file API declares with \c read_data_encoding \c "raw"; the bytes are never
    base64-encoded on the way out.
*/
public class GoogleCloudStorageDownloadResultDataType inherits HashDataType {
    private {
        const Fields = {
            "name": {
                "display_name": "Object Name",
                "short_desc": "The object's name",
                "desc": "The name of the object that was downloaded, verbatim",
                "type": AbstractDataProviderTypeMap."string",
            },
            "bucket": {
                "display_name": "Bucket",
                "short_desc": "The bucket holding the object",
                "desc": "The name of the bucket the object was downloaded from",
                "type": AbstractDataProviderTypeMap."string",
            },
            "content": {
                "display_name": "Content",
                "short_desc": "The object's content",
                "desc": "The object's content as raw bytes; not base64-encoded",
                "type": AbstractDataProviderTypeMap."*binary",
            },
            "content_type": {
                "display_name": "Content Type",
                "short_desc": "The object's MIME type",
                "desc": "The MIME type Google Cloud Storage served the content with",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "size": {
                "display_name": "Size",
                "short_desc": "The number of bytes downloaded",
                "desc": "The number of bytes of content returned",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "generation": {
                "display_name": "Generation",
                "short_desc": "The content generation downloaded",
                "desc": "The content generation of the object that was downloaded",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "md5_hash": {
                "display_name": "MD5 Hash",
                "short_desc": "Base64-encoded MD5 digest of the content",
                "desc": "The base64-encoded MD5 digest Google Cloud Storage reported for the content, "
                    "if any; absent for composed objects",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "crc32c": {
                "display_name": "CRC32C",
                "short_desc": "Base64-encoded CRC32C checksum of the content",
                "desc": "The base64-encoded CRC32C checksum Google Cloud Storage reported for the "
                    "content, if any",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "etag": {
                "display_name": "ETag",
                "short_desc": "HTTP 1.1 entity tag for the object",
                "desc": "The HTTP 1.1 entity tag Google Cloud Storage reported for the content",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleCloudStorageDownloadResultDataType = new GoogleCloudStorageDownloadResultDataType();

#! Google Cloud Storage signed URL result data type
public class GoogleCloudStorageSignedUrlDataType inherits HashDataType {
    private {
        const Fields = {
            "url": {
                "display_name": "Signed URL",
                "short_desc": "The signed URL",
                "desc": "The signed URL; anyone holding it can perform the signed method on the "
                    "object until it expires, with no further credentials",
                "type": AbstractDataProviderTypeMap."uri",
            },
            "method": {
                "display_name": "HTTP Method",
                "short_desc": "The HTTP method the URL is signed for",
                "desc": "The HTTP method the URL is valid for; a URL signed for `GET` cannot be used "
                    "to `PUT`",
                "type": AbstractDataProviderTypeMap."string",
            },
            "expires": {
                "display_name": "Expires",
                "short_desc": "When the URL stops working",
                "desc": "The date and time the signed URL expires",
                "type": DateType,
            },
            "expires_in_seconds": {
                "display_name": "Expires In (Seconds)",
                "short_desc": "The URL's lifetime in seconds",
                "desc": "The lifetime of the signed URL in seconds from the time it was signed",
                "type": AbstractDataProviderTypeMap."int",
            },
            "signer_email": {
                "display_name": "Signer Email",
                "short_desc": "The service account that signed the URL",
                "desc": "The email address of the service account whose key signed the URL",
                "type": AbstractDataProviderTypeMap."email",
            },
            "bucket": {
                "display_name": "Bucket",
                "short_desc": "The bucket the URL addresses",
                "desc": "The name of the bucket the signed URL addresses",
                "type": AbstractDataProviderTypeMap."string",
            },
            "object": {
                "display_name": "Object Name",
                "short_desc": "The object the URL addresses",
                "desc": "The name of the object the signed URL addresses, verbatim",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleCloudStorageSignedUrlDataType = new GoogleCloudStorageSignedUrlDataType();

#! Google Cloud Storage delete result data type
public class GoogleCloudStorageDeleteResultDataType inherits HashDataType {
    private {
        const Fields = {
            "bucket": {
                "display_name": "Bucket",
                "short_desc": "The bucket the object was deleted from",
                "desc": "The name of the bucket the object was deleted from",
                "type": AbstractDataProviderTypeMap."string",
            },
            "object": {
                "display_name": "Object Name",
                "short_desc": "The object that was deleted",
                "desc": "The name of the object that was deleted, verbatim",
                "type": AbstractDataProviderTypeMap."string",
            },
            "generation": {
                "display_name": "Generation",
                "short_desc": "The generation that was deleted",
                "desc": "The content generation that was deleted, if a specific generation was "
                    "addressed",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "deleted": {
                "display_name": "Deleted",
                "short_desc": "If the object was deleted",
                "desc": "Always @ref True; the call throws an exception if the object could not be "
                    "deleted",
                "type": AbstractDataProviderTypeMap."bool",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleCloudStorageDeleteResultDataType = new GoogleCloudStorageDeleteResultDataType();

#! Google Cloud Storage object list result data type
public class GoogleCloudStorageObjectListDataType inherits HashDataType {
    private {
        const Fields = {
            "objects": {
                "display_name": "Objects",
                "short_desc": "The objects matched",
                "desc": "The objects matched by the request",
                "type": new ListDataType("GoogleCloudStorageObjects", GoogleCloudStorageObjectDataType, True),
            },
            "prefixes": {
                "display_name": "Prefixes",
                "short_desc": "Synthetic common prefixes",
                "desc": "When a delimiter is given, the set of synthetic \"directory\" prefixes that "
                    "objects were rolled up into; these are not objects and do not exist in the "
                    "bucket",
                "type": new ListDataType("GoogleCloudStoragePrefixes", SoftStringType, True),
            },
            "next_page_token": {
                "display_name": "Next Page Token",
                "short_desc": "Token for the next page of results",
                "desc": "The token to pass as `page_token` to retrieve the next page; absent when "
                    "this is the last page",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleCloudStorageObjectListDataType = new GoogleCloudStorageObjectListDataType();

#! Google Cloud Storage bucket list result data type
public class GoogleCloudStorageBucketListDataType inherits HashDataType {
    private {
        const Fields = {
            "buckets": {
                "display_name": "Buckets",
                "short_desc": "The buckets in the project",
                "desc": "The buckets in the project matched by the request",
                "type": new ListDataType("GoogleCloudStorageBuckets", GoogleCloudStorageBucketDataType, True),
            },
            "next_page_token": {
                "display_name": "Next Page Token",
                "short_desc": "Token for the next page of results",
                "desc": "The token to pass as `page_token` to retrieve the next page; absent when "
                    "this is the last page",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleCloudStorageBucketListDataType = new GoogleCloudStorageBucketListDataType();
}
