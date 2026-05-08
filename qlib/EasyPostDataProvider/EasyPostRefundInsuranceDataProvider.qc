# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for refunding an insurance record
public class EasyPostRefundInsuranceRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "insurance_id": {
                "display_name": "Insurance ID",
                "short_desc": "Insurance ID (ins_xxx)",
                "desc": "Insurance ID to refund",
                "type": SoftStringType,
                "required": True,
                "preselected": True,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for refunding an insurance record
public class EasyPostRefundInsuranceDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "refund-insurance",
            "desc": "Refund an insurance record",
            "type": "EasyPostRefundInsuranceDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostRefundInsuranceRequestDataType RequestType();
        static EasyPostInsuranceType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "refund-insurance";
    }

    *string getDesc() {
        return "Refund an insurance record";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost(sprintf("insurances/%s/refund", req.insurance_id), {});
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
