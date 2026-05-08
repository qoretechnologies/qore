# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for buying an order
public class EasyPostBuyOrderRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "order_id": {
                "display_name": "Order ID",
                "short_desc": "Order ID (order_xxx)",
                "desc": "Order to buy",
                "type": SoftStringType,
                "required": True,
                "preselected": True,
            },
            "carrier": {
                "display_name": "Carrier",
                "short_desc": "Carrier reference",
                "desc": "Carrier name or carrier account ID",
                "type": SoftStringType,
                "required": True,
                "preselected": True,
                "ref_data": "carrier_accounts",
            },
            "service": {
                "display_name": "Service",
                "short_desc": "Carrier service",
                "desc": "Carrier service code",
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

#! Data provider for buying an order
public class EasyPostBuyOrderDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "buy-order",
            "desc": "Buy all shipments in an order with a single carrier/service",
            "type": "EasyPostBuyOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostBuyOrderRequestDataType RequestType();
        static EasyPostOrderType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "buy-order";
    }

    *string getDesc() {
        return "Buy all shipments in an order with a single carrier/service";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost(sprintf("orders/%s/buy", req.order_id), {
            "carrier": req.carrier,
            "service": req.service,
        });
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
