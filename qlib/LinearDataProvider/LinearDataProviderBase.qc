# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearDataProviderBase.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the LinearDataProvider module
public namespace LinearDataProvider {

#! Base class for Linear data providers
public class LinearDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Linear data providers
        const BaseConstructorOptions = {
            "linearrestclient": <DataProviderOptionInfo>{
                "display_name": "Linear REST Client",
                "short_desc": "The LinearRestClient object",
                "type": AbstractDataProviderType::get(new Type("LinearRestClient::LinearRestClient")),
                "desc": "The LinearRestClient object to use for API calls",
            },
            "linearrestclient_options": <DataProviderOptionInfo>{
                "display_name": "Linear REST Client Options",
                "short_desc": "Options for creating a LinearRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a LinearRestClient object if one is not provided directly",
            },
            "token": <DataProviderOptionInfo>{
                "display_name": "API Token",
                "short_desc": "Linear API token",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The Linear API token (API key or OAuth2 access token) for authentication",
                "sensitive": True,
            },
            "oauth2_client_id": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client ID",
                "short_desc": "The OAuth2 client ID",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The OAuth2 client ID for authentication",
            },
            "oauth2_client_secret": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client Secret",
                "short_desc": "The OAuth2 client secret",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The OAuth2 client secret for authentication",
                "sensitive": True,
            },
        };
    }

    private {
        #! The REST client object for API calls
        LinearRestClient::LinearRestClient rest;

        #! The connection object, if applicable
        *LinearRestClient::LinearRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(LinearRestClient::LinearRestClient rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(LinearRestClient::LinearRestConnection conn) {
        self.conn = conn;
        rest = conn.get(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.linearrestclient) {
            rest = copts.linearrestclient;
        } else {
            hash<auto> client_opts = copts.linearrestclient_options ?? {};
            if (copts.token) {
                client_opts.token = copts.token;
            }
            if (copts.oauth2_client_id) {
                client_opts.oauth2_client_id = copts.oauth2_client_id;
            }
            if (copts.oauth2_client_secret) {
                client_opts.oauth2_client_secret = copts.oauth2_client_secret;
            }
            rest = new LinearRestClient::LinearRestClient(client_opts);
        }
    }

    #! Returns the REST client
    LinearRestClient::LinearRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *LinearRestClient::LinearRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Executes a GraphQL query
    /** @param query the GraphQL query string
        @param variables optional variables for the query
        @return the response data

        @throw GRAPHQL-ERROR if the response contains errors
    */
    hash<auto> doGraphQL(string query, *hash<auto> variables) {
        hash<auto> response = rest.graphql(query, variables);
        if (response.body.errors) {
            string err_msg = response.body.errors[0].message ?? "Unknown GraphQL error";
            throw "GRAPHQL-ERROR", err_msg, response.body.errors;
        }
        return response.body.data;
    }

    #! Helper method to get client options from constructor options
    static *hash<auto> getClientOptions(*hash<auto> copts) {
        if (!copts) {
            return;
        }
        hash<auto> rv;
        if (copts.token) {
            rv.token = copts.token;
        }
        if (copts.oauth2_client_id) {
            rv.oauth2_client_id = copts.oauth2_client_id;
        }
        if (copts.oauth2_client_secret) {
            rv.oauth2_client_secret = copts.oauth2_client_secret;
        }
        foreach string key in (keys copts.linearrestclient_options) {
            rv{key} = copts.linearrestclient_options{key};
        }
        return rv;
    }
}

} # namespace LinearDataProvider
