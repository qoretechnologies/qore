# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ChargebeeDataProvider module definition

/*  ChargebeeDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ChargebeeDataProvider module
public namespace ChargebeeDataProvider {

#! Base class for Chargebee data providers
public class ChargebeeDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Chargebee data providers
        const BaseConstructorOptions = {
            "chargebeerestclient": <DataProviderOptionInfo>{
                "display_name": "Chargebee REST Client",
                "short_desc": "The ChargebeeRestClient object",
                "type": AbstractDataProviderType::get(new Type("ChargebeeRestClient::ChargebeeRestClient")),
                "desc": "The ChargebeeRestClient object to use for API calls",
            },
            "chargebeerestclient_options": <DataProviderOptionInfo>{
                "display_name": "Chargebee REST Client Options",
                "short_desc": "Options for creating a ChargebeeRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a ChargebeeRestClient object if one is not provided directly",
            },
            "api_key": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "Chargebee API key",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The Chargebee API key for authentication",
                "sensitive": True,
                "required": True,
            },
            "site": <DataProviderOptionInfo>{
                "display_name": "Site",
                "short_desc": "Chargebee site name",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The Chargebee site name (e.g., `mycompany` for mycompany.chargebee.com)",
                "required": True,
            },
        };
    }

    private {
        #! The REST client object for API calls
        ChargebeeRestClient::ChargebeeRestClient rest;

        #! The connection object, if applicable
        *ChargebeeRestClient::ChargebeeRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(ChargebeeRestClient::ChargebeeRestClient rest) {
        @assert(rest, "REST client cannot be null");
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(ChargebeeRestClient::ChargebeeRestConnection conn) {
        @assert(conn, "connection cannot be null");
        self.conn = conn;
        rest = conn.get(True);
        @assert(rest, "REST client from connection cannot be null");
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.chargebeerestclient) {
            rest = copts.chargebeerestclient;
        } else {
            hash<auto> client_opts = copts.chargebeerestclient_options ?? {};
            if (copts.api_key) {
                client_opts.api_key = copts.api_key;
            }
            if (copts.site) {
                client_opts.site = copts.site;
            }
            rest = new ChargebeeRestClient::ChargebeeRestClient(client_opts);
        }
    }

    #! Returns the REST client
    ChargebeeRestClient::ChargebeeRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *ChargebeeRestClient::ChargebeeRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Makes a GET request to the Chargebee API
    /** @param path the API path
        @param params optional query parameters
        @return the response data (can be hash or list depending on endpoint)

        @throw CHARGEBEE-API-ERROR if the response indicates an error
    */
    auto doGet(string path, *hash<auto> params) {
        @assert(path.val(), "API path cannot be empty");
        @assert(rest, "REST client must be initialized before making API calls");
        @debug(log(LoggerLevel::DEBUG, "doGet: GET %s with params: %y", path, params));
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        hash<auto> response = rest.get(url);
        checkResponse(response);
        @debug(log(LoggerLevel::DEBUG, "doGet: success, response keys: %y", keys response.body));
        return response.body;
    }

    #! Builds a query string from a hash of parameters
    /** Handles both flat parameters and Chargebee filter notation.

        Chargebee uses bracket notation for filtering, e.g.:
        - @code status[is]=active @endcode
        - @code created_at[after]=1609459200 @endcode

        @param params a hash of query parameters; keys may contain bracket notation
        @return the URL-encoded query string
    */
    private string buildQueryString(hash<auto> params) {
        list<string> parts;
        foreach string key in (keys params) {
            auto val = params{key};
            if (!exists val) {
                continue;
            }
            if (val.typeCode() == NT_HASH) {
                # Handle Chargebee filter notation: e.g., {"status": {"is": "active"}}
                # becomes status[is]=active
                foreach string op in (keys val) {
                    if (exists val{op}) {
                        parts += sprintf("%s[%s]=%s", key, op, encode_url(string(val{op}), True));
                    }
                }
            } else {
                parts += sprintf("%s=%s", key, encode_url(string(val), True));
            }
        }
        return parts.join("&");
    }

    #! Makes a POST request to the Chargebee API
    /** @param path the API path
        @param body the request body
        @return the response data

        @throw CHARGEBEE-API-ERROR if the response indicates an error
    */
    hash<auto> doPost(string path, *hash<auto> body) {
        @assert(path.val(), "API path cannot be empty");
        @assert(rest, "REST client must be initialized before making API calls");
        # Remove NOTHING values to avoid form-encoding errors
        if (body) {
            body = remove_nothing(body);
        }
        @debug(log(LoggerLevel::DEBUG, "doPost: POST %s with body: %y", path, body));
        hash<auto> response = rest.post(path, body ? body : NOTHING);
        checkResponse(response);
        @debug(log(LoggerLevel::DEBUG, "doPost: success, response keys: %y", keys response.body));
        return response.body;
    }

    #! Removes keys with NOTHING values from a hash
    private static hash<auto> remove_nothing(hash<auto> h) {
        hash<auto> rv;
        foreach string key in (keys h) {
            if (exists h{key}) {
                rv{key} = h{key};
            }
        }
        return rv;
    }

    #! Makes a DELETE request to the Chargebee API
    /** @param path the API path
        @return the response data

        @throw CHARGEBEE-API-ERROR if the response indicates an error
    */
    hash<auto> doDelete(string path) {
        @assert(path.val(), "API path cannot be empty");
        @assert(rest, "REST client must be initialized before making API calls");
        @debug(log(LoggerLevel::DEBUG, "doDelete: DELETE %s", path));
        hash<auto> response = rest.del(path);
        checkResponse(response);
        @debug(log(LoggerLevel::DEBUG, "doDelete: success"));
        return response.body;
    }

    #! Checks the Chargebee API response for errors
    /** @param response the API response hash

        @throw CHARGEBEE-API-ERROR if the response indicates an error
    */
    private checkResponse(hash<auto> response) {
        if (response.status_code >= 400) {
            string err_msg;
            if (response.body.message) {
                err_msg = response.body.message;
            } else if (response.body.error_msg) {
                err_msg = response.body.error_msg;
            } else if (response.body.api_error_code) {
                err_msg = response.body.api_error_code;
            } else {
                err_msg = sprintf("Chargebee API error (HTTP %d)", response.status_code);
            }
            @debug(log(LoggerLevel::DEBUG, "checkResponse: Chargebee API error: %s (status: %d)",
                err_msg, response.status_code));
            throw "CHARGEBEE-API-ERROR", err_msg, response.body;
        }
    }

    #! Helper method to get client options from constructor options
    static *hash<auto> getClientOptions(*hash<auto> copts) {
        if (!copts) {
            return;
        }
        hash<auto> rv;
        if (copts.api_key) {
            rv.api_key = copts.api_key;
        }
        if (copts.site) {
            rv.site = copts.site;
        }
        foreach string key in (keys copts.chargebeerestclient_options) {
            rv{key} = copts.chargebeerestclient_options{key};
        }
        return rv;
    }

    #! Returns information on supported reference data
    /** @return a hash of supported reference data; keys in the hash returned are supported in calls to
        @ref getReferenceData()
    */
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "customers": True,
            "subscriptions": True,
            "invoices": True,
            "coupons": True,
        };
    }

    #! Returns reference data of the given kind if available
    /** @param type the unique type name of the reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return a list of allowed values for this data
    */
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "customers": return getReferenceCustomers();
            case "subscriptions": return getReferenceSubscriptions();
            case "invoices": return getReferenceInvoices();
            case "coupons": return getReferenceCoupons();
        }
    }

    #! Returns available customers for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCustomers() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/customers");
            if (response.list.typeCode() == NT_LIST) {
                foreach hash<auto> entry in (response.list) {
                    hash<auto> customer = entry.customer;
                    string display_name = customer.first_name && customer.last_name
                        ? sprintf("%s %s", customer.first_name, customer.last_name)
                        : customer.email ?? customer.id;
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": customer.email ?? "",
                        "desc": sprintf("%s (%s)", display_name, customer.id),
                        "value": customer.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceCustomers: error fetching customers: %s", ex.desc));
        }
        return rv;
    }

    #! Returns available subscriptions for reference data
    private *list<hash<AllowedValueInfo>> getReferenceSubscriptions() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/subscriptions");
            if (response.list.typeCode() == NT_LIST) {
                foreach hash<auto> entry in (response.list) {
                    hash<auto> subscription = entry.subscription;
                    string display_name = sprintf("%s (%s)", subscription.id, subscription.status ?? "unknown");
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": subscription.customer_id ?? "",
                        "desc": sprintf("Subscription %s for customer %s", subscription.id,
                            subscription.customer_id ?? "unknown"),
                        "value": subscription.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceSubscriptions: error fetching subscriptions: %s", ex.desc));
        }
        return rv;
    }

    #! Returns available invoices for reference data
    private *list<hash<AllowedValueInfo>> getReferenceInvoices() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/invoices");
            if (response.list.typeCode() == NT_LIST) {
                foreach hash<auto> entry in (response.list) {
                    hash<auto> invoice = entry.invoice;
                    string display_name = sprintf("%s (%s)", invoice.id, invoice.status ?? "unknown");
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": invoice.customer_id ?? "",
                        "desc": sprintf("Invoice %s for customer %s", invoice.id,
                            invoice.customer_id ?? "unknown"),
                        "value": invoice.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceInvoices: error fetching invoices: %s", ex.desc));
        }
        return rv;
    }

    #! Returns available coupons for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCoupons() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/coupons");
            if (response.list.typeCode() == NT_LIST) {
                foreach hash<auto> entry in (response.list) {
                    hash<auto> coupon = entry.coupon;
                    string display_name = coupon.name ?? coupon.id;
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": coupon.discount_type ?? "",
                        "desc": sprintf("%s (%s)", display_name, coupon.id),
                        "value": coupon.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceCoupons: error fetching coupons: %s", ex.desc));
        }
        return rv;
    }
}

} # namespace ChargebeeDataProvider
