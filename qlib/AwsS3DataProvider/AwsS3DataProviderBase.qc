# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AwsS3DataProviderBase.qc common base class for the AwsS3DataProvider module

/*  AwsS3DataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The Amazon S3 data provider common base class
/** Holds the REST client and the request plumbing shared by every action provider in this module.

    @note Amazon S3 speaks XML, so this module requires the \c xml module.  The dependency is declared with
    \c %%try-module rather than \c %%requires: the \c xml module is built from a separate repository and is not
    guaranteed to be installed, and making it a hard dependency would prevent this module from loading at all --
    and therefore prevent its application and actions from appearing in the catalog -- on installations that do
    not have it.  Every request path funnels through @ref parseXml(), which raises a single clear error naming
    the requirement when the module is unavailable.
*/
public class AwsS3DataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Constructor options
        const ConstructorOptions = {
            "aws_keyid": <DataProviderOptionInfo>{
                "display_name": "AWS Key ID",
                "short_desc": "The AWS access key ID",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The AWS access key ID to use for authentication",
            },
            "aws_secret": <DataProviderOptionInfo>{
                "display_name": "AWS Secret",
                "short_desc": "The AWS secret access key",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The AWS secret access key to use for authentication",
                "sensitive": True,
            },
            "aws_region": <DataProviderOptionInfo>{
                "display_name": "AWS Region",
                "short_desc": "The AWS region to use",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The AWS region holding the buckets to be used (ex: `us-east-1`)",
                "example_value": "us-east-1",
            },
            "aws_token": <DataProviderOptionInfo>{
                "display_name": "AWS Token",
                "short_desc": "A temporary session token",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "A temporary session token from the AWS Security Token Service",
                "sensitive": True,
            },
            "path_style": <DataProviderOptionInfo>{
                "display_name": "Path-Style Addressing",
                "short_desc": "Address buckets in the URI path rather than in the hostname",
                "type": AbstractDataProviderTypeMap."*bool",
                "desc": "Address buckets in the URI path rather than in the hostname; required by most "
                    "S3-compatible services such as MinIO and Ceph",
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(new Type("AwsRestClient::AwsRestClient"), NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "The REST client object to use",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Options used when creating a REST client",
                "type": AbstractDataProviderType::get(AutoHashType, NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "Options to the `AwsRestClient` constructor; only used if a REST client object is "
                    "created for a call",
            },
        };

        #! I/O errors worth retrying
        const RetrySet = {
            "SOCKET-SSL-ERROR": True,
            "SOCKET-CLOSED": True,
        };

        #! Maximum I/O retries
        const MaxIoRetries = 3;

        #! HTTP status codes indicating the bucket lives in another region
        const RedirectCodes = {
            301: True,
            307: True,
        };
    }

    private {
        #! The REST client used for API calls
        AwsRestClient::AwsRestClient rest;

        #! If buckets are addressed in the URI path rather than in the hostname
        bool path_style = False;
    }

    #! Creates the object from an AwsRestClient
    constructor(AwsRestClient::AwsRestClient rest, bool path_style = False) {
        self.rest = rest;
        self.path_style = path_style;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
        } else {
            if (!copts.aws_keyid.val() || !copts.aws_secret.val()) {
                throw "CONSTRUCTOR-ERROR", "missing 'aws_keyid' and 'aws_secret' options required for Amazon S3 "
                    "communication";
            }
            if (!copts.aws_region.val()) {
                throw "CONSTRUCTOR-ERROR", "missing the 'aws_region' option required for Amazon S3 communication";
            }
            rest = new AwsRestClient::AwsRestClient((copts.restclient_options ?? {}) + {
                "url": sprintf("https://s3.%s.amazonaws.com", copts.aws_region),
                "aws_keyid": copts.aws_keyid,
                "aws_secret": copts.aws_secret,
                "aws_region": copts.aws_region,
                "aws_service": "s3",
                "aws_s3": True,
                "aws_token": copts.aws_token,
                "data": "bin",
            });
        }
        if (exists copts.path_style) {
            path_style = copts.path_style;
        }
    }

    #! Returns the REST client object
    AwsRestClient::AwsRestClient getRestClient() {
        return rest;
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        LoggerWrapper::setLogger(logger);
    }

    #! Parses an S3 XML response body into a hash
    /** @param body the response body

        @return the parsed document, or @ref nothing if the body is empty

        @throw S3-XML-UNAVAILABLE the \c xml module is not available

        Every code path in this module that has to read an S3 response body goes through this method, so that the
        missing-module case produces one clear, actionable error rather than an obscure failure at the point of
        use.
    */
    static *hash<auto> parseXml(*data body) {
        if (!body.val()) {
            return;
        }
%ifdef NoXml
        throw "S3-XML-UNAVAILABLE", "the Amazon S3 API returns XML, so this module requires the 'xml' module, "
            "which is not available on this installation; install the 'xml' module (from the module-xml "
            "repository) to use Amazon S3";
%else
        string str = body.typeCode() == NT_BINARY ? body.toString("UTF-8") : body;
        if (!str.val()) {
            return;
        }
        return parse_xml(str);
%endif
    }

    #! Returns the value of an S3 XML element as a list, whatever its cardinality
    /** The XML parser returns a single repeated element as a hash and several as a list of hashes; callers that
        iterate results must not have to special-case the one-element document, which is exactly the shape a
        test with a single object produces and a production bucket does not.

        @param value the parsed value

        @return the value as a list; an empty list if the value is not present
    */
    static list<auto> forceList(auto value) {
        if (!exists value) {
            return ();
        }
        return value.typeCode() == NT_LIST ? value : (value,);
    }

    #! Returns the URI path for a bucket and optional key, honoring the addressing mode
    /** @param bucket the bucket name
        @param key the object key, if any

        @return the URI path to use in the request
    */
    private string getPath(*string bucket, *string key) {
        list<string> segments = ();
        if (bucket.val() && path_style) {
            segments += bucket;
        }
        if (key.val()) {
            # the key may contain "/" characters, which are path separators and must not be encoded
            segments += (map encode_url($1, True), key.split("/"));
        }
        return "/" + (foldl $1 + "/" + $2, segments);
    }

    #! Returns the base URL to use for a request, honoring the addressing mode
    /** With virtual-host addressing the bucket is part of the hostname, so the request has to go to a different
        host than the client's configured URL.

        @param bucket the bucket name, if any

        @return the URL to use, or @ref nothing to use the client's configured URL
    */
    private *string getRequestUrl(*string bucket) {
        if (!bucket.val() || path_style) {
            return;
        }
        hash<UrlInfo> info = parse_url(rest.getURL());
        return sprintf("%s://%s.%s", info.protocol, bucket, info.host);
    }

    #! Makes an S3 request and returns the response
    /** @param method the HTTP method
        @param bucket the bucket, if any
        @param key the object key, if any
        @param query any query arguments
        @param body the request body, if any
        @param hdr any additional request headers

        @return the response hash

        @throw S3-API-ERROR the request failed; the S3 error code and message are included when the response
        carried them
    */
    private hash<auto> doS3Request(string method, *string bucket, *string key, *hash<auto> query, auto body,
            *hash<auto> hdr) {
        string path = getPath(bucket, key);
        if (query) {
            path += "?" + (foldl $1 + "&" + $2, (map
                exists query{$1} && query{$1} !== True
                    ? sprintf("%s=%s", encode_url($1, True), encode_url(query{$1}.toString(), True))
                    : encode_url($1, True),
                keys query));
        }

        *string url = getRequestUrl(bucket);
        int retries = 0;
        while (True) {
            try {
                hash<auto> info;
                # a URL override is applied by sending an absolute URI, which the client signs against the
                # correct host
                string target = url.val() ? url + path : path;
                debug("S3 %s %y", method, target);
                return rest.doRequest(method, target, body, \info, True, hdr);
            } catch (hash<ExceptionInfo> ex) {
                if (RetrySet{ex.err} && retries < MaxIoRetries) {
                    ++retries;
                    debug("S3 I/O error %s; retrying (%d/%d)", ex.err, retries, MaxIoRetries);
                    continue;
                }
                rethrowS3Error(ex, method, bucket, key);
            }
        }
    }

    #! Rethrows an exception from an S3 request with the S3 error code and message when the response carried them
    /** S3 reports failures as an XML document in the response body; without unpacking it the caller sees only
        an HTTP status code, which does not distinguish "no such bucket" from "access denied".

        @param ex the original exception
        @param method the HTTP method used
        @param bucket the bucket, if any
        @param key the object key, if any
    */
    private nothing rethrowS3Error(hash<ExceptionInfo> ex, string method, *string bucket, *string key) {
        *hash<auto> err;
        try {
            err = AwsS3DataProviderBase::parseXml(ex.arg."response-body" ?? ex.arg.body).Error;
        } catch (hash<ExceptionInfo> ignored) {
            # the body was absent or not XML; fall through and report the original error unchanged
        }
        string target = bucket.val()
            ? (key.val() ? sprintf("%s/%s", bucket, key) : bucket)
            : "the S3 endpoint";
        if (err.Code.val()) {
            throw "S3-API-ERROR", sprintf("%s %s failed: %s: %s", method, target, err.Code,
                err.Message ?? ex.desc), ex.arg;
        }
        # NOTE: "throw" rather than "rethrow"; rethrow is only valid directly inside a catch block, and this
        # method is called from one rather than being one.  The original code and argument are preserved in the
        # message and argument so nothing is lost
        throw "S3-API-ERROR", sprintf("%s %s failed: %s: %s", method, target, ex.err, ex.desc), ex.arg;
    }
}
}
