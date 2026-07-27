# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FhirRestDataProvider root and container provider classes

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
#! Root FHIR REST data provider
public class FhirRestDataProvider inherits FhirRestDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "FhirRestDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_children": True,
            "children_can_support_apis": True,
        };

        #! Child providers
        const ChildMap = {
            "metadata": Class::forName("FhirRestDataProvider::FhirMetadataDataProvider"),
            "resources": Class::forName("FhirRestDataProvider::FhirResourcesDataProvider"),
            "terminology": Class::forName("FhirRestDataProvider::FhirTerminologyDataProvider"),
        };
    }

    #! Creates the object with the given FHIR REST client
    constructor(FhirRestClient::FhirRestClient fhir) : FhirRestDataProviderBase(fhir) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : FhirRestDataProviderBase(options) {
    }

    #! Returns the data provider name
    string getName() {
        return FactoryName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("FHIR REST data provider for %s", fhir.getURL());
    }

    #! Returns a list of child data provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns the given child provider or \c NOTHING if the given child is unknown
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return NOTHING;
        }
        return cls.newObject(fhir);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! FHIR metadata container data provider
public class FhirMetadataDataProvider inherits FhirRestDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "metadata",
            "desc": "FHIR metadata data provider",
            "type": "FhirMetadataDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
        };
    }

    #! Creates the object with the given FHIR REST client
    constructor(FhirRestClient::FhirRestClient fhir) : FhirRestDataProviderBase(fhir) {
    }

    #! Returns the data provider name
    string getName() {
        return "metadata";
    }

    #! Returns child data provider names
    private *list<string> getChildProviderNamesImpl() {
        return ("capability-statement", "smart-configuration");
    }

    #! Returns the given child provider or \c NOTHING if the given child is unknown
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        switch (name) {
            case "capability-statement":
                return new FhirCapabilityStatementDataProvider(fhir);
            case "smart-configuration":
                return new FhirSmartConfigurationDataProvider(fhir);
        }
        return NOTHING;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! FHIR resources container data provider
public class FhirResourcesDataProvider inherits FhirRestDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "resources",
            "desc": "FHIR resource operations data provider",
            "type": "FhirResourcesDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
        };
    }

    #! Creates the object with the given FHIR REST client
    constructor(FhirRestClient::FhirRestClient fhir) : FhirRestDataProviderBase(fhir) {
    }

    #! Returns the data provider name
    string getName() {
        return "resources";
    }

    #! Returns child data provider names
    private *list<string> getChildProviderNamesImpl() {
        return ("read", "search", "search-all", "create", "submit-bundle", "update", "conditional-update", "patch",
                "delete", "validate")
            + getSupportedGeneratedResourcePathNames();
    }

    #! Returns the given child provider or \c NOTHING if the given child is unknown
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        if (*string resource_type = getFhirR4GeneratedResourceTypeForPathName(name)) {
            return fhir.isCapabilityStatementResourceType(resource_type)
                ? new FhirR4ResourceDataProvider(fhir, resource_type) : NOTHING;
        }
        switch (name) {
            case "read":
                return new FhirReadResourceDataProvider(fhir);
            case "search":
                return new FhirSearchResourcesDataProvider(fhir);
            case "search-all":
                return new FhirSearchAllResourcesDataProvider(fhir);
            case "create":
                return new FhirCreateResourceDataProvider(fhir);
            case "submit-bundle":
                return new FhirSubmitBundleDataProvider(fhir);
            case "update":
                return new FhirUpdateResourceDataProvider(fhir);
            case "conditional-update":
                return new FhirConditionalUpdateResourceDataProvider(fhir);
            case "patch":
                return new FhirPatchResourceDataProvider(fhir);
            case "delete":
                return new FhirDeleteResourceDataProvider(fhir);
            case "validate":
                return new FhirValidateResourceDataProvider(fhir);
        }
        return NOTHING;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! FHIR terminology container data provider
public class FhirTerminologyDataProvider inherits FhirRestDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "terminology",
            "desc": "FHIR terminology operations data provider",
            "type": "FhirTerminologyDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
        };
    }

    #! Creates the object with the given FHIR REST client
    constructor(FhirRestClient::FhirRestClient fhir) : FhirRestDataProviderBase(fhir) {
    }

    #! Returns the data provider name
    string getName() {
        return "terminology";
    }

    #! Returns child data provider names
    private *list<string> getChildProviderNamesImpl() {
        return ("validate-code", "lookup-code", "translate-code", "expand-value-set");
    }

    #! Returns the given child provider or \c NOTHING if the given child is unknown
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        switch (name) {
            case "validate-code":
                return new FhirValidateCodeDataProvider(fhir);
            case "lookup-code":
                return new FhirLookupCodeDataProvider(fhir);
            case "translate-code":
                return new FhirTranslateCodeDataProvider(fhir);
            case "expand-value-set":
                return new FhirExpandValueSetDataProvider(fhir);
        }
        return NOTHING;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
