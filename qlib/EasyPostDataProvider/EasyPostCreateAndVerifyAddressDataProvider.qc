# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Data provider for creating and verifying an address in one call
public class EasyPostCreateAndVerifyAddressDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-and-verify-address",
            "desc": "Create and verify an EasyPost address in a single request",
            "type": "EasyPostCreateAndVerifyAddressDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostCreateAddressRequestDataType RequestType();
        static EasyPostAddressType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "create-and-verify-address";
    }

    *string getDesc() {
        return "Create and verify an EasyPost address in a single request";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = req ?? {};
        hash<auto> inner = body - ("verify", "verify_strict");
        hash<auto> extra = {};
        if (body.verify) {
            extra.verify = body.verify;
        }
        if (body.verify_strict) {
            extra.verify_strict = body.verify_strict;
        }
        # POST /addresses/create_and_verify with the same body shape
        auto response = doPostWrapped("addresses/create_and_verify", WrapperKeyAddress, inner, extra ?? NOTHING);
        # The endpoint returns {"address": {...}}; unwrap it
        if (response.typeCode() == NT_HASH && response.address) {
            return response.address;
        }
        return response;
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
