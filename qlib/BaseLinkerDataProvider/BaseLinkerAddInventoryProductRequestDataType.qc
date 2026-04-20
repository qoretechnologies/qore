# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerAddInventoryProductRequestDataType.qc addInventoryProduct request type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Request type for the BaseLinker `addInventoryProduct` method (create/update upsert)
/** If `product_id` is omitted a new product is created. If `product_id` is
    provided the existing product is updated in place.
*/
public class BaseLinkerAddInventoryProductRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "inventory_id": {
                "display_name": "Inventory ID",
                "short_desc": "Target inventory (catalog)",
                "type": SoftStringType,
                "required": True,
                "desc": "Identifier of the inventory (catalog); see `list-inventories`.",
                "ref_data": "inventories",
            },
            "product_id": {
                "display_name": "Product ID",
                "short_desc": "Existing product ID (omit to create new)",
                "type": SoftStringOrNothingType,
                "desc": "If omitted, a new product is created. If provided, the existing product "
                    "is updated in place.",
                "ref_data": "inventory_products",
                "depends_on": ("inventory_id",),
            },
            "parent_id": {
                "display_name": "Parent Product ID",
                "short_desc": "Parent product (for bundles / variants)",
                "type": SoftStringOrNothingType,
            },
            "is_bundle": {
                "display_name": "Is Bundle",
                "short_desc": "Whether this product is a bundle",
                "type": SoftBoolOrNothingType,
            },
            "ean": {
                "display_name": "EAN",
                "short_desc": "EAN / barcode",
                "type": SoftStringOrNothingType,
            },
            "sku": {
                "display_name": "SKU",
                "short_desc": "Stock-keeping unit",
                "type": SoftStringOrNothingType,
            },
            "tax_rate": {
                "display_name": "Tax Rate",
                "short_desc": "Tax percentage",
                "type": SoftFloatOrNothingType,
                "desc": "Tax rate as a percentage (e.g. `23` for 23%). "
                    "Special values: `-1` = tax exempt, `-0.02` = 0% reverse charge, "
                    "`-0.03` = 0% VAT-OSS.",
            },
            "weight": {
                "display_name": "Weight (kg)",
                "short_desc": "Product weight",
                "type": SoftFloatOrNothingType,
            },
            "height": {
                "display_name": "Height (cm)",
                "short_desc": "Product height",
                "type": SoftFloatOrNothingType,
            },
            "width": {
                "display_name": "Width (cm)",
                "short_desc": "Product width",
                "type": SoftFloatOrNothingType,
            },
            "length": {
                "display_name": "Length (cm)",
                "short_desc": "Product length",
                "type": SoftFloatOrNothingType,
            },
            "star": {
                "display_name": "Rating",
                "short_desc": "Internal 0-5 rating",
                "type": SoftIntOrNothingType,
                "desc": "Internal product rating in the range `0`-`5`.",
            },
            "manufacturer_id": {
                "display_name": "Manufacturer ID",
                "short_desc": "Manufacturer identifier",
                "type": SoftStringOrNothingType,
            },
            "category_id": {
                "display_name": "Category ID",
                "short_desc": "Category identifier",
                "type": SoftStringOrNothingType,
            },
            "prices": {
                "display_name": "Prices",
                "short_desc": "Per-price-group prices",
                "type": AbstractDataProviderType::get(AutoHashOrNothingType),
                "desc": "Map of `{\"<price_group_id>\": <price>}`; keys are the IDs of price "
                    "groups configured in BaseLinker.",
            },
            "stock": {
                "display_name": "Stock",
                "short_desc": "Per-warehouse stock quantities",
                "type": AbstractDataProviderType::get(AutoHashOrNothingType),
                "desc": "Map of `{\"<warehouse_id>\": <qty>}` (warehouse IDs use prefixes like "
                    "`bl_<id>` for BaseLinker warehouses or `shop_<id>` for shops).",
            },
            "locations": {
                "display_name": "Locations",
                "short_desc": "Per-warehouse location codes",
                "type": AbstractDataProviderType::get(AutoHashOrNothingType),
                "desc": "Map of `{\"<warehouse_id>\": \"<location>\"}`.",
            },
            "text_fields": {
                "display_name": "Text Fields",
                "short_desc": "Multilang text fields",
                "type": AbstractDataProviderType::get(AutoHashOrNothingType),
                "desc": "Multilang text fields keyed as `<field>|<lang>|<source_id>` "
                    "(e.g. `name`, `name|en`, `description|de|0`). The special key `features` "
                    "holds a hash of `{param_name: param_value}`.",
            },
            "images": {
                "display_name": "Images",
                "short_desc": "Image slots (0-15)",
                "type": AbstractDataProviderType::get(AutoHashOrNothingType),
                "desc": "Map keyed by slot index (`0`-`15`) to slot value. Values: "
                    "`url:<https-url>`, `data:<base64>`, or empty string to clear.",
            },
            "links": {
                "display_name": "Links",
                "short_desc": "External-system product links",
                "type": AbstractDataProviderType::get(AutoHashOrNothingType),
                "desc": "Map of external-system identifiers linked to this product.",
            },
            "average_cost": {
                "display_name": "Average Cost",
                "short_desc": "Average purchase cost",
                "type": SoftFloatOrNothingType,
            },
            "asin": {
                "display_name": "ASIN",
                "short_desc": "Amazon Standard Identification Number",
                "type": SoftStringOrNothingType,
            },
        };
    }
    constructor() : HashDataType("BaseLinkerAddInventoryProductRequestDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
