# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Katana product record data type
public class KatanaProductDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntOrNothingType, "display_name": "ID",
                "short_desc": "Product ID", "desc": "Unique product identifier"},
            "name": {"type": SoftStringOrNothingType, "display_name": "Name",
                "short_desc": "Product name", "desc": "Name of the product"},
            "uom": {"type": SoftStringOrNothingType, "display_name": "Unit of Measure",
                "short_desc": "Unit of measure", "desc": "Unit of measure (e.g. `pcs`, `kg`)"},
            "category_name": {"type": SoftStringOrNothingType, "display_name": "Category",
                "short_desc": "Category name", "desc": "Name of the product category"},
            "is_sellable": {"type": SoftBoolOrNothingType, "display_name": "Sellable",
                "short_desc": "Sellable flag", "desc": "If `True`, this product can be sold"},
            "is_producible": {"type": SoftBoolOrNothingType, "display_name": "Producible",
                "short_desc": "Producible flag",
                "desc": "If `True`, this product can be manufactured"},
            "is_purchasable": {"type": SoftBoolOrNothingType, "display_name": "Purchasable",
                "short_desc": "Purchasable flag",
                "desc": "If `True`, this product can be purchased"},
            "type": {"type": SoftStringOrNothingType, "display_name": "Type",
                "short_desc": "Product type", "desc": "Product type"},
            "default_supplier_id": {"type": SoftIntOrNothingType,
                "display_name": "Default Supplier ID",
                "short_desc": "Default supplier ID",
                "desc": "The ID of the default supplier",
                "ref_data": "suppliers"},
            "additional_info": {"type": SoftStringOrNothingType,
                "display_name": "Additional Info",
                "short_desc": "Additional info", "desc": "Free-form additional information"},
            "batch_tracked": {"type": SoftBoolOrNothingType, "display_name": "Batch Tracked",
                "short_desc": "Batch tracked",
                "desc": "If `True`, inventory of this product is tracked by batch"},
            "serial_tracked": {"type": SoftBoolOrNothingType, "display_name": "Serial Tracked",
                "short_desc": "Serial number tracked",
                "desc": "If `True`, inventory is tracked by serial number"},
            "operations_in_sequence": {"type": SoftBoolOrNothingType,
                "display_name": "Operations In Sequence",
                "short_desc": "Operations in sequence",
                "desc": "If `True`, operations must occur in sequence"},
            "purchase_uom": {"type": SoftStringOrNothingType, "display_name": "Purchase UoM",
                "short_desc": "Purchase UoM", "desc": "Unit of measure used for purchasing"},
            "purchase_uom_conversion_rate": {"type": SoftFloatOrNothingType,
                "display_name": "Purchase UoM Conversion Rate",
                "short_desc": "Purchase UoM conversion rate",
                "desc": "Conversion rate between purchase UoM and stock UoM"},
            "variants": {"type": new ListDataType("KatanaProductVariantList",
                    new KatanaVariantDataType(), True),
                "display_name": "Variants", "short_desc": "Product variants",
                "desc": "List of variants belonging to the product"},
            "configs": {"type": new ListDataType("KatanaProductConfigList",
                    new KatanaConfigDataType(), True),
                "display_name": "Configs", "short_desc": "Configuration attributes",
                "desc": "List of configuration attribute definitions"},
            "custom_field_collection_id": {"type": SoftIntOrNothingType,
                "display_name": "Custom Field Collection ID",
                "short_desc": "Custom field collection ID",
                "desc": "The ID of the custom field collection"},
            "created_at": {"type": SoftDateOrNothingType, "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "UTC timestamp when the product was created"},
            "updated_at": {"type": SoftDateOrNothingType, "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "UTC timestamp when the product was last updated"},
            "deleted_at": {"type": SoftDateOrNothingType, "display_name": "Deleted At",
                "short_desc": "Deletion timestamp",
                "desc": "UTC timestamp when the product was soft-deleted, if applicable"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Create product request type
public class KatanaCreateProductRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {"type": SoftStringType, "required": True, "display_name": "Name",
                "short_desc": "Product name",
                "desc": "Name of the product (required)", "example_value": "Widget"},
            "uom": {"type": SoftStringOrNothingType, "display_name": "Unit of Measure",
                "short_desc": "Unit of measure",
                "desc": "Unit of measure (e.g. `pcs`)"},
            "category_name": {"type": SoftStringOrNothingType, "display_name": "Category",
                "short_desc": "Category", "desc": "Product category name"},
            "is_sellable": {"type": SoftBoolOrNothingType, "display_name": "Sellable",
                "short_desc": "Sellable", "desc": "If `True`, product can be sold"},
            "is_producible": {"type": SoftBoolOrNothingType, "display_name": "Producible",
                "short_desc": "Producible", "desc": "If `True`, product can be manufactured"},
            "is_purchasable": {"type": SoftBoolOrNothingType, "display_name": "Purchasable",
                "short_desc": "Purchasable", "desc": "If `True`, product can be purchased"},
            "default_supplier_id": {"type": SoftIntOrNothingType,
                "display_name": "Default Supplier",
                "short_desc": "Default supplier",
                "desc": "Default supplier ID", "ref_data": "suppliers"},
            "additional_info": {"type": SoftStringOrNothingType,
                "display_name": "Additional Info",
                "short_desc": "Additional info", "desc": "Free-form additional information"},
            "batch_tracked": {"type": SoftBoolOrNothingType, "display_name": "Batch Tracked",
                "short_desc": "Batch tracked",
                "desc": "If `True`, inventory tracked by batch"},
            "serial_tracked": {"type": SoftBoolOrNothingType, "display_name": "Serial Tracked",
                "short_desc": "Serial tracked",
                "desc": "If `True`, inventory tracked by serial number"},
            "operations_in_sequence": {"type": SoftBoolOrNothingType,
                "display_name": "Operations In Sequence",
                "short_desc": "Operations sequential", "desc": "Operations occur in sequence"},
            "purchase_uom": {"type": SoftStringOrNothingType, "display_name": "Purchase UoM",
                "short_desc": "Purchase UoM", "desc": "Purchase unit of measure"},
            "purchase_uom_conversion_rate": {"type": SoftFloatOrNothingType,
                "display_name": "Purchase UoM Conversion",
                "short_desc": "Conversion rate",
                "desc": "Conversion between purchase UoM and stock UoM"},
            "variants": {"type": new ListDataType("KatanaCreateProductVariantList",
                    new KatanaVariantDataType(), False),
                "required": True, "display_name": "Variants",
                "short_desc": "Product variants",
                "desc": "List of variants for the product (at least one required)"},
            "configs": {"type": new ListDataType("KatanaCreateProductConfigList",
                    new KatanaConfigDataType(), True),
                "display_name": "Configs", "short_desc": "Configuration attributes",
                "desc": "List of configuration attribute definitions"},
            "custom_field_collection_id": {"type": SoftIntOrNothingType,
                "display_name": "Custom Field Collection",
                "short_desc": "Custom field collection ID",
                "desc": "Custom field collection ID"},
            "is_auto_assembly": {"type": SoftBoolOrNothingType,
                "display_name": "Auto Assembly",
                "short_desc": "Auto assembly",
                "desc": "If `True`, the product is auto-assembled on sales orders"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Update product request type
public class KatanaUpdateProductRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True, "display_name": "Product ID",
                "short_desc": "Product ID to update", "desc": "ID of the product to update",
                "ref_data": "products"},
        } + (KatanaDataProviderBase::makeFieldsOptional(KatanaCreateProductRequestDataType::Fields)
            - ("variants",)) + {
            "is_archived": {"type": SoftBoolOrNothingType, "display_name": "Archived",
                "short_desc": "Archived flag",
                "desc": "If `True`, the product is archived (hidden from active lists)"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Delete product request type
public class KatanaDeleteProductRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True, "display_name": "Product ID",
                "short_desc": "Product ID to delete", "desc": "ID of the product to delete",
                "ref_data": "products"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace KatanaDataProvider
