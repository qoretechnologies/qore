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
                "type": AbstractDataProviderTypeMap."string",
                "desc": "AWS region (e.g., `us-east-1`, `eu-west-1`).  Determines "
                    "the Textract endpoint hostname.",
                "required": True,
                "example_value": "us-east-1",
            },
            "aws_keyid": <DataProviderOptionInfo>{
                "display_name": "AWS Access Key ID",
                "short_desc": "AWS access key ID for static credentials",
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
            if (!copts.aws_region.val()) {
                throw "CONSTRUCTOR-ERROR",
                    "missing required 'aws_region' argument for AWS Textract communication";
            }
            hash<auto> opts = {"aws_region": copts.aws_region};
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
    */
    static hash<auto> buildDocument(*hash<auto> req) {
        if (exists req.document_bytes) {
            return {"Document": {"Bytes": req.document_bytes}};
        }
        if (req.document_path.val()) {
            return {"Document": {"Bytes": File::readBinaryFile(req.document_path)}};
        }
        if (req.s3_bucket.val() && req.s3_name.val()) {
            hash<auto!> s3;
            s3.Bucket = req.s3_bucket;
            s3.Name   = req.s3_name;
            if (req.s3_version.val()) {
                s3.Version = req.s3_version;
            }
            return {"Document": {"S3Object": s3}};
        }
        throw "AWS-TEXTRACT-CONFIG-ERROR",
            "one of document_bytes / document_path / (s3_bucket + s3_name) is required";
    }
}
}
