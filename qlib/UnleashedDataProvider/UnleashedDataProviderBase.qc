# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnleashedDataProvider module definition

/*  UnleashedDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the UnleashedDataProvider module
public namespace UnleashedDataProvider {

#! Base class for Unleashed data providers
public class UnleashedDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Unleashed data providers
        const BaseConstructorOptions = {
            "unleashedrestclient": <DataProviderOptionInfo>{
                "display_name": "Unleashed REST Client",
                "short_desc": "The UnleashedRestClient object",
                "type": AbstractDataProviderType::get(new Type("UnleashedRestClient::UnleashedRestClient")),
                "desc": "The UnleashedRestClient object to use for API calls",
            },
            "unleashedrestclient_options": <DataProviderOptionInfo>{
                "display_name": "Unleashed REST Client Options",
                "short_desc": "Options for creating an UnleashedRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating an UnleashedRestClient object if one is not provided directly",
            },
            "api_id": <DataProviderOptionInfo>{
                "display_name": "API ID",
                "short_desc": "Unleashed API ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "The Unleashed API ID for authentication",
                "sensitive": True,
            },
            "apikey": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "Unleashed API Key",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "The Unleashed API Key for HMAC signature computation",
                "sensitive": True,
            },
        };
    }

    private {
        #! The REST client object for API calls
        UnleashedRestClient::UnleashedRestClient rest;

        #! The connection object, if applicable
        *UnleashedRestClient::UnleashedRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(UnleashedRestClient::UnleashedRestClient rest) {
        @assert(rest, "REST client cannot be null");
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(UnleashedRestClient::UnleashedRestConnection conn) {
        @assert(conn, "connection cannot be null");
        self.conn = conn;
        rest = conn.get(True);
        @assert(rest, "REST client from connection cannot be null");
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.unleashedrestclient) {
            rest = copts.unleashedrestclient;
        } else {
            hash<auto> client_opts = copts.unleashedrestclient_options ?? {};
            if (copts.api_id) {
                client_opts.api_id = copts.api_id;
            }
            if (copts.apikey) {
                client_opts.apikey = copts.apikey;
            }
            rest = new UnleashedRestClient::UnleashedRestClient(client_opts);
        }
    }

    #! Returns the REST client
    UnleashedRestClient::UnleashedRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *UnleashedRestClient::UnleashedRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Makes a GET request to the Unleashed API
    /** @param path the API path
        @param params optional query parameters
        @return the response data (can be hash or list depending on endpoint)

        @throw UNLEASHED-API-ERROR if the response indicates an error
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
    private string buildQueryString(hash<auto> params) {
        list<string> parts;
        foreach string key in (keys params) {
            auto val = params{key};
            if (exists val) {
                # Unleashed uses ISO format for dates (YYYY-MM-DD)
                if (val.typeCode() == NT_DATE) {
                    val = format_date("YYYY-MM-DD", val);
                }
                parts += sprintf("%s=%s", key, encode_url(string(val), True));
            }
        }
        return parts.join("&");
    }

    #! Makes a POST request to the Unleashed API
    /** @param path the API path
        @param body the request body
        @return the response data

        @throw UNLEASHED-API-ERROR if the response indicates an error
    */
    hash<auto> doPost(string path, *hash<auto> body) {
        @assert(path.val(), "API path cannot be empty");
        @assert(rest, "REST client must be initialized before making API calls");
        @debug(log(LoggerLevel::DEBUG, "doPost: POST %s with body: %y", path, body));
        hash<auto> response = rest.post(path, body);
        checkResponse(response);
        @debug(log(LoggerLevel::DEBUG, "doPost: success, response keys: %y", keys response.body));
        return response.body;
    }

    #! Makes a PUT request to the Unleashed API
    /** @param path the API path
        @param body the request body
        @return the response data

        @throw UNLEASHED-API-ERROR if the response indicates an error
    */
    hash<auto> doPut(string path, *hash<auto> body) {
        @assert(path.val(), "API path cannot be empty");
        @assert(rest, "REST client must be initialized before making API calls");
        @debug(log(LoggerLevel::DEBUG, "doPut: PUT %s with body: %y", path, body));
        hash<auto> response = rest.put(path, body);
        checkResponse(response);
        @debug(log(LoggerLevel::DEBUG, "doPut: success, response keys: %y", keys response.body));
        return response.body;
    }

    #! Makes a DELETE request to the Unleashed API
    /** @param path the API path
        @param params optional query parameters
        @return the response data

        @throw UNLEASHED-API-ERROR if the response indicates an error
    */
    hash<auto> doDelete(string path, *hash<auto> params) {
        @assert(path.val(), "API path cannot be empty");
        @assert(rest, "REST client must be initialized before making API calls");
        @debug(log(LoggerLevel::DEBUG, "doDelete: DELETE %s", path));
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        hash<auto> response = rest.del(url);
        checkResponse(response);
        @debug(log(LoggerLevel::DEBUG, "doDelete: success"));
        return response.body;
    }

    #! Checks the Unleashed API response for errors
    /** @param response the API response hash

        @throw UNLEASHED-API-ERROR if the response indicates an error
    */
    private checkResponse(hash<auto> response) {
        # Unleashed API returns errors with status codes >= 400
        if (response.status_code >= 400) {
            string err_msg;
            # Unleashed error format varies
            if (response.body.Description) {
                err_msg = response.body.Description;
            } else if (response.body.Exception) {
                err_msg = response.body.Exception;
            } else if (response.body.Message) {
                err_msg = response.body.Message;
            } else if (response.body.error) {
                err_msg = response.body.error;
            } else if (response.body.typeCode() == NT_STRING) {
                err_msg = response.body;
            } else {
                err_msg = sprintf("Unleashed API error (HTTP %d)", response.status_code);
            }
            @debug(log(LoggerLevel::DEBUG, "checkResponse: Unleashed API error: %s (status: %d)",
                err_msg, response.status_code));
            throw "UNLEASHED-API-ERROR", err_msg, response.body;
        }
    }

    #! Formats a date value for the Unleashed API
    /** @param val the date value to format
        @return the formatted date string in ISO 8601 format (YYYY-MM-DD)

        Unleashed API expects dates in ISO 8601 format: YYYY-MM-DD
        This method formats dates consistently for both query strings and request bodies.
    */
    static string formatDate(date val) {
        return format_date("YYYY-MM-DD", val);
    }

    #! Helper method to get client options from constructor options
    static *hash<auto> getClientOptions(*hash<auto> copts) {
        if (!copts) {
            return;
        }
        hash<auto> rv;
        if (copts.api_id) {
            rv.api_id = copts.api_id;
        }
        if (copts.apikey) {
            rv.apikey = copts.apikey;
        }
        foreach string key in (keys copts.unleashedrestclient_options) {
            rv{key} = copts.unleashedrestclient_options{key};
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
            "products": True,
            "salesorders": True,
            "creditnotes": True,
            "warehouses": True,
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
            case "products": return getReferenceProducts();
            case "salesorders": return getReferenceSalesOrders();
            case "creditnotes": return getReferenceCreditNotes();
            case "warehouses": return getReferenceWarehouses();
        }
    }

    #! Returns available customers for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCustomers() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/Customers", {"pageSize": 50});
            list<auto> customers;
            if (response.typeCode() == NT_HASH && response.Items.typeCode() == NT_LIST) {
                customers = response.Items;
            }
            foreach hash<auto> customer in (customers) {
                string display_name = customer.CustomerName ?? customer.CustomerCode ?? customer.Guid;
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": customer.CustomerCode ?? "",
                    "desc": sprintf("%s (%s)", display_name, customer.Guid),
                    "value": customer.Guid,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceCustomers: error fetching customers: %s", ex.desc));
        }
        return rv;
    }

    #! Returns available products for reference data
    private *list<hash<AllowedValueInfo>> getReferenceProducts() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/Products", {"pageSize": 50});
            list<auto> products;
            if (response.typeCode() == NT_HASH && response.Items.typeCode() == NT_LIST) {
                products = response.Items;
            }
            foreach hash<auto> product in (products) {
                string display_name = product.ProductDescription ?? product.ProductCode ?? product.Guid;
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": product.ProductCode ?? "",
                    "desc": sprintf("%s (Code: %s)", display_name, product.ProductCode ?? "N/A"),
                    "value": product.Guid,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceProducts: error fetching products: %s", ex.desc));
        }
        return rv;
    }

    #! Returns available sales orders for reference data
    private *list<hash<AllowedValueInfo>> getReferenceSalesOrders() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/SalesOrders", {"pageSize": 50});
            list<auto> orders;
            if (response.typeCode() == NT_HASH && response.Items.typeCode() == NT_LIST) {
                orders = response.Items;
            }
            foreach hash<auto> order in (orders) {
                string display_name = order.OrderNumber ?? order.Guid;
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": order.OrderStatus ?? "",
                    "desc": sprintf("Order %s (%s)", display_name, order.OrderStatus ?? "unknown"),
                    "value": order.Guid,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceSalesOrders: error fetching sales orders: %s", ex.desc));
        }
        return rv;
    }

    #! Returns available credit notes for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCreditNotes() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/CreditNotes", {"pageSize": 50});
            list<auto> creditnotes;
            if (response.typeCode() == NT_HASH && response.Items.typeCode() == NT_LIST) {
                creditnotes = response.Items;
            }
            foreach hash<auto> creditnote in (creditnotes) {
                string display_name = creditnote.CreditNumber ?? creditnote.Guid;
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": creditnote.CreditStatus ?? "",
                    "desc": sprintf("Credit Note %s (%s)", display_name, creditnote.CreditStatus ?? "unknown"),
                    "value": creditnote.Guid,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceCreditNotes: error fetching credit notes: %s", ex.desc));
        }
        return rv;
    }

    #! Returns available warehouses for reference data
    private *list<hash<AllowedValueInfo>> getReferenceWarehouses() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/Warehouses", {"pageSize": 50});
            list<auto> warehouses;
            if (response.typeCode() == NT_HASH && response.Items.typeCode() == NT_LIST) {
                warehouses = response.Items;
            }
            foreach hash<auto> warehouse in (warehouses) {
                string display_name = warehouse.WarehouseName ?? warehouse.WarehouseCode ?? warehouse.Guid;
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": warehouse.WarehouseCode ?? "",
                    "desc": sprintf("%s (%s)", display_name, warehouse.Guid),
                    "value": warehouse.Guid,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceWarehouses: error fetching warehouses: %s", ex.desc));
        }
        return rv;
    }
}

} # namespace UnleashedDataProvider
