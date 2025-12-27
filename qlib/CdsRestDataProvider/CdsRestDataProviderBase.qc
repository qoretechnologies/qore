# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CdsRestDataProvider module definition

/** CdsRestDataProviderBase.qc Copyright 2023 - 2025 Qore Technologies, s.r.o.

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
#! The CdsRest data provider class
public class CdsRestDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options (without app-specific options like "api")
        const BaseConstructorOptions = {
            "cdsrestclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client to use",
                "type": AbstractDataProviderType::get(new Type("CdsRestClient")),
                "desc": "The `CdsRestClient` object to use",
            },
            "cdsrestclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Sets options to the REST client constructor",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Sets options to the REST client constructor",
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
            "scope": <DataProviderOptionInfo>{
                "display_name": "OAuth2 Scope",
                "short_desc": "The OAuth2 scope string",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The OAuth2 scope string",
            },
            "tenant": <DataProviderOptionInfo>{
                "display_name": "Tenant",
                "short_desc": "The tenant ID of the CDS server instance",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The tenant ID of the CDS server instance",
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "URL",
                "short_desc": "The URL to the CDS server instance",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The URL to the CDS server instance",
                "required": True,
            },
        };

        #! Environment options
        const EnvOptions = {
            "url": "CDS_URL",
            "client_id": "CDS_CLIENT_ID",
            "client_secret": "CDS_CLIENT_SECRET",
            "tenant": "CDS_TENANT",
            "scope": "CDS_SCOPE",
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
        *CdsRestClient::CdsRestConnection conn;
    }

    #! Creates the object
    constructor() {
    }

    #! Creates the object
    constructor(CdsRestClient::CdsRestClient rest) {
        self.rest = rest;
    }

    #! Creates the object from a CDS connection
    constructor(CdsRestClient::CdsRestConnection conn) {
        self.conn = conn;
        self.rest = conn.get();
    }

    #! Creates the object from a Business Central connection
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
        return copts - ("cdsrestclient_options", "cdsrestclient")
            + copts.cdsrestclient_options;
    }
}
}
