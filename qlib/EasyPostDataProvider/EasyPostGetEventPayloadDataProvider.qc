# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for retrieving a single event payload
public class EasyPostGetEventPayloadRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "event_id": {
                "display_name": "Event ID",
                "short_desc": "Event ID (evt_xxx)",
                "desc": "Parent event ID",
                "type": SoftStringType,
                "required": True,
                "preselected": True,
            },
            "payload_id": {
                "display_name": "Payload ID",
                "short_desc": "Payload ID (payload_xxx)",
                "desc": "Payload to retrieve",
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

#! Data provider for retrieving a single event payload
public class EasyPostGetEventPayloadDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-event-payload",
            "desc": "Retrieve a single webhook delivery payload",
            "type": "EasyPostGetEventPayloadDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostGetEventPayloadRequestDataType RequestType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "get-event-payload";
    }

    *string getDesc() {
        return "Retrieve a single webhook delivery payload";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doGet(sprintf("events/%s/payloads/%s", req.event_id, req.payload_id));
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
