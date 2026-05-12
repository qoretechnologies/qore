# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for updating a webhook
public class EasyPostUpdateWebhookRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "webhook_id": {
                "display_name": "Webhook ID",
                "short_desc": "Webhook ID (hook_xxx)",
                "desc": "Webhook to update",
                "type": SoftStringType,
                "required": True,
                "ref_data": "webhooks",
            },
            "webhook_secret": {
                "display_name": "Webhook Secret",
                "short_desc": "Shared secret",
                "desc": "Optional shared secret used to sign webhook payloads",
                "type": SoftStringOrNothingType,
                "sensitive": True,
            },
            "custom_headers": {
                "display_name": "Custom Headers",
                "short_desc": "Custom HTTP headers",
                "desc": "Custom HTTP headers to send with webhook callbacks",
                "type": new ListDataType("EasyPostUpdateWebhookHeaderList", new EasyPostCustomHeaderType(), True),
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for updating a webhook
public class EasyPostUpdateWebhookDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-webhook",
            "desc": "Update an existing webhook",
            "type": "EasyPostUpdateWebhookDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostUpdateWebhookRequestDataType RequestType();
        static EasyPostWebhookType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "update-webhook";
    }

    *string getDesc() {
        return "Update an existing webhook";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = req ?? {};
        string id = body.webhook_id;
        return doPatch(sprintf("webhooks/%s", id), body - "webhook_id");
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
