# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceDataProviderBase.qc base class for BigCommerce data providers

/*  BigCommerceDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BigCommerceDataProvider module
public namespace BigCommerceDataProvider {

#! Base class for BigCommerce data providers
public class BigCommerceDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for BigCommerce data providers
        const BaseConstructorOptions = {
            "bigcommercerestclient": <DataProviderOptionInfo>{
                "display_name": "BigCommerce REST Client",
                "short_desc": "The BigCommerceRestClient object",
                "type": AbstractDataProviderType::get(
                    new Type("BigCommerceRestClient::BigCommerceRestClient")),
                "desc": "The BigCommerceRestClient object to use for API calls",
            },
            "bigcommercerestclient_options": <DataProviderOptionInfo>{
                "display_name": "BigCommerce REST Client Options",
                "short_desc": "Options for creating a BigCommerceRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a BigCommerceRestClient object if one is not "
                    "provided directly",
            },
        };
    }

    private {
        #! The REST client object for API calls
        BigCommerceRestClient::BigCommerceRestClient rest;

        #! The connection object, if applicable
        *BigCommerceRestClient::BigCommerceRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(BigCommerceRestClient::BigCommerceRestClient rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(BigCommerceRestClient::BigCommerceRestConnection conn) {
        self.conn = conn;
        rest = conn.get(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.bigcommercerestclient) {
            rest = copts.bigcommercerestclient;
        } else {
            hash<auto> client_opts = copts.bigcommercerestclient_options ?? {};
            rest = new BigCommerceRestClient::BigCommerceRestClient(client_opts);
        }
    }

    #! Returns the REST client
    BigCommerceRestClient::BigCommerceRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *BigCommerceRestClient::BigCommerceRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    # ========== V3 API Helpers ==========

    #! Makes a GET request to the BigCommerce V3 API
    /** @param path the API path (relative to /v3, without leading slash)
        @param params optional query parameters
        @return the full response body including "data" and "meta" keys
    */
    hash<auto> doGetV3(string path, *hash<auto> params) {
        string url = "v3/" + path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a POST request to the BigCommerce V3 API
    /** @param path the API path (relative to /v3, without leading slash)
        @param body the request body
        @return the full response body including "data" and "meta" keys
    */
    hash<auto> doPostV3(string path, auto body) {
        return doRestRequest("POST", "v3/" + path, body);
    }

    #! Makes a PUT request to the BigCommerce V3 API
    /** @param path the API path (relative to /v3, without leading slash)
        @param body the request body
        @return the full response body including "data" and "meta" keys
    */
    hash<auto> doPutV3(string path, auto body) {
        return doRestRequest("PUT", "v3/" + path, body);
    }

    #! Makes a DELETE request to the BigCommerce V3 API
    /** @param path the API path (relative to /v3, without leading slash)
        @param params optional query parameters (BigCommerce uses ?id:in= for batch deletes)
        @return the response body
    */
    hash<auto> doDeleteV3(string path, *hash<auto> params) {
        string url = "v3/" + path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("DELETE", url);
    }

    # ========== V2 API Helpers ==========

    #! Makes a GET request to the BigCommerce V2 API
    /** @param path the API path (relative to /v2, without leading slash)
        @param params optional query parameters
        @return the response body (V2 returns flat arrays or objects)
    */
    auto doGetV2(string path, *hash<auto> params) {
        string url = "v2/" + path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a POST request to the BigCommerce V2 API
    /** @param path the API path (relative to /v2, without leading slash)
        @param body the request body
        @return the response body
    */
    auto doPostV2(string path, *hash<auto> body) {
        return doRestRequest("POST", "v2/" + path, body);
    }

    #! Makes a PUT request to the BigCommerce V2 API
    /** @param path the API path (relative to /v2, without leading slash)
        @param body the request body
        @return the response body
    */
    auto doPutV2(string path, *hash<auto> body) {
        return doRestRequest("PUT", "v2/" + path, body);
    }

    #! Makes a DELETE request to the BigCommerce V2 API
    /** @param path the API path (relative to /v2, without leading slash)
        @return the response body
    */
    auto doDeleteV2(string path) {
        return doRestRequest("DELETE", "v2/" + path);
    }

    # ========== V3 Batch Helpers ==========

    #! Creates a single record via a V3 batch endpoint (wraps in array, extracts first result)
    /** BigCommerce V3 customers and addresses endpoints accept arrays and return
        {"data": [...]}. This helper wraps a single item in an array and extracts the first
        result.

        @param path the API path (relative to /v3, without leading slash)
        @param body the single record to create
        @return the created record
    */
    hash<auto> doCreateSingleV3(string path, hash<auto> body) {
        hash<auto> response = doPostV3(path, (body,));
        if (response.data.typeCode() == NT_LIST && response.data.size() > 0) {
            return response.data[0];
        }
        return response.data ?? {};
    }

    #! Updates a single record via a V3 batch endpoint (wraps in array, extracts first result)
    /** @param path the API path (relative to /v3, without leading slash)
        @param body the record to update (must include "id")
        @return the updated record
    */
    hash<auto> doUpdateSingleV3(string path, hash<auto> body) {
        hash<auto> response = doPutV3(path, (body,));
        if (response.data.typeCode() == NT_LIST && response.data.size() > 0) {
            return response.data[0];
        }
        return response.data ?? {};
    }

    # ========== Pagination ==========

    #! Fetches all pages from a V3 GET endpoint using page/limit pagination
    /** BigCommerce V3 responses have: {"data": [...], "meta": {"pagination": {...}}}

        @param path the API path (relative to /v3, without leading slash)
        @param params optional query parameters
        @param limit optional maximum number of results to return
        @return a list of all result records
    */
    list<hash<auto>> fetchAllPagesV3(string path, *hash<auto> params, *int limit) {
        list<hash<auto>> results = ();
        int page = 1;
        while (True) {
            hash<auto> qparams = params ?? {};
            qparams.page = page;
            qparams.limit = MaxPageSize;
            hash<auto> response = doGetV3(path, qparams);
            auto data = response.data;
            if (data.typeCode() == NT_LIST) {
                results += data;
            } else if (data.typeCode() == NT_HASH) {
                results += data;
            }
            if (limit && results.size() >= limit) {
                results = results[0..limit-1];
                break;
            }
            # Check pagination meta
            *hash<auto> pagination = response.meta.pagination;
            if (!pagination || page >= pagination.total_pages) {
                break;
            }
            ++page;
        }
        return results;
    }

    #! Fetches all pages from a V2 GET endpoint
    /** V2 returns flat arrays with no pagination meta wrapper

        @param path the API path (relative to /v2, without leading slash)
        @param params optional query parameters
        @param limit optional maximum number of results to return
        @return a list of all result records
    */
    list<hash<auto>> fetchAllPagesV2(string path, *hash<auto> params, *int limit) {
        list<hash<auto>> results = ();
        int page = 1;
        while (True) {
            hash<auto> qparams = params ?? {};
            qparams.page = page;
            qparams.limit = MaxPageSize;
            auto response = doGetV2(path, qparams);
            # V2 returns the list directly in the body
            if (response.typeCode() == NT_LIST) {
                results += response;
            } else {
                # No more results or unexpected response
                break;
            }
            if (limit && results.size() >= limit) {
                results = results[0..limit-1];
                break;
            }
            # If we got fewer results than the page size, we're done
            if (response.size() < MaxPageSize) {
                break;
            }
            ++page;
        }
        return results;
    }

    # ========== Error Handling ==========

    #! Makes a REST request with retry and rate limit handling
    /** @param method the HTTP method
        @param path the full API path (e.g., "v3/customers" or "v2/orders")
        @param body optional request body
        @return the response body hash

        BigCommerce V3 error responses:
        @code
        {"status": 422, "title": "...", "type": "...", "errors": {"field": "message"}}
        @endcode

        @throw BIGCOMMERCE-API-ERROR if the response indicates an error
    */
    private auto doRestRequest(string method, string path, auto body = NOTHING) {
        int retries = 0;
        while (True) {
            try {
                hash<auto> info;
                hash<auto> response;
                switch (method) {
                    case "GET":
                        response = rest.get(path, NOTHING, \info);
                        break;
                    case "POST":
                        response = rest.post(path, body, \info);
                        break;
                    case "PUT":
                        response = rest.put(path, body, \info);
                        break;
                    case "DELETE":
                        response = rest.del(path, NOTHING, \info);
                        break;
                    default:
                        throw "BIGCOMMERCE-ERROR",
                            sprintf("unsupported HTTP method: %s", method);
                }

                checkResponse(response);
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
    /** @param ex the exception info
        @param retries reference to retry counter
        @return True if the request should be retried
    */
    private bool retryRequest(hash<ExceptionInfo> ex, reference<int> retries) {
        if (retries >= MaxRetries) {
            return False;
        }
        # Retry on certain socket errors
        if (RetrySet{ex.err}) {
            ++retries;
            # Exponential backoff
            sleep(pow(2, retries) * 1000ms);
            return True;
        }
        # Retry on rate limit errors (HTTP 429)
        if (ex.err == "REST-RESPONSE-ERROR" && ex.arg.status_code == 429) {
            ++retries;
            # BigCommerce uses X-Rate-Limit-Time-Reset-Ms header (milliseconds)
            *string reset_ms = ex.arg.headers."x-rate-limit-time-reset-ms";
            int wait_ms = reset_ms.val() ? reset_ms.toInt() : 5000;
            if (wait_ms < 100 || wait_ms > 60000) {
                wait_ms = 5000;
            }
            sleep(wait_ms * 1ms);
            return True;
        }
        return False;
    }

    #! Builds a query string from a hash of parameters
    /** @param params the hash of query parameters
        @return the URL-encoded query string
    */
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

    #! Checks the BigCommerce API response for errors
    /** @param response the API response hash

        BigCommerce V3 returns errors as:
        @code
        {"status": 422, "title": "...", "type": "...", "errors": {"field": "message"}}
        @endcode

        @throw BIGCOMMERCE-API-ERROR if the response contains errors
    */
    private checkResponse(hash<auto> response) {
        if (response.body.status.typeCode() == NT_INT && response.body.status >= 400) {
            string err_msg = response.body.title ?? "BigCommerce API Error";
            if (response.body.errors) {
                if (response.body.errors.typeCode() == NT_HASH) {
                    foreach string key in (keys response.body.errors) {
                        err_msg += sprintf("; %s: %s", key, response.body.errors{key});
                    }
                } else if (response.body.errors.typeCode() == NT_LIST) {
                    foreach auto error in (response.body.errors) {
                        err_msg += sprintf("; %s", string(error));
                    }
                }
            }
            throw "BIGCOMMERCE-API-ERROR", err_msg, response.body;
        }
    }

    # ========== Reference Data ==========

    #! Returns information on supported reference data
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "blog_posts": True,
            "brands": True,
            "customer_addresses": True,
            "customers": True,
            "orders": True,
            "products": True,
            "customer_groups": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type,
            *hash<auto> action_opts) {
        switch (type) {
            case "blog_posts": return getReferenceBlogPosts();
            case "brands": return getReferenceBrands();
            case "customer_addresses": return getReferenceCustomerAddresses();
            case "customers": return getReferenceCustomers();
            case "orders": return getReferenceOrders();
            case "products": return getReferenceProducts();
            case "customer_groups": return getReferenceCustomerGroups();
        }
    }

    #! Returns available blog posts for reference data
    private *list<hash<AllowedValueInfo>> getReferenceBlogPosts() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGetV2("blog/posts", {"limit": DefaultPageSize});
            if (response.typeCode() == NT_LIST) {
                foreach hash<auto> post in (response) {
                    string display_name = post.title
                        ?? sprintf("Post %d", post.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": post.is_published ? "Published" : "Draft",
                        "desc": sprintf("%s (ID: %d)", display_name, post.id),
                        "value": post.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available brands for reference data
    private *list<hash<AllowedValueInfo>> getReferenceBrands() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGetV3("catalog/brands", {"limit": DefaultPageSize});
            if (response.data.typeCode() == NT_LIST) {
                foreach hash<auto> brand in (response.data) {
                    string display_name = brand.name
                        ?? sprintf("Brand %d", brand.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": "",
                        "desc": sprintf("%s (ID: %d)", display_name, brand.id),
                        "value": brand.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available customers for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCustomers() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGetV3("customers", {"limit": DefaultPageSize});
            if (response.data.typeCode() == NT_LIST) {
                foreach hash<auto> customer in (response.data) {
                    string display_name = trim(sprintf("%s %s",
                        customer.first_name ?? "", customer.last_name ?? ""));
                    if (!display_name.val()) {
                        display_name = customer.email
                            ?? sprintf("Customer %d", customer.id);
                    }
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": customer.email ?? "",
                        "desc": sprintf("%s (ID: %d)", display_name, customer.id),
                        "value": customer.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available customer addresses for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCustomerAddresses() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGetV3("customers/addresses",
                {"limit": DefaultPageSize});
            if (response.data.typeCode() == NT_LIST) {
                foreach hash<auto> addr in (response.data) {
                    string display_name = trim(sprintf("%s %s, %s",
                        addr.first_name ?? "", addr.last_name ?? "",
                        addr.city ?? ""));
                    if (!display_name.val()) {
                        display_name = sprintf("Address %d", addr.id);
                    }
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": addr.address1 ?? "",
                        "desc": sprintf("%s (ID: %d)", display_name, addr.id),
                        "value": addr.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available orders for reference data
    private *list<hash<AllowedValueInfo>> getReferenceOrders() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGetV2("orders", {"limit": DefaultPageSize,
                "sort": "date_created:desc"});
            if (response.typeCode() == NT_LIST) {
                foreach hash<auto> order in (response) {
                    string display_name = sprintf("Order #%d", order.id);
                    if (order.status.val()) {
                        display_name += sprintf(" (%s)", order.status);
                    }
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": order.total_inc_tax
                            ? sprintf("%s %s", order.currency_code ?? "",
                                order.total_inc_tax)
                            : "",
                        "desc": sprintf("Order #%d (ID: %d)", order.id, order.id),
                        "value": order.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available products for reference data
    private *list<hash<AllowedValueInfo>> getReferenceProducts() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGetV3("catalog/products", {"limit": DefaultPageSize});
            if (response.data.typeCode() == NT_LIST) {
                foreach hash<auto> product in (response.data) {
                    string display_name = product.name
                        ?? sprintf("Product %d", product.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": product.sku ?? "",
                        "desc": sprintf("%s (ID: %d)", display_name, product.id),
                        "value": product.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available customer groups for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCustomerGroups() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGetV2("customer_groups");
            if (response.typeCode() == NT_LIST) {
                foreach hash<auto> group in (response) {
                    rv += <AllowedValueInfo>{
                        "display_name": group.name
                            ?? sprintf("Group %d", group.id),
                        "short_desc": "",
                        "desc": sprintf("%s (ID: %d)",
                            group.name ?? "Group", group.id),
                        "value": group.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    # ========== Date Formatting ==========

    #! Formats a date for BigCommerce V3 API parameters (RFC 3339 / ISO 8601)
    static string formatDateTimeV3(date dt) {
        return dt.format("YYYY-MM-DDTHH:mm:ssZ");
    }

    #! Formats a date for BigCommerce V2 API parameters (RFC 2822)
    static string formatDateTimeV2(date dt) {
        return dt.format("Dy, DD Mon YYYY HH:mm:ss +0000");
    }
}

} # namespace BigCommerceDataProvider
