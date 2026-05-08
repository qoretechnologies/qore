# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Customs item input type (polymorphic: id or full fields)
public class EasyPostCustomsItemInputType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Customs Item ID",
                "short_desc": "Existing customs item ID (cstitem_xxx)",
                "desc": "If provided, references an existing customs item.",
                "type": SoftStringOrNothingType,
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Item description",
                "desc": "Description of the item being shipped",
                "type": SoftStringOrNothingType,
            },
            "quantity": {
                "display_name": "Quantity",
                "short_desc": "Number of units",
                "desc": "Quantity of items being shipped",
                "type": SoftIntOrNothingType,
            },
            "value": {
                "display_name": "Value",
                "short_desc": "Total value (string)",
                "desc": "Total value of all items, as a string (e.g. `\"12.50\"`)",
                "type": SoftStringOrNothingType,
            },
            "weight": {
                "display_name": "Weight",
                "short_desc": "Weight in ounces",
                "desc": "Weight of all items in ounces",
                "type": SoftFloatOrNothingType,
            },
            "hs_tariff_number": {
                "display_name": "HS Tariff Number",
                "short_desc": "Harmonized System tariff number",
                "desc": "Harmonized System tariff number (six-digit code)",
                "type": SoftStringOrNothingType,
            },
            "origin_country": {
                "display_name": "Origin Country",
                "short_desc": "Origin country code",
                "desc": "ISO 2-letter country code of the item's origin",
                "type": SoftStringOrNothingType,
            },
            "code": {
                "display_name": "Code",
                "short_desc": "Internal item code",
                "desc": "Internal reference code for the item",
                "type": SoftStringOrNothingType,
            },
            "currency": {
                "display_name": "Currency",
                "short_desc": "ISO currency code",
                "desc": "ISO 4217 currency code (e.g. `USD`)",
                "type": SoftStringOrNothingType,
            },
            "manufacturer": {
                "display_name": "Manufacturer",
                "short_desc": "Manufacturer name",
                "desc": "Manufacturer of the item",
                "type": SoftStringOrNothingType,
            },
            "eccn": {
                "display_name": "ECCN",
                "short_desc": "Export Control Classification Number",
                "desc": "Export Control Classification Number",
                "type": SoftStringOrNothingType,
            },
            "printed_commodity_identifier": {
                "display_name": "Printed Commodity Identifier",
                "short_desc": "Identifier printed on label",
                "desc": "Optional identifier printed on the customs form",
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

#! Customs item record (response)
public class EasyPostCustomsItemType inherits HashDataType {
    public {
        const Fields = {
            "id": {"display_name": "ID", "type": SoftStringOrNothingType, "desc": "Customs item ID (`cstitem_xxx`)"},
            "object": {"display_name": "Object Type", "type": SoftStringOrNothingType, "desc": "Always `CustomsItem`"},
            "description": {"display_name": "Description", "type": SoftStringOrNothingType, "desc": "Item description"},
            "quantity": {"display_name": "Quantity", "type": SoftIntOrNothingType, "desc": "Quantity"},
            "value": {"display_name": "Value", "type": SoftStringOrNothingType, "desc": "Total value (string)"},
            "weight": {"display_name": "Weight", "type": SoftFloatOrNothingType, "desc": "Weight (oz)"},
            "hs_tariff_number": {"display_name": "HS Tariff Number", "type": SoftStringOrNothingType, "desc": "Harmonized System tariff number"},
            "origin_country": {"display_name": "Origin Country", "type": SoftStringOrNothingType, "desc": "Origin country code"},
            "code": {"display_name": "Code", "type": SoftStringOrNothingType, "desc": "Internal code"},
            "currency": {"display_name": "Currency", "type": SoftStringOrNothingType, "desc": "Currency code"},
            "manufacturer": {"display_name": "Manufacturer", "type": SoftStringOrNothingType, "desc": "Manufacturer"},
            "eccn": {"display_name": "ECCN", "type": SoftStringOrNothingType, "desc": "Export classification"},
            "printed_commodity_identifier": {"display_name": "Printed Commodity Identifier", "type": SoftStringOrNothingType, "desc": "Identifier printed on label"},
            "created_at": {"display_name": "Created At", "type": SoftDateOrNothingType, "desc": "Creation timestamp"},
            "updated_at": {"display_name": "Updated At", "type": SoftDateOrNothingType, "desc": "Last-update timestamp"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
