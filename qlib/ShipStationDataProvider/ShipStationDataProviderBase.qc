# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShipStationDataProvider module
public namespace ShipStationDataProvider {

#! Base class for ShipStation data providers
public class ShipStationDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for ShipStation data providers
        const BaseConstructorOptions = {
            "shipstationrestclient": <DataProviderOptionInfo>{
                "display_name": "ShipStation REST Client",
                "short_desc": "The ShipStationRestClient object",
                "type": AbstractDataProviderType::get(new Type("ShipStationRestClient::ShipStationRestClient")),
                "desc": "The ShipStationRestClient object to use for API calls",
            },
            "shipstationrestclient_options": <DataProviderOptionInfo>{
                "display_name": "ShipStation REST Client Options",
                "short_desc": "Options for creating a ShipStationRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a ShipStationRestClient object if one is not provided directly",
            },
            "api_key": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "ShipStation API key",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The ShipStation API key for authentication (used as Basic Auth username)",
                "sensitive": True,
                "required": True,
                "preselected": True,
            },
            "api_secret": <DataProviderOptionInfo>{
                "display_name": "API Secret",
                "short_desc": "ShipStation API secret",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The ShipStation API secret for authentication (used as Basic Auth password)",
                "sensitive": True,
                "required": True,
                "preselected": True,
            },
        };
    }

    private {
        #! The REST client object for API calls
        ShipStationRestClient::ShipStationRestClient rest;

        #! The connection object, if applicable
        *ShipStationRestClient::ShipStationRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(ShipStationRestClient::ShipStationRestClient rest) {
        @assert(rest, "REST client cannot be null");
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(ShipStationRestClient::ShipStationRestConnection conn) {
        @assert(conn, "connection cannot be null");
        self.conn = conn;
        rest = conn.get(True);
        @assert(rest, "REST client from connection cannot be null");
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.shipstationrestclient) {
            rest = copts.shipstationrestclient;
        } else {
            hash<auto> client_opts = copts.shipstationrestclient_options ?? {};
            if (copts.api_key) {
                client_opts.api_key = copts.api_key;
            }
            if (copts.api_secret) {
                client_opts.api_secret = copts.api_secret;
            }
            rest = new ShipStationRestClient::ShipStationRestClient(client_opts);
        }
    }

    #! Returns the REST client
    ShipStationRestClient::ShipStationRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *ShipStationRestClient::ShipStationRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Makes a GET request to the ShipStation API
    /** @param path the API path
        @param params optional query parameters
        @return the response data (can be hash or list depending on endpoint)

        @throw SHIPSTATION-API-ERROR if the response indicates an error
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
                parts += sprintf("%s=%s", key, encode_url(string(val), True));
            }
        }
        return parts.join("&");
    }

    #! Makes a POST request to the ShipStation API
    /** @param path the API path
        @param body the request body
        @return the response data

        @throw SHIPSTATION-API-ERROR if the response indicates an error
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

    #! Makes a PUT request to the ShipStation API
    /** @param path the API path
        @param body the request body
        @return the response data

        @throw SHIPSTATION-API-ERROR if the response indicates an error
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

    #! Makes a DELETE request to the ShipStation API
    /** @param path the API path
        @return the response data

        @throw SHIPSTATION-API-ERROR if the response indicates an error
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

    #! Checks the ShipStation API response for errors
    /** @param response the API response hash

        @throw SHIPSTATION-API-ERROR if the response indicates an error
    */
    private checkResponse(hash<auto> response) {
        # ShipStation API returns error in message field on failure
        if (response.status_code >= 400) {
            string err_msg;
            if (response.body.Message) {
                err_msg = response.body.Message;
            } else if (response.body.message) {
                err_msg = response.body.message;
            } else if (response.body.ExceptionMessage) {
                err_msg = response.body.ExceptionMessage;
            } else {
                err_msg = sprintf("ShipStation API error (HTTP %d)", response.status_code);
            }
            @debug(log(LoggerLevel::DEBUG, "checkResponse: ShipStation API error: %s (status: %d)",
                err_msg, response.status_code));
            throw "SHIPSTATION-API-ERROR", err_msg, response.body;
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
        if (copts.api_secret) {
            rv.api_secret = copts.api_secret;
        }
        foreach string key in (keys copts.shipstationrestclient_options) {
            rv{key} = copts.shipstationrestclient_options{key};
        }
        return rv;
    }

    #! Returns information on supported reference data
    /** @return a hash of supported reference data; keys in the hash returned are supported in calls to
        @ref getReferenceData()
    */
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "carriers": True,
            "stores": True,
            "tags": True,
            "users": True,
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
            case "carriers": return getReferenceCarriers();
            case "stores": return getReferenceStores();
            case "tags": return getReferenceTags();
            case "users": return getReferenceUsers();
            case "warehouses": return getReferenceWarehouses();
            case "services": return getReferenceServices(action_opts.carrierCode);
            case "packages": return getReferencePackages(action_opts.carrierCode);
        }
    }

    #! Returns available carriers for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCarriers() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/carriers");
            if (response.typeCode() == NT_LIST) {
                foreach hash<auto> carrier in (response) {
                    string display_name = carrier.name ?? carrier.code;
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": carrier.code,
                        "desc": sprintf("%s (%s)", display_name, carrier.code),
                        "value": carrier.code,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceCarriers: error fetching carriers: %s", ex.desc));
        }
        return rv;
    }

    #! Returns available stores for reference data
    private *list<hash<AllowedValueInfo>> getReferenceStores() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/stores");
            if (response.typeCode() == NT_LIST) {
                foreach hash<auto> store in (response) {
                    string display_name = store.storeName ?? sprintf("Store %d", store.storeId);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": store.marketplaceName ?? "Unknown",
                        "desc": sprintf("%s (%s)", display_name, store.marketplaceName ?? "Unknown"),
                        "value": store.storeId,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceStores: error fetching stores: %s", ex.desc));
        }
        return rv;
    }

    #! Returns available tags for reference data
    private *list<hash<AllowedValueInfo>> getReferenceTags() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/accounts/listtags");
            if (response.typeCode() == NT_LIST) {
                foreach hash<auto> tag in (response) {
                    string display_name = tag.name ?? sprintf("Tag %d", tag.tagId);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": tag.color ?? "",
                        "desc": display_name,
                        "value": tag.tagId,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceTags: error fetching tags: %s", ex.desc));
        }
        return rv;
    }

    #! Returns available users for reference data
    private *list<hash<AllowedValueInfo>> getReferenceUsers() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/users");
            if (response.typeCode() == NT_LIST) {
                foreach hash<auto> user in (response) {
                    string display_name = user.name ?? user.userName ?? sprintf("User %s", user.userId);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": user.userName ?? "",
                        "desc": display_name,
                        "value": user.userId,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceUsers: error fetching users: %s", ex.desc));
        }
        return rv;
    }

    #! Returns available warehouses for reference data
    private *list<hash<AllowedValueInfo>> getReferenceWarehouses() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/warehouses");
            if (response.typeCode() == NT_LIST) {
                foreach hash<auto> warehouse in (response) {
                    string display_name = warehouse.warehouseName ?? sprintf("Warehouse %d", warehouse.warehouseId);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": warehouse.isDefault ? "Default" : "",
                        "desc": display_name,
                        "value": warehouse.warehouseId,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceWarehouses: error fetching warehouses: %s", ex.desc));
        }
        return rv;
    }

    #! Returns available services for a carrier
    private *list<hash<AllowedValueInfo>> getReferenceServices(*string carrierCode) {
        if (!carrierCode) {
            return;
        }
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/carriers/listservices", {"carrierCode": carrierCode});
            if (response.typeCode() == NT_LIST) {
                foreach hash<auto> service in (response) {
                    string display_name = service.name ?? service.code;
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": service.code,
                        "desc": sprintf("%s (%s)", display_name, service.code),
                        "value": service.code,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceServices: error fetching services: %s", ex.desc));
        }
        return rv;
    }

    #! Returns available packages for a carrier
    private *list<hash<AllowedValueInfo>> getReferencePackages(*string carrierCode) {
        if (!carrierCode) {
            return;
        }
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/carriers/listpackages", {"carrierCode": carrierCode});
            if (response.typeCode() == NT_LIST) {
                foreach hash<auto> pkg in (response) {
                    string display_name = pkg.name ?? pkg.code;
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": pkg.code,
                        "desc": sprintf("%s (%s)", display_name, pkg.code),
                        "value": pkg.code,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferencePackages: error fetching packages: %s", ex.desc));
        }
        return rv;
    }
}

} # namespace ShipStationDataProvider
