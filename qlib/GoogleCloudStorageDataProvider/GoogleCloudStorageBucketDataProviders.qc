# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GoogleCloudStorageDataProvider module definition

/** GoogleCloudStorageBucketDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Request type for listing buckets
public class GoogleCloudStorageListBucketsRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "prefix": {
                "display_name": "Prefix",
                "short_desc": "List only buckets whose name begins with this string",
                "desc": "List only buckets whose name begins with this string",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "prod-",
            },
            "max_results": {
                "display_name": "Max Results",
                "short_desc": "Maximum buckets to return per page",
                "desc": "The maximum number of buckets to return in one page (max `1000`)",
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

public const GoogleCloudStorageListBucketsRequestDataType =
    new GoogleCloudStorageListBucketsRequestDataType();

#! Data provider for listing the buckets in a project
public class GoogleCloudStorageListBucketsDataProvider inherits GoogleCloudStorageDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-buckets",
            "type": "GoogleCloudStorageListBucketsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = GoogleCloudStorageListBucketsRequestDataType;
        const ResponseType = GoogleCloudStorageBucketListDataType;
    }

    constructor(GoogleCloudStorageRestClient::GoogleCloudStorageRestClientIo rest, hash<GcsAuthContext> auth)
            : GoogleCloudStorageDataProviderBase(rest, auth) {}
    constructor(*hash<auto> options) : GoogleCloudStorageDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> query = {
            "project": auth.project_id,
            "projection": "noAcl",
            "prefix": req.prefix,
            "maxResults": req.max_results ?? MaxPageSize,
            "pageToken": req.page_token,
        };
        hash<auto> resp = doGcsRequest("GET", GoogleCloudStorageRestClient::GcsApiPrefix + "/b", query);
        return {
            "buckets": (map GoogleCloudStorageBucketDataProviderUtil::normalizeBucket($1),
                resp.body.items ?? ()),
            "next_page_token": resp.body.nextPageToken,
        };
    }
}

#! Request type for retrieving one bucket's metadata
public class GoogleCloudStorageGetBucketRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "bucket": {
                "display_name": "Bucket",
                "short_desc": "The bucket to retrieve",
                "desc": "The name of the bucket whose metadata should be retrieved",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "my-bucket",
                "ref_data": "buckets",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleCloudStorageGetBucketRequestDataType =
    new GoogleCloudStorageGetBucketRequestDataType();

#! Data provider for retrieving one bucket's metadata
public class GoogleCloudStorageGetBucketDataProvider inherits GoogleCloudStorageDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-bucket",
            "type": "GoogleCloudStorageGetBucketDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = GoogleCloudStorageGetBucketRequestDataType;
        const ResponseType = GoogleCloudStorageBucketDataType;
    }

    constructor(GoogleCloudStorageRestClient::GoogleCloudStorageRestClientIo rest, hash<GcsAuthContext> auth)
            : GoogleCloudStorageDataProviderBase(rest, auth) {}
    constructor(*hash<auto> options) : GoogleCloudStorageDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string path = sprintf("%s/b/%s", GoogleCloudStorageRestClient::GcsApiPrefix,
            encode_url(req.bucket, True));
        hash<auto> resp = doGcsRequest("GET", path, {"projection": "noAcl"});
        return GoogleCloudStorageBucketDataProviderUtil::normalizeBucket(resp.body);
    }
}

#! Static helpers shared by the bucket providers
public class GoogleCloudStorageBucketDataProviderUtil {
    #! Converts a Google Cloud Storage Bucket resource to this module's flattened bucket hash
    /** The JSON API nests a few single-value objects (\c versioning.enabled,
        \c iamConfiguration.uniformBucketLevelAccess.enabled, \c billing.requesterPays) and reports
        \c projectNumber as a string because it is a 64-bit value; both are normalized here so
        consumers see a flat hash with %Qore types.

        @param bucket the Bucket resource as returned by the JSON API

        @return the flattened bucket hash matching @ref GoogleCloudStorageBucketDataType
    */
    static hash<auto> normalizeBucket(hash<auto> bucket) {
        hash<auto!> rv = {
            "name": bucket.name,
            "id": bucket.id,
            "location": bucket.location,
            "location_type": bucket.locationType,
            "storage_class": bucket.storageClass,
            "labels": bucket.labels,
            "etag": bucket.etag,
            "self_link": bucket.selfLink,
        };
        if (exists bucket.projectNumber) {
            rv.project_number = bucket.projectNumber.toInt();
        }
        if (exists bucket.versioning.enabled) {
            rv.versioning_enabled = bucket.versioning.enabled.toBool();
        }
        if (exists bucket.iamConfiguration.uniformBucketLevelAccess.enabled) {
            rv.uniform_bucket_level_access =
                bucket.iamConfiguration.uniformBucketLevelAccess.enabled.toBool();
        }
        if (exists bucket.billing.requesterPays) {
            rv.requester_pays = bucket.billing.requesterPays.toBool();
        }
        if (bucket.timeCreated.val()) {
            rv.time_created = date(bucket.timeCreated);
        }
        if (bucket.updated.val()) {
            rv.updated = date(bucket.updated);
        }
        return rv;
    }
}
}
