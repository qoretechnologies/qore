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
                "desc": "Address buckets in the URI path (`https://s3.<region>.amazonaws.com/<bucket>/<key>`) "
                    "rather than in the hostname.  This is the default and currently the only supported mode: "
                    "virtual-host addressing needs a separate endpoint per bucket, which this module does not "
                    "yet create.  Path-style addressing works against Amazon S3 for existing buckets and is "
                    "required by most S3-compatible services such as MinIO and Ceph",
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
        bool path_style = True;
    }

    #! Creates the object from an AwsRestClient
    constructor(AwsRestClient::AwsRestClient rest, bool path_style = True) {
        self.rest = rest;
        self.path_style = path_style;
    }

    #! Creates the object from constructor options
    /** @param options the constructor options
        @param con_opts the option set to validate \a options against; a watch provider passes its own larger
        set, since @ref checkOptions() rejects any option the given set does not declare and a watch provider
        takes options (the poll interval, the bucket to watch) that an action provider does not
    */
    constructor(*hash<auto> options,
            hash<string, hash<DataProviderOptionInfo>> con_opts = ConstructorOptions) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", con_opts, options);
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

    #! Returns the reference data types supported by this provider
    /** Every action in this module takes a bucket name, and each declares \c "buckets" as its reference data;
        without this the bucket picker is empty on every form
    */
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "buckets": True,
        };
    }

    #! Returns reference data for the given type
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "buckets": return getBucketReferenceData();
        }
    }

    #! Returns the account's buckets as reference values
    /** @return the reference values, or an empty list if they cannot be retrieved

        @note Errors are logged and swallowed: reference data populates a UI picker, and expired credentials or
        a policy that does not allow \c ListAllMyBuckets must degrade to an empty list rather than break the form
    */
    private *list<hash<AllowedValueInfo>> getBucketReferenceData() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> resp = doS3Request("GET", NOTHING, NOTHING);
            *hash<auto> doc = AwsS3DataProviderBase::parseXml(resp.body).ListAllMyBucketsResult;
            foreach hash<auto> b in (AwsS3DataProviderBase::forceList(doc.Buckets.Bucket)) {
                if (!b.Name.val()) {
                    continue;
                }
                rv += <AllowedValueInfo>{
                    "display_name": b.Name,
                    "short_desc": b.Name,
                    "desc": b.CreationDate.val()
                        ? sprintf("%s (created %s)", b.Name, b.CreationDate)
                        : b.Name,
                    "value": b.Name,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getBucketReferenceData(): %s: %s", ex.err, ex.desc);
        }
        return rv;
    }

    #! Parses an S3 XML response body into a hash
    /** @param body the response body; either the raw document or a hash the REST client already deserialized

        @return the parsed document, or @ref nothing if the body is empty

        @throw S3-XML-UNAVAILABLE the \c xml module is not available

        Every code path in this module that has to read an S3 response body goes through this method, so that the
        missing-module case produces one clear, actionable error rather than an obscure failure at the point of
        use.

        @note The REST client deserializes a response according to its content type, so an S3 response arrives
        already parsed whenever the \c xml module is present - which is exactly when this module works at all.
        The raw form is still handled, since a response can also arrive as a string or as bytes depending on the
        content type the server sets.
    */
    static *hash<auto> parseXml(auto body) {
        if (!body.val()) {
            return;
        }
        if (body.typeCode() == NT_HASH) {
            # already deserialized by the REST client
            return body;
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

    #! Verifies that the addressing mode in use can be served by the configured client
    /** Virtual-host addressing puts the bucket in the hostname, so each bucket is a different endpoint and needs
        its own client; a REST client sends every request to its configured host, and passing an absolute URI as
        the request path does not retarget it -- the URI is sent as a literal path and the request still reaches
        the configured host.

        Rather than issue a request that would be signed and routed for the wrong host, this reports the
        situation and names the option that resolves it.  Path-style addressing works against Amazon S3 for
        existing buckets and is required by most S3-compatible services, so it is the default here.

        @param bucket the bucket name, if any

        @throw S3-ADDRESSING-ERROR virtual-host addressing was selected, which is not yet supported
    */
    private checkAddressing(*string bucket) {
        if (bucket.val() && !path_style) {
            throw "S3-ADDRESSING-ERROR", sprintf("cannot address bucket %y: virtual-host addressing "
                "(https://<bucket>.s3.<region>.amazonaws.com) requires a separate endpoint per bucket, which "
                "this module does not yet create; set the 'path_style' connection option to address buckets in "
                "the URI path instead", bucket);
        }
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

        checkAddressing(bucket);

        int retries = 0;
        while (True) {
            try {
                hash<auto> info;
                debug("S3 %s %y", method, path);
                return rest.doRequest(method, path, body, \info, True, hdr);
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
            # NOTE: widened with "+ {}" first; a REST client raises its errors with a hashdecl argument in some
            # configurations, and reading a member a hashdecl does not declare throws INVALID-MEMBER, which the
            # catch below would then swallow along with the S3 error code this exists to report
            *hash<auto> arg = ex.arg.typeCode() == NT_HASH ? ex.arg + {} : NOTHING;
            err = AwsS3DataProviderBase::parseXml(arg."response-body" ?? arg.body).Error;
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
