# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Tax identifier (used on customs / international shipments)
public class EasyPostTaxIdentifierType inherits HashDataType {
    public {
        const Fields = {
            "entity": {
                "display_name": "Entity",
                "short_desc": "Whether the tax ID is the sender's or receiver's",
                "desc": "Whether the tax ID belongs to the sender or the receiver of the shipment.",
                "type": SoftStringType,
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "SENDER", "desc": "Sender"},
                    <AllowedValueInfo>{"value": "RECEIVER", "desc": "Receiver"},
                ),
            },
            "tax_id_type": {
                "display_name": "Tax ID Type",
                "short_desc": "Type of tax identifier",
                "desc": "Type of tax identifier",
                "type": SoftStringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "IOSS", "desc": "Import One-Stop Shop"},
                    <AllowedValueInfo>{"value": "VAT", "desc": "Value Added Tax"},
                    <AllowedValueInfo>{"value": "EIN", "desc": "Employer Identification Number"},
                    <AllowedValueInfo>{"value": "VOEC", "desc": "VAT On E-Commerce (Norway)"},
                ),
            },
            "tax_id": {
                "display_name": "Tax ID",
                "short_desc": "The tax identifier value",
                "desc": "The tax identifier value",
                "type": SoftStringOrNothingType,
            },
            "issuing_country": {
                "display_name": "Issuing Country",
                "short_desc": "ISO 2-letter issuing country code",
                "desc": "ISO 2-letter country code of the tax authority",
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

} # namespace EasyPostDataProvider
