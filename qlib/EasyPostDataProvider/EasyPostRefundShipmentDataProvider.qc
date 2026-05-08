# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for refunding a shipment
public class EasyPostRefundShipmentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "shipment_id": {
                "display_name": "Shipment ID",
                "short_desc": "Shipment ID to refund (shp_xxx)",
                "desc": "ID of a previously purchased shipment to refund",
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

#! Data provider for refunding a shipment
public class EasyPostRefundShipmentDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "refund-shipment",
            "desc": "Refund a previously purchased shipment",
            "type": "EasyPostRefundShipmentDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostRefundShipmentRequestDataType RequestType();
        static EasyPostShipmentType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "refund-shipment";
    }

    *string getDesc() {
        return "Refund a previously purchased shipment";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost(sprintf("shipments/%s/refund", req.shipment_id), {});
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
