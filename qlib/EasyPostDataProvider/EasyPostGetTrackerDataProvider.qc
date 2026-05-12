# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for retrieving a tracker
public class EasyPostGetTrackerRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "tracker_id": {
                "display_name": "Tracker ID",
                "short_desc": "Tracker ID (trk_xxx)",
                "desc": "Tracker to retrieve",
                "type": SoftStringType,
                "required": True,
                "ref_data": "trackers",
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for retrieving a tracker by ID
public class EasyPostGetTrackerDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-tracker",
            "desc": "Retrieve an EasyPost tracker by ID",
            "type": "EasyPostGetTrackerDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostGetTrackerRequestDataType RequestType();
        static EasyPostTrackerType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "get-tracker";
    }

    *string getDesc() {
        return "Retrieve an EasyPost tracker by ID";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doGet(sprintf("trackers/%s", req.tracker_id));
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
