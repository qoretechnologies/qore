# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for retrieving a webhook
public class EasyPostGetWebhookRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "webhook_id": {
                "display_name": "Webhook ID",
                "short_desc": "Webhook ID (hook_xxx)",
                "desc": "Webhook to retrieve",
                "type": SoftStringType,
                "required": True,
                "ref_data": "webhooks",
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for retrieving a webhook
public class EasyPostGetWebhookDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-webhook",
            "desc": "Retrieve a webhook by ID",
            "type": "EasyPostGetWebhookDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostGetWebhookRequestDataType RequestType();
        static EasyPostWebhookType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "get-webhook";
    }

    *string getDesc() {
        return "Retrieve a webhook by ID";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doGet(sprintf("webhooks/%s", req.webhook_id));
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
