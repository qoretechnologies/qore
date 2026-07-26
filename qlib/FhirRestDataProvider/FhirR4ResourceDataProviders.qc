# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FHIR R4 generated resource action data providers

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
#! FHIR R4 generated resource container data provider
public class FhirR4ResourceDataProvider inherits FhirRestDataProviderBase {
    private {
        string resource_type;
    }

    #! Creates the object with the given FHIR REST client and generated resource type
    constructor(FhirRestClient::FhirRestClient fhir, string resource_type) : FhirRestDataProviderBase(fhir) {
        @assert(getFhirR4GeneratedResourceDataType(resource_type));
        self.resource_type = resource_type;
    }

    #! Returns the data provider name
    string getName() {
        return resource_type.lwr();
    }

    #! Returns child data provider names
    private *list<string> getChildProviderNamesImpl() {
        return ("read", "search", "search-all", "create", "update", "conditional-update", "patch", "delete",
            "validate");
    }

    #! Returns the given child provider or \c NOTHING if the given child is unknown
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        switch (name) {
            case "read":
                return new FhirR4ReadResourceDataProvider(fhir, resource_type);
            case "search":
                return new FhirR4SearchResourcesDataProvider(fhir, resource_type);
            case "search-all":
                return new FhirR4SearchAllResourcesDataProvider(fhir, resource_type);
            case "create":
                return new FhirR4CreateResourceDataProvider(fhir, resource_type);
            case "update":
                return new FhirR4UpdateResourceDataProvider(fhir, resource_type);
            case "conditional-update":
                return new FhirR4ConditionalUpdateResourceDataProvider(fhir, resource_type);
            case "patch":
                return new FhirR4PatchResourceDataProvider(fhir, resource_type);
            case "delete":
                return new FhirR4DeleteResourceDataProvider(fhir, resource_type);
            case "validate":
                return new FhirR4ValidateResourceDataProvider(fhir, resource_type);
        }
        return NOTHING;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return <DataProviderInfo>{
            "name": getName(),
            "desc": sprintf("Generated FHIR R4 %s resource actions", resource_type),
            "type": "FhirR4ResourceDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
        };
    }
}

#! Base class for generated FHIR R4 resource action providers
public class FhirR4ResourceActionDataProviderBase inherits FhirRestDataProviderBase {
    private {
        string resource_type;
    }

    #! Creates the object with the given FHIR REST client and generated resource type
    constructor(FhirRestClient::FhirRestClient fhir, string resource_type) : FhirRestDataProviderBase(fhir) {
        @assert(getFhirR4GeneratedResourceDataType(resource_type));
        self.resource_type = resource_type;
    }

    #! Returns the generated resource response type
    FhirR4GeneratedResourceDataType getResourceDataType() {
        return getFhirR4GeneratedResourceDataType(resource_type);
    }

    #! Returns the generated resource type for this action provider
    string getResourceType() {
        return resource_type;
    }
}

#! Generated FHIR R4 resource read data provider
public class FhirR4ReadResourceDataProvider inherits FhirR4ResourceActionDataProviderBase {
    constructor(FhirRestClient::FhirRestClient fhir, string resource_type)
            : FhirR4ResourceActionDataProviderBase(fhir, resource_type) {
    }

    string getName() {
        return "read";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return new FhirR4GeneratedResourceRequestDataType(getResourceType(), "Read",
            getFhirR4GeneratedReadResourceRequestFields());
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return getResourceDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return fhir.readResource(assertServerResourceType(getResourceType()), req.id);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return <DataProviderInfo>{"name": "read", "desc": sprintf("Reads a FHIR R4 %s resource", getResourceType()),
            "type": "FhirR4ReadResourceDataProvider", "supports_request": True};
    }
}

#! Generated FHIR R4 resource search data provider
public class FhirR4SearchResourcesDataProvider inherits FhirR4ResourceActionDataProviderBase {
    constructor(FhirRestClient::FhirRestClient fhir, string resource_type)
            : FhirR4ResourceActionDataProviderBase(fhir, resource_type) {
    }

    string getName() {
        return "search";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return new FhirR4GeneratedResourceRequestDataType(getResourceType(), "Search",
            getFhirR4GeneratedSearchResourcesRequestFields());
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return FhirR4BundleDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return fhir.searchResources(assertServerResourceType(getResourceType()), getSearchParams(req));
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return <DataProviderInfo>{"name": "search",
            "desc": sprintf("Searches FHIR R4 %s resources", getResourceType()),
            "type": "FhirR4SearchResourcesDataProvider", "supports_request": True};
    }
}

#! Generated FHIR R4 resource search-all data provider
public class FhirR4SearchAllResourcesDataProvider inherits FhirR4ResourceActionDataProviderBase {
    constructor(FhirRestClient::FhirRestClient fhir, string resource_type)
            : FhirR4ResourceActionDataProviderBase(fhir, resource_type) {
    }

    string getName() {
        return "search-all";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return new FhirR4GeneratedResourceRequestDataType(getResourceType(), "SearchAll",
            getFhirR4GeneratedSearchAllResourcesRequestFields());
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return FhirR4BundleDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return fhir.searchAllResources(assertServerResourceType(getResourceType()), getSearchParams(req),
            req.max_pages ?? 100);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return <DataProviderInfo>{"name": "search-all",
            "desc": sprintf("Searches all FHIR R4 %s resource pages", getResourceType()),
            "type": "FhirR4SearchAllResourcesDataProvider", "supports_request": True};
    }
}

#! Generated FHIR R4 resource create data provider
public class FhirR4CreateResourceDataProvider inherits FhirR4ResourceActionDataProviderBase {
    constructor(FhirRestClient::FhirRestClient fhir, string resource_type)
            : FhirR4ResourceActionDataProviderBase(fhir, resource_type) {
    }

