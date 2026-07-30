# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file QlipDataProviderBase.qc base class for Qlip data providers

/*  QlipDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the QlipDataProvider module
public namespace QlipDataProvider {

#! Base class for Qlip data providers
public class QlipDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Qlip data providers
        const BaseConstructorOptions = {
            "qliprestclient": <DataProviderOptionInfo>{
                "display_name": "Qlip REST Client",
                "short_desc": "The QlipRestClient object",
                "type": AbstractDataProviderType::get(
                    new Type("QlipRestClient::QlipRestClient")),
                "desc": "The QlipRestClient object to use for API calls",
            },
            "qliprestclient_options": <DataProviderOptionInfo>{
                "display_name": "Qlip REST Client Options",
                "short_desc": "Options for creating a QlipRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a QlipRestClient object if one is not provided "
                    "directly",
            },
        };
    }

    private {
        #! The REST client object for API calls
        QlipRestClient::QlipRestClient rest;

        #! The connection object, if applicable
        *QlipRestClient::QlipRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(QlipRestClient::QlipRestClient rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(QlipRestClient::QlipRestConnection conn) {
        self.conn = conn;
        rest = conn.get(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.qliprestclient) {
            rest = copts.qliprestclient;
        } else {
            # Only pass known-safe options to the REST client to prevent callers from
            # disabling TLS verification or injecting unexpected configuration
            hash<auto> all_client_opts = copts.qliprestclient_options ?? {};
            hash<auto> client_opts = all_client_opts{
                "token", "url", "proxy", "timeout", "connect_timeout",
                "http_version", "max_redirects", "headers",
            };
            rest = new QlipRestClient::QlipRestClient(client_opts);
        }
    }

    #! Returns the REST client
    QlipRestClient::QlipRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *QlipRestClient::QlipRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Makes a GET request to the Qlip API
    /** @param path the API path (relative to /api)
        @param params optional query parameters
        @return the response body

        @throw QLIP-API-ERROR if the response indicates an error
    */
    hash<auto> doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a POST request to the Qlip API
    /** @param path the API path (relative to /api)
        @param body the request body
        @return the response body

        @throw QLIP-API-ERROR if the response indicates an error
    */
    hash<auto> doPost(string path, *hash<auto> body) {
        return doRestRequest("POST", path, body);
    }

    #! Formats a date/time as an ISO 8601 string for Qlip API date params
    static string formatDateTime(date dt) {
        return dt.format("YYYY-MM-DDTHH:mm:ssZ");
    }

    #! Fetches all pages from a page/pageSize-paginated GET endpoint
    /** @param path the API path
        @param result_key the key in the response body containing the result list (e.g. \c "builds")
        @param params optional query parameters
        @param limit optional maximum number of results to return
        @return a list of all result records

        Qlip list endpoints return \c {"<result_key>": [...], "total": N, "page": P, "pageSize": S}
        and use 1-based \c page / \c pageSize pagination.
    */
    list<hash<auto>> fetchAllPages(string path, string result_key, *hash<auto> params, *int limit) {
        list<hash<auto>> results = ();
        int page = 1;
        int page_size = DefaultPageSize;
        if (limit && limit < page_size) {
            page_size = limit;
        }

        while (True) {
            hash<auto> qparams = (params ?? {}) + {
                "page": page,
                "pageSize": page_size,
            };
            hash<auto> response = doGet(path, qparams);

            list<auto> page_data = response{result_key} ?? ();
            if (!page_data) {
                break;
            }
            results += page_data;
            if (limit && results.size() >= limit) {
                results = results[0..limit-1];
                break;
            }
            # Stop when we have all rows (per "total") or a short page signals the last page
            int total = response.total ?? 0;
            if (total ? results.size() >= total : page_data.size() < page_size) {
                break;
            }
            ++page;
        }
        return results;
    }

    #! Makes a REST request with retry handling
    /** @param method the HTTP method
        @param path the API path
        @param body optional request body
        @return the response body hash

        @throw QLIP-API-ERROR if the response indicates an error
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
                    case "PATCH":
                        response = rest.patch(path, body, \info);
                        break;
                    case "DELETE":
                        response = rest.del(path, NOTHING, \info);
                        break;
                    default:
                        throw "QLIP-ERROR", sprintf("unsupported HTTP method: %s", method);
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
    private bool retryRequest(hash<ExceptionInfo> ex, reference<int> retries) {
        if (retries >= MaxRetries) {
            return False;
        }
        if (RetrySet{ex.err}) {
            ++retries;
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
}

} # namespace QlipDataProvider
