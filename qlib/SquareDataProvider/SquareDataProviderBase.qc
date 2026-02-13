# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SquareDataProvider module definition

/*  SquareDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquareDataProvider module
public namespace SquareDataProvider {

#! Base class for Square data providers
public class SquareDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Square data providers
        const BaseConstructorOptions = {
            "squarerestclient": <DataProviderOptionInfo>{
                "display_name": "Square REST Client",
                "short_desc": "The SquareRestClient object",
                "type": AbstractDataProviderType::get(
                    new Type("SquareRestClient::SquareRestClient")),
                "desc": "The SquareRestClient object to use for API calls",
            },
            "squarerestclient_options": <DataProviderOptionInfo>{
                "display_name": "Square REST Client Options",
                "short_desc": "Options for creating a SquareRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a SquareRestClient object if one is not provided "
                    "directly",
            },
        };
    }

    private {
        #! The REST client object for API calls
        SquareRestClient::SquareRestClient rest;

        #! The connection object, if applicable
        *SquareRestClient::SquareRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(SquareRestClient::SquareRestClient rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(SquareRestClient::SquareRestConnection conn) {
        self.conn = conn;
        rest = conn.get(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.squarerestclient) {
            rest = copts.squarerestclient;
        } else {
            hash<auto> client_opts = copts.squarerestclient_options ?? {};
            rest = new SquareRestClient::SquareRestClient(client_opts);
        }
    }

    #! Returns the REST client
    SquareRestClient::SquareRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *SquareRestClient::SquareRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Makes a GET request to the Square API
    /** @param path the API path (relative to /v2)
        @param params optional query parameters
        @return the response body

        @throw SQUARE-API-ERROR if the response indicates an error
    */
    hash<auto> doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a POST request to the Square API
    /** @param path the API path (relative to /v2)
        @param body the request body
        @return the response body

        @throw SQUARE-API-ERROR if the response indicates an error
    */
    hash<auto> doPost(string path, *hash<auto> body) {
        return doRestRequest("POST", path, body);
    }

    #! Makes a PUT request to the Square API
    /** @param path the API path (relative to /v2)
        @param body the request body
        @return the response body

        @throw SQUARE-API-ERROR if the response indicates an error
    */
    hash<auto> doPut(string path, *hash<auto> body) {
        return doRestRequest("PUT", path, body);
    }

    #! Makes a DELETE request to the Square API
    /** @param path the API path (relative to /v2)
        @return the response body

        @throw SQUARE-API-ERROR if the response indicates an error
    */
    hash<auto> doDelete(string path) {
        return doRestRequest("DELETE", path);
    }

    #! Fetches all pages from a GET endpoint using cursor-based pagination
    /** @param path the API path
        @param result_key the key in the response containing the result list
        @param params optional query parameters
        @param limit optional maximum number of results to return
        @return a list of all result records
    */
    list<hash<auto>> fetchAllPages(string path, string result_key, *hash<auto> params, *int limit) {
        list<hash<auto>> results = ();
        *string cursor;
        while (True) {
            hash<auto> qparams = params ?? {};
            if (cursor) {
                qparams.cursor = cursor;
            }
            hash<auto> response = doGet(path, qparams);
            if (response{result_key}) {
                results += response{result_key};
            }
            if (limit && results.size() >= limit) {
                results = results[0..limit-1];
                break;
            }
            cursor = response.cursor;
            if (!cursor) {
                break;
            }
        }
        return results;
    }

    #! Fetches all pages from a POST search endpoint using cursor-based pagination
    /** @param path the API path
        @param result_key the key in the response containing the result list
        @param body the request body (cursor will be added automatically)
        @param limit optional maximum number of results to return
        @return a list of all result records
    */
    list<hash<auto>> fetchAllPagesPost(string path, string result_key, hash<auto> body, *int limit) {
        list<hash<auto>> results = ();
        *string cursor;
        while (True) {
            hash<auto> req_body = body;
            if (cursor) {
                req_body.cursor = cursor;
            }
            hash<auto> response = doPost(path, req_body);
            if (response{result_key}) {
                results += response{result_key};
            }
            if (limit && results.size() >= limit) {
                results = results[0..limit-1];
                break;
            }
            cursor = response.cursor;
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

    #! Makes a REST request with retry and rate limit handling
    /** @param method the HTTP method
        @param path the API path
        @param body optional request body
        @return the response body hash

        Square API responses are flat JSON objects with the result data directly in the body.
        Error responses contain an "errors" array at the top level:
        @code
        {"errors": [{"category": "...", "code": "...", "detail": "..."}]}
        @endcode

        @throw SQUARE-API-ERROR if the response indicates an error
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
                        throw "SQUARE-ERROR", sprintf("unsupported HTTP method: %s", method);
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

    #! Checks the Square API response for errors
    /** @param response the API response hash

        Square returns errors as:
        @code
        {"errors": [{"category": "INVALID_REQUEST_ERROR", "code": "NOT_FOUND", "detail": "..."}]}
        @endcode

        @throw SQUARE-API-ERROR if the response contains errors
    */
    private checkResponse(hash<auto> response) {
        if (response.body.errors) {
            list<auto> errors = response.body.errors;
            if (errors.size() > 0) {
                string err_msg;
                foreach hash<auto> error in (errors) {
                    if (err_msg.val()) {
                        err_msg += "; ";
                    }
                    err_msg += error.detail ?? error.code ?? "Unknown error";
                    if (error.field.val()) {
                        err_msg += sprintf(" (field: %s)", error.field);
                    }
                }
                throw "SQUARE-API-ERROR", err_msg, response.body;
            }
        }
    }

    #! Fetches all location IDs for the current merchant
    /** @return a list of location ID strings
    */
    list<string> getLocationIds() {
        hash<auto> response = doGet("locations");
        list<string> rv = ();
        if (response.locations) {
            foreach hash<auto> loc in (response.locations) {
                rv += loc.id;
            }
        }
        return rv;
    }

    #! Fetches the first location ID for the current merchant
    /** @return the first location ID, or throws if none found

        @throw SQUARE-ERROR no locations found
    */
    string getFirstLocationId() {
        list<string> locs = getLocationIds();
        if (!locs) {
            throw "SQUARE-ERROR", "no locations found for this merchant";
        }
        return locs[0];
    }

    #! Returns information on supported reference data
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "bookings": True,
            "customers": True,
            "customer_groups": True,
            "invoices": True,
            "locations": True,
            "orders": True,
            "payments": True,
            "refunds": True,
            "team_members": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "bookings": return getReferenceBookings();
            case "customers": return getReferenceCustomers();
            case "customer_groups": return getReferenceCustomerGroups();
            case "invoices": return getReferenceInvoices();
            case "locations": return getReferenceLocations();
            case "orders": return getReferenceOrders();
            case "payments": return getReferencePayments();
            case "refunds": return getReferenceRefunds();
            case "team_members": return getReferenceTeamMembers();
        }
    }

    #! Returns available locations for reference data
    private *list<hash<AllowedValueInfo>> getReferenceLocations() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("locations");
            if (response.locations) {
                foreach hash<auto> loc in (response.locations) {
                    string display_name = loc.name ?? sprintf("Location %s", loc.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": loc.status ?? "",
                        "desc": sprintf("%s (ID: %s)", display_name, loc.id),
                        "value": loc.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available customers for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCustomers() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("customers");
            if (response.customers) {
                foreach hash<auto> customer in (response.customers) {
                    string display_name;
                    if (customer.company_name.val()) {
                        display_name = customer.company_name;
                    } else {
                        display_name = trim(sprintf("%s %s",
                            customer.given_name ?? "", customer.family_name ?? ""));
                    }
                    if (!display_name.val()) {
                        display_name = sprintf("Customer %s", customer.id);
                    }
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": customer.email_address ?? "",
                        "desc": sprintf("%s (ID: %s)", display_name, customer.id),
                        "value": customer.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }
    #! Returns available customer groups for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCustomerGroups() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("customers/groups");
            if (response.groups) {
                foreach hash<auto> group in (response.groups) {
                    rv += <AllowedValueInfo>{
                        "display_name": group.name ?? sprintf("Group %s", group.id),
                        "short_desc": "",
                        "desc": sprintf("%s (ID: %s)", group.name ?? "Group", group.id),
                        "value": group.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available payments for reference data
    private *list<hash<AllowedValueInfo>> getReferencePayments() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("payments");
            if (response.payments) {
                foreach hash<auto> payment in (response.payments) {
                    string display_name = sprintf("%s %s",
                        payment.amount_money.currency ?? "USD",
                        payment.amount_money.amount ?? "0");
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": payment.status ?? "",
                        "desc": sprintf("%s - %s (ID: %s)", display_name,
                            payment.status ?? "", payment.id),
                        "value": payment.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available refunds for reference data
    private *list<hash<AllowedValueInfo>> getReferenceRefunds() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("refunds");
            if (response.refunds) {
                foreach hash<auto> refund in (response.refunds) {
                    string display_name = sprintf("%s %s",
                        refund.amount_money.currency ?? "USD",
                        refund.amount_money.amount ?? "0");
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": refund.status ?? "",
                        "desc": sprintf("%s - %s (ID: %s)", display_name,
                            refund.status ?? "", refund.id),
                        "value": refund.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available invoices for reference data
    private *list<hash<AllowedValueInfo>> getReferenceInvoices() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            string location_id = getFirstLocationId();
            hash<auto> response = doGet("invoices", {"location_id": location_id});
            if (response.invoices) {
                foreach hash<auto> invoice in (response.invoices) {
                    string display_name = invoice.invoice_number
                        ?? sprintf("Invoice %s", invoice.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": invoice.status ?? "",
                        "desc": sprintf("%s - %s (ID: %s)", display_name,
                            invoice.status ?? "", invoice.id),
                        "value": invoice.id,
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
            list<string> location_ids = getLocationIds();
            if (!location_ids) {
                return rv;
            }
            hash<auto> body = {
                "location_ids": location_ids,
                "limit": DefaultPageSize,
            };
            hash<auto> response = doPost("orders/search", body);
            if (response.orders) {
                foreach hash<auto> order in (response.orders) {
                    string display_name = sprintf("Order %s", order.id);
                    if (order.total_money) {
                        display_name = sprintf("%s %s - %s",
                            order.total_money.currency ?? "USD",
                            order.total_money.amount ?? "0",
                            order.state ?? "");
                    }
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": order.state ?? "",
                        "desc": sprintf("%s (ID: %s)", display_name, order.id),
                        "value": order.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available bookings for reference data
    private *list<hash<AllowedValueInfo>> getReferenceBookings() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("bookings", {"limit": DefaultPageSize});
            if (response.bookings) {
                foreach hash<auto> booking in (response.bookings) {
                    string display_name = sprintf("Booking %s", booking.id);
                    if (booking.start_at) {
                        display_name = sprintf("%s - %s", booking.start_at, booking.status ?? "");
                    }
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": booking.status ?? "",
                        "desc": sprintf("%s (ID: %s)", display_name, booking.id),
                        "value": booking.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available team members for reference data
    private *list<hash<AllowedValueInfo>> getReferenceTeamMembers() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doPost("team-members/search", {});
            if (response.team_members) {
                foreach hash<auto> member in (response.team_members) {
                    string display_name = trim(sprintf("%s %s",
                        member.given_name ?? "", member.family_name ?? ""));
                    if (!display_name.val()) {
                        display_name = sprintf("Member %s", member.id);
                    }
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": member.status ?? "",
                        "desc": sprintf("%s (ID: %s)", display_name, member.id),
                        "value": member.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Formats a date for Square API parameters (RFC 3339)
    static string formatDateTimeSquare(date dt) {
        return dt.format("YYYY-MM-DDTHH:mm:ss.xxxZ");
    }

    #! Formats a date for Square API date-only parameters (YYYY-MM-DD)
    static string formatDateSquare(date dt) {
        return dt.format("YYYY-MM-DD");
    }
}

} # namespace SquareDataProvider
