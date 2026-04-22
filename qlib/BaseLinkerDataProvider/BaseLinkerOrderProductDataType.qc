# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerOrderProductDataType.qc shared order line-item data type

/*  BaseLinkerOrderProductDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT licensed — see BaseLinkerDataProvider.qm for the full notice.
*/

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! Shared data type for BaseLinker order line items
/** Used by addOrder, addOrderProduct, setOrderProductFields and addOrderReturn.
*/
public class BaseLinkerOrderProductDataType inherits HashDataType {
    public {
        const Fields = {
            "storage": {
                "display_name": "Storage",
                "short_desc": "Storage identifier for product origin",
                "type": SoftStringType,
                "required": True,
                "desc": "The storage the product originates from. One of `db`, `shop`, `warehouse`.",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "db",
                        "display_name": "BaseLinker Inventory"},
                    <AllowedValueInfo>{"value": "shop",
                        "display_name": "External Shop"},
                    <AllowedValueInfo>{"value": "warehouse",
                        "display_name": "Legacy Warehouse"},
                ),
            },
            "storage_id": {
                "display_name": "Storage ID",
                "short_desc": "Storage-specific ID (inventory/shop/warehouse ID)",
                "type": SoftStringType,
                "required": True,
                "desc": "Identifier of the storage (inventory ID for `db`, shop ID for `shop`, "
                    "warehouse ID for `warehouse`).",
            },
            "product_id": {
                "display_name": "Product ID",
                "short_desc": "Product identifier in the chosen storage",
                "type": SoftStringOrNothingType,
                "desc": "Product identifier inside the chosen `storage`.",
            },
            "variant_id": {
                "display_name": "Variant ID",
                "short_desc": "Variant identifier",
                "type": SoftStringOrNothingType,
                "desc": "Optional product-variant identifier.",
            },
            "name": {
                "display_name": "Product Name",
                "short_desc": "Name of the product",
                "type": SoftStringType,
                "required": True,
                "desc": "Product name shown on the order line.",
                "example_value": "Sample Widget",
            },
            "sku": {
                "display_name": "SKU",
                "short_desc": "Stock-keeping unit",
                "type": SoftStringOrNothingType,
                "desc": "Product SKU.",
            },
            "ean": {
                "display_name": "EAN",
                "short_desc": "EAN / barcode",
                "type": SoftStringOrNothingType,
                "desc": "Product EAN (barcode).",
            },
            "location": {
                "display_name": "Warehouse Location",
                "short_desc": "Storage location identifier",
                "type": SoftStringOrNothingType,
                "desc": "Warehouse location code.",
            },
            "warehouse_id": {
                "display_name": "Warehouse ID",
                "short_desc": "BaseLinker warehouse ID",
                "type": SoftIntOrNothingType,
                "desc": "BaseLinker warehouse ID from which this item is shipped.",
            },
            "attributes": {
                "display_name": "Attributes",
                "short_desc": "Free-text variant attributes",
                "type": SoftStringOrNothingType,
                "desc": "Comma-separated list of variant attributes, e.g. `Size: L, Color: Red`.",
            },
            "price_brutto": {
                "display_name": "Price (Gross)",
                "short_desc": "Unit price including tax",
                "type": SoftFloatType,
                "required": True,
                "desc": "Unit price (gross, including tax).",
                "example_value": 49.99,
            },
            "tax_rate": {
                "display_name": "Tax Rate",
                "short_desc": "Tax percentage",
                "type": SoftFloatType,
                "required": True,
                "desc": "Tax rate as a percentage (e.g. `23` for 23%). "
                    "Special values: `-1` = tax exempt, `-0.02` = 0% reverse charge, "
                    "`-0.03` = 0% VAT-OSS.",
                "example_value": 23,
            },
            "quantity": {
                "display_name": "Quantity",
                "short_desc": "Number of units",
                "type": SoftIntType,
                "required": True,
                "desc": "Quantity of the product on the order line.",
                "example_value": 1,
            },
            "weight": {
                "display_name": "Weight",
                "short_desc": "Unit weight (kg)",
                "type": SoftFloatOrNothingType,
                "desc": "Weight per unit in kilograms.",
            },
            "auction_id": {
                "display_name": "Marketplace Listing ID",
                "short_desc": "Marketplace auction/listing ID",
                "type": SoftStringOrNothingType,
                "desc": "Marketplace auction or listing ID (eBay, Allegro, Amazon, etc.).",
            },
        };
    }

    constructor() : HashDataType("BaseLinkerOrderProductDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
