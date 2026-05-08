# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for retrieving an order
public class EasyPostGetOrderRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "order_id": {
                "display_name": "Order ID",
                "short_desc": "Order ID (order_xxx)",
                "desc": "Order to retrieve",
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

#! Data provider for retrieving an order
public class EasyPostGetOrderDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-order",
            "desc": "Retrieve an EasyPost order by ID",
            "type": "EasyPostGetOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostGetOrderRequestDataType RequestType();
        static EasyPostOrderType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "get-order";
    }

    *string getDesc() {
        return "Retrieve an EasyPost order by ID";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doGet(sprintf("orders/%s", req.order_id));
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
