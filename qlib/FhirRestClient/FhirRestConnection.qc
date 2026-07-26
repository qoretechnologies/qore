# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FhirRestConnection.qc @brief FHIR REST connection provider implementation

/*  FhirRestConnection.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! the %FhirRestClient namespace holds all public definitions in the %FhirRestClient module
public namespace FhirRestClient {

#! Class for FHIR REST connections; returns @ref FhirRestClient objects
/** This class implements all options of @ref RestClient::RestConnection

    @see @ref FhirRestClient::FhirRestClient::constructor() for more information
*/
public class FhirRestConnection inherits RestClient::RestConnection {
    public {
        #! Connection entry info
        const ConnectionScheme = <ConnectionSchemeInfo>{
            "display_name": "FHIR REST Connection",
            "short_desc": "A connection to a FHIR REST server",
            "desc": "A connection to a FHIR REST server using JSON resources",
            "cls": Class::forName("FhirRestClient::FhirRestConnection"),
            "schemes": {
                "fhirs": True,
                "fhir": True,
            },
            "base_scheme_map": {
                "https": "fhirs",
                "http": "fhir",
            },
            "options": RestConnection::ConnectionScheme.options + {
                "data": RestConnection::ConnectionScheme.options."data" + <ConnectionOptionInfo>{
                    "default_value": FhirRestClientBase::DefaultData,
                },
                "error_passthru": RestConnection::ConnectionScheme.options."error_passthru" + <ConnectionOptionInfo>{
                    "default_value": True,
                },
                "headers": RestConnection::ConnectionScheme.options."headers" + <ConnectionOptionInfo>{
                    "default_value": FhirRestClientBase::DefaultHeaders,
                },
                "no_charset": <ConnectionOptionInfo>{
                    "display_name": "No Charset in Content-Type?",
                    "short_desc": "Do not append a charset parameter to request Content-Type headers",
                    "type": "bool",
                    "desc": "If `True`, no charset parameter is appended to request Content-Type headers",
                    "default_value": True,
                },
                "ping_headers": RestConnection::ConnectionScheme.options."ping_headers" + <ConnectionOptionInfo>{
                    "default_value": FhirRestClientBase::DefaultHeaders,
                },
                "ping_method": RestConnection::ConnectionScheme.options."ping_method" + <ConnectionOptionInfo>{
                    "default_value": FhirRestClientBase::DefaultPingMethod,
                },
                "ping_path": RestConnection::ConnectionScheme.options."ping_path" + <ConnectionOptionInfo>{
                    "default_value": FhirRestClientBase::DefaultPingPath,
                },
            },
            "metadata": {
                "standard": "HL7 FHIR",
                "resource_format": FhirRestClientBase::MimeTypeFhirJson,
                "initial_version": "R4",
            },
        };
    }

    #! Creates the FhirRestConnection object
    /** @param name connection name
        @param description connection description
        @param url FHIR REST URL using \c fhir, \c fhirs, \c http, or \c https
        @param attributes optional connection attributes
        @param options optional connection options
        @throw CONNECTION-ERROR if the underlying REST connection cannot be initialized
    */
    constructor(string name, string description, string url, hash<auto> attributes = {},
            hash<auto> options = {})
            : RestConnection(name, description, url, attributes, options) {
    }

    #! Creates the object from a serialized connection hash
    /** @param config serialized connection configuration
        @param attr optional connection attributes
        @throw CONNECTION-ERROR if the underlying REST connection cannot be initialized
    */
    constructor(hash<auto> config, *hash<auto> attr) : RestConnection(config, attr) {
    }

    #! returns \c "fhir"
    /** @return static connection type name
    */
    string getType() {
        return "fhir";
    }

    #! Returns connection options
    /** @param rtopts optional runtime option overrides
        @return normalized FHIR REST client options for a concrete connection instance
    */
    *hash<auto> getConnectionOptions(*hash<auto> rtopts) {
        return FhirRestClientBase::getOptions(RestConnection::getConnectionOptions(rtopts));
    }

    #! returns a @ref FhirRestClient object
    /** @param connect if True, then connect before returning the client
        @param rtopts optional runtime option overrides
        @return FHIR REST client implementation for this connection
    */
    private FhirRestClient getImpl(bool connect = True, *hash<auto> rtopts) {
        return new FhirRestClient(getConnectionOptions(rtopts), !connect);
    }

    #! Returns the ConnectionSchemeInfo hash for this object
    /** @return FHIR connection scheme metadata
    */
    private hash<ConnectionSchemeInfo> getConnectionSchemeInfoImpl() {
        return ConnectionScheme;
    }
}

} # namespace FhirRestClient
