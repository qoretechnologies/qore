# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file TableauDataProviderBase.qc base class for Tableau data providers

/*  TableauDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the TableauDataProvider module
public namespace TableauDataProvider {

#! Base class for Tableau data providers
public class TableauDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Tableau data providers
        const BaseConstructorOptions = {
            "tableaurestclient": <DataProviderOptionInfo>{
                "display_name": "Tableau REST Client",
                "short_desc": "The TableauRestClient object",
                "type": AbstractDataProviderType::get(
                    new Type("TableauRestClient::TableauRestClient")),
                "desc": "The TableauRestClient object to use for API calls",
            },
            "tableaurestclient_options": <DataProviderOptionInfo>{
                "display_name": "Tableau REST Client Options",
                "short_desc": "Options for creating a TableauRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a TableauRestClient object if one is not provided "
                    "directly",
            },
        };
    }

    private {
        #! The REST client object for API calls
        TableauRestClient::TableauRestClient rest;

        #! The connection object, if applicable
        *TableauRestClient::TableauRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(TableauRestClient::TableauRestClient rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(TableauRestClient::TableauRestConnection conn) {
        self.conn = conn;
        rest = conn.get(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.tableaurestclient) {
            rest = copts.tableaurestclient;
        } else {
            hash<auto> client_opts = copts.tableaurestclient_options ?? {};
            rest = new TableauRestClient::TableauRestClient(client_opts, True);
        }
    }

    #! Returns the REST client
    TableauRestClient::TableauRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *TableauRestClient::TableauRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Makes a GET request to the Tableau API
    /** @param path the API path (relative, will be prefixed with sites/{site-luid}/ by REST client)
        @param params optional query parameters
        @return the response body

        @throw TABLEAU-API-ERROR if the response indicates an error
    */
    hash<auto> doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a POST request to the Tableau API
    /** @param path the API path
        @param body the request body
        @return the response body

        @throw TABLEAU-API-ERROR if the response indicates an error
    */
    hash<auto> doPost(string path, *hash<auto> body) {
        return doRestRequest("POST", path, body);
    }

    #! Makes a PUT request to the Tableau API
    /** @param path the API path
        @param body the request body
        @return the response body

        @throw TABLEAU-API-ERROR if the response indicates an error
    */
    hash<auto> doPut(string path, *hash<auto> body) {
        return doRestRequest("PUT", path, body);
    }

    #! Makes a DELETE request to the Tableau API
    /** @param path the API path
        @return the response body

        @throw TABLEAU-API-ERROR if the response indicates an error
    */
    hash<auto> doDelete(string path) {
        return doRestRequest("DELETE", path);
    }

    #! Fetches all pages from a GET endpoint using page-number pagination
    /** @param path the API path
        @param result_key the dot-separated key path in the response containing the result list
        (e.g., "datasources.datasource" or "projects.project")
        @param params optional query parameters
        @param limit optional maximum number of results to return
        @return a list of all result records
    */
    list<hash<auto>> fetchAllPages(string path, string result_key, *hash<auto> params,
            *int limit) {
        list<hash<auto>> results = ();
        int page = 1;
        while (True) {
            hash<auto> qparams = params ?? {};
            qparams.pageSize = DefaultPageSize;
            qparams.pageNumber = page;
            hash<auto> response = doGet(path, qparams);

            # Navigate the nested result key path (e.g., "datasources.datasource")
            auto items = getNestedValue(response, result_key);
            if (items) {
                if (items.typeCode() == NT_HASH) {
                    # Single result wrapped in a hash instead of a list
                    results += items;
                } else if (items.typeCode() == NT_LIST) {
                    results += items;
                }
            }

            if (limit && results.size() >= limit) {
                results = results[0..limit-1];
                break;
            }

            # Check pagination info
            *hash<auto> pagination = response.pagination;
            if (!pagination) {
                break;
            }
            int total = int(pagination.totalAvailable);
            if (page * DefaultPageSize >= total) {
                break;
            }
            ++page;
        }
        return results;
    }

    #! Gets a nested value from a hash using a dot-separated key path
    /** @param h the hash to navigate
        @param key_path dot-separated key path (e.g., "datasources.datasource")
        @return the value at the specified path
    */
    static auto getNestedValue(hash<auto> h, string key_path) {
        list<string> parts = key_path.split(".");
        auto val = h;
        foreach string part in (parts) {
            if (val.typeCode() != NT_HASH) {
                return NOTHING;
            }
            val = val{part};
        }
        return val;
    }

    #! Makes a REST request with retry and rate limit handling
    /** @param method the HTTP method
        @param path the API path
        @param body optional request body
        @return the response body hash

        @throw TABLEAU-API-ERROR if the response indicates an error
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
                        throw "TABLEAU-ERROR", sprintf("unsupported HTTP method: %s", method);
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

    #! Checks the Tableau API response for errors
    /** Tableau returns errors as:
        @code
        {"error": {"summary": "...", "detail": "...", "code": "..."}}
        @endcode

        @throw TABLEAU-API-ERROR if the response contains an error
    */
    private checkResponse(hash<auto> response) {
        if (response.body.error) {
            hash<auto> error = response.body.error;
            string err_msg = error.detail ?? error.summary ?? "Unknown error";
            if (error.code.val()) {
                err_msg = sprintf("%s (code: %s)", err_msg, error.code);
            }
            throw "TABLEAU-API-ERROR", err_msg, response.body;
        }
    }

    #! Returns information on supported reference data
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "datasources": True,
            "projects": True,
            "users": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "datasources": return getReferenceDatasources();
            case "projects": return getReferenceProjects();
            case "users": return getReferenceUsers();
        }
    }

    #! Returns available datasources for reference data
    private *list<hash<AllowedValueInfo>> getReferenceDatasources() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> datasources = fetchAllPages("datasources",
                "datasources.datasource");
            foreach hash<auto> ds in (datasources) {
                string display_name = ds.name ?? sprintf("Datasource %s", ds.id);
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": ds.contentUrl ?? "",
                    "desc": sprintf("%s (ID: %s)", display_name, ds.id),
                    "value": ds.id,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available projects for reference data
    private *list<hash<AllowedValueInfo>> getReferenceProjects() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> projects = fetchAllPages("projects", "projects.project");
            foreach hash<auto> project in (projects) {
                string display_name = project.name ?? sprintf("Project %s", project.id);
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": project.description ?? "",
                    "desc": sprintf("%s (ID: %s)", display_name, project.id),
                    "value": project.id,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available users for reference data
    private *list<hash<AllowedValueInfo>> getReferenceUsers() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> users = fetchAllPages("users", "users.user");
            foreach hash<auto> user in (users) {
                string display_name = user.name ?? sprintf("User %s", user.id);
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": user.siteRole ?? "",
                    "desc": sprintf("%s - %s (ID: %s)", display_name,
                        user.siteRole ?? "", user.id),
                    "value": user.id,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }
}

} # namespace TableauDataProvider
