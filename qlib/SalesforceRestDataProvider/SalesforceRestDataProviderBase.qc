# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SalesforceRestDataProvider module definition

/** SalesforceRestDataProviderBase.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SalesforceRestDataProvider module
public namespace SalesforceRestDataProvider {
#! The SalesforceRest data provider class
public class SalesforceRestDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Constructor options
        const ConstructorOptions = {
            "api": <DataProviderOptionInfo>{
                "display_name": "API Version",
                "short_desc": "The Salesforce API version to use",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The Salesforce API version to use (\"auto\" = latest version available)",
            },
            "oauth2_auth_url": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Auth URL",
                "short_desc": "The OAuth2 auth URL",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The URL for logging in, overrides the default value",
            },
            "oauth2_client_id": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client ID",
                "short_desc": "Sets the Salesforce consumer key",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "the Salesforce consumer key / OAuth2 client ID",
            },
            "oauth2_client_secret": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client Secret",
                "short_desc": "Sets the Salesforce consumer secret",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "the Salesforce.com consumer secret / OAuth2 client secret",
            },
            "oauth2_token_url": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Token URL",
                "short_desc": "The OAuth2 token URL",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The URL for token acquisition, overrides the default value, used when logging in",
            },
            "oauth2_logout_url": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Revoke URL",
                "short_desc": "The OAuth2 token revoke URL",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The URL for token revocation, overrides the default value, used when logging out",
            },
            "password": <DataProviderOptionInfo>{
                "display_name": "Password",
                "short_desc": "The password for authentication",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "the Salesforce.com account password",
            },
            "salesforcerestclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client to use",
                "type": AbstractDataProviderType::get(new Type("SalesforceRestClient")),
                "desc": "The `SalesforceRestClient` object to use",
            },
            "salesforcerestclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Options when creating a REST client",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options to the SalesforceRestClient constructor; only used if a `SalesforceRestClient` "
                    "object is created for a call",
            },
            "username": <DataProviderOptionInfo>{
                "display_name": "Username",
                "short_desc": "Sets the username for authentication",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "the Salesforce.com account username",
            },
        };

        #! Environment options
        const EnvOptions = {
            "client_id": "SALESFORCE_CONSUMER_KEY",
            "client_secret": "SALESFORCE_CONSUMER_SECRET",
            "username": "SALESFORCE_USER",
            "password": "SALESFORCE_PASS",
        };

        #! Hash of HTTP Methods
        const HttpMethods = {
            "GET": True,
            "PUT": True,
            "PATCH": True,
            "POST": True,
            "DELETE": True,
        };
    }

    private {
        #! The connection object used to create the REST client
        *SalesforceRestClient::SalesforceRestConnection conn;

        #! The REST client object for API calls
        SalesforceRestClient::SalesforceRestClient rest;
    }

    #! Creates the object
    constructor() {
    }

    #! Creates the object
    constructor(SalesforceRestClient::SalesforceRestClient rest) {
        self.rest = rest;
    }

    #! Creates the object
    constructor(SalesforceRestClient::SalesforceRestConnection conn) {
        self.conn = conn;
        self.rest = conn.get();
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        if (rest) {
            rest.setLogger(logger);
        }
        LoggerWrapper::setLogger(logger);
    }

    #! Returns options for the SalesforceRestClient
    static private *hash<auto> getClientOptions(*hash<auto> copts) {
        return copts - ("salesforcerestclient_options", "salesforcerestclient")
            + copts.salesforcerestclient_options;
    }
}
}
