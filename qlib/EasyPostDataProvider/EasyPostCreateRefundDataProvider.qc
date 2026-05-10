# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for creating a refund
public class EasyPostCreateRefundRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "carrier": {
                "display_name": "Carrier",
                "short_desc": "Carrier name",
                "desc": "Carrier name (e.g. `USPS`, `UPS`, `FedEx`) of the labels being refunded",
                "type": SoftStringType,
                "required": True,
                "ref_data": "carrier_names",
            },
            "tracking_codes": {
                "display_name": "Tracking Codes",
                "short_desc": "Tracking codes to refund",
                "desc": "List of tracking codes to refund",
                "type": new ListDataType("EasyPostCreateRefundCodeList", SoftStringType, False),
                "required": True,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating refunds
public class EasyPostCreateRefundDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-refund",
            "desc": "Create one or more refund requests by tracking code",
            "type": "EasyPostCreateRefundDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostCreateRefundRequestDataType RequestType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "create-refund";
    }

    *string getDesc() {
        return "Create one or more refund requests by tracking code";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPostWrapped("refunds", WrapperKeyRefund, req);
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return new ListDataType("EasyPostCreateRefundResponseList", new EasyPostRefundType(), True);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace EasyPostDataProvider
