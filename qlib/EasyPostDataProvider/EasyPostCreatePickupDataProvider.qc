# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! ID-only reference type
public class EasyPostIdRefType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Object ID",
                "desc": "ID of the referenced object",
                "type": SoftStringType,
                "required": True,
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

#! Request type for creating a pickup
public class EasyPostCreatePickupRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "address": {
                "display_name": "Pickup Address",
                "short_desc": "Pickup address (or {id: adr_xxx})",
                "desc": "Pickup address - full address or address reference",
                "type": new EasyPostAddressInputType(),
                "required": True,
            },
            "shipment": {
                "display_name": "Shipment",
                "short_desc": "Shipment to pick up ({id: shp_xxx})",
                "desc": "Shipment to pick up - reference by `id`. Provide either `shipment` or `batch`, "
                    "but not both.",
                "type": new EasyPostIdRefType(AutoHashOrNothingType),
                "preselected": True,
                "ref_data": "shipments",
            },
            "batch": {
                "display_name": "Batch",
                "short_desc": "Batch to pick up ({id: batch_xxx})",
                "desc": "Fully-purchased batch to pick up - reference by `id`. Provide either `shipment` "
                    "or `batch`, but not both.",
                "type": new EasyPostIdRefType(AutoHashOrNothingType),
                "ref_data": "batches",
            },
            "min_datetime": {
                "display_name": "Min Datetime",
                "short_desc": "Earliest pickup datetime",
                "desc": "Earliest acceptable pickup datetime (ISO 8601)",
                "type": SoftDateType,
                "required": True,
            },
            "max_datetime": {
                "display_name": "Max Datetime",
                "short_desc": "Latest pickup datetime",
                "desc": "Latest acceptable pickup datetime (ISO 8601)",
                "type": SoftDateType,
                "required": True,
            },
            "is_account_address": {
                "display_name": "Is Account Address",
                "short_desc": "Pickup at the account address",
                "desc": "Set to `True` if the pickup address matches the EasyPost account address",
                "type": SoftBoolOrNothingType,
            },
            "instructions": {
                "display_name": "Instructions",
                "short_desc": "Pickup instructions",
                "desc": "Special pickup instructions for the carrier",
                "type": SoftStringOrNothingType,
            },
            "reference": {
                "display_name": "Reference",
                "short_desc": "Caller-supplied reference",
                "desc": "Caller-supplied reference",
                "type": SoftStringOrNothingType,
            },
            "carrier_accounts": {
                "display_name": "Carrier Accounts",
                "short_desc": "Carrier account IDs",
                "desc": "Carrier account IDs to use for rating the pickup",
                "type": new ListDataType("EasyPostPickupReqCarrierAccountList", SoftStringType, True),
                "ref_data": "carrier_accounts",
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating a pickup
public class EasyPostCreatePickupDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-pickup",
            "desc": "Schedule a new pickup",
            "type": "EasyPostCreatePickupDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostCreatePickupRequestDataType RequestType();
        static EasyPostPickupType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "create-pickup";
    }

    *string getDesc() {
        return "Schedule a new pickup";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPostWrapped("pickups", WrapperKeyPickup, req);
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
