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
                "preselected": True,
            },
            "carrier": {
                "display_name": "Carrier",
                "short_desc": "Carrier reference (account ID or name)",
                "desc": "Carrier account ID or carrier name",
                "type": SoftStringOrNothingType,
                "ref_data": "carrier_accounts",
                "preselected": True,
            },
            "amount": {
                "display_name": "Amount",
                "short_desc": "Insured amount",
                "desc": "Insured amount as a string (e.g. `\"100.00\"`)",
                "type": SoftStringOrNothingType,
            },
            "carrier_account_id": {
                "display_name": "Carrier Account ID",
                "short_desc": "Carrier account ID",
                "desc": "Specific carrier account ID to use",
                "type": SoftStringOrNothingType,
                "ref_data": "carrier_accounts",
            },
            "is_return": {
                "display_name": "Is Return",
                "short_desc": "Mark as return",
                "desc": "Set to `True` if this is a return shipment",
                "type": SoftBoolOrNothingType,
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
