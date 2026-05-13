# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for deleting a webhook
public class EasyPostDeleteWebhookRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "webhook_id": {
                "display_name": "Webhook ID",
                "short_desc": "Webhook ID (hook_xxx)",
                "desc": "Webhook to delete",
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

#! Delete-response type (empty, but required for output_type)
public class EasyPostDeleteResponseType inherits HashDataType {
    public {
        const Fields = {
            "deleted": {
                "display_name": "Deleted",
                "short_desc": "Whether the deletion succeeded",
                "desc": "Always `True` if the deletion call succeeded",
                "type": SoftBoolOrNothingType,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for deleting a webhook
public class EasyPostDeleteWebhookDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-webhook",
            "desc": "Delete a webhook",
            "type": "EasyPostDeleteWebhookDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostDeleteWebhookRequestDataType RequestType();
        static EasyPostDeleteResponseType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "delete-webhook";
    }

    *string getDesc() {
        return "Delete a webhook";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doDelete(sprintf("webhooks/%s", req.webhook_id));
        return {"deleted": True};
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
