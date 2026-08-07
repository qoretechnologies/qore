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
                "type": AbstractDataProviderType::get(new Type("AwsRestClient::AwsRestClientIo"), NOTHING, {
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
                "desc": "Options to the `AwsRestClientIo` constructor; only used if a REST client object is "
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
        AwsRestClient::AwsRestClientIo rest;

        #! If buckets are addressed in the URI path rather than in the hostname
        bool path_style = True;
    }

    #! Creates the object from an AwsRestClientIo
    constructor(AwsRestClient::AwsRestClientIo rest, bool path_style = True) {
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
            rest = new AwsRestClient::AwsRestClientIo((copts.restclient_options ?? {}) + {
                "url": sprintf("https://s3.%s.amazonaws.com", copts.aws_region),
                # NOTE: "data" must stay "bin".  S3 object content is raw bytes; with the client left on its
                # JSON default every uploaded object is stored as a quoted base64 string rather than as its
                # content.  AwsRestClientIo applies {"data": "json"} as a default (left of "+"), so this
                # pinned value wins
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
    AwsRestClient::AwsRestClientIo getRestClient() {
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

    #! Returns the full request path, with the bucket, the key, and any query arguments
    /** Shared by the blocking request path and the polling one, so that a polled read addresses -- and
        therefore signs -- exactly the URI a blocking read would.

        @param bucket the bucket, if any
        @param key the object key, if any
        @param query any query arguments

        @return the request path
    */
    private string getRequestPath(*string bucket, *string key, *hash<auto> query) {
        string path = getPath(bucket, key);
        if (query) {
            path += "?" + (foldl $1 + "&" + $2, (map
                exists query{$1} && query{$1} !== True
                    ? sprintf("%s=%s", encode_url($1, True), encode_url(query{$1}.toString(), True))
                    : encode_url($1, True),
                keys query));
        }
        return path;
    }

    #! Returns a caller-driven poll operation for an S3 request
    /** The non-blocking counterpart of @ref doS3Request(): the request is prepared and signed here, and the
        caller drives it with @ref Qore::AbstractPollOperation::continuePoll() "continuePoll()".

        @param method the HTTP method
        @param bucket the bucket, if any
        @param key the object key, if any
        @param query any query arguments

        @return a poll operation whose output is the response body

        @note Signing is not repeated here.  @ref AwsRestClient::AwsRestClientIo "AwsRestClientIo" signs each
        async attempt as it is prepared, so the poller and the blocking call produce the same signature over
        the same URI; a poller that signed separately would be one edit away from \c SignatureDoesNotMatch
    */
    private Qore::AbstractPollOperation startPollS3Request(string method, *string bucket, *string key,
            *hash<auto> query) {
        string path = getRequestPath(bucket, key, query);
        checkAddressing(bucket);
        debug("S3 poll %s %y", method, path);
        return new AwsS3PollOperation(rest, method, path, bucket, key);
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
        string path = getRequestPath(bucket, key, query);

        checkAddressing(bucket);

        int retries = 0;
        while (True) {
            try {
                debug("S3 %s %y", method, path);
                # NOTE: "restDoRequestAssumingXml()", not "restDoRequest()".  S3 answers
                # CreateMultipartUpload and CompleteMultipartUpload with XML and no Content-Type header at
                # all, which the REST client refuses to deserialize; S3 only ever returns XML in that
                # position
                return AwsS3DataProviderBase::normalizeResponse(
                    rest.restDoRequestAssumingXml(method, path, body, hdr));
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

    #! Uploads content to S3, using a multipart upload when the content is large
    /** Amazon S3 rejects a single-request upload above 5 GB, and a large one has to start again from the
        beginning if the connection drops.  Above @ref MultipartThreshold the content is therefore sent as a
        multipart upload, which commits each part as it arrives.

        @param bucket the bucket to write to
        @param key the object key
        @param content the content to store
        @param hdr the request headers, which carry the content type and any storage class or encryption

        @return a hash with the \c etag and \c version_id of the stored object
    */
    private hash<auto> uploadContent(string bucket, string key, binary content, hash<auto> hdr) {
        if (content.size() <= MultipartThreshold) {
            hash<auto> resp = doS3Request("PUT", bucket, key, NOTHING, content, hdr);
            return {
                "etag": AwsS3ListObjectsDataProvider::stripEtag(resp.hdr.etag),
                "version_id": resp.hdr."x-amz-version-id",
            };
        }
        return uploadMultipart(bucket, key, new BinaryInputStream(content), hdr);
    }

    #! Uploads the content of an input stream, sending each part as it is read
    /** The total size is not known in advance, so the first part decides the transfer method: content that
        ends within it is sent in a single request, and anything larger opens a multipart upload.  Only one
        part is held in memory at a time, so a file larger than memory can be written.

        Each part is a separate signed request with a known body, so SigV4 needs no streaming-signature work.

        @param bucket the bucket to write to
        @param key the object key
        @param is the stream to read the content from
        @param hdr the request headers

        @return a hash with the \c etag and \c version_id of the stored object

        @throw S3-UPLOAD-ERROR the server opened an upload without giving it an ID

        @note A multipart upload that is abandoned leaves its parts stored and billed until a lifecycle rule
        removes them, so a failure part-way through aborts the upload rather than leaving it open.
    */
    private hash<auto> uploadMultipart(string bucket, string key, Qore::InputStream is, hash<auto> hdr) {
        binary buf = AwsS3DataProviderBase::readExactly(is, MultipartPartSize);
        if (buf.size() < MultipartPartSize) {
            # the whole object arrived within the first part; a multipart upload would cost two extra requests
            hash<auto> resp = doS3Request("PUT", bucket, key, NOTHING, buf, hdr);
            return {
                "etag": AwsS3ListObjectsDataProvider::stripEtag(resp.hdr.etag),
                "version_id": resp.hdr."x-amz-version-id",
            };
        }

        # the create request carries the metadata headers; the part requests carry none of them
        hash<auto> start = doS3Request("POST", bucket, key, {"uploads": True}, NOTHING, hdr);
        *string upload_id = AwsS3DataProviderBase::parseXml(start.body).InitiateMultipartUploadResult.UploadId;
        if (!upload_id.val()) {
            throw "S3-UPLOAD-ERROR", sprintf("the multipart upload of %s/%s was not given an upload ID by the "
                "server", bucket, key);
        }
        debug("S3 multipart upload %y for %s/%s", upload_id, bucket, key);

        list<hash<auto>> parts = ();
        try {
            int part_number = 1;
            while (True) {
                hash<auto> resp = doS3Request("PUT", bucket, key, {
                    "partNumber": part_number,
                    "uploadId": upload_id,
                }, buf);
                *string etag = AwsS3ListObjectsDataProvider::stripEtag(resp.hdr.etag);
                if (!etag.val()) {
                    throw "S3-UPLOAD-ERROR", sprintf("part %d of the multipart upload of %s/%s was stored "
                        "without an entity tag, so the upload cannot be completed", part_number, bucket, key);
                }
                parts += {"number": part_number, "etag": etag};

                buf = AwsS3DataProviderBase::readExactly(is, MultipartPartSize);
                if (!buf.size()) {
                    break;
                }
                ++part_number;
            }

            string body = AwsS3DataProviderBase::getCompleteBody(parts);
            hash<auto> resp = doS3Request("POST", bucket, key, {"uploadId": upload_id}, body, {
                "Content-Type": MimeTypeXml,
            });
            # completion can fail with a 200 status and an error document in the body, so the result has to be
            # inspected rather than trusted
            *hash<auto> doc = AwsS3DataProviderBase::parseXml(resp.body);
            if (doc.Error.Code.val()) {
                throw "S3-API-ERROR", sprintf("completing the multipart upload of %s/%s failed: %s: %s",
                    bucket, key, doc.Error.Code, doc.Error.Message);
            }
            # NOTE: a multipart upload reports its entity tag in the completion document rather than in a
            # header, and that tag is not the MD5 of the content: it carries a "-<parts>" suffix instead
            return {
                "etag": AwsS3ListObjectsDataProvider::stripEtag(doc.CompleteMultipartUploadResult.ETag),
                "version_id": resp.hdr."x-amz-version-id",
            };
        } catch (hash<ExceptionInfo> ex) {
            # an abandoned multipart upload keeps its parts stored and billed, so it is aborted here; the
            # abort is best-effort, since the original failure is what the caller has to see
            try {
                doS3Request("DELETE", bucket, key, {"uploadId": upload_id});
                debug("S3 aborted multipart upload %y after a failure", upload_id);
            } catch (hash<ExceptionInfo> ignored) {
                LoggerWrapper::error(sprintf("could not abort the multipart upload %y of %s/%s after a "
                    "failure; its parts remain stored and billed until a lifecycle rule removes them: %s: %s",
                    upload_id, bucket, key, ignored.err, ignored.desc));
            }
            rethrow;
        }
    }

    #! Returns the XML document naming the parts of a completed multipart upload
    /** @param parts the parts, each with a \c number and an \c etag

        @return the request body
    */
    static string getCompleteBody(list<hash<auto>> parts) {
        string body = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><CompleteMultipartUpload>";
        foreach hash<auto> p in (parts) {
            body += sprintf("<Part><PartNumber>%d</PartNumber><ETag>&quot;%s&quot;</ETag></Part>", p.number,
                AwsS3ExtendedXml::escape(p.etag));
        }
        return body + "</CompleteMultipartUpload>";
    }

    #! Reads exactly the given number of bytes from a stream, or fewer if the stream ends first
    /** A stream read may return fewer bytes than asked for without being at the end, so a single read cannot
        be used to decide that the content is finished; that decision selects a single-request upload over a
        multipart one, and getting it wrong would truncate the object.

        @param is the stream to read from
        @param n the number of bytes wanted

        @return the bytes read; shorter than \a n only when the stream ended
    */
    static binary readExactly(Qore::InputStream is, int n) {
        binary rv = binary();
        while (rv.size() < n) {
            *binary b = is.read(n - rv.size());
            if (!b) {
                break;
            }
            rv += b;
        }
        return rv;
    }

    #! Returns a response with its headers gathered under \c "hdr"
    /** Every caller here reads a header as \c resp.hdr.<name>, and a response hash that carries no \c hdr key
        answers all of them with @ref nothing and no error: an object's MIME type, size, entity tag,
        modification time, and version ID, the entity tag and version ID of every write, and the location of a
        newly created bucket were all silently empty when this was first written against a client whose
        response shape did not match.

        Rather than have every call site know where the headers live, the response is normalized once here,
        where every request already passes.

        @ref RestClientIo::RestClientIo "RestClientIo" returns a
        @ref RestClient::RestResponseInfo "RestResponseInfo" hash, which already separates the headers from
        the status fields, so this is the single point that has to know that.

        @param resp the response as the client returned it

        @return the response with \c body, \c status_code, and the headers under \c hdr
    */
    static hash<auto> normalizeResponse(hash<RestClient::RestResponseInfo> resp) {
        return {
            "body": resp.body,
            "status_code": resp.status_code,
            "hdr": resp.headers,
        };
    }

    #! Rethrows an exception from an S3 request with the S3 error code and message when the response carried them
    /** S3 reports failures as an XML document in the response body; without unpacking it the caller sees only
        an HTTP status code, which does not distinguish "no such bucket" from "access denied".

        @param ex the original exception
        @param method the HTTP method used
        @param bucket the bucket, if any
        @param key the object key, if any
    */
    static nothing rethrowS3Error(hash<ExceptionInfo> ex, string method, *string bucket, *string key) {
        *hash<auto> err;
        try {
            # NOTE: widened with "+ {}" first; a REST client raises its errors with a hashdecl argument in some
            # configurations, and reading a member a hashdecl does not declare throws INVALID-MEMBER, which the
            # catch below would then swallow along with the S3 error code this exists to report
            *hash<auto> arg = ex.arg.typeCode() == NT_HASH ? ex.arg + {} : NOTHING;
            # NOTE: three spellings, because the error argument's shape depends on which client raised it:
            # RestClientIo raises a RestResponseInfo, whose deserialized body is under "body" and whose raw
            # bytes are under "response_body_raw", while the HTTPClient-era shape uses "response-body".
            # parseXml() accepts a hash, a string, or bytes, so whichever is present is unpacked
            err = AwsS3DataProviderBase::parseXml(arg."response-body" ?? arg.body
                ?? arg.response_body_raw).Error;
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

#! Reads an S3 object with non-blocking I/O
/** Wraps the REST client's caller-driven poll operation so that a polled S3 request behaves like a blocking
    one in the two ways that matter to a caller:

    - **a transient I/O error is retried.**  @ref AwsS3DataProviderBase::doS3Request() retries up to
      @ref AwsS3DataProviderBase::MaxIoRetries times, and a poller that gave up on the first dropped
      connection would be less reliable than the blocking path it replaces rather than more
    - **a failure is reported as an S3 error.**  S3 describes what went wrong in an XML document in the
      response body; without unpacking it the caller sees only an HTTP status, which does not distinguish
      "no such bucket" from "access denied"

    The output is the object's content as a binary, which is what
    @ref FileLocationHandler::AbstractFileLocationHandler::getIoPollerForLocation() "getIoPollerForLocation()"
    consumers expect.
*/
public class AwsS3PollOperation inherits Qore::AbstractPollOperation<auto> {
    private {
        #! The client the request is made with, which also signs each attempt
        AwsRestClient::AwsRestClientIo rest;

        #! The current attempt
        Qore::AbstractPollOperation op;

        #! The HTTP method
        string method;

        #! The request path, already encoded and ready to sign
        string path;

        #! The bucket being addressed, if any; used to describe a failure
        *string bucket;

        #! The object key being addressed, if any; used to describe a failure
        *string key;

        #! I/O errors retried so far
        int retries = 0;
    }

    #! Creates the object and prepares the first attempt
    /** @param rest the client to make the request with
        @param method the HTTP method
        @param path the request path
        @param bucket the bucket being addressed, if any
        @param key the object key being addressed, if any
    */
    constructor(AwsRestClient::AwsRestClientIo rest, string method, string path, *string bucket, *string key) {
        self.rest = rest;
        self.method = method;
        self.path = path;
        self.bucket = bucket;
        self.key = key;
        op = rest.startPollRequest(method, path, NOTHING, NOTHING);
    }

    #! Returns a description of what the operation is trying to do
    string getGoal() {
        return sprintf("S3 %s %s", method, path);
    }

    #! Returns the state of the current attempt
    string getState() {
        return op.getState();
    }

    #! Returns @ref True when the object has been read
    bool goalReached() {
        return op.goalReached();
    }

    #! Advances the current attempt, starting a new one after a transient I/O error
    /** @return the socket poll descriptor to wait on, or @ref nothing when there is nothing to wait for

        @throw S3-API-ERROR the request failed; the S3 error code and message are included when the response
        carried them
    */
    *hash<SocketPollInfo> continuePoll() {
        while (True) {
            try {
                return op.continuePoll();
            } catch (hash<ExceptionInfo> ex) {
                if (AwsS3DataProviderBase::RetrySet{ex.err} && retries < AwsS3DataProviderBase::MaxIoRetries) {
                    ++retries;
                    # a poll operation cannot be rewound, so the retry is a new one; it is prepared and
                    # signed afresh, which it must be -- a SigV4 signature carries the time it was made and
                    # AWS rejects one that has aged out
                    op = rest.startPollRetry(method, path, NOTHING, NOTHING);
                    continue;
                }
                AwsS3DataProviderBase::rethrowS3Error(ex, method, bucket, key);
            }
        }
    }

    #! Returns the object's content
    /** @return the content as a binary; an object stored empty reads back as an empty binary rather than as
        @ref nothing, so that a caller can tell "empty" from "not read"
    */
    auto getOutput() {
        auto body = op.getOutput().body;
        if (!exists body) {
            return binary();
        }
        return body.typeCode() == NT_BINARY ? body : binary(body.toString());
    }

    #! Aborts the request
    abort() {
        op.abort();
    }
}
}
