# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! EasyPost address input type (polymorphic: id or full address fields)
public class EasyPostAddressInputType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Address ID",
                "short_desc": "Existing address ID (e.g. adr_xxx)",
                "desc": "If provided, references an existing EasyPost address; in that case the other "
                    "fields may be omitted.",
                "type": SoftStringOrNothingType,
            },
            "street1": {
                "display_name": "Street 1",
                "short_desc": "First line of the street address",
                "desc": "The first line of the street address",
                "type": SoftStringOrNothingType,
            },
            "street2": {
                "display_name": "Street 2",
                "short_desc": "Second line of the street address",
                "desc": "The second line of the street address (apartment, suite, etc.)",
                "type": SoftStringOrNothingType,
            },
            "city": {
                "display_name": "City",
                "short_desc": "City",
                "desc": "City name",
                "type": SoftStringOrNothingType,
            },
            "state": {
                "display_name": "State",
                "short_desc": "State or province code",
                "desc": "State or province code (e.g. `CA`, `NY`)",
                "type": SoftStringOrNothingType,
            },
            "zip": {
                "display_name": "ZIP / Postal Code",
                "short_desc": "ZIP or postal code",
                "desc": "ZIP / postal code",
                "type": SoftStringOrNothingType,
            },
            "country": {
                "display_name": "Country",
                "short_desc": "ISO 2-letter country code",
                "desc": "ISO 2-letter country code (e.g. `US`, `GB`). Defaults to `US`.",
                "type": SoftStringOrNothingType,
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Recipient name",
                "desc": "Recipient name",
                "type": SoftStringOrNothingType,
            },
            "company": {
                "display_name": "Company",
                "short_desc": "Company name",
                "desc": "Company name",
                "type": SoftStringOrNothingType,
            },
            "phone": {
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "Phone number",
                "type": SoftStringOrNothingType,
            },
            "email": {
                "display_name": "Email",
                "short_desc": "Email address",
                "desc": "Email address",
                "type": SoftStringOrNothingType,
            },
            "residential": {
                "display_name": "Residential",
                "short_desc": "Whether the address is residential",
                "desc": "Set to `True` if the address is residential",
                "type": SoftBoolOrNothingType,
            },
            "carrier_facility": {
                "display_name": "Carrier Facility",
                "short_desc": "Carrier facility name",
                "desc": "Optional carrier facility identifier",
                "type": SoftStringOrNothingType,
            },
            "federal_tax_id": {
                "display_name": "Federal Tax ID",
                "short_desc": "Federal tax identifier",
                "desc": "Federal tax identifier of the address holder",
                "type": SoftStringOrNothingType,
            },
            "state_tax_id": {
                "display_name": "State Tax ID",
                "short_desc": "State tax identifier",
                "desc": "State tax identifier of the address holder",
                "type": SoftStringOrNothingType,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

#! Single verification error entry (element of `verifications.delivery.errors` / `verifications.zip4.errors`)
public class EasyPostAddressVerificationErrorType inherits HashDataType {
    public {
        const Fields = {
            "code": {"display_name": "Code", "type": SoftStringOrNothingType, "desc": "Error code (e.g. `E.ADDRESS.NOT_FOUND`)"},
            "field": {"display_name": "Field", "type": SoftStringOrNothingType, "desc": "Address field the error applies to"},
            "message": {"display_name": "Message", "type": SoftStringOrNothingType, "desc": "Human-readable error description"},
            "suggestion": {"display_name": "Suggestion", "type": SoftStringOrNothingType, "desc": "Suggested correction, if any"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

#! Verification details object (e.g. `verifications.delivery.details`)
public class EasyPostAddressVerificationDetailsType inherits HashDataType {
    public {
        const Fields = {
            "latitude": {"display_name": "Latitude", "type": SoftFloatOrNothingType, "desc": "Latitude of the address"},
            "longitude": {"display_name": "Longitude", "type": SoftFloatOrNothingType, "desc": "Longitude of the address"},
            "time_zone": {"display_name": "Time Zone", "type": SoftStringOrNothingType, "desc": "Time zone of the address (e.g. `America/Los_Angeles`)"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

#! Single address verification result (delivery or zip4 sub-object of `verifications`)
public class EasyPostAddressVerificationResultType inherits HashDataType {
    public {
        const Fields = {
            "success": {"display_name": "Success", "type": SoftBoolOrNothingType, "desc": "`True` if verification succeeded"},
            "errors": {
                "display_name": "Errors",
                "type": new ListDataType("EasyPostAddressVerificationErrorList", new EasyPostAddressVerificationErrorType(), True),
                "desc": "List of verification error objects",
            },
            "details": {
                "display_name": "Details",
                "type": new EasyPostAddressVerificationDetailsType(AutoHashOrNothingType),
                "desc": "Verification details (`latitude`, `longitude`, `time_zone`)",
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

#! EasyPost address verifications object (`delivery` and `zip4` results)
public class EasyPostAddressVerificationsType inherits HashDataType {
    public {
        const Fields = {
            "delivery": {"display_name": "Delivery", "type": new EasyPostAddressVerificationResultType(AutoHashOrNothingType), "desc": "Delivery verification result"},
            "zip4": {"display_name": "ZIP+4", "type": new EasyPostAddressVerificationResultType(AutoHashOrNothingType), "desc": "ZIP+4 verification result"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

#! EasyPost full address record (response type)
public class EasyPostAddressType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "EasyPost address ID (adr_xxx)",
                "desc": "Unique EasyPost identifier (prefixed with `adr_`)",
                "type": SoftStringOrNothingType,
            },
            "object": {
                "display_name": "Object Type",
                "short_desc": "Object type (Address)",
                "desc": "Always `Address`",
                "type": SoftStringOrNothingType,
            },
            "mode": {
                "display_name": "Mode",
                "short_desc": "Environment mode",
                "desc": "Either `production` or `test`",
                "type": SoftStringOrNothingType,
            },
            "street1": {"display_name": "Street 1", "type": SoftStringOrNothingType, "desc": "First line of the street address"},
            "street2": {"display_name": "Street 2", "type": SoftStringOrNothingType, "desc": "Second line of the street address"},
            "city": {"display_name": "City", "type": SoftStringOrNothingType, "desc": "City"},
            "state": {"display_name": "State", "type": SoftStringOrNothingType, "desc": "State or province code"},
            "zip": {"display_name": "ZIP / Postal Code", "type": SoftStringOrNothingType, "desc": "ZIP or postal code"},
            "country": {"display_name": "Country", "type": SoftStringOrNothingType, "desc": "ISO 2-letter country code"},
            "name": {"display_name": "Name", "type": SoftStringOrNothingType, "desc": "Recipient name"},
            "company": {"display_name": "Company", "type": SoftStringOrNothingType, "desc": "Company name"},
            "phone": {"display_name": "Phone", "type": SoftStringOrNothingType, "desc": "Phone number"},
            "email": {"display_name": "Email", "type": SoftStringOrNothingType, "desc": "Email address"},
            "residential": {"display_name": "Residential", "type": SoftBoolOrNothingType, "desc": "Whether the address is residential"},
            "carrier_facility": {"display_name": "Carrier Facility", "type": SoftStringOrNothingType, "desc": "Carrier facility name"},
            "federal_tax_id": {"display_name": "Federal Tax ID", "type": SoftStringOrNothingType, "desc": "Federal tax identifier"},
            "state_tax_id": {"display_name": "State Tax ID", "type": SoftStringOrNothingType, "desc": "State tax identifier"},
            "verifications": {"display_name": "Verifications", "type": new EasyPostAddressVerificationsType(AutoHashOrNothingType), "desc": "Verification results from EasyPost (delivery and zip4)"},
            "created_at": {"display_name": "Created At", "type": SoftDateOrNothingType, "desc": "Creation timestamp"},
            "updated_at": {"display_name": "Updated At", "type": SoftDateOrNothingType, "desc": "Last-update timestamp"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
