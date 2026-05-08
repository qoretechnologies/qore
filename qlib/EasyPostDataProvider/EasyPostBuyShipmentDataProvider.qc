# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Rate reference (sub-type for shipment buy)
public class EasyPostRateRefType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Rate ID",
                "short_desc": "Selected rate ID (rate_xxx)",
                "desc": "ID of one of the rates returned by `create-shipment`",
                "type": SoftStringType,
                "required": True,
                "preselected": True,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

#! Request type for buying a shipment label
public class EasyPostBuyShipmentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "shipment_id": {
                "display_name": "Shipment ID",
                "short_desc": "Shipment ID (shp_xxx)",
                "desc": "Shipment to purchase a label for",
                "type": SoftStringType,
                "required": True,
                "preselected": True,
            },
            "rate": {
                "display_name": "Rate",
                "short_desc": "Selected rate ({id: rate_xxx})",
                "desc": "Rate to purchase - select one of the rates returned by `create-shipment`",
                "type": new EasyPostRateRefType(),
                "required": True,
                "preselected": True,
            },
            "insurance": {
                "display_name": "Insurance",
                "short_desc": "Insurance amount (string)",
                "desc": "Insure the shipment for this amount (string, e.g. `\"100.00\"`)",
                "type": SoftStringOrNothingType,
            },
            "carbon_offset": {
                "display_name": "Carbon Offset",
                "short_desc": "Apply carbon offset",
                "desc": "Set to `True` to apply EasyPost's carbon offset surcharge",
                "type": SoftBoolOrNothingType,
            },
            "end_shipper_id": {
                "display_name": "End Shipper ID",
                "short_desc": "End shipper ID (es_xxx)",
                "desc": "End shipper ID for the purchase",
                "type": SoftStringOrNothingType,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for buying a shipment (purchasing a postage label)
public class EasyPostBuyShipmentDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "buy-shipment",
            "desc": "Buy a shipment (purchase the postage label)",
            "type": "EasyPostBuyShipmentDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostBuyShipmentRequestDataType RequestType();
        static EasyPostShipmentType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "buy-shipment";
    }

    *string getDesc() {
        return "Buy a shipment (purchase the postage label)";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = req ?? {};
        string id = body.shipment_id;
        hash<auto> post_body = body - "shipment_id";
        return doPost(sprintf("shipments/%s/buy", id), post_body);
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
