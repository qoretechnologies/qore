# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudDataProviderBase.qc base class for SendCloud data providers

/*  SendCloudDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SendCloudDataProvider module
public namespace SendCloudDataProvider {

#! Base class for SendCloud data providers
public class SendCloudDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for SendCloud data providers
        const BaseConstructorOptions = {
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The RestClientIo object to use",
                "type": AbstractDataProviderType::get(
                    new Type("RestClientIo::RestClientIo"), NOTHING, {
                        DTT_ClientOnly: True,
                    }),
                "desc": "The RestClientIo object to use for API calls",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Options for creating a REST client",
                "type": AbstractDataProviderType::get(AutoHashType, NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "Options for creating a RestClientIo object if one is not provided directly",
            },
        };
    }

    public {
        #! v3 API base URL (used for shipments, returns, parcel-documents, etc.)
        const V3BaseUrl = "https://" + SendCloudRestClient::SendCloudRestClientBase::DefaultHost + "/api/v3";
    }

    private {
        #! The REST client object for API calls (async-capable). May be NOTHING for
        #! introspection-only constructions (e.g., fetching example event data).
        *RestClientIo::RestClientIo rest;

        #! Cached SendCloud credentials (\c {public_key, secret_key}) used to lazily
        #! build separate synchronous clients for cross-host or cross-prefix endpoints
        #! (service points, v3).  NOTHING when constructed from a raw RestClientIo
        #! without access to the original options.
        *hash<auto> sc_creds;

        #! Lazy synchronous REST clients keyed by base URL (origin or origin+prefix).
        #! Used for cross-host endpoints (servicepoints subdomain) and v3 prefix calls.
        hash<string, RestClient::RestClient> sync_clients;

        #! Cached shipping-method-id → shipping-option-code translations from the v3
        #! compatibility endpoint (process-local; safe to cache as IDs are stable).
        hash<string, *string> shipping_option_code_cache;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object from a RestClientIo client
    constructor(*RestClientIo::RestClientIo rest) {
        self.rest = rest;
        recoverCredentialsFromRest();
    }

    #! Creates the object from a connection
    constructor(SendCloudRestClient::SendCloudRestConnection conn) {
        rest = conn.getAsync();
        # Try recovering credentials from the rest client first (preferred path: it
        # retains the original public_key/secret_key supplied at construction time).
        recoverCredentialsFromRest();
        # Fall back to pulling raw connection options.  AbstractConnection.getRealOptions()
        # returns the original user-supplied option hash before per-request translation,
        # so it still contains public_key/secret_key.
        if (!sc_creds) {
            *hash<auto> ropts = conn.getRealOptions();
            extractCredentialsFromOptions(ropts);
        }
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
        }
        if (!rest) {
            hash<auto> opts = copts.restclient_options ?? {};
            rest = new SendCloudRestClient::SendCloudRestClientIo(opts);
            extractCredentialsFromOptions(opts);
        }
        # Always try recovering creds from the rest client (caller may have supplied
        # a pre-built SendCloudRestClientIo via `restclient`) and from `restclient_options`
        # if it was provided alongside an explicit `restclient`.
        if (!sc_creds) {
            recoverCredentialsFromRest();
        }
        if (!sc_creds && copts.restclient_options.typeCode() == NT_HASH) {
            extractCredentialsFromOptions(copts.restclient_options);
        }
    }

    #! Populates sc_creds from the rest client's retained credentials, if available
    private recoverCredentialsFromRest() {
        if (rest instanceof SendCloudRestClient::SendCloudRestClientIo) {
            *hash<auto> creds = (cast<SendCloudRestClient::SendCloudRestClientIo>(rest))
                .getSendCloudCredentials();
            if (creds.public_key && creds.secret_key) {
                sc_creds = creds;
            }
        }
    }

    #! Populates sc_creds from a raw options hash (accepts both naming conventions)
    private extractCredentialsFromOptions(*hash<auto> opts) {
        *string pk = opts.public_key ?? opts.username;
        *string sk = opts.secret_key ?? opts.password;
        if (pk.val() && sk.val()) {
            sc_creds = {"public_key": pk, "secret_key": sk};
        }
    }

    #! Returns the REST client object
    *RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    #! Returns the cached SendCloud credentials (or NOTHING if unknown)
    *hash<auto> getSendCloudCredentials() {
        return sc_creds;
    }

    #! Stores SendCloud credentials so cross-host endpoints (service points) can authenticate
    /** This is called by the parent provider when constructing children so each child can
        lazily build its own service-points REST client.  Does nothing if \a creds is NOTHING.
    */
    setSendCloudCredentials(*hash<auto> creds) {
        if (creds.public_key && creds.secret_key) {
            sc_creds = creds;
        }
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        # RestClientIo does not support setLogger()
        LoggerWrapper::setLogger(logger);
    }

    #! Makes a GET request to the SendCloud API
    /** @param path the API path (no leading slash)
        @param params optional query parameters
        @return the response body hash

        @throw SENDCLOUD-API-ERROR if the response indicates an error
    */
    hash<auto> doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        try {
            hash<auto> response = rest.restGet(url);
            return response.body ?? {};
        } catch (hash<ExceptionInfo> ex) {
            rethrow ex.err, getApiErrorDesc(ex), ex.arg;
        }
    }

    #! Makes a POST request to the SendCloud API
    hash<auto> doPost(string path, *hash<auto> body) {
        try {
            hash<auto> response = rest.restPost(path, body);
            return response.body ?? {};
        } catch (hash<ExceptionInfo> ex) {
            rethrow ex.err, getApiErrorDesc(ex), ex.arg;
        }
    }

    #! Makes a PUT request to the SendCloud API
    hash<auto> doPut(string path, *hash<auto> body) {
        try {
            hash<auto> response = rest.restPut(path, body);
            return response.body ?? {};
        } catch (hash<ExceptionInfo> ex) {
            rethrow ex.err, getApiErrorDesc(ex), ex.arg;
        }
    }

    #! Makes a DELETE request to the SendCloud API
    hash<auto> doDelete(string path) {
        try {
            hash<auto> response = rest.restDel(path);
            return response.body ?? {};
        } catch (hash<ExceptionInfo> ex) {
            rethrow ex.err, getApiErrorDesc(ex), ex.arg;
        }
    }

    #! POST with a body wrapped under a key, and unwrap response under the same key when present
    /** @param path the API path
        @param wrapperKey the wrapper key (e.g. "parcel")
        @param body the inner body to wrap

        @return the unwrapped response hash (response.body{wrapperKey} if present; otherwise the whole body)
    */
    hash<auto> doPostWrapped(string path, string wrapperKey, hash<auto> body) {
        hash<auto> response = doPost(path, {wrapperKey: body});
        if (response{wrapperKey}.typeCode() == NT_HASH) {
            return response{wrapperKey};
        }
        return response;
    }

    #! PUT with a body wrapped under a key, and unwrap response under the same key when present
    hash<auto> doPutWrapped(string path, string wrapperKey, hash<auto> body) {
        hash<auto> response = doPut(path, {wrapperKey: body});
        if (response{wrapperKey}.typeCode() == NT_HASH) {
            return response{wrapperKey};
        }
        return response;
    }

    #! Makes an absolute-URL GET request (used for cross-host endpoints like Service Points)
    /** @param absolute_url the full https://... URL (host + path; query string built from \c params)
        @param params optional query parameters

        @return the parsed response body (may be a list for endpoints that return bare arrays)

        @throw SENDCLOUD-ERROR no credentials are available for the cross-host call (this can
        happen only when the data provider was constructed from a raw RestClientIo without
        connection or option-level credentials)
    */
    auto doAbsoluteGet(string absolute_url, *hash<auto> params) {
        # The async RestClientIo passed in `rest` is bound to panel.sendcloud.sc/api/v2;
        # cross-host endpoints (servicepoints.sendcloud.sc) and cross-prefix endpoints
        # (panel.sendcloud.sc/api/v3) need separate sync clients built with the same
        # credentials.  Split the absolute URL into origin (scheme://host) and path, then
        # call the path on a cached client bound to that origin.
        hash<UrlInfo> ui = parse_url(absolute_url);
        string origin = sprintf("%s://%s", ui.protocol ?? "https", ui.host);
        string path = ui.path ?? "/";
        if (params) {
            path += "?" + buildQueryString(params);
        }
        RestClient::RestClient client = getSyncClient(origin);
        try {
            hash<auto> response = client.restGet(path);
            return response.body;
        } catch (hash<ExceptionInfo> ex) {
            rethrow ex.err, getApiErrorDesc(ex), ex.arg;
        }
    }

    #! Makes a v3 GET request against \c https://panel.sendcloud.sc/api/v3
    /** @param path the v3 path (no leading slash; e.g. \c "shipments")
        @param params optional query parameters

        @return the parsed response body
    */
    auto doV3Get(string path, *hash<auto> params) {
        string url = "/" + path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        RestClient::RestClient client = getSyncClient(V3BaseUrl);
        try {
            hash<auto> response = client.restGet(url);
            return response.body;
        } catch (hash<ExceptionInfo> ex) {
            rethrow ex.err, getApiErrorDesc(ex), ex.arg;
        }
    }

    #! Makes a v3 POST request against \c https://panel.sendcloud.sc/api/v3
    /** @param path the v3 path (no leading slash; e.g. \c "shipments/announce")
        @param body the request body (will be JSON-encoded)

        @return the parsed response body
    */
    auto doV3Post(string path, hash<auto> body) {
        string url = "/" + path;
        RestClient::RestClient client = getSyncClient(V3BaseUrl);
        try {
            hash<auto> response = client.restPost(url, body);
            return response.body;
        } catch (hash<ExceptionInfo> ex) {
            rethrow ex.err, getApiErrorDesc(ex), ex.arg;
        }
    }

    #! Translates a v2 shipping-method integer ID into a v3 shipping_option_code (cached)
    /** Calls SendCloud's v3 compatibility endpoint
        \c POST /api/v3/compat/shipping-options once per unseen ID and caches the result.
        Returns NOTHING if SendCloud could not map the ID (the compat endpoint returns
        \c null for unmappable IDs).

        @param shipping_method_id the v2 integer shipping method ID (e.g., \c 8)

        @return the corresponding v3 shipping option code (e.g., \c "sendcloud:letter") or
        NOTHING if no mapping exists

        @throw SENDCLOUD-ERROR if the compatibility endpoint cannot be reached
    */
    *string translateShippingMethodId(softint shipping_method_id) {
        string key = sprintf("%d", shipping_method_id);
        if (shipping_option_code_cache.hasKey(key)) {
            return shipping_option_code_cache{key};
        }
        hash<auto> resp = cast<hash>(doV3Post("compat/shipping-options",
            {"shipping_method_ids": (shipping_method_id,)}));
        # Response shape: {"data": {"<id>": "<code>" | null}}
        *string code;
        if (resp.data.typeCode() == NT_HASH) {
            auto v = resp.data{key};
            if (v.typeCode() == NT_STRING) {
                code = v;
            }
        }
        shipping_option_code_cache{key} = code;
        return code;
    }

    #! Returns a cached synchronous RestClient bound to the given base URL
    /** @param base_url the URL the client should treat as its base (origin or origin+prefix)
    */
    private RestClient::RestClient getSyncClient(string base_url) {
        if (sync_clients{base_url}) {
            return sync_clients{base_url};
        }
        if (!sc_creds) {
            throw "SENDCLOUD-ERROR",
                "cannot build a synchronous SendCloud client: no credentials are available "
                "(the data provider was constructed without access to the SendCloud public_key/secret_key)";
        }
        RestClient::RestClient c = new RestClient::RestClient({
            "url": base_url,
            "username": sc_creds.public_key,
            "password": sc_creds.secret_key,
            "data": "json",
        });
        sync_clients{base_url} = c;
        return c;
    }

    #! Fetches raw bytes from an endpoint (for PDF/binary downloads)
    /** @param path the API path
        @param params optional query parameters
        @param accept optional Accept header value

        @return a hash with keys: body (binary), content_type (string)
    */
    hash<auto> doGetBytes(string path, *hash<auto> params, *string accept) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        try {
            hash<auto> response = rest.restGet(url);
            return {
                "body": response.body,
                "content_type": response.content_type
                    ?? response.headers."content-type"
                    ?? "application/pdf",
            };
        } catch (hash<ExceptionInfo> ex) {
            rethrow ex.err, getApiErrorDesc(ex), ex.arg;
        }
    }

    #! Returns an enriched error description that includes the SendCloud API error message if available
    /** SendCloud uses two distinct error envelopes:

        v2 (\c /api/v2/...):
        - \c {"error": {"code": 123, "message": "...", "request": "..."}}
        - \c {"error": {"message": "...", "errors": [{"field": "name", "message": "is required"}]}}

        v3 (\c /api/v3/..., e.g. shipments/announce, returns/announce-synchronously) follows
        a JSON:API-style top-level array:
        - \c {"errors": [{"status": "400", "code": "validation_error", "detail": "...",
          "source": {"pointer": "/ship_with"}}, ...]}
    */
    private string getApiErrorDesc(hash<ExceptionInfo> ex) {
        string desc = ex.desc;
        # Prefer the already-deserialized body when present.  RestClient parses JSON
        # response bodies into `deserialized_body` even when the underlying error is
        # raised as `HTTP-CLIENT-RECEIVE-ERROR` (where `ex.arg.body` is the raw JSON
        # *string*).  Without this we'd fall through to NT_STRING and only append the
        # raw JSON dump — defeating the structured v2/v3 envelope parsing below.
        auto body = (ex.arg.deserialized_body.typeCode() == NT_HASH)
            ? ex.arg.deserialized_body
            : (ex.arg.body ?? ex.arg.response_body_raw);
        switch (body.typeCode()) {
            case NT_STRING:
                if (body.val()) {
                    desc += ": " + body;
                }
                break;
            case NT_HASH: {
                auto err = body.error;
                if (err.typeCode() == NT_HASH) {
                    # v2 nested-hash envelope
                    *string msg = err.message;
                    if (msg.val()) {
                        desc += ": " + msg;
                    }
                    if (err.errors.typeCode() == NT_LIST && err.errors) {
                        list<string> parts = ();
                        foreach auto f in (err.errors) {
                            if (f.typeCode() == NT_HASH) {
                                *string field = f.field ?? f.name;
                                *string fm = f.message ?? f.error;
                                if (field.val() && fm.val()) {
                                    parts += sprintf("%s: %s", field, fm);
                                } else if (fm.val()) {
                                    parts += fm;
                                }
                            } else if (exists f) {
                                parts += string(f);
                            }
                        }
                        if (parts) {
                            desc += " [" + parts.join("; ") + "]";
                        }
                    }
                } else if (err.typeCode() == NT_STRING) {
                    desc += ": " + err;
                } else if (body.errors.typeCode() == NT_LIST && body.errors) {
                    # v3 JSON:API-style top-level errors array
                    list<string> parts = ();
                    foreach auto e in (body.errors) {
                        if (e.typeCode() == NT_HASH) {
                            *string detail = e.detail ?? e.title ?? e.message;
                            *string code = e.code;
                            *string pointer;
                            if (e.source.typeCode() == NT_HASH) {
                                pointer = e.source.pointer ?? e.source.parameter;
                            }
                            string part;
                            if (pointer.val() && detail.val()) {
                                part = sprintf("%s: %s", pointer, detail);
                            } else if (detail.val()) {
                                part = detail;
                            } else if (code.val()) {
                                part = code;
                            }
                            if (code.val() && part.val() && part != code) {
                                part += sprintf(" (%s)", code);
                            }
                            if (part.val()) {
                                parts += part;
                            }
                        } else if (exists e) {
                            parts += string(e);
                        }
                    }
                    if (parts) {
                        desc += ": " + (parts.size() == 1 ? parts[0] : "[" + parts.join("; ") + "]");
                    }
                } else if (body.message.val()) {
                    desc += ": " + body.message;
                } else if (body.detail.val()) {
                    desc += ": " + body.detail;
                }
                break;
            }
        }
        return desc;
    }

    #! Builds a query string from a hash of parameters
    private string buildQueryString(hash<auto> params) {
        list<string> parts;
        foreach string key in (keys params) {
            auto val = params{key};
            if (exists val) {
                parts += sprintf("%s=%s", key, encode_url(string(val), True));
            }
        }
        return parts.join("&");
    }

    #! Fetches records using SendCloud cursor-based pagination
    /** @param path the API path
        @param result_key the key in the response containing the result list (e.g. "parcels", "returns", "brands")
        @param params optional query parameters
        @param limit optional maximum number of results to return (aggregate across pages)

        @return the combined list of records from all pages
    */
    list<auto> fetchAllCursorPages(string path, string result_key, *hash<auto> params, *int limit) {
        list<auto> results = ();
        hash<auto> qparams = cast<hash>(params ?? {});
        if (!exists qparams.page_size) {
            qparams.page_size = MaxPageSize;
        }
        string url = path + "?" + buildQueryString(qparams);
        while (True) {
            hash<auto> response;
            try {
                response = rest.restGet(url).body ?? {};
            } catch (hash<ExceptionInfo> ex) {
                rethrow ex.err, getApiErrorDesc(ex), ex.arg;
            }
            list<auto> page = response{result_key} ?? ();
            results += page;
            if (limit && results.size() >= limit) {
                results = results[0..limit-1];
                break;
            }
            *string next_url = response.next;
            if (!next_url) {
                break;
            }
            # Use the cursor from the next URL but route through the REST client's base URL
            try {
                hash<UrlInfo> parsed = parse_url(next_url);
                *string cursor;
                if (parsed.query) {
                    # parse_url returns query as a string; extract cursor= param manually
                    foreach string pair in (parsed.query.split("&")) {
                        list<string> kv = pair.split("=");
                        if (kv[0] == "cursor" && kv.size() > 1) {
                            cursor = decode_url(kv[1]);
                            break;
                        }
                    }
                }
                if (!cursor) {
                    break;
                }
                qparams.cursor = cursor;
                url = path + "?" + buildQueryString(qparams);
            } catch () {
                break;
            }
        }
        return results;
    }

    #! Returns information on supported reference data
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "shipping_methods": True,
            "sender_addresses": True,
            "brands": True,
            "parcels": True,
            "returns": True,
            "countries": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "shipping_methods": return getReferenceShippingMethods();
            case "sender_addresses": return getReferenceSenderAddresses();
            case "brands": return getReferenceBrands();
            case "parcels": return getReferenceParcels();
            case "returns": return getReferenceReturns();
            case "countries": return SendCloudDataProvider::CountryReferenceData;
        }
    }

    #! Returns available shipping methods for reference data
    private *list<hash<AllowedValueInfo>> getReferenceShippingMethods() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("shipping_methods");
            if (response.shipping_methods.typeCode() == NT_LIST) {
                foreach hash<auto> sm in (response.shipping_methods) {
                    string display_name = sm.name ?? sprintf("Method %d", sm.id);
                    string carrier = sm.carrier ?? "";
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": carrier,
                        "desc": carrier.val() ? sprintf("%s (%s)", display_name, carrier) : display_name,
                        "value": int(sm.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available sender addresses for reference data
    private *list<hash<AllowedValueInfo>> getReferenceSenderAddresses() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("user/addresses/sender");
            if (response.sender_addresses.typeCode() == NT_LIST) {
                foreach hash<auto> sa in (response.sender_addresses) {
                    string display_name = sa.company_name ?? sa.contact_name
                        ?? sprintf("Address %d", sa.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": sa.city ?? "",
                        "desc": display_name,
                        "value": int(sa.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available brands for reference data
    private *list<hash<AllowedValueInfo>> getReferenceBrands() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("brands", {"page_size": MaxPageSize});
            if (response.brands.typeCode() == NT_LIST) {
                foreach hash<auto> b in (response.brands) {
                    string display_name = b.name ?? sprintf("Brand %d", b.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": b.url ?? "",
                        "desc": display_name,
                        "value": int(b.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available parcels for reference data
    private *list<hash<AllowedValueInfo>> getReferenceParcels() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("parcels", {"page_size": MaxPageSize});
            if (response.parcels.typeCode() == NT_LIST) {
                foreach hash<auto> p in (response.parcels) {
                    string tracking = p.tracking_number ?? "";
                    string name = p.name ?? "";
                    string display_name;
                    if (tracking.val() && name.val()) {
                        display_name = sprintf("%s — %s", tracking, name);
                    } else if (tracking.val()) {
                        display_name = tracking;
                    } else if (name.val()) {
                        display_name = name;
                    } else {
                        display_name = sprintf("Parcel %d", p.id);
                    }
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": p.status.message ?? "",
                        "desc": display_name,
                        "value": int(p.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available returns for reference data
    private *list<hash<AllowedValueInfo>> getReferenceReturns() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("returns", {"page_size": MaxPageSize});
            if (response.returns.typeCode() == NT_LIST) {
                foreach hash<auto> r in (response.returns) {
                    string parcel_label = "—";
                    if (r.parcels.typeCode() == NT_LIST && r.parcels) {
                        parcel_label = string(r.parcels[0].id ?? r.parcels[0].tracking_number ?? "—");
                    } else if (exists r.parcel_id) {
                        parcel_label = string(r.parcel_id);
                    }
                    string display_name = sprintf("Return %d (parcel %s)", r.id, parcel_label);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": r.status ?? "",
                        "desc": display_name,
                        "value": int(r.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Formats a date as an ISO-8601 UTC datetime string for SendCloud query parameters
    static string formatDateTime(date dt) {
        return dt.format("YYYY-MM-DDTHH:mm:SSZ");
    }
}

} # namespace SendCloudDataProvider
