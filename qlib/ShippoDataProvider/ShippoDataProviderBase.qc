# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShippoDataProvider module
public namespace ShippoDataProvider {

#! Base class for Shippo data providers
public class ShippoDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Shippo data providers
        const BaseConstructorOptions = {
            "shipporestclient": <DataProviderOptionInfo>{
                "display_name": "Shippo REST Client",
                "short_desc": "The ShippoRestClient object",
                "type": AbstractDataProviderType::get(new Type("ShippoRestClient::ShippoRestClient")),
                "desc": "The ShippoRestClient object to use for API calls",
            },
            "shipporestclient_options": <DataProviderOptionInfo>{
                "display_name": "Shippo REST Client Options",
                "short_desc": "Options for creating a ShippoRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a ShippoRestClient object if one is not provided directly",
            },
            "apikey": <DataProviderOptionInfo>{
                "display_name": "API Token",
                "short_desc": "Shippo API token",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "The Shippo API token for authentication. Use this OR OAuth2 credentials.",
                "sensitive": True,
            },
        };
    }

    private {
        #! The REST client object for API calls
        ShippoRestClient::ShippoRestClient rest;

        #! The connection object, if applicable
        *ShippoRestClient::ShippoRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(ShippoRestClient::ShippoRestClient rest) {
        @assert(rest, "REST client cannot be null");
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(ShippoRestClient::ShippoRestConnection conn) {
        @assert(conn, "connection cannot be null");
        self.conn = conn;
        rest = conn.get(True);
        @assert(rest, "REST client from connection cannot be null");
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.shipporestclient) {
            rest = copts.shipporestclient;
        } else {
            hash<auto> client_opts = copts.shipporestclient_options ?? {};
            if (copts.apikey) {
                client_opts.apikey = copts.apikey;
            }
            rest = new ShippoRestClient::ShippoRestClient(client_opts);
        }
    }

    #! Returns the REST client
    ShippoRestClient::ShippoRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *ShippoRestClient::ShippoRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Makes a GET request to the Shippo API
    /** @param path the API path
        @param params optional query parameters
        @return the response data (can be hash or list depending on endpoint)

        @throw SHIPPO-API-ERROR if the response indicates an error
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
                # Shippo uses ISO format for dates
                if (val.typeCode() == NT_DATE) {
                    val = format_date("YYYY-MM-DDTHH:mm:ssZ", val);
                }
                parts += sprintf("%s=%s", key, encode_url(string(val), True));
            }
        }
        return parts.join("&");
    }

    #! Makes a POST request to the Shippo API
    /** @param path the API path
        @param body the request body
        @return the response data

        @throw SHIPPO-API-ERROR if the response indicates an error
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

    #! Makes a PUT request to the Shippo API
    /** @param path the API path
        @param body the request body
        @return the response data

        @throw SHIPPO-API-ERROR if the response indicates an error
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

    #! Makes a DELETE request to the Shippo API
    /** @param path the API path
        @return the response data

        @throw SHIPPO-API-ERROR if the response indicates an error
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

    #! Checks the Shippo API response for errors
    /** @param response the API response hash

        @throw SHIPPO-API-ERROR if the response indicates an error
    */
    private checkResponse(hash<auto> response) {
        # Shippo API returns errors with status codes >= 400
        if (response.status_code >= 400) {
            string err_msg;
            # Shippo error format: {"detail": "message"} or {"status": "ERROR", "messages": [...]}
            if (response.body.detail) {
                err_msg = response.body.detail;
            } else if (response.body.messages && response.body.messages.typeCode() == NT_LIST) {
                err_msg = (map $1.text ?? $1, response.body.messages).join("; ");
            } else if (response.body.message) {
                err_msg = response.body.message;
            } else if (response.body.error) {
                err_msg = response.body.error;
            } else {
                err_msg = sprintf("Shippo API error (HTTP %d)", response.status_code);
            }
            @debug(log(LoggerLevel::DEBUG, "checkResponse: Shippo API error: %s (status: %d)",
                err_msg, response.status_code));
            throw "SHIPPO-API-ERROR", err_msg, response.body;
        }
    }

    #! Helper method to get client options from constructor options
    static *hash<auto> getClientOptions(*hash<auto> copts) {
        if (!copts) {
            return;
        }
        hash<auto> rv;
        if (copts.apikey) {
            rv.apikey = copts.apikey;
        }
        foreach string key in (keys copts.shipporestclient_options) {
            rv{key} = copts.shipporestclient_options{key};
        }
        return rv;
    }

    #! Returns information on supported reference data
    /** @return a hash of supported reference data; keys in the hash returned are supported in calls to
        @ref getReferenceData()
    */
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "carrier_accounts": True,
            "shipments": True,
            "transactions": True,
            "orders": True,
        };
    }

    #! Returns reference data of the given kind if available
    /** @param type the unique type name of the reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return a list of allowed values for this data
    */
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "carrier_accounts": return getReferenceCarrierAccounts();
            case "shipments": return getReferenceShipments();
            case "transactions": return getReferenceTransactions();
            case "orders": return getReferenceOrders();
        }
    }

    #! Returns available carrier accounts for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCarrierAccounts() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/carrier_accounts");
            list<auto> accounts;
            # Shippo returns paginated results with "results" key
            if (response.typeCode() == NT_HASH && response.results.typeCode() == NT_LIST) {
                accounts = response.results;
            } else if (response.typeCode() == NT_LIST) {
                accounts = response;
            }
            foreach hash<auto> account in (accounts) {
                string display_name = account.carrier ?? account.object_id;
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": account.account_id ?? "",
                    "desc": sprintf("%s (%s)", display_name, account.object_id),
                    "value": account.object_id,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceCarrierAccounts: error fetching carrier accounts: %s", ex.desc));
        }
        return rv;
    }

    #! Returns available shipments for reference data
    private *list<hash<AllowedValueInfo>> getReferenceShipments() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/shipments", {"results": 50});
            list<auto> shipments;
            if (response.typeCode() == NT_HASH && response.results.typeCode() == NT_LIST) {
                shipments = response.results;
            } else if (response.typeCode() == NT_LIST) {
                shipments = response;
            }
            foreach hash<auto> shipment in (shipments) {
                string display_name = shipment.object_id;
                # Build a descriptive name from addresses if available
                if (shipment.address_to.city) {
                    display_name = sprintf("%s → %s", shipment.address_from.city ?? "?",
                        shipment.address_to.city);
                }
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": shipment.status ?? "",
                    "desc": sprintf("Shipment %s (%s)", shipment.object_id, shipment.status ?? "unknown"),
                    "value": shipment.object_id,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceShipments: error fetching shipments: %s", ex.desc));
        }
        return rv;
    }

    #! Returns available transactions (shipping labels) for reference data
    private *list<hash<AllowedValueInfo>> getReferenceTransactions() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/transactions", {"results": 50});
            list<auto> transactions;
            if (response.typeCode() == NT_HASH && response.results.typeCode() == NT_LIST) {
                transactions = response.results;
            } else if (response.typeCode() == NT_LIST) {
                transactions = response;
            }
            foreach hash<auto> transaction in (transactions) {
                string display_name = transaction.tracking_number ?? transaction.object_id;
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": transaction.status ?? "",
                    "desc": sprintf("Label %s (tracking: %s)", transaction.object_id,
                        transaction.tracking_number ?? "pending"),
                    "value": transaction.object_id,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceTransactions: error fetching transactions: %s", ex.desc));
        }
        return rv;
    }

    #! Returns available orders for reference data
    private *list<hash<AllowedValueInfo>> getReferenceOrders() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/orders", {"results": 50});
            list<auto> orders;
            if (response.typeCode() == NT_HASH && response.results.typeCode() == NT_LIST) {
                orders = response.results;
            } else if (response.typeCode() == NT_LIST) {
                orders = response;
            }
            foreach hash<auto> order in (orders) {
                string display_name = order.order_number ?? order.object_id;
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": order.order_status ?? "",
                    "desc": sprintf("Order %s (%s)", order.order_number ?? order.object_id,
                        order.order_status ?? "unknown"),
                    "value": order.object_id,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceOrders: error fetching orders: %s", ex.desc));
        }
        return rv;
    }
}

} # namespace ShippoDataProvider
