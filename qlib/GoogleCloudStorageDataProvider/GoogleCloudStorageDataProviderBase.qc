# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GoogleCloudStorageDataProvider module definition

/** GoogleCloudStorageDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The GCP credential context every provider in this module needs
/** A GcpAuth credential mints a short-lived bearer token per request rather than holding a static
    one, so the credential itself has to travel with the client to every child provider; a
    @ref RestClientIo::RestClientIo "RestClientIo" alone is not enough to make a request.  Carrying
    it as one hashdecl keeps the child constructor signature stable as options are added.
*/
public hashdecl GcsAuthContext {
    #! The GCP project ID; needed to list buckets and as the default billing project
    string project_id;

    #! Location of a service-account JSON key, or @ref nothing for Application Default Credentials
    *string sa_json_path;

    #! The storage access level: \c "read_only", \c "read_write" or \c "full_control"
    string access = GoogleCloudStorageRestClient::DefaultStorageAccess;

    #! Optional billing/quota project sent as the \c x-goog-user-project header
    *string quota_project_id;

    #! Optional service-account email used to sign signed URLs
    /** When unset it is read from the \c client_email field of the service-account key
    */
    *string signer_email;

    #! An externally-minted bearer token to use instead of obtaining one from the credential
    /** For callers that mint tokens themselves — workload identity federation, service-account
        impersonation, or a platform that already holds a token for the caller.  It is used verbatim
        and is **not** refreshed, so it stops working when it expires; leave it unset to let
        @ref gcpauthintro "GcpAuth" obtain and refresh tokens from the credential instead.

        The token must carry the scopes for everything it is used for, which includes
        \c cloud-platform if signed URLs are generated.
    */
    *string access_token;
}

