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
                "desc": "Identifier of the parent product when this product is a variant "
                    "or bundle component.",
                "ref_data": "inventory_products",
                "depends_on": ("inventory_id",),
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
                "example_value": "5901234123457",
            },
            "ean_additional": {
                "display_name": "Additional EANs",
                "short_desc": "List of additional EANs with quantities",
                "type": new ListDataType("ean_additional",
                    new BaseLinkerEanAdditionalDataType(), True),
                "desc": "List of additional EANs. Each entry is a hash "
                    "`{\"ean\": \"<ean>\", \"quantity\": <n>}`.",
            },
            "sku": {
                "display_name": "SKU",
                "short_desc": "Stock-keeping unit",
                "type": SoftStringOrNothingType,
                "example_value": "WIDGET-001",
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
                "desc": "Internal product rating in the range `0`-`5`, where `0` means no rating.",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 0, "display_name": "No Rating"},
                    <AllowedValueInfo>{"value": 1, "display_name": "1 Star"},
                    <AllowedValueInfo>{"value": 2, "display_name": "2 Stars"},
                    <AllowedValueInfo>{"value": 3, "display_name": "3 Stars"},
                    <AllowedValueInfo>{"value": 4, "display_name": "4 Stars"},
                    <AllowedValueInfo>{"value": 5, "display_name": "5 Stars"},
                ),
            },
            "manufacturer_id": {
                "display_name": "Manufacturer ID",
                "short_desc": "Manufacturer identifier",
                "type": SoftStringOrNothingType,
                "desc": "Manufacturer identifier; see `list-inventory-manufacturers`.",
                "ref_data": "inventory_manufacturers",
            },
            "category_id": {
                "display_name": "Category ID",
                "short_desc": "Category identifier",
                "type": SoftStringOrNothingType,
                "desc": "Category identifier; see `list-inventory-categories`. Scoped to the "
                    "selected `inventory_id`.",
                "ref_data": "inventory_categories",
                "depends_on": ("inventory_id",),
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
                "desc": "Map of external-system product links. Keys are of the form "
                    "`<type>_<id>` (where `<type>` identifies the external system, e.g. "
                    "`shop`, `amazon`, and `<id>` is the BaseLinker-side identifier of "
                    "that system). Each value is a hash "
                    "`{\"product_id\": \"<external-id>\", \"variant_id\": \"<external-variant-id>\"}` "
                    "where `variant_id` is optional.",
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
            "tags": {
                "display_name": "Tags",
                "short_desc": "List of tag names",
                "type": new ListDataType("tags", SoftStringType, True),
                "desc": "List of tag names (strings) to associate with this product.",
            },
            "bundle_products": {
                "display_name": "Bundle Products",
                "short_desc": "Products included in this bundle",
                "type": AbstractDataProviderType::get(AutoHashOrNothingType),
                "desc": "Map of `{\"<product_id>\": <quantity>}` where each key is the "
                    "identifier of a product included in this bundle and the value is the "
                    "number of pieces.",
            },
        };
    }
    constructor() : HashDataType("BaseLinkerAddInventoryProductRequestDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
