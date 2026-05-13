# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! EasyPost message (carrier feedback for a shipment)
public class EasyPostMessageType inherits HashDataType {
    public {
        const Fields = {
            "type": {"display_name": "Type", "type": SoftStringOrNothingType, "desc": "Message type"},
            "carrier": {"display_name": "Carrier", "type": SoftStringOrNothingType, "desc": "Carrier name"},
            "carrier_account_id": {"display_name": "Carrier Account ID", "type": SoftStringOrNothingType, "desc": "Carrier account ID (`ca_xxx`)"},
            "message": {"display_name": "Message", "type": SoftStringOrNothingType, "desc": "Message text"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
