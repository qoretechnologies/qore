# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for creating a webhook
public class EasyPostCreateWebhookRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "url": {
                "display_name": "URL",
                "short_desc": "Webhook target URL",
                "desc": "URL to receive webhook callbacks",
                "type": SoftStringType,
                "required": True,
                "preselected": True,
            },
            "webhook_secret": {
                "display_name": "Webhook Secret",
                "short_desc": "Optional shared secret",
                "desc": "Optional shared secret used to sign webhook payloads (HMAC-SHA-256)",
                "type": SoftStringOrNothingType,
            },
            "custom_headers": {
                "display_name": "Custom Headers",
                "short_desc": "Custom HTTP headers",
                "desc": "Custom HTTP headers to send with webhook callbacks",
                "type": new ListDataType("EasyPostCreateWebhookHeaderList", new EasyPostCustomHeaderType(), True),
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating a webhook
public class EasyPostCreateWebhookDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-webhook",
            "desc": "Create a new webhook subscription",
            "type": "EasyPostCreateWebhookDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostCreateWebhookRequestDataType RequestType();
        static EasyPostWebhookType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "create-webhook";
    }

    *string getDesc() {
        return "Create a new webhook subscription";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPostWrapped("webhooks", WrapperKeyWebhook, req);
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
