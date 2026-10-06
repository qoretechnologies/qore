# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RestClientDataProvider module definition

/** GenericApiCallProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the RestClientDataProvider module
public namespace RestClientDataProvider {
#! Framework-injected generic "Make an API call" data provider
/** This provider backs the auto-injected
    @ref DataProvider::AbstractDataProvider::GenericApiCallChildName "__call__" child and the
    <tt>make-api-call</tt> action that the @c DataProvider framework registers on every REST-capable app
    (see @ref DataProvider::DataProviderAppInfo::disable_generic_api_call "disable_generic_api_call"
    for the opt-out).

    It accepts the connection's authenticated REST client as an @c AbstractRestClient — the common
    parent of both the synchronous @c RestClient and the async @c RestClientIo — and dispatches
    arbitrary requests through it using the shared @c restDoRequest() entry point.

    Beyond the basic method/path/body/header inputs, the provider supports:
    - <b>Body type selection</b>: a <tt>body_type</tt> field (json / form / multipart / xml / yaml / text /
      binary / auto) drives both the body serialization and the <tt>Content-Type</tt> header. The body
      field's editor in the UI reshapes accordingly via the dynamic options mechanism.
    - <b>Path templating</b>: <tt>path_vars</tt> substitutes <tt>{name}</tt> placeholders in the URI path.
    - <b>Status assertions</b>: <tt>expected_status</tt> whitelists response codes; mismatches throw
      <tt>REST-STATUS-ERROR</tt>.
    - <b>Error passthrough</b>: <tt>error_passthru</tt> returns non-2xx responses as normal data instead of
      throwing.
    - <b>Multipart upload</b>: when <tt>body_type = multipart</tt>, the body is a
      @ref DataProvider::FileDataType "FileDataType" structure and the framework builds the
      multipart envelope.

    @note The legacy @c RestClientCallDataProvider remains the canonical generic-call provider for
    the standalone @c "GenericRest" app; this class is specific to the framework-wide auto-injection
    and exposes a richer schema.

    @since RestClientDataProvider 2.4
*/
public class GenericApiCallProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "__call__",
            "desc": "Framework-injected generic API call provider; makes a request to the configured "
                "REST server and returns the response",
            "type": "GenericApiCallProvider",
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type — the static, body_type-agnostic shape
        const RequestType = GenericApiCallRequestDataType;

        #! Response type — the lean three-field shape (status/headers/body, plus optional debug)
        const ResponseType = GenericApiCallResponseDataType;

        #! Default Content-Type per body_type
        const ContentTypeMap = {
            "json":      "application/json",
            "form":      "application/x-www-form-urlencoded",
            "xml":       "application/xml",
            "yaml":      "application/x-yaml",
            "text":      "text/plain",
            "binary":    "application/octet-stream",
            # multipart Content-Type includes a boundary parameter; set at build time
        };

        #! Header names whose values are redacted in any <tt>debug.request.headers</tt> echo
        /** Lowercased; matched case-insensitively. Anything matching these names has its value
            replaced with @c "***" so that bearer tokens, API keys, cookies, and proxy credentials
            never leak into response payloads, logs, or pipeline state. The set is conservative —
            extend it if your deployments use additional custom-named secrets in headers.
        */
        const RedactedRequestHeaders = {
            "ocp-apim-subscription-key": True,
            "authorization":            True,
            "proxy-authorization":      True,
            "cookie":                   True,
            "set-cookie":               True,
            "x-api-key":                True,
            "x-auth-token":             True,
            "x-amz-security-token":     True,
            "x-csrf-token":             True,
        };
    }

    private {
        #! The underlying REST client; sync @c RestClient or async @c RestClientIo are both accepted
        RestClient::AbstractRestClient rest;
        #! Private synchronous client; serializes call-specific settings and retains refreshed tokens
        *RestClient::RestClient sync_client;
        Mutex sync_mutex();
    }

    #! Creates the object from a REST client
    /** @param rest the authenticated REST client to dispatch requests through; may be any
        @c AbstractRestClient subclass (e.g. @c RestClient or @c RestClientIo)

        @note Configure the client before creating the provider. Synchronous clients are copied
        with @c copyWithUrl() to isolate encoding and error policies; their @c copySelf() overrides
        must preserve subclass authentication. The private copy retains refreshed tokens and
        connections across calls, which are serialized with a mutex. Async clients are used directly.

        @throw RESTCLIENT-ERROR the synchronous client's configuration cannot be copied
    */
    constructor(RestClient::AbstractRestClient rest) {
        self.rest = rest;
        if (rest instanceof RestClient::RestClient) {
            # HTTPClient's legacy send() encodes '%' unless pre_encoded_urls is enabled.
            # Own a same-class client so connection settings remain unchanged. Reusing this copy
            # retains refreshed/rotated OAuth tokens and the HTTP connection across generic calls.
            RestClient::RestClient sync = cast<RestClient::RestClient>(rest);
            sync_client = cast<RestClient::RestClient>(sync.copyWithUrl(sync.getUrl()));
            sync_client.setPreEncodedUrls(True);
            sync_client.setErrorPassthru(sync.getErrorPassthru());
            sync_client.setSerialization(sync.getSerialization());
            sync_client.setSendEncoding(sync.getSendEncoding() ?? "identity");
            sync_client.setAllowAnyResponse(sync.getAllowAnyResponse());
            sync_client.setValidator(sync.getValidator());
            sync_client.setConnectionPath(sync.getConnectionPath());
            sync_client.replaceDefaultHeaders(sync.getDefaultHeaders());
            sync_client.setLogger(sync.getLogger());
        }
    }

    #! Returns the data provider name
    string getName() {
        return AbstractDataProvider::GenericApiCallChildName;
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        # AbstractRestClient does not expose a uniform setLogger; subclasses opt-in via constructor
        LoggerWrapper::setLogger(logger);
    }

    #! The generic API call provider never nests inside itself
    /** This class holds the REST client in the conventional @c rest member, so the framework's reflective
        discovery would otherwise match it and advertise an endless
        @ref DataProvider::AbstractDataProvider::GenericApiCallChildName "__call__" chain. The framework
        also marks children it builds, so this override only matters for instances created directly.

        @return @ref False

        @since RestClientDataProvider 2.4
    */
    private bool hasGenericApiCallChildImpl() {
        return False;
    }

    #! Makes a request and returns the response
    /** @param req the request info — see @ref GenericApiCallRequestDataType
        @param request_options the request options after processing by validateRequestOptions()

        @return the response to the request, in the shape described by @ref ResponseType

        @throw REST-STATUS-ERROR the response status is not in <tt>expected_status</tt>
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        date start_time = now_us();

        # 1. resolve path variables
        string path = applyPathVars(req.path ?? "/", req.path_vars);

        # 2. append query args
        path = appendQueryArgs(path, req.query_args);

        # 3. serialize body and set Content-Type per body_type
        *string body_type = req.body_type;
        auto body;
        hash<auto> hdr = req.hdr ?? {};
        # case-insensitive scan for an explicit Content-Type on the per-call hdr — when present,
        # we must NOT overwrite it with the body_type default (the caller is explicitly steering
        # serialization, e.g. sending JSON body bytes with a custom Content-Type for an API that
        # wants a non-standard MIME type)
        bool ct_present = False;
        foreach string k in (keys hdr) {
            if (k.lwr() == "content-type") {
                ct_present = True;
                break;
            }
        }
        switch (body_type) {
            case "json":
                body = exists req.body ? make_json(req.body) : NOTHING;
                if (!ct_present) {
                    hdr."Content-Type" = ContentTypeMap.json;
                }
                break;
            case "form":
                body = exists req.body ? encodeFormBody(req.body) : NOTHING;
                if (!ct_present) {
                    hdr."Content-Type" = ContentTypeMap.form;
                }
                break;
            case "yaml":
                body = exists req.body ? make_yaml(req.body) : NOTHING;
                if (!ct_present) {
                    hdr."Content-Type" = ContentTypeMap.yaml;
                }
                break;
            case "xml":
%ifndef NoXml
                body = exists req.body
                    ? (req.body.typeCode() == NT_HASH ? make_xml(req.body) : string(req.body))
                    : NOTHING;
%else
                if (exists req.body && req.body.typeCode() != NT_STRING) {
                    throw "GENERIC-API-CALL-SERIALIZER-ERROR",
                        "XML serializer not available (install the xml module); supply body as a raw XML string";
                }
                body = exists req.body ? string(req.body) : NOTHING;
%endif
                if (!ct_present) {
                    hdr."Content-Type" = ContentTypeMap.xml;
                }
                break;
            case "text":
                body = exists req.body ? string(req.body) : NOTHING;
                if (!ct_present) {
                    hdr."Content-Type" = ContentTypeMap.text;
                }
                break;
            case "binary":
                body = exists req.body
                    ? (req.body.typeCode() == NT_BINARY ? req.body : binary(req.body))
                    : NOTHING;
                if (!ct_present) {
                    hdr."Content-Type" = ContentTypeMap.binary;
                }
                break;
            case "multipart":
                # build a multipart/form-data body from the FileDataType-shaped input
                string boundary = generateMultipartBoundary();
                body = buildMultipartBody(req.body, boundary);
                if (!ct_present) {
                    hdr."Content-Type" = "multipart/form-data; boundary=" + boundary;
                }
                break;
            case "auto":
            default:
                # pass-through: let the REST client decide
                body = req.body;
                break;
        }

        # 4. dispatch through the underlying REST client; capture status-code handling.
        # For body_type=auto we use restDoRequest() so the client's configured serialization runs.
        # For everything else we've already serialized the body to the requested format (and set the
        # Content-Type header), so we use restDoRawRequest() to bypass client-side serialization —
        # otherwise RestClient would JSON-wrap our already-serialized string a second time.
        RestClient::AbstractRestClient client = rest;
        *AutoLock sync_lock;
        *bool saved_passthru;
        on_exit if (exists saved_passthru) {
            sync_client.setErrorPassthru(saved_passthru);
        }
        if (sync_client) {
            # The lock remains held until the call's original policy is restored on every exit.
            sync_lock = new AutoLock(sync_mutex);
            saved_passthru = sync_client.setErrorPassthru(req.expected_status ? True
                : (req.error_passthru ?? sync_client.getErrorPassthru()));
            client = sync_client;
            path = HttpClientIo::encode_http_request_target(path);
        }
        hash<RestClient::RestResponseInfo> resp;
        try {
            if (!body_type || body_type == "auto") {
                resp = client.restDoRequest(req.method, path, body, hdr);
            } else {
                resp = client.restDoRawRequest(req.method, path, body, hdr);
            }
        } catch (hash<ExceptionInfo> ex) {
            # Only HTTP response errors can be accepted by a status policy. Authentication,
            # transport, serialization, and application-policy exceptions must retain their meaning.
            if (!(client instanceof RestClient::RestClient) && (req.error_passthru || req.expected_status)
                && ex.err == "REST-RESPONSE-ERROR"
                && ex.arg.typeCode() == NT_HASH
                && exists ex.arg.status_code) {
                resp = cast<hash<RestClient::RestResponseInfo>>(ex.arg);
            } else {
                rethrow;
            }
        }

        # An explicit False also overrides an async client's connection-level passthrough.
        if (exists req.error_passthru && !req.error_passthru && !req.expected_status && resp.status_code >= 300) {
            throw "REST-RESPONSE-ERROR", sprintf("HTTP %d response received", resp.status_code), resp;
        }

        # 5. expected_status assertion
        if (req.expected_status) {
            bool ok = False;
            foreach auto code in (req.expected_status) {
                if (int(code) == resp.status_code) {
                    ok = True;
                    break;
                }
            }
            if (!ok) {
                throw "REST-STATUS-ERROR",
                    sprintf("response status %d is not in expected_status %y (path: %y, method: %y)",
                        resp.status_code, req.expected_status, path, req.method),
                    resp;
            }
        }

        return buildLeanResponse(resp, req, hdr, body, path, start_time);
    }

    #! Builds the lean response hash matching @ref GenericApiCallResponseDataType
    /** Replaces @ref RestClientDataProviderBase::buildResponseHash() which echoed the full request
        (leaking bearer tokens in <tt>request-headers</tt>), duplicated <tt>response-headers</tt> into
        <tt>response-headers-raw</tt>, and included several @ref NOTHING placeholder fields.

        This builder returns only <tt>status</tt> / <tt>headers</tt> / <tt>body</tt> by default; when the caller set
        <tt>include_debug = True</tt> on the request, it also populates a <tt>debug</tt> sub-hash with the
        request method/URL/sanitized-headers and request/response sizes plus round-trip duration.
    */
    private hash<auto> buildLeanResponse(hash<RestClient::RestResponseInfo> resp, auto req,
            hash<auto> sent_hdr, auto sent_body, string sent_path, date start_time) {
        # lowercase header keys for case-insensitive lookup
        *hash<auto> response_headers;
        if (resp.headers) {
            response_headers = map {$1.key.lwr(): $1.value}, resp.headers.pairIterator();
            # strip the synthetic "status_message" pseudo-header that RestClient injects into
            # the headers hash — it's not a real HTTP header and clutters the response
            response_headers -= ("status_message",);
        }

        hash<auto> rv = {
            "status": resp.status_code,
            "headers": response_headers,
            "body": resp.body,
        };

        if (req.include_debug) {
            int duration_ms = (now_us() - start_time).durationMilliseconds();
            *int request_size = sent_body.typeCode() == NT_BINARY ? sent_body.size() :
                (sent_body.typeCode() == NT_STRING ? sent_body.length() : NOTHING);
            *int response_size = resp.body.typeCode() == NT_BINARY ? resp.body.size() :
                (resp.body.typeCode() == NT_STRING ? resp.body.length() : NOTHING);
            rv.debug = {
                "request": {
                    "method": req.method,
                    "url": sent_path,
                    # echo the headers we actually sent, with sensitive values redacted
                    "headers": redactRequestHeaders(sent_hdr),
                },
                "duration_ms": duration_ms,
                "request_size": request_size,
                "response_size": response_size,
            };
        }
        return rv;
    }

    #! Returns a copy of the given header hash with sensitive values replaced by "***"
    /** Matching is case-insensitive against @ref RedactedRequestHeaders. The keys themselves are
        preserved so callers can still see WHICH sensitive headers were sent — only the values
        are masked.
    */
    static *hash<auto> redactRequestHeaders(*hash<auto> hdr) {
        if (!hdr) {
            return;
        }
        hash<auto> rv = {};
        foreach hash<auto> i in (hdr.pairIterator()) {
            rv{i.key} = RedactedRequestHeaders{i.key.lwr()} ? "***" : i.value;
        }
        return rv;
    }

    #! Returns the description of a successful request message
    /** Returns the static request type (body field typed as @c auto). Refined at form-render time by
        @ref getRequestTypeWithOptionsImpl() based on the user's @c body_type selection.
    */
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the request type given current option data
    /** Refines the request type's @c body field based on the currently-selected @c body_type, so the
        UI form re-renders the body editor appropriately (JSON editor / file picker / text area /
        etc.). Called by the action catalog when the UI requests the option set.

        @param options the current option-data hash from the UI (option values may be wrapped in a
        @c value sub-key or supplied as raw scalars)
    */
    private *AbstractDataProviderType getRequestTypeWithOptionsImpl(*hash<auto> options) {
        return RequestType.withBodyType(extractBodyType(options));
    }

    #! Returns the request type given current request data
    /** Called during request validation; refines the body type based on the actual body_type value
        in the request data.
    */
    private *AbstractDataProviderType getRequestTypeWithDataImpl(auto req) {
        if (req.typeCode() != NT_HASH) {
            return RequestType;
        }
        return RequestType.withBodyType(extractBodyType(req));
    }

    #! Returns the description of a response message
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Substitutes {name} placeholders in a path with URL-encoded values from path_vars
    /** Missing keys are left unsubstituted (the placeholder remains literally in the path), so the
        server returns a 404 that's easier to diagnose than a silent path corruption.
    */
    static string applyPathVars(string path, *hash<auto> path_vars) {
        if (!path_vars) {
            return path;
        }
        foreach hash<auto> i in (path_vars.pairIterator()) {
            string placeholder = "{" + i.key + "}";
            string val = encode_url(string(i.value), True);
            path = replace(path, placeholder, val);
        }
        return path;
    }

    #! Appends query_args to a path, URL-encoding values
    /** @param path URI request target, optionally including an existing query
        @param query_args query names and values; booleans use lowercase and lists use comma separators
        @return the request target with separately escaped query names and values appended
        @par Example
        @code{.py}
string path = GenericApiCallProvider::appendQueryArgs("/contacts", {"email": "a+b@example.com"});
# /contacts?email=a%2Bb%40example.com
        @endcode
    */
    static string appendQueryArgs(string path, *hash<auto> query_args) {
        if (!query_args) {
            return path;
        }
        list<string> pairs = map encode_url($1.key, True) + "="
            + encode_url(RestClientDataProviderBase::getUriValue($1.value), True), query_args.pairIterator();
        return path + (path =~ /\?/ ? "&" : "?") + pairs.join("&");
    }

    #! URL-encodes a hash of form fields into application/x-www-form-urlencoded body bytes
    static string encodeFormBody(hash<auto> form) {
        list<string> pairs = ();
        foreach hash<auto> i in (form.pairIterator()) {
            pairs += encode_url(string(i.key), True) + "=" + encode_url(string(i.value), True);
        }
        return pairs.join("&");
    }

    #! Generates a token-safe random boundary for multipart body construction
    /** Returns a boundary value composed only of characters that are safe to interpolate into an
        unquoted RFC 2046 <tt>boundary=</tt> parameter — base64url charset (A-Z a-z 0-9 - _) prefixed with
        a recognizable marker. Avoids <tt>/</tt> and <tt>+</tt> (which standard base64 produces and which are in
        RFC 2045 tspecials, requiring the parameter value to be quoted) and <tt>=</tt> (used for padding).

        Includes 16 bytes of entropy from @c rand() so concurrent calls within the same microsecond
        don't collide on the timestamp alone — a boundary collision between two interleaved
        multipart uploads on the same connection would let one upload's body terminate the other's
        envelope. The timestamp is folded in for human-readable debuggability.
    */
    static string generateMultipartBoundary() {
        binary entropy = binary();
        for (int i = 0; i < 16; ++i) {
            entropy += chr(rand() % 256);
        }
        string raw = make_base64_string(binary(string(now_us())) + entropy);
        # base64url-safe normalisation: strip padding, replace +/ with -_
        return "----QoreGenericApiCall" + replace(replace(replace(raw, "=", ""), "/", "_"), "+", "-");
    }

    #! Builds a multipart/form-data body from a FileDataType-shaped input
    /** The input is expected to be a hash with @c name (filename), @c content (base64 string or raw
        binary), and @c mime_type fields, matching @ref DataProvider::FileDataType.

        @param body the FileDataType-shaped input
        @param boundary the multipart boundary string

        @return the raw multipart body bytes ready to send

        @throw GENERIC-API-CALL-MULTIPART-ERROR the body shape is not a recognized FileDataType
    */
    static binary buildMultipartBody(auto body, string boundary) {
        if (body.typeCode() != NT_HASH) {
            throw "GENERIC-API-CALL-MULTIPART-ERROR",
                sprintf("multipart body must be a FileDataType hash (name + content + mime_type); got %y",
                    body.fullType());
        }
        # sanitize values before interpolating into multipart headers — otherwise a CR/LF or `"` in
        # filename or mime_type could break the envelope (malformed parts) or inject arbitrary
        # header lines into the multipart body
        string filename = sanitizeMultipartHeaderValue(body.name ?? "upload");
        string mime_type = sanitizeMultipartHeaderValue(body.mime_type ?? "application/octet-stream");
        binary content = body.content.typeCode() == NT_BINARY
            ? body.content
            : binary(parse_base64_string(string(body.content)));
        binary msg = binary();
        msg += binary(sprintf("--%s\r\nContent-Disposition: form-data; name=\"file\"; "
            "filename=\"%s\"\r\nContent-Type: %s\r\n\r\n",
            boundary, filename, mime_type));
        msg += content;
        msg += binary(sprintf("\r\n--%s--\r\n", boundary));
        return msg;
    }

    #! Sanitizes a value for safe interpolation into a multipart Content-Disposition / Content-Type
    #! header
    /** Strips CR, LF, and NUL (which would terminate or inject header lines) and replaces literal
        double-quote with the percent-encoded form <tt>&#37;22</tt> (so the quoted-string form of the header
        attribute is not broken). Backslashes are also stripped to avoid quoted-string escape
        ambiguities. Whitespace at the edges is trimmed.

        Matches RFC 7578 §4.2 expectations for the quoted-string form and RFC 9110 §5.6.4 for
        general header field value safety.
    */
    static string sanitizeMultipartHeaderValue(string v) {
        # remove characters that would terminate the header line or break the multipart envelope
        v = replace(v, "\r", "");
        v = replace(v, "\n", "");
        v = replace(v, chr(0), "");
        # neutralize quoting characters that would corrupt the quoted-string value
        v = replace(v, "\\", "");
        v = replace(v, "\"", "%22");
        return trim(v);
    }

    #! Extracts the body_type value from either the wrapped or unwrapped option format
    /** Action options may arrive as <tt>{"body_type": "json"}</tt> or as <tt>{"body_type": {"value": "json"}}</tt>
        depending on the caller. Returns the unwrapped string value, or @ref NOTHING if not set.
    */
    static *string extractBodyType(*hash<auto> opts) {
        if (!opts) {
            return;
        }
        auto v = opts.body_type;
        if (v.typeCode() == NT_HASH && exists v.value) {
            return v.value;
        }
        if (v.typeCode() == NT_STRING) {
            return v;
        }
    }
}
}
