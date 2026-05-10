# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for retrieving an insurance record
public class EasyPostGetInsuranceRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "insurance_id": {
                "display_name": "Insurance ID",
                "short_desc": "Insurance ID (ins_xxx)",
                "desc": "Insurance record to retrieve",
                "type": SoftStringType,
                "required": True,
                "ref_data": "insurances",
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for retrieving an insurance record by ID
public class EasyPostGetInsuranceDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-insurance",
            "desc": "Retrieve an EasyPost insurance record by ID",
            "type": "EasyPostGetInsuranceDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostGetInsuranceRequestDataType RequestType();
        static EasyPostInsuranceType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "get-insurance";
    }

    *string getDesc() {
        return "Retrieve an EasyPost insurance record by ID";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doGet(sprintf("insurances/%s", req.insurance_id));
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
