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
                "desc": "New production credentials. Carrier-specific schema; see "
                    "[carrier types](https://docs.easypost.com/docs/carrier-types).\n\n"
                    "**Example (UPS)**: `{\"account_number\": \"...\", \"user_id\": \"...\"}`",
                "type": AutoHashOrNothingType,
            },
            "test_credentials": {
                "display_name": "Test Credentials",
                "short_desc": "Test credentials hash",
                "desc": "New test credentials. Same schema as `credentials`.",
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

    #! Returns the request type with carrier-specific credential fields based on the selected carrier account
    private *DataProvider::AbstractDataProviderType getRequestTypeWithOptionsImpl(*hash<auto> options) {
        *string carrier_account_id = options.carrier_account_id.value ?? options.carrier_account_id;
        return buildDynamicRequestTypeForAccount(carrier_account_id);
    }

    #! Returns the request type with carrier-specific credential fields based on the request data
    private *DataProvider::AbstractDataProviderType getRequestTypeWithDataImpl(auto req) {
        if (req.typeCode() != NT_HASH) {
            return RequestType;
        }
        *string carrier_account_id = req.carrier_account_id.value ?? req.carrier_account_id;
        return buildDynamicRequestTypeForAccount(carrier_account_id);
    }

    #! Builds a request type with credentials/test_credentials typed for the carrier account's type
    private *DataProvider::AbstractDataProviderType buildDynamicRequestTypeForAccount(*string carrier_account_id) {
        if (!carrier_account_id) {
            return RequestType;
        }
        *string carrier_type = fetchCarrierAccountType(carrier_account_id);
        if (!carrier_type) {
            return RequestType;
        }
        *list<hash<auto>> carrier_types = fetchCarrierTypes();
        *hash<auto> ct;
        foreach hash<auto> entry in (carrier_types) {
            if (entry."type" == carrier_type) {
                ct = entry;
                break;
            }
        }
        if (!ct || !ct.fields) {
            return RequestType;
        }

        hash<string, AbstractDataField> dyn_fields = RequestType.getFields();
        if (ct.fields.credentials) {
            *AbstractDataProviderType cred_type = buildCarrierCredentialsType(carrier_type, ct.fields.credentials);
            if (cred_type) {
                dyn_fields."credentials" = new QoreDataField({
                    "name": "credentials",
                    "display_name": "Credentials",
                    "short_desc": sprintf("%s production credentials", ct.readable ?? carrier_type),
                    "desc": sprintf("Production credentials for `%s`", carrier_type),
                    "type": cred_type,
                });
            }
        }
        if (ct.fields.test_credentials) {
            *AbstractDataProviderType cred_type = buildCarrierCredentialsType(carrier_type + "Test",
                    ct.fields.test_credentials);
            if (cred_type) {
                dyn_fields."test_credentials" = new QoreDataField({
                    "name": "test_credentials",
                    "display_name": "Test Credentials",
                    "short_desc": sprintf("%s test credentials", ct.readable ?? carrier_type),
                    "desc": sprintf("Test credentials for `%s`", carrier_type),
                    "type": cred_type,
                });
            }
        }
        return new HashDataType(sprintf("EasyPostUpdate%sRequest", carrier_type), dyn_fields);
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace EasyPostDataProvider
