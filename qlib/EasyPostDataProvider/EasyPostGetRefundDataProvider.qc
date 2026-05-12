# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for retrieving a refund
public class EasyPostGetRefundRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "refund_id": {
                "display_name": "Refund ID",
                "short_desc": "Refund ID (rfnd_xxx)",
                "desc": "Refund to retrieve",
                "type": SoftStringType,
                "required": True,
                "ref_data": "refunds",
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for retrieving a refund by ID
public class EasyPostGetRefundDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-refund",
            "desc": "Retrieve an EasyPost refund by ID",
            "type": "EasyPostGetRefundDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostGetRefundRequestDataType RequestType();
        static EasyPostRefundType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "get-refund";
    }

    *string getDesc() {
        return "Retrieve an EasyPost refund by ID";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doGet(sprintf("refunds/%s", req.refund_id));
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
