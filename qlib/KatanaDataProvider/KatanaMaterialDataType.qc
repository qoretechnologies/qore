# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Katana material record data type
public class KatanaMaterialDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntOrNothingType, "display_name": "ID",
                "short_desc": "Material ID", "desc": "Unique material identifier"},
            "name": {"type": SoftStringOrNothingType, "display_name": "Name",
                "short_desc": "Material name", "desc": "Name of the material"},
            "uom": {"type": SoftStringOrNothingType, "display_name": "Unit of Measure",
                "short_desc": "Unit of measure", "desc": "Unit of measure (e.g. `kg`)"},
            "category_name": {"type": SoftStringOrNothingType, "display_name": "Category",
                "short_desc": "Category", "desc": "Material category name"},
            "is_sellable": {"type": SoftBoolOrNothingType, "display_name": "Sellable",
                "short_desc": "Sellable", "desc": "If `True`, can be sold"},
            "is_purchasable": {"type": SoftBoolOrNothingType, "display_name": "Purchasable",
                "short_desc": "Purchasable", "desc": "If `True`, can be purchased"},
            "default_supplier_id": {"type": SoftIntOrNothingType,
                "display_name": "Default Supplier ID",
                "short_desc": "Default supplier",
                "desc": "Default supplier ID", "ref_data": "suppliers"},
            "additional_info": {"type": SoftStringOrNothingType,
                "display_name": "Additional Info",
                "short_desc": "Additional info", "desc": "Free-form additional information"},
            "batch_tracked": {"type": SoftBoolOrNothingType, "display_name": "Batch Tracked",
                "short_desc": "Batch tracked", "desc": "If `True`, tracked by batch"},
            "serial_tracked": {"type": SoftBoolOrNothingType, "display_name": "Serial Tracked",
                "short_desc": "Serial tracked", "desc": "If `True`, tracked by serial number"},
            "purchase_uom": {"type": SoftStringOrNothingType, "display_name": "Purchase UoM",
                "short_desc": "Purchase UoM", "desc": "Purchase unit of measure"},
            "purchase_uom_conversion_rate": {"type": SoftFloatOrNothingType,
                "display_name": "Purchase UoM Conversion",
                "short_desc": "Conversion rate",
                "desc": "Conversion between purchase UoM and stock UoM"},
            "variants": {"type": new ListDataType("KatanaMaterialVariantList",
                    new KatanaVariantDataType(), True),
                "display_name": "Variants", "short_desc": "Material variants",
                "desc": "List of variants belonging to the material"},
            "configs": {"type": new ListDataType("KatanaMaterialConfigList",
                    new KatanaConfigDataType(), True),
                "display_name": "Configs", "short_desc": "Configuration attributes",
                "desc": "List of configuration attribute definitions"},
            "custom_field_collection_id": {"type": SoftIntOrNothingType,
                "display_name": "Custom Field Collection ID",
                "short_desc": "Custom field collection ID",
                "desc": "Custom field collection ID"},
            "created_at": {"type": SoftDateOrNothingType, "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "UTC timestamp when the material was created"},
            "updated_at": {"type": SoftDateOrNothingType, "display_name": "Updated At",
                "short_desc": "Last update",
                "desc": "UTC timestamp when last updated"},
            "deleted_at": {"type": SoftDateOrNothingType, "display_name": "Deleted At",
                "short_desc": "Deletion timestamp",
                "desc": "UTC timestamp when the material was soft-deleted"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Create material request type
public class KatanaCreateMaterialRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {"type": SoftStringType, "required": True, "display_name": "Name",
                "short_desc": "Material name", "desc": "Name of the material (required)",
                "example_value": "Steel sheet"},
            "uom": {"type": SoftStringOrNothingType, "display_name": "Unit of Measure",
                "short_desc": "Unit of measure", "desc": "Unit of measure (e.g. `kg`)"},
            "category_name": {"type": SoftStringOrNothingType, "display_name": "Category",
                "short_desc": "Category", "desc": "Material category name"},
            "is_sellable": {"type": SoftBoolOrNothingType, "display_name": "Sellable",
                "short_desc": "Sellable", "desc": "If `True`, can be sold"},
            "default_supplier_id": {"type": SoftIntOrNothingType,
                "display_name": "Default Supplier",
                "short_desc": "Default supplier",
                "desc": "Default supplier ID", "ref_data": "suppliers"},
            "additional_info": {"type": SoftStringOrNothingType,
                "display_name": "Additional Info",
                "short_desc": "Additional info", "desc": "Free-form additional information"},
            "batch_tracked": {"type": SoftBoolOrNothingType, "display_name": "Batch Tracked",
                "short_desc": "Batch tracked", "desc": "If `True`, tracked by batch"},
            "purchase_uom": {"type": SoftStringOrNothingType, "display_name": "Purchase UoM",
                "short_desc": "Purchase UoM", "desc": "Purchase unit of measure"},
            "purchase_uom_conversion_rate": {"type": SoftFloatOrNothingType,
                "display_name": "Purchase UoM Conversion",
                "short_desc": "Conversion rate",
                "desc": "Conversion between purchase UoM and stock UoM"},
            "variants": {"type": new ListDataType("KatanaCreateMaterialVariantList",
                    new KatanaVariantDataType(), False),
                "required": True, "display_name": "Variants",
                "short_desc": "Material variants",
                "desc": "List of variants for the material (at least one required)"},
            "configs": {"type": new ListDataType("KatanaCreateMaterialConfigList",
                    new KatanaConfigDataType(), True),
                "display_name": "Configs", "short_desc": "Configuration attributes",
                "desc": "List of configuration attribute definitions"},
            "custom_field_collection_id": {"type": SoftIntOrNothingType,
                "display_name": "Custom Field Collection",
                "short_desc": "Custom field collection ID",
                "desc": "Custom field collection ID"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaUpdateMaterialRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True, "display_name": "Material ID",
                "short_desc": "Material ID to update", "desc": "ID of the material to update",
                "ref_data": "materials"},
        } + (KatanaDataProviderBase::makeFieldsOptional(KatanaCreateMaterialRequestDataType::Fields)
            - ("variants",)) + {
            "is_archived": {"type": SoftBoolOrNothingType, "display_name": "Archived",
                "short_desc": "Archived flag",
                "desc": "If `True`, the material is archived (hidden from active lists)"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class KatanaDeleteMaterialRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {"type": SoftIntType, "required": True, "display_name": "Material ID",
                "short_desc": "Material ID to delete", "desc": "ID of the material to delete",
                "ref_data": "materials"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace KatanaDataProvider
