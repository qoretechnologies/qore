# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Pickup rate (sub-record returned in pickups)
public class EasyPostPickupRateType inherits HashDataType {
    public {
        const Fields = {
            "id": {"display_name": "ID", "type": SoftStringOrNothingType, "desc": "Pickup rate ID (`pickuprate_xxx`)"},
            "object": {"display_name": "Object Type", "type": SoftStringOrNothingType, "desc": "Always `PickupRate`"},
            "carrier": {"display_name": "Carrier", "type": SoftStringOrNothingType, "desc": "Carrier name"},
            "service": {"display_name": "Service", "type": SoftStringOrNothingType, "desc": "Pickup service code"},
            "rate": {"display_name": "Rate", "type": SoftStringOrNothingType, "desc": "Pickup rate as a string"},
            "currency": {"display_name": "Currency", "type": SoftStringOrNothingType, "desc": "ISO currency code"},
            "pickup_id": {"display_name": "Pickup ID", "type": SoftStringOrNothingType, "desc": "Parent pickup ID"},
            "created_at": {"display_name": "Created At", "type": SoftDateOrNothingType, "desc": "Creation timestamp"},
            "updated_at": {"display_name": "Updated At", "type": SoftDateOrNothingType, "desc": "Last-update timestamp"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! EasyPost pickup record
public class EasyPostPickupType inherits HashDataType {
    public {
        const Fields = {
            "id": {"display_name": "ID", "type": SoftStringOrNothingType, "desc": "Pickup ID (`pickup_xxx`)"},
            "object": {"display_name": "Object Type", "type": SoftStringOrNothingType, "desc": "Always `Pickup`"},
            "mode": {"display_name": "Mode", "type": SoftStringOrNothingType, "desc": "`production` or `test`"},
            "reference": {"display_name": "Reference", "type": SoftStringOrNothingType, "desc": "Caller-supplied reference"},
            "status": {
                "display_name": "Status",
                "type": SoftStringOrNothingType,
                "desc": "Pickup status",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "unknown", "desc": "Unknown"},
                    <AllowedValueInfo>{"value": "scheduled", "desc": "Scheduled"},
                    <AllowedValueInfo>{"value": "canceled", "desc": "Canceled"},
                ),
            },
            "min_datetime": {"display_name": "Min Datetime", "type": SoftDateOrNothingType, "desc": "Earliest pickup datetime"},
            "max_datetime": {"display_name": "Max Datetime", "type": SoftDateOrNothingType, "desc": "Latest pickup datetime"},
            "is_account_address": {"display_name": "Is Account Address", "type": SoftBoolOrNothingType, "desc": "Whether the pickup address matches the account address"},
            "instructions": {"display_name": "Instructions", "type": SoftStringOrNothingType, "desc": "Pickup instructions"},
            "messages": {"display_name": "Messages", "type": new ListDataType("EasyPostPickupMessageList", new EasyPostMessageType(), True), "desc": "Carrier messages"},
            "confirmation": {"display_name": "Confirmation", "type": SoftStringOrNothingType, "desc": "Confirmation number from the carrier"},
            "address": {"display_name": "Address", "type": new EasyPostAddressType(), "desc": "Pickup address"},
            "carrier_accounts": {"display_name": "Carrier Accounts", "type": SoftAutoListOrNothingType, "desc": "List of carrier accounts considered for the pickup"},
            "pickup_rates": {"display_name": "Pickup Rates", "type": new ListDataType("EasyPostPickupRateList", new EasyPostPickupRateType(), True), "desc": "Available pickup rates"},
            "created_at": {"display_name": "Created At", "type": SoftDateOrNothingType, "desc": "Creation timestamp"},
            "updated_at": {"display_name": "Updated At", "type": SoftDateOrNothingType, "desc": "Last-update timestamp"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
