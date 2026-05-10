# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Allowed values for shipment label format
const ShipmentLabelFormatAllowedValues = (
    <AllowedValueInfo>{"value": "PDF", "desc": "PDF"},
    <AllowedValueInfo>{"value": "ZPL", "desc": "Zebra ZPL"},
    <AllowedValueInfo>{"value": "EPL2", "desc": "Eltron EPL2"},
    <AllowedValueInfo>{"value": "PNG", "desc": "PNG image"},
);

#! Request type for retrieving a shipment label
public class EasyPostGetShipmentLabelRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "shipment_id": {
                "display_name": "Shipment ID",
                "short_desc": "Shipment ID (shp_xxx)",
                "desc": "Shipment to fetch the label for",
                "type": SoftStringType,
                "required": True,
                "ref_data": "shipments",
            },
            "file_format": {
                "display_name": "File Format",
                "short_desc": "Label file format",
                "desc": "Format of the label file to retrieve",
                "type": SoftStringType,
                "required": True,
                "allowed_values": ShipmentLabelFormatAllowedValues,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for retrieving a shipment label in a specific format
public class EasyPostGetShipmentLabelDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-shipment-label",
            "desc": "Retrieve a shipment label in a specific format",
            "type": "EasyPostGetShipmentLabelDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostGetShipmentLabelRequestDataType RequestType();
        static EasyPostShipmentType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "get-shipment-label";
    }

    *string getDesc() {
        return "Retrieve a shipment label in a specific format";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doGet(sprintf("shipments/%s/label", req.shipment_id), {"file_format": req.file_format});
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