    string getName() {
        return "create";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return new FhirR4GeneratedResourceRequestDataType(getResourceType(), "Create",
            getFhirR4GeneratedCreateResourceRequestFields(getResourceType()));
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return FhirRestResponseDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string resource_type = assertServerResourceType(getResourceType());
        return getResponseData(fhir.createResource(resource_type,
            assertFhirR4TypedResourceBody(resource_type, req.resource), req.if_none_exist));
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return <DataProviderInfo>{"name": "create",
            "desc": sprintf("Creates a FHIR R4 %s resource", getResourceType()),
            "type": "FhirR4CreateResourceDataProvider", "supports_request": True};
    }
}

#! Generated FHIR R4 resource update data provider
public class FhirR4UpdateResourceDataProvider inherits FhirR4ResourceActionDataProviderBase {
    constructor(FhirRestClient::FhirRestClient fhir, string resource_type)
            : FhirR4ResourceActionDataProviderBase(fhir, resource_type) {
    }

    string getName() {
        return "update";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return new FhirR4GeneratedResourceRequestDataType(getResourceType(), "Update",
            getFhirR4GeneratedUpdateResourceRequestFields(getResourceType()));
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return FhirRestResponseDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string resource_type = assertServerResourceType(getResourceType());
        return getResponseData(fhir.updateResource(resource_type, req.id,
            assertFhirR4TypedResourceBody(resource_type, req.resource), req.etag));
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return <DataProviderInfo>{"name": "update",
            "desc": sprintf("Updates a FHIR R4 %s resource", getResourceType()),
            "type": "FhirR4UpdateResourceDataProvider", "supports_request": True};
    }
}

#! Generated FHIR R4 resource conditional update data provider
public class FhirR4ConditionalUpdateResourceDataProvider inherits FhirR4ResourceActionDataProviderBase {
    constructor(FhirRestClient::FhirRestClient fhir, string resource_type)
            : FhirR4ResourceActionDataProviderBase(fhir, resource_type) {
    }

    string getName() {
        return "conditional-update";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return new FhirR4GeneratedResourceRequestDataType(getResourceType(), "ConditionalUpdate",
            getFhirR4GeneratedConditionalUpdateResourceRequestFields(getResourceType()));
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return FhirRestResponseDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string resource_type = assertServerResourceType(getResourceType());
        return getResponseData(fhir.conditionalUpdateResource(resource_type, req.search_params,
            assertFhirR4TypedResourceBody(resource_type, req.resource)));
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return <DataProviderInfo>{"name": "conditional-update",
            "desc": sprintf("Conditionally updates a FHIR R4 %s resource", getResourceType()),
            "type": "FhirR4ConditionalUpdateResourceDataProvider", "supports_request": True};
    }
}

#! Generated FHIR R4 resource patch data provider
public class FhirR4PatchResourceDataProvider inherits FhirR4ResourceActionDataProviderBase {
    constructor(FhirRestClient::FhirRestClient fhir, string resource_type)
            : FhirR4ResourceActionDataProviderBase(fhir, resource_type) {
    }

    string getName() {
        return "patch";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return new FhirR4GeneratedResourceRequestDataType(getResourceType(), "Patch",
            getFhirR4GeneratedPatchResourceRequestFields());
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return FhirRestResponseDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return getResponseData(fhir.patchResource(assertServerResourceType(getResourceType()), req.id, req.patch,
            req.content_type ?? "application/json-patch+json", req.etag));
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return <DataProviderInfo>{"name": "patch",
            "desc": sprintf("Patches a FHIR R4 %s resource", getResourceType()),
            "type": "FhirR4PatchResourceDataProvider", "supports_request": True};
    }
}

#! Generated FHIR R4 resource delete data provider
public class FhirR4DeleteResourceDataProvider inherits FhirR4ResourceActionDataProviderBase {
    constructor(FhirRestClient::FhirRestClient fhir, string resource_type)
            : FhirR4ResourceActionDataProviderBase(fhir, resource_type) {
    }

    string getName() {
        return "delete";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return new FhirR4GeneratedResourceRequestDataType(getResourceType(), "Delete",
            getFhirR4GeneratedDeleteResourceRequestFields());
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return FhirRestResponseDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return getResponseData(fhir.deleteResource(assertServerResourceType(getResourceType()), req.id, req.etag));
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return <DataProviderInfo>{"name": "delete",
            "desc": sprintf("Deletes a FHIR R4 %s resource", getResourceType()),
            "type": "FhirR4DeleteResourceDataProvider", "supports_request": True};
    }
}

#! Generated FHIR R4 resource validate data provider
public class FhirR4ValidateResourceDataProvider inherits FhirR4ResourceActionDataProviderBase {
    constructor(FhirRestClient::FhirRestClient fhir, string resource_type)
            : FhirR4ResourceActionDataProviderBase(fhir, resource_type) {
    }

    string getName() {
        return "validate";
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return new FhirR4GeneratedResourceRequestDataType(getResourceType(), "Validate",
            getFhirR4GeneratedValidateResourceRequestFields(getResourceType()));
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return FhirOperationOutcomeDataType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string resource_type = assertServerResourceType(getResourceType());
        return fhir.validateResource(assertFhirR4TypedResourceBody(resource_type, req.resource), resource_type,
            req.id);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return <DataProviderInfo>{"name": "validate",
            "desc": sprintf("Validates a FHIR R4 %s resource through $validate", getResourceType()),
            "type": "FhirR4ValidateResourceDataProvider", "supports_request": True};
    }
}
}
