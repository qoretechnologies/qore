# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Katana variant sub-type (used inside product/material create + standalone)
public class KatanaVariantDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftIntOrNothingType,
                "display_name": "ID",
                "short_desc": "Variant ID",
                "desc": "The unique identifier of the variant",
            },
            "sku": {
                "type": SoftStringType,
                "display_name": "SKU",
                "required": True,
                "short_desc": "Stock keeping unit",
                "desc": "Unique stock keeping unit identifier",
            },
            "sales_price": {
                "type": SoftFloatOrNothingType,
                "display_name": "Sales Price",
                "short_desc": "Variant sales price",
                "desc": "Sales price for the variant",
            },
            "purchase_price": {
                "type": SoftFloatOrNothingType,
                "display_name": "Purchase Price",
                "short_desc": "Variant purchase price",
                "desc": "Purchase (cost) price for the variant",
            },
            "product_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Product ID",
                "short_desc": "Parent product ID",
                "desc": "The ID of the parent product, if this variant belongs to a product",
                "ref_data": "products",
            },
            "material_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Material ID",
                "short_desc": "Parent material ID",
                "desc": "The ID of the parent material, if this variant belongs to a material",
                "ref_data": "materials",
            },
            "internal_barcode": {
                "type": SoftStringOrNothingType,
                "display_name": "Internal Barcode",
                "short_desc": "Internal barcode",
                "desc": "Internal barcode for the variant",
            },
            "registered_barcode": {
                "type": SoftStringOrNothingType,
                "display_name": "Registered Barcode",
                "short_desc": "Registered barcode (e.g. UPC/EAN)",
                "desc": "Registered barcode such as UPC or EAN",
            },
            "supplier_item_codes": {
                "type": new ListDataType("KatanaSupplierItemCodesList", SoftStringType, True),
                "display_name": "Supplier Item Codes",
                "short_desc": "Supplier-specific item codes",
                "desc": "List of supplier-specific item codes",
            },
            "config_attributes": {
                "type": new ListDataType("KatanaVariantConfigAttributesList",
                    new KatanaConfigAttributeDataType(), True),
                "display_name": "Config Attributes",
                "short_desc": "Variant configuration attributes",
                "desc": "List of configuration attributes assigned to this variant",
            },
            "custom_fields": {
                "type": new ListDataType("KatanaVariantCustomFieldsList",
                    new KatanaCustomFieldEntryDataType(), True),
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "List of custom field values for the variant",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace KatanaDataProvider
