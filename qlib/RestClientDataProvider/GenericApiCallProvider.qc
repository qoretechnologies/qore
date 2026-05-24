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
    `make-api-call` action that the @c DataProvider framework registers on every REST-capable app
    (see @ref DataProvider::DataProviderAppInfo::disable_generic_api_call "disable_generic_api_call"
    for the opt-out).

    It accepts the connection's authenticated REST client as an @c AbstractRestClient — the common
    parent of both the synchronous @c RestClient and the async @c RestClientIo — and dispatches
    arbitrary requests through it using the shared @c restDoRequest() entry point.

    Beyond the basic method/path/body/header inputs, the provider supports:
    - **Body type selection**: a `body_type` field (json / form / multipart / xml / yaml / text /
      binary / auto) drives both the body serialization and the `Content-Type` header. The body
      field's editor in the UI reshapes accordingly via the dynamic options mechanism.
    - **Path templating**: `path_vars` substitutes `{name}` placeholders in the URI path.
    - **Status assertions**: `expected_status` whitelists response codes; mismatches throw
      `REST-STATUS-ERROR`.
    - **Error passthrough**: `error_passthru` returns non-2xx responses as normal data instead of
      throwing.
    - **Multipart upload**: when `body_type = multipart`, the body is a
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

        #! Header names whose values are redacted in any `debug.request.headers` echo
        /** Lowercased; matched case-insensitively. Anything matching these names has its value
            replaced with @c "***" so that bearer tokens, API keys, cookies, and proxy credentials
            never leak into response payloads, logs, or pipeline state. The set is conservative —
            extend it if your deployments use additional custom-named secrets in headers.
        */
        const RedactedRequestHeaders = {
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
    }

    #! Creates the object from a REST client
    /** @param rest the authenticated REST client to dispatch requests through; may be any
        @c AbstractRestClient subclass (e.g. @c RestClient or @c RestClientIo)
    */
    constructor(RestClient::AbstractRestClient rest) {
        self.rest = rest;
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

    #! Makes a request and returns the response
    /** @param req the request info — see @ref GenericApiCallRequestDataType
        @param request_options the request options after processing by validateRequestOptions()

        @return the response to the request, in the shape described by @ref ResponseType

        @throw REST-STATUS-ERROR the response status is not in `expected_status`
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
%ifndef NoJson
                body = exists req.body ? make_json(req.body) : NOTHING;
%else
                throw "GENERIC-API-CALL-SERIALIZER-ERROR", "JSON serializer not available; install the json module";
%endif
                if (!ct_present) hdr."Content-Type" = ContentTypeMap.json;
                break;
            case "form":
                body = exists req.body ? encodeFormBody(req.body) : NOTHING;
                if (!ct_present) hdr."Content-Type" = ContentTypeMap.form;
                break;
            case "yaml":
%ifndef NoYaml
                body = exists req.body ? make_yaml(req.body) : NOTHING;
%else
                throw "GENERIC-API-CALL-SERIALIZER-ERROR", "YAML serializer not available; install the yaml module";
%endif
                if (!ct_present) hdr."Content-Type" = ContentTypeMap.yaml;
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
                if (!ct_present) hdr."Content-Type" = ContentTypeMap.xml;
                break;
            case "text":
                body = exists req.body ? string(req.body) : NOTHING;
                if (!ct_present) hdr."Content-Type" = ContentTypeMap.text;
                break;
            case "binary":
                body = exists req.body
                    ? (req.body.typeCode() == NT_BINARY ? req.body : binary(req.body))
                    : NOTHING;
                if (!ct_present) hdr."Content-Type" = ContentTypeMap.binary;
                break;
            case "multipart":
                # build a multipart/form-data body from the FileDataType-shaped input
                string boundary = generateMultipartBoundary();
                body = buildMultipartBody(req.body, boundary);
                if (!ct_present) hdr."Content-Type" = "multipart/form-data; boundary=" + boundary;
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
        hash<RestClient::RestResponseInfo> resp;
        try {
            if (!body_type || body_type == "auto") {
                resp = rest.restDoRequest(req.method, path, body, hdr);
            } else {
                resp = rest.restDoRawRequest(req.method, path, body, hdr);
            }
        } catch (hash<ExceptionInfo> ex) {
            # if error_passthru is requested and the exception carries a response, convert it to data
            if (req.error_passthru
                && (ex.err == "REST-RESPONSE-ERROR" || ex.err == "REST-CLIENT-ERROR" || ex.err == "REST-CLIENT-RECEIVE-ERROR")
                && ex.arg.typeCode() == NT_HASH
                && exists ex.arg.status_code) {
                resp = cast<hash<RestClient::RestResponseInfo>>(ex.arg);
            } else {
                rethrow;
            }
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
        (leaking bearer tokens in `request-headers`), duplicated `response-headers` into
        `response-headers-raw`, and included several @ref NOTHING placeholder fields.

        This builder returns only `status` / `headers` / `body` by default; when the caller set
        `include_debug = True` on the request, it also populates a `debug` sub-hash with the
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
        if (!hdr) return;
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
        if (!path_vars) return path;
        foreach hash<auto> i in (path_vars.pairIterator()) {
            string placeholder = "{" + i.key + "}";
            string val = encode_url(string(i.value), True);
            path = replace(path, placeholder, val);
        }
        return path;
    }

    #! Appends query_args to a path, URL-encoding values
    static string appendQueryArgs(string path, *hash<auto> query_args) {
        if (!query_args) return path;
        # reuse the existing helper for consistency
        return RestClientDataProviderBase::getRequestPath({"path": path, "query_args": query_args});
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
        unquoted RFC 2046 `boundary=` parameter — base64url charset (A-Z a-z 0-9 - _) prefixed with
        a recognizable marker. Avoids `/` and `+` (which standard base64 produces and which are in
        RFC 2045 tspecials, requiring the parameter value to be quoted) and `=` (used for padding).

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
        double-quote with the percent-encoded form `%22` (so the quoted-string form of the header
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
    /** Action options may arrive as `{"body_type": "json"}` or as `{"body_type": {"value": "json"}}`
        depending on the caller. Returns the unwrapped string value, or @ref NOTHING if not set.
    */
    static *string extractBodyType(*hash<auto> opts) {
        if (!opts) return;
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
