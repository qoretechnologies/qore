# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaDataProviderBase.qc Base class for Katana data providers

/*  KatanaDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/

#! Contains all public definitions in the KatanaDataProvider module
public namespace KatanaDataProvider {

#! Base class for Katana data providers
public class KatanaDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Katana data providers
        const BaseConstructorOptions = {
            "katanarestclient": <DataProviderOptionInfo>{
                "display_name": "Katana REST Client",
                "short_desc": "The KatanaRestClientIo object",
                "type": AbstractDataProviderType::get(
                    new Type("RestClientIo::RestClientIo")),
                "desc": "The RestClientIo object to use for API calls",
            },
            "katanarestclient_options": <DataProviderOptionInfo>{
                "display_name": "Katana REST Client Options",
                "short_desc": "Options for creating a KatanaRestClientIo",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a KatanaRestClientIo object if one is not provided",
            },
        };

        #! Common search options for all list/find collection providers
        const CommonSearchOptions = {
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of records to return per page",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of records to return per page (max " + MaxPageSize + ")",
                "preselected": True,
            },
            "page": <DataProviderOptionInfo>{
                "display_name": "Page",
                "short_desc": "Page number (1-based)",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Page number for pagination (1-based)",
            },
            "ids": <DataProviderOptionInfo>{
                "display_name": "IDs",
                "short_desc": "Filter by a list of IDs",
                "type": AbstractDataProviderType::get(SoftListOrNothingType),
                "desc": "Filter results to only the records with the given integer IDs",
            },
            "created_at_min": <DataProviderOptionInfo>{
                "display_name": "Created At (Min)",
                "short_desc": "Only return records created on/after this UTC timestamp",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Only return records created on or after this UTC timestamp (ISO 8601)",
            },
            "created_at_max": <DataProviderOptionInfo>{
                "display_name": "Created At (Max)",
                "short_desc": "Only return records created on/before this UTC timestamp",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Only return records created on or before this UTC timestamp",
            },
            "updated_at_min": <DataProviderOptionInfo>{
                "display_name": "Updated At (Min)",
                "short_desc": "Only return records updated on/after this UTC timestamp",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Only return records updated on or after this UTC timestamp",
            },
            "updated_at_max": <DataProviderOptionInfo>{
                "display_name": "Updated At (Max)",
                "short_desc": "Only return records updated on/before this UTC timestamp",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Only return records updated on or before this UTC timestamp",
            },
            "include_deleted": <DataProviderOptionInfo>{
                "display_name": "Include Deleted",
                "short_desc": "Include soft-deleted records",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "If `True`, includes soft-deleted records in the response",
            },
        };
    }

    private {
        #! The REST client object for API calls (async RestClientIo)
        RestClientIo::RestClientIo rest;

        #! The connection object, if applicable
        *KatanaRestClient::KatanaRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client (async)
    constructor(RestClientIo::RestClientIo rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(KatanaRestClient::KatanaRestConnection conn) {
        self.conn = conn;
        rest = conn.getAsync(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.katanarestclient) {
            rest = copts.katanarestclient;
        } else {
            hash<auto> client_opts = copts.katanarestclient_options ?? {};
            rest = new KatanaRestClient::KatanaRestClientIo(client_opts);
        }
    }

    #! Returns the REST client
    RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *KatanaRestClient::KatanaRestConnection getConnection() {
        return conn;
    }

    #! Makes a GET request to the Katana API
    auto doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequestIntern("GET", url);
    }

    #! Makes a GET request and unwraps the {data: [...]} envelope
    list<auto> doGetList(string path, *hash<auto> params) {
        auto result = doGet(path, params);
        if (result.typeCode() == NT_HASH && exists result.data) {
            return result.data ?? ();
        }
        if (result.typeCode() == NT_LIST) {
            return result;
        }
        return ();
    }

    #! Makes a POST request to the Katana API
    auto doPost(string path, *hash<auto> body) {
        return doRestRequestIntern("POST", path,
            body ? KatanaDataProviderBase::serializeDates(body) : body);
    }

    #! Makes a PATCH request to the Katana API
    auto doPatch(string path, *hash<auto> body) {
        return doRestRequestIntern("PATCH", path,
            body ? KatanaDataProviderBase::serializeDates(body) : body);
    }

    #! Makes a DELETE request to the Katana API
    auto doDelete(string path) {
        return doRestRequestIntern("DELETE", path);
    }

    #! Fetches all pages from a GET endpoint using page-based pagination
    list<hash<auto>> fetchAllPages(string path, *hash<auto> params, *int limit) {
        list<hash<auto>> results = ();
        int page = 1;
        int per_page = (limit && limit < MaxPageSize) ? limit : MaxPageSize;

        while (True) {
            hash<auto> qparams = params ?? {};
            qparams.page = page;
            qparams.limit = per_page;

            list<auto> chunk = doGetList(path, qparams);
            if (chunk) {
                results += chunk;
            }
            if (limit && results.size() >= limit) {
                results = results[0..limit-1];
                break;
            }
            if (!chunk || chunk.size() < per_page) {
                break;
            }
            ++page;
        }
        return results;
    }

    #! Internal REST request implementation with retry and rate-limit handling
    private auto doRestRequestIntern(string method, string path, auto body = NOTHING) {
        int retries = 0;
        while (True) {
            try {
                hash<auto> response;
                switch (method) {
                    case "GET":
                        response = rest.restGet(path);
                        break;
                    case "POST":
                        response = rest.restPost(path, body);
                        break;
                    case "PATCH":
                        response = rest.restPatch(path, body);
                        break;
                    case "DELETE":
                        response = rest.restDel(path);
                        break;
                    default:
                        throw "KATANA-ERROR", sprintf("unsupported HTTP method: %s", method);
                }
                return response.body;
            } catch (hash<ExceptionInfo> ex) {
                if (retryRequest(ex, \retries)) {
                    continue;
                }
                rethrow ex.err, getApiErrorDesc(ex), ex.arg;
            }
        }
    }

    #! Returns an enriched error description that includes the Katana API error message
    private string getApiErrorDesc(hash<ExceptionInfo> ex) {
        string desc = ex.desc;
        auto body = ex.arg.body;
        if (body.typeCode() == NT_HASH) {
            *string msg = DataProvider::DataProviderErrorUtil::firstStringAt(
                body, ("message", "Message", "error", "Error"));
            if (msg.val()) {
                desc += ": " + msg;
            }
            if (body.errors.typeCode() == NT_LIST) {
                list<string> parts = ();
                foreach auto e in (body.errors) {
                    if (e.typeCode() == NT_HASH) {
                        string part = "";
                        if (e.path.val()) {
                            part = string(e.path) + ": ";
                        }
                        part += string(e.message ?? e.code ?? "");
                        if (part.val()) {
                            push parts, part;
                        }
                    } else {
                        push parts, sprintf("%y", e);
                    }
                }
                if (parts) {
                    desc += " [" + join("; ", parts) + "]";
                }
            }
        } else if (body.typeCode() == NT_STRING && body.val()) {
            desc += ": " + body;
        }
        return desc;
    }

    #! Checks if a request should be retried
    private bool retryRequest(hash<ExceptionInfo> ex, reference<int> retries) {
        if (retries >= MaxRetries) {
            return False;
        }
        if (RetrySet{ex.err}) {
            ++retries;
            sleep(pow(2, retries) * 1000ms);
            return True;
        }
        if (ex.err == "REST-RESPONSE-ERROR" && ex.arg.status_code == 429) {
            ++retries;
            *string retry_after = ex.arg.headers."retry-after";
            int wait_secs = retry_after.val() ? retry_after.toInt() : 5;
            if (wait_secs < 1 || wait_secs > 60) {
                wait_secs = 5;
            }
            sleep(wait_secs * 1000ms);
            return True;
        }
        return False;
    }

    #! Formats a date value as an ISO 8601 UTC string
    static string formatUtcIso8601(date d) {
        return d.format("YYYY-MM-DDTHH:mm:SS") + "Z";
    }

    #! Recursively converts every NT_DATE value in a request body to ISO 8601 UTC strings
    static auto serializeDates(auto v) {
        switch (v.typeCode()) {
            case NT_DATE:
                return KatanaDataProviderBase::formatUtcIso8601(v);
            case NT_HASH:
                return map {$1.key: KatanaDataProviderBase::serializeDates($1.value)},
                    v.pairIterator();
            case NT_LIST:
                return map KatanaDataProviderBase::serializeDates($1), v;
        }
        return v;
    }

    #! Builds list query parameters from a SearchOptions hash
    static hash<auto> buildListParams(*hash<auto> search_options) {
        hash<auto> params = {};
        foreach string k in (("limit", "page")) {
            if (exists search_options{k}) {
                params{k} = search_options{k};
            }
        }
        if (search_options.ids) {
            params.ids = search_options.ids.typeCode() == NT_LIST
                ? join(",", map string($1), search_options.ids)
                : string(search_options.ids);
        }
        foreach string k in (("created_at_min", "created_at_max", "updated_at_min",
                              "updated_at_max")) {
            if (search_options{k}) {
                params{k} = KatanaDataProviderBase::formatUtcIso8601(search_options{k});
            }
        }
        if (exists search_options.include_deleted) {
            params.include_deleted = search_options.include_deleted ? "true" : "false";
        }
        # passthrough additional resource-specific filters
        list<string> reserved_keys = ("ids", "limit", "page", "created_at_min", "created_at_max",
            "updated_at_min", "updated_at_max", "include_deleted", "id");
        foreach string k in (keys search_options) {
            if (!params.hasKey(k) && exists search_options{k}
                && !inlist(k, reserved_keys)) {
                auto v = search_options{k};
                if (v.typeCode() == NT_BOOLEAN) {
                    params{k} = v ? "true" : "false";
                } else if (v.typeCode() == NT_DATE) {
                    params{k} = KatanaDataProviderBase::formatUtcIso8601(v);
                } else if (v.typeCode() == NT_LIST) {
                    params{k} = join(",", map string($1), v);
                } else {
                    params{k} = v;
                }
            }
        }
        return params;
    }

    #! Runs a list/search and returns a record iterator
    /** When @ref use_ids_filter is @ref True, single-record lookups by @ref id_field use the
        @c ids= query parameter on the list endpoint instead of @c GET @c <resource>/<id>;
        this is required for Katana resources that do not expose @c GET on the single-record
        path (customers, suppliers, customer_addresses, sales_order_addresses, stock_adjustments).
    */
    private AbstractDataProviderRecordIterator<hash<auto>> runListSearch(string resource,
            *hash<auto> where_cond, *hash<auto> search_options, string id_field = "id",
            bool use_ids_filter = False) {
        if (exists search_options{id_field}) {
            if (use_ids_filter) {
                list<hash<auto>> records = doGetList(resource,
                    {"ids": string(search_options{id_field})});
                return new DefaultRecordIterator<hash<auto>>(new ListHashIterator(records),
                    where_cond, search_options);
            }
            auto result = doGet(resource + "/" + string(search_options{id_field}));
            hash<auto> record = result.typeCode() == NT_HASH ? result : {};
            return new DefaultRecordIterator<hash<auto>>(new ListHashIterator((record,)),
                where_cond, search_options);
        }
        hash<auto> params = KatanaDataProviderBase::buildListParams(search_options);
        list<hash<auto>> records;
        if (search_options.limit || search_options.page) {
            records = doGetList(resource, params);
        } else {
            records = fetchAllPages(resource, params);
        }
        return new DefaultRecordIterator<hash<auto>>(new ListHashIterator(records),
            where_cond, search_options);
    }

    #! Fetches records updated after the given timestamp
    list<hash<auto>> fetchRecordsUpdatedAfter(string resource, date since,
            *hash<auto> extra_params, int max_records = MaxPageSize) {
        hash<auto> params = (extra_params ?? {}) + {
            "updated_at_min": KatanaDataProviderBase::formatUtcIso8601(since),
            "limit": max_records,
        };
        try {
            return doGetList(resource, params);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN,
                "Failed to fetch %s?updated_at_min=%s: %s: %s",
                resource, KatanaDataProviderBase::formatUtcIso8601(since), ex.err, ex.desc);
            return ();
        }
    }

    #! Returns supported reference data types
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "customers": True,
            "suppliers": True,
            "locations": True,
            "variants": True,
            "products": True,
            "materials": True,
            "tax_rates": True,
            "batch_stocks": True,
            "sales_orders": True,
            "sales_order_rows": True,
            "sales_order_addresses": True,
            "purchase_orders": True,
            "purchase_order_rows": True,
            "manufacturing_orders": True,
            "stock_adjustments": True,
            "customer_addresses": True,
            "webhooks": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type,
            *hash<auto> action_opts) {
        switch (type) {
            case "customers": return getReferenceSimple("customers", "id", "name");
            case "suppliers": return getReferenceSimple("suppliers", "id", "name");
            case "locations": return getReferenceSimple("locations", "id", "name");
            case "variants": return getReferenceSimple("variants", "id", "sku");
            case "products": return getReferenceSimple("products", "id", "name");
            case "materials": return getReferenceSimple("materials", "id", "name");
            case "tax_rates": return getReferenceSimple("tax_rates", "id", "name");
            case "batch_stocks": return getReferenceSimple("batch_stocks", "batch_id",
                "batch_number");
            case "sales_orders": return getReferenceSimple("sales_orders", "id", "order_no");
            case "sales_order_rows": return getReferenceSimple("sales_order_rows", "id", "id");
            case "sales_order_addresses": return getReferenceSimple("sales_order_addresses",
                "id", "id");
            case "purchase_orders": return getReferenceSimple("purchase_orders", "id",
                "order_no");
            case "purchase_order_rows": return getReferenceSimple("purchase_order_rows", "id",
                "id");
            case "manufacturing_orders": return getReferenceSimple("manufacturing_orders", "id",
                "order_no");
            case "stock_adjustments": return getReferenceSimple("stock_adjustments", "id",
                "stock_adjustment_number");
            case "customer_addresses": return getReferenceSimple("customer_addresses", "id",
                "id");
            case "webhooks": return getReferenceSimple("webhooks", "id", "url");
        }
    }

    #! Returns reference data from a simple list endpoint
    private *list<hash<AllowedValueInfo>> getReferenceSimple(string endpoint, string id_field,
            string label_field) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> items = doGetList(endpoint, {"limit": 250});
            foreach hash<auto> item in (items) {
                rv += <AllowedValueInfo>{
                    "display_name": item{label_field} ? string(item{label_field})
                        : sprintf("ID %d", item{id_field}),
                    "value": int(item{id_field}),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch %s reference data: %s: %s",
                endpoint, ex.err, ex.desc);
        }
        return rv;
    }

    #! Converts create request fields to optional fields for update request types
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
        } + (or_nothing_map{$1.value.type.getName()}
            ? {"type": or_nothing_map{$1.value.type.getName()}} : {})},
            fields.pairIterator();
    }

    #! Builds a query string from a hash of parameters
    private string buildQueryString(hash<auto> params) {
        list<string> parts;
        foreach string key in (keys params) {
            auto val = params{key};
            if (exists val) {
                parts += sprintf("%s=%s", encode_url(key, True),
                    encode_url(string(val), True));
            }
        }
        return parts.join("&");
    }
}

} # namespace KatanaDataProvider
