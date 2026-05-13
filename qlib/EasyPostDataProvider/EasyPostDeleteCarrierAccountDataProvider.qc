# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for deleting a carrier account
public class EasyPostDeleteCarrierAccountRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "carrier_account_id": {
                "display_name": "Carrier Account ID",
                "short_desc": "Carrier account ID (ca_xxx)",
                "desc": "Carrier account to delete",
                "type": SoftStringType,
                "required": True,
                "ref_data": "carrier_accounts",
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for deleting a carrier account
public class EasyPostDeleteCarrierAccountDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-carrier-account",
            "desc": "Delete a carrier account",
            "type": "EasyPostDeleteCarrierAccountDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostDeleteCarrierAccountRequestDataType RequestType();
        static EasyPostDeleteResponseType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "delete-carrier-account";
    }

    *string getDesc() {
        return "Delete a carrier account";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doDelete(sprintf("carrier_accounts/%s", req.carrier_account_id));
        return {"deleted": True};
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
