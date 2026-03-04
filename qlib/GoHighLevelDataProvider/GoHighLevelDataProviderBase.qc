# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GoHighLevelDataProvider module definition

/*  GoHighLevelDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GoHighLevelDataProvider module
public namespace GoHighLevelDataProvider {

#! Base class for GoHighLevel data providers
public class GoHighLevelDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for GoHighLevel data providers
        const BaseConstructorOptions = {
            "gohighlevelrestclient": <DataProviderOptionInfo>{
                "display_name": "GoHighLevel REST Client",
                "short_desc": "The GoHighLevelRestClient object",
                "type": AbstractDataProviderType::get(
                    new Type("GoHighLevelRestClient::GoHighLevelRestClient")),
                "desc": "The GoHighLevelRestClient object to use for API calls",
            },
            "gohighlevelrestclient_options": <DataProviderOptionInfo>{
                "display_name": "GoHighLevel REST Client Options",
                "short_desc": "Options for creating a GoHighLevelRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a GoHighLevelRestClient object if one is not provided "
                    "directly",
            },
        };
    }

    private {
        #! The REST client object for API calls
        GoHighLevelRestClient::GoHighLevelRestClient rest;

        #! The connection object, if applicable
        *GoHighLevelRestClient::GoHighLevelRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(GoHighLevelRestClient::GoHighLevelRestClient rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(GoHighLevelRestClient::GoHighLevelRestConnection conn) {
        self.conn = conn;
        rest = conn.get(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.gohighlevelrestclient) {
            rest = copts.gohighlevelrestclient;
        } else {
            hash<auto> client_opts = copts.gohighlevelrestclient_options ?? {};
            rest = new GoHighLevelRestClient::GoHighLevelRestClient(client_opts);
        }
    }

    #! Returns the REST client
    GoHighLevelRestClient::GoHighLevelRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *GoHighLevelRestClient::GoHighLevelRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Makes a GET request to the GoHighLevel API
    /** @param path the API path
        @param params optional query parameters
        @return the response body

        @throw GOHIGHLEVEL-API-ERROR if the response indicates an error
    */
    hash<auto> doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a POST request to the GoHighLevel API
    /** @param path the API path
        @param body the request body
        @return the response body

        @throw GOHIGHLEVEL-API-ERROR if the response indicates an error
    */
    hash<auto> doPost(string path, *hash<auto> body) {
        return doRestRequest("POST", path, body);
    }

    #! Makes a PUT request to the GoHighLevel API
    /** @param path the API path
        @param body the request body
        @return the response body

        @throw GOHIGHLEVEL-API-ERROR if the response indicates an error
    */
    hash<auto> doPut(string path, *hash<auto> body) {
        return doRestRequest("PUT", path, body);
    }

    #! Makes a DELETE request to the GoHighLevel API
    /** @param path the API path
        @return the response body

        @throw GOHIGHLEVEL-API-ERROR if the response indicates an error
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

    #! Fetches all pages from a GET endpoint using offset-based pagination
    /** @param path the API path
        @param result_key the key in the response containing the result list
        @param params optional query parameters
        @param limit optional maximum number of results to return
        @return a list of all result records

        Uses \c offset and \c limit query parameters for pagination
    */
    list<hash<auto>> fetchAllPagesOffset(string path, string result_key, *hash<auto> params,
            *int limit) {
        list<hash<auto>> results = ();
        int offset = 0;
        int page_size = limit && limit < DefaultPageSize ? limit : DefaultPageSize;
        while (True) {
            hash<auto> qparams = params ?? {};
            qparams.offset = offset;
            qparams.limit = page_size;
            hash<auto> response = doGet(path, qparams);
            if (response{result_key}) {
                list<hash<auto>> page_results = response{result_key};
                results += page_results;
                if (page_results.size() < page_size) {
                    break;
                }
            } else {
                break;
            }
            if (limit && results.size() >= limit) {
                results = results[0..limit-1];
                break;
            }
            offset += page_size;
        }
        return results;
    }

    #! Fetches all pages from a GET endpoint using page-based pagination
    /** @param path the API path
        @param result_key the key in the response containing the result list
        @param params optional query parameters
        @param limit optional maximum number of results to return
        @return a list of all result records

        Uses a \c page query parameter that increments from 1
    */
    list<hash<auto>> fetchAllPagesPage(string path, string result_key, *hash<auto> params,
            *int limit) {
        list<hash<auto>> results = ();
        int page = 1;
        while (True) {
            hash<auto> qparams = params ?? {};
            qparams.page = page;
            hash<auto> response = doGet(path, qparams);
            if (response{result_key}) {
                list<hash<auto>> page_results = response{result_key};
                results += page_results;
                if (page_results.size() == 0) {
                    break;
                }
            } else {
                break;
            }
            if (limit && results.size() >= limit) {
                results = results[0..limit-1];
                break;
            }
            ++page;
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

        GoHighLevel API responses are JSON objects with the result data directly in the body.
        Error responses contain an "error" or "message" string field at the top level:
        @code
        {"error": "Unauthorized", "message": "Invalid API key"}
        @endcode

        @throw GOHIGHLEVEL-API-ERROR if the response indicates an error
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
                        throw "GOHIGHLEVEL-ERROR", sprintf("unsupported HTTP method: %s", method);
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

    #! Checks the GoHighLevel API response for errors
    /** @param response the API response hash

        GoHighLevel returns errors as:
        @code
        {"error": "Unauthorized"}
        @endcode
        or:
        @code
        {"message": "Invalid request", "statusCode": 422}
        @endcode

        @throw GOHIGHLEVEL-API-ERROR if the response contains errors
    */
    private checkResponse(hash<auto> response) {
        if (response.body.error.val()) {
            string err_msg = response.body.error;
            if (response.body.message.val()) {
                err_msg += sprintf(": %s", response.body.message);
            }
            throw "GOHIGHLEVEL-API-ERROR", err_msg, response.body;
        } else if (response.body.message.val() && response.body.statusCode
                && response.body.statusCode >= 400) {
            throw "GOHIGHLEVEL-API-ERROR", response.body.message, response.body;
        }
    }

    #! Returns information on supported reference data
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "campaigns": True,
            "contacts": True,
            "opportunities": True,
            "pipelines": True,
            "users": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "campaigns": return getReferenceCampaigns();
            case "contacts": return getReferenceContacts();
            case "opportunities": return getReferenceOpportunities();
            case "pipelines": return getReferencePipelines();
            case "users": return getReferenceUsers();
        }
    }

    #! Returns available campaigns for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCampaigns() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("campaigns");
            if (response.campaigns) {
                foreach hash<auto> campaign in (response.campaigns) {
                    string display_name = campaign.name ?? sprintf("Campaign %s", campaign.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": campaign.status ?? "",
                        "desc": sprintf("%s (ID: %s)", display_name, campaign.id),
                        "value": campaign.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available contacts for reference data
    private *list<hash<AllowedValueInfo>> getReferenceContacts() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("contacts", {"limit": DefaultPageSize});
            if (response.contacts) {
                foreach hash<auto> contact in (response.contacts) {
                    string display_name = trim(sprintf("%s %s",
                        contact.firstName ?? "", contact.lastName ?? ""));
                    if (!display_name.val()) {
                        display_name = sprintf("Contact %s", contact.id);
                    }
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": contact.email ?? "",
                        "desc": sprintf("%s (ID: %s)", display_name, contact.id),
                        "value": contact.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available opportunities for reference data
    private *list<hash<AllowedValueInfo>> getReferenceOpportunities() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("opportunities/search", {"limit": DefaultPageSize});
            if (response.opportunities) {
                foreach hash<auto> opp in (response.opportunities) {
                    string display_name = opp.name ?? sprintf("Opportunity %s", opp.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": opp.status ?? "",
                        "desc": sprintf("%s (ID: %s)", display_name, opp.id),
                        "value": opp.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available pipelines for reference data
    private *list<hash<AllowedValueInfo>> getReferencePipelines() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("opportunities/pipelines");
            if (response.pipelines) {
                foreach hash<auto> pipeline in (response.pipelines) {
                    string display_name = pipeline.name ?? sprintf("Pipeline %s", pipeline.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": "",
                        "desc": sprintf("%s (ID: %s)", display_name, pipeline.id),
                        "value": pipeline.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available users for reference data
    private *list<hash<AllowedValueInfo>> getReferenceUsers() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("users");
            if (response.users) {
                foreach hash<auto> user in (response.users) {
                    string display_name = trim(sprintf("%s %s",
                        user.firstName ?? "", user.lastName ?? ""));
                    if (!display_name.val()) {
                        display_name = user.name ?? user.email
                            ?? sprintf("User %s", user.id);
                    }
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": user.email ?? "",
                        "desc": sprintf("%s (ID: %s)", display_name, user.id),
                        "value": user.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Formats a date for GoHighLevel API parameters (ISO 8601)
    static string formatDateTimeGHL(date dt) {
        return dt.format("YYYY-MM-DDTHH:mm:ss.xxxZ");
    }
}

} # namespace GoHighLevelDataProvider
