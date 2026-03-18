# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedditDataProvider module definition

/*  RedditDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the RedditDataProvider module
public namespace RedditDataProvider {

#! Base class for Reddit data providers
public class RedditDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Reddit data providers
        const BaseConstructorOptions = {
            "redditrestclient": <DataProviderOptionInfo>{
                "display_name": "Reddit REST Client",
                "short_desc": "The RedditRestClient object",
                "type": AbstractDataProviderType::get(
                    new Type("RedditRestClient::RedditRestClient")),
                "desc": "The RedditRestClient object to use for API calls",
            },
            "redditrestclient_options": <DataProviderOptionInfo>{
                "display_name": "Reddit REST Client Options",
                "short_desc": "Options for creating a RedditRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a RedditRestClient object if one is not provided "
                    "directly",
            },
        };
    }

    private {
        #! The REST client object for API calls
        RedditRestClient::RedditRestClient rest;

        #! The connection object, if applicable
        *RedditRestClient::RedditRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(RedditRestClient::RedditRestClient rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(RedditRestClient::RedditRestConnection conn) {
        self.conn = conn;
        rest = conn.get(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.redditrestclient) {
            rest = copts.redditrestclient;
        } else {
            hash<auto> client_opts = copts.redditrestclient_options ?? {};
            rest = new RedditRestClient::RedditRestClient(client_opts);
        }
    }

    #! Returns the REST client
    RedditRestClient::RedditRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *RedditRestClient::RedditRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Makes a GET request to the Reddit API
    /** @param path the API path
        @param params optional query parameters
        @return the response body

        @throw REDDIT-API-ERROR if the response indicates an error
    */
    hash<auto> doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a POST request to the Reddit API with form-encoded body
    /** @param path the API path
        @param body the request body (sent as form-encoded)
        @return the response body

        @throw REDDIT-API-ERROR if the response indicates an error
    */
    hash<auto> doPost(string path, *hash<auto> body) {
        return doRestRequest("POST", path, body);
    }

    #! Makes a PUT request to the Reddit API
    /** @param path the API path
        @param body the request body
        @return the response body

        @throw REDDIT-API-ERROR if the response indicates an error
    */
    hash<auto> doPut(string path, *hash<auto> body) {
        return doRestRequest("PUT", path, body);
    }

    #! Makes a PATCH request to the Reddit API
    /** @param path the API path
        @param body the request body
        @return the response body

        @throw REDDIT-API-ERROR if the response indicates an error
    */
    hash<auto> doPatch(string path, *hash<auto> body) {
        return doRestRequest("PATCH", path, body);
    }

    #! Makes a DELETE request to the Reddit API
    /** @param path the API path
        @return the response body

        @throw REDDIT-API-ERROR if the response indicates an error
    */
    hash<auto> doDelete(string path) {
        return doRestRequest("DELETE", path);
    }

    #! Fetches all pages from a GET endpoint using Reddit's Listing pagination
    /** Reddit returns paginated data in the format:
        @code
        {"kind": "Listing", "data": {"children": [{"kind": "t3", "data": {...}}], "after": "t3_xxx"}}
        @endcode

        @param path the API path
        @param params optional query parameters
        @param limit optional maximum number of results to return
        @return a list of all result records (unwrapped from children[].data)
    */
    list<hash<auto>> fetchAllPages(string path, *hash<auto> params, *int limit) {
        list<hash<auto>> results = ();
        *string after;
        while (True) {
            hash<auto> qparams = params ?? {};
            qparams.limit = DefaultPageSize;
            if (after) {
                qparams.after = after;
            }
            hash<auto> response = doGet(path, qparams);
            # Unwrap Reddit Listing format
            *list<auto> children = response.data.children;
            if (children) {
                foreach auto child in (children) {
                    results += child.data;
                }
            }
            if (limit && results.size() >= limit) {
                results = results[0..limit-1];
                break;
            }
            after = response.data.after;
            if (!after) {
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

        @throw REDDIT-API-ERROR if the response indicates an error
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
                        throw "REDDIT-ERROR", sprintf("unsupported HTTP method: %s", method);
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

    #! Checks the Reddit API response for errors
    /** @param response the API response hash

        Reddit returns errors as:
        @code
        {"error": 403, "message": "Forbidden"}
        @endcode
        or
        @code
        {"json": {"errors": [["ERROR_CODE", "message", "field"]]}}
        @endcode

        @throw REDDIT-API-ERROR if the response contains errors
    */
    private checkResponse(hash<auto> response) {
        # Check for top-level error field
        if (response.body.error) {
            string err_msg = response.body.message ?? string(response.body.error);
            throw "REDDIT-API-ERROR", err_msg, response.body;
        }
        # Check for JSON errors array (returned by action endpoints)
        if (response.body.json.errors && response.body.json.errors.size() > 0) {
            string err_msg;
            foreach auto error in (response.body.json.errors) {
                if (err_msg.val()) {
                    err_msg += "; ";
                }
                # Each error is a list: [code, message, field]
                if (error.typeCode() == NT_LIST && error.size() >= 2) {
                    err_msg += sprintf("%s: %s", error[0], error[1]);
                } else {
                    err_msg += string(error);
                }
            }
            throw "REDDIT-API-ERROR", err_msg, response.body;
        }
    }

    #! Returns information on supported reference data
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "subreddits": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "subreddits": return getReferenceSubreddits();
        }
    }

    #! Returns subscribed subreddits for reference data
    private *list<hash<AllowedValueInfo>> getReferenceSubreddits() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> subs = fetchAllPages("subreddits/mine/subscriber", NOTHING, 100);
            foreach hash<auto> sub in (subs) {
                rv += <AllowedValueInfo>{
                    "display_name": sub.display_name ?? sub.name,
                    "short_desc": sub.title ?? "",
                    "desc": sprintf("%s - %s", sub.display_name ?? sub.name,
                        sub.public_description ?? ""),
                    "value": sub.display_name,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }
}

} # namespace RedditDataProvider
