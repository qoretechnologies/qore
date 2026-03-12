# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LemlistDataProvider module definition

/*  LemlistDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the LemlistDataProvider module
public namespace LemlistDataProvider {

#! Base class for Lemlist data providers
public class LemlistDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Lemlist data providers
        const BaseConstructorOptions = {
            "lemlistrestclient": <DataProviderOptionInfo>{
                "display_name": "Lemlist REST Client",
                "short_desc": "The LemlistRestClient object",
                "type": AbstractDataProviderType::get(new Type("LemlistRestClient::LemlistRestClient")),
                "desc": "The LemlistRestClient object to use for API calls",
            },
            "lemlistrestclient_options": <DataProviderOptionInfo>{
                "display_name": "Lemlist REST Client Options",
                "short_desc": "Options for creating a LemlistRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a LemlistRestClient object if one is not provided directly",
            },
            "apikey": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "Lemlist API key",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The Lemlist API key for authentication (used as Basic Auth password with empty username)",
                "sensitive": True,
                "required": True,
            },
        };
    }

    private {
        #! The REST client object for API calls
        LemlistRestClient::LemlistRestClient rest;

        #! The connection object, if applicable
        *LemlistRestClient::LemlistRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(LemlistRestClient::LemlistRestClient rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(LemlistRestClient::LemlistRestConnection conn) {
        self.conn = conn;
        rest = conn.get(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.lemlistrestclient) {
            rest = copts.lemlistrestclient;
        } else {
            hash<auto> client_opts = copts.lemlistrestclient_options ?? {};
            if (copts.apikey) {
                client_opts.apikey = copts.apikey;
            }
            rest = new LemlistRestClient::LemlistRestClient(client_opts);
        }
    }

    #! Returns the REST client
    LemlistRestClient::LemlistRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *LemlistRestClient::LemlistRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Makes a GET request to the Lemlist API
    /** @param path the API path (version=v2 is added automatically by the REST client)
        @param params optional query parameters
        @return the response body

        @throw LEMLIST-API-ERROR if the response indicates an error
    */
    auto doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += (path =~ /\?/ ? "&" : "?") + buildQueryString(params);
        }
        hash<auto> response = rest.get(url);
        checkResponse(response);
        return response.body;
    }

    #! Makes a POST request to the Lemlist API
    /** @param path the API path
        @param body the request body
        @return the response body

        @throw LEMLIST-API-ERROR if the response indicates an error
    */
    auto doPost(string path, *hash<auto> body) {
        hash<auto> response = rest.post(path, body);
        checkResponse(response);
        return response.body;
    }

    #! Makes a PATCH request to the Lemlist API
    /** @param path the API path
        @param body the request body
        @return the response body

        @throw LEMLIST-API-ERROR if the response indicates an error
    */
    auto doPatch(string path, *hash<auto> body) {
        hash<auto> response = rest.patch(path, body);
        checkResponse(response);
        return response.body;
    }

    #! Makes a DELETE request to the Lemlist API
    /** @param path the API path
        @return the response body

        @throw LEMLIST-API-ERROR if the response indicates an error
    */
    auto doDelete(string path) {
        hash<auto> response = rest.del(path);
        checkResponse(response);
        return response.body;
    }

    #! Builds a query string from a hash of parameters
    private string buildQueryString(hash<auto> params) {
        list<string> parts;
        foreach string key in (keys params) {
            auto val = params{key};
            if (exists val) {
                # Preserve @ in values - Lemlist API doesn't accept %40 encoding in query params
                string encoded = replace(encode_url(string(val), True), "%40", "@");
                parts += sprintf("%s=%s", key, encoded);
            }
        }
        return parts.join("&");
    }

    #! Returns a hash of supported reference data types for dropdown support
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "campaigns": True,
            "leads": True,
            "tasks": True,
        };
    }

    #! Returns reference data for dropdown support
    list<hash<AllowedValueInfo>> getReferenceDataImpl(string ref_type, *hash<auto> options) {
        switch (ref_type) {
            case "campaigns":
                return getReferenceCampaigns();
            case "leads":
                return getReferenceLeads(options);
            case "tasks":
                return getReferenceTasks();
        }
        throw "UNSUPPORTED-REFERENCE-DATA", sprintf("unsupported reference data type %y", ref_type);
    }

    #! Returns reference data for campaigns
    private list<hash<AllowedValueInfo>> getReferenceCampaigns() {
        list<hash<AllowedValueInfo>> rv;
        auto response = doGet("/campaigns");
        list<auto> campaigns = response.typeCode() == NT_LIST ? response
            : (response.campaigns.typeCode() == NT_LIST ? response.campaigns : ());
        foreach auto campaign in (campaigns) {
            string id = string(campaign._id ?? campaign.id ?? "");
            string name = campaign.name ?? id;
            if (id) {
                rv += <AllowedValueInfo>{
                    "display_name": name,
                    "value": id,
                };
            }
        }
        return rv;
    }

    #! Returns reference data for leads
    private list<hash<AllowedValueInfo>> getReferenceLeads(*hash<auto> options) {
        list<hash<AllowedValueInfo>> rv;
        # Leads require a campaignId; use one from options if provided
        *string campaign_id = options.campaignId;
        if (!campaign_id) {
            # Without a campaign, return empty list - leads are campaign-scoped
            return rv;
        }
        auto response = doGet(sprintf("/campaigns/%s/leads", encode_url(campaign_id, True)), {"limit": 100});
        list<auto> leads = response.typeCode() == NT_LIST ? response
            : (response.leads.typeCode() == NT_LIST ? response.leads : ());
        foreach auto lead in (leads) {
            string id = string(lead._id ?? lead.email ?? "");
            string email = string(lead.email ?? id);
            list<string> name_parts = select (lead.firstName, lead.lastName), $1;
            string name = (name_parts ? (name_parts.join(" ") + " ") : "") + "<" + email + ">";
            if (id) {
                rv += <AllowedValueInfo>{
                    "display_name": name,
                    "value": id,
                };
            }
        }
        return rv;
    }

    #! Returns reference data for tasks
    private list<hash<AllowedValueInfo>> getReferenceTasks() {
        list<hash<AllowedValueInfo>> rv;
        auto response = doGet("/tasks", {"limit": 100});
        list<auto> tasks;
        if (response.typeCode() == NT_LIST) {
            tasks = response;
        } else if (response.tasks.typeCode() == NT_LIST) {
            tasks = response.tasks;
        } else if (response.data.typeCode() == NT_LIST) {
            tasks = response.data;
        } else if (response.items.typeCode() == NT_LIST) {
            tasks = response.items;
        }
        foreach auto task in (tasks) {
            string id = string(task._id ?? task.id ?? "");
            string label = sprintf("%s (%s)", task.type ?? "task", id);
            if (id) {
                rv += <AllowedValueInfo>{
                    "display_name": label,
                    "value": id,
                };
            }
        }
        return rv;
    }

    #! Checks the Lemlist API response for errors
    /** @param response the API response hash

        @throw LEMLIST-API-ERROR if the response indicates an error
    */
    private checkResponse(hash<auto> response) {
        if (response.status_code >= 400) {
            string err_msg;
            if (response.body.message) {
                err_msg = response.body.message;
            } else if (response.body.error) {
                err_msg = response.body.error;
            } else {
                err_msg = sprintf("Lemlist API error (HTTP %d)", response.status_code);
            }
            throw "LEMLIST-API-ERROR", err_msg, response.body;
        }
    }
}

} # namespace LemlistDataProvider
