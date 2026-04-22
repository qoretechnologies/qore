# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerDataProviderBase.qc base class for BaseLinker data providers

/*  BaseLinkerDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! Base class for all BaseLinker data providers
/** All data-provider REST traffic is dispatched through @ref callMethod() which
    wraps BaseLinker's single-endpoint RPC-over-HTTP protocol: every operation
    is a POST to `/connector.php` with a form-encoded body of the shape
    `method=<name>&parameters=<URL-encoded JSON>`. Responses are always HTTP 200;
    success/failure is signalled by the JSON body's `status` field.
*/
public class BaseLinkerDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options shared by all BaseLinker data providers
        const BaseConstructorOptions = {
            "baselinkerrestclient": <DataProviderOptionInfo>{
                "display_name": "BaseLinker REST Client",
                "short_desc": "The BaseLinkerRestClientIo object",
                "type": AbstractDataProviderType::get(
                    new Type("RestClientIo::RestClientIo")),
                "desc": "The `RestClientIo` object to use for API calls; if not provided, "
                    "one is built from `baselinkerrestclient_options`.",
            },
            "baselinkerrestclient_options": <DataProviderOptionInfo>{
                "display_name": "BaseLinker REST Client Options",
                "short_desc": "Fallback options used when no REST client is given",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a `BaseLinkerRestClientIo` object when one is not "
                    "supplied directly (must include `apikey`).",
            },
        };
    }

    private {
        #! The async REST client used for API calls
        RestClientIo::RestClientIo rest;

        #! The connection object, if applicable
        *BaseLinkerRestClient::BaseLinkerRestConnection conn;
    }

    #! Creates the object with no arguments (for use by child classes with deferred init)
    private constructor() {
    }

    #! Creates the object with an async REST client
    constructor(RestClientIo::RestClientIo rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(BaseLinkerRestClient::BaseLinkerRestConnection conn) {
        self.conn = conn;
        rest = conn.getAsync(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.baselinkerrestclient) {
            rest = copts.baselinkerrestclient;
        } else {
            hash<auto> client_opts = copts.baselinkerrestclient_options ?? {};
            rest = new BaseLinkerRestClient::BaseLinkerRestClientIo(client_opts);
        }
    }

    #! Returns the REST client
    RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    #! Returns the connection if any
    *BaseLinkerRestClient::BaseLinkerRestConnection getConnection() {
        return conn;
    }

    #! Invokes a BaseLinker API method and returns the parsed response body
    /** This is the single dispatcher that every child provider uses to call the
        BaseLinker API. It serializes `parameters` as JSON, pre-escapes `+` to
        `%2B` (so BaseLinker's form decoder does not convert `+` back to space —
        this matters for base64 payloads), URL-encodes the JSON, builds the
        form-encoded body `method=<name>&parameters=<encoded-json>`, and POSTs to
        the root endpoint. The parsed JSON response body is then validated by
        @ref checkResponse(); on API errors a `BASELINKER-API-ERROR` is thrown.

        @param method BaseLinker API method name, e.g. `"getOrders"`
        @param parameters hash of method parameters (may be `NOTHING` / empty)

        @return the successful response body hash
        @throw BASELINKER-API-ERROR if the response body indicates an error
        @throw BASELINKER-RESPONSE-ERROR if the response body is not a hash
    */
    hash<auto> callMethod(string method, *hash<auto> parameters) {
        # Serialize parameters as JSON (empty string if no params)
        string params_json = "";
        if (parameters) {
            params_json = make_json(parameters);
        }

        # Pre-escape '+' characters in the JSON string before URL-encoding.
        # Without this, BaseLinker's form decoder would convert '+' back to
        # space on the server side — which breaks base64-encoded payloads
        # inside the JSON (e.g. courier label data).
        params_json = replace(params_json, "+", "%2B");

        # URL-encode the JSON and build the form-encoded body
        string encoded_params = encode_url(params_json, True);
        string body = "method=" + encode_url(method, True) + "&parameters=" + encoded_params;

        hash<auto> headers = {
            "Content-Type": "application/x-www-form-urlencoded",
        };

        # BaseLinker's single RPC endpoint is at /connector.php; pass it as the path
        # rather than an empty string. RestClientIo normalizes an empty path to "/" and
        # then prepends the URL's connection path, producing "/connector.php/" — which
        # BaseLinker's nginx rewrites to a 404.
        return retryRequest(auto sub () {
            try {
                hash<auto> response = rest.restDoRawRequest("POST", "connector.php",
                    body, headers);
                return checkResponse(method, response.body);
            } catch (hash<ExceptionInfo> ex) {
                # BASELINKER-API-ERROR is already enriched by checkResponse; everything
                # else (REST-RESPONSE-ERROR, DESERIALIZATION-ERROR, socket errors) is
                # low-level and lacks context. Enrich the desc with the method name and
                # any parseable error body before re-throwing.
                if (ex.err == "BASELINKER-API-ERROR") {
                    rethrow;
                }
                rethrow ex.err, getApiErrorDesc(method, ex), ex.arg;
            }
        });
    }

    #! Returns an enriched error description for non-BASELINKER-API-ERROR failures
    /** BaseLinker normally reports application-level errors as HTTP 200 with a JSON body
        carrying \c status: \c "ERROR" — those are enriched by @ref checkResponse(). Two
        other failure modes are possible and reach this path:

        - HTTP 4xx/5xx from the endpoint (rare; usually infrastructure or CDN): raised as
          \c REST-RESPONSE-ERROR with \c arg.status_code and \c arg.body
        - Non-JSON response body (e.g., nginx HTML 404, maintenance page): raised as
          \c DESERIALIZATION-ERROR with the body embedded in \c desc

        @param method the BaseLinker API method being called
        @param ex the original exception

        @return an enriched description string that names the method and surfaces any
        API-provided error text that could be extracted
    */
    private string getApiErrorDesc(string method, hash<ExceptionInfo> ex) {
        if (ex.err == "REST-RESPONSE-ERROR") {
            string desc = sprintf("%s on method %s", ex.desc, method);
            # BaseLinker may return its RPC error envelope even on a non-2xx response
            *string api_msg = DataProvider::DataProviderErrorUtil::firstStringAt(
                ex.arg.body, ("error_message",));
            *string api_code = DataProvider::DataProviderErrorUtil::firstStringAt(
                ex.arg.body, ("error_code",));
            if (api_msg.val() || api_code.val()) {
                desc += sprintf(": %s (%s)",
                    api_msg ?? "(no message)", api_code ?? "no code");
            }
            return desc;
        }
        if (ex.err == "DESERIALIZATION-ERROR") {
            # The unparseable body lives in ex.desc; don't forward the full "types
            # supported" list — surface a compact message naming the method.
            return sprintf("BaseLinker returned a non-JSON response for method %s "
                "(likely an upstream infrastructure error or maintenance page)", method);
        }
        # Socket / timeout / other low-level errors: just add method context
        return sprintf("%s on method %s", ex.desc, method);
    }

    #! Validates a BaseLinker response body and converts API errors to Qore exceptions
    /** @param method the API method name (included in exception arg/desc for debugging)
        @param body the parsed response body

        @return the body unchanged on success (`status == "SUCCESS"` or no error indicator)
        @throw BASELINKER-API-ERROR if `body.status == "ERROR"`
        @throw BASELINKER-RESPONSE-ERROR if `body` is not a hash
    */
    hash<auto> checkResponse(string method, auto body) {
        if (body.typeCode() != NT_HASH) {
            throw "BASELINKER-RESPONSE-ERROR",
                sprintf("BaseLinker response for method %y is not a hash (got %y)",
                    method, body.type());
        }
        if (body.status == "ERROR") {
            throw "BASELINKER-API-ERROR",
                sprintf("%s (%s) on method %s",
                    body.error_message ?? "unknown error",
                    body.error_code ?? "UNKNOWN", method),
                {
                    "method": method,
                    "error_code": body.error_code,
                    "error_message": body.error_message,
                    "status_code": 200,
                };
        }
        return body;
    }

    #! Runs a callable with exponential-backoff retry on rate-limit and transient errors
    /** Retries up to @ref MaxRetries times with 1s/2s/4s sleep between attempts. Retries on:
        - `BASELINKER-API-ERROR` whose `error_code` matches a known rate-limit code
        - socket-level errors from @ref RetrySet
    */
    auto retryRequest(code func) {
        int retries = 0;
        while (True) {
            try {
                return func();
            } catch (hash<ExceptionInfo> ex) {
                if (retries >= MaxRetries) {
                    rethrow;
                }
                bool do_retry = False;
                if (RetrySet{ex.err}) {
                    do_retry = True;
                } else if (ex.err == "BASELINKER-API-ERROR") {
                    *string code_str = ex.arg.error_code;
                    if (code_str) {
                        if (RateLimitErrorCodes{code_str}) {
                            do_retry = True;
                        } else if (regex(code_str, "rate[_ ]?limit", RE_Caseless)) {
                            do_retry = True;
                        }
                    }
                } else if (ex.err == "REST-RESPONSE-ERROR") {
                    # Retry on edge/infrastructure 429 and transient 5xx
                    int status = ex.arg.status_code ?? 0;
                    if (status == 429 || (status >= 500 && status < 600)) {
                        do_retry = True;
                    }
                }
                if (!do_retry) {
                    rethrow;
                }
                ++retries;
                sleep(pow(2, retries - 1) * 1000ms);
            }
        }
    }

    #! Converts create-request fields into optional fields for matching update-request types
    /** Removes `required: True`, swaps strict types for their `*OrNothing` counterparts
        where available, and leaves every other attribute intact. Used by `set*` and
        `update*` request types to avoid duplicating the full field hash.
    */
    static hash<auto> makeFieldsOptional(hash<auto> fields) {
        hash<auto> or_nothing_map = {
            "string": StringOrNothingType,
            "softstring": SoftStringOrNothingType,
            "int": IntOrNothingType,
            "softint": SoftIntOrNothingType,
            "float": FloatOrNothingType,
            "softfloat": SoftFloatOrNothingType,
            "bool": BoolOrNothingType,
            "softbool": SoftBoolOrNothingType,
            "date": DateOrNothingType,
            "softdate": SoftDateOrNothingType,
        };
        return map {$1.key: $1.value + {
            "required": False,
        } + (($1.value.type.typeCode() == NT_STRING && or_nothing_map{$1.value.type})
            ? {"type": or_nothing_map{$1.value.type}} : {})},
            fields.pairIterator();
    }

    #! Returns the list of reference data kinds supported by this module
    *hash<string, bool> getSupportedReferenceDataImpl() {
        return {
            "orders": True,
            "order_returns": True,
            "order_products": True,
            "order_statuses": True,
            "order_return_statuses": True,
            "order_sources": True,
            "inventories": True,
            "inventory_products": True,
            "inventory_manufacturers": True,
            "inventory_categories": True,
            "couriers": True,
            "courier_services": True,
            "courier_accounts": True,
        };
    }

    #! Returns reference data for the requested kind
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string kind,
            *string filter_value, *hash<auto> depends_on_values) {
        try {
            switch (kind) {
                case "orders": return getReferenceOrders();
                case "order_returns": return getReferenceOrderReturns();
                case "order_products":
                    return getReferenceOrderProducts(depends_on_values.order_id);
                case "order_statuses": return getReferenceOrderStatuses();
                case "order_return_statuses": return getReferenceOrderReturnStatuses();
                case "order_sources": return getReferenceOrderSources();
                case "inventories": return getReferenceInventories();
                case "inventory_products":
                    return getReferenceInventoryProducts(depends_on_values.inventory_id);
                case "inventory_manufacturers":
                    return getReferenceInventoryManufacturers();
                case "inventory_categories":
                    return getReferenceInventoryCategories(depends_on_values.inventory_id);
                case "couriers": return getReferenceCouriers();
                case "courier_services":
                    return getReferenceCourierServices(depends_on_values.courier_code);
                case "courier_accounts":
                    return getReferenceCourierAccounts(depends_on_values.courier_code);
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "BaseLinker reference data (%s) failed: %s: %s",
                kind, ex.err, ex.desc);
        }
    }

    #! Returns recent orders as reference data (id → short description)
    private *list<hash<AllowedValueInfo>> getReferenceOrders() {
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> response = callMethod("getOrders", {"get_unconfirmed_orders": False});
        foreach hash<auto> order in (response.orders ?? ()) {
            rv += <AllowedValueInfo>{
                "value": int(order.order_id),
                "display_name": sprintf("Order %d (%s)", order.order_id,
                    order.email ?? order.delivery_fullname ?? "n/a"),
            };
        }
        return rv;
    }

    #! Returns recent order returns as reference data
    private *list<hash<AllowedValueInfo>> getReferenceOrderReturns() {
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> response = callMethod("getOrderReturns", NOTHING);
        foreach hash<auto> ret in (response.returns ?? ()) {
            rv += <AllowedValueInfo>{
                "value": int(ret.return_id),
                "display_name": sprintf("Return %d (order %d)", ret.return_id,
                    ret.order_id),
            };
        }
        return rv;
    }

    #! Returns order statuses
    private *list<hash<AllowedValueInfo>> getReferenceOrderStatuses() {
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> response = callMethod("getOrderStatusList", NOTHING);
        foreach hash<auto> status in (response.statuses ?? ()) {
            rv += <AllowedValueInfo>{
                "value": int(status.id),
                "display_name": status.name ?? sprintf("Status %d", status.id),
            };
        }
        return rv;
    }

    #! Returns order return statuses
    private *list<hash<AllowedValueInfo>> getReferenceOrderReturnStatuses() {
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> response = callMethod("getOrderReturnStatusList", NOTHING);
        foreach hash<auto> status in (response.statuses ?? ()) {
            rv += <AllowedValueInfo>{
                "value": int(status.id),
                "display_name": status.name ?? sprintf("Return status %d", status.id),
            };
        }
        return rv;
    }

    #! Returns order sources grouped by type
    private *list<hash<AllowedValueInfo>> getReferenceOrderSources() {
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> response = callMethod("getOrderSources", NOTHING);
        # sources is a hash keyed by type -> hash of id -> name
        foreach string source_type in (keys response.sources ?? {}) {
            hash<auto> by_id = response.sources{source_type};
            foreach string id in (keys by_id) {
                rv += <AllowedValueInfo>{
                    "value": int(id),
                    "display_name": sprintf("%s: %s", source_type, by_id{id}),
                };
            }
        }
        return rv;
    }

    #! Returns inventories (catalogs) as reference data
    private *list<hash<AllowedValueInfo>> getReferenceInventories() {
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> response = callMethod("getInventories", NOTHING);
        foreach hash<auto> inv in (response.inventories ?? ()) {
            rv += <AllowedValueInfo>{
                "value": string(inv.inventory_id),
                "display_name": inv.name ?? sprintf("Inventory %s", inv.inventory_id),
            };
        }
        return rv;
    }

    #! Returns inventory products for a given inventory
    private *list<hash<AllowedValueInfo>> getReferenceInventoryProducts(*string inventory_id) {
        if (!inventory_id.val()) {
            return ();
        }
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> response = callMethod("getInventoryProductsList", {
            "inventory_id": inventory_id,
            "page": 1,
        });
        # products is a hash keyed by product_id
        foreach string pid in (keys response.products ?? {}) {
            auto p = response.products{pid};
            string name;
            if (p.typeCode() == NT_HASH) {
                name = p.name ?? pid;
            } else {
                name = pid;
            }
            rv += <AllowedValueInfo>{
                "value": pid,
                "display_name": sprintf("%s (%s)", name, pid),
            };
        }
        return rv;
    }

    #! Returns couriers
    private *list<hash<AllowedValueInfo>> getReferenceCouriers() {
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> response = callMethod("getCouriersList", NOTHING);
        foreach hash<auto> courier in (response.couriers ?? ()) {
            rv += <AllowedValueInfo>{
                "value": string(courier.code),
                "display_name": courier.name ?? string(courier.code),
            };
        }
        return rv;
    }

    #! Returns courier services for a given courier code
    private *list<hash<AllowedValueInfo>> getReferenceCourierServices(*string courier_code) {
        if (!courier_code.val()) {
            return ();
        }
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> response = callMethod("getCourierServices", {
            "courier_code": courier_code,
        });
        foreach string service_id in (keys response.services ?? {}) {
            rv += <AllowedValueInfo>{
                "value": service_id,
                "display_name": string(response.services{service_id}),
            };
        }
        return rv;
    }

    #! Returns courier accounts for a given courier code
    private *list<hash<AllowedValueInfo>> getReferenceCourierAccounts(*string courier_code) {
        if (!courier_code.val()) {
            return ();
        }
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> response = callMethod("getCourierAccounts", {
            "courier_code": courier_code,
        });
        foreach hash<auto> account in (response.accounts ?? ()) {
            rv += <AllowedValueInfo>{
                "value": int(account.id),
                "display_name": account.name ?? sprintf("Account %s", account.id),
            };
        }
        return rv;
    }

    #! Returns line items for a given order (cascading ref_data)
    private *list<hash<AllowedValueInfo>> getReferenceOrderProducts(*string order_id) {
        if (!order_id.val()) {
            return ();
        }
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> response = callMethod("getOrders", {
            "order_id": int(order_id),
            "get_unconfirmed_orders": True,
        });
        foreach hash<auto> order in (response.orders ?? ()) {
            foreach hash<auto> product in (order.products ?? ()) {
                rv += <AllowedValueInfo>{
                    "value": int(product.order_product_id),
                    "display_name": sprintf("%s (line %d, qty %d)",
                        product.name ?? "product",
                        product.order_product_id,
                        product.quantity ?? 0),
                };
            }
        }
        return rv;
    }

    #! Returns inventory manufacturers as reference data
    private *list<hash<AllowedValueInfo>> getReferenceInventoryManufacturers() {
        list<hash<AllowedValueInfo>> rv = ();
        hash<auto> response = callMethod("getInventoryManufacturers", NOTHING);
        foreach hash<auto> m in (response.manufacturers ?? ()) {
            rv += <AllowedValueInfo>{
                "value": string(m.manufacturer_id),
                "display_name": m.name ?? m.manufacturer_name
                    ?? sprintf("Manufacturer %s", m.manufacturer_id),
            };
        }
        return rv;
    }

    #! Returns inventory categories as reference data, optionally scoped to a catalog
    private *list<hash<AllowedValueInfo>> getReferenceInventoryCategories(*string inventory_id) {
        list<hash<AllowedValueInfo>> rv = ();
        *hash<auto> params;
        if (inventory_id.val()) {
            params = {"inventory_id": inventory_id};
        }
        hash<auto> response = callMethod("getInventoryCategories", params);
        foreach hash<auto> cat in (response.categories ?? ()) {
            rv += <AllowedValueInfo>{
                "value": string(cat.category_id),
                "display_name": cat.name ?? sprintf("Category %s", cat.category_id),
            };
        }
        return rv;
    }
}

} # namespace BaseLinkerDataProvider
