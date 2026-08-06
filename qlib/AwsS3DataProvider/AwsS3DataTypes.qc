# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AwsS3DataTypes.qc data types for the AwsS3DataProvider module

/*  AwsS3DataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Common field describing an S3 bucket name
const BucketField = {
    "display_name": "Bucket",
    "short_desc": "The S3 bucket name",
    "desc": "The name of the S3 bucket holding the object",
    "type": AbstractDataProviderTypeMap."string",
    "required": True,
    "example_value": "my-application-data",
    "ref_data": "buckets",
};

#! Common field describing an S3 object key
const ObjectKeyField = {
    "display_name": "Object Key",
    "short_desc": "The S3 object key",
    "desc": "The full key of the object within the bucket, including any prefix; S3 keys are flat strings, so "
        "`/` characters are part of the key rather than directory separators",
    "type": AbstractDataProviderTypeMap."string",
    "required": True,
    "example_value": "invoices/2026/inv-1001.pdf",
};

#! Common field describing an object version ID
const VersionIdField = {
    "display_name": "Version ID",
    "short_desc": "A specific object version to address",
    "desc": "A specific version of the object to address; only meaningful on version-enabled buckets. If not "
        "set, the current version is used",
    "type": AbstractDataProviderTypeMap."*string",
};

#! Request type for listing buckets
public class AwsS3ListBucketsRequestDataType inherits HashDataType {
    public {
        const Fields = {};
    }
    constructor() { addQoreFields(Fields); }
}

public const AwsS3ListBucketsRequestDataType = new AwsS3ListBucketsRequestDataType();

#! Response type for listing buckets
public class AwsS3ListBucketsResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "buckets": {
                "display_name": "Buckets",
                "short_desc": "The buckets owned by the account",
                "desc": "The buckets owned by the authenticated account",
                "type": AbstractDataProviderTypeMap."list",
            },
            "owner_id": {
                "display_name": "Owner ID",
                "short_desc": "The canonical ID of the account owning the buckets",
                "desc": "The canonical ID of the account owning the buckets",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "owner_name": {
                "display_name": "Owner Name",
                "short_desc": "The display name of the account owning the buckets",
                "desc": "The display name of the account owning the buckets",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AwsS3ListBucketsResponseDataType = new AwsS3ListBucketsResponseDataType();

#! Request type for creating a bucket
public class AwsS3CreateBucketRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "bucket": BucketField + {
                "desc": "The name of the bucket to create; bucket names are globally unique across all of "
                    "Amazon S3 and must be DNS-compatible lower-case names",
                "ref_data": NOTHING,
            },
            "acl": {
                "display_name": "Canned ACL",
                "short_desc": "The access control policy for the new bucket",
                "desc": "The access control policy applied to the new bucket",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": BucketAclValues,
                "default_value": "private",
            },
            "object_lock_enabled": {
                "display_name": "Object Lock Enabled",
                "short_desc": "Enable object lock on the new bucket",
                "desc": "Enable object lock on the new bucket; object lock cannot be enabled after creation and "
                    "requires versioning, which is enabled automatically when this is set",
                "type": AbstractDataProviderTypeMap."*bool",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AwsS3CreateBucketRequestDataType = new AwsS3CreateBucketRequestDataType();

#! Response type for operations that return a bucket
public class AwsS3BucketResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "bucket": {
                "display_name": "Bucket",
                "short_desc": "The bucket name",
                "desc": "The name of the bucket",
                "type": AbstractDataProviderTypeMap."string",
            },
            "location": {
                "display_name": "Location",
                "short_desc": "The bucket location",
                "desc": "The location of the bucket",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AwsS3BucketResponseDataType = new AwsS3BucketResponseDataType();

#! Request type for listing objects
public class AwsS3ListObjectsRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "bucket": BucketField,
            "prefix": {
                "display_name": "Prefix",
                "short_desc": "Return only keys beginning with this prefix",
                "desc": "Return only keys beginning with this prefix; used with `delimiter` to emulate browsing "
                    "a directory tree",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "invoices/2026/",
                "preselected": True,
            },
            "delimiter": {
                "display_name": "Delimiter",
                "short_desc": "Group keys sharing a prefix up to this character",
                "desc": "Group keys that share a prefix up to the first occurrence of this character; set to "
                    "`/` together with `prefix` to list one level of a key hierarchy rather than every key "
                    "below it",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "/",
            },
            "max_keys": {
                "display_name": "Maximum Keys",
                "short_desc": "The maximum number of keys to return per page",
                "desc": "The maximum number of keys to return per page; Amazon S3 caps this at `1000`",
                "type": AbstractDataProviderTypeMap."*int",
                "default_value": DefaultMaxKeys,
            },
            "start_after": {
                "display_name": "Start After",
                "short_desc": "Begin listing after this key",
                "desc": "Begin listing after this key; keys are returned in ascending order",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "continuation_token": {
                "display_name": "Continuation Token",
                "short_desc": "The token returned by a previous truncated listing",
                "desc": "The `next_continuation_token` value returned by a previous truncated listing; used to "
                    "retrieve the following page",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AwsS3ListObjectsRequestDataType = new AwsS3ListObjectsRequestDataType();

#! Response type for listing objects
public class AwsS3ListObjectsResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "objects": {
                "display_name": "Objects",
                "short_desc": "The objects matching the request",
                "desc": "The objects matching the request; each entry has `key`, `size`, `last_modified`, "
                    "`etag`, and `storage_class`",
                "type": AbstractDataProviderTypeMap."list",
            },
            "prefixes": {
                "display_name": "Common Prefixes",
                "short_desc": "Key prefixes grouped by the delimiter",
                "desc": "Key prefixes grouped by the delimiter; these correspond to subdirectories when the "
                    "delimiter is `/`",
                "type": AbstractDataProviderTypeMap."*list",
            },
            "truncated": {
                "display_name": "Truncated",
                "short_desc": "More results are available",
                "desc": "`True` if more results are available; pass `next_continuation_token` to the next call "
                    "to retrieve them",
                "type": AbstractDataProviderTypeMap."bool",
            },
            "next_continuation_token": {
                "display_name": "Next Continuation Token",
                "short_desc": "The token to pass to retrieve the next page",
                "desc": "The token to pass in `continuation_token` to retrieve the next page of results",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "key_count": {
                "display_name": "Key Count",
                "short_desc": "The number of keys returned",
                "desc": "The number of keys returned in this page",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AwsS3ListObjectsResponseDataType = new AwsS3ListObjectsResponseDataType();

#! Request type for retrieving an object
public class AwsS3GetObjectRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "bucket": BucketField,
            "object_key": ObjectKeyField,
            "include_content": {
                "display_name": "Include Content",
                "short_desc": "Return the object content as well as its metadata",
                "desc": "Return the object content in the `content` field as well as its metadata; when `False` "
                    "only the metadata is retrieved, which is much cheaper for large objects",
                "type": AbstractDataProviderTypeMap."*bool",
                "default_value": True,
                "preselected": True,
            },
            "version_id": VersionIdField,
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AwsS3GetObjectRequestDataType = new AwsS3GetObjectRequestDataType();

#! Response type for retrieving an object
public class AwsS3GetObjectResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "name": {
                "display_name": "Name",
                "short_desc": "The object key",
                "desc": "The key of the object retrieved",
                "type": AbstractDataProviderTypeMap."string",
            },
            "content": {
                "display_name": "Content",
                "short_desc": "The object content, base64-encoded",
                "desc": "The object content, base64-encoded; present only when `include_content` was set",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "mime_type": {
                "display_name": "MIME Type",
                "short_desc": "The object content type",
                "desc": "The content type reported by S3 for the object",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "size": {
                "display_name": "Size",
                "short_desc": "The object size in bytes",
                "desc": "The size of the object in bytes",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "etag": {
                "display_name": "ETag",
                "short_desc": "The object entity tag",
                "desc": "The entity tag for the object; for single-part uploads this is the MD5 digest of the "
                    "content, for multipart uploads it is not",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "last_modified": {
                "display_name": "Last Modified",
                "short_desc": "When the object was last modified",
                "desc": "When the object was last modified",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "version_id": {
                "display_name": "Version ID",
                "short_desc": "The version of the object retrieved",
                "desc": "The version of the object retrieved, on version-enabled buckets",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AwsS3GetObjectResponseDataType = new AwsS3GetObjectResponseDataType();

#! Request type for uploading a file
public class AwsS3UploadFileRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "bucket": BucketField,
            "object_key": ObjectKeyField + {
                "desc": "The key to store the object under; if not set, the file name is used",
                "required": False,
                "type": AbstractDataProviderTypeMap."*string",
            },
            "file": {
                "display_name": "File",
                "short_desc": "The file to upload",
                "desc": "The file to upload, including its name, content, and MIME type",
                "type": FileDataType,
                "required": True,
            },
            "storage_class": {
                "display_name": "Storage Class",
                "short_desc": "The storage class for the new object",
                "desc": "The storage class for the new object, which determines its cost and retrieval "
                    "characteristics",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": StorageClassValues,
            },
            "metadata": {
                "display_name": "Metadata",
                "short_desc": "User-defined metadata to store with the object",
                "desc": "User-defined metadata to store with the object; each key is sent as an "
                    "`x-amz-meta-<key>` header and is returned when the object is retrieved",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AwsS3UploadFileRequestDataType = new AwsS3UploadFileRequestDataType();

#! Request type for creating a text object
public class AwsS3CreateTextFileRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "bucket": BucketField,
            "object_key": ObjectKeyField,
            "content": {
                "display_name": "Content",
                "short_desc": "The text content to store",
                "desc": "The text content to store in the object",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "content_type": {
                "display_name": "Content Type",
                "short_desc": "The MIME type to store with the object",
                "desc": "The MIME type to store with the object",
                "type": AbstractDataProviderTypeMap."*string",
                "default_value": "text/plain",
            },
            "storage_class": {
                "display_name": "Storage Class",
                "short_desc": "The storage class for the new object",
                "desc": "The storage class for the new object",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": StorageClassValues,
            },
            "metadata": {
                "display_name": "Metadata",
                "short_desc": "User-defined metadata to store with the object",
                "desc": "User-defined metadata to store with the object",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AwsS3CreateTextFileRequestDataType = new AwsS3CreateTextFileRequestDataType();

#! Response type for operations returning an object reference
public class AwsS3ObjectResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "bucket": {
                "display_name": "Bucket",
                "short_desc": "The bucket holding the object",
                "desc": "The bucket holding the object",
                "type": AbstractDataProviderTypeMap."string",
            },
            "object_key": {
                "display_name": "Object Key",
                "short_desc": "The object key",
                "desc": "The key of the object",
                "type": AbstractDataProviderTypeMap."string",
            },
            "etag": {
                "display_name": "ETag",
                "short_desc": "The object entity tag",
                "desc": "The entity tag assigned to the object",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "version_id": {
                "display_name": "Version ID",
                "short_desc": "The version assigned to the object",
                "desc": "The version assigned to the object, on version-enabled buckets",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AwsS3ObjectResponseDataType = new AwsS3ObjectResponseDataType();

#! Request type for deleting an object
public class AwsS3DeleteObjectRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "bucket": BucketField,
            "object_key": ObjectKeyField,
            "version_id": VersionIdField,
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AwsS3DeleteObjectRequestDataType = new AwsS3DeleteObjectRequestDataType();

#! Request type for copying an object
public class AwsS3CopyObjectRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "source_bucket": BucketField + {
                "display_name": "Source Bucket",
                "short_desc": "The bucket holding the object to copy",
                "desc": "The bucket holding the object to copy",
            },
            "source_key": ObjectKeyField + {
                "display_name": "Source Key",
                "short_desc": "The key of the object to copy",
                "desc": "The key of the object to copy",
            },
            "bucket": BucketField + {
                "display_name": "Target Bucket",
                "short_desc": "The bucket to copy the object into",
                "desc": "The bucket to copy the object into",
            },
            "object_key": ObjectKeyField + {
                "display_name": "Target Key",
                "short_desc": "The key to copy the object to",
                "desc": "The key to copy the object to",
            },
            "storage_class": {
                "display_name": "Storage Class",
                "short_desc": "The storage class for the copy",
                "desc": "The storage class for the copy; if not set, the source object's class is used",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": StorageClassValues,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AwsS3CopyObjectRequestDataType = new AwsS3CopyObjectRequestDataType();

#! Request type for downloading a file
public class AwsS3DownloadFileRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "bucket": BucketField,
            "object_key": ObjectKeyField,
            "version_id": VersionIdField,
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AwsS3DownloadFileRequestDataType = new AwsS3DownloadFileRequestDataType();

#! Event type raised when a bucket is created
public class AwsS3NewBucketEventDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "Event ID",
                "short_desc": "The identity of the bucket in the event feed",
                "desc": "The identity of the bucket in the event feed, which is its name",
                "type": AbstractDataProviderTypeMap."string",
            },
            "name": {
                "display_name": "Bucket Name",
                "short_desc": "The name of the bucket created",
                "desc": "The name of the bucket that was created",
                "type": AbstractDataProviderTypeMap."string",
            },
            "creation_date": {
                "display_name": "Creation Date",
                "short_desc": "When the bucket was created",
                "desc": "When the bucket was created",
                "type": AbstractDataProviderTypeMap."date",
            },
            "owner_id": {
                "display_name": "Owner ID",
                "short_desc": "The canonical ID of the account owning the bucket",
                "desc": "The canonical identifier of the account that owns the bucket",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "owner_name": {
                "display_name": "Owner Name",
                "short_desc": "The display name of the account owning the bucket",
                "desc": "The display name of the account that owns the bucket",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AwsS3NewBucketEventDataType = new AwsS3NewBucketEventDataType();

#! Event type raised when an object is created or updated
public class AwsS3ObjectEventDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "Event ID",
                "short_desc": "The identity of the object in the event feed",
                "desc": "The identity of the object in the event feed, which is its key",
                "type": AbstractDataProviderTypeMap."string",
            },
            "bucket": {
                "display_name": "Bucket",
                "short_desc": "The bucket holding the object",
                "desc": "The bucket holding the object",
                "type": AbstractDataProviderTypeMap."string",
            },
            "key": {
                "display_name": "Object Key",
                "short_desc": "The key of the object created or updated",
                "desc": "The key of the object that was created or updated",
                "type": AbstractDataProviderTypeMap."string",
            },
            "size": {
                "display_name": "Size",
                "short_desc": "The object size in bytes",
                "desc": "The size of the object in bytes",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "last_modified": {
                "display_name": "Last Modified",
                "short_desc": "When the object was last modified",
                "desc": "When the object was last modified",
                "type": AbstractDataProviderTypeMap."date",
            },
            "etag": {
                "display_name": "ETag",
                "short_desc": "The entity tag of the object content",
                "desc": "The entity tag of the object content, without the quotes Amazon S3 wraps it in.\n\n"
                    "For an object uploaded in one request this is the MD5 of the content; for one uploaded as "
                    "a multipart upload it is not, and carries a `-<parts>` suffix instead",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "storage_class": {
                "display_name": "Storage Class",
                "short_desc": "The storage class of the object",
                "desc": "The storage class the object is stored under",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AwsS3ObjectEventDataType = new AwsS3ObjectEventDataType();
}
