# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for re-rating a shipment
public class EasyPostRerateShipmentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "shipment_id": {
                "display_name": "Shipment ID",
                "short_desc": "Shipment ID to re-rate (shp_xxx)",
                "desc": "Shipment to re-rate",
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

#! Data provider for re-rating a shipment
public class EasyPostRerateShipmentDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "rerate-shipment",
            "desc": "Re-rate an existing shipment to get fresh rate quotes",
            "type": "EasyPostRerateShipmentDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostRerateShipmentRequestDataType RequestType();
        static EasyPostShipmentType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "rerate-shipment";
    }

    *string getDesc() {
        return "Re-rate an existing shipment to get fresh rate quotes";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost(sprintf("shipments/%s/rerate", req.shipment_id), {});
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
