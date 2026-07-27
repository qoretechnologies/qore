# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FHIR R4 generated resource action data types

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
#! Request data type for generated FHIR R4 resource actions
public class FhirR4GeneratedResourceRequestDataType inherits HashDataType {
    #! Creates the generated resource action request data type
    constructor(string resource_type, string operation, hash<auto> fields)
            : HashDataType(AutoHashType, sprintf("FhirR4%s%sRequest", resource_type, operation)) {
        addQoreFields(fields);
    }
}

#! Returns request fields for a generated resource read action
public hash<auto> sub getFhirR4GeneratedReadResourceRequestFields() {
    return FhirReadResourceRequestDataType::Fields - "resource_type";
}

#! Returns request fields for a generated resource search action
public hash<auto> sub getFhirR4GeneratedSearchResourcesRequestFields() {
    return FhirSearchResourcesRequestDataType::Fields - "resource_type";
}

#! Returns request fields for a generated resource paginated search action
public hash<auto> sub getFhirR4GeneratedSearchAllResourcesRequestFields() {
    return FhirSearchAllResourcesRequestDataType::Fields - "resource_type";
}

#! Returns request fields for a generated resource create action
public hash<auto> sub getFhirR4GeneratedCreateResourceRequestFields(string resource_type) {
    hash<auto> fields = FhirCreateResourceRequestDataType::Fields - "resource_type";
    fields.resource = FhirRestDataProvider::getFhirR4GeneratedTypedResourceField(resource_type,
        "Resource", "FHIR resource body", "FHIR `%s` resource body to create");
    return fields;
}

#! Returns request fields for a generated resource update action
public hash<auto> sub getFhirR4GeneratedUpdateResourceRequestFields(string resource_type) {
    hash<auto> fields = FhirUpdateResourceRequestDataType::Fields - "resource_type";
    fields.resource = FhirRestDataProvider::getFhirR4GeneratedTypedResourceField(resource_type,
        "Resource", "FHIR resource body", "FHIR `%s` resource body to update");
    return fields;
}

#! Returns request fields for a generated resource conditional update action
public hash<auto> sub getFhirR4GeneratedConditionalUpdateResourceRequestFields(string resource_type) {
    hash<auto> fields = FhirConditionalUpdateResourceRequestDataType::Fields - "resource_type";
    fields.resource = FhirRestDataProvider::getFhirR4GeneratedTypedResourceField(resource_type,
        "Resource", "FHIR resource body", "FHIR `%s` resource body to update conditionally");
    return fields;
}

#! Returns request fields for a generated resource patch action
public hash<auto> sub getFhirR4GeneratedPatchResourceRequestFields() {
    return FhirPatchResourceRequestDataType::Fields - "resource_type";
}

#! Returns request fields for a generated resource delete action
public hash<auto> sub getFhirR4GeneratedDeleteResourceRequestFields() {
    return FhirDeleteResourceRequestDataType::Fields - "resource_type";
}

#! Returns request fields for a generated resource validate action
public hash<auto> sub getFhirR4GeneratedValidateResourceRequestFields(string resource_type) {
    hash<auto> fields = FhirValidateResourceRequestDataType::Fields - "resource_type";
    fields.resource = FhirRestDataProvider::getFhirR4GeneratedTypedResourceField(resource_type,
        "Resource", "FHIR resource body", "FHIR `%s` resource body to validate");
    return fields;
}

#! Returns a typed generated resource request field
hash<auto> sub getFhirR4GeneratedTypedResourceField(string resource_type, string display_name,
        string short_desc, string desc_template) {
    *FhirR4GeneratedResourceDataType data_type = getFhirR4GeneratedResourceDataType(resource_type);
    if (!data_type) {
        throw "FHIR-RESOURCE-TYPE-ERROR",
            sprintf("resource type %y has no generated FHIR R4 data type", resource_type);
    }
    hash<auto> metadata = data_type.getResourceMetadata();
    return {
        "type": data_type,
        "display_name": display_name,
        "short_desc": short_desc,
        "desc": sprintf(desc_template, resource_type) + ". The hash must include "
            "`resourceType` with the fixed value `" + resource_type + "`.\n\n"
            "**Example**: `" + make_json(metadata.example_value) + "`",
        "example_value": metadata.example_value,
    };
}
}
