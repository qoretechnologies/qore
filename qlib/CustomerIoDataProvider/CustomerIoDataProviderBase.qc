# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CustomerIoDataProvider module definition

/*  CustomerIoDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CustomerIoDataProvider module
public namespace CustomerIoDataProvider {

#! Base class for Customer.io data providers
public class CustomerIoDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Customer.io data providers
        const BaseConstructorOptions = {
            "customeriorestclient": <DataProviderOptionInfo>{
                "display_name": "Customer.io REST Client",
                "short_desc": "The CustomerIoRestClient object",
                "type": AbstractDataProviderType::get(new Type("CustomerIoRestClient::CustomerIoRestClient")),
                "desc": "The CustomerIoRestClient object to use for App API calls",
            },
            "customeriorestclient_options": <DataProviderOptionInfo>{
                "display_name": "Customer.io REST Client Options",
                "short_desc": "Options for creating a CustomerIoRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a CustomerIoRestClient object if one is not provided directly",
            },
            "app_api_key": <DataProviderOptionInfo>{
                "display_name": "App API Key",
                "short_desc": "Customer.io App API key",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The Customer.io App API key for Bearer token authentication",
                "sensitive": True,
                "required": True,
                "preselected": True,
            },
            "site_id": <DataProviderOptionInfo>{
                "display_name": "Site ID",
                "short_desc": "Customer.io Site ID",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The Customer.io Site ID for Track API Basic auth",
                "sensitive": True,
                "required": True,
                "preselected": True,
            },
            "tracking_api_key": <DataProviderOptionInfo>{
                "display_name": "Tracking API Key",
                "short_desc": "Customer.io Tracking API key",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The Customer.io Tracking API key for Track API Basic auth",
                "sensitive": True,
                "required": True,
                "preselected": True,
            },
            "region": <DataProviderOptionInfo>{
                "display_name": "Region",
                "short_desc": "Customer.io data center region",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The Customer.io data center region (us or eu)",
                "default_value": "us",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "us", "desc": "US data center"},
                    <AllowedValueInfo>{"value": "eu", "desc": "EU data center"},
                ),
            },
        };

        #! Track API base URLs by region
        const TrackApiUrls = {
            "us": "https://track.customer.io/api/v1",
            "eu": "https://track-eu.customer.io/api/v1",
        };

        #! Track API v2 base URLs by region
        const TrackApiV2Urls = {
            "us": "https://track.customer.io/api/v2",
            "eu": "https://track-eu.customer.io/api/v2",
        };
    }

    private {
        #! The REST client object for App API calls (Bearer token auth)
        CustomerIoRestClient::CustomerIoRestClient rest;

        #! The connection object, if applicable
        *CustomerIoRestClient::CustomerIoRestConnection conn;

        #! Secondary REST client for Track API (Basic auth), lazily created
        *RestClient::RestClient track_rest;

        #! Stored Site ID for Track API construction
        *string site_id;

        #! Stored Tracking API key for Track API construction
        *string tracking_api_key;

        #! Stored region for Track API construction
        *string region;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(CustomerIoRestClient::CustomerIoRestClient rest) {
        @assert(rest, "REST client cannot be null");
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(CustomerIoRestClient::CustomerIoRestConnection conn) {
        @assert(conn, "connection cannot be null");
        self.conn = conn;
        rest = conn.get(True);
        @assert(rest, "REST client from connection cannot be null");
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.customeriorestclient) {
            rest = copts.customeriorestclient;
        } else {
            hash<auto> client_opts = copts.customeriorestclient_options ?? {};
            if (copts.app_api_key) {
                client_opts.app_api_key = copts.app_api_key;
            }
            if (copts.region) {
                client_opts.region = copts.region;
            }
            rest = new CustomerIoRestClient::CustomerIoRestClient(client_opts);
        }
        # Store credentials for lazy Track API client construction
        if (copts.site_id) {
            site_id = copts.site_id;
        }
        if (copts.tracking_api_key) {
            tracking_api_key = copts.tracking_api_key;
        }
        region = copts.region ?? "us";
    }

    #! Returns the REST client
    CustomerIoRestClient::CustomerIoRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *CustomerIoRestClient::CustomerIoRestConnection getConnection() {
        return conn;
    }

    #! Sets Track API credentials for child providers
    /** Called by the root provider to propagate Track API credentials to children */
    setTrackApiCredentials(*string sid, *string tkey, *string reg) {
        if (sid) {
            site_id = sid;
        }
        if (tkey) {
            tracking_api_key = tkey;
        }
        if (reg) {
            region = reg;
        }
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
        if (track_rest) {
            track_rest.setLogger(logger);
        }
    }

    #! Makes a GET request to the Customer.io App API
    /** @param path the API path
        @param params optional query parameters
        @return the response data

        @throw CUSTOMERIO-API-ERROR if the response indicates an error
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

    #! Makes a POST request to the Customer.io App API
    /** @param path the API path
        @param body the request body
        @return the response data

        @throw CUSTOMERIO-API-ERROR if the response indicates an error
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

    #! Makes a PUT request to the Customer.io App API
    /** @param path the API path
        @param body the request body
        @return the response data

        @throw CUSTOMERIO-API-ERROR if the response indicates an error
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

    #! Makes a DELETE request to the Customer.io App API
    /** @param path the API path
        @return the response data

        @throw CUSTOMERIO-API-ERROR if the response indicates an error
    */
    auto doDelete(string path) {
        @assert(path.val(), "API path cannot be empty");
        @assert(rest, "REST client must be initialized before making API calls");
        @debug(log(LoggerLevel::DEBUG, "doDelete: DELETE %s", path));
        hash<auto> response = rest.del(path);
        checkResponse(response);
        @debug(log(LoggerLevel::DEBUG, "doDelete: success"));
        return response.body;
    }

    #! Returns the Track API REST client, creating it lazily if needed
    /** @return the Track API REST client

        @throw CUSTOMERIO-API-ERROR if Track API credentials are not configured
    */
    RestClient::RestClient getTrackRestClient() {
        if (!track_rest) {
            if (!site_id || !tracking_api_key) {
                throw "CUSTOMERIO-API-ERROR", "Track API credentials (site_id and tracking_api_key) are required "
                    "for Track API operations";
            }
            string base_url = TrackApiUrls{region} ?? TrackApiUrls."us";
            track_rest = new RestClient::RestClient({
                "url": base_url,
                "username": site_id,
                "password": tracking_api_key,
                "data": "json",
            });
            if (rest) {
                *LoggerInterface logger = rest.getLogger();
                if (logger) {
                    track_rest.setLogger(logger);
                }
            }
        }
        return track_rest;
    }

    #! Makes a POST request to the Customer.io Track API
    /** @param path the API path
        @param body the request body
        @return the response data

        @throw CUSTOMERIO-API-ERROR if the response indicates an error
    */
    auto doTrackPost(string path, *hash<auto> body) {
        @assert(path.val(), "API path cannot be empty");
        RestClient::RestClient tc = getTrackRestClient();
        @debug(log(LoggerLevel::DEBUG, "doTrackPost: POST %s with body: %y", path, body));
        hash<auto> response = tc.post(path, body);
        checkResponse(response);
        @debug(log(LoggerLevel::DEBUG, "doTrackPost: success"));
        return response.body;
    }

    #! Makes a PUT request to the Customer.io Track API
    /** @param path the API path
        @param body the request body
        @return the response data

        @throw CUSTOMERIO-API-ERROR if the response indicates an error
    */
    auto doTrackPut(string path, *hash<auto> body) {
        @assert(path.val(), "API path cannot be empty");
        RestClient::RestClient tc = getTrackRestClient();
        @debug(log(LoggerLevel::DEBUG, "doTrackPut: PUT %s with body: %y", path, body));
        hash<auto> response = tc.put(path, body);
        checkResponse(response);
        @debug(log(LoggerLevel::DEBUG, "doTrackPut: success"));
        return response.body;
    }

    #! Makes a DELETE request to the Customer.io Track API
    /** @param path the API path
        @return the response data

        @throw CUSTOMERIO-API-ERROR if the response indicates an error
    */
    auto doTrackDelete(string path) {
        @assert(path.val(), "API path cannot be empty");
        RestClient::RestClient tc = getTrackRestClient();
        @debug(log(LoggerLevel::DEBUG, "doTrackDelete: DELETE %s", path));
        hash<auto> response = tc.del(path);
        checkResponse(response);
        @debug(log(LoggerLevel::DEBUG, "doTrackDelete: success"));
        return response.body;
    }

    #! Makes a POST request to the Customer.io Track API v2
    auto doTrackV2Post(string path, *hash<auto> body) {
        @assert(path.val(), "API path cannot be empty");
        if (!site_id || !tracking_api_key) {
            throw "CUSTOMERIO-API-ERROR", "Track API credentials (site_id and tracking_api_key) are required "
                "for Track API v2 operations";
        }
        string base_url = TrackApiV2Urls{region} ?? TrackApiV2Urls."us";
        RestClient::RestClient v2 = new RestClient::RestClient({
            "url": base_url,
            "username": site_id,
            "password": tracking_api_key,
            "data": "json",
        });
        if (rest) {
            *LoggerInterface logger = rest.getLogger();
            if (logger) {
                v2.setLogger(logger);
            }
        }
        @debug(log(LoggerLevel::DEBUG, "doTrackV2Post: POST %s with body: %y", path, body));
        hash<auto> response = v2.post(path, body);
        checkResponse(response);
        return response.body;
    }

    #! Returns information on supported reference data
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "segments": True,
            "campaigns": True,
            "broadcasts": True,
            "collections": True,
            "newsletters": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "segments": return getReferenceSegments();
            case "campaigns": return getReferenceCampaigns();
            case "broadcasts": return getReferenceBroadcasts();
            case "collections": return getReferenceCollections();
            case "newsletters": return getReferenceNewsletters();
        }
    }

    #! Returns available segments for reference data
    private *list<hash<AllowedValueInfo>> getReferenceSegments() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/segments");
            auto segments = response.segments ?? response;
            if (segments.typeCode() == NT_LIST) {
                foreach hash<auto> segment in (segments) {
                    string display_name = segment.name ?? sprintf("Segment %d", segment.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": sprintf("ID: %d", segment.id),
                        "desc": display_name,
                        "value": int(segment.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceSegments: error: %s", ex.desc));
        }
        return rv;
    }

    #! Returns available campaigns for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCampaigns() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/campaigns");
            auto campaigns = response.campaigns ?? response;
            if (campaigns.typeCode() == NT_LIST) {
                foreach hash<auto> campaign in (campaigns) {
                    string display_name = campaign.name ?? sprintf("Campaign %d", campaign.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": sprintf("ID: %d", campaign.id),
                        "desc": display_name,
                        "value": int(campaign.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceCampaigns: error: %s", ex.desc));
        }
        return rv;
    }

    #! Returns available broadcasts for reference data
    private *list<hash<AllowedValueInfo>> getReferenceBroadcasts() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/broadcasts");
            auto broadcasts = response.broadcasts ?? response;
            if (broadcasts.typeCode() == NT_LIST) {
                foreach hash<auto> broadcast in (broadcasts) {
                    string display_name = broadcast.name ?? sprintf("Broadcast %d", broadcast.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": sprintf("ID: %d", broadcast.id),
                        "desc": display_name,
                        "value": int(broadcast.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceBroadcasts: error: %s", ex.desc));
        }
        return rv;
    }

    #! Returns available collections for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCollections() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/collections");
            auto collections = response.collections ?? response;
            if (collections.typeCode() == NT_LIST) {
                foreach hash<auto> collection in (collections) {
                    string display_name = collection.name ?? sprintf("Collection %d", collection.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": sprintf("ID: %d", collection.id),
                        "desc": display_name,
                        "value": int(collection.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceCollections: error: %s", ex.desc));
        }
        return rv;
    }

    #! Returns available newsletters for reference data
    private *list<hash<AllowedValueInfo>> getReferenceNewsletters() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("/newsletters");
            auto newsletters = response.newsletters ?? response;
            if (newsletters.typeCode() == NT_LIST) {
                foreach hash<auto> newsletter in (newsletters) {
                    string display_name = newsletter.name ?? sprintf("Newsletter %d", newsletter.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": sprintf("ID: %d", newsletter.id),
                        "desc": display_name,
                        "value": int(newsletter.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            @debug(log(LoggerLevel::DEBUG, "getReferenceNewsletters: error: %s", ex.desc));
        }
        return rv;
    }

    #! Checks the Customer.io API response for errors
    /** @param response the API response hash

        @throw CUSTOMERIO-API-ERROR if the response indicates an error
    */
    private checkResponse(hash<auto> response) {
        if (response.status_code >= 400) {
            string err_msg;
            if (response.body.errors) {
                if (response.body.errors.typeCode() == NT_LIST) {
                    err_msg = (map $1.detail ?? $1.message ?? string($1), response.body.errors).join("; ");
                } else {
                    err_msg = string(response.body.errors);
                }
            } else if (response.body.meta && response.body.meta.error) {
                err_msg = response.body.meta.error;
            } else if (response.body.error) {
                err_msg = response.body.error;
            } else if (response.body.message) {
                err_msg = response.body.message;
            } else {
                err_msg = sprintf("Customer.io API error (HTTP %d)", response.status_code);
            }
            @debug(log(LoggerLevel::DEBUG, "checkResponse: Customer.io API error: %s (status: %d)",
                err_msg, response.status_code));
            throw "CUSTOMERIO-API-ERROR", err_msg, response.body;
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

    #! Helper method to get client options from constructor options
    static *hash<auto> getClientOptions(*hash<auto> copts) {
        if (!copts) {
            return;
        }
        hash<auto> rv;
        if (copts.app_api_key) {
            rv.app_api_key = copts.app_api_key;
        }
        foreach string key in (keys copts.customeriorestclient_options) {
            rv{key} = copts.customeriorestclient_options{key};
        }
        return rv;
    }
}

} # namespace CustomerIoDataProvider
