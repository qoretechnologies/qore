# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SquarespaceDataProvider module definition

/*  SquarespaceDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquarespaceDataProvider module
public namespace SquarespaceDataProvider {

#! Base class for Squarespace data providers
public class SquarespaceDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Squarespace data providers
        const BaseConstructorOptions = {
            "squarespacerestclient": <DataProviderOptionInfo>{
                "display_name": "Squarespace REST Client",
                "short_desc": "The SquarespaceRestClient object",
                "type": AbstractDataProviderType::get(
                    new Type("SquarespaceRestClient::SquarespaceRestClient")),
                "desc": "The SquarespaceRestClient object to use for API calls",
            },
            "squarespacerestclient_options": <DataProviderOptionInfo>{
                "display_name": "Squarespace REST Client Options",
                "short_desc": "Options for creating a SquarespaceRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a SquarespaceRestClient object if one is not provided "
                    "directly",
            },
        };
    }

    private {
        #! The REST client object for API calls
        SquarespaceRestClient::SquarespaceRestClient rest;

        #! The connection object, if applicable
        *SquarespaceRestClient::SquarespaceRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(SquarespaceRestClient::SquarespaceRestClient rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(SquarespaceRestClient::SquarespaceRestConnection conn) {
        self.conn = conn;
        rest = conn.get(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.squarespacerestclient) {
            rest = copts.squarespacerestclient;
        } else {
            hash<auto> client_opts = copts.squarespacerestclient_options ?? {};
            rest = new SquarespaceRestClient::SquarespaceRestClient(client_opts);
        }
    }

    #! Returns the REST client
    SquarespaceRestClient::SquarespaceRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *SquarespaceRestClient::SquarespaceRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Makes a GET request to the Squarespace API
    /** @param path the API path
        @param params optional query parameters
        @return the response body

        @throw SQUARESPACE-API-ERROR if the response indicates an error
    */
    hash<auto> doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a POST request to the Squarespace API
    /** @param path the API path
        @param body the request body
        @return the response body

        @throw SQUARESPACE-API-ERROR if the response indicates an error
    */
    hash<auto> doPost(string path, *hash<auto> body) {
        return doRestRequest("POST", path, body);
    }

    #! Makes a PUT request to the Squarespace API
    /** @param path the API path
        @param body the request body
        @return the response body

        @throw SQUARESPACE-API-ERROR if the response indicates an error
    */
    hash<auto> doPut(string path, *hash<auto> body) {
        return doRestRequest("PUT", path, body);
    }

    #! Makes a DELETE request to the Squarespace API
    /** @param path the API path
        @return the response body

        @throw SQUARESPACE-API-ERROR if the response indicates an error
    */
    hash<auto> doDelete(string path) {
        return doRestRequest("DELETE", path);
    }

    #! Fetches all pages from a GET endpoint using cursor-based pagination
    /** @param path the API path
        @param result_key the key in the response containing the result list
        @param params optional query parameters (used only on the first request)
        @param limit optional maximum number of results to return
        @return a list of all result records

        @note Squarespace pagination: the cursor is in @c response.pagination.nextPageCursor.
        The cursor parameter cannot be combined with other filter params (modifiedAfter/modifiedBefore),
        so on subsequent pages only the cursor parameter is used.
    */
    list<hash<auto>> fetchAllPages(string path, string result_key, *hash<auto> params, *int limit) {
        list<hash<auto>> results = ();
        *string cursor;
        while (True) {
            hash<auto> qparams;
            if (cursor) {
                # On subsequent pages, use ONLY the cursor parameter
                qparams = {
                    "cursor": cursor,
                };
            } else {
                # On the first request, use all provided params
                qparams = params ?? {};
            }
            hash<auto> response = doGet(path, qparams);
            if (response{result_key}) {
                results += response{result_key};
            }
            if (limit && results.size() >= limit) {
                results = results[0..limit-1];
                break;
            }
            cursor = response.pagination.nextPageCursor;
            if (!cursor) {
                break;
            }
        }
        return results;
    }

    #! Generates a UUID for use as an idempotency key
    /** @return a UUID string
    */
    static string generateIdempotencyKey() {
        return UUID::get();
    }

    #! Formats a date/time value to ISO 8601 UTC format for Squarespace API
    /** @param dt the date/time value
        @return the formatted date string in ISO 8601 UTC format
    */
    static string formatDateTime(date dt) {
        return dt.format("YYYY-MM-DDTHH:mm:ss.xxxZ");
    }

    #! Makes a REST request with retry and rate limit handling
    /** @param method the HTTP method
        @param path the API path
        @param body optional request body
        @return the response body hash

        Squarespace API responses are JSON objects with the result data directly in the body.
        Error responses use HTTP status codes (400, 404, etc.) with body like:
        @code
        {"type": "INVALID_REQUEST_ERROR", "subtype": "...", "message": "..."}
        @endcode

        @throw SQUARESPACE-API-ERROR if the response indicates an error
    */
    private hash<auto> doRestRequest(string method, string path, auto body = NOTHING) {
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
                        throw "SQUARESPACE-ERROR", sprintf("unsupported HTTP method: %s", method);
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

    #! Checks the Squarespace API response for errors
    /** @param response the API response hash

        Squarespace uses HTTP status codes for errors (400, 404, etc.) with body like:
        @code
        {"type": "INVALID_REQUEST_ERROR", "subtype": "INVALID_STRING_VALUE", "message": "..."}
        @endcode

        @throw SQUARESPACE-API-ERROR if the response indicates an error (non-2xx status)
    */
    private checkResponse(hash<auto> response) {
        int status_code = response.status_code ?? 200;
        if (status_code >= 200 && status_code < 300) {
            return;
        }
        string err_msg;
        if (response.body.message.val()) {
            err_msg = response.body.message;
        } else if (response.body.type.val()) {
            err_msg = response.body.type;
            if (response.body.subtype.val()) {
                err_msg += sprintf(": %s", response.body.subtype);
            }
        } else {
            err_msg = sprintf("HTTP %d error", status_code);
        }
        throw "SQUARESPACE-API-ERROR", err_msg, response.body;
    }

    #! Returns information on supported reference data
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "products": True,
            "store_pages": True,
            "orders": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "products": return getReferenceProducts();
            case "store_pages": return getReferenceStorePages();
            case "orders": return getReferenceOrders();
        }
    }

    #! Returns available products for reference data
    private *list<hash<AllowedValueInfo>> getReferenceProducts() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet(ProductsApiVersion + "/commerce/products");
            if (response.products) {
                foreach hash<auto> product in (response.products) {
                    string display_name = product.name ?? sprintf("Product %s", product.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": "",
                        "desc": sprintf("%s (ID: %s)", display_name, product.id),
                        "value": product.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available store pages for reference data
    private *list<hash<AllowedValueInfo>> getReferenceStorePages() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet(OrdersApiVersion + "/commerce/store_pages");
            if (response.storePages) {
                foreach hash<auto> page in (response.storePages) {
                    string display_name = page.title ?? sprintf("Store Page %s", page.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": "",
                        "desc": sprintf("%s (ID: %s)", display_name, page.id),
                        "value": page.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available orders for reference data
    private *list<hash<AllowedValueInfo>> getReferenceOrders() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet(OrdersApiVersion + "/commerce/orders");
            if (response.result) {
                foreach hash<auto> order in (response.result) {
                    string display_name = sprintf("Order #%s", order.orderNumber ?? order.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": "",
                        "desc": sprintf("%s (ID: %s)", display_name, order.id),
                        "value": order.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }
}

} # namespace SquarespaceDataProvider
