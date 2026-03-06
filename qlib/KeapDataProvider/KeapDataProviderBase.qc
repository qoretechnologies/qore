# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KeapDataProvider module definition

/*  KeapDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {

#! Base class for Keap data providers
public class KeapDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Keap data providers
        const BaseConstructorOptions = {
            "keaprestclient": <DataProviderOptionInfo>{
                "display_name": "Keap REST Client",
                "short_desc": "The KeapRestClient object",
                "type": AbstractDataProviderType::get(
                    new Type("KeapRestClient::KeapRestClient")),
                "desc": "The KeapRestClient object to use for API calls",
            },
            "keaprestclient_options": <DataProviderOptionInfo>{
                "display_name": "Keap REST Client Options",
                "short_desc": "Options for creating a KeapRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a KeapRestClient object if one is not provided "
                    "directly",
            },
        };
    }

    private {
        #! The REST client object for API calls
        KeapRestClient::KeapRestClient rest;

        #! The connection object, if applicable
        *KeapRestClient::KeapRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(KeapRestClient::KeapRestClient rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(KeapRestClient::KeapRestConnection conn) {
        self.conn = conn;
        rest = conn.get(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.keaprestclient) {
            rest = copts.keaprestclient;
        } else {
            hash<auto> client_opts = copts.keaprestclient_options ?? {};
            rest = new KeapRestClient::KeapRestClient(client_opts);
        }
    }

    #! Returns the REST client
    KeapRestClient::KeapRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *KeapRestClient::KeapRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Makes a GET request to the Keap API
    /** @param path the API path
        @param params optional query parameters
        @return the response body

        @throw KEAP-API-ERROR if the response indicates an error
    */
    hash<auto> doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a POST request to the Keap API
    /** @param path the API path
        @param body the request body
        @return the response body

        @throw KEAP-API-ERROR if the response indicates an error
    */
    hash<auto> doPost(string path, *hash<auto> body) {
        return doRestRequest("POST", path, body);
    }

    #! Makes a PUT request to the Keap API
    /** @param path the API path
        @param body the request body
        @return the response body

        @throw KEAP-API-ERROR if the response indicates an error
    */
    hash<auto> doPut(string path, *hash<auto> body) {
        return doRestRequest("PUT", path, body);
    }

    #! Makes a PATCH request to the Keap API
    /** @param path the API path
        @param body the request body
        @return the response body

        @throw KEAP-API-ERROR if the response indicates an error
    */
    hash<auto> doPatch(string path, *hash<auto> body) {
        return doRestRequest("PATCH", path, body);
    }

    #! Makes a DELETE request to the Keap API
    /** @param path the API path
        @return the response body

        @throw KEAP-API-ERROR if the response indicates an error
    */
    hash<auto> doDelete(string path) {
        return doRestRequest("DELETE", path);
    }

    #! Fetches all pages from a GET endpoint using offset-based pagination
    /** @param path the API path
        @param result_key the key in the response containing the result list
        @param params optional query parameters
        @param limit optional maximum number of results to return
        @return a list of all result records
    */
    list<hash<auto>> fetchAllPages(string path, string result_key, *hash<auto> params, *int limit) {
        list<hash<auto>> results = ();
        int offset = 0;
        int page_size = limit ?? DefaultPageSize;
        while (True) {
            hash<auto> qparams = params ?? {};
            qparams.offset = offset;
            qparams.limit = page_size;
            hash<auto> response = doGet(path, qparams);
            if (response{result_key}) {
                results += response{result_key};
            }
            if (limit && results.size() >= limit) {
                results = results[0..limit-1];
                break;
            }
            if (!response{result_key} || response{result_key}.size() < page_size) {
                break;
            }
            offset += page_size;
        }
        return results;
    }

    #! Makes a REST request with retry and rate limit handling
    /** @param method the HTTP method
        @param path the API path
        @param body optional request body
        @return the response body hash

        Keap API error responses contain a "message" or "error" field at the top level:
        @code
        {"message": "...", "status_code": 400}
        @endcode

        @throw KEAP-API-ERROR if the response indicates an error
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
                    case "PATCH":
                        response = rest.patch(path, body, \info);
                        break;
                    case "DELETE":
                        response = rest.del(path, NOTHING, \info);
                        break;
                    default:
                        throw "KEAP-ERROR", sprintf("unsupported HTTP method: %s", method);
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

    #! Checks the Keap API response for errors
    /** @param response the API response hash

        Keap returns errors as:
        @code
        {"message": "Error description", "status_code": 400}
        @endcode
        or:
        @code
        {"error": "error_type", "error_description": "..."}
        @endcode

        @throw KEAP-API-ERROR if the response contains errors
    */
    private checkResponse(hash<auto> response) {
        if (response.body.message) {
            throw "KEAP-API-ERROR", response.body.message, response.body;
        }
        if (response.body.error) {
            string err_msg = response.body.error;
            if (response.body.error_description.val()) {
                err_msg += sprintf(": %s", response.body.error_description);
            }
            throw "KEAP-API-ERROR", err_msg, response.body;
        }
    }

    #! Returns information on supported reference data
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "contacts": True,
            "companies": True,
            "tags": True,
            "opportunities": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "contacts": return getReferenceContacts();
            case "companies": return getReferenceCompanies();
            case "tags": return getReferenceTags();
            case "opportunities": return getReferenceOpportunities();
        }
    }

    #! Returns available contacts for reference data
    private *list<hash<AllowedValueInfo>> getReferenceContacts() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("contacts", {"limit": DefaultPageSize});
            if (response.contacts) {
                foreach hash<auto> contact in (response.contacts) {
                    string display_name = trim(sprintf("%s %s",
                        contact.given_name ?? "", contact.family_name ?? ""));
                    if (!display_name.val()) {
                        display_name = contact.email_addresses[0].email
                            ?? sprintf("Contact %d", contact.id);
                    }
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": contact.email_addresses[0].email ?? "",
                        "desc": sprintf("%s (ID: %d)", display_name, contact.id),
                        "value": contact.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available companies for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCompanies() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("companies", {"limit": DefaultPageSize});
            if (response.companies) {
                foreach hash<auto> company in (response.companies) {
                    string display_name = company.company_name
                        ?? sprintf("Company %d", company.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": "",
                        "desc": sprintf("%s (ID: %d)", display_name, company.id),
                        "value": company.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available tags for reference data
    private *list<hash<AllowedValueInfo>> getReferenceTags() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("tags", {"limit": DefaultPageSize});
            if (response.tags) {
                foreach hash<auto> tag in (response.tags) {
                    string display_name = tag.name ?? sprintf("Tag %d", tag.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": tag.category.name ?? "",
                        "desc": sprintf("%s (ID: %d)", display_name, tag.id),
                        "value": tag.id,
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
            hash<auto> response = doGet("opportunities", {"limit": DefaultPageSize});
            if (response.opportunities) {
                foreach hash<auto> opp in (response.opportunities) {
                    string display_name = opp.opportunity_title
                        ?? sprintf("Opportunity %d", opp.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": opp.stage.name ?? "",
                        "desc": sprintf("%s (ID: %d)", display_name, opp.id),
                        "value": opp.id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Formats a date for Keap API parameters (ISO 8601)
    static string formatDateTime(date dt) {
        return dt.format("YYYY-MM-DDTHH:mm:ss.xxxZ");
    }
}

} # namespace KeapDataProvider
