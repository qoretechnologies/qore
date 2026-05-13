# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Katana variant record (standalone) - reuses KatanaVariantDataType fields plus timestamps
public class KatanaVariantRecordDataType inherits HashDataType {
    public {
        const Fields = KatanaVariantDataType::Fields + {
            "created_at": {"type": SoftDateOrNothingType, "display_name": "Created At",
                "short_desc": "Creation timestamp", "desc": "UTC timestamp when created"},
            "updated_at": {"type": SoftDateOrNothingType, "display_name": "Updated At",
                "short_desc": "Last update", "desc": "UTC timestamp when last updated"},
            "deleted_at": {"type": SoftDateOrNothingType, "display_name": "Deleted At",
                "short_desc": "Deletion timestamp",
                "desc": "UTC timestamp when soft-deleted"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Create variant request type
public class KatanaCreateVariantRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "sku": {"type": SoftStringType, "required": True, "display_name": "SKU",
                "short_desc": "Stock keeping unit", "desc": "Unique SKU (required)",
                "example_value": "WIDGET-001"},
            "product_id": {"type": SoftIntOrNothingType, "display_name": "Product ID",
                "short_desc": "Parent product ID",
                "desc": "ID of the parent product (mutually exclusive with `material_id`)",
                "ref_data": "products"},
            "material_id": {"type": SoftIntOrNothingType, "display_name": "Material ID",
                "short_desc": "Parent material ID",
                "desc": "ID of the parent material (mutually exclusive with `product_id`)",
                "ref_data": "materials"},
            "sales_price": {"type": SoftFloatOrNothingType, "display_name": "Sales Price",
                "short_desc": "Sales price", "desc": "Sales price for the variant"},
            "purchase_price": {"type": SoftFloatOrNothingType, "display_name": "Purchase Price",
                "short_desc": "Purchase price", "desc": "Purchase cost price for the variant"},
            "internal_barcode": {"type": SoftStringOrNothingType,
                "display_name": "Internal Barcode",
                "short_desc": "Internal barcode", "desc": "Internal barcode"},
            "registered_barcode": {"type": SoftStringOrNothingType,
                "display_name": "Registered Barcode",
                "short_desc": "Registered barcode", "desc": "Registered barcode (e.g. UPC/EAN)"},
            "supplier_item_codes": {"type": new ListDataType("KatanaVariantSupplierCodes",
                    SoftStringType, True),
                "display_name": "Supplier Item Codes",
                "short_desc": "Supplier codes", "desc": "Supplier-specific item codes"},
            "config_attributes": {"type": new ListDataType("KatanaCreateVariantConfigAttrs",
                    new KatanaConfigAttributeDataType(), True),
                "display_name": "Config Attributes", "short_desc": "Config attributes",
                "desc": "List of configuration attribute assignments"},
            "custom_fields": {"type": new ListDataType("KatanaCreateVariantCustomFields",
                    new KatanaCustomFieldEntryDataType(), True),
                "display_name": "Custom Fields", "short_desc": "Custom fields",
                "desc": "List of custom field values"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaUpdateVariantRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True, "display_name": "Variant ID",
                "short_desc": "Variant ID", "desc": "ID of the variant to update",
                "ref_data": "variants"},
        } + KatanaDataProviderBase::makeFieldsOptional(KatanaCreateVariantRequestDataType::Fields);
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaDeleteVariantRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True, "display_name": "Variant ID",
                "short_desc": "Variant ID", "desc": "ID of the variant to delete",
                "ref_data": "variants"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace KatanaDataProvider
