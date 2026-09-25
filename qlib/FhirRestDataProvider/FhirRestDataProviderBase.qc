# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FhirRestDataProviderBase class definition

/*  Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the FhirRestDataProvider module
public namespace FhirRestDataProvider {
#! Base class for FHIR REST data providers
public class FhirRestDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Constructor options
        const ConstructorOptions = {
            "fhir": <DataProviderOptionInfo>{
                "display_name": "FHIR Client",
                "short_desc": "FHIR REST client object",
                "type": AbstractDataProviderType::get(new Type("RestClient::AbstractRestClient")),
                "desc": "Existing `FhirRestClient::FhirRestClientIo` object to use for API calls; a synchronous "
                    "`FhirRestClient::FhirRestClient` is also accepted, and a `FhirRestClientIo` is created with its "
                    "options",
            },
            "fhirconnection": <DataProviderOptionInfo>{
                "display_name": "FHIR Connection",
                "short_desc": "FHIR connection name",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "Name of a configured FHIR connection to use",
                "example_value": "ehr-r4",
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "FHIR URL",
                "short_desc": "FHIR server base URL",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "FHIR server base URL. Use `fhir://` or `fhirs://` connection schemes, or an "
                    "`http://` / `https://` URL.",
                "example_value": "https://fhir.example.org/r4",
            },
            "token": <DataProviderOptionInfo>{
                "display_name": "Access Token",
                "short_desc": "Bearer token for FHIR requests",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "OAuth2 bearer token for FHIR requests when creating a client from `url`",
                "sensitive": True,
            },
            "headers": <DataProviderOptionInfo>{
                "display_name": "Headers",
                "short_desc": "Additional request headers",
                "type": AbstractDataProviderType::get(AutoHashOrNothingType),
                "desc": "Additional HTTP request headers to apply when creating a client from `url`.\n\n"
                    "**Example**: `{\"X-Request-ID\": \"qore-fhir-test\"}`",
            },
            "client_options": <DataProviderOptionInfo>{
                "display_name": "Client Options",
                "short_desc": "Additional REST client options",
                "type": AbstractDataProviderType::get(AutoHashOrNothingType),
                "desc": "Additional `FhirRestClientIo` constructor options when creating a client from `url`.\n\n"
                    "**Example**: `{\"no_charset\": True}`",
            },
        };
    }

    private {
        #! The FHIR REST client for API calls
        FhirRestClient::FhirRestClientIo fhir;
    }

    #! Creates the object with the given FHIR REST client
    /** @param fhir the FHIR REST client for API calls
    */
    constructor(FhirRestClient::FhirRestClientIo fhir) {
        @assert(fhir);
        self.fhir = fhir;
    }

    #! Creates the object with the given synchronous FHIR REST client
    /** @param fhir a synchronous FHIR REST client; API calls are made with a
        @ref FhirRestClient::FhirRestClientIo "FhirRestClientIo" created from it with @ref getAsyncClient()
    */
    constructor(FhirRestClient::FhirRestClient fhir) {
        self.fhir = FhirRestDataProviderBase::getAsyncClient(fhir);
    }

    #! Creates the object from constructor options
    /** @param options constructor options; provide one of `fhir`, `fhirconnection`, or `url`

        @throw FHIR-DATA-PROVIDER-ERROR if no FHIR client source is provided, or if the `fhir` option is not a FHIR
        REST client
    */
    constructor(*hash<auto> options) {
        *FhirRestClient::FhirRestClientIo fhir = FhirRestDataProviderBase::getFhirClient(options);
        if (!fhir) {
            fhir = FhirRestDataProviderBase::getFhirClientFromConnection(options);
        }
        if (!fhir) {
            throw "FHIR-DATA-PROVIDER-ERROR", "one of `fhir`, `fhirconnection`, or `url` must be provided";
        }
        self.fhir = fhir;
    }

    #! Returns the FHIR REST client used for API calls
    /** @return the FHIR REST client used for API calls
    */
    FhirRestClient::FhirRestClientIo getFhirClient() {
        return fhir;
    }

    #! Returns the FHIR REST client from direct options
    /** @param options constructor options

        @return the client given with the `fhir` option (a synchronous client is converted with
        @ref getAsyncClient()), a client created for the `url` option, or no value if neither option is set

        @throw FHIR-DATA-PROVIDER-ERROR if the `fhir` option is not a FHIR REST client
    */
    static *FhirRestClient::FhirRestClientIo getFhirClient(*hash<auto> options) {
        if (options.fhir) {
            if (options.fhir instanceof FhirRestClient::FhirRestClientIo) {
                return options.fhir;
            }
            if (options.fhir instanceof FhirRestClient::FhirRestClient) {
                return FhirRestDataProviderBase::getAsyncClient(options.fhir);
            }
            throw "FHIR-DATA-PROVIDER-ERROR", sprintf("the `fhir` option must be a FhirRestClientIo or a "
                "FhirRestClient object; got %y", options.fhir.fullType());
        }
        if (options.url) {
            # the options have values of different types
            hash<auto!> client_options = {"skip_login": True} + options.client_options;
            if (options.headers) {
                client_options.headers = options.headers;
            }
            if (options.token) {
                client_options.token = options.token;
            }
            return new FhirRestClient::FhirRestClientIo(client_options + {"url": options.url});
        }
        return NOTHING;
    }

    #! Returns the FHIR REST client from a connection
    /** @param options constructor options

        @return the asynchronous client of the connection given with the `fhirconnection` option (from the
        connection's \c getAsync() method), or no value if the option is not set

        @throw FHIR-DATA-PROVIDER-ERROR if the connection does not provide a FHIR REST client
    */
    static *FhirRestClient::FhirRestClientIo getFhirClientFromConnection(*hash<auto> options) {
        if (options.fhirconnection) {
            AbstractConnection conn = get_connection(options.fhirconnection);
            object client = conn.getAsync();
            if (!(client instanceof FhirRestClient::FhirRestClientIo)) {
                throw "FHIR-DATA-PROVIDER-ERROR", sprintf("connection %y is not a FHIR connection; its client "
                    "class is %y", options.fhirconnection, client.className());
            }
            return client;
        }
        return NOTHING;
    }

    #! Returns an asynchronous FHIR REST client created with the options and the connection path of a synchronous one
    /** @param fhir the synchronous FHIR REST client

        @return a @ref FhirRestClient::FhirRestClientIo "FhirRestClientIo" created with the constructor options and
        the connection path of \a fhir; if \a fhir already has an authentication token, it is used by the new
        client as well
    */
    static FhirRestClient::FhirRestClientIo getAsyncClient(FhirRestClient::FhirRestClient fhir) {
        hash<auto> opts = fhir.getConstructorOptions();
        # use the current token, if any, so that the new client does not have to log in again
        *string token = fhir.getToken();
        if (token) {
            opts.token = token;
        }
        FhirRestClient::FhirRestClientIo rv(opts);
        if (*string path = fhir.getConnectionPath()) {
            rv.setConnectionPath(path);
        }
        return rv;
    }

    #! Normalizes a REST response for DataProvider action output
    static hash<auto> getResponseData(hash<RestClient::RestResponseInfo> response) {
        return {
            "status_code": response.status_code,
            "headers": response.headers,
            "body": response.body,
        };
    }

    #! Returns a locally and server-supported FHIR R4 resource type
    string assertServerResourceType(string resource_type) {
        return fhir.assertCapabilityStatementResourceType(assertFhirR4ResourceType(resource_type));
    }

    #! Returns a locally and server-supported FHIR R4 request resource type
    string assertServerResourceRequestType(string resource_type, *hash<auto> resource = NOTHING) {
        assertFhirR4ResourceRequestType(resource_type, resource);
        return assertServerResourceType(resource_type);
    }

    #! Returns generated R4 resource path names supported by the connected server
    list<string> getSupportedGeneratedResourcePathNames() {
        list<string> rv = ();
        foreach string resource_type in (FhirR4GeneratedResourceTypeList) {
            if (fhir.isCapabilityStatementResourceType(resource_type)) {
                push rv, resource_type.lwr();
            }
        }
        return rv;
    }

    #! Builds a FHIR search parameter hash from typed common fields and raw advanced params
    hash<auto> getSearchParams(auto req) {
        hash params = cast<hash>(req.params ?? {});
        addSearchParam(\params, "_id", req.id);
        addSearchParam(\params, "_lastUpdated", req.last_updated);
        addSearchParam(\params, "_tag", req.tag);
        addSearchParam(\params, "_profile", req.profile);
        addSearchParam(\params, "_security", req.security);
        addSearchParam(\params, "_text", req.text);
        addSearchParam(\params, "_content", req.content);
        addSearchParam(\params, "_count", req.count);
        addSearchParam(\params, "_sort", req.sort);
        addSearchParam(\params, "_include", req.include);
        addSearchParam(\params, "_revinclude", req.revinclude);
        return params;
    }

    private static addSearchParam(reference<hash> params, string key, auto value) {
        if (!exists value) {
            return;
        }
        if (exists params{key}) {
            throw "FHIR-SEARCH-PARAM-ERROR",
                sprintf("FHIR search parameter %y was provided both directly and in `params`", key);
        }
        params{key} = value;
    }
}
}
