# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Allowed values for event status
const EventStatusAllowedValues = (
    <AllowedValueInfo>{"value": "completed", "desc": "Completed"},
    <AllowedValueInfo>{"value": "failed", "desc": "Failed"},
    <AllowedValueInfo>{"value": "in_queue", "desc": "In queue"},
    <AllowedValueInfo>{"value": "retrying", "desc": "Retrying"},
);

#! EasyPost event record
public class EasyPostEventType inherits HashDataType {
    public {
        const Fields = {
            "id": {"display_name": "ID", "type": SoftStringOrNothingType, "desc": "Event ID (`evt_xxx`)"},
            "object": {"display_name": "Object Type", "type": SoftStringOrNothingType, "desc": "Always `Event`"},
            "mode": {"display_name": "Mode", "type": SoftStringOrNothingType, "desc": "`production` or `test`"},
            "description": {
                "display_name": "Description",
                "type": SoftStringOrNothingType,
                "desc": "Event type / description (e.g. `tracker.updated`, `shipment.purchased`)",
            },
            "previous_attributes": {
                "display_name": "Previous Attributes",
                "type": AutoHashOrNothingType,
                "desc": "Hash of attribute values before the change (only on updates)",
            },
            "pending_urls": {
                "display_name": "Pending URLs",
                "type": new ListDataType("EasyPostEventPendingUrlList", SoftStringType, True),
                "desc": "Pending webhook delivery URLs",
            },
            "completed_urls": {
                "display_name": "Completed URLs",
                "type": new ListDataType("EasyPostEventCompletedUrlList", SoftStringType, True),
                "desc": "Completed webhook delivery URLs",
            },
            "result": {
                "display_name": "Result",
                "type": AutoHashOrNothingType,
                "desc": "Resulting object payload (e.g. tracker, batch, refund)",
            },
            "status": {
                "display_name": "Status",
                "type": SoftStringOrNothingType,
                "desc": "Event delivery status",
                "allowed_values": EventStatusAllowedValues,
            },
            "user_id": {"display_name": "User ID", "type": SoftStringOrNothingType, "desc": "User ID associated with the event"},
            "created_at": {"display_name": "Created At", "type": SoftDateOrNothingType, "desc": "Creation timestamp"},
            "updated_at": {"display_name": "Updated At", "type": SoftDateOrNothingType, "desc": "Last-update timestamp"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
