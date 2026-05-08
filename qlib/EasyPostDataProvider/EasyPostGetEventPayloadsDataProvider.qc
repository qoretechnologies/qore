# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for listing event payloads
public class EasyPostGetEventPayloadsRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "event_id": {
                "display_name": "Event ID",
                "short_desc": "Event ID (evt_xxx)",
                "desc": "Event to list payloads for",
                "type": SoftStringType,
                "required": True,
                "preselected": True,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for listing event payloads (delivery attempts)
public class EasyPostGetEventPayloadsDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-event-payloads",
            "desc": "List webhook delivery payloads for an event",
            "type": "EasyPostGetEventPayloadsDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostGetEventPayloadsRequestDataType RequestType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "get-event-payloads";
    }

    *string getDesc() {
        return "List webhook delivery payloads for an event";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doGet(sprintf("events/%s/payloads", req.event_id));
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderType::get(AutoHashType);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace EasyPostDataProvider
