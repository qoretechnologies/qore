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
#! Routes Bedrock control-plane and runtime paths to their regional endpoints
/** Both endpoints use the same SigV4 service name and connection manager.  A single routing client lets
    schema-derived runtime actions use control-plane reference-data listings without leaking endpoint
    selection into action implementations.
*/
public class BedrockEndpointRestClientIo inherits AwsRestClient::AwsRestClientIo {
    private {
        #! Control-plane client sharing this client's connection manager
        *AwsRestClient::AwsRestClientIo control;

        #! Retains the supplied manager's owner for this router and every child action using it
        *AwsRestClient::AwsRestClientIo source_client;
    }

    #! Creates a routing client
    /** @param opts AWS REST client options for the runtime endpoint
        @param shared_mgr optional shared connection manager
        @param source_client optional client owning the shared manager; retained for this router's lifetime

        @throw AWSRESTCLIENT-ERROR for invalid AWS client options
    */
    constructor(hash<auto> opts, *HttpClientIo::HttpClientConnectionManager shared_mgr,
            *AwsRestClient::AwsRestClientIo source_client)
            : AwsRestClient::AwsRestClientIo(opts, shared_mgr) {
        self.source_client = source_client;
        string control_url = opts.bedrock_control_url
            ?? BedrockRestClient::BedrockRestConnection::getControlPlaneUrl(getUrl());
        if (control_url != getUrl()) {
            control = new AwsRestClient::AwsRestClientIo((opts - "bedrock_control_url")
                + {"url": control_url}, getConnectionManager());
        }
    }

    #! Sends a request to the endpoint owning its path
    /** @param method HTTP method
        @param path request path
        @param body request body
        @param hdr optional request headers
        @return the REST response
    */
    hash<RestClient::RestResponseInfo> restDoRequest(string method, string path, auto body,
            *hash<auto> hdr) {
        return control && !BedrockEndpointRestClientIo::isRuntimePath(path)
            ? control.restDoRequest(method, path, body, hdr)
            : AwsRestClient::AwsRestClientIo::restDoRequest(method, path, body, hdr);
    }

    #! Sends a pre-serialized request to the endpoint owning its path
    /** @param method HTTP method
        @param path request path
        @param body pre-serialized request body
        @param hdr optional request headers
        @return the REST response
    */
    hash<RestClient::RestResponseInfo> restDoRawRequest(string method, string path, *data body,
            *hash<auto> hdr) {
        return control && !BedrockEndpointRestClientIo::isRuntimePath(path)
            ? control.restDoEncodedRawRequest(method, path, body, hdr)
            : AwsRestClient::AwsRestClientIo::restDoEncodedRawRequest(method, path, body, hdr);
    }

    #! Returns true for paths belonging to the Bedrock Runtime service
    private static bool isRuntimePath(string path) {
        return path =~ /^\/(?:async-invoke(?:\/|\?|$)|guardrail\/|guardrail-checks\/|model\/)/;
    }

