# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Allowed values for `contents_type`
const ContentsTypeAllowedValues = (
    <AllowedValueInfo>{"value": "documents", "desc": "Documents"},
    <AllowedValueInfo>{"value": "gift", "desc": "Gift"},
    <AllowedValueInfo>{"value": "merchandise", "desc": "Merchandise"},
    <AllowedValueInfo>{"value": "returned_goods", "desc": "Returned goods"},
    <AllowedValueInfo>{"value": "sample", "desc": "Sample"},
    <AllowedValueInfo>{"value": "dangerous_goods", "desc": "Dangerous goods"},
    <AllowedValueInfo>{"value": "humanitarian_donation", "desc": "Humanitarian donation"},
    <AllowedValueInfo>{"value": "other", "desc": "Other (requires `contents_explanation`)"},
);

#! Allowed values for `non_delivery_option`
const NonDeliveryOptionAllowedValues = (
    <AllowedValueInfo>{"value": "return", "desc": "Return to sender"},
    <AllowedValueInfo>{"value": "abandon", "desc": "Abandon"},
);

#! Allowed values for `restriction_type`
const RestrictionTypeAllowedValues = (
    <AllowedValueInfo>{"value": "none", "desc": "None"},
    <AllowedValueInfo>{"value": "other", "desc": "Other"},
    <AllowedValueInfo>{"value": "quarantine", "desc": "Quarantine"},
    <AllowedValueInfo>{"value": "sanitary_phytosanitary_inspection", "desc": "Sanitary / phytosanitary inspection"},
);

#! Customs info input type (polymorphic: id or full fields)
public class EasyPostCustomsInfoInputType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Customs Info ID",
                "short_desc": "Existing customs info ID (cstinfo_xxx)",
                "desc": "If provided, references an existing customs info object.",
                "type": SoftStringOrNothingType,
            },
            "eel_pfc": {
                "display_name": "EEL/PFC",
                "short_desc": "EEL or PFC value",
                "desc": "Either an Electronic Export Information (EEI) filing citation, an Exemption "
                    "and Exclusion Legend (EEL) code, or a Proof of Filing Citation (PFC) number. Use "
                    "`NOEEI 30.37(a)` for shipments under USD $2,500.",
                "type": SoftStringOrNothingType,
            },
            "contents_type": {
                "display_name": "Contents Type",
                "short_desc": "Nature of the shipment contents",
                "desc": "Type of contents being shipped",
                "type": SoftStringOrNothingType,
                "allowed_values": ContentsTypeAllowedValues,
            },
            "contents_explanation": {
                "display_name": "Contents Explanation",
                "short_desc": "Required if contents_type is other",
                "desc": "Free-text explanation of contents (required when `contents_type` is `other`)",
                "type": SoftStringOrNothingType,
            },
            "customs_certify": {
                "display_name": "Customs Certify",
                "short_desc": "Certify customs declarations",
                "desc": "Set to `True` to certify the customs declarations",
                "type": SoftBoolOrNothingType,
            },
            "customs_signer": {
                "display_name": "Customs Signer",
                "short_desc": "Name of customs declaration signer",
                "desc": "Name of the person certifying the customs declarations",
                "type": SoftStringOrNothingType,
            },
            "non_delivery_option": {
                "display_name": "Non-Delivery Option",
                "short_desc": "What to do if delivery fails",
                "desc": "Action to take if the shipment cannot be delivered",
                "type": SoftStringOrNothingType,
                "allowed_values": NonDeliveryOptionAllowedValues,
            },
            "restriction_type": {
                "display_name": "Restriction Type",
                "short_desc": "Restriction category",
                "desc": "Type of customs restriction (if any)",
                "type": SoftStringOrNothingType,
                "allowed_values": RestrictionTypeAllowedValues,
            },
            "restriction_comments": {
                "display_name": "Restriction Comments",
                "short_desc": "Comments on the restriction",
                "desc": "Free-text comments on the restriction (when applicable)",
                "type": SoftStringOrNothingType,
            },
            "customs_items": {
                "display_name": "Customs Items",
                "short_desc": "List of items in the shipment",
                "desc": "List of customs items contained in the shipment",
                "type": new ListDataType("EasyPostCustomsItemList", new EasyPostCustomsItemInputType(), True),
            },
            "declaration": {
                "display_name": "Declaration",
                "short_desc": "Free-form declaration text",
                "desc": "Free-form declaration text",
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

#! Customs info record (response)
public class EasyPostCustomsInfoType inherits HashDataType {
    public {
        const Fields = {
            "id": {"display_name": "ID", "type": SoftStringOrNothingType, "desc": "Customs info ID (`cstinfo_xxx`)"},
            "object": {"display_name": "Object Type", "type": SoftStringOrNothingType, "desc": "Always `CustomsInfo`"},
            "eel_pfc": {"display_name": "EEL/PFC", "type": SoftStringOrNothingType, "desc": "EEL or PFC value"},
            "contents_type": {"display_name": "Contents Type", "type": SoftStringOrNothingType, "desc": "Nature of contents"},
            "contents_explanation": {"display_name": "Contents Explanation", "type": SoftStringOrNothingType, "desc": "Explanation when `contents_type` is `other`"},
            "customs_certify": {"display_name": "Customs Certify", "type": SoftBoolOrNothingType, "desc": "Whether the customs declarations are certified"},
            "customs_signer": {"display_name": "Customs Signer", "type": SoftStringOrNothingType, "desc": "Name of the person certifying the declarations"},
            "non_delivery_option": {"display_name": "Non-Delivery Option", "type": SoftStringOrNothingType, "desc": "Action on non-delivery"},
            "restriction_type": {"display_name": "Restriction Type", "type": SoftStringOrNothingType, "desc": "Type of restriction"},
            "restriction_comments": {"display_name": "Restriction Comments", "type": SoftStringOrNothingType, "desc": "Comments on restriction"},
            "customs_items": {"display_name": "Customs Items", "type": new ListDataType("EasyPostCustomsItemRecordList", new EasyPostCustomsItemType(), True), "desc": "List of customs items"},
            "declaration": {"display_name": "Declaration", "type": SoftStringOrNothingType, "desc": "Free-form declaration"},
            "created_at": {"display_name": "Created At", "type": SoftDateOrNothingType, "desc": "Creation timestamp"},
            "updated_at": {"display_name": "Updated At", "type": SoftDateOrNothingType, "desc": "Last-update timestamp"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
