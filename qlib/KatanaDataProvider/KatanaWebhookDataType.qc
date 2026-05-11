# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaWebhookDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntOrNothingType, "display_name": "ID",
                "short_desc": "Webhook ID", "desc": "Unique webhook identifier"},
            "url": {"type": SoftStringOrNothingType, "display_name": "URL",
                "short_desc": "Webhook URL", "desc": "URL Katana will POST events to"},
            "description": {"type": SoftStringOrNothingType, "display_name": "Description",
                "short_desc": "Description", "desc": "Description of the webhook"},
            "enabled": {"type": SoftBoolOrNothingType, "display_name": "Enabled",
                "short_desc": "Enabled", "desc": "If `True`, the webhook is active"},
            "subscribed_events": {"type": new ListDataType("KatanaWebhookSubsEventsList",
                    SoftStringType, True),
                "display_name": "Subscribed Events",
                "short_desc": "Subscribed events",
                "desc": "List of Katana event names this webhook subscribes to",
                "element_allowed_values": WebhookEventAllowed},
            "token": {"type": SoftStringOrNothingType, "display_name": "Token",
                "short_desc": "Verification token",
                "desc": "Token used to verify webhook deliveries"},
            "created_at": {"type": SoftDateOrNothingType, "display_name": "Created At",
                "short_desc": "Created", "desc": "UTC creation timestamp"},
            "updated_at": {"type": SoftDateOrNothingType, "display_name": "Updated At",
                "short_desc": "Updated", "desc": "UTC update timestamp"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaCreateWebhookRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "url": {"type": SoftStringType, "required": True, "display_name": "URL",
                "short_desc": "Webhook URL",
                "desc": "URL Katana will POST events to (required)",
                "example_value": "https://example.com/katana/webhook"},
            "subscribed_events": {"type": new ListDataType(
                    "KatanaCreateWebhookSubsEventsList", SoftStringType, False),
                "required": True, "display_name": "Subscribed Events",
                "short_desc": "Subscribed events",
                "desc": "List of Katana event names to subscribe to (required)",
                "element_allowed_values": WebhookEventAllowed},
            "description": {"type": SoftStringOrNothingType, "display_name": "Description",
                "short_desc": "Description", "desc": "Description of the webhook"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaUpdateWebhookRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True, "display_name": "Webhook ID",
                "short_desc": "Webhook ID", "desc": "ID of the webhook to update",
                "ref_data": "webhooks"},
        } + KatanaDataProviderBase::makeFieldsOptional(
            KatanaCreateWebhookRequestDataType::Fields) + {
            "enabled": {"type": SoftBoolOrNothingType, "display_name": "Enabled",
                "short_desc": "Enabled", "desc": "If `True`, the webhook is active"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaDeleteWebhookRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True, "display_name": "Webhook ID",
                "short_desc": "Webhook ID", "desc": "ID of the webhook to delete",
                "ref_data": "webhooks"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace KatanaDataProvider
