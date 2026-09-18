# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AwsTextractDataProviderCommon class definition

/** AwsTextractDataProviderCommon.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace AwsTextractDataProvider {
public class AwsTextractDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        const ConstructorOptions = {
            "aws_region": <DataProviderOptionInfo>{
                "display_name": "AWS Region",
                "short_desc": "AWS region for Textract endpoint",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "AWS region (e.g., `us-east-1`, `eu-west-1`).  Determines "
                    "the Textract endpoint hostname.",
                "example_value": "us-east-1",
            },
            "aws_keyid": <DataProviderOptionInfo>{
                "display_name": "AWS Access Key ID",
                "short_desc": "AWS access key ID for static credentials",
                "sensitive": True,
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "AWS access key ID.  Optional — when omitted, the AWS "
                    "credential chain (env, ECS task role, IMDSv2) is used.",
            },
            "aws_secret": <DataProviderOptionInfo>{
                "display_name": "AWS Secret Access Key",
                "short_desc": "AWS secret access key for static credentials",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "AWS secret access key.  Optional — see `aws_keyid`.",
                "sensitive": True,
            },
            "aws_token": <DataProviderOptionInfo>{
                "display_name": "AWS Session Token",
                "short_desc": "Optional AWS session token (STS / temporary credentials)",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Optional AWS session token for STS / temporary credentials.",
                "sensitive": True,
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(
                    new Type("AwsTextractRestClient::AwsTextractRestClientIo"), NOTHING, {
                        DTT_ClientOnly: True,
                    }),
                "desc": "The AWS Textract REST client object to use",
            },
        };
    }

    private {
        *AwsTextractRestClient::AwsTextractRestClientIo rest;
    }

    constructor(AwsTextractRestClient::AwsTextractRestClientIo rest) {
        self.rest = rest;
    }

    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
        }
        if (!rest) {
            hash<auto> opts = {};
            if (copts.aws_region.val()) { opts.aws_region = copts.aws_region; }
            if (copts.aws_keyid.val()) {
                opts.aws_keyid = copts.aws_keyid;
            }
            if (copts.aws_secret.val()) {
                opts.aws_secret = copts.aws_secret;
            }
            if (copts.aws_token.val()) {
                opts.aws_token = copts.aws_token;
            }
            rest = new AwsTextractRestClient::AwsTextractRestClientIo(opts);
        }
    }

    #! Adapter dropdowns are scoped by this client's AWS credentials and region
    /** @return supported reference names; jobs have no listing API */
    *hash<string, bool> getSupportedReferenceData() {
        return {"adapters": True, "adapter-versions": True};
    }

    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        bool versions = type == "adapter-versions";
        if (versions && !action_opts.AdapterId.val()) { return; }
        hash<auto> request = hash({"MaxResults": 100});
        if (versions) { request.AdapterId = action_opts.AdapterId; }
        list<hash<AllowedValueInfo>> result = ();
        hash<string, bool> seen;
        for (int page = 0; page < 100; ++page) {
            hash<auto> response = doTextractAction(versions ? "ListAdapterVersions" : "ListAdapters", request);
            foreach hash<auto> item in (versions ? response.AdapterVersions : response.Adapters) {
                result += <AllowedValueInfo>{"value": versions ? item.AdapterVersion : item.AdapterId,
                    "display_name": versions ? item.AdapterVersion : item.AdapterName};
            }
            if (!response.NextToken.val()) { return result; }
            if (seen{response.NextToken}) {
                throw "AWS-TEXTRACT-PAGINATION-ERROR", "adapter listing returned a repeated continuation token";
            }
            seen{response.NextToken} = True;
            request.NextToken = response.NextToken;
        }
        throw "AWS-TEXTRACT-PAGINATION-ERROR", "adapter reference listing exceeded 100 pages; use the raw list action";
    }

    #! Posts a Textract action: sets X-Amz-Target header and serializes JSON body
    private hash<auto> doTextractAction(string action_name, hash<auto> body) {
        return rest.restPost("/", body, {
            "X-Amz-Target":  "Textract." + action_name,
            "Content-Type":  "application/x-amz-json-1.1",
        }).body;
    }

    #! Builds a Textract Document hash from caller-supplied source fields
    /** Accepts either inline bytes via \c document_bytes (binary), an
        S3 reference via \c s3_bucket + \c s3_name, or a file path via
        \c document_path (read locally and uploaded as bytes).
        @param req compatible document-source options
        @param extra_fields additional input keys accepted by the calling action
        @param read_file read local file content; false validates only the source options
        @return validated native Document envelope
        @throw AWS-TEXTRACT-CONFIG-ERROR missing or conflicting document sources
        @throw AWS-TEXTRACT-INPUT-ERROR malformed source fields
    */
    static hash<auto> buildDocument(*hash<auto> req, *list<string> extra_fields, bool read_file = True) {
        foreach string field in (keys req) {
            if (!inlist(field, ("document_bytes", "document_path", "s3_bucket", "s3_name", "s3_version")
                    + (extra_fields ?? ()))) {
                throw "AWS-TEXTRACT-INPUT-ERROR", "unsupported document field " + field;
            }
        }
        bool inline_bytes = req.hasKey("document_bytes");
        bool local_file = req.hasKey("document_path");
        bool s3 = req.hasKey("s3_bucket") || req.hasKey("s3_name") || req.hasKey("s3_version");
        if (int(inline_bytes) + int(local_file) + int(s3) != 1) {
            throw "AWS-TEXTRACT-CONFIG-ERROR", "supply exactly one document source: bytes, local file, or S3";
        }
        hash<auto> document;
        if (inline_bytes) {
            if (req.document_bytes.typeCode() != NT_BINARY) {
                throw "AWS-TEXTRACT-INPUT-ERROR", "document_bytes must contain binary document data";
            }
            document = {"Bytes": req.document_bytes};
        } else if (local_file) {
            if (req.document_path.typeCode() != NT_STRING || !req.document_path.val()) {
                throw "AWS-TEXTRACT-INPUT-ERROR", "document_path must be a nonempty local file path";
            }
            document = {"Bytes": read_file ? File::readBinaryFile(req.document_path) : binary("validation")};
        } else {
            foreach string field in ("s3_bucket", "s3_name") {
                if (req{field}.typeCode() != NT_STRING || !req{field}.val()) {
                    throw "AWS-TEXTRACT-INPUT-ERROR", field + " must be a nonempty string for an S3 document";
                }
            }
            document = {"S3Object": {"Bucket": req.s3_bucket, "Name": req.s3_name}};
            if (req.hasKey("s3_version")) { document.S3Object.Version = req.s3_version; }
        }
        AwsTextractSchema::validate("Document", document);
        return {"Document": document};
    }
}
}
