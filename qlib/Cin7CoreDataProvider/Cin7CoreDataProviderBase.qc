# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the Cin7CoreDataProvider module
public namespace Cin7CoreDataProvider {

#! Base class for Cin7Core data providers
public class Cin7CoreDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Cin7Core data providers
        const BaseConstructorOptions = {
            "cin7corerestclient": <DataProviderOptionInfo>{
                "display_name": "Cin7Core REST Client",
                "short_desc": "The Cin7CoreRestClient object",
                "type": AbstractDataProviderType::get(new Type("Cin7CoreRestClient::Cin7CoreRestClient")),
                "desc": "The Cin7CoreRestClient object to use for API calls",
            },
            "cin7corerestclient_options": <DataProviderOptionInfo>{
                "display_name": "Cin7Core REST Client Options",
                "short_desc": "Options for creating a Cin7CoreRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a Cin7CoreRestClient object if one is not provided directly",
            },
            "account_id": <DataProviderOptionInfo>{
                "display_name": "Account ID",
                "short_desc": "Cin7Core Account ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "The Cin7Core Account ID for authentication",
                "sensitive": True,
            },
            "apikey": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "Cin7Core API Key",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "The Cin7Core Application/API Key for authentication",
                "sensitive": True,
            },
        };
    }

    private {
        #! The REST client object for API calls
        Cin7CoreRestClient::Cin7CoreRestClient rest;

        #! The connection object, if applicable
        *Cin7CoreRestClient::Cin7CoreRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(Cin7CoreRestClient::Cin7CoreRestClient rest) {
        @assert(rest, "REST client cannot be null");
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(Cin7CoreRestClient::Cin7CoreRestConnection conn) {
        @assert(conn, "connection cannot be null");
        self.conn = conn;
        rest = conn.get(True);
        @assert(rest, "REST client from connection cannot be null");
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.cin7corerestclient) {
            rest = copts.cin7corerestclient;
        } else {
            hash<auto> client_opts = copts.cin7corerestclient_options ?? {};
            if (copts.account_id) {
                client_opts.account_id = copts.account_id;
            }
            if (copts.apikey) {
                client_opts.apikey = copts.apikey;
            }
            rest = new Cin7CoreRestClient::Cin7CoreRestClient(client_opts);
        }
    }

    #! Returns the REST client
    Cin7CoreRestClient::Cin7CoreRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *Cin7CoreRestClient::Cin7CoreRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Makes a GET request to the Cin7Core API
    /** @param path the API path
        @param params optional query parameters
        @return the response data (can be hash or list depending on endpoint)

        @throw CIN7CORE-API-ERROR if the response indicates an error
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
                # Cin7Core uses ISO format for dates
                if (val.typeCode() == NT_DATE) {
                    val = format_date("YYYY-MM-DDTHH:mm:SS", val);
                }
                parts += sprintf("%s=%s", key, encode_url(string(val), True));
            }
        }
        return parts.join("&");
    }

    #! Makes a POST request to the Cin7Core API
    /** @param path the API path
        @param body the request body
        @return the response data

        @throw CIN7CORE-API-ERROR if the response indicates an error
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

    #! Makes a PUT request to the Cin7Core API
    /** @param path the API path
        @param body the request body
        @return the response data

        @throw CIN7CORE-API-ERROR if the response indicates an error
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

    #! Makes a DELETE request to the Cin7Core API
    /** @param path the API path
        @param params optional query parameters
        @return the response data

        @throw CIN7CORE-API-ERROR if the response indicates an error
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

    #! Checks the Cin7Core API response for errors
    /** @param response the API response hash

        @throw CIN7CORE-API-ERROR if the response indicates an error
    */
    private checkResponse(hash<auto> response) {
        # Cin7Core API returns errors with status codes >= 400
        if (response.status_code >= 400) {
            string err_msg;
            # Cin7Core error format varies
            if (response.body.Exception) {
                err_msg = response.body.Exception;
            } else if (response.body.ErrorMessage) {
                err_msg = response.body.ErrorMessage;
            } else if (response.body.Message) {
                err_msg = response.body.Message;
            } else if (response.body.error) {
                err_msg = response.body.error;
            } else if (response.body.typeCode() == NT_STRING) {
                err_msg = response.body;
            } else {
                err_msg = sprintf("Cin7Core API error (HTTP %d)", response.status_code);
            }
            @debug(log(LoggerLevel::DEBUG, "checkResponse: Cin7Core API error: %s (status: %d)",
                err_msg, response.status_code));
            throw "CIN7CORE-API-ERROR", err_msg, response.body;
        }
    }

    #! Formats a date value for the Cin7Core API
    /** @param val the date value to format
        @return the formatted date string in ISO 8601 format without timezone (YYYY-MM-DDTHH:mm:SS)

        Cin7Core API expects dates in ISO 8601 format: yyyy-MM-ddTHH:mm:ss.fff
        This method formats dates consistently for both query strings and request bodies.
    */
    static string formatDate(date val) {
        return format_date("YYYY-MM-DDTHH:mm:SS", val);
    }

    #! Helper method to get client options from constructor options
    static *hash<auto> getClientOptions(*hash<auto> copts) {
        if (!copts) {
            return;
        }
        hash<auto> rv;
        if (copts.account_id) {
            rv.account_id = copts.account_id;
        }
        if (copts.apikey) {
            rv.apikey = copts.apikey;
        }
        foreach string key in (keys copts.cin7corerestclient_options) {
            rv{key} = copts.cin7corerestclient_options{key};
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
            "suppliers": True,
            "products": True,
            "sales": True,
            "locations": True,
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
            case "suppliers": return getReferenceSuppliers();
            case "products": return getReferenceProducts();
            case "sales": return getReferenceSales();
            case "locations": return getReferenceLocations();
        }
    }

    #! Returns available customers for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCustomers() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/Customer", {"Page": 1, "Limit": 50});
            list<auto> customers;
            if (response.typeCode() == NT_HASH && response.CustomerList.typeCode() == NT_LIST) {
                customers = response.CustomerList;
            }
            foreach hash<auto> customer in (customers) {
                string display_name = customer.Name ?? customer.ID;
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": customer.Email ?? "",
                    "desc": sprintf("%s (%s)", display_name, customer.ID),
                    "value": customer.ID,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceCustomers: error fetching customers: %s", ex.desc));
        }
        return rv;
    }

    #! Returns available suppliers for reference data
    private *list<hash<AllowedValueInfo>> getReferenceSuppliers() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/Supplier", {"Page": 1, "Limit": 50});
            list<auto> suppliers;
            if (response.typeCode() == NT_HASH && response.SupplierList.typeCode() == NT_LIST) {
                suppliers = response.SupplierList;
            }
            foreach hash<auto> supplier in (suppliers) {
                string display_name = supplier.Name ?? supplier.ID;
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": supplier.Email ?? "",
                    "desc": sprintf("%s (%s)", display_name, supplier.ID),
                    "value": supplier.ID,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceSuppliers: error fetching suppliers: %s", ex.desc));
        }
        return rv;
    }

    #! Returns available products for reference data
    private *list<hash<AllowedValueInfo>> getReferenceProducts() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/Product", {"Page": 1, "Limit": 50});
            list<auto> products;
            if (response.typeCode() == NT_HASH && response.Products.typeCode() == NT_LIST) {
                products = response.Products;
            }
            foreach hash<auto> product in (products) {
                string display_name = product.Name ?? product.SKU ?? product.ID;
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": product.SKU ?? "",
                    "desc": sprintf("%s (SKU: %s)", display_name, product.SKU ?? "N/A"),
                    "value": product.ID,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceProducts: error fetching products: %s", ex.desc));
        }
        return rv;
    }

    #! Returns available sales for reference data
    private *list<hash<AllowedValueInfo>> getReferenceSales() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/SaleList", {"Page": 1, "Limit": 50});
            list<auto> sales;
            if (response.typeCode() == NT_HASH && response.SaleList.typeCode() == NT_LIST) {
                sales = response.SaleList;
            }
            foreach hash<auto> sale in (sales) {
                string display_name = sale.SaleOrderNumber ?? sale.ID;
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": sale.Status ?? "",
                    "desc": sprintf("Sale %s (%s)", display_name, sale.Status ?? "unknown"),
                    "value": sale.ID,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceSales: error fetching sales: %s", ex.desc));
        }
        return rv;
    }

    #! Returns available locations for reference data
    private *list<hash<AllowedValueInfo>> getReferenceLocations() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/Location", {"Page": 1, "Limit": 50});
            list<auto> locations;
            if (response.typeCode() == NT_HASH && response.LocationList.typeCode() == NT_LIST) {
                locations = response.LocationList;
            }
            foreach hash<auto> location in (locations) {
                string display_name = location.Name ?? location.ID;
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": location.LocationType ?? "",
                    "desc": sprintf("%s (%s)", display_name, location.ID),
                    "value": location.ID,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceLocations: error fetching locations: %s", ex.desc));
        }
        return rv;
    }
}

} # namespace Cin7CoreDataProvider
