# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for deleting a tracker
public class EasyPostDeleteTrackerRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "tracker_id": {
                "display_name": "Tracker ID",
                "short_desc": "Tracker ID (trk_xxx)",
                "desc": "Tracker to delete",
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

#! Response type for delete-tracker (empty body)
public class EasyPostDeleteTrackerResponseDataType inherits HashDataType {
    public {
        const Fields = {};
    }
    constructor() {
    }
}

#! Data provider for deleting a tracker by ID
public class EasyPostDeleteTrackerDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-tracker",
            "desc": "Delete an EasyPost tracker by ID",
            "type": "EasyPostDeleteTrackerDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostDeleteTrackerRequestDataType RequestType();
        static EasyPostDeleteTrackerResponseDataType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "delete-tracker";
    }

    *string getDesc() {
        return "Delete an EasyPost tracker by ID";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doDelete(sprintf("trackers/%s", req.tracker_id));
        return {};
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
