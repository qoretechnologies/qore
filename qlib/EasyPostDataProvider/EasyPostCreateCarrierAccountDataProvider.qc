# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for creating a carrier account
public class EasyPostCreateCarrierAccountRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "display_name": "Type",
                "short_desc": "Carrier account type",
                "desc": "Carrier account type (e.g. `UpsAccount`, `FedExAccount`, `DhlExpressAccount`)",
                "type": SoftStringType,
                "required": True,
                "preselected": True,
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Account description",
                "desc": "Human-readable description for this account",
                "type": SoftStringOrNothingType,
                "preselected": True,
            },
            "reference": {
                "display_name": "Reference",
                "short_desc": "Caller-supplied reference",
                "desc": "Caller-supplied reference",
                "type": SoftStringOrNothingType,
            },
            "credentials": {
                "display_name": "Credentials",
                "short_desc": "Production credentials hash",
                "desc": "Carrier-specific credentials hash for production",
                "type": AutoHashOrNothingType,
            },
            "test_credentials": {
                "display_name": "Test Credentials",
                "short_desc": "Test credentials hash",
                "desc": "Carrier-specific credentials hash for the test environment",
                "type": AutoHashOrNothingType,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating a carrier account
public class EasyPostCreateCarrierAccountDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-carrier-account",
            "desc": "Create a new carrier account",
            "type": "EasyPostCreateCarrierAccountDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostCreateCarrierAccountRequestDataType RequestType();
        static EasyPostCarrierAccountType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "create-carrier-account";
    }

    *string getDesc() {
        return "Create a new carrier account";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPostWrapped("carrier_accounts", WrapperKeyCarrierAccount, req);
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
