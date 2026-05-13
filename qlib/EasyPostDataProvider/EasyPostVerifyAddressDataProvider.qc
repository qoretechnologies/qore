# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for verifying an address by ID
public class EasyPostVerifyAddressRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "address_id": {
                "display_name": "Address ID",
                "short_desc": "Address ID to verify (adr_xxx)",
                "desc": "Existing EasyPost address ID to verify",
                "type": SoftStringType,
                "required": True,
                "ref_data": "addresses",
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for verifying an existing address
public class EasyPostVerifyAddressDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "verify-address",
            "desc": "Verify an existing EasyPost address",
            "type": "EasyPostVerifyAddressDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostVerifyAddressRequestDataType RequestType();
        static EasyPostAddressType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "verify-address";
    }

    *string getDesc() {
        return "Verify an existing EasyPost address";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.address_id;
        return doGet(sprintf("addresses/%s/verify", id));
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
