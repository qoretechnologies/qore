# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SmartSheetDataProvider module definition

/*  SmartSheetDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SmartSheetDataProvider module
public namespace SmartSheetDataProvider {

#! Base class for SmartSheet data providers
public class SmartSheetDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for SmartSheet data providers
        const BaseConstructorOptions = {
            "smartsheetrestclient": <DataProviderOptionInfo>{
                "display_name": "Smartsheet REST Client",
                "short_desc": "The SmartSheetRestClientIo object",
                "type": AbstractDataProviderType::get(
                    new Type("RestClientIo::RestClientIo")),
                "desc": "The RestClientIo object to use for API calls",
            },
            "smartsheetrestclient_options": <DataProviderOptionInfo>{
                "display_name": "Smartsheet REST Client Options",
                "short_desc": "Options for creating a SmartSheetRestClientIo",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a SmartSheetRestClientIo object if one is not "
                    "provided directly",
            },
        };
    }

    private {
        #! The REST client object for API calls (async RestClientIo)
        RestClientIo::RestClientIo rest;

        #! The connection object, if applicable
        *SmartSheetRestClient::SmartSheetRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client (async)
    constructor(RestClientIo::RestClientIo rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(SmartSheetRestClient::SmartSheetRestConnection conn) {
        self.conn = conn;
        rest = conn.getAsync(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.smartsheetrestclient) {
            rest = copts.smartsheetrestclient;
        } else {
            hash<auto> client_opts = copts.smartsheetrestclient_options ?? {};
            rest = new SmartSheetRestClient::SmartSheetRestClientIo(client_opts);
        }
    }

    #! Returns the REST client
    RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *SmartSheetRestClient::SmartSheetRestConnection getConnection() {
        return conn;
    }

    #! Makes a GET request to the Smartsheet API
    hash<auto> doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a POST request to the Smartsheet API
    hash<auto> doPost(string path, auto body = NOTHING) {
        return doRestRequest("POST", path, body);
    }

    #! Makes a PUT request to the Smartsheet API
    hash<auto> doPut(string path, auto body = NOTHING) {
        return doRestRequest("PUT", path, body);
    }

    #! Makes a DELETE request to the Smartsheet API
    hash<auto> doDelete(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("DELETE", url);
    }

    #! Fetches all pages from a GET endpoint using offset-based pagination
    /** @param path the API path
        @param result_key the key in the response containing the result list
        @param params optional query parameters
        @param limit optional maximum number of results to return
        @return a list of all result records
    */
    list<hash<auto>> fetchAllPages(string path, string result_key, *hash<auto> params,
            *int limit) {
        list<hash<auto>> results = ();
        int page = 1;
        int page_size = limit && limit < DefaultPageSize ? limit : DefaultPageSize;

        while (True) {
            hash<auto> qparams = params ?? {};
            qparams.page = page;
            qparams.pageSize = page_size;

            hash<auto> response = doGet(path, qparams);
            if (response{result_key}) {
                results += response{result_key};
            }
            if (limit && results.size() >= limit) {
                results = results[0..limit-1];
                break;
            }
            # Check for more pages
            if (!response.totalPages || page >= response.totalPages) {
                break;
            }
            ++page;
        }
        return results;
    }

    #! Makes a REST request with retry and rate limit handling
    private hash<auto> doRestRequest(string method, string path, auto body = NOTHING) {
        int retries = 0;
        while (True) {
            try {
                hash<auto> response;
                switch (method) {
                    case "GET":
                        response = rest.restGet(path);
                        break;
                    case "POST":
                        response = rest.restPost(path, body);
                        break;
                    case "PUT":
                        response = rest.restPut(path, body);
                        break;
                    case "DELETE":
                        response = rest.restDel(path);
                        break;
                    default:
                        throw "SMARTSHEET-ERROR", sprintf("unsupported HTTP method: %s", method);
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
        # Retry on certain socket errors
        if (RetrySet{ex.err}) {
            ++retries;
            sleep(pow(2, retries) * 1000ms);
            return True;
        }
        # Retry on rate limit errors (HTTP 429) - Smartsheet recommends waiting 60 seconds
        if (ex.err == "REST-RESPONSE-ERROR" && ex.arg.status_code == 429) {
            ++retries;
            *string retry_after = ex.arg.headers."retry-after";
            int wait_secs = retry_after.val() ? retry_after.toInt() : 60;
            if (wait_secs < 1 || wait_secs > 120) {
                wait_secs = 60;
            }
            sleep(wait_secs * 1000ms);
            return True;
        }
        return False;
    }

    #! Builds a query string from a hash of parameters
    static string buildQueryString(hash<auto> params) {
        list<string> parts;
        foreach string key in (keys params) {
            auto val = params{key};
            if (exists val) {
                string sval = string(val);
                sval = replace(sval, "%", "%25");
                sval = replace(sval, "&", "%26");
                sval = replace(sval, "=", "%3D");
                sval = replace(sval, "#", "%23");
                sval = replace(sval, "+", "%2B");
                sval = replace(sval, " ", "%20");
                parts += sprintf("%s=%s", key, sval);
            }
        }
        return parts.join("&");
    }

    #! Returns information on supported reference data
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "sheets": True,
            "workspaces": True,
            "folders": True,
            "reports": True,
            "users": True,
            "groups": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type,
            *hash<auto> action_opts) {
        switch (type) {
            case "sheets": return getReferenceSheets();
            case "workspaces": return getReferenceWorkspaces();
            case "folders": return getReferenceFolders();
            case "reports": return getReferenceReports();
            case "users": return getReferenceUsers();
            case "groups": return getReferenceGroups();
        }
    }

    #! Returns available sheets for reference data
    private *list<hash<AllowedValueInfo>> getReferenceSheets() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> sheets = fetchAllPages("sheets", "data", NOTHING, 200);
            foreach hash<auto> sheet in (sheets) {
                rv += <AllowedValueInfo>{
                    "display_name": sheet.name,
                    "short_desc": string(sheet.id),
                    "desc": sprintf("%s (ID: %s)", sheet.name, sheet.id),
                    "value": string(sheet.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch sheets reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available workspaces for reference data
    private *list<hash<AllowedValueInfo>> getReferenceWorkspaces() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> workspaces = fetchAllPages("workspaces", "data", NOTHING, 200);
            foreach hash<auto> ws in (workspaces) {
                rv += <AllowedValueInfo>{
                    "display_name": ws.name,
                    "short_desc": string(ws.id),
                    "desc": sprintf("%s (ID: %s)", ws.name, ws.id),
                    "value": string(ws.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch workspaces reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available folders for reference data
    private *list<hash<AllowedValueInfo>> getReferenceFolders() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("home/folders");
            if (response.data) {
                foreach hash<auto> folder in (response.data) {
                    rv += <AllowedValueInfo>{
                        "display_name": folder.name,
                        "short_desc": string(folder.id),
                        "desc": sprintf("%s (ID: %s)", folder.name, folder.id),
                        "value": string(folder.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch folders reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available reports for reference data
    private *list<hash<AllowedValueInfo>> getReferenceReports() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> reports = fetchAllPages("reports", "data", NOTHING, 200);
            foreach hash<auto> report in (reports) {
                rv += <AllowedValueInfo>{
                    "display_name": report.name,
                    "short_desc": string(report.id),
                    "desc": sprintf("%s (ID: %s)", report.name, report.id),
                    "value": string(report.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch reports reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available users for reference data
    private *list<hash<AllowedValueInfo>> getReferenceUsers() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> users = fetchAllPages("users", "data", NOTHING, 200);
            foreach hash<auto> user in (users) {
                string display = user.firstName && user.lastName
                    ? sprintf("%s %s (%s)", user.firstName, user.lastName, user.email)
                    : user.email;
                rv += <AllowedValueInfo>{
                    "display_name": display,
                    "short_desc": string(user.id),
                    "value": string(user.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch users reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available groups for reference data
    private *list<hash<AllowedValueInfo>> getReferenceGroups() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("groups");
            if (response.data) {
                foreach hash<auto> group in (response.data) {
                    rv += <AllowedValueInfo>{
                        "display_name": group.name,
                        "short_desc": string(group.id),
                        "desc": sprintf("%s (ID: %s)", group.name, group.id),
                        "value": string(group.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch groups reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }
}

} # namespace SmartSheetDataProvider
