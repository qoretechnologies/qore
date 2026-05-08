# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! EasyPost insurance record
public class EasyPostInsuranceType inherits HashDataType {
    public {
        const Fields = {
            "id": {"display_name": "ID", "type": SoftStringOrNothingType, "desc": "Insurance ID (`ins_xxx`)"},
            "object": {"display_name": "Object Type", "type": SoftStringOrNothingType, "desc": "Always `Insurance`"},
            "mode": {"display_name": "Mode", "type": SoftStringOrNothingType, "desc": "`production` or `test`"},
            "reference": {"display_name": "Reference", "type": SoftStringOrNothingType, "desc": "Caller-supplied reference"},
            "amount": {"display_name": "Amount", "type": SoftStringOrNothingType, "desc": "Insured amount as a string (e.g. `\"100.00\"`)"},
            "provider": {"display_name": "Provider", "type": SoftStringOrNothingType, "desc": "Insurance provider name"},
            "provider_id": {"display_name": "Provider ID", "type": SoftStringOrNothingType, "desc": "Provider-specific identifier"},
            "status": {
                "display_name": "Status",
                "type": SoftStringOrNothingType,
                "desc": "Insurance status",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "new", "desc": "New"},
                    <AllowedValueInfo>{"value": "pending", "desc": "Pending"},
                    <AllowedValueInfo>{"value": "purchased", "desc": "Purchased"},
                    <AllowedValueInfo>{"value": "failed", "desc": "Failed"},
                    <AllowedValueInfo>{"value": "cancelled", "desc": "Cancelled"},
                ),
            },
            "tracking_code": {"display_name": "Tracking Code", "type": SoftStringOrNothingType, "desc": "Carrier tracking code"},
            "tracker": {"display_name": "Tracker", "type": AutoHashOrNothingType, "desc": "Associated tracker (embedded)"},
            "to_address": {"display_name": "To Address", "type": new EasyPostAddressType(), "desc": "Destination address"},
            "from_address": {"display_name": "From Address", "type": new EasyPostAddressType(), "desc": "Origin address"},
            "fee": {"display_name": "Fee", "type": new EasyPostFeeType(), "desc": "Fee charged for this insurance"},
            "messages": {"display_name": "Messages", "type": new ListDataType("EasyPostInsuranceMessageList", new EasyPostMessageType(), True), "desc": "Messages from the insurance provider"},
            "created_at": {"display_name": "Created At", "type": SoftDateOrNothingType, "desc": "Creation timestamp"},
            "updated_at": {"display_name": "Updated At", "type": SoftDateOrNothingType, "desc": "Last-update timestamp"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
