# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FHIR REST data provider request data types

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
#! Read resource request data type
public class FhirReadResourceRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "resource_type": {
                "type": StringType,
                "display_name": "Resource Type",
                "short_desc": "FHIR resource type",
                "desc": "FHIR resource type, for example `Patient` or `Observation`",
                "example_value": "Patient",
            },
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "FHIR logical ID",
                "desc": "FHIR logical ID of the resource instance",
                "example_value": "example",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Search resources request data type
public class FhirSearchResourcesRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "resource_type": {
                "type": StringType,
                "display_name": "Resource Type",
                "short_desc": "FHIR resource type",
                "desc": "FHIR resource type to search",
                "example_value": "Patient",
            },
            "params": {
                "type": AutoHashOrNothingType,
                "display_name": "Search Parameters",
                "short_desc": "FHIR search parameters",
                "desc": "FHIR search parameters as a hash. Keys are FHIR parameter names such as `family`, "
                    "`_include`, or `_revinclude`; list values are sent as repeated query parameters.\n\n"
                    "**Example**: `{\"family\": \"Smith\"}`",
                "example_value": {"family": "Smith"},
            },
            "id": {
                "type": StringOrNothingType,
                "display_name": "ID Search",
                "short_desc": "FHIR _id search parameter",
                "desc": "Convenience field mapped to the FHIR `_id` search parameter",
                "example_value": "example",
            },
            "last_updated": {
                "type": StringOrNothingType,
                "display_name": "Last Updated",
                "short_desc": "FHIR _lastUpdated search parameter",
                "desc": "Convenience field mapped to the FHIR `_lastUpdated` search parameter",
                "example_value": "ge2026-05-01",
            },
            "tag": {
                "type": AutoType,
                "display_name": "Tag",
                "short_desc": "FHIR _tag search parameter",
                "desc": "Convenience field mapped to the FHIR `_tag` search parameter",
                "example_value": "http://example.org/tags|reviewed",
            },
            "profile": {
                "type": AutoType,
                "display_name": "Profile",
                "short_desc": "FHIR _profile search parameter",
                "desc": "Convenience field mapped to the FHIR `_profile` search parameter",
                "example_value": "http://hl7.org/fhir/us/core/StructureDefinition/us-core-patient",
            },
            "security": {
                "type": AutoType,
                "display_name": "Security",
                "short_desc": "FHIR _security search parameter",
                "desc": "Convenience field mapped to the FHIR `_security` search parameter",
                "example_value": "http://terminology.hl7.org/CodeSystem/v3-ActCode|N",
            },
            "text": {
                "type": StringOrNothingType,
                "display_name": "Text",
                "short_desc": "FHIR _text search parameter",
                "desc": "Convenience field mapped to the FHIR `_text` full-text search parameter",
                "example_value": "Smith",
            },
            "content": {
                "type": StringOrNothingType,
                "display_name": "Content",
                "short_desc": "FHIR _content search parameter",
                "desc": "Convenience field mapped to the FHIR `_content` full-content search parameter",
                "example_value": "diabetes",
            },
            "count": {
                "type": IntOrNothingType,
                "display_name": "Count",
                "short_desc": "FHIR _count search parameter",
                "desc": "Convenience field mapped to the FHIR `_count` page-size search parameter",
                "example_value": 50,
            },
            "sort": {
                "type": AutoType,
                "display_name": "Sort",
                "short_desc": "FHIR _sort search parameter",
                "desc": "Convenience field mapped to the FHIR `_sort` search parameter",
                "example_value": "-_lastUpdated",
            },
            "include": {
                "type": AutoType,
                "display_name": "Include",
                "short_desc": "FHIR _include search parameter",
                "desc": "Convenience field mapped to the FHIR `_include` search parameter; use a list for "
                    "repeated include values",
                "example_value": ("Patient:general-practitioner",),
            },
            "revinclude": {
                "type": AutoType,
                "display_name": "RevInclude",
                "short_desc": "FHIR _revinclude search parameter",
                "desc": "Convenience field mapped to the FHIR `_revinclude` search parameter; use a list for "
                    "repeated reverse include values",
                "example_value": ("Observation:subject",),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Search all resources request data type
public class FhirSearchAllResourcesRequestDataType inherits HashDataType {
    public {
        const Fields = FhirSearchResourcesRequestDataType::Fields + {
            "max_pages": {
                "type": IntOrNothingType,
                "display_name": "Maximum Pages",
                "short_desc": "Maximum Bundle pages to fetch",
                "desc": "Maximum number of Bundle pages to fetch by following `next` pagination links",
                "example_value": 100,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Create resource request data type
public class FhirCreateResourceRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "resource_type": {
                "type": StringType,
                "display_name": "Resource Type",
                "short_desc": "FHIR resource type",
                "desc": "FHIR resource type to create",
                "example_value": "Patient",
            },
            "resource": {
                "type": AutoHashType,
                "display_name": "Resource",
                "short_desc": "FHIR resource body",
                "desc": "FHIR resource body as a JSON-compatible hash. The hash must include `resourceType`.\n\n"
                    "**Example**: `{\"resourceType\": \"Patient\", \"name\": [{\"family\": \"Jones\"}]}`",
                "example_value": {"resourceType": "Patient", "name": ({"family": "Jones"},)},
            },
            "if_none_exist": {
                "type": StringOrNothingType,
                "display_name": "If-None-Exist",
                "short_desc": "Conditional create criteria",
                "desc": "FHIR conditional create search expression sent in the `If-None-Exist` header",
                "example_value": "identifier=http://hospital.example/mrn|12345",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Submit Bundle request data type
public class FhirSubmitBundleRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "bundle": {
                "type": AutoHashType,
                "display_name": "Bundle",
                "short_desc": "FHIR transaction or batch Bundle",
                "desc": "FHIR `Bundle` resource with `type` set to `transaction` or `batch`. The Bundle is "
                    "POSTed to the FHIR service base for atomic transaction writes or batch execution.\n\n"
                    "**Example**: `{\"resourceType\": \"Bundle\", \"type\": \"transaction\", \"entry\": [...]}`",
                "example_value": {
                    "resourceType": "Bundle",
                    "type": "transaction",
                    "entry": (),
                },
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Update resource request data type
public class FhirUpdateResourceRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "resource_type": {
                "type": StringType,
                "display_name": "Resource Type",
                "short_desc": "FHIR resource type",
                "desc": "FHIR resource type to update",
                "example_value": "Patient",
            },
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "FHIR logical ID",
                "desc": "FHIR logical ID of the resource instance",
                "example_value": "example",
            },
            "resource": {
                "type": AutoHashType,
                "display_name": "Resource",
                "short_desc": "FHIR resource body",
                "desc": "FHIR resource body as a JSON-compatible hash. The hash must include `resourceType` "
                    "and should include the logical `id` being updated.\n\n"
                    "**Example**: `{\"resourceType\": \"Patient\", \"id\": \"example\", \"active\": True}`",
                "example_value": {"resourceType": "Patient", "id": "example", "active": True},
            },
            "etag": {
                "type": StringOrNothingType,
                "display_name": "ETag",
                "short_desc": "FHIR version ETag",
                "desc": "ETag value sent in the `If-Match` header for optimistic concurrency",
                "example_value": "W/\"1\"",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Conditional update resource request data type
public class FhirConditionalUpdateResourceRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "resource_type": {
                "type": StringType,
                "display_name": "Resource Type",
                "short_desc": "FHIR resource type",
                "desc": "FHIR resource type to update conditionally",
                "example_value": "Patient",
            },
            "search_params": {
                "type": AutoHashType,
                "display_name": "Search Parameters",
                "short_desc": "Conditional update search parameters",
                "desc": "FHIR search parameters used in the conditional update request URL",
                "example_value": {"identifier": "http://hospital.example/mrn|12345"},
            },
            "resource": {
                "type": AutoHashType,
                "display_name": "Resource",
                "short_desc": "FHIR resource body",
                "desc": "FHIR resource body as a JSON-compatible hash. The hash must include `resourceType` "
                    "matching `resource_type`.",
                "example_value": {"resourceType": "Patient", "active": True},
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Patch resource request data type
public class FhirPatchResourceRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "resource_type": {
                "type": StringType,
                "display_name": "Resource Type",
                "short_desc": "FHIR resource type",
                "desc": "FHIR resource type to patch",
                "example_value": "Patient",
            },
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "FHIR logical ID",
                "desc": "FHIR logical ID of the resource instance",
                "example_value": "example",
            },
            "patch": {
                "type": AutoType,
                "display_name": "Patch",
                "short_desc": "Patch document body",
                "desc": "FHIR patch document body. Use `content_type` to choose JSON Patch, XML Patch, or "
                    "FHIRPath Patch encoding.",
                "example_value": ({"op": "replace", "path": "/active", "value": False},),
            },
            "content_type": {
                "type": StringOrNothingType,
                "display_name": "Content Type",
                "short_desc": "Patch document content type",
                "desc": "Patch document content type; defaults to `application/json-patch+json`",
                "example_value": "application/json-patch+json",
            },
            "etag": {
                "type": StringOrNothingType,
                "display_name": "ETag",
                "short_desc": "FHIR version ETag",
                "desc": "ETag value sent in the `If-Match` header for optimistic concurrency",
                "example_value": "W/\"1\"",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Delete resource request data type
public class FhirDeleteResourceRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "resource_type": {
                "type": StringType,
                "display_name": "Resource Type",
                "short_desc": "FHIR resource type",
                "desc": "FHIR resource type to delete",
                "example_value": "Patient",
            },
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "FHIR logical ID",
                "desc": "FHIR logical ID of the resource instance",
                "example_value": "example",
            },
            "etag": {
                "type": StringOrNothingType,
                "display_name": "ETag",
                "short_desc": "FHIR version ETag",
                "desc": "ETag value sent in the `If-Match` header for optimistic concurrency",
                "example_value": "W/\"1\"",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Validate resource request data type
public class FhirValidateResourceRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "resource": {
                "type": AutoHashType,
                "display_name": "Resource",
                "short_desc": "FHIR resource body",
                "desc": "FHIR resource body to validate. The hash must include `resourceType`.\n\n"
                    "**Example**: `{\"resourceType\": \"Patient\", \"id\": \"example\"}`",
                "example_value": {"resourceType": "Patient", "id": "example"},
            },
            "resource_type": {
                "type": StringOrNothingType,
                "display_name": "Resource Type",
                "short_desc": "FHIR resource type",
                "desc": "Optional FHIR resource type for type-level or instance-level validation",
                "example_value": "Patient",
            },
            "id": {
                "type": StringOrNothingType,
                "display_name": "ID",
                "short_desc": "FHIR logical ID",
                "desc": "Optional FHIR logical ID for instance-level validation",
                "example_value": "example",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Validate code request data type
public class FhirValidateCodeRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "path": {
                "type": StringType,
                "display_name": "Path",
                "short_desc": "CodeSystem or ValueSet path",
                "desc": "Relative FHIR path to a CodeSystem or ValueSet endpoint",
                "example_value": "CodeSystem/example",
            },
            "params": {
                "type": AutoHashType,
                "display_name": "Parameters",
                "short_desc": "Operation parameters",
                "desc": "FHIR `$validate-code` operation parameters as query parameters. Keys are operation "
                    "parameter names accepted by the server.\n\n"
                    "**Example**: `{\"code\": \"abc\"}`",
                "example_value": {"code": "abc"},
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Lookup code request data type
public class FhirLookupCodeRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "path": {
                "type": StringType,
                "display_name": "Path",
                "short_desc": "CodeSystem path",
                "desc": "Relative FHIR path to a CodeSystem endpoint",
                "example_value": "CodeSystem/example",
            },
            "params": {
                "type": AutoHashType,
                "display_name": "Parameters",
                "short_desc": "Operation parameters",
                "desc": "FHIR `$lookup` operation parameters as query parameters. Keys are operation "
                    "parameter names accepted by the server.\n\n"
                    "**Example**: `{\"system\": \"http://loinc.org\", \"code\": \"8867-4\"}`",
                "example_value": {"system": "http://loinc.org", "code": "8867-4"},
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Translate code request data type
public class FhirTranslateCodeRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "path": {
                "type": StringType,
                "display_name": "Path",
                "short_desc": "ConceptMap path",
                "desc": "Relative FHIR path to a ConceptMap endpoint",
                "example_value": "ConceptMap/example",
            },
            "params": {
                "type": AutoHashType,
                "display_name": "Parameters",
                "short_desc": "Operation parameters",
                "desc": "FHIR `$translate` operation parameters as query parameters. Keys are operation "
                    "parameter names accepted by the server.\n\n"
                    "**Example**: `{\"system\": \"http://example.org/local\", \"code\": \"abc\"}`",
                "example_value": {"system": "http://example.org/local", "code": "abc"},
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Expand ValueSet request data type
public class FhirExpandValueSetRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "path": {
                "type": StringType,
                "display_name": "Path",
                "short_desc": "ValueSet path",
                "desc": "Relative FHIR path to a ValueSet endpoint",
                "example_value": "ValueSet/example",
            },
            "params": {
                "type": AutoHashOrNothingType,
                "display_name": "Parameters",
                "short_desc": "Operation parameters",
                "desc": "Optional FHIR `$expand` operation parameters as query parameters. Keys are "
                    "operation parameter names accepted by the server.\n\n"
                    "**Example**: `{\"filter\": \"smi\"}`",
                "example_value": {"filter": "smi"},
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

public const FhirReadResourceRequestDataType = new FhirReadResourceRequestDataType();
public const FhirSearchResourcesRequestDataType = new FhirSearchResourcesRequestDataType();
public const FhirSearchAllResourcesRequestDataType = new FhirSearchAllResourcesRequestDataType();
public const FhirCreateResourceRequestDataType = new FhirCreateResourceRequestDataType();
public const FhirSubmitBundleRequestDataType = new FhirSubmitBundleRequestDataType();
public const FhirUpdateResourceRequestDataType = new FhirUpdateResourceRequestDataType();
public const FhirConditionalUpdateResourceRequestDataType = new FhirConditionalUpdateResourceRequestDataType();
public const FhirPatchResourceRequestDataType = new FhirPatchResourceRequestDataType();
public const FhirDeleteResourceRequestDataType = new FhirDeleteResourceRequestDataType();
public const FhirValidateResourceRequestDataType = new FhirValidateResourceRequestDataType();
public const FhirValidateCodeRequestDataType = new FhirValidateCodeRequestDataType();
public const FhirLookupCodeRequestDataType = new FhirLookupCodeRequestDataType();
public const FhirTranslateCodeRequestDataType = new FhirTranslateCodeRequestDataType();
public const FhirExpandValueSetRequestDataType = new FhirExpandValueSetRequestDataType();
}
