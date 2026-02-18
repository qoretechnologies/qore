# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BigMLDataProviderBase class definition

/*  BigMLDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BigMLDataProvider module
public namespace BigMLDataProvider {

#! Base class for BigML data providers
public class BigMLDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for BigML data providers
        const BaseConstructorOptions = {
            "bigmlrestclient": <DataProviderOptionInfo>{
                "display_name": "BigML REST Client",
                "short_desc": "The BigMLRestClient object",
                "type": AbstractDataProviderType::get(new Type("BigMLRestClient::BigMLRestClient")),
                "desc": "The BigMLRestClient object to use for API calls",
            },
            "bigmlrestclient_options": <DataProviderOptionInfo>{
                "display_name": "BigML REST Client Options",
                "short_desc": "Options for creating a BigMLRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a BigMLRestClient object if one is not provided directly",
            },
            "token": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "BigML API key",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "The BigML API key for authentication",
                "sensitive": True,
            },
            "username": <DataProviderOptionInfo>{
                "display_name": "Username",
                "short_desc": "BigML username",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "The BigML username for authentication",
            },
        };
    }

    private {
        #! The REST client object for API calls
        BigMLRestClient::BigMLRestClient rest;

        #! The connection object, if applicable
        *BigMLRestClient::BigMLRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(BigMLRestClient::BigMLRestClient rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(BigMLRestClient::BigMLRestConnection conn) {
        self.conn = conn;
        rest = conn.get(True);
    }

    #! Creates the object with options
    /** @note does not call checkOptions() because child classes pass their full options hash
        (which includes additional keys like \c model, \c input_fields, etc.) and checkOptions()
        would reject them; child classes are responsible for calling checkOptions() with the full
        option set
    */
    constructor(*hash<auto> options) {
        if (options.bigmlrestclient) {
            rest = options.bigmlrestclient;
        } else {
            hash<auto> client_opts = options.bigmlrestclient_options ?? {};
            if (options.token) {
                client_opts.token = options.token;
            }
            if (options.username) {
                client_opts.username = options.username;
            }
            rest = new BigMLRestClient::BigMLRestClient(client_opts);
        }
    }

    #! Returns the REST client
    BigMLRestClient::BigMLRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *BigMLRestClient::BigMLRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Makes a POST request to the BigML API
    /** @param path the API path (e.g., "prediction")
        @param body the request body
        @return the response data

        @throw BIGML-API-ERROR if the response indicates an error
    */
    hash<auto> doPost(string path, *hash<auto> body) {
        hash<auto> response = rest.post(path, body);
        checkResponse(response);
        return response.body;
    }

    #! Makes a GET request to the BigML API
    /** @param path the API path
        @param params optional query parameters
        @return the response data

        @throw BIGML-API-ERROR if the response indicates an error
    */
    auto doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            list<string> parts;
            foreach string key in (keys params) {
                if (exists params{key}) {
                    parts += sprintf("%s=%s", key, encode_url(string(params{key}), True));
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

    #! Makes a DELETE request to the BigML API
    /** @param path the API path
        @return the response data

        @throw BIGML-API-ERROR if the response indicates an error
    */
    hash<auto> doDelete(string path) {
        hash<auto> response = rest.del(path);
        checkResponse(response);
        return response.body;
    }

    #! Polls a BigML resource until it reaches the FINISHED status
    /** @param resource_path the resource path (e.g., "batchprediction/abc123")
        @param poll_interval_ms polling interval in milliseconds (default: 2000)
        @param timeout_ms timeout in milliseconds (default: 300000 = 5 minutes)
        @return the completed resource data

        @throw BIGML-TIMEOUT-ERROR if the resource does not finish within the timeout
        @throw BIGML-API-ERROR if the resource fails
    */
    hash<auto> pollUntilFinished(string resource_path, int poll_interval_ms = 2000,
            int timeout_ms = 300000) {
        date start = now_us();
        while (True) {
            hash<auto> result = doGet(resource_path);
            int status_code = result.status.code;
            # BigML status codes: 0=WAITING, 1=QUEUED, 2=STARTED, 3=IN_PROGRESS, 4=SUMMARIZED, 5=FINISHED
            if (status_code == 5) {
                return result;
            }
            if (status_code < 0) {
                throw "BIGML-API-ERROR", sprintf("BigML resource %y failed with status: %y",
                    resource_path, result.status);
            }
            int elapsed_ms = (now_us() - start).durationMilliseconds();
            if (elapsed_ms >= timeout_ms) {
                throw "BIGML-TIMEOUT-ERROR", sprintf("BigML resource %y did not finish within %d ms; "
                    "last status: %y", resource_path, timeout_ms, result.status);
            }
            usleep(poll_interval_ms * 1000);
        }
    }

    #! Checks the BigML API response for errors
    private checkResponse(hash<auto> response) {
        if (response.status_code >= 400) {
            string err_msg;
            if (response.body.status.message) {
                err_msg = response.body.status.message;
            } else if (response.body.typeCode() == NT_STRING) {
                err_msg = response.body;
            } else {
                err_msg = sprintf("BigML API error (HTTP %d)", response.status_code);
            }
            throw "BIGML-API-ERROR", err_msg, response.body;
        }
    }
}

} # namespace BigMLDataProvider