    #! Preserves the supplied manager's owner when the router is copied
    private RestClient::AbstractRestClient copySelf(hash<auto> opts) {
        return new BedrockEndpointRestClientIo(opts, getConnectionManager(), source_client);
    }
}

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
            "aws_role_session_name": <DataProviderOptionInfo>{
                "display_name": "STS Role Session Name",
                "short_desc": "Optional name for the assumed-role session",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Session name sent to AWS STS when `aws_role_arn` is set.",
                "default_value": "qore-sdk-session",
            },
            "aws_external_id": <DataProviderOptionInfo>{
                "display_name": "STS External ID",
                "short_desc": "Optional external ID required by the role",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "External ID sent to AWS STS when the target role requires one.",
                "sensitive": True,
            },
            "aws_sts_endpoint": <DataProviderOptionInfo>{
                "display_name": "STS Endpoint",
                "short_desc": "Optional AWS STS endpoint override",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Optional AWS STS endpoint override for tests or private AWS-compatible environments.",
            },
            "aws_imds_disabled": <DataProviderOptionInfo>{
                "display_name": "Disable Instance Metadata",
                "short_desc": "Disable ECS and EC2 instance-role credential lookup",
                "type": AbstractDataProviderTypeMap."*bool",
                "desc": "Set to `True` to disable ECS task-role and EC2 IMDSv2 credential lookup.",
                "default_value": False,
            },
            "aws_imds_url": <DataProviderOptionInfo>{
                "display_name": "Instance Metadata Endpoint",
                "short_desc": "Optional EC2 instance metadata endpoint override",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Optional IMDSv2 endpoint override for tests or private environments.",
            },
            "aws_imds_timeout": <DataProviderOptionInfo>{
                "display_name": "Instance Metadata Timeout",
                "short_desc": "Credential metadata timeout in milliseconds",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "Connection and response timeout for ECS and IMDS credential requests, in milliseconds.",
            },
            "runtime_url": <DataProviderOptionInfo>{
                "display_name": "Runtime Endpoint",
                "short_desc": "Optional Bedrock Runtime endpoint override",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Optional Bedrock Runtime endpoint override for private or AWS-compatible environments.",
            },
            "control_url": <DataProviderOptionInfo>{
                "display_name": "Control-Plane Endpoint",
                "short_desc": "Optional Bedrock control-plane endpoint override",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Optional Bedrock control-plane endpoint override for private or AWS-compatible environments.",
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
    /** @param rest AWS client whose credentials and connection manager are retained

        @throw AWSRESTCLIENT-ERROR if a routing client cannot be constructed from the supplied options
    */
    constructor(AwsRestClient::AwsRestClientIo rest) {
        if (rest instanceof BedrockEndpointRestClientIo) {
            self.rest = rest;
        } else {
            self.rest = new BedrockEndpointRestClientIo(rest.getConstructorOptions(), rest.getConnectionManager(),
                rest);
        }
    }

    #! Creates the object from constructor options
    /** @param options provider and AWS authentication options

        @throw CONSTRUCTOR-ERROR if the options are invalid or omit both `aws_region` and `restclient`
        @throw AWSRESTCLIENT-ERROR if the AWS client options are invalid
    */
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            AwsRestClient::AwsRestClientIo supplied = copts.restclient;
            rest = supplied instanceof BedrockEndpointRestClientIo
                ? supplied
                : new BedrockEndpointRestClientIo(supplied.getConstructorOptions(),
                    supplied.getConnectionManager(), supplied);
            return;
        }
        if (!copts.aws_region.val()) {
            throw "CONSTRUCTOR-ERROR",
                "BedrockDataProvider requires 'aws_region' when no 'restclient' is supplied";
        }
        hash<auto> opts = {
            "url": copts.runtime_url
                ?? "https://" + sprintf(DefaultRuntimeHostPattern, copts.aws_region),
            "aws_region": copts.aws_region,
            "aws_service": "bedrock",
        };
        foreach string key in (("aws_keyid", "aws_secret", "aws_token",
                "aws_role_arn", "aws_role_session_name", "aws_external_id",
                "aws_sts_endpoint", "aws_imds_disabled", "aws_imds_url",
                "aws_imds_timeout")) {
            if (exists copts{key}) {
                opts{key} = copts{key};
            }
        }
        if (copts.control_url.val()) {
            opts.bedrock_control_url = copts.control_url;
        }
        rest = new BedrockEndpointRestClientIo(opts);
    }

    #! Returns the REST client
    /** @return the routing AWS REST client used by this provider */
    *AwsRestClient::AwsRestClientIo getRestClient() {
        return rest;
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    /** @param logger logger to use, or @ref nothing to clear logging */
    setLogger(*LoggerInterface logger) {
        LoggerWrapper::setLogger(logger);
        rest.setLogger(logger);
    }

    #! Makes a REST call against the Bedrock runtime with retry on transient errors
    private hash<auto> doRestCommand(string method, string path, auto body, *hash<auto> hdr) {
        int retries = 0;
        while (True) {
            try {
                # Curated providers already encode the model ID as a URI segment, just like schema actions.
                return rest.restDoRawRequest(method.upr(), path, exists body ? binary(make_json(body)) : NOTHING,
                    {"Content-Type": MimeTypeJson, "Accept": MimeTypeJson} + (hdr ?? {}));
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
