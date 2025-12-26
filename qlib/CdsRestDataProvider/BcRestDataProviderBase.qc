# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BcRestDataProviderBase class definition

/** BcRestDataProviderBase.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the CdsRestDataProvider module
public namespace CdsRestDataProvider {
#! Base class for Business Central data providers
public class BcRestDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Constructor options
        const ConstructorOptions = {
            "api": <DataProviderOptionInfo>{
                "display_name": "API Version",
                "short_desc": "Sets the API version to use",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "Sets the Business Central API version to use",
                "default_value": CdsRestClient::CdsRestClient::DefaultBcApiVersion,
            },
            "bcrestclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client to use",
                "type": AbstractDataProviderType::get(new Type("CdsRestClient")),
                "desc": "The `CdsRestClient` object to use for Business Central",
            },
            "bcrestclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Sets options to the REST client constructor",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Sets options to the REST client constructor",
            },
            "instance_type": <DataProviderOptionInfo>{
                "display_name": "Instance Type",
                "short_desc": "Business Central instance type",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "Business Central instance type (e.g., 'sandbox', 'production', or environment GUID)",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "sandbox",
                        "display_name": "Sandbox",
                    },
                    <AllowedValueInfo>{
                        "value": "production",
                        "display_name": "Production",
                    },
                ),
                "allowed_values_creatable": True,
            },
            "oauth2_client_id": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client ID",
                "short_desc": "Sets the OAuth2 client ID",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "Sets the OAuth2 client ID (required if 'client_secret' provided)",
            },
            "oauth2_client_secret": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Client Secret",
                "short_desc": "Sets the OAuth2 client secret",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "Sets the OAuth2 client secret (required if 'client_id' provided)",
            },
            "tenant": <DataProviderOptionInfo>{
                "display_name": "Tenant",
                "short_desc": "The Azure AD tenant ID",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The Azure AD tenant ID for authentication",
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "URL",
                "short_desc": "The URL to the Business Central server",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The URL to the Business Central server instance",
                "default_value": CdsRestClient::CdsRestClient::DefaultBusinessCentralBaseUri,
            },
        };

        #! Environment options
        const EnvOptions = {
            "url": "BC_URL",
            "oauth2_client_id": "BC_CLIENT_ID",
            "oauth2_client_secret": "BC_CLIENT_SECRET",
            "tenant": "BC_TENANT",
            "instance_type": "BC_INSTANCE_TYPE",
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
        #! The REST client object for API calls
        CdsRestClient::CdsRestClient rest;

        #! The connection object used to create the REST client
        *CdsRestClient::BusinessCentralRestConnection conn;
    }

    #! Creates the object
    constructor() {
    }

    #! Creates the object
    constructor(CdsRestClient::CdsRestClient rest) {
        self.rest = rest;
    }

    #! Creates the object
    constructor(CdsRestClient::BusinessCentralRestConnection conn) {
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

    #! Returns options for the CdsRestClient
    static private *hash<auto> getClientOptions(*hash<auto> copts) {
        return copts - ("bcrestclient_options", "bcrestclient")
            + copts.bcrestclient_options;
    }
}
}
