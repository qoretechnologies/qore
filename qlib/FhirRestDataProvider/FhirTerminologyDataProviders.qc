# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FHIR terminology operation data providers

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
#! FHIR validate-code data provider
public class FhirValidateCodeDataProvider inherits FhirRestDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "validate-code",
            "desc": "Validates a code through a FHIR $validate-code operation",
            "type": "FhirValidateCodeDataProvider",
            "supports_request": True,
        };
    }

    constructor(FhirRestClient::FhirRestClient fhir) : FhirRestDataProviderBase(fhir) {
    }

    string getName() {
        return "validate-code";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return FhirValidateCodeRequestDataType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return FhirParametersDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return fhir.validateCode(req.path, req.params);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! FHIR lookup-code data provider
public class FhirLookupCodeDataProvider inherits FhirRestDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "lookup-code",
            "desc": "Looks up code metadata through a FHIR $lookup operation",
            "type": "FhirLookupCodeDataProvider",
            "supports_request": True,
        };
    }

    constructor(FhirRestClient::FhirRestClient fhir) : FhirRestDataProviderBase(fhir) {
    }

    string getName() {
        return "lookup-code";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return FhirLookupCodeRequestDataType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return FhirParametersDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return fhir.lookupCode(req.path, req.params);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! FHIR translate-code data provider
public class FhirTranslateCodeDataProvider inherits FhirRestDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "translate-code",
            "desc": "Translates a code through a FHIR $translate operation",
            "type": "FhirTranslateCodeDataProvider",
            "supports_request": True,
        };
    }

    constructor(FhirRestClient::FhirRestClient fhir) : FhirRestDataProviderBase(fhir) {
    }

    string getName() {
        return "translate-code";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return FhirTranslateCodeRequestDataType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return FhirParametersDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return fhir.translateCode(req.path, req.params);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! FHIR ValueSet expand data provider
public class FhirExpandValueSetDataProvider inherits FhirRestDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "expand-value-set",
            "desc": "Expands a FHIR ValueSet through $expand",
            "type": "FhirExpandValueSetDataProvider",
            "supports_request": True,
        };
    }

    constructor(FhirRestClient::FhirRestClient fhir) : FhirRestDataProviderBase(fhir) {
    }

    string getName() {
        return "expand-value-set";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return FhirExpandValueSetRequestDataType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return FhirResourceDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return fhir.expandValueSet(req.path, req.params);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
