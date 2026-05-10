# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for insuring a shipment
public class EasyPostInsureShipmentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "shipment_id": {
                "display_name": "Shipment ID",
                "short_desc": "Shipment ID (shp_xxx)",
                "desc": "Shipment to insure",
                "type": SoftStringType,
                "required": True,
                "ref_data": "shipments",
            },
            "amount": {
                "display_name": "Amount",
                "short_desc": "Amount to insure (string)",
                "desc": "Amount to insure the shipment for, as a string (e.g. `\"100.00\"`)",
                "type": SoftStringType,
                "required": True,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for insuring a shipment
public class EasyPostInsureShipmentDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "insure-shipment",
            "desc": "Insure a previously purchased shipment",
            "type": "EasyPostInsureShipmentDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostInsureShipmentRequestDataType RequestType();
        static EasyPostShipmentType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "insure-shipment";
    }

    *string getDesc() {
        return "Insure a previously purchased shipment";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost(sprintf("shipments/%s/insure", req.shipment_id), {"amount": req.amount});
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
