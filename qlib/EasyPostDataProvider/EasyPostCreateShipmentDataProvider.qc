# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for creating a shipment
public class EasyPostCreateShipmentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "to_address": {
                "display_name": "To Address",
                "short_desc": "Destination address (or {id: adr_xxx})",
                "desc": "Destination address - either a full address object or a reference by `id`",
                "type": new EasyPostAddressInputType(),
                "required": True,
            },
            "from_address": {
                "display_name": "From Address",
                "short_desc": "Origin address (or {id: adr_xxx})",
                "desc": "Origin address - either a full address object or a reference by `id`",
                "type": new EasyPostAddressInputType(),
                "required": True,
            },
            "parcel": {
                "display_name": "Parcel",
                "short_desc": "Parcel details (or {id: prcl_xxx})",
                "desc": "Parcel details - either a full parcel object or a reference by `id`",
                "type": new EasyPostParcelInputType(),
                "required": True,
            },
            "insurance": {
                "display_name": "Insurance",
                "short_desc": "Insurance amount for one-call buy",
                "desc": "Insurance amount as a string (e.g. `\"100.00\"`). Used by the one-call buy flow.",
                "type": SoftStringOrNothingType,
            },
            "return_address": {
                "display_name": "Return Address",
                "short_desc": "Return address",
                "desc": "Return address (used for return shipments)",
                "type": new EasyPostAddressInputType(AutoHashOrNothingType),
            },
            "buyer_address": {
                "display_name": "Buyer Address",
                "short_desc": "Buyer address",
                "desc": "Buyer address (when different from `to_address`)",
                "type": new EasyPostAddressInputType(AutoHashOrNothingType),
            },
            "customs_info": {
                "display_name": "Customs Info",
                "short_desc": "Customs information for international shipments",
                "desc": "Customs information for international shipments",
                "type": new EasyPostCustomsInfoInputType(AutoHashOrNothingType),
            },
            "options": {
                "display_name": "Options",
                "short_desc": "Shipment options",
                "desc": "Shipment options (e.g. `delivery_confirmation`, `label_format`)",
                "type": new EasyPostShipmentOptionsType(AutoHashOrNothingType),
            },
            "is_return": {
                "display_name": "Is Return",
                "short_desc": "Mark as return shipment",
                "desc": "Set to `True` to mark this as a return shipment",
                "type": SoftBoolOrNothingType,
            },
            "reference": {
                "display_name": "Reference",
                "short_desc": "Caller-supplied reference",
                "desc": "Caller-supplied reference",
                "type": SoftStringOrNothingType,
            },
            "tax_identifiers": {
                "display_name": "Tax Identifiers",
                "short_desc": "Tax identifiers for international shipments",
                "desc": "List of tax identifiers (e.g. VAT, EIN)",
                "type": new ListDataType("EasyPostShipmentReqTaxIdList", new EasyPostTaxIdentifierType(), True),
            },
            "carrier_accounts": {
                "display_name": "Carrier Accounts",
                "short_desc": "Carrier accounts to use for rating",
                "desc": "List of carrier account IDs to use for rating this shipment",
                "type": new ListDataType("EasyPostShipmentReqCarrierAccountList", SoftStringType, True),
                "ref_data": "carrier_accounts",
            },
            "service": {
                "display_name": "Service",
                "short_desc": "Carrier service",
                "desc": "Carrier service code (e.g. `Priority`, `Ground`)",
                "type": SoftStringOrNothingType,
            },
            "carrier": {
                "display_name": "Carrier",
                "short_desc": "Carrier name",
                "desc": "Carrier name (e.g. `USPS`, `UPS`, `FedEx`)",
                "type": SoftStringOrNothingType,
                "ref_data": "carrier_names",
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating a shipment
public class EasyPostCreateShipmentDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-shipment",
            "desc": "Create a new EasyPost shipment (returns rate options)",
            "type": "EasyPostCreateShipmentDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostCreateShipmentRequestDataType RequestType();
        static EasyPostShipmentType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "create-shipment";
    }

    *string getDesc() {
        return "Create a new EasyPost shipment (returns rate options)";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPostWrapped("shipments", WrapperKeyShipment, req);
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
