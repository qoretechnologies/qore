# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Allowed values for verify entries
const VerifyAllowedValues = (
    <AllowedValueInfo>{"value": "delivery", "desc": "Delivery verification"},
    <AllowedValueInfo>{"value": "zip4", "desc": "ZIP+4 verification (USPS)"},
);

#! Request type for creating an address
public class EasyPostCreateAddressRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "street1": {
                "display_name": "Street 1",
                "short_desc": "First line of the street address",
                "desc": "First line of the street address",
                "type": SoftStringType,
                "required": True,
                "preselected": True,
            },
            "street2": {
                "display_name": "Street 2",
                "short_desc": "Second line of the street address",
                "desc": "Second line of the street address (apartment, suite, etc.)",
                "type": SoftStringOrNothingType,
            },
            "city": {
                "display_name": "City",
                "short_desc": "City",
                "desc": "City name",
                "type": SoftStringType,
                "required": True,
                "preselected": True,
            },
            "state": {
                "display_name": "State",
                "short_desc": "State or province code",
                "desc": "State or province code (e.g. `CA`, `NY`)",
                "type": SoftStringType,
                "required": True,
                "preselected": True,
            },
            "zip": {
                "display_name": "ZIP / Postal Code",
                "short_desc": "ZIP or postal code",
                "desc": "ZIP / postal code",
                "type": SoftStringType,
                "required": True,
                "preselected": True,
            },
            "country": {
                "display_name": "Country",
                "short_desc": "ISO 2-letter country code",
                "desc": "ISO 2-letter country code (e.g. `US`, `GB`). Defaults to `US`.",
                "type": SoftStringType,
                "required": True,
                "default_value": "US",
                "preselected": True,
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Recipient name",
                "desc": "Recipient name",
                "type": SoftStringOrNothingType,
                "preselected": True,
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
                "desc": "Federal tax identifier",
                "type": SoftStringOrNothingType,
            },
            "state_tax_id": {
                "display_name": "State Tax ID",
                "short_desc": "State tax identifier",
                "desc": "State tax identifier",
                "type": SoftStringOrNothingType,
            },
            "verify": {
                "display_name": "Verify",
                "short_desc": "Verifications to perform (lenient)",
                "desc": "Verifications to perform; the API still creates the address even if verification fails. "
                    "Allowed entries: `delivery`, `zip4`.",
                "type": new ListDataType("EasyPostAddressVerifyList", SoftStringType, True),
                "element_allowed_values": VerifyAllowedValues,
            },
            "verify_strict": {
                "display_name": "Verify Strict",
                "short_desc": "Verifications to perform (strict)",
                "desc": "Verifications to perform; the API rejects the request if any strict verification fails. "
                    "Allowed entries: `delivery`, `zip4`.",
                "type": new ListDataType("EasyPostAddressVerifyStrictList", SoftStringType, True),
                "element_allowed_values": VerifyAllowedValues,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating an address
public class EasyPostCreateAddressDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-address",
            "desc": "Create a new EasyPost address",
            "type": "EasyPostCreateAddressDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostCreateAddressRequestDataType RequestType();
        static EasyPostAddressType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "create-address";
    }

    *string getDesc() {
        return "Create a new EasyPost address";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = req ?? {};
        hash<auto> inner = body - ("verify", "verify_strict");
        hash<auto> extra = {};
        if (body.verify) {
            extra.verify = body.verify;
        }
        if (body.verify_strict) {
            extra.verify_strict = body.verify_strict;
        }
        return doPostWrapped("addresses", WrapperKeyAddress, inner, extra ?? NOTHING);
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace EasyPostDataProvider
