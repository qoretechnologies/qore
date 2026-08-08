# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GoogleCloudStorageDataProvider module definition

/** GoogleCloudStorageSignedUrlDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Request type for creating a signed URL
public class GoogleCloudStorageCreateSignedUrlRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "bucket": {
                "display_name": "Bucket",
                "short_desc": "The bucket holding the object",
                "desc": "The name of the bucket the signed URL should address",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "my-bucket",
                "ref_data": "buckets",
            },
            "object": {
                "display_name": "Object Name",
                "short_desc": "The object's name",
                "desc": "The name of the object the signed URL should address, used verbatim",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "reports/2026/q1.csv",
            },
            "method": {
                "display_name": "HTTP Method",
                "short_desc": "The HTTP method to sign for",
                "desc": "The HTTP method the URL should be valid for.  A URL signed for `GET` grants "
                    "read access only; sign for `PUT` to grant an upload",
                "type": AbstractDataProviderTypeMap."*string",
                "default_value": "GET",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "GET",
                        "display_name": "GET (download)",
                        "short_desc": "Grants reading the object",
                        "desc": "Grants reading the object's content",
                    },
                    <AllowedValueInfo>{
                        "value": "PUT",
                        "display_name": "PUT (upload)",
                        "short_desc": "Grants writing the object",
                        "desc": "Grants writing the object's content",
                    },
                    <AllowedValueInfo>{
                        "value": "HEAD",
                        "display_name": "HEAD (metadata)",
                        "short_desc": "Grants reading the object's metadata",
                        "desc": "Grants reading the object's response headers without its content",
                    },
                    <AllowedValueInfo>{
                        "value": "DELETE",
                        "display_name": "DELETE",
                        "short_desc": "Grants deleting the object",
                        "desc": "Grants deleting the object",
                    },
                ),
            },
            "expires_in_seconds": {
                "display_name": "Expires In (Seconds)",
                "short_desc": "How long the URL stays valid",
                "desc": "The lifetime of the signed URL in seconds; Google Cloud Storage allows at "
                    "most 604800 (7 days) for a V4 signature",
                "type": AbstractDataProviderTypeMap."*int",
                "default_value": DefaultSignedUrlExpirySeconds,
                "example_value": 3600,
            },
            "signer_email": {
                "display_name": "Signer Email",
                "short_desc": "The service account to sign with",
                "desc": "The service account whose key should sign the URL.  When omitted the "
                    "connection's `gcp_signer_email` is used, and failing that the `client_email` "
                    "from the service-account key",
                "type": AbstractDataProviderTypeMap."*email",
                "example_value": "storage-signer@my-project.iam.gserviceaccount.com",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleCloudStorageCreateSignedUrlRequestDataType =
    new GoogleCloudStorageCreateSignedUrlRequestDataType();

#! Data provider for creating a V4 signed URL for an object
/** A signed URL grants time-limited access to one object for one HTTP method, with no credentials
    needed by whoever holds it — which is how content is handed to a browser or to a third party that
    has no Google account.

    @par Signing without a local private key

    A V4 signature is RSA-SHA256 over a canonical request string.  %Qore exposes no general-purpose
    RSA signing primitive, so the signing is delegated to the IAM Credentials
    <a href="https://cloud.google.com/iam/docs/reference/credentials/rest/v1/projects.serviceAccounts/signBlob">signBlob</a>
    method, which signs an arbitrary payload with the service account's own key.

    That is not merely a workaround: it is the only approach that works when the credential is
    Application Default Credentials or a workload identity, where there is no private key on the host
    at all.  It requires the caller to hold \c iam.serviceAccounts.signBlob on the signing service
    account (the \c roles/iam.serviceAccountTokenCreator role); a service account can be granted that
    on itself.
*/
public class GoogleCloudStorageCreateSignedUrlDataProvider inherits GoogleCloudStorageDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-signed-url",
            "type": "GoogleCloudStorageCreateSignedUrlDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = GoogleCloudStorageCreateSignedUrlRequestDataType;
        const ResponseType = GoogleCloudStorageSignedUrlDataType;

        #! The credential scope suffix for a Google Cloud Storage V4 signature
        /** The region is always the literal \c "auto" for Google Cloud Storage, unlike the AWS
            SigV4 scheme this format is derived from.
        */
        const CredentialScopeSuffix = "auto/storage/goog4_request";

        #! The only header a V4 signed URL signs
        const SignedHeaders = "host";

        #! The payload placeholder used for a signed URL rather than a signed request body
        const UnsignedPayload = "UNSIGNED-PAYLOAD";
    }

    private {
        #! The IAM Credentials client, created on first use
        *RestClientIo::RestClientIo iam;

        #! Serializes creation of the IAM Credentials client
        Mutex iam_mutex();
    }

    constructor(GoogleCloudStorageRestClient::GoogleCloudStorageRestClientIo rest, hash<GcsAuthContext> auth)
            : GoogleCloudStorageDataProviderBase(rest, auth) {}
    constructor(*hash<auto> options) : GoogleCloudStorageDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string method = (req.method ?? "GET").upr();
        int expires = req.expires_in_seconds ?? DefaultSignedUrlExpirySeconds;
        if (expires <= 0 || expires > MaxSignedUrlExpirySeconds) {
            throw "GCS-SIGNED-URL-ERROR", sprintf("expires_in_seconds is %d; it must be between 1 and "
                "%d (7 days), which is the maximum lifetime Google Cloud Storage allows for a V4 "
                "signature", expires, MaxSignedUrlExpirySeconds);
        }
        string signer = getSignerEmail(req);

        date now = now_utc();
        string datestamp = now.format("YYYYMMDD");
        string timestamp = datestamp + "T" + now.format("HHmmSS") + "Z";
        string credential_scope = datestamp + "/" + CredentialScopeSuffix;

        hash<auto> url = parse_url(GoogleCloudStorageRestClient::GcsApiUrl);
        string host = url.host;

        # the resource is addressed as "/BUCKET/OBJECT" with "/" left literal in the object name, which
        # is the form the signature covers
        string canonical_uri = "/" + encode_url(req.bucket, True) + "/"
            + GoogleCloudStorageRestClient::gcsEncodeObjectPath(req.object);

        # the query parameters are signed, so they have to appear in the canonical request sorted by
        # encoded name and in exactly the same encoding as in the final URL
        hash<auto> params = {
            "X-Goog-Algorithm": SignedUrlAlgorithm,
            "X-Goog-Credential": signer + "/" + credential_scope,
            "X-Goog-Date": timestamp,
            "X-Goog-Expires": expires.toString(),
            "X-Goog-SignedHeaders": SignedHeaders,
        };
        string canonical_query =
            GoogleCloudStorageCreateSignedUrlDataProvider::canonicalQueryString(params);

        string canonical_request = method + "\n"
            + canonical_uri + "\n"
            + canonical_query + "\n"
            + sprintf("host:%s\n", host) + "\n"
            + SignedHeaders + "\n"
            + UnsignedPayload;

        string string_to_sign = SignedUrlAlgorithm + "\n"
            + timestamp + "\n"
            + credential_scope + "\n"
            + SHA256(canonical_request);

        debug("GCS signed URL canonical request: %y", canonical_request);

        string signature = signBlob(signer, string_to_sign);

        return {
            "url": sprintf("https://%s%s?%s&X-Goog-Signature=%s", host, canonical_uri, canonical_query,
                signature),
            "method": method,
            "expires": now + seconds(expires),
            "expires_in_seconds": expires,
            "signer_email": signer,
            "bucket": req.bucket,
            "object": req.object,
        };
    }

    #! Builds the canonical query string: every parameter strictly encoded and sorted by encoded name
    static string canonicalQueryString(hash<auto> params) {
        list<string> encoded = ();
        foreach hash<auto> i in (params.pairIterator()) {
            push encoded, sprintf("%s=%s", encode_url(i.key, True),
                encode_url(i.value.toString(), True));
        }
        # sorting the fully-encoded "name=value" strings orders by encoded name, since "=" sorts below
        # every character that can appear in an encoded name
        return join("&", sort(encoded));
    }

    #! Returns the service-account email to sign with
    /** @throw GCS-SIGNED-URL-ERROR no signer could be determined
    */
    private string getSignerEmail(hash<auto> req) {
        foreach *string candidate in ((req.signer_email, auth.signer_email)) {
            if (candidate.val()) {
                return candidate;
            }
        }
        if (auth.sa_json_path.val()) {
            *string email = readServiceAccountEmail(auth.sa_json_path);
            if (email.val()) {
                return email;
            }
            throw "GCS-SIGNED-URL-ERROR", sprintf("the service-account key at %y has no "
                "\"client_email\" field, so the signing service account cannot be determined; set "
                "\"signer_email\" on the request or \"gcp_signer_email\" on the connection",
                auth.sa_json_path);
        }
        throw "GCS-SIGNED-URL-ERROR", "no signing service account is configured and none could be "
            "discovered: Application Default Credentials are in use, so there is no key file to read "
            "a \"client_email\" from.  Set \"signer_email\" on the request or \"gcp_signer_email\" on "
            "the connection";
    }

    #! Reads the \c client_email field from a service-account key
    private *string readServiceAccountEmail(string location) {
        string text;
        try {
            text = FileLocationHandler::getTextFileFromLocation(location);
        } catch (hash<ExceptionInfo> ex) {
            throw "GCS-SIGNED-URL-ERROR", sprintf("cannot read the service-account key at %y to "
                "determine the signing service account: %s: %s", location, ex.err, ex.desc), ex;
        }
        try {
            return parse_json(text).client_email;
        } catch (hash<ExceptionInfo> ex) {
            throw "GCS-SIGNED-URL-ERROR", sprintf("the service-account key at %y is not valid JSON: "
                "%s", location, ex.desc), ex;
        }
    }

    #! Signs a payload with the service account's key via the IAM Credentials API and returns hex
    /** @param signer the signing service account's email
        @param payload the string to sign

        @return the signature as a lower-case hex string, which is the form a V4 signed URL carries

        @throw GCS-SIGNED-URL-ERROR the signing request failed or returned no signature
    */
    private string signBlob(string signer, string payload) {
        # signBlob is an IAM operation, so it needs a cloud-platform token rather than the storage
        # token the rest of this module uses; GcpAuth caches per credential and scope set, so this is
        # a separate cached token and not a second exchange per call.  An externally-minted token is
        # used as supplied, which is why it has to carry cloud-platform when URLs are signed
        string token = auth.access_token.val()
            ? auth.access_token
            : GcpAuth::getAccessToken(auth.sa_json_path,
                (GoogleCloudStorageRestClient::ScopeCloudPlatform,));
        string path = sprintf("/v1/projects/-/serviceAccounts/%s:signBlob", encode_url(signer, True));

        hash<auto> resp;
        try {
            resp = getIamClient().restPost(path, {
                "payload": make_base64_string(payload),
            }, GoogleCloudStorageRestClient::gcsAuthHeaders(token, auth.quota_project_id));
        } catch (hash<ExceptionInfo> ex) {
            rethrow ex.err, sprintf("signing a URL for service account %y failed: %s.  The caller "
                "needs the \"iam.serviceAccounts.signBlob\" permission on that service account (the "
                "\"roles/iam.serviceAccountTokenCreator\" role), and the IAM Service Account "
                "Credentials API must be enabled on the project", signer, ex.desc), ex.arg;
        }
        *string signed = resp.body.signedBlob;
        if (!signed.val()) {
            throw "GCS-SIGNED-URL-ERROR", sprintf("the signBlob call for service account %y returned "
                "no \"signedBlob\": %y", signer, resp.body);
        }
        return parse_base64_string(signed).toHex();
    }

    #! Returns the IAM Credentials client, creating it on first use
    /** The IAM Credentials API is a different host from Google Cloud Storage, so it needs its own
        client; it is created lazily because most providers never sign anything.
    */
    private RestClientIo::RestClientIo getIamClient() {
        if (iam) {
            return iam;
        }
        iam_mutex.lock();
        on_exit iam_mutex.unlock();
        if (!iam) {
            iam = new RestClientIo::RestClientIo({
                "url": GoogleCloudStorageRestClient::IamCredentialsUrl,
                "data": "json",
            });
        }
        return iam;
    }
}
}
