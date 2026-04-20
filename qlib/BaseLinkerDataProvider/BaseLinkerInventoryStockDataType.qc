# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerInventoryStockDataType.qc
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! A single inventory-stock row (getInventoryProductsStock result entry)
public class BaseLinkerInventoryStockDataType inherits HashDataType {
    public {
        const Fields = {
            "product_id": {"display_name": "Product ID",
                "short_desc": "Inventory product identifier", "type": SoftStringType},
            "variant_id": {"display_name": "Variant ID",
                "short_desc": "Variant identifier (if any)", "type": SoftStringOrNothingType},
            "stock": {
                "display_name": "Stock",
                "short_desc": "Map of warehouse-id to quantity",
                "type": AbstractDataProviderType::get(AutoHashOrNothingType),
                "desc": "Hash whose keys are warehouse identifiers and values are integer "
                    "stock quantities.",
            },
        };
    }
    constructor() : HashDataType("BaseLinkerInventoryStockDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
