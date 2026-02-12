# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FreshBooksDataProvider module definition

/*  FreshBooksDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FreshBooksDataProvider module
public namespace FreshBooksDataProvider {

#! Base class for FreshBooks data providers
public class FreshBooksDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Reusable account_id field definition for data types
        const AccountIdField = {
            "account_id": {
                "type": StringType,
                "display_name": "Account ID",
                "short_desc": "FreshBooks account ID",
                "desc": "The FreshBooks account ID for accounting endpoints",
                "required": True,
            },
        };

        #! Reusable business_id field definition for data types
        const BusinessIdField = {
            "business_id": {
                "type": StringType,
                "display_name": "Business ID",
                "short_desc": "FreshBooks business ID",
                "desc": "The FreshBooks business ID for time tracking endpoints",
                "required": True,
            },
        };

        #! Reusable account_id search option
        const AccountIdSearchOption = {
            "account_id": <DataProviderOptionInfo>{
                "display_name": "Account ID",
                "short_desc": "FreshBooks account ID",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The FreshBooks account ID for accounting endpoints",
                "required": True,
            },
        };

        #! Reusable business_id search option
        const BusinessIdSearchOption = {
            "business_id": <DataProviderOptionInfo>{
                "display_name": "Business ID",
                "short_desc": "FreshBooks business ID",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The FreshBooks business ID for time tracking endpoints",
                "required": True,
            },
        };

        #! Base constructor options for FreshBooks data providers
        const BaseConstructorOptions = {
            "freshbooksrestclient": <DataProviderOptionInfo>{
                "display_name": "FreshBooks REST Client",
                "short_desc": "The FreshBooksRestClient object",
                "type": AbstractDataProviderType::get(
                    new Type("FreshBooksRestClient::FreshBooksRestClient")),
                "desc": "The FreshBooksRestClient object to use for API calls",
            },
            "freshbooksrestclient_options": <DataProviderOptionInfo>{
                "display_name": "FreshBooks REST Client Options",
                "short_desc": "Options for creating a FreshBooksRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a FreshBooksRestClient object if one is not provided "
                    "directly",
            },
        };
    }

    private {
        #! The REST client object for API calls
        FreshBooksRestClient::FreshBooksRestClient rest;

        #! The connection object, if applicable
        *FreshBooksRestClient::FreshBooksRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(FreshBooksRestClient::FreshBooksRestClient rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(FreshBooksRestClient::FreshBooksRestConnection conn) {
        self.conn = conn;
        rest = conn.get(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.freshbooksrestclient) {
            rest = copts.freshbooksrestclient;
        } else {
            hash<auto> client_opts = copts.freshbooksrestclient_options ?? {};
            rest = new FreshBooksRestClient::FreshBooksRestClient(client_opts);
        }
    }

    #! Returns the REST client
    FreshBooksRestClient::FreshBooksRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *FreshBooksRestClient::FreshBooksRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Returns the accounting API path prefix for the configured account
    /** @param path the path suffix (e.g., "/users/clients")
        @return the full path (e.g., "/accounting/account/ABC123/users/clients")
    */
    string accountPath(string path) {
        return sprintf("/accounting/account/%s%s", rest.getAccountId(), path);
    }

    #! Returns the time tracking API path prefix for the configured business
    /** @param path the path suffix (e.g., "/services")
        @return the full path (e.g., "/timetracking/business/123456/services")
    */
    string businessPath(string path) {
        return sprintf("/timetracking/business/%s%s", rest.getBusinessId(), path);
    }

    #! Returns the events API path prefix for the configured account
    /** @param path the path suffix (e.g., "/events/callbacks")
        @return the full path (e.g., "/events/account/ABC123/events/callbacks")
    */
    string eventsPath(string path) {
        return sprintf("/events/account/%s%s", rest.getAccountId(), path);
    }

    #! Makes a GET request to the FreshBooks API
    /** @param path the API path
        @param params optional query parameters
        @return the unwrapped response result

        @throw FRESHBOOKS-API-ERROR if the response indicates an error
    */
    hash<auto> doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a POST request to the FreshBooks API
    /** @param path the API path
        @param body the request body
        @return the unwrapped response result

        @throw FRESHBOOKS-API-ERROR if the response indicates an error
    */
    hash<auto> doPost(string path, *hash<auto> body) {
        return doRestRequest("POST", path, body);
    }

    #! Makes a PUT request to the FreshBooks API
    /** @param path the API path
        @param body the request body
        @return the unwrapped response result

        @throw FRESHBOOKS-API-ERROR if the response indicates an error
    */
    hash<auto> doPut(string path, *hash<auto> body) {
        return doRestRequest("PUT", path, body);
    }

    #! Makes a DELETE request to the FreshBooks API
    /** @param path the API path
        @return the unwrapped response result

        @throw FRESHBOOKS-API-ERROR if the response indicates an error
    */
    hash<auto> doDelete(string path) {
        return doRestRequest("DELETE", path);
    }

    #! Makes a REST request with retry and rate limit handling
    /** @param method the HTTP method
        @param path the API path
        @param body optional request body
        @return the unwrapped response result from the FreshBooks response envelope

        The FreshBooks response envelope for accounting/timetracking endpoints is:
        @code
        {"response": {"result": { ... }}}
        @endcode

        This method unwraps the envelope and returns the contents of \c result.
        For endpoints that do not use the envelope (e.g., /auth/api/v1/...), the
        raw response body is returned.

        @throw FRESHBOOKS-API-ERROR if the response indicates an error
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
                        throw "FRESHBOOKS-ERROR", sprintf("unsupported HTTP method: %s", method);
                }

                # Check for rate limiting (headers are lowercase in RestClient)
                if (info."response-headers"."x-ratelimit-remaining" == "0") {
                    *string reset = info."response-headers"."retry-after";
                    if (reset.val()) {
                        int reset_secs = reset.toInt();
                        if (reset_secs > 0 && reset_secs <= 60) {
                            # Wait for rate limit reset (up to 60 seconds)
                            sleep(reset_secs * 1000ms);
                        }
                    }
                }

                checkResponse(response);

                # Unwrap the FreshBooks response envelope
                # Accounting/timetracking endpoints return:
                #   {"response": {"result": { ... }}}
                # Auth endpoints return:
                #   {"response": { ... }} (no "result" wrapper)
                if (response.body.response.result) {
                    return response.body.response.result;
                } else if (response.body.response) {
                    return response.body.response;
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
            # Wait before retrying rate-limited requests
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

    #! Checks the FreshBooks API response for errors
    /** @param response the API response hash

        The FreshBooks API returns errors in the response envelope:
        @code
        {"response": {"errors": [{"errno": 1012, "field": "...", "message": "...", "object": "..."}]}}
        @endcode

        @throw FRESHBOOKS-API-ERROR if the response indicates an error
    */
    private checkResponse(hash<auto> response) {
        # Check for errors in the FreshBooks response envelope
        if (response.body.response.errors) {
            list<auto> errors = response.body.response.errors;
            if (errors.size() > 0) {
                string err_msg;
                foreach hash<auto> error in (errors) {
                    if (err_msg.val()) {
                        err_msg += "; ";
                    }
                    err_msg += error.message ?? "Unknown error";
                    if (error.field.val()) {
                        err_msg += sprintf(" (field: %s)", error.field);
                    }
                }
                throw "FRESHBOOKS-API-ERROR", err_msg, response.body.response;
            }
        }
    }

    #! Helper method to get client options from constructor options
    static *hash<auto> getClientOptions(*hash<auto> copts) {
        if (!copts) {
            return;
        }
        hash<auto> rv;
        foreach string key in (keys copts.freshbooksrestclient_options) {
            rv{key} = copts.freshbooksrestclient_options{key};
        }
        return rv;
    }

    #! Extracts account_id from a request hash, sets it on the REST client, and returns the cleaned request
    private hash<auto> setAccountIdFromReq(hash<auto> req) {
        string aid = remove req.account_id;
        rest.setAccountId(aid);
        return req;
    }

    #! Extracts business_id from a request hash, sets it on the REST client, and returns the cleaned request
    private hash<auto> setBusinessIdFromReq(hash<auto> req) {
        string bid = remove req.business_id;
        rest.setBusinessId(bid);
        return req;
    }

    #! Sets the account ID from search options on the REST client
    private setAccountIdFromSearchOpts(*hash<auto> search_options) {
        rest.setAccountId(search_options.account_id);
    }

    #! Sets the business ID from search options on the REST client
    private setBusinessIdFromSearchOpts(*hash<auto> search_options) {
        rest.setBusinessId(search_options.business_id);
    }

    #! Returns information on supported reference data
    /** @return a hash of supported reference data; keys in the hash returned are supported in calls to
        @ref getReferenceData()
    */
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "accounts": True,
            "businesses": True,
            "categories": True,
            "clients": True,
            "invoices": True,
            "expenses": True,
            "estimates": True,
            "items": True,
            "payments": True,
            "services": True,
        };
    }

    #! Returns reference data of the given kind if available
    /** @param type the unique type name of the reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return a list of allowed values for this data
    */
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        # Set account/business IDs from action options if provided
        if (action_opts.account_id.val()) {
            rest.setAccountId(action_opts.account_id);
        }
        if (action_opts.business_id.val()) {
            rest.setBusinessId(action_opts.business_id);
        }
        switch (type) {
            case "accounts": return getReferenceAccounts();
            case "businesses": return getReferenceBusinesses();
            case "categories": return getReferenceCategories();
            case "clients": return getReferenceClients();
            case "invoices": return getReferenceInvoices();
            case "expenses": return getReferenceExpenses();
            case "estimates": return getReferenceEstimates();
            case "items": return getReferenceItems();
            case "payments": return getReferencePayments();
            case "services": return getReferenceServices();
        }
    }

    #! Returns available accounts for reference data (from /auth/api/v1/users/me)
    private *list<hash<AllowedValueInfo>> getReferenceAccounts() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("/auth/api/v1/users/me");
            if (response.business_memberships) {
                foreach hash<auto> membership in (response.business_memberships) {
                    *string account_id = membership.business.account_id;
                    if (!account_id) {
                        continue;
                    }
                    string display_name = membership.business.name
                        ?? sprintf("Account %s", account_id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": sprintf("Account ID: %s", account_id),
                        "desc": sprintf("%s (Account ID: %s)", display_name, account_id),
                        "value": account_id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available businesses for reference data (from /auth/api/v1/users/me)
    private *list<hash<AllowedValueInfo>> getReferenceBusinesses() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("/auth/api/v1/users/me");
            if (response.business_memberships) {
                foreach hash<auto> membership in (response.business_memberships) {
                    *string business_id = string(membership.business.id);
                    if (!business_id) {
                        continue;
                    }
                    string display_name = membership.business.name
                        ?? sprintf("Business %s", business_id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": sprintf("Business ID: %s", business_id),
                        "desc": sprintf("%s (Business ID: %s)", display_name, business_id),
                        "value": business_id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available expense categories for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCategories() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet(accountPath("/expenses/categories"));
            if (response.categories) {
                foreach hash<auto> category in (response.categories) {
                    string display_name = category.category ?? sprintf("Category %s", category.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": category.subcategory ?? "",
                        "desc": sprintf("%s (ID: %s)", display_name, category.id),
                        "value": category.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available clients for reference data
    private *list<hash<AllowedValueInfo>> getReferenceClients() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet(accountPath("/users/clients"));
            if (response.clients) {
                foreach hash<auto> client in (response.clients) {
                    string display_name;
                    if (client.organization.val()) {
                        display_name = client.organization;
                    } else {
                        display_name = trim(sprintf("%s %s", client.fname ?? "", client.lname ?? ""));
                    }
                    if (!display_name.val()) {
                        display_name = sprintf("Client %s", client.id);
                    }
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": client.email ?? "",
                        "desc": sprintf("%s (ID: %s)", display_name, client.id),
                        "value": client.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available invoices for reference data
    private *list<hash<AllowedValueInfo>> getReferenceInvoices() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet(accountPath("/invoices/invoices"));
            if (response.invoices) {
                foreach hash<auto> invoice in (response.invoices) {
                    string display_name = invoice.invoice_number
                        ?? sprintf("Invoice %s", invoice.id);
                    string customer = invoice.current_organization ?? "";
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": customer,
                        "desc": sprintf("%s - %s (ID: %s)", display_name, customer, invoice.id),
                        "value": invoice.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available expenses for reference data
    private *list<hash<AllowedValueInfo>> getReferenceExpenses() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet(accountPath("/expenses/expenses"));
            if (response.expenses) {
                foreach hash<auto> expense in (response.expenses) {
                    string display_name = expense.notes
                        ?? sprintf("Expense %s", expense.id);
                    string category = expense.category_name ?? "";
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": category,
                        "desc": sprintf("%s - %s (ID: %s)", display_name, category, expense.id),
                        "value": expense.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available estimates for reference data
    private *list<hash<AllowedValueInfo>> getReferenceEstimates() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet(accountPath("/estimates/estimates"));
            if (response.estimates) {
                foreach hash<auto> estimate in (response.estimates) {
                    string display_name = estimate.estimate_number
                        ?? sprintf("Estimate %s", estimate.id);
                    string customer = estimate.current_organization ?? "";
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": customer,
                        "desc": sprintf("%s - %s (ID: %s)", display_name, customer, estimate.id),
                        "value": estimate.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available items for reference data
    private *list<hash<AllowedValueInfo>> getReferenceItems() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet(accountPath("/items/items"));
            if (response.items) {
                foreach hash<auto> item in (response.items) {
                    string display_name = item.name ?? sprintf("Item %s", item.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": item.description ?? "",
                        "desc": sprintf("%s (ID: %s)", display_name, item.id),
                        "value": item.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available payments for reference data
    private *list<hash<AllowedValueInfo>> getReferencePayments() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet(accountPath("/payments/payments"));
            if (response.payments) {
                foreach hash<auto> payment in (response.payments) {
                    string display_name = sprintf("Payment %s", payment.paymentid ?? payment.id);
                    string short_desc = payment.type ?? "";
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": short_desc,
                        "desc": sprintf("%s (ID: %s)", display_name, payment.id),
                        "value": payment.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available services for reference data
    private *list<hash<AllowedValueInfo>> getReferenceServices() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet(businessPath("/services"));
            if (response.services) {
                foreach hash<auto> service in (response.services) {
                    string display_name = service.name ?? sprintf("Service %s", service.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": service.billable ? "Billable" : "Non-billable",
                        "desc": sprintf("%s (ID: %s)", display_name, service.id),
                        "value": service.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }
}

} # namespace FreshBooksDataProvider
