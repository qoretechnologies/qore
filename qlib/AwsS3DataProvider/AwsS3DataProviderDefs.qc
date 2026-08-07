# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AwsS3DataProviderDefs.qc shared definitions for the AwsS3DataProvider module

/*  AwsS3DataProviderDefs.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The application name
/** @note This is the same name used by the Amazon S3 application in the v8 application catalog, so that
    connections and OAuth2 configuration created for that application continue to resolve to this one
*/
public const AppName = "AmazonS3";

#! The application logo
const AmazonS3Logo = File::readTextFile(get_script_dir() + "/amazon-s3-logo.svg");

#! Maximum keys returned in a single list request
public const MaxKeys = 1000;

#! Default number of keys requested per list page
public const DefaultMaxKeys = 200;

#! The size above which an upload is sent as a multipart upload
/** Amazon S3 rejects single-part uploads above 5 GB; a much lower threshold is used so that large uploads are
    not held in memory in a single request
*/
public const MultipartThreshold = 8 * 1024 * 1024;

#! The size of each part in a multipart upload
/** Amazon S3 requires every part except the last to be at least 5 MB
*/
public const MultipartPartSize = 8 * 1024 * 1024;

#! Event name for new or updated objects
public const EVENT_NEW_OR_UPDATED_FILE = "new-or-updated-file";

#! Event name for new buckets
public const EVENT_NEW_BUCKET = "new-bucket";

#! Default polling interval in seconds for watch providers
public const DefaultPollInterval = 60;

#! Minimum polling interval in seconds for watch providers
public const MinPollInterval = 30;

#! Default maximum number of keys a single object-watch poll scans
/** Amazon S3 has no server-side "modified since" filter on a listing, so an object watch has to list keys and
    compare their timestamps itself.  A bucket holding millions of keys would therefore make every poll scan the
    whole bucket, so a scan is bounded; reaching the bound is logged as a warning naming the options that
    narrow it, never silently.
*/
public const DefaultMaxScanKeys = 10000;

#! Storage classes accepted by Amazon S3
public const StorageClassValues = (
    <AllowedValueInfo>{
        "value": "STANDARD",
        "display_name": "Standard",
        "short_desc": "Frequently accessed data",
        "desc": "Frequently accessed data; the default storage class",
    },
    <AllowedValueInfo>{
        "value": "INTELLIGENT_TIERING",
        "display_name": "Intelligent-Tiering",
        "short_desc": "Data with changing or unknown access patterns",
        "desc": "Data with changing or unknown access patterns; Amazon S3 moves objects between access tiers",
    },
    <AllowedValueInfo>{
        "value": "STANDARD_IA",
        "display_name": "Standard-Infrequent Access",
        "short_desc": "Long-lived, infrequently accessed data",
        "desc": "Long-lived but infrequently accessed data; a retrieval charge applies",
    },
    <AllowedValueInfo>{
        "value": "ONEZONE_IA",
        "display_name": "One Zone-Infrequent Access",
        "short_desc": "Infrequently accessed data in a single availability zone",
        "desc": "Long-lived, infrequently accessed data stored in a single availability zone; cheaper than "
            "`STANDARD_IA` but lost if the zone is destroyed",
    },
    <AllowedValueInfo>{
        "value": "GLACIER_IR",
        "display_name": "Glacier Instant Retrieval",
        "short_desc": "Archive data needing immediate access",
        "desc": "Archive data that still requires millisecond retrieval",
    },
    <AllowedValueInfo>{
        "value": "GLACIER",
        "display_name": "Glacier Flexible Retrieval",
        "short_desc": "Archive data retrieved in minutes to hours",
        "desc": "Archive data retrieved in minutes to hours; objects must be restored before they can be read",
    },
    <AllowedValueInfo>{
        "value": "DEEP_ARCHIVE",
        "display_name": "Glacier Deep Archive",
        "short_desc": "Long-term archive retrieved in hours",
        "desc": "Long-term archive data retrieved in hours; the lowest-cost storage class",
    },
);

#! Canned ACLs accepted when creating a bucket
public const BucketAclValues = (
    <AllowedValueInfo>{
        "value": "private",
        "display_name": "Private",
        "short_desc": "Only the bucket owner has access",
        "desc": "Only the bucket owner has access; the default and the recommended value",
    },
    <AllowedValueInfo>{
        "value": "public-read",
        "display_name": "Public Read",
        "short_desc": "Anyone can read the bucket contents",
        "desc": "Anyone on the internet can read the bucket contents; **use with care**, as this exposes every "
            "object in the bucket",
    },
    <AllowedValueInfo>{
        "value": "public-read-write",
        "display_name": "Public Read/Write",
        "short_desc": "Anyone can read and write the bucket contents",
        "desc": "Anyone on the internet can read and write the bucket contents; **strongly discouraged**, as it "
            "allows anyone to store objects at the account owner's expense",
    },
    <AllowedValueInfo>{
        "value": "authenticated-read",
        "display_name": "Authenticated Read",
        "short_desc": "Any authenticated AWS user can read the bucket contents",
        "desc": "Any authenticated AWS user can read the bucket contents, not only users in this account",
    },
);

#! HTTP methods a presigned URL can be signed for
/** A presigned URL grants exactly one operation: a URL signed for a `GET` cannot be used to upload
*/
public const PresignedMethodValues = (
    <AllowedValueInfo>{
        "value": "GET",
        "display_name": "Download (GET)",
        "short_desc": "The holder can download the object",
        "desc": "The holder can download the object",
    },
    <AllowedValueInfo>{
        "value": "PUT",
        "display_name": "Upload (PUT)",
        "short_desc": "The holder can upload to the key",
        "desc": "The holder can upload content to the key, **replacing whatever is there**",
    },
    <AllowedValueInfo>{
        "value": "HEAD",
        "display_name": "Metadata Only (HEAD)",
        "short_desc": "The holder can read the object's metadata but not its content",
        "desc": "The holder can read the object's size, type, and modification time without downloading it",
    },
    <AllowedValueInfo>{
        "value": "DELETE",
        "display_name": "Delete (DELETE)",
        "short_desc": "The holder can delete the object",
        "desc": "The holder can delete the object; **this cannot be undone on a bucket without versioning**",
    },
);

#! Default lifetime of a presigned URL
public const DefaultPresignedUrlLifetime = 3600;

#! Server-side encryption modes accepted when storing an object
public const ServerSideEncryptionValues = (
    <AllowedValueInfo>{
        "value": "AES256",
        "display_name": "S3-Managed Keys (SSE-S3)",
        "short_desc": "Encrypt with keys Amazon S3 manages",
        "desc": "Encrypt the object with keys Amazon S3 manages entirely; nothing further is needed",
    },
    <AllowedValueInfo>{
        "value": "aws:kms",
        "display_name": "KMS Keys (SSE-KMS)",
        "short_desc": "Encrypt with an AWS KMS key",
        "desc": "Encrypt the object with an AWS KMS key.\n\n"
            "Name the key in `kms_key_id`; if it is left unset, the bucket's default KMS key is used.  The "
            "credentials must be allowed to use the key, or the upload fails even though they can write to "
            "the bucket",
    },
    <AllowedValueInfo>{
        "value": "aws:kms:dsse",
        "display_name": "Dual-Layer KMS (DSSE-KMS)",
        "short_desc": "Encrypt twice with an AWS KMS key",
        "desc": "Apply two independent layers of encryption with an AWS KMS key, for data subject to a "
            "requirement for it",
    },
);

#! The maximum number of keys Amazon S3 accepts in one batch delete
public const MaxDeleteKeys = 1000;
}
