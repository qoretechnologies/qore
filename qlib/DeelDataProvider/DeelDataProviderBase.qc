# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelDataProviderBase.qc base class for Deel data providers

/*  DeelDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DeelDataProvider module
public namespace DeelDataProvider {

#! Base class for Deel data providers
public class DeelDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Deel data providers
        const BaseConstructorOptions = {
            "deelrestclient": <DataProviderOptionInfo>{
                "display_name": "Deel REST Client",
                "short_desc": "The DeelRestClient object",
                "type": AbstractDataProviderType::get(
                    new Type("DeelRestClient::DeelRestClient")),
                "desc": "The DeelRestClient object to use for API calls",
            },
            "deelrestclient_options": <DataProviderOptionInfo>{
                "display_name": "Deel REST Client Options",
                "short_desc": "Options for creating a DeelRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a DeelRestClient object if one is not provided "
                    "directly",
            },
        };
    }

    private {
        #! The REST client object for API calls
        DeelRestClient::DeelRestClient rest;

        #! The connection object, if applicable
        *DeelRestClient::DeelRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(DeelRestClient::DeelRestClient rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(DeelRestClient::DeelRestConnection conn) {
        self.conn = conn;
        rest = conn.get(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.deelrestclient) {
            rest = copts.deelrestclient;
        } else {
            # Only pass known-safe options to the REST client to prevent callers from
            # disabling TLS verification or injecting unexpected configuration
            hash<auto> all_client_opts = copts.deelrestclient_options ?? {};
            hash<auto> client_opts = all_client_opts{
                "apikey", "url", "proxy", "timeout", "connect_timeout",
                "http_version", "max_redirects", "headers",
            };
            rest = new DeelRestClient::DeelRestClient(client_opts);
        }
    }

    #! Returns the REST client
    DeelRestClient::DeelRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *DeelRestClient::DeelRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Makes a GET request to the Deel API
    /** @param path the API path (relative to /rest/v2)
        @param params optional query parameters
        @return the response body

        @throw DEEL-API-ERROR if the response indicates an error
    */
    hash<auto> doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a POST request to the Deel API
    /** @param path the API path (relative to /rest/v2)
        @param body the request body
        @return the response body

        @throw DEEL-API-ERROR if the response indicates an error
    */
    hash<auto> doPost(string path, *hash<auto> body) {
        return doRestRequest("POST", path, body);
    }

    #! Makes a PATCH request to the Deel API
    /** @param path the API path (relative to /rest/v2)
        @param body the request body
        @return the response body

        @throw DEEL-API-ERROR if the response indicates an error
    */
    hash<auto> doPatch(string path, *hash<auto> body) {
        return doRestRequest("PATCH", path, body);
    }

    #! Makes a DELETE request to the Deel API
    /** @param path the API path (relative to /rest/v2)
        @return the response body

        @throw DEEL-API-ERROR if the response indicates an error
    */
    hash<auto> doDelete(string path) {
        return doRestRequest("DELETE", path);
    }

    #! Fetches all pages from a GET endpoint using offset-based pagination
    /** @param path the API path
        @param result_key the key in the response containing the result list (used only when
               response.data is a hash, not a list)
        @param params optional query parameters
        @param limit optional maximum number of results to return
        @return a list of all result records
    */
    list<hash<auto>> fetchAllPages(string path, string result_key, *hash<auto> params, *int limit) {
        list<hash<auto>> results = ();
        int offset = 0;
        int page_size = DefaultPageSize;
        if (limit && limit < page_size) {
            page_size = limit;
        }

        while (True) {
            hash<auto> qparams = (params ?? {}) + {
                "limit": page_size,
                "offset": offset,
            };
            hash<auto> response = doGet(path, qparams);

            # Deel API returns data as a list directly in response.data,
            # or occasionally as a hash with a named key (legacy endpoints)
            list<auto> page_data;
            if (response.data.typeCode() == NT_LIST) {
                page_data = response.data;
            } else if (response.data.typeCode() == NT_HASH) {
                page_data = response.data{result_key} ?? ();
            } else {
                page_data = response{result_key} ?? ();
            }

            if (!page_data) {
                break;
            }
            results += page_data;
            if (limit && results.size() >= limit) {
                results = results[0..limit-1];
                break;
            }
            # Check if there are more pages: total can be in page.total_rows,
            # data.meta.total, or meta.total; otherwise stop when page is smaller
            # than the requested page_size (a partial page means last page)
            int total = response.page.total_rows ?? response.data.meta.total
                ?? response.meta.total ?? 0;
            if (total ? results.size() >= total : page_data.size() < page_size) {
                break;
            }
            offset += page_size;
        }
        return results;
    }

    #! Formats a date/time for Deel API parameters (ISO 8601)
    static string formatDateTime(date dt) {
        return dt.format("YYYY-MM-DDTHH:mm:ssZ");
    }

    #! Formats a date for Deel API date-only parameters (YYYY-MM-DD)
    static string formatDate(date dt) {
        return dt.format("YYYY-MM-DD");
    }

    #! Makes a REST request with retry handling
    /** @param method the HTTP method
        @param path the API path
        @param body optional request body
        @return the response body hash

        Deel API error responses contain an "errors" array or "error" object.

        @throw DEEL-API-ERROR if the response indicates an error
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
                        throw "DEEL-ERROR", sprintf("unsupported HTTP method: %s", method);
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

    #! Returns information on supported reference data
    /** @return a hash of supported reference data types
    */
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "contracts": True,
            "workers": True,
            "timesheets": True,
            "adjustments": True,
            "time_off_types": True,
            "time_off_requests": True,
        };
    }

    #! Returns reference data of the given kind if available
    /** @param type the unique type name of the reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return a list of allowed values for this data
    */
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "contracts": return getReferenceContracts();
            case "workers": return getReferenceWorkers();
            case "timesheets": return getReferenceTimesheets();
            case "adjustments": return getReferenceAdjustments();
            case "time_off_types": return getReferenceTimeOffTypes();
            case "time_off_requests": return getReferenceTimeOffRequests();
        }
    }

    #! Returns available contracts for reference data
    private *list<hash<AllowedValueInfo>> getReferenceContracts() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> records = fetchAllPages("contracts", "contracts", NOTHING, DefaultPageSize);
            foreach hash<auto> record in (records) {
                string id = string(record.id);
                string display_name = record.title ?? id;
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": record.status ?? "unknown",
                    "desc": sprintf("%s (Status: %s)", display_name, record.status ?? "unknown"),
                    "value": id,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceContracts: error fetching contracts: %s", ex.desc);
        }
        return rv;
    }

    #! Returns available workers for reference data
    private *list<hash<AllowedValueInfo>> getReferenceWorkers() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> records = fetchAllPages("people", "people", NOTHING, DefaultPageSize);
            foreach hash<auto> record in (records) {
                string id = string(record.id);
                string display_name = (record.first_name ?? "") + " " + (record.last_name ?? "");
                display_name = trim(display_name);
                if (!display_name.val()) {
                    display_name = record.email ?? id;
                }
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": record.email ?? id,
                    "desc": sprintf("%s (%s)", display_name, record.email ?? id),
                    "value": id,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceWorkers: error fetching workers: %s", ex.desc);
        }
        return rv;
    }

    #! Returns available timesheets for reference data
    private *list<hash<AllowedValueInfo>> getReferenceTimesheets() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> records = fetchAllPages("timesheets", "timesheets", NOTHING, DefaultPageSize);
            foreach hash<auto> record in (records) {
                string id = string(record.id);
                string display_name = sprintf("Timesheet %s - %s", id, record.date ?? "unknown");
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": record.status ?? "unknown",
                    "desc": sprintf("%s (Status: %s)", display_name, record.status ?? "unknown"),
                    "value": id,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceTimesheets: error fetching timesheets: %s", ex.desc);
        }
        return rv;
    }

    #! Returns available invoice adjustments for reference data
    private *list<hash<AllowedValueInfo>> getReferenceAdjustments() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> records = fetchAllPages("invoice-adjustments", "adjustments", NOTHING, DefaultPageSize);
            foreach hash<auto> record in (records) {
                string id = string(record.id);
                string display_name = sprintf("Adjustment %s - %s", id, record.reason ?? "no reason");
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": record.status ?? "unknown",
                    "desc": sprintf("%s (Status: %s)", display_name, record.status ?? "unknown"),
                    "value": id,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceAdjustments: error fetching adjustments: %s", ex.desc);
        }
        return rv;
    }

    #! Returns available time-off types/entitlements for reference data
    private *list<hash<AllowedValueInfo>> getReferenceTimeOffTypes() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            # time_offs uses pageSize and cursor-based pagination
            hash<auto> response = doGet("time_offs", {"pageSize": DefaultPageSize});
            list<auto> records = response{"data"} ?? ();
            foreach hash<auto> record in (records) {
                string id = string(record.id);
                string display_name = record.name ?? id;
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": record.type ?? "unknown",
                    "desc": sprintf("%s (%s)", display_name, record.type ?? "unknown"),
                    "value": id,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceTimeOffTypes: error fetching time-off types: %s", ex.desc);
        }
        return rv;
    }

    #! Returns available time-off requests for reference data
    private *list<hash<AllowedValueInfo>> getReferenceTimeOffRequests() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            # time_offs uses pageSize and cursor-based pagination
            hash<auto> response = doGet("time_offs", {"pageSize": DefaultPageSize});
            list<auto> records = response{"data"} ?? ();
            foreach hash<auto> record in (records) {
                string id = string(record.id);
                string display_name = sprintf("Time-Off Request %s (%s to %s)",
                    id, record.start_date ?? "?", record.end_date ?? "?");
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": record.status ?? "unknown",
                    "desc": sprintf("%s (Status: %s)", display_name, record.status ?? "unknown"),
                    "value": id,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getReferenceTimeOffRequests: error fetching requests: %s",
                ex.desc);
        }
        return rv;
    }

    #! Checks the Deel API response for errors
    /** @param response the API response hash

        Deel returns errors in various formats depending on the endpoint.

        @throw DEEL-API-ERROR if the response contains errors
    */
    private checkResponse(hash<auto> response) {
        # Check for errors array
        if (response.body.errors) {
            list<auto> errors = response.body.errors;
            if (errors.size() > 0) {
                string err_msg;
                foreach auto error in (errors) {
                    if (err_msg.val()) {
                        err_msg += "; ";
                    }
                    if (error.typeCode() == NT_HASH) {
                        err_msg += error.message ?? error.detail ?? error.code ?? "Unknown error";
                    } else {
                        err_msg += string(error);
                    }
                }
                throw "DEEL-API-ERROR", err_msg, response.body;
            }
        }
        # Check for single error object
        if (response.body.error) {
            auto err = response.body.error;
            string err_msg;
            if (err.typeCode() == NT_HASH) {
                err_msg = err.message ?? err.detail ?? "API error";
            } else {
                err_msg = string(err);
            }
            throw "DEEL-API-ERROR", err_msg, response.body;
        }
    }
}

} # namespace DeelDataProvider
