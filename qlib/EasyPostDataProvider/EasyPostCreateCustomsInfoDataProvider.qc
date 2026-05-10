# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Request type for creating customs info
public class EasyPostCreateCustomsInfoRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "eel_pfc": {
                "display_name": "EEL/PFC",
                "short_desc": "EEL or PFC value",
                "desc": "Either an EEL code or a Proof of Filing Citation. Use `NOEEI 30.37(a)` for shipments under USD $2,500.",
                "type": SoftStringType,
                "required": True,
            },
            "contents_type": {
                "display_name": "Contents Type",
                "short_desc": "Type of contents",
                "desc": "Type of contents being shipped",
                "type": SoftStringType,
                "required": True,
                "allowed_values": ContentsTypeAllowedValues,
            },
            "customs_certify": {
                "display_name": "Customs Certify",
                "short_desc": "Certify customs",
                "desc": "Set to `True` to certify the customs declarations",
                "type": SoftBoolType,
                "required": True,
            },
            "customs_signer": {
                "display_name": "Customs Signer",
                "short_desc": "Signer name",
                "desc": "Name of the person certifying the customs declarations",
                "type": SoftStringType,
                "required": True,
            },
            "non_delivery_option": {
                "display_name": "Non-Delivery Option",
                "short_desc": "What to do if delivery fails",
                "desc": "Action to take if the shipment cannot be delivered. Defaults to `return` if not specified.",
                "type": SoftStringOrNothingType,
                "default_value": "return",
                "allowed_values": NonDeliveryOptionAllowedValues,
            },
            "restriction_type": {
                "display_name": "Restriction Type",
                "short_desc": "Restriction category",
                "desc": "Type of customs restriction",
                "type": SoftStringType,
                "required": True,
                "allowed_values": RestrictionTypeAllowedValues,
            },
            "customs_items": {
                "display_name": "Customs Items",
                "short_desc": "List of items in the shipment",
                "desc": "List of customs items in the shipment",
                "type": new ListDataType("EasyPostCreateCustomsInfoItemList", new EasyPostCustomsItemInputType(), False),
                "required": True,
            },
            "contents_explanation": {
                "display_name": "Contents Explanation",
                "short_desc": "Required if contents_type is other",
                "desc": "Free-text explanation of contents (required when `contents_type` is `other`)",
                "type": SoftStringOrNothingType,
            },
            "restriction_comments": {
                "display_name": "Restriction Comments",
                "short_desc": "Comments on the restriction",
                "desc": "Comments on the restriction (required when `restriction_type` is not `none`)",
                "type": SoftStringOrNothingType,
            },
            "declaration": {
                "display_name": "Declaration",
                "short_desc": "Free-form declaration text",
                "desc": "Free-form customs declaration text",
                "type": SoftStringOrNothingType,
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating customs info
public class EasyPostCreateCustomsInfoDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-customs-info",
            "desc": "Create a customs info object",
            "type": "EasyPostCreateCustomsInfoDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static EasyPostCreateCustomsInfoRequestDataType RequestType();
        static EasyPostCustomsInfoType ResponseType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "create-customs-info";
    }

    *string getDesc() {
        return "Create a customs info object";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPostWrapped("customs_infos", WrapperKeyCustomsInfo, req);
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
