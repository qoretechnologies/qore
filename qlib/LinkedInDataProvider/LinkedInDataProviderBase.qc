# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinkedInDataProvider module definition

/** LinkedInDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the LinkedInDataProvider module
public namespace LinkedInDataProvider {
#! The LinkedIn data provider common base class
/** Uses RestClientIo for async-capable multiplexed HTTP connections.
*/
public class LinkedInDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Constructor options
        const ConstructorOptions = {
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(
                    new Type("LinkedInRestClient::LinkedInRestClientIo"), NOTHING, {
                        DTT_ClientOnly: True,
                    }),
                "desc": "The LinkedIn REST client object to use",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "The options to use when creating a REST client",
                "type": AbstractDataProviderType::get(AutoHashType, NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "Options to the LinkedInRestClientIo constructor; only used if a client "
                    "object is created for a call",
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
        *LinkedInRestClient::LinkedInRestClientIo rest;

        #! The authenticated member's person URN, resolved lazily from the userinfo endpoint
        *string member_urn;
    }

    #! Creates the object from a REST client
    constructor(LinkedInRestClient::LinkedInRestClientIo rest) {
        self.rest = rest;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
        } else {
            rest = new LinkedInRestClient::LinkedInRestClientIo(copts.restclient_options);
        }
    }

    #! Returns the REST client object
    *RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        LoggerWrapper::setLogger(logger);
    }

    #! Returns a REST client for the framework's generic \c "make-api-call" action
    /** The generic-call provider dispatches straight through the client it is handed, with no
        opportunity to add a header per request.  Every LinkedIn call under \c "/rest" needs a
        \c LinkedIn-Version header — LinkedIn never defaults to the latest version, and a missing
        header is an error, not a fallback — so a client discovered reflectively would fail on every
        versioned endpoint.

        A client carrying this connection's API version and the Rest.li protocol version as default
        headers is returned instead.  The unversioned \c "/v2" endpoints ignore both headers, so one
        client serves the whole surface.  The connection manager is shared with this provider's own
        client, so pooled connections are reused rather than duplicated.

        @return a LinkedIn client that sends the protocol headers, or @ref nothing when there is no
        client to derive one from
    */
    private *object getRestClientForGenericCallImpl() {
        if (!rest) {
            return;
        }
        *string tok = rest.getToken();
        if (!tok.val()) {
            # nothing to carry over; the caller gets this provider's own client
            return rest;
        }
        return new RestClientIo::RestClientIo({
            "url": rest.getUrl(),
            "token": tok,
            "token_type": rest.getTokenType(),
            "data": "json",
            "allow_any_response": True,
            "headers": {
                LinkedInRestClient::LinkedInVersionHeader: rest.getLinkedInVersion(),
                LinkedInRestClient::RestliProtocolVersionHeader:
                    LinkedInRestClient::RestliProtocolVersion,
            },
        }, rest.getConnectionManager());
    }

    #! Returns the authenticated member's person URN
    /** Every post names its author, and for a member connection the author is always the
        authenticated member, whose ID is only available from the OpenID Connect userinfo endpoint.
        It is resolved once per provider object and then reused, because it cannot change for a
        given access token.

        @return the member's URN in \c "urn:li:person:{id}" form

        @throw LINKEDIN-USER-ERROR the userinfo endpoint did not return a subject identifier
    */
    private string getMemberUrn() {
        if (member_urn.val()) {
            return member_urn;
        }
        hash<auto> resp = doLinkedInRequest("GET", UserInfoPath);
        # NOTE: not named "sub"; that is a Qore keyword and declaring a variable with that name is a
        # parse error
        *string subject = resp.body.sub;
        if (!subject.val()) {
            throw "LINKEDIN-USER-ERROR", sprintf("the LinkedIn userinfo endpoint %y returned no "
                "\"sub\" field, so the authenticated member's URN cannot be determined; the "
                "connection needs the \"openid\" and \"profile\" scopes", UserInfoPath);
        }
        return member_urn = "urn:li:person:" + subject;
    }

    #! Makes a LinkedIn API call and returns the response
    /** @param method the HTTP method
        @param path the URI path, with any URN already encoded
        @param body the request body, serialized by the client's JSON codec
        @param hdr any additional headers, which win over the LinkedIn protocol headers

        @return the REST response

        @throw LINKEDIN-API-ERROR the request failed; the LinkedIn error message is included when
        the response carried one
    */
    private hash<auto> doLinkedInRequest(string method, string path, auto body, *hash<auto> hdr) {
        int retries = 0;
        while (True) {
            try {
                debug("LinkedIn %s %y body: %y", method, path, body);
                return rest.restDoRequest(method, path, body, rest.getApiHeaders(path, hdr));
            } catch (hash<ExceptionInfo> ex) {
                if (retry(ex, \retries)) {
                    continue;
                }
                rethrow ex.err, getApiErrorDesc(ex, path), ex.arg;
            }
        }
    }

    #! Returns the URN of the entity a versioned write created
    /** A LinkedIn versioned write answers \c 201 with an **empty body**: the created entity's URN
        is only in the \c x-restli-id response header.  Reading it from the body — as an
        implementation modelled on the older \c ugcPosts endpoint would — yields nothing at all, and
        silently, because the call itself succeeded.

        @param resp the REST response
        @param what what was being created, for the error message

        @return the created entity's URN

        @throw LINKEDIN-RESPONSE-ERROR the response carried no identifier
    */
    private string getCreatedUrn(hash<auto> resp, string what) {
        # response header keys are lower-cased by the HTTP client; the body is checked as well
        # because a few endpoints do return the entity
        foreach auto candidate in ((resp.headers{RestliIdHeader}, resp.body.id)) {
            if (candidate.typeCode() == NT_STRING && candidate.val()) {
                return candidate;
            }
        }
        throw "LINKEDIN-RESPONSE-ERROR", sprintf("LinkedIn returned HTTP %d for the %s but no %y "
            "response header and no \"id\" in the body, so the created entity's URN is unknown",
            resp.status_code ?? 0, what, RestliIdHeader);
    }

    #! Splits an absolute URL into the origin to bind a client to and the path to request
    /** LinkedIn hands back media upload URLs on a **different host** from the API
        (\c www.linkedin.com rather than \c api.linkedin.com), so the upload cannot go through this
        provider's client: a @ref RestClientIo::RestClientIo "RestClientIo" request takes a path and
        resolves it against the client's own URL, which would send the bytes to the wrong host.

        @param url the absolute URL

        @return a hash of \c origin (\c "scheme://host[:port]") and \c path (the path and query)

        @throw LINKEDIN-UPLOAD-ERROR the URL is not absolute
    */
    static hash<auto> splitAbsoluteUrl(string url) {
        hash<auto> u = parse_url(url);
        if (!u.protocol.val() || !u.host.val()) {
            throw "LINKEDIN-UPLOAD-ERROR", sprintf("LinkedIn returned upload URL %y, which is not an "
                "absolute URL with a scheme and a host", url);
        }
        string origin = sprintf("%s://%s", u.protocol, u.host);
        if (u.port) {
            origin += sprintf(":%d", u.port);
        }
        # parse_url() returns the query string as part of "path"
        string path = u.path ?? "/";
        if (path[0] != "/") {
            path = "/" + path;
        }
        return {
            "origin": origin,
            "path": path,
        };
    }

    #! Returns a client bound to the given origin for uploading media bytes
    /** The connection manager is shared with this provider's own client, so the upload host's
        connections are pooled alongside the API host's rather than opened and discarded.

        @param origin the \c "scheme://host[:port]" to bind the client to

        @return a client for the upload host
    */
    private RestClientIo::RestClientIo getUploadClient(string origin) {
        return new RestClientIo::RestClientIo({
            "url": origin,
            "data": "json",
            # an upload response has an empty body and frequently no content type at all
            "allow_any_response": True,
        }, rest.getConnectionManager());
    }

    #! Uploads bytes to a LinkedIn media upload URL and returns the response
    /** @param client the client bound to the upload origin, from @ref getUploadClient()
        @param path the upload path and query, from @ref splitAbsoluteUrl()
        @param content the bytes to upload
        @param content_type the content type to declare

        @return the REST response

        @note the raw request path is used, so the bytes reach LinkedIn unchanged.  Sending them
        through the ordinary request path would hand them to the connection's JSON codec, which
        base64-encodes a @ref binary into a JSON string — LinkedIn would then store a corrupted
        asset and report success, and only a byte comparison would show it.  Calling
        \c setSerialization("bin") around the request is **not** an alternative: it mutates
        client-wide state shared between provider objects and threads.
    */
    private hash<auto> uploadBytes(RestClientIo::RestClientIo client, string path, binary content,
            string content_type) {
        debug("LinkedIn upload PUT %y %d byte(s) %y", path, content.size(), content_type);
        return client.restDoRawRequest("PUT", path, content, {
            "Content-Type": content_type,
        });
    }

    #! Uploads an image and returns the image asset
    /** LinkedIn's Images API is a two-step upload: \c initializeUpload reserves an image URN and
        returns a one-time upload URL, and the bytes are then PUT to that URL.

        @param content the image bytes
        @param content_type the image's MIME type
        @param owner the URN of the entity that will own the image; the authenticated member when
        not given

        @return the image asset hash matching @ref LinkedInMediaAssetDataType

        @throw LINKEDIN-UPLOAD-ERROR LinkedIn did not return an image URN and an upload URL

        @note the asset is **not** read back afterwards: a token holding only \c w_member_social is
        write-only on \c "/rest/images", so a \c GET would fail with HTTP 403 on exactly the
        connections this action is meant for
    */
    private hash<auto> uploadImageAsset(binary content, string content_type, *string owner) {
        string owner_urn = owner.val() ? owner : getMemberUrn();
        hash<auto> init = doLinkedInRequest("POST", ImagesPath + "?action=initializeUpload", {
            "initializeUploadRequest": {
                "owner": owner_urn,
            },
        });
        *string image_urn = init.body.value.image;
        *string upload_url = init.body.value.uploadUrl;
        if (!image_urn.val() || !upload_url.val()) {
            throw "LINKEDIN-UPLOAD-ERROR", sprintf("the LinkedIn Images API accepted the upload "
                "initialization for owner %y but returned image URN %y and upload URL %y", owner_urn,
                image_urn, upload_url);
        }
        hash<auto> target = LinkedInDataProviderBase::splitAbsoluteUrl(upload_url);
        uploadBytes(getUploadClient(target.origin), target.path, content, content_type);
        return {
            "id": image_urn,
            "owner": owner_urn,
        };
    }

    #! Uploads a video and returns the video asset
    /** LinkedIn's Videos API is a three-step multi-part upload: \c initializeUpload reserves a video
        URN and returns one upload instruction per part, each part is PUT to its own URL, and
        \c finalizeUpload links the parts together using the \c ETag each part upload returned.

        @param content the video bytes
        @param content_type the video's MIME type
        @param owner the URN of the entity that will own the video; the authenticated member when
        not given

        @return the video asset hash matching @ref LinkedInMediaAssetDataType

        @throw LINKEDIN-UPLOAD-ERROR LinkedIn did not return a video URN and upload instructions, or
        a part upload returned no \c ETag
    */
    private hash<auto> uploadVideoAsset(binary content, string content_type, *string owner) {
        string owner_urn = owner.val() ? owner : getMemberUrn();
        hash<auto> init = doLinkedInRequest("POST", VideosPath + "?action=initializeUpload", {
            "initializeUploadRequest": {
                "owner": owner_urn,
                "fileSizeBytes": content.size(),
                "uploadCaptions": False,
                "uploadThumbnail": False,
            },
        });
        auto value = init.body.value;
        *string video_urn = value.video;
        *list<auto> instructions = value.uploadInstructions;
        if (!video_urn.val() || !instructions) {
            throw "LINKEDIN-UPLOAD-ERROR", sprintf("the LinkedIn Videos API accepted the upload "
                "initialization for owner %y (%d byte(s)) but returned video URN %y and %d upload "
                "instruction(s)", owner_urn, content.size(), video_urn, instructions.size());
        }

        # LinkedIn returns every part on the same host in practice, but the instructions are
        # authoritative, so a client is bound per distinct origin rather than assumed
        hash<string, object> clients = {};
        list<string> part_ids = ();
        int total = content.size();
        foreach hash<auto> instruction in (instructions) {
            int first = instruction.firstByte.toInt();
            # the final instruction's range runs to the end of its 4 MiB part, which is past the end
            # of the content unless the size is an exact multiple
            int last = min(instruction.lastByte.toInt(), total - 1);
            if (first > last) {
                # an instruction entirely past the end of the content has nothing to send
                continue;
            }
            hash<auto> target = LinkedInDataProviderBase::splitAbsoluteUrl(instruction.uploadUrl);
            if (!clients{target.origin}) {
                clients{target.origin} = getUploadClient(target.origin);
            }
            hash<auto> resp = uploadBytes(cast<RestClientIo::RestClientIo>(clients{target.origin}),
                target.path, content[first..last], content_type);
            push part_ids, LinkedInDataProviderBase::getPartId(resp, video_urn, first, last);
        }

        doLinkedInRequest("POST", VideosPath + "?action=finalizeUpload", {
            "finalizeUploadRequest": {
                "video": video_urn,
                # LinkedIn returns an empty upload token for a single-part upload and requires the
                # field to be present regardless
                "uploadToken": value.uploadToken ?? "",
                "uploadedPartIds": part_ids,
            },
        });
        return {
            "id": video_urn,
            "owner": owner_urn,
        };
    }

    #! Returns the part ID of an uploaded video part from its upload response
    /** LinkedIn identifies a part by the \c ETag its upload returned, and \c finalizeUpload takes
        those IDs **unquoted** and in part order; an \c ETag is often sent quoted, so the quotes are
        stripped.

        @param resp the part upload response
        @param video_urn the video URN, for the error message
        @param first the first byte of the part, for the error message
        @param last the last byte of the part, for the error message

        @return the part ID

        @throw LINKEDIN-UPLOAD-ERROR the part upload returned no \c ETag
    */
    static string getPartId(hash<auto> resp, string video_urn, int first, int last) {
        # response header keys are lower-cased by the HTTP client
        *string etag = resp.headers.etag;
        if (!etag.val()) {
            throw "LINKEDIN-UPLOAD-ERROR", sprintf("the upload of bytes %d-%d of video %y returned "
                "HTTP %d with no \"ETag\" response header, so the part cannot be named in the "
                "finalize request", first, last, video_urn, resp.status_code ?? 0);
        }
        string rv = etag;
        rv =~ s/^"//;
        rv =~ s/"$//;
        return rv;
    }

    #! Returns the binary content of a @ref DataProvider::FileDataType "FileDataType" value
    /** The content is declared as base64 but may already have been decoded upstream, so both forms
        are accepted.
    */
    static binary getFileContent(hash<auto> file) {
        if (file.content.typeCode() == NT_BINARY) {
            return file.content;
        }
        if (!exists file.content) {
            return binary();
        }
        return parse_base64_string(file.content.toString());
    }

    #! Returns the content type to declare for a @ref DataProvider::FileDataType "FileDataType" value
    /** \c "auto" is the @ref DataProvider::FileDataType "FileDataType" placeholder meaning "work it
        out from the name", so it is never sent as a MIME type.

        @param file the file value
        @param fallback the content type to use when neither the value nor the name yields one
    */
    static string getFileContentType(hash<auto> file, string fallback) {
        if (file.mime_type.val() && file.mime_type != "auto") {
            return file.mime_type;
        }
        if (file.name.val()) {
            *string from_name = get_mime_type_from_ext(file.name);
            # get_mime_type_from_ext() answers with the generic octet-stream when the name tells it
            # nothing, and that is *less* informative than the caller's own default -- an image
            # uploaded as "application/octet-stream" is declared wrongly, where the caller's
            # fallback names the type the action actually deals in
            if (from_name.val() && from_name != MimeTypeOctetStream) {
                return from_name;
            }
        }
        return fallback;
    }

    #! Returns an enriched error description including the LinkedIn API error message if available
    /** LinkedIn reports errors as
        \c {"message": "...", "status": 403, "code": "ACCESS_DENIED", "serviceErrorCode": 100}; the
        \c message is the developer-facing prose and \c code the machine-readable identifier.
    */
    private string getApiErrorDesc(hash<ExceptionInfo> ex, string path) {
        auto body = ex.arg.body;
        # an error from an upload host arrives as raw bytes rather than a parsed hash, because those
        # clients are constructed with allow_any_response so that an empty body survives
        if (body.typeCode() == NT_STRING || body.typeCode() == NT_BINARY) {
            try {
                body = parse_json(body.toString("UTF-8"));
            } catch (hash<ExceptionInfo> ex2) {
                # not JSON; fall through and report the bare status
            }
        }
        return sprintf("%s (request path: %y)",
            LinkedInRestClient::LinkedInRestClientBase::getErrorDesc(ex.desc, body),
            ex.arg.request_path ?? path);
    }

    #! Returns True if the error indicates that the operation should be retried
    private bool retry(hash<ExceptionInfo> ex, reference<int> retries) {
        # retry on I/O errors
        if (RetrySet{ex.err}) {
            if (++retries == MaxIoRetries) {
                LoggerWrapper::error(sprintf("Maximum retries exceeded (%d)", retries));
                return False;
            }
            warn("Technical error making REST call: %s: %s; retrying", ex.err, ex.desc);
            return True;
        }
        if (ex.err != "REST-RESPONSE-ERROR") {
            return False;
        }
        int status_code = ex.arg.status_code ?? 0;
        # 429 is LinkedIn's documented throttle response; 5xx are the transient server errors it
        # asks callers to retry.  Nothing else can succeed on a retry
        if (status_code != 429 && status_code < 500) {
            return False;
        }
        if (++retries == MaxIoRetries) {
            LoggerWrapper::error(sprintf("Maximum retries exceeded (%d)", retries));
            return False;
        }
        *string retry_after = ex.arg.headers."retry-after";
        int wait_secs = retry_after.val() ? retry_after.toInt() : (1 << retries);
        warn("HTTP %d from LinkedIn; waiting %d second(s) before retry %d", status_code, wait_secs,
            retries);
        sleep(wait_secs);
        return True;
    }
}
}