#! The Google Cloud Storage data provider common base class
/** Uses RestClientIo for async-capable multiplexed HTTP connections.
*/
public class GoogleCloudStorageDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Constructor options
        const ConstructorOptions = {
            "gcp_project_id": <DataProviderOptionInfo>{
                "display_name": "GCP Project ID",
                "short_desc": "GCP project that owns the buckets",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "GCP project ID; required to list buckets and used as the default billing "
                    "project for requester-pays buckets.",
                "required": True,
                "example_value": "my-gcp-project-123",
            },
            "gcp_sa_json_path": <DataProviderOptionInfo>{
                "display_name": "Service-Account JSON Location",
                "short_desc": "Location of a service-account JSON key file",
                "type": AbstractDataProviderType::get(StringOrNothingType, NOTHING, {
                    DTT_FromLocation: True,
                }),
                "desc": "Location of a GCP service-account JSON key file for JWT-bearer auth, read "
                    "via the @ref filelocationhandlerintro \"FileLocationHandler\" module (a bare "
                    "path or any registered scheme: `file://`, `http(s)://`, `sftp://`, ...).  When "
                    "omitted, Application Default Credentials are used.",
            },
            "gcp_storage_access": <DataProviderOptionInfo>{
                "display_name": "Storage Access Level",
                "short_desc": "The OAuth2 scope requested for storage access",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The access level to request: `read_only`, `read_write` (the default) or "
                    "`full_control`.  Only the scope this level names is requested, so a read-only "
                    "connection cannot write even if the service account is otherwise permitted to.",
                "default_value": GoogleCloudStorageRestClient::DefaultStorageAccess,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "read_only",
                        "display_name": "Read Only",
                        "short_desc": "Read buckets and objects; no writes",
                        "desc": "Read buckets and objects; no writes (`devstorage.read_only`)",
                    },
                    <AllowedValueInfo>{
                        "value": "read_write",
                        "display_name": "Read/Write",
                        "short_desc": "Read and write buckets and objects",
                        "desc": "Read and write buckets and objects (`devstorage.read_write`)",
                    },
                    <AllowedValueInfo>{
                        "value": "full_control",
                        "display_name": "Full Control",
                        "short_desc": "Read and write buckets and objects and change their ACLs",
                        "desc": "Read and write buckets and objects and change their ACLs "
                            "(`devstorage.full_control`)",
                    },
                ),
            },
            "gcp_quota_project_id": <DataProviderOptionInfo>{
                "display_name": "GCP Quota Project ID",
                "short_desc": "Billing/quota project for the x-goog-user-project header",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Optional GCP billing/quota project; when set it is sent as the "
                    "`x-goog-user-project` header so quota and billing are attributed to that "
                    "project.  Required to read a requester-pays bucket owned by another project.",
            },
            "gcp_signer_email": <DataProviderOptionInfo>{
                "display_name": "Signer Service-Account Email",
                "short_desc": "Service-account email used to sign signed URLs",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The service account whose key signs generated signed URLs.  When omitted "
                    "it is read from the `client_email` field of the service-account key; it must "
                    "be given explicitly when Application Default Credentials are used, since "
                    "there is then no key file to read it from.",
            },
            "gcp_access_token": <DataProviderOptionInfo>{
                "display_name": "Bearer Access Token",
                "short_desc": "An externally-minted bearer token to use directly",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "A GCP bearer access token to use verbatim instead of obtaining one from a "
                    "credential, for callers that mint tokens themselves (workload identity "
                    "federation, service-account impersonation, or a platform that already holds a "
                    "token).  It is **not** refreshed, so it stops working when it expires; leave it "
                    "unset to let GcpAuth obtain and refresh tokens.  It must carry the scopes for "
                    "everything it is used for, including `cloud-platform` if signed URLs are "
                    "generated.",
                "sensitive": True,
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(
                    new Type("GoogleCloudStorageRestClient::GoogleCloudStorageRestClientIo"), NOTHING, {
                        DTT_ClientOnly: True,
                    }),
                "desc": "The Google Cloud Storage REST client object to use",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "The options to use when creating a REST client",
                "type": AbstractDataProviderType::get(AutoHashType, NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "Options to the GoogleCloudStorageRestClientIo constructor; only used if a "
                    "client object is created for a call",
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
        *GoogleCloudStorageRestClient::GoogleCloudStorageRestClientIo rest;

        #! The GCP credential context
        hash<GcsAuthContext> auth;
    }

    #! Creates the object from a REST client and a credential context
    constructor(GoogleCloudStorageRestClient::GoogleCloudStorageRestClientIo rest,
            hash<GcsAuthContext> auth) {
        self.rest = rest;
        self.auth = auth;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (!copts.gcp_project_id.val()) {
            throw "CONSTRUCTOR-ERROR", "missing required 'gcp_project_id' option for Google Cloud Storage";
        }
        auth = <GcsAuthContext>{
            "project_id": copts.gcp_project_id,
            "sa_json_path": copts.gcp_sa_json_path,
            "access": copts.gcp_storage_access ?? GoogleCloudStorageRestClient::DefaultStorageAccess,
            "quota_project_id": copts.gcp_quota_project_id,
            "signer_email": copts.gcp_signer_email,
            "access_token": copts.gcp_access_token,
        };
        # validate the access level now rather than at the first request
        GoogleCloudStorageRestClient::gcsScopes(auth.access);

        if (copts.restclient) {
            rest = copts.restclient;
        } else {
            rest = new GoogleCloudStorageRestClient::GoogleCloudStorageRestClientIo(
                copts.restclient_options);
        }
    }

    #! Returns the REST client object
    *RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    #! Returns the GCP credential context
    /** @note public rather than private because the main provider reads it to construct its children
    */
    hash<GcsAuthContext> getAuthContext() {
        return auth;
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        LoggerWrapper::setLogger(logger);
    }

    #! Returns the reference data types supported by this provider
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "buckets": True,
        };
    }

    #! Returns reference data for the given type
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "buckets": return getReferenceBuckets(action_opts);
        }
    }

    #! Returns reference data for the buckets visible to the credential in the configured project
    private *list<hash<AllowedValueInfo>> getReferenceBuckets(*hash<auto> action_opts) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> resp = doGcsRequest("GET", GoogleCloudStorageRestClient::GcsApiPrefix + "/b", {
                "project": auth.project_id,
                "maxResults": MaxPageSize,
                "projection": "noAcl",
            });
            foreach hash<auto> bucket in (resp.body.items ?? ()) {
                string name = bucket.name;
                string label = bucket.location.val()
                    ? sprintf("%s (%s)", name, bucket.location)
                    : name;
                rv += <AllowedValueInfo>{
                    "display_name": name,
                    "short_desc": label,
                    "desc": label,
                    "value": name,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceBuckets: error: %s: %s", ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns a REST client for the framework's generic \c "make-api-call" action
    /** The generic-call provider dispatches straight through the client it is handed, with no
        opportunity to add a header per request — but a GcpAuth credential has no static token to hold
        on the client, which is why every request in this module attaches a freshly minted one.  A
        client discovered reflectively would therefore make an unauthenticated call and get HTTP 401
        for every endpoint.

        So a client carrying a current bearer token as a default header is returned instead.  The
        token is valid for about an hour from this call, which suits an escape hatch used
        interactively; the connection manager is shared with this provider's own client, so pooled
        connections are reused rather than duplicated.

        @return a Google Cloud Storage client that authenticates
    */
    private *object getRestClientForGenericCallImpl() {
        if (!rest) {
            return;
        }
        return new GoogleCloudStorageRestClient::GoogleCloudStorageRestClientIo({
            "url": rest.getUrl(),
            "headers": getGcsHeaders(),
        }, rest.getConnectionManager());
    }

    #! Returns a bearer access token for the configured credential and storage access level
    /** An externally-minted token, when configured, is used verbatim and no credential exchange
        happens; see @ref GcsAuthContext::access_token "GcsAuthContext::access_token".
    */
    private string getStorageToken() {
        if (auth.access_token.val()) {
            return auth.access_token;
        }
        return GcpAuth::getAccessToken(auth.sa_json_path,
            GoogleCloudStorageRestClient::gcsScopes(auth.access));
    }

    #! Returns the request headers for a Google Cloud Storage call: a fresh bearer token plus the quota project
    private hash<auto> getGcsHeaders(*hash<auto> hdr) {
        return GoogleCloudStorageRestClient::gcsAuthHeaders(getStorageToken(), auth.quota_project_id)
            + (hdr ?? {});
    }

    #! Makes a Google Cloud Storage JSON API call and returns the response
    /** @param method the HTTP method
        @param path the URI path, with any object name already encoded
        @param query optional query parameters, added to \a path url-encoded
        @param body the request body, serialized by the client's JSON codec
        @param hdr any additional headers

        @return the REST response

        @throw GCS-API-ERROR the request failed; the Google error message and reason are included
        when the response carried them
    */
    private hash<auto> doGcsRequest(string method, string path, *hash<auto> query, auto body, *hash<auto> hdr) {
        string full_path = GoogleCloudStorageDataProviderBase::addQuery(path, query);
        int retries = 0;
        while (True) {
            try {
                debug("GCS %s %y body: %y", method, full_path, body);
                return rest.restDoRequest(method, full_path, body, getGcsHeaders(hdr));
            } catch (hash<ExceptionInfo> ex) {
                if (retry(ex, \retries)) {
                    continue;
                }
                rethrow ex.err, getApiErrorDesc(ex, full_path), ex.arg;
            }
        }
    }

    #! Makes a Google Cloud Storage call with a body that must reach the service byte for byte
    /** The upload endpoints take the object's bytes as the request body, but the connection
        serializes bodies as JSON, and @ref doGcsRequest() hands the body to that codec: a
        @ref binary body is then base64-encoded into a JSON string, so 256 bytes of content are
        stored as 346 bytes of quoted base64.  Google Cloud Storage accepts that and reports
        success, so the corruption is only visible in a byte comparison of the downloaded object.

        This method uses the client's raw request path instead, where the caller owns both the body
        and the \c Content-Type and no codec runs.  Note that it does **not** work to call
        \c setSerialization("bin") around the request: that mutates client-wide state, and the client
        is shared between provider objects and threads, so a concurrent JSON call would be silently
        serialized as binary.

        @param method the HTTP method
        @param path the URI path, with any object name already encoded
        @param query optional query parameters, added to \a path url-encoded
        @param body the request body, sent verbatim
        @param content_type the content type of \a body
        @param hdr any additional headers

        @return the REST response
    */
    private hash<auto> doGcsRawRequest(string method, string path, *hash<auto> query, data body,
            string content_type, *hash<auto> hdr) {
        string full_path = GoogleCloudStorageDataProviderBase::addQuery(path, query);
        hdr = (hdr ?? {}) + {"Content-Type": content_type};
        int retries = 0;
        while (True) {
            try {
                debug("GCS (raw) %s %y %d byte(s) %y", method, full_path, body.size(), content_type);
                return rest.restDoRawRequest(method, full_path, body, getGcsHeaders(hdr));
            } catch (hash<ExceptionInfo> ex) {
                if (retry(ex, \retries)) {
                    continue;
                }
                rethrow ex.err, getApiErrorDesc(ex, full_path), ex.arg;
            }
        }
    }

    #! Appends url-encoded query parameters to a path
    /** Values are encoded strictly, so an object name passed as the \c name parameter of an upload
        survives verbatim even when it contains \c "/", \c "+", \c "&" or a space.

        @param path the URI path
        @param query the query parameters; @ref nothing and empty hashes return \a path unchanged

        @return the path with the query string appended
    */
    static string addQuery(string path, *hash<auto> query) {
        if (!query) {
            return path;
        }
        list<string> parts = ();
        foreach hash<auto> i in (query.pairIterator()) {
            if (!exists i.value) {
                continue;
            }
            push parts, sprintf("%s=%s", encode_url(i.key, True),
                encode_url(i.value.toString(), True));
        }
        if (!parts) {
            return path;
        }
        return path + (path =~ /\?/ ? "&" : "?") + join("&", parts);
    }

    #! Returns an enriched error description including the Google Cloud Storage API error message if available
    /** The JSON API returns errors in the Google API envelope:
        \c {"error": {"code": 404, "message": "...", "errors": [{"domain": "global",
        "reason": "notFound", "message": "..."}], "status": "NOT_FOUND"}}.  The \c message is the
        developer-facing prose, \c reason the machine-readable identifier (e.g. \c notFound,
        \c forbidden, \c invalid), and \c status the canonical error code.
    */
    private string getApiErrorDesc(hash<ExceptionInfo> ex, string path) {
        string desc = ex.desc;
        auto body = ex.arg.body;
        # a media download error arrives as raw bytes rather than a parsed hash, because the client is
        # constructed with allow_any_response so that object content of any type survives
        if (body.typeCode() == NT_STRING || body.typeCode() == NT_BINARY) {
            try {
                body = parse_json(body.toString("UTF-8"));
            } catch (hash<ExceptionInfo> ex2) {
                # not JSON; fall through and report the bare status
            }
        }
        if (body.typeCode() == NT_HASH && body.error.typeCode() == NT_HASH) {
            auto err = body.error;
            *string message = DataProvider::DataProviderErrorUtil::firstStringAt(err, ("message",));
            *string status = DataProvider::DataProviderErrorUtil::firstStringAt(err, ("status",));
            *string reason;
            if (err.errors.typeCode() == NT_LIST && err.errors) {
                reason = DataProvider::DataProviderErrorUtil::firstStringAt(err.errors[0], ("reason",));
            }

            string primary;
            if (message.val() && reason.val()) {
                primary = sprintf("%s (reason: %s)", message, reason);
            } else if (message.val()) {
                primary = message;
            } else if (reason.val()) {
                primary = reason;
            }
            if (status.val() && status != reason) {
                primary = primary.val()
                    ? primary + sprintf(" [status: %s]", status)
                    : sprintf("[status: %s]", status);
            }
            if (primary.val()) {
                desc += ": " + primary;
            }
        }
        return sprintf("%s (request path: %y)", desc, ex.arg.request_path ?? path);
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
        # 429 is the documented rate limit; Google Cloud Storage also asks callers to retry 5xx with
        # exponential backoff, and those are the only statuses where a retry can succeed
        if (status_code != 429 && status_code < 500) {
            return False;
        }
        if (++retries == MaxIoRetries) {
            LoggerWrapper::error(sprintf("Maximum retries exceeded (%d)", retries));
            return False;
        }
        *string retry_after = ex.arg.headers."retry-after";
        int wait_secs = retry_after.val() ? retry_after.toInt() : (1 << retries);
        warn("HTTP %d from Google Cloud Storage; waiting %d second(s) before retry %d", status_code,
            wait_secs, retries);
        sleep(wait_secs);
        return True;
    }
}
}
