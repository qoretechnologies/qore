# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ApolloDataProvider module definition

/*  ApolloDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ApolloDataProvider module
public namespace ApolloDataProvider {

#! Base class for Apollo data providers
public class ApolloDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Apollo data providers
        const BaseConstructorOptions = {
            "apollorestclient": <DataProviderOptionInfo>{
                "display_name": "Apollo REST Client",
                "short_desc": "The ApolloRestClient object",
                "type": AbstractDataProviderType::get(
                    new Type("ApolloRestClient::ApolloRestClient")),
                "desc": "The ApolloRestClient object to use for API calls",
            },
            "apollorestclient_options": <DataProviderOptionInfo>{
                "display_name": "Apollo REST Client Options",
                "short_desc": "Options for creating an ApolloRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating an ApolloRestClient object if one is not provided "
                    "directly",
            },
        };
    }

    private {
        #! The REST client object for API calls
        ApolloRestClient::ApolloRestClient rest;

        #! The connection object, if applicable
        *ApolloRestClient::ApolloRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(ApolloRestClient::ApolloRestClient rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(ApolloRestClient::ApolloRestConnection conn) {
        self.conn = conn;
        rest = conn.get(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.apollorestclient) {
            rest = copts.apollorestclient;
        } else {
            hash<auto> client_opts = copts.apollorestclient_options ?? {};
            rest = new ApolloRestClient::ApolloRestClient(client_opts);
        }
    }

    #! Returns the REST client
    ApolloRestClient::ApolloRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *ApolloRestClient::ApolloRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Makes a GET request to the Apollo API
    /** @param path the API path
        @param params optional query parameters
        @return the response body

        @throw APOLLO-API-ERROR if the response indicates an error
    */
    hash<auto> doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a POST request to the Apollo API
    /** @param path the API path
        @param body the request body
        @return the response body

        @throw APOLLO-API-ERROR if the response indicates an error
    */
    hash<auto> doPost(string path, *hash<auto> body) {
        return doRestRequest("POST", path, body);
    }

    #! Makes a PUT request to the Apollo API
    /** @param path the API path
        @param body the request body
        @return the response body

        @throw APOLLO-API-ERROR if the response indicates an error
    */
    hash<auto> doPut(string path, *hash<auto> body) {
        return doRestRequest("PUT", path, body);
    }

    #! Makes a PATCH request to the Apollo API
    /** @param path the API path
        @param body the request body
        @return the response body

        @throw APOLLO-API-ERROR if the response indicates an error
    */
    hash<auto> doPatch(string path, *hash<auto> body) {
        return doRestRequest("PATCH", path, body);
    }

    #! Makes a DELETE request to the Apollo API
    /** @param path the API path
        @return the response body

        @throw APOLLO-API-ERROR if the response indicates an error
    */
    hash<auto> doDelete(string path) {
        return doRestRequest("DELETE", path);
    }

    #! Makes a POST-based search request to the Apollo API
    /** @param path the API path for the search endpoint
        @param body the request body with search criteria
        @param result_key the key in the response containing the result list
        @param limit optional maximum number of results to return
        @return a list of all result records

        Apollo search endpoints use POST with page/per_page in the request body.

        @throw APOLLO-API-ERROR if the response indicates an error
    */
    list<hash<auto>> doPostSearch(string path, hash<auto> body, string result_key, *int limit) {
        return fetchAllPagesPost(path, result_key, body, limit);
    }

    #! Fetches all pages from a GET endpoint using page-based pagination
    /** @param path the API path
        @param result_key the key in the response containing the result list
        @param params optional query parameters
        @param limit optional maximum number of results to return
        @return a list of all result records

        Apollo GET search endpoints use `page` and `per_page` query parameters
        for pagination.
    */
    list<hash<auto>> fetchAllPages(string path, string result_key, *hash<auto> params,
            *int limit, *string extra_qs) {
        list<hash<auto>> results = ();
        int page = 1;
        int page_size = limit ?? DefaultPageSize;
        while (True) {
            hash qparams = cast<hash>(params ?? {});
            qparams.page = page;
            qparams.per_page = page_size;
            string url = path;
            url += "?" + buildQueryString(qparams);
            if (extra_qs) {
                url += "&" + extra_qs;
            }
            hash<auto> response = doRestRequest("GET", url);
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
            ++page;
        }
        return results;
    }

    #! Fetches all pages from a POST-based search endpoint using page-based pagination
    /** @param path the API path for the search endpoint
        @param result_key the key in the response containing the result list
        @param body optional request body with search criteria
        @param limit optional maximum number of results to return
        @return a list of all result records

        Apollo search endpoints use POST requests with `page` and `per_page` parameters
        in the request body for pagination.
    */
    list<hash<auto>> fetchAllPagesPost(string path, string result_key, *hash<auto> body,
            *int limit) {
        list<hash<auto>> results = ();
        int page = 1;
        int page_size = limit ?? DefaultPageSize;
        while (True) {
            hash<auto> request_body = body ?? {};
            request_body.page = page;
            request_body.per_page = page_size;
            hash<auto> response = doRestRequest("POST", path, request_body);
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
            ++page;
        }
        return results;
    }

    #! Makes a REST request with retry and rate limit handling
    /** @param method the HTTP method
        @param path the API path
        @param body optional request body
        @return the response body hash

        Apollo API error responses contain an "error" field at the top level:
        @code
        {"error": "..."}
        @endcode

        @throw APOLLO-API-ERROR if the response indicates an error
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
                        throw "APOLLO-ERROR", sprintf("unsupported HTTP method: %s", method);
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

    #! Checks the Apollo API response for errors
    /** @param response the API response hash

        Apollo returns errors as:
        @code
        {"error": "Error description"}
        @endcode

        @throw APOLLO-API-ERROR if the response contains errors
    */
    private checkResponse(hash<auto> response) {
        if (response.body.error) {
            string err_msg = response.body.error;
            throw "APOLLO-API-ERROR", err_msg, response.body;
        }
    }

    #! Returns information on supported reference data
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "contacts": True,
            "accounts": True,
            "deals": True,
            "users": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "contacts": return getReferenceContacts();
            case "accounts": return getReferenceAccounts();
            case "deals": return getReferenceDeals();
            case "users": return getReferenceUsers();
        }
    }

    #! Returns available contacts for reference data
    /** Apollo contacts are fetched via POST to `contacts/search` */
    private *list<hash<AllowedValueInfo>> getReferenceContacts() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doPost("contacts/search", {"per_page": 100});
            if (response.contacts) {
                foreach hash<auto> contact in (response.contacts) {
                    string display_name = trim(sprintf("%s %s",
                        contact.first_name ?? "", contact.last_name ?? ""));
                    if (!display_name.val()) {
                        display_name = contact.email
                            ?? sprintf("Contact %s", contact.id);
                    }
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": contact.email ?? "",
                        "desc": sprintf("%s (ID: %s)", display_name, contact.id),
                        "value": string(contact.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available accounts for reference data
    /** Apollo accounts are fetched via POST to `accounts/search` */
    private *list<hash<AllowedValueInfo>> getReferenceAccounts() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doPost("accounts/search", {"per_page": 100});
            if (response.accounts) {
                foreach hash<auto> account in (response.accounts) {
                    string display_name = account.name
                        ?? sprintf("Account %s", account.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": account.domain ?? "",
                        "desc": sprintf("%s (ID: %s)", display_name, account.id),
                        "value": string(account.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available deals for reference data
    /** Apollo deals/opportunities are fetched via GET from `opportunities/search` */
    private *list<hash<AllowedValueInfo>> getReferenceDeals() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("opportunities/search");
            if (response.opportunities) {
                foreach hash<auto> deal in (response.opportunities) {
                    string display_name = deal.name
                        ?? sprintf("Deal %s", deal.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": deal.status ?? "",
                        "desc": sprintf("%s (ID: %s)", display_name, deal.id),
                        "value": string(deal.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available users for reference data
    /** Apollo users are fetched via GET from `users/search` */
    private *list<hash<AllowedValueInfo>> getReferenceUsers() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("users/search");
            if (response.users) {
                foreach hash<auto> user in (response.users) {
                    string display_name = trim(sprintf("%s %s",
                        user.first_name ?? "", user.last_name ?? ""));
                    if (!display_name.val()) {
                        display_name = user.email
                            ?? sprintf("User %s", user.id);
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
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Formats a date for Apollo API parameters (ISO 8601)
    static string formatDateTime(date dt) {
        return dt.format("YYYY-MM-DDTHH:mm:ss.xxxZ");
    }
}

} # namespace ApolloDataProvider
