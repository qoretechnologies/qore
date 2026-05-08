# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for retrieving an event
public class EasyPostGetEventRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "event_id": {
                "display_name": "Event ID",
                "short_desc": "Event ID (evt_xxx)",
                "desc": "Event to retrieve",
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

#! Data provider for retrieving an event
public class EasyPostGetEventDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-event",
            "desc": "Retrieve a webhook event by ID",
            "type": "EasyPostGetEventDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostGetEventRequestDataType RequestType();
        static EasyPostEventType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "get-event";
    }

    *string getDesc() {
        return "Retrieve a webhook event by ID";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doGet(sprintf("events/%s", req.event_id));
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
