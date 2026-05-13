# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for retrieving a pickup
public class EasyPostGetPickupRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "pickup_id": {
                "display_name": "Pickup ID",
                "short_desc": "Pickup ID (pickup_xxx)",
                "desc": "Pickup to retrieve",
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

#! Data provider for retrieving a pickup by ID
public class EasyPostGetPickupDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-pickup",
            "desc": "Retrieve an EasyPost pickup by ID",
            "type": "EasyPostGetPickupDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostGetPickupRequestDataType RequestType();
        static EasyPostPickupType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "get-pickup";
    }

    *string getDesc() {
        return "Retrieve an EasyPost pickup by ID";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doGet(sprintf("pickups/%s", req.pickup_id));
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
