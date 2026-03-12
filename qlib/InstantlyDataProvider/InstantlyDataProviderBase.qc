# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the InstantlyDataProvider module
public namespace InstantlyDataProvider {

#! Base class for Instantly data providers
public class InstantlyDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Instantly data providers
        const BaseConstructorOptions = {
            "instantlyrestclient": <DataProviderOptionInfo>{
                "display_name": "Instantly REST Client",
                "short_desc": "The InstantlyRestClient object",
                "type": AbstractDataProviderType::get(new Type("InstantlyRestClient::InstantlyRestClient")),
                "desc": "The InstantlyRestClient object to use for API calls",
            },
            "instantlyrestclient_options": <DataProviderOptionInfo>{
                "display_name": "Instantly REST Client Options",
                "short_desc": "Options for creating an InstantlyRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating an InstantlyRestClient object if one is not provided directly",
            },
            "apikey": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "Instantly API key",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The Instantly API key for Bearer token authentication",
                "sensitive": True,
                "required": True,
                "preselected": True,
            },
        };
    }

    private {
        #! The REST client object for API calls
        InstantlyRestClient::InstantlyRestClient rest;

        #! The connection object, if applicable
        *InstantlyRestClient::InstantlyRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(InstantlyRestClient::InstantlyRestClient rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(InstantlyRestClient::InstantlyRestConnection conn) {
        self.conn = conn;
        rest = conn.get(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.instantlyrestclient) {
            rest = copts.instantlyrestclient;
        } else {
            hash<auto> client_opts = copts.instantlyrestclient_options ?? {};
            if (copts.apikey) {
                client_opts.apikey = copts.apikey;
            }
            rest = new InstantlyRestClient::InstantlyRestClient(client_opts);
        }
    }

    #! Returns the REST client
    InstantlyRestClient::InstantlyRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *InstantlyRestClient::InstantlyRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Makes a GET request to the Instantly API
    /** @param path the API path
        @param params optional query parameters
        @return the response data

        @throw INSTANTLY-API-ERROR if the response indicates an error
    */
    auto doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        try {
            return rest.get(url).body;
        } catch (hash<ExceptionInfo> ex) {
            handleRestException(ex);
        }
    }

    #! Builds a query string from a hash of parameters
    private string buildQueryString(hash<auto> params) {
        list<string> parts;
        foreach string key in (keys params) {
            auto val = params{key};
            if (exists val) {
                parts += sprintf("%s=%s", encode_url(key, True), encode_url(string(val), True));
            }
        }
        return parts.join("&");
    }

    #! Makes a POST request to the Instantly API
    /** @param path the API path
        @param body the request body
        @return the response data

        @throw INSTANTLY-API-ERROR if the response indicates an error
    */
    auto doPost(string path, *hash<auto> body) {
        try {
            return rest.post(path, body).body;
        } catch (hash<ExceptionInfo> ex) {
            handleRestException(ex);
        }
    }

    #! Makes a PATCH request to the Instantly API
    /** @param path the API path
        @param body the request body
        @return the response data

        @throw INSTANTLY-API-ERROR if the response indicates an error
    */
    auto doPatch(string path, *hash<auto> body) {
        try {
            return rest.patch(path, body).body;
        } catch (hash<ExceptionInfo> ex) {
            handleRestException(ex);
        }
    }

    #! Makes a DELETE request to the Instantly API
    /** @param path the API path
        @param body optional request body
        @return the response data

        @throw INSTANTLY-API-ERROR if the response indicates an error
    */
    auto doDelete(string path, *hash<auto> body) {
        try {
            return rest.del(path, body).body;
        } catch (hash<ExceptionInfo> ex) {
            handleRestException(ex);
        }
    }

    #! Makes a paginated GET request and returns all items
    /** @param path the API path
        @param params optional query parameters
        @param data_key the key in the response containing the items list (default: "items")
        @return a list of all items across all pages
    */
    list<auto> doPaginatedGet(string path, *hash<auto> params, string data_key = "items") {
        list<auto> all_items = ();
        hash<auto> req_params = params ?? {};
        if (!req_params.limit) {
            req_params.limit = DefaultPageSize;
        }
        while (True) {
            auto response;
            try {
                response = doGet(path, req_params);
            } catch (hash<ExceptionInfo> ex) {
                # If we already have items, return them; otherwise rethrow
                if (all_items) {
                    break;
                }
                rethrow;
            }
            if (response.typeCode() == NT_HASH) {
                auto items = response{data_key};
                if (items.typeCode() == NT_LIST && items.size() > 0) {
                    all_items += items;
                } else {
                    break;
                }
                if (response.next_starting_after) {
                    req_params.starting_after = response.next_starting_after;
                } else {
                    break;
                }
            } else if (response.typeCode() == NT_LIST) {
                all_items += response;
                break;
            } else {
                break;
            }
        }
        return all_items;
    }

    #! Makes a paginated POST request and returns all items
    /** @param path the API path
        @param body the request body
        @param data_key the key in the response containing the items list (default: "items")
        @return a list of all items across all pages
    */
    list<auto> doPaginatedPost(string path, *hash<auto> body, string data_key = "items") {
        list<auto> all_items = ();
        hash<auto> req_body = body ?? {};
        if (!req_body.limit) {
            req_body.limit = DefaultPageSize;
        }
        while (True) {
            auto response;
            try {
                response = doPost(path, req_body);
            } catch (hash<ExceptionInfo> ex) {
                # If we already have items, return them; otherwise rethrow
                if (all_items) {
                    break;
                }
                rethrow;
            }
            if (response.typeCode() == NT_HASH) {
                auto items = response{data_key};
                if (items.typeCode() == NT_LIST && items.size() > 0) {
                    all_items += items;
                } else {
                    break;
                }
                if (response.next_starting_after) {
                    req_body.starting_after = response.next_starting_after;
                } else {
                    break;
                }
            } else if (response.typeCode() == NT_LIST) {
                all_items += response;
                break;
            } else {
                break;
            }
        }
        return all_items;
    }

    #! Returns supported reference data types
    /** @return a hash of supported reference data types

        @see @ref getReferenceData()
    */
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "campaigns": True,
            "lead_lists": True,
            "lead_labels": True,
        };
    }

    #! Returns reference data of the given kind if available
    /** @param type the unique type name of the reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return a list of allowed values for this data
    */
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "campaigns": return getReferenceCampaigns();
            case "lead_lists": return getReferenceLeadLists();
            case "lead_labels": return getReferenceLeadLabels();
        }
    }

    #! Returns available campaigns for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCampaigns() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> campaigns = doPaginatedGet("campaigns");
            foreach hash<auto> campaign in (campaigns) {
                string display_name = campaign.name ?? string(campaign.id);
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": sprintf("Status: %d", campaign.status ?? 0),
                    "desc": display_name,
                    "value": string(campaign.id),
                };
            }
        } catch () {
        }
        return rv;
    }

    #! Returns available lead lists for reference data
    private *list<hash<AllowedValueInfo>> getReferenceLeadLists() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> lists = doPaginatedGet("lead-lists");
            foreach hash<auto> lead_list in (lists) {
                string display_name = lead_list.name ?? string(lead_list.id);
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "desc": display_name,
                    "value": string(lead_list.id),
                };
            }
        } catch () {
        }
        return rv;
    }

    #! Returns available lead labels for reference data
    private *list<hash<AllowedValueInfo>> getReferenceLeadLabels() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> labels = doPaginatedGet("lead-labels");
            foreach hash<auto> label in (labels) {
                string display_name = label.name ?? string(label.id);
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "desc": display_name,
                    "value": string(label.id),
                };
            }
        } catch () {
        }
        return rv;
    }

    #! Handles REST exceptions by extracting Instantly-specific error details
    /** @param ex the exception info from the REST client

        @throw INSTANTLY-API-ERROR with the Instantly error message extracted from the response body
    */
    private handleRestException(hash<ExceptionInfo> ex) {
        if (ex.err == "REST-RESPONSE-ERROR") {
            # Try to extract a meaningful error message from the response body
            *hash<auto> body;
            if (ex.arg.typeCode() == NT_HASH) {
                body = ex.arg;
            }
            string err_msg;
            if (body.message) {
                err_msg = body.message;
            } else if (body.error) {
                err_msg = body.error;
            } else if (body.detail) {
                err_msg = body.detail;
            } else {
                err_msg = ex.desc;
            }
            throw "INSTANTLY-API-ERROR", err_msg, body;
        }
        throw ex.err, ex.desc, ex.arg;
    }
}

} # namespace InstantlyDataProvider
