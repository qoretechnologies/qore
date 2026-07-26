# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FHIR resource operation data providers

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
#! Read FHIR resource data provider
public class FhirReadResourceDataProvider inherits FhirRestDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "read",
            "desc": "Reads a FHIR resource by type and ID",
            "type": "FhirReadResourceDataProvider",
            "supports_request": True,
        };
    }

    constructor(FhirRestClient::FhirRestClient fhir) : FhirRestDataProviderBase(fhir) {
    }

    string getName() {
        return "read";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return FhirReadResourceRequestDataType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return FhirResourceDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return fhir.readResource(assertServerResourceType(req.resource_type), req.id);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Search FHIR resources data provider
public class FhirSearchResourcesDataProvider inherits FhirRestDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "search",
            "desc": "Searches FHIR resources",
            "type": "FhirSearchResourcesDataProvider",
            "supports_request": True,
        };
    }

    constructor(FhirRestClient::FhirRestClient fhir) : FhirRestDataProviderBase(fhir) {
    }

    string getName() {
        return "search";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return FhirSearchResourcesRequestDataType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return FhirBundleDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return fhir.searchResources(assertServerResourceType(req.resource_type), getSearchParams(req));
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Search all FHIR resource pages data provider
public class FhirSearchAllResourcesDataProvider inherits FhirRestDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "search-all",
            "desc": "Searches FHIR resources and follows Bundle pagination links",
            "type": "FhirSearchAllResourcesDataProvider",
            "supports_request": True,
        };
    }

    constructor(FhirRestClient::FhirRestClient fhir) : FhirRestDataProviderBase(fhir) {
    }

    string getName() {
        return "search-all";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return FhirSearchAllResourcesRequestDataType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return FhirBundleDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return fhir.searchAllResources(assertServerResourceType(req.resource_type), getSearchParams(req),
            req.max_pages ?? 100);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Create FHIR resource data provider
public class FhirCreateResourceDataProvider inherits FhirRestDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Creates a FHIR resource",
            "type": "FhirCreateResourceDataProvider",
            "supports_request": True,
        };
    }

    constructor(FhirRestClient::FhirRestClient fhir) : FhirRestDataProviderBase(fhir) {
    }

    string getName() {
        return "create";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return FhirCreateResourceRequestDataType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return FhirRestResponseDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return getResponseData(fhir.createResource(assertServerResourceRequestType(req.resource_type, req.resource),
            req.resource, req.if_none_exist));
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Submit FHIR transaction or batch Bundle data provider
public class FhirSubmitBundleDataProvider inherits FhirRestDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "submit-bundle",
            "desc": "Submits a FHIR transaction or batch Bundle",
            "type": "FhirSubmitBundleDataProvider",
            "supports_request": True,
        };
    }

    constructor(FhirRestClient::FhirRestClient fhir) : FhirRestDataProviderBase(fhir) {
    }

    string getName() {
        return "submit-bundle";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return FhirSubmitBundleRequestDataType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return FhirRestResponseDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return getResponseData(fhir.submitBundle(req.bundle));
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Update FHIR resource data provider
public class FhirUpdateResourceDataProvider inherits FhirRestDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Updates a FHIR resource",
            "type": "FhirUpdateResourceDataProvider",
            "supports_request": True,
        };
    }

    constructor(FhirRestClient::FhirRestClient fhir) : FhirRestDataProviderBase(fhir) {
    }

    string getName() {
        return "update";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return FhirUpdateResourceRequestDataType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return FhirRestResponseDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return getResponseData(fhir.updateResource(assertServerResourceRequestType(req.resource_type, req.resource),
            req.id, req.resource, req.etag));
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Conditional update FHIR resource data provider
public class FhirConditionalUpdateResourceDataProvider inherits FhirRestDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "conditional-update",
            "desc": "Conditionally updates a FHIR resource",
            "type": "FhirConditionalUpdateResourceDataProvider",
            "supports_request": True,
        };
    }

    constructor(FhirRestClient::FhirRestClient fhir) : FhirRestDataProviderBase(fhir) {
    }

    string getName() {
        return "conditional-update";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return FhirConditionalUpdateResourceRequestDataType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return FhirRestResponseDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return getResponseData(fhir.conditionalUpdateResource(
            assertServerResourceRequestType(req.resource_type, req.resource), req.search_params, req.resource));
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Patch FHIR resource data provider
public class FhirPatchResourceDataProvider inherits FhirRestDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "patch",
            "desc": "Patches a FHIR resource",
            "type": "FhirPatchResourceDataProvider",
            "supports_request": True,
        };
    }

    constructor(FhirRestClient::FhirRestClient fhir) : FhirRestDataProviderBase(fhir) {
    }

    string getName() {
        return "patch";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return FhirPatchResourceRequestDataType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return FhirRestResponseDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return getResponseData(fhir.patchResource(assertServerResourceType(req.resource_type), req.id, req.patch,
            req.content_type ?? "application/json-patch+json", req.etag));
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Delete FHIR resource data provider
public class FhirDeleteResourceDataProvider inherits FhirRestDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "desc": "Deletes a FHIR resource",
            "type": "FhirDeleteResourceDataProvider",
            "supports_request": True,
        };
    }

    constructor(FhirRestClient::FhirRestClient fhir) : FhirRestDataProviderBase(fhir) {
    }

    string getName() {
        return "delete";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return FhirDeleteResourceRequestDataType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return FhirRestResponseDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return getResponseData(fhir.deleteResource(assertServerResourceType(req.resource_type), req.id, req.etag));
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Validate FHIR resource data provider
public class FhirValidateResourceDataProvider inherits FhirRestDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "validate",
            "desc": "Validates a FHIR resource through $validate",
            "type": "FhirValidateResourceDataProvider",
            "supports_request": True,
        };
    }

    constructor(FhirRestClient::FhirRestClient fhir) : FhirRestDataProviderBase(fhir) {
    }

    string getName() {
        return "validate";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return FhirValidateResourceRequestDataType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return FhirOperationOutcomeDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        if (req.resource_type != NOTHING) {
            assertServerResourceRequestType(req.resource_type, req.resource);
        } else {
            assertServerResourceType(assertFhirR4ResourceBodyType(req.resource));
        }
        return fhir.validateResource(req.resource, req.resource_type, req.id);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
