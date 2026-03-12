# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CloseDataProvider module definition

/*  CloseDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CloseDataProvider module
public namespace CloseDataProvider {

#! Base class for Close data providers
public class CloseDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Close data providers
        const BaseConstructorOptions = {
            "closerestclient": <DataProviderOptionInfo>{
                "display_name": "Close REST Client",
                "short_desc": "The CloseRestClient object",
                "type": AbstractDataProviderType::get(
                    new Type("CloseRestClient::CloseRestClient")),
                "desc": "The CloseRestClient object to use for API calls",
            },
            "closerestclient_options": <DataProviderOptionInfo>{
                "display_name": "Close REST Client Options",
                "short_desc": "Options for creating a CloseRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a CloseRestClient object if one is not provided "
                    "directly",
            },
        };
    }

    private {
        #! The REST client object for API calls
        CloseRestClient::CloseRestClient rest;

        #! The connection object, if applicable
        *CloseRestClient::CloseRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(CloseRestClient::CloseRestClient rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(CloseRestClient::CloseRestConnection conn) {
        self.conn = conn;
        rest = conn.get(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.closerestclient) {
            rest = copts.closerestclient;
        } else {
            hash<auto> client_opts = copts.closerestclient_options ?? {};
            rest = new CloseRestClient::CloseRestClient(client_opts);
        }
    }

    #! Returns the REST client
    CloseRestClient::CloseRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *CloseRestClient::CloseRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Makes a GET request to the Close API
    /** @param path the API path (relative to /api/v1)
        @param params optional query parameters
        @return the response body

        @throw CLOSE-API-ERROR if the response indicates an error
    */
    hash<auto> doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a POST request to the Close API
    /** @param path the API path (relative to /api/v1)
        @param body the request body
        @return the response body

        @throw CLOSE-API-ERROR if the response indicates an error
    */
    hash<auto> doPost(string path, *hash<auto> body) {
        return doRestRequest("POST", path, body);
    }

    #! Makes a PUT request to the Close API
    /** @param path the API path (relative to /api/v1)
        @param body the request body
        @return the response body

        @throw CLOSE-API-ERROR if the response indicates an error
    */
    hash<auto> doPut(string path, *hash<auto> body) {
        return doRestRequest("PUT", path, body);
    }

    #! Makes a DELETE request to the Close API
    /** @param path the API path (relative to /api/v1)
        @return the response body

        @throw CLOSE-API-ERROR if the response indicates an error
    */
    hash<auto> doDelete(string path) {
        return doRestRequest("DELETE", path);
    }

    #! Fetches all pages from a GET endpoint using offset-based pagination
    /** Close uses _limit/_skip with has_more for standard CRUD endpoints.

        @param path the API path
        @param params optional query parameters
        @param limit optional maximum number of results to return
        @return a list of all result records
    */
    list<hash<auto>> fetchAllPages(string path, *hash<auto> params, *int limit) {
        list<hash<auto>> results = ();
        int skip = 0;
        int page_size = DefaultPageSize;
        while (True) {
            hash<auto> qparams = params ?? {};
            qparams._limit = page_size;
            qparams._skip = skip;
            hash<auto> response = doGet(path, qparams);
            if (response.data) {
                results += response.data;
            }
            if (limit && results.size() >= limit) {
                results = results[0..limit-1];
                break;
            }
            if (!response.has_more) {
                break;
            }
            skip += page_size;
        }
        return results;
    }

    #! Fetches event log pages using cursor-based pagination
    /** Close's Event Log uses _cursor/cursor_next for pagination.

        @param params query parameters (must include object_type and action)
        @param limit optional maximum number of results to return
        @return a list of event records (in API order, latest-first)
    */
    list<hash<auto>> fetchEventPages(hash<auto> params, *int limit) {
        list<hash<auto>> results = ();
        *string cursor;
        while (True) {
            hash<auto> qparams = params;
            if (cursor) {
                qparams._cursor = cursor;
            }
            hash<auto> response = doGet("event", qparams);
            if (response.data) {
                results += response.data;
            }
            if (limit && results.size() >= limit) {
                results = results[0..limit-1];
                break;
            }
            cursor = response.cursor_next;
            if (!cursor) {
                break;
            }
        }
        return results;
    }

    #! Makes a REST request with retry and rate limit handling
    /** @param method the HTTP method
        @param path the API path
        @param body optional request body
        @return the response body hash

        @throw CLOSE-API-ERROR if the response indicates an error
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
                        throw "CLOSE-ERROR", sprintf("unsupported HTTP method: %s", method);
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
            *string rate_reset = ex.arg.headers."rate-reset";
            int wait_secs = rate_reset.val() ? rate_reset.toInt() : 5;
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

    #! Returns information on supported reference data
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "leads": True,
            "contacts": True,
            "opportunities": True,
            "users": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "leads": return getReferenceLeads();
            case "contacts": return getReferenceContacts();
            case "opportunities": return getReferenceOpportunities();
            case "users": return getReferenceUsers();
        }
    }

    #! Returns available leads for reference data
    private *list<hash<AllowedValueInfo>> getReferenceLeads() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("lead", {"_limit": DefaultPageSize});
            if (response.data) {
                foreach hash<auto> lead in (response.data) {
                    string display_name = lead.display_name ?? sprintf("Lead %s", lead.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": lead.status_label ?? "",
                        "desc": sprintf("%s (ID: %s)", display_name, lead.id),
                        "value": string(lead.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available contacts for reference data
    private *list<hash<AllowedValueInfo>> getReferenceContacts() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("contact", {"_limit": DefaultPageSize});
            if (response.data) {
                foreach hash<auto> contact in (response.data) {
                    string display_name = contact.name ?? sprintf("Contact %s", contact.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": contact.title ?? "",
                        "desc": sprintf("%s (ID: %s)", display_name, contact.id),
                        "value": string(contact.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available opportunities for reference data
    private *list<hash<AllowedValueInfo>> getReferenceOpportunities() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("opportunity", {"_limit": DefaultPageSize});
            if (response.data) {
                foreach hash<auto> opp in (response.data) {
                    string display_name = opp.lead_name
                        ?? sprintf("Opportunity %s", opp.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": opp.status_label ?? "",
                        "desc": sprintf("%s - %s (ID: %s)", display_name,
                            opp.status_label ?? "", opp.id),
                        "value": string(opp.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available users for reference data
    private *list<hash<AllowedValueInfo>> getReferenceUsers() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("user");
            if (response.data) {
                foreach hash<auto> user in (response.data) {
                    string display_name = trim(sprintf("%s %s",
                        user.first_name ?? "", user.last_name ?? ""));
                    if (!display_name.val()) {
                        display_name = user.email ?? sprintf("User %s", user.id);
                    }
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": user.email ?? "",
                        "desc": sprintf("%s (ID: %s)", display_name, user.id),
                        "value": string(user.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Formats a date for Close API parameters (ISO 8601)
    static string formatDateTime(date dt) {
        return dt.format("YYYY-MM-DDTHH:mm:ss.xxxZ");
    }
}

} # namespace CloseDataProvider
