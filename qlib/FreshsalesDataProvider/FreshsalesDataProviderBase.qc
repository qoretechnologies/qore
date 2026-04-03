# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FreshsalesDataProvider module definition

/*  FreshsalesDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FreshsalesDataProvider module
public namespace FreshsalesDataProvider {

#! Base class for Freshsales data providers
public class FreshsalesDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Freshsales data providers
        const BaseConstructorOptions = {
            "freshsalesrestclient": <DataProviderOptionInfo>{
                "display_name": "Freshsales REST Client",
                "short_desc": "The FreshsalesRestClientIo object",
                "type": AbstractDataProviderType::get(
                    new Type("RestClientIo::RestClientIo")),
                "desc": "The RestClientIo object to use for API calls",
            },
            "freshsalesrestclient_options": <DataProviderOptionInfo>{
                "display_name": "Freshsales REST Client Options",
                "short_desc": "Options for creating a FreshsalesRestClientIo",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a FreshsalesRestClientIo object if one is not "
                    "provided directly",
            },
        };
    }

    private {
        #! The REST client object for API calls (async RestClientIo)
        RestClientIo::RestClientIo rest;

        #! The connection object, if applicable
        *FreshsalesRestClient::FreshsalesRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client (async)
    constructor(RestClientIo::RestClientIo rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(FreshsalesRestClient::FreshsalesRestConnection conn) {
        self.conn = conn;
        rest = conn.getAsync(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.freshsalesrestclient) {
            rest = copts.freshsalesrestclient;
        } else {
            hash<auto> client_opts = copts.freshsalesrestclient_options ?? {};
            rest = new FreshsalesRestClient::FreshsalesRestClientIo(client_opts);
        }
    }

    #! Returns the REST client
    RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *FreshsalesRestClient::FreshsalesRestConnection getConnection() {
        return conn;
    }

    #! Makes a GET request to the Freshsales API
    hash<auto> doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a POST request to the Freshsales API
    hash<auto> doPost(string path, *hash<auto> body) {
        return doRestRequest("POST", path, body);
    }

    #! Makes a PUT request to the Freshsales API
    hash<auto> doPut(string path, *hash<auto> body) {
        return doRestRequest("PUT", path, body);
    }

    #! Makes a DELETE request to the Freshsales API
    /** @return an empty hash on success (204 No Content)
    */
    hash<auto> doDelete(string path) {
        return doRestRequest("DELETE", path);
    }

    #! Fetches all pages from a GET endpoint using page-based pagination
    /** @param path the API path
        @param result_key the key in the response containing the result list
        @param params optional query parameters
        @param limit optional maximum number of results to return
        @return a list of all result records
    */
    list<hash<auto>> fetchAllPages(string path, string result_key, *hash<auto> params,
            *int limit) {
        list<hash<auto>> results = ();
        int page = 1;

        while (True) {
            hash<auto> qparams = params ?? {};
            qparams.page = page;

            hash<auto> response = doGet(path, qparams);
            if (response{result_key}) {
                results += response{result_key};
            }
            if (limit && results.size() >= limit) {
                results = results[0..limit-1];
                break;
            }
            # Check if there are more pages via meta
            *int total_pages = response.meta.total_pages;
            if (!total_pages || page >= total_pages) {
                break;
            }
            ++page;
        }
        return results;
    }

    #! Makes a REST request with retry and rate limit handling
    private hash<auto> doRestRequest(string method, string path, auto body = NOTHING) {
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
                    case "PUT":
                        response = rest.restPut(path, body);
                        break;
                    case "DELETE":
                        response = rest.restDel(path);
                        break;
                    default:
                        throw "FRESHSALES-ERROR", sprintf("unsupported HTTP method: %s", method);
                }
                return response.body ?? {};
            } catch (hash<ExceptionInfo> ex) {
                if (retryRequest(ex, \retries)) {
                    continue;
                }
                rethrow;
            }
        }
    }

    #! Checks if a request should be retried
    private bool retryRequest(hash<ExceptionInfo> ex, reference<int> retries) {
        if (retries >= MaxRetries) {
            return False;
        }
        # Retry on certain socket errors
        if (RetrySet{ex.err}) {
            ++retries;
            sleep(pow(2, retries) * 1000ms);
            return True;
        }
        # Retry on rate limit errors (HTTP 429)
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

    #! Returns supported reference data types
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "owners": True,
            "sales_accounts": True,
            "currencies": True,
            "deal_stages": True,
            "deal_pipelines": True,
            "deal_types": True,
            "deal_reasons": True,
            "deal_payment_statuses": True,
            "lead_sources": True,
            "campaigns": True,
            "territories": True,
            "industry_types": True,
            "business_types": True,
            "contact_statuses": True,
            "lifecycle_stages": True,
            "sales_activity_types": True,
            "sales_activity_outcomes": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type,
            *hash<auto> action_opts) {
        switch (type) {
            case "owners": return getReferenceOwners();
            case "sales_accounts": return getReferenceSalesAccounts();
            case "currencies": return getReferenceSelectorItems("currencies", "currencies");
            case "deal_stages": return getReferenceSelectorItems("deal_stages", "deal_stages");
            case "deal_pipelines":
                return getReferenceSelectorItems("deal_pipelines", "deal_pipelines");
            case "deal_types": return getReferenceSelectorItems("deal_types", "deal_types");
            case "deal_reasons": return getReferenceSelectorItems("deal_reasons", "deal_reasons");
            case "deal_payment_statuses":
                return getReferenceSelectorItems("deal_payment_statuses",
                    "deal_payment_statuses");
            case "lead_sources": return getReferenceSelectorItems("lead_sources", "lead_sources");
            case "campaigns": return getReferenceSelectorItems("campaigns", "campaigns");
            case "territories": return getReferenceSelectorItems("territories", "territories");
            case "industry_types":
                return getReferenceSelectorItems("industry_types", "industry_types");
            case "business_types":
                return getReferenceSelectorItems("business_types", "business_types");
            case "contact_statuses":
                return getReferenceSelectorItems("contact_statuses", "contact_statuses");
            case "lifecycle_stages":
                return getReferenceSelectorItems("lifecycle_stages", "lifecycle_stages");
            case "sales_activity_types":
                return getReferenceSelectorItems("sales_activity_types",
                    "sales_activity_types");
            case "sales_activity_outcomes":
                return getReferenceSelectorItems("sales_activity_outcomes",
                    "sales_activity_outcomes");
        }
    }

    #! Returns available owners/users for reference data
    private *list<hash<AllowedValueInfo>> getReferenceOwners() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("selector/owners");
            *list<hash<auto>> users = response.users;
            if (users) {
                foreach hash<auto> user in (users) {
                    string display = user.display_name ?? sprintf("User %d", user.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display,
                        "short_desc": user.email ?? "",
                        "value": int(user.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch owners reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available sales accounts for reference data
    private *list<hash<AllowedValueInfo>> getReferenceSalesAccounts() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> filters = doGet("sales_accounts/filters");
            *list<hash<auto>> filter_list = filters.filters;
            if (filter_list) {
                list<hash<auto>> accounts = fetchAllPages(
                    "sales_accounts/view/" + string(filter_list[0].id), "sales_accounts",
                    NOTHING, 100);
                foreach hash<auto> account in (accounts) {
                    rv += <AllowedValueInfo>{
                        "display_name": account.name ?? sprintf("Account %d", account.id),
                        "value": int(account.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch sales accounts reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns reference data from a selector endpoint
    /** @param selector_path the selector endpoint path (e.g., \c "deal_stages")
        @param response_key the key in the response containing the list of items
        @return a list of allowed values from the selector
    */
    private *list<hash<AllowedValueInfo>> getReferenceSelectorItems(string selector_path,
            string response_key) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("selector/" + selector_path);
            *list<hash<auto>> items = response{response_key};
            if (items) {
                foreach hash<auto> item in (items) {
                    string display = item.name ?? sprintf("ID %d", item.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display,
                        "value": int(item.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch %s reference data: %s: %s",
                selector_path, ex.err, ex.desc);
        }
        return rv;
    }

    #! Converts create request fields to optional fields for update request types
    /** Makes all fields non-required and converts non-optional types to their or-nothing variants.
        Used by update request data types to derive fields from create request data types.
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
                string sval = string(val);
                sval = replace(sval, "%", "%25");
                sval = replace(sval, "&", "%26");
                sval = replace(sval, "=", "%3D");
                sval = replace(sval, "#", "%23");
                sval = replace(sval, "+", "%2B");
                sval = replace(sval, " ", "%20");
                parts += sprintf("%s=%s", key, sval);
            }
        }
        return parts.join("&");
    }
}

} # namespace FreshsalesDataProvider
