# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for retrieving a carrier account
public class EasyPostGetCarrierAccountRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "carrier_account_id": {
                "display_name": "Carrier Account ID",
                "short_desc": "Carrier account ID (ca_xxx)",
                "desc": "Carrier account to retrieve",
                "type": SoftStringType,
                "required": True,
                "preselected": True,
                "ref_data": "carrier_accounts",
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for retrieving a carrier account
public class EasyPostGetCarrierAccountDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-carrier-account",
            "desc": "Retrieve a carrier account by ID",
            "type": "EasyPostGetCarrierAccountDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostGetCarrierAccountRequestDataType RequestType();
        static EasyPostCarrierAccountType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "get-carrier-account";
    }

    *string getDesc() {
        return "Retrieve a carrier account by ID";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doGet(sprintf("carrier_accounts/%s", req.carrier_account_id));
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
