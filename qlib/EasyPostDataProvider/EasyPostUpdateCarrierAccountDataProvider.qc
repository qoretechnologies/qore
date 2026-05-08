# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for updating a carrier account
public class EasyPostUpdateCarrierAccountRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "carrier_account_id": {
                "display_name": "Carrier Account ID",
                "short_desc": "Carrier account ID (ca_xxx)",
                "desc": "Carrier account to update",
                "type": SoftStringType,
                "required": True,
                "preselected": True,
                "ref_data": "carrier_accounts",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Account description",
                "desc": "New account description",
                "type": SoftStringOrNothingType,
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
                "desc": "New production credentials",
                "type": AutoHashOrNothingType,
            },
            "test_credentials": {
                "display_name": "Test Credentials",
                "short_desc": "Test credentials hash",
                "desc": "New test credentials",
                "type": AutoHashOrNothingType,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for updating a carrier account
public class EasyPostUpdateCarrierAccountDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-carrier-account",
            "desc": "Update an existing carrier account",
            "type": "EasyPostUpdateCarrierAccountDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostUpdateCarrierAccountRequestDataType RequestType();
        static EasyPostCarrierAccountType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "update-carrier-account";
    }

    *string getDesc() {
        return "Update an existing carrier account";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = req ?? {};
        string id = body.carrier_account_id;
        return doPatchWrapped(sprintf("carrier_accounts/%s", id), WrapperKeyCarrierAccount,
            body - "carrier_account_id");
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
