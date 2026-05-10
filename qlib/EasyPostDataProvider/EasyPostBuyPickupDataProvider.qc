# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for buying a pickup
public class EasyPostBuyPickupRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "pickup_id": {
                "display_name": "Pickup ID",
                "short_desc": "Pickup ID (pickup_xxx)",
                "desc": "Pickup to confirm/buy",
                "type": SoftStringType,
                "required": True,
                "ref_data": "pickups",
            },
            "carrier": {
                "display_name": "Carrier",
                "short_desc": "Carrier name",
                "desc": "Carrier name (e.g. `UPS`, `FedEx`) — must match a `carrier` returned in the "
                    "Pickup's `pickup_rates`",
                "type": SoftStringType,
                "required": True,
                "ref_data": "pickup_carriers",
                "depends_on": ("pickup_id",),
            },
            "service": {
                "display_name": "Service",
                "short_desc": "Pickup service code",
                "desc": "Pickup service code returned in `pickup_rates`",
                "type": SoftStringType,
                "required": True,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for buying a pickup
public class EasyPostBuyPickupDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "buy-pickup",
            "desc": "Confirm/buy a previously scheduled pickup",
            "type": "EasyPostBuyPickupDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostBuyPickupRequestDataType RequestType();
        static EasyPostPickupType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "buy-pickup";
    }

    *string getDesc() {
        return "Confirm/buy a previously scheduled pickup";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost(sprintf("pickups/%s/buy", req.pickup_id), {
            "carrier": req.carrier,
            "service": req.service,
        });
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
