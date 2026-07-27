# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FHIR metadata data providers

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
#! CapabilityStatement data provider
public class FhirCapabilityStatementDataProvider inherits FhirRestDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "capability-statement",
            "desc": "Reads the FHIR server CapabilityStatement",
            "type": "FhirCapabilityStatementDataProvider",
            "supports_request": True,
        };
    }

    #! Creates the object with the given FHIR REST client
    constructor(FhirRestClient::FhirRestClient fhir) : FhirRestDataProviderBase(fhir) {
    }

    #! Returns the data provider name
    string getName() {
        return "capability-statement";
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return NOTHING;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return FhirCapabilityStatementDataType;
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return fhir.getCapabilityStatement();
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! SMART on FHIR configuration data provider
public class FhirSmartConfigurationDataProvider inherits FhirRestDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "smart-configuration",
            "desc": "Reads SMART on FHIR discovery metadata",
            "type": "FhirSmartConfigurationDataProvider",
            "supports_request": True,
        };
    }

    #! Creates the object with the given FHIR REST client
    constructor(FhirRestClient::FhirRestClient fhir) : FhirRestDataProviderBase(fhir) {
    }

    #! Returns the data provider name
    string getName() {
        return "smart-configuration";
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return NOTHING;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return FhirSmartConfigurationDataType;
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return fhir.getSmartConfiguration();
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
