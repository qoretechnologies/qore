# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for retrieving an address
public class EasyPostGetAddressRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "address_id": {
                "display_name": "Address ID",
                "short_desc": "Address ID (adr_xxx)",
                "desc": "Address to retrieve",
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

#! Data provider for retrieving an address by ID
public class EasyPostGetAddressDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-address",
            "desc": "Retrieve an EasyPost address by ID",
            "type": "EasyPostGetAddressDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostGetAddressRequestDataType RequestType();
        static EasyPostAddressType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "get-address";
    }

    *string getDesc() {
        return "Retrieve an EasyPost address by ID";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doGet(sprintf("addresses/%s", req.address_id));
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
