# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Base event payload type (used for all webhook events)
public class EasyPostBaseEventType inherits HashDataType {
    public {
        const Fields = {
            "id": {"display_name": "ID", "type": SoftStringOrNothingType, "desc": "Event ID (`evt_xxx`)"},
            "object": {"display_name": "Object Type", "type": SoftStringOrNothingType, "desc": "Always `Event`"},
            "mode": {"display_name": "Mode", "type": SoftStringOrNothingType, "desc": "`production` or `test`"},
            "description": {"display_name": "Description", "type": SoftStringOrNothingType, "desc": "Event type / description"},
            "previous_attributes": {"display_name": "Previous Attributes", "type": AutoHashOrNothingType, "desc": "Hash of attribute values before the change (if applicable)"},
            "pending_urls": {"display_name": "Pending URLs", "type": new ListDataType("EasyPostBaseEventPendingUrlList", SoftStringType, True), "desc": "Pending delivery URLs"},
            "completed_urls": {"display_name": "Completed URLs", "type": new ListDataType("EasyPostBaseEventCompletedUrlList", SoftStringType, True), "desc": "Completed delivery URLs"},
            "result": {"display_name": "Result", "type": AutoHashOrNothingType, "desc": "Resulting object payload"},
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
