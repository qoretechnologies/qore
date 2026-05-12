# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for creating a tracker
public class EasyPostCreateTrackerRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "tracking_code": {
                "display_name": "Tracking Code",
                "short_desc": "Carrier tracking code",
                "desc": "Carrier tracking number to monitor",
                "type": SoftStringType,
                "required": True,
                "example_value": "EZ1000000001",
            },
            "carrier": {
                "display_name": "Carrier",
                "short_desc": "Carrier name",
                "desc": "Carrier name (e.g. `USPS`, `FedEx`); EasyPost auto-detects when omitted",
                "type": SoftStringOrNothingType,
                "ref_data": "carrier_names",
                "preselected": True,
                "example_value": "USPS",
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating a tracker
public class EasyPostCreateTrackerDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-tracker",
            "desc": "Create a new tracker for a tracking code",
            "type": "EasyPostCreateTrackerDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostCreateTrackerRequestDataType RequestType();
        static EasyPostTrackerType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "create-tracker";
    }

    *string getDesc() {
        return "Create a new tracker for a tracking code";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPostWrapped("trackers", WrapperKeyTracker, req);
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
