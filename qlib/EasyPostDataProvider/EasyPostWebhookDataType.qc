# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! EasyPost custom webhook header type
public class EasyPostCustomHeaderType inherits HashDataType {
    public {
        const Fields = {
            "name": {"display_name": "Name", "type": SoftStringOrNothingType, "desc": "Header name", "required": True},
            "value": {"display_name": "Value", "type": SoftStringOrNothingType, "desc": "Header value", "required": True},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

#! EasyPost webhook record
public class EasyPostWebhookType inherits HashDataType {
    public {
        const Fields = {
            "id": {"display_name": "ID", "type": SoftStringOrNothingType, "desc": "Webhook ID (`hook_xxx`)"},
            "object": {"display_name": "Object Type", "type": SoftStringOrNothingType, "desc": "Always `Webhook`"},
            "mode": {
                "display_name": "Mode",
                "type": SoftStringOrNothingType,
                "desc": "Webhook environment",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "production", "desc": "Production"},
                    <AllowedValueInfo>{"value": "test", "desc": "Test"},
                ),
            },
            "url": {"display_name": "URL", "type": SoftStringOrNothingType, "desc": "Webhook target URL"},
            "disabled_at": {"display_name": "Disabled At", "type": SoftDateOrNothingType, "desc": "When the webhook was disabled, or `null` if active"},
            "custom_headers": {"display_name": "Custom Headers", "type": new ListDataType("EasyPostWebhookHeaderList", new EasyPostCustomHeaderType(), True), "desc": "Custom HTTP headers sent with each webhook"},
            "created_at": {"display_name": "Created At", "type": SoftDateOrNothingType, "desc": "Creation timestamp"},
            "updated_at": {"display_name": "Updated At", "type": SoftDateOrNothingType, "desc": "Last-update timestamp"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
