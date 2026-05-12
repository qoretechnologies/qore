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
                "desc": "Carrier account type (e.g. `UpsAccount`, `FedExAccount`, `DhlExpressAccount`). "
                    "Selecting a carrier type updates the `credentials` and `test_credentials` fields "
                    "to match that carrier's schema. See "
                    "[carrier types](https://docs.easypost.com/docs/carrier-types) for the full list.",
                "type": SoftStringType,
                "required": True,
                "ref_data": "carrier_types",
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
                "desc": "Carrier-specific credentials hash for production. Required for most carrier types "
                    "(except `custom_workflow`). The shape varies per carrier - select a `type` to populate "
                    "this field with the carrier's specific schema.\n\n"
                    "**Example (UPS)**: `{\"account_number\": \"...\", \"user_id\": \"...\", \"password\": \"...\"}`",
                "type": AutoHashOrNothingType,
                "sensitive": True,
            },
            "test_credentials": {
                "display_name": "Test Credentials",
                "short_desc": "Test credentials hash",
                "desc": "Carrier-specific credentials hash for the test environment. Same shape as `credentials` - "
                    "select a `type` to populate this field with the carrier's specific schema.",
                "type": AutoHashOrNothingType,
                "sensitive": True,
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

    #! Returns the request type with carrier-specific credential fields based on the selected `type` option
    private *DataProvider::AbstractDataProviderType getRequestTypeWithOptionsImpl(*hash<auto> options) {
        *string carrier_type = options."type".value ?? options."type";
        if (!carrier_type) {
            return RequestType;
        }
        return buildDynamicRequestType(carrier_type);
    }

    #! Returns the request type with carrier-specific credential fields based on the request data
    private *DataProvider::AbstractDataProviderType getRequestTypeWithDataImpl(auto req) {
        *string carrier_type;
        if (req.typeCode() == NT_HASH) {
            carrier_type = req."type".value ?? req."type";
        }
        if (!carrier_type) {
            return RequestType;
        }
        return buildDynamicRequestType(carrier_type);
    }

    #! Builds a request type whose `credentials`/`test_credentials` fields are typed for the selected carrier
    private AbstractDataProviderType buildDynamicRequestType(string carrier_type) {
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
        return new HashDataType(sprintf("EasyPostCreate%sRequest", carrier_type), dyn_fields);
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace EasyPostDataProvider
