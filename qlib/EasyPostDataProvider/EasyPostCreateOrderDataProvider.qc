# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Order shipment input - shipment fields without to/from address (those come from the order)
public class EasyPostOrderShipmentInputType inherits HashDataType {
    public {
        const Fields = {
            "parcel": {
                "display_name": "Parcel",
                "short_desc": "Parcel (or {id: prcl_xxx})",
                "desc": "Parcel for this shipment",
                "type": new EasyPostParcelInputType(),
                "required": True,
            },
            "customs_info": {
                "display_name": "Customs Info",
                "short_desc": "Customs information",
                "desc": "Customs information for this shipment",
                "type": new EasyPostCustomsInfoInputType(AutoHashOrNothingType),
            },
            "options": {
                "display_name": "Options",
                "short_desc": "Shipment options",
                "desc": "Per-shipment options",
                "type": new EasyPostShipmentOptionsType(AutoHashOrNothingType),
            },
            "service": {
                "display_name": "Service",
                "short_desc": "Carrier service",
                "desc": "Carrier service code",
                "type": SoftStringOrNothingType,
            },
            "carrier": {
                "display_name": "Carrier",
                "short_desc": "Carrier name",
                "desc": "Carrier name",
                "type": SoftStringOrNothingType,
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

#! Request type for creating an order
public class EasyPostCreateOrderRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "to_address": {
                "display_name": "To Address",
                "short_desc": "Destination address",
                "desc": "Destination address (or `{id: adr_xxx}`)",
                "type": new EasyPostAddressInputType(),
                "required": True,
                "preselected": True,
            },
            "from_address": {
                "display_name": "From Address",
                "short_desc": "Origin address",
                "desc": "Origin address (or `{id: adr_xxx}`)",
                "type": new EasyPostAddressInputType(),
                "required": True,
                "preselected": True,
            },
            "shipments": {
                "display_name": "Shipments",
                "short_desc": "Shipments in the order",
                "desc": "List of shipments to include in the order (each contains a parcel, etc.)",
                "type": new ListDataType("EasyPostOrderReqShipmentList", new EasyPostOrderShipmentInputType(), False),
                "required": True,
                "preselected": True,
            },
            "buyer_address": {
                "display_name": "Buyer Address",
                "short_desc": "Buyer address",
                "desc": "Buyer address (when different from `to_address`)",
                "type": new EasyPostAddressInputType(AutoHashOrNothingType),
            },
            "return_address": {
                "display_name": "Return Address",
                "short_desc": "Return address",
                "desc": "Return address",
                "type": new EasyPostAddressInputType(AutoHashOrNothingType),
            },
            "reference": {
                "display_name": "Reference",
                "short_desc": "Caller-supplied reference",
                "desc": "Caller-supplied reference",
                "type": SoftStringOrNothingType,
            },
            "is_return": {
                "display_name": "Is Return",
                "short_desc": "Mark as return order",
                "desc": "Set to `True` if this is a return order",
                "type": SoftBoolOrNothingType,
            },
            "customs_info": {
                "display_name": "Customs Info",
                "short_desc": "Order-level customs info",
                "desc": "Customs info applied across all shipments in the order",
                "type": new EasyPostCustomsInfoInputType(AutoHashOrNothingType),
            },
            "carrier_accounts": {
                "display_name": "Carrier Accounts",
                "short_desc": "Carrier accounts",
                "desc": "Carrier account IDs to use for rating",
                "type": new ListDataType("EasyPostOrderReqCarrierAccountList", SoftStringType, True),
                "ref_data": "carrier_accounts",
            },
            "service": {
                "display_name": "Service",
                "short_desc": "Carrier service",
                "desc": "Carrier service code",
                "type": SoftStringOrNothingType,
            },
            "carrier": {
                "display_name": "Carrier",
                "short_desc": "Carrier name",
                "desc": "Carrier name",
                "type": SoftStringOrNothingType,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating an order
public class EasyPostCreateOrderDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-order",
            "desc": "Create a multi-shipment order",
            "type": "EasyPostCreateOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostCreateOrderRequestDataType RequestType();
        static EasyPostOrderType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "create-order";
    }

    *string getDesc() {
        return "Create a multi-shipment order";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPostWrapped("orders", WrapperKeyOrder, req);
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
