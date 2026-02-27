# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksDataProviderBase class definition

/*  DatabricksDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DatabricksDataProvider module
public namespace DatabricksDataProvider {

#! Base class for Databricks data providers
public class DatabricksDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Databricks data providers
        const BaseConstructorOptions = {
            "databricksrestclient": <DataProviderOptionInfo>{
                "display_name": "Databricks REST Client",
                "short_desc": "The DatabricksRestClient object",
                "type": AbstractDataProviderType::get(
                    new Type("DatabricksRestClient::DatabricksRestClient")),
                "desc": "The DatabricksRestClient object to use for API calls",
            },
            "databricksrestclient_options": <DataProviderOptionInfo>{
                "display_name": "Databricks REST Client Options",
                "short_desc": "Options for creating a DatabricksRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a DatabricksRestClient object if one is not provided "
                    "directly",
            },
            "connection": <DataProviderOptionInfo>{
                "display_name": "Databricks Connection",
                "short_desc": "A Databricks REST connection object",
                "type": AbstractDataProviderType::get(
                    new Type("DatabricksRestClient::DatabricksRestConnection")),
                "desc": "A DatabricksRestConnection object; injected automatically by the framework "
                    "for scheme-based apps",
            },
            "token": <DataProviderOptionInfo>{
                "display_name": "Personal Access Token",
                "short_desc": "Databricks PAT for authentication",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "The Databricks Personal Access Token for authentication",
                "sensitive": True,
            },
            "workspace": <DataProviderOptionInfo>{
                "display_name": "Workspace Hostname",
                "short_desc": "Databricks workspace hostname",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "The Databricks workspace hostname (e.g., "
                    "`adb-1234567890.12.azuredatabricks.net`)",
            },
        };
    }

    private {
        #! The REST client object for API calls
        DatabricksRestClient::DatabricksRestClient rest;

        #! The connection object, if applicable
        *DatabricksRestClient::DatabricksRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(DatabricksRestClient::DatabricksRestClient rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(DatabricksRestClient::DatabricksRestConnection conn) {
        self.conn = conn;
        rest = conn.get(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        if (options.databricksrestclient) {
            rest = options.databricksrestclient;
        } else if (options.connection instanceof DatabricksRestClient::DatabricksRestConnection) {
            conn = options.connection;
            rest = conn.get(True);
        } else if (options.token || options.workspace || options.databricksrestclient_options) {
            hash<auto> client_opts = options.databricksrestclient_options ?? {};
            if (options.token) {
                client_opts.token = options.token;
            }
            if (options.workspace) {
                client_opts.workspace = options.workspace;
            }
            rest = new DatabricksRestClient::DatabricksRestClient(client_opts);
        }
        # else: REST client will be initialized later (e.g., by the framework)
    }

    #! Returns the REST client; throws if not initialized
    DatabricksRestClient::DatabricksRestClient getRestClient() {
        checkRestClient();
        return rest;
    }

    #! Returns the connection, if any
    *DatabricksRestClient::DatabricksRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        if (rest) {
            rest.setLogger(logger);
        }
    }

    #! Makes a POST request to the Databricks API
    /** @param path the API path (e.g., "api/2.0/clusters/create")
        @param body the request body
        @return the response data

        @throw DATABRICKS-API-ERROR if the response indicates an error
        @throw DATABRICKS-CONFIG-ERROR if the REST client is not initialized
    */
    hash<auto> doPost(string path, *hash<auto> body) {
        checkRestClient();
        hash<auto> response = rest.post(path, body);
        checkResponse(response);
        return response.body;
    }

    #! Makes a GET request to the Databricks API
    /** @param path the API path
        @param params optional query parameters
        @return the response data

        @throw DATABRICKS-API-ERROR if the response indicates an error
    */
    auto doGet(string path, *hash<auto> params) {
        checkRestClient();
        string url = path;
        if (params) {
            list<string> parts;
            foreach string key in (keys params) {
                if (exists params{key}) {
                    parts += sprintf("%s=%s", key, encode_url(string(params{key})));
                }
            }
            if (parts) {
                url += "?" + parts.join("&");
            }
        }
        hash<auto> response = rest.get(url);
        checkResponse(response);
        return response.body;
    }

    #! Makes a PUT request to the Databricks API
    /** @param path the API path
        @param body the request body
        @return the response data

        @throw DATABRICKS-API-ERROR if the response indicates an error
    */
    hash<auto> doPut(string path, *hash<auto> body) {
        checkRestClient();
        hash<auto> response = rest.put(path, body);
        checkResponse(response);
        return response.body;
    }

    #! Makes a DELETE request to the Databricks API
    /** @param path the API path
        @param body optional request body
        @return the response data

        @throw DATABRICKS-API-ERROR if the response indicates an error
    */
    hash<auto> doDelete(string path, *hash<auto> body) {
        checkRestClient();
        hash<auto> response = rest.del(path, body);
        checkResponse(response);
        return response.body;
    }

    #! Makes a PATCH request to the Databricks API
    /** @param path the API path
        @param body the request body
        @return the response data

        @throw DATABRICKS-API-ERROR if the response indicates an error
    */
    hash<auto> doPatch(string path, *hash<auto> body) {
        checkRestClient();
        hash<auto> response = rest.patch(path, body);
        checkResponse(response);
        return response.body;
    }

    #! Paginates through a GET endpoint that uses next_page_token
    /** @param path the API path
        @param result_key the key in the response containing the list of records
        @param params optional query parameters
        @param max_records optional maximum total records to return; 0 or NOTHING means unlimited
        @return all records from all pages (up to max_records if set)

        @throw DATABRICKS-API-ERROR if any response indicates an error
    */
    list<auto> paginateGet(string path, string result_key, *hash<auto> params, *int max_records) {
        list<auto> all_records = ();
        *string page_token;
        do {
            hash<auto> req_params = params ?? {};
            if (page_token) {
                req_params.page_token = page_token;
            }
            hash<auto> response = doGet(path, req_params);
            if (response{result_key}) {
                all_records += response{result_key};
            }
            # Stop if we have reached the requested limit
            if (max_records > 0 && all_records.lsize() >= max_records) {
                all_records = all_records[0..max_records - 1];
                break;
            }
            page_token = response.next_page_token;
        } while (page_token.val());
        return all_records;
    }

    #! Throws if the REST client is not initialized
    private checkRestClient() {
        if (!rest) {
            throw "DATABRICKS-CONFIG-ERROR", "Databricks REST client is not initialized; supply a "
                "DatabricksRestClient object, a DatabricksRestConnection, or token + workspace options";
        }
    }

    #! Checks the Databricks API response for errors
    private checkResponse(hash<auto> response) {
        if (response.status_code >= 400) {
            *string msg = response.body.message
                ?? response.body.error
                ?? sprintf("Databricks API error (HTTP %d)", response.status_code);
            *string err_code = response.body.error_code;
            throw "DATABRICKS-API-ERROR",
                err_code ? sprintf("%s: %s", err_code, msg) : msg,
                response.body;
        }
    }
}

} # namespace DatabricksDataProvider
