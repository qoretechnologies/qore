# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BedrockDataProvider common base class

/** BedrockDataProviderCommon.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BedrockDataProvider module
public namespace BedrockDataProvider {
#! The Bedrock data provider common base class
/** Wraps an @ref AwsRestClient::AwsRestClientIo configured for the
    \c bedrock-runtime service so subproviders can issue signed REST calls
    against \c bedrock-runtime.\{region\}.amazonaws.com without
    re-implementing SigV4.
*/
public class BedrockDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        #! Default Bedrock runtime hostname pattern
        const DefaultRuntimeHostPattern = "bedrock-runtime.%s.amazonaws.com";

        #! Constructor options
        const ConstructorOptions = {
            "aws_region": <DataProviderOptionInfo>{
                "display_name": "AWS Region",
                "short_desc": "The AWS region for Bedrock runtime calls",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The AWS region for Bedrock runtime calls (e.g. `us-east-1`)",
            },
            "aws_keyid": <DataProviderOptionInfo>{
                "display_name": "AWS Access Key ID",
                "short_desc": "AWS access key ID for SigV4 signing",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "AWS access key ID for SigV4 signing; omit to use the "
                    "standard AWS credential chain (env vars / ECS task-role / "
                    "IMDSv2 / STS AssumeRole)",
            },
            "aws_secret": <DataProviderOptionInfo>{
                "display_name": "AWS Secret Access Key",
                "short_desc": "AWS secret access key for SigV4 signing",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "AWS secret access key for SigV4 signing",
                "sensitive": True,
            },
            "aws_token": <DataProviderOptionInfo>{
                "display_name": "AWS Session Token",
                "short_desc": "STS-issued temporary session token",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "STS-issued temporary session token (when using "
                    "temporary credentials)",
                "sensitive": True,
            },
            "aws_role_arn": <DataProviderOptionInfo>{
                "display_name": "STS Role ARN",
                "short_desc": "Optional STS AssumeRole ARN",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Optional STS AssumeRole ARN — when set, the data "
                    "provider performs an AssumeRole call with the base "
                    "credentials and uses the returned temporary credentials",
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "Pre-built AwsRestClientIo to use",
                "type": AbstractDataProviderType::get(
                    new Type("AwsRestClient::AwsRestClientIo"), NOTHING, {
                        DTT_ClientOnly: True,
                    }),
                "desc": "A pre-built AwsRestClientIo to use; when provided, "
                    "the credential options above are ignored",
            },
        };

        #! IO errors for REST retries
        const RetrySet = {
            "SOCKET-SSL-ERROR": True,
            "SOCKET-CLOSED": True,
        };

        #! Max retries
        const MaxIoRetries = 5;
    }

    private {
        #! The REST client object for API calls (async-capable)
        *AwsRestClient::AwsRestClientIo rest;
    }

    #! Creates the object from a pre-built AwsRestClientIo
    constructor(AwsRestClient::AwsRestClientIo rest) {
        self.rest = rest;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
            return;
        }
        if (!copts.aws_region.val()) {
            throw "CONSTRUCTOR-ERROR",
                "BedrockDataProvider requires 'aws_region' when no 'restclient' is supplied";
        }
        hash<auto> opts = {
            "url": "https://" + sprintf(DefaultRuntimeHostPattern, copts.aws_region),
            "aws_region": copts.aws_region,
            "aws_service": "bedrock",
        };
        foreach string key in (("aws_keyid", "aws_secret", "aws_token",
                "aws_role_arn")) {
            if (copts{key}.val()) {
                opts{key} = copts{key};
            }
        }
        rest = new AwsRestClient::AwsRestClientIo(opts);
    }

    #! Returns the REST client
    *AwsRestClient::AwsRestClientIo getRestClient() {
        return rest;
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        LoggerWrapper::setLogger(logger);
    }

    #! Makes a REST call against the Bedrock runtime with retry on transient errors
    private hash<auto> doRestCommand(string method, string path, auto body, *hash<auto> hdr) {
        int retries = 0;
        while (True) {
            try {
                switch (method.upr()) {
                    case "GET":  return rest.restGet(path, hdr);
                    case "POST": return rest.restPost(path, body, hdr);
                    case "PUT":  return rest.restPut(path, body, hdr);
                    case "DELETE": return rest.restDel(path, body, hdr);
                    default: return rest.restDoRequest(method, path, body, hdr);
                }
            } catch (hash<ExceptionInfo> ex) {
                if (RetrySet{ex.err} && ++retries < MaxIoRetries) {
                    warn("Bedrock transient error: %s: %s; retrying", ex.err, ex.desc);
                    continue;
                }
                if (ex.err == "REST-RESPONSE-ERROR" && ex.arg.status_code == 429
                        && ++retries < MaxIoRetries) {
                    int wait_secs = 1 << retries;
                    warn("Bedrock rate-limited; waiting %ds before retry %d", wait_secs, retries);
                    sleep(wait_secs);
                    continue;
                }
                rethrow ex.err,
                    sprintf("%s (path: %y)", ex.desc, ex.arg.request_path ?? path),
                    ex.arg;
            }
        }
    }
}
}
