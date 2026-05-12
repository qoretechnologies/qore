# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! EasyPost carrier account record
public class EasyPostCarrierAccountType inherits HashDataType {
    public {
        const Fields = {
            "id": {"display_name": "ID", "type": SoftStringOrNothingType, "desc": "Carrier account ID (`ca_xxx`)"},
            "object": {"display_name": "Object Type", "type": SoftStringOrNothingType, "desc": "Always `CarrierAccount`"},
            "type": {"display_name": "Type", "type": SoftStringOrNothingType, "desc": "Carrier account type (e.g. `UpsAccount`, `FedExAccount`)"},
            "fields": {"display_name": "Fields", "type": AutoHashOrNothingType, "desc": "Carrier-specific field metadata returned by the API"},
            "clone": {"display_name": "Clone", "type": SoftBoolOrNothingType, "desc": "Whether this is a cloned account"},
            "description": {"display_name": "Description", "type": SoftStringOrNothingType, "desc": "Account description"},
            "reference": {"display_name": "Reference", "type": SoftStringOrNothingType, "desc": "Caller-supplied reference"},
            "readable": {"display_name": "Readable Name", "type": SoftStringOrNothingType, "desc": "Human-friendly carrier name"},
            "logo": {"display_name": "Logo", "type": SoftStringOrNothingType, "desc": "URL to the carrier logo"},
            "credentials": {"display_name": "Credentials", "type": AutoHashOrNothingType, "desc": "Production credentials hash", "sensitive": True},
            "test_credentials": {"display_name": "Test Credentials", "type": AutoHashOrNothingType, "desc": "Test credentials hash", "sensitive": True},
            "billing_type": {"display_name": "Billing Type", "type": SoftStringOrNothingType, "desc": "Billing type"},
            "created_at": {"display_name": "Created At", "type": SoftDateOrNothingType, "desc": "Creation timestamp"},
            "updated_at": {"display_name": "Updated At", "type": SoftDateOrNothingType, "desc": "Last-update timestamp"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
