# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AwsS3BucketDataProviders.qc bucket-level action providers for the AwsS3DataProvider module

/*  AwsS3BucketDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Lists the buckets owned by the authenticated account
public class AwsS3ListBucketsDataProvider inherits AwsS3DataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-buckets",
            "type": "AwsS3ListBucketsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = AwsS3ListBucketsRequestDataType;
        const ResponseType = AwsS3ListBucketsResponseDataType;
    }

    constructor(AwsRestClient::AwsRestClientIo rest, bool path_style = True)
            : AwsS3DataProviderBase(rest, path_style) {
    }
    constructor(*hash<auto> options) : AwsS3DataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> resp = doS3Request("GET", NOTHING, NOTHING);
        *hash<auto> doc = AwsS3DataProviderBase::parseXml(resp.body).ListAllMyBucketsResult;
        # NOTE: built with an explicit loop rather than "map"; the hash-map form produces a single hash rather
        # than one hash per element, and the list form is ambiguous to the parser in a hash value position
        list<auto> buckets = ();
        foreach hash<auto> b in (AwsS3DataProviderBase::forceList(doc.Buckets.Bucket)) {
            buckets += {
                "name": b.Name,
                "creation_date": b.CreationDate.val() ? date(b.CreationDate) : NOTHING,
            };
        }

        return {
            "buckets": buckets,
            "owner_id": doc.Owner.ID,
            "owner_name": doc.Owner.DisplayName,
        };
    }
}

#! Creates a bucket
public class AwsS3CreateBucketDataProvider inherits AwsS3DataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-bucket",
            "type": "AwsS3CreateBucketDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = AwsS3CreateBucketRequestDataType;
        const ResponseType = AwsS3BucketResponseDataType;
    }

    constructor(AwsRestClient::AwsRestClientIo rest, bool path_style = True)
            : AwsS3DataProviderBase(rest, path_style) {
    }
    constructor(*hash<auto> options) : AwsS3DataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> hdr;
        if (req.acl.val()) {
            hdr."x-amz-acl" = req.acl;
        }
        if (req.object_lock_enabled) {
            hdr."x-amz-bucket-object-lock-enabled" = "true";
        }

        # the region is carried in the request body rather than a header, and us-east-1 must not be sent at all:
        # it is the API default and naming it explicitly is rejected as an invalid location constraint
        *string body;
        string region = rest.getAwsRegion();
        if (region.val() && region != "us-east-1") {
            body = sprintf("<CreateBucketConfiguration "
                "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
                "<LocationConstraint>%s</LocationConstraint></CreateBucketConfiguration>", region);
        }

        hash<auto> resp = doS3Request("PUT", req.bucket, NOTHING, NOTHING, body, hdr);
        return {
            "bucket": req.bucket,
            "location": resp.hdr.location ?? ("/" + req.bucket),
        };
    }
}
}
