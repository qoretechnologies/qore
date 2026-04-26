# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerInventoryProductDataType.qc
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! BaseLinker inventory product record
public class BaseLinkerInventoryProductDataType inherits HashDataType {
    public {
        const Fields = {
            "product_id": {"display_name": "Product ID",
                "short_desc": "Product identifier", "type": SoftStringType},
            "inventory_id": {"display_name": "Inventory ID",
                "short_desc": "Owning inventory ID", "type": SoftStringOrNothingType},
            "sku": {"display_name": "SKU",
                "short_desc": "Stock-keeping unit", "type": SoftStringOrNothingType},
            "ean": {"display_name": "EAN",
                "short_desc": "EAN barcode", "type": SoftStringOrNothingType},
            "name": {"display_name": "Name",
                "short_desc": "Product name", "type": SoftStringOrNothingType},
            "description": {"display_name": "Description",
                "short_desc": "HTML product description", "type": SoftStringOrNothingType},
            "category_id": {"display_name": "Category ID",
                "short_desc": "Owning category ID", "type": SoftStringOrNothingType},
            "tax_rate": {"display_name": "Tax Rate",
                "short_desc": "Default tax rate (%)", "type": SoftFloatOrNothingType},
            "weight": {"display_name": "Weight",
                "short_desc": "Unit weight (kg)", "type": SoftFloatOrNothingType},
            "height": {"display_name": "Height",
                "short_desc": "Unit height (cm)", "type": SoftFloatOrNothingType},
            "width": {"display_name": "Width",
                "short_desc": "Unit width (cm)", "type": SoftFloatOrNothingType},
            "length": {"display_name": "Length",
                "short_desc": "Unit length (cm)", "type": SoftFloatOrNothingType},
            "is_bundle": {"display_name": "Is Bundle",
                "short_desc": "Whether this is a bundle product",
                "type": SoftBoolOrNothingType},
            "manufacturer_id": {"display_name": "Manufacturer ID",
                "short_desc": "Manufacturer identifier", "type": SoftStringOrNothingType},
            "average_cost": {"display_name": "Average Cost",
                "short_desc": "Average stock cost", "type": SoftFloatOrNothingType},
            "star": {"display_name": "Star Rating",
                "short_desc": "Product star rating (0-5)", "type": SoftIntOrNothingType},
            "prices": {"display_name": "Prices",
                "short_desc": "Map of price-group-id to gross price",
                "type": AbstractDataProviderType::get(AutoHashOrNothingType)},
            "stock": {"display_name": "Stock",
                "short_desc": "Map of warehouse-id to stock quantity",
                "type": AbstractDataProviderType::get(AutoHashOrNothingType)},
            "text_fields": {"display_name": "Text Fields",
                "short_desc": "Multilang text-field map",
                "type": AbstractDataProviderType::get(AutoHashOrNothingType)},
            "images": {"display_name": "Images",
                "short_desc": "Product image URLs / base64 data",
                "type": AbstractDataProviderType::get(AutoHashOrNothingType)},
            "links": {"display_name": "Links",
                "short_desc": "Product marketplace/shop links",
                "type": AbstractDataProviderType::get(AutoHashOrNothingType)},
        };
    }
    constructor() : HashDataType("BaseLinkerInventoryProductDataType") {
        addQoreFields(Fields);
    }
}
} # namespace BaseLinkerDataProvider
