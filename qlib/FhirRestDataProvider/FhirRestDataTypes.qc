# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FHIR REST data provider data types

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
#! Generic FHIR resource data type
public class FhirResourceDataType inherits HashDataType {
    private {
        const Fields = {
            "resourceType": {
                "type": StringType,
                "display_name": "Resource Type",
                "short_desc": "FHIR resource type",
                "desc": "FHIR resource type, for example `Patient`, `Observation`, or `Bundle`",
                "example_value": "Patient",
            },
            "id": {
                "type": StringOrNothingType,
                "display_name": "ID",
                "short_desc": "Logical resource ID",
                "desc": "FHIR logical resource ID",
                "example_value": "example",
            },
            "meta": {
                "type": AutoHashOrNothingType,
                "display_name": "Metadata",
                "short_desc": "FHIR resource metadata",
                "desc": "FHIR `meta` element with version, profile, security, or tag information",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

#! Generic FHIR resource data type constant
public const FhirResourceDataType = new FhirResourceDataType();

#! Generic FHIR Bundle data type
public class FhirBundleDataType inherits HashDataType {
    private {
        const Fields = {
            "resourceType": {
                "type": StringType,
                "display_name": "Resource Type",
                "short_desc": "FHIR resource type",
                "desc": "FHIR resource type; should be `Bundle` for search responses",
                "example_value": "Bundle",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Bundle Type",
                "short_desc": "FHIR Bundle type",
                "desc": "FHIR Bundle type, for example `searchset`",
                "example_value": "searchset",
            },
            "total": {
                "type": SoftIntOrNothingType,
                "display_name": "Total",
                "short_desc": "Total search matches",
                "desc": "Total number of matches reported by the server, if supplied",
            },
            "link": {
                "type": AutoListOrNothingType,
                "display_name": "Links",
                "short_desc": "Bundle pagination links",
                "desc": "FHIR Bundle pagination links such as `self`, `next`, and `previous`",
            },
            "entry": {
                "type": AutoListOrNothingType,
                "display_name": "Entries",
                "short_desc": "Bundle entries",
                "desc": "FHIR Bundle entries containing matching resources",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

#! Generic FHIR Bundle data type constant
public const FhirBundleDataType = new FhirBundleDataType();

#! Generic FHIR OperationOutcome data type
public class FhirOperationOutcomeDataType inherits HashDataType {
    private {
        const Fields = {
            "resourceType": {
                "type": StringType,
                "display_name": "Resource Type",
                "short_desc": "FHIR resource type",
                "desc": "FHIR resource type; should be `OperationOutcome`",
                "example_value": "OperationOutcome",
            },
            "issue": {
                "type": AutoListType,
                "display_name": "Issues",
                "short_desc": "Outcome issues",
                "desc": "List of FHIR OperationOutcome issues",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

#! Generic FHIR OperationOutcome data type constant
public const FhirOperationOutcomeDataType = new FhirOperationOutcomeDataType();

#! FHIR CapabilityStatement data type
public class FhirCapabilityStatementDataType inherits HashDataType {
    private {
        const Fields = {
            "resourceType": {
                "type": StringType,
                "display_name": "Resource Type",
                "short_desc": "FHIR resource type",
                "desc": "FHIR resource type; should be `CapabilityStatement`",
                "example_value": "CapabilityStatement",
            },
            "fhirVersion": {
                "type": StringOrNothingType,
                "display_name": "FHIR Version",
                "short_desc": "FHIR version",
                "desc": "FHIR version reported by the server",
                "example_value": "4.0.1",
            },
            "rest": {
                "type": AutoListOrNothingType,
                "display_name": "REST Metadata",
                "short_desc": "FHIR REST capabilities",
                "desc": "FHIR REST capability metadata advertised by the server",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

#! FHIR CapabilityStatement data type constant
public const FhirCapabilityStatementDataType = new FhirCapabilityStatementDataType();

#! SMART on FHIR configuration data type
public class FhirSmartConfigurationDataType inherits HashDataType {
    private {
        const Fields = {
            "authorization_endpoint": {
                "type": StringOrNothingType,
                "display_name": "Authorization Endpoint",
                "short_desc": "OAuth2 authorization endpoint",
                "desc": "SMART on FHIR OAuth2 authorization endpoint",
            },
            "token_endpoint": {
                "type": StringOrNothingType,
                "display_name": "Token Endpoint",
                "short_desc": "OAuth2 token endpoint",
                "desc": "SMART on FHIR OAuth2 token endpoint",
            },
            "capabilities": {
                "type": AutoListOrNothingType,
                "display_name": "Capabilities",
                "short_desc": "SMART capabilities",
                "desc": "SMART on FHIR capabilities advertised by the server",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

#! SMART on FHIR configuration data type constant
public const FhirSmartConfigurationDataType = new FhirSmartConfigurationDataType();

#! FHIR Parameters data type
public class FhirParametersDataType inherits HashDataType {
    private {
        const Fields = {
            "resourceType": {
                "type": StringType,
                "display_name": "Resource Type",
                "short_desc": "FHIR resource type",
                "desc": "FHIR resource type; should be `Parameters`",
                "example_value": "Parameters",
            },
            "parameter": {
                "type": AutoListOrNothingType,
                "display_name": "Parameters",
                "short_desc": "Operation parameters",
                "desc": "FHIR operation response parameters",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

#! FHIR Parameters data type constant
public const FhirParametersDataType = new FhirParametersDataType();

#! FHIR REST response data type
public class FhirRestResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "status_code": {
                "type": SoftIntType,
                "display_name": "Status Code",
                "short_desc": "HTTP status code",
                "desc": "HTTP response status code",
                "example_value": 200,
            },
            "headers": {
                "type": AutoHashType,
                "display_name": "Headers",
                "short_desc": "HTTP response headers",
                "desc": "HTTP response headers",
            },
            "body": {
                "type": AutoHashOrNothingType,
                "display_name": "Body",
                "short_desc": "FHIR response body",
                "desc": "Parsed FHIR response body, if supplied by the server",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! FHIR REST response data type constant
public const FhirRestResponseDataType = new FhirRestResponseDataType();
}
