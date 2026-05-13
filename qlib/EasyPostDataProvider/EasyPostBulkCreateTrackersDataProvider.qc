# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Bulk tracker entry
public class EasyPostBulkTrackerEntryType inherits HashDataType {
    public {
        const Fields = {
            "tracking_code": {
                "display_name": "Tracking Code",
                "short_desc": "Carrier tracking code",
                "desc": "Tracking number to monitor",
                "type": SoftStringType,
                "required": True,
            },
            "carrier": {
                "display_name": "Carrier",
                "short_desc": "Carrier name",
                "desc": "Carrier name (e.g. `USPS`, `UPS`)",
                "type": SoftStringOrNothingType,
                "ref_data": "carrier_names",
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

#! Request type for bulk-creating trackers
public class EasyPostBulkCreateTrackersRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "trackers": {
                "display_name": "Trackers",
                "short_desc": "List of tracker entries",
                "desc": "List of tracker entries to create in a single batch",
                "type": new ListDataType("EasyPostBulkTrackerList", new EasyPostBulkTrackerEntryType(), False),
                "required": True,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for bulk-creating trackers
public class EasyPostBulkCreateTrackersDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "bulk-create-trackers",
            "desc": "Bulk-create trackers (up to 100 per call)",
            "type": "EasyPostBulkCreateTrackersDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostBulkCreateTrackersRequestDataType RequestType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "bulk-create-trackers";
    }

    *string getDesc() {
        return "Bulk-create trackers (up to 100 per call)";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("trackers/create_list", {"trackers": req.trackers});
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderType::get(AutoHashOrNothingType);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace EasyPostDataProvider
