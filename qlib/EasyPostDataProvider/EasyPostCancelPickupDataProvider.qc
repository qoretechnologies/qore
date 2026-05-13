# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for cancelling a pickup
public class EasyPostCancelPickupRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "pickup_id": {
                "display_name": "Pickup ID",
                "short_desc": "Pickup ID (pickup_xxx)",
                "desc": "Pickup to cancel",
                "type": SoftStringType,
                "required": True,
                "ref_data": "pickups",
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for cancelling a pickup
public class EasyPostCancelPickupDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "cancel-pickup",
            "desc": "Cancel a scheduled pickup",
            "type": "EasyPostCancelPickupDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostCancelPickupRequestDataType RequestType();
        static EasyPostPickupType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "cancel-pickup";
    }

    *string getDesc() {
        return "Cancel a scheduled pickup";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost(sprintf("pickups/%s/cancel", req.pickup_id), {});
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
