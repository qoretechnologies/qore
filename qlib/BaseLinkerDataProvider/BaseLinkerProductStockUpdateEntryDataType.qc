# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerProductStockUpdateEntryDataType.qc
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! A single product entry in updateInventoryProductsStock
public class BaseLinkerProductStockUpdateEntryDataType inherits HashDataType {
    public {
        const Fields = {
            "product_id": {
                "display_name": "Product ID",
                "short_desc": "BaseLinker inventory product ID",
                "type": SoftStringType,
                "required": True,
                "desc": "BaseLinker inventory product ID to update.",
            },
            "variant_id": {
                "display_name": "Variant ID",
                "short_desc": "Optional variant identifier",
                "type": SoftStringOrNothingType,
            },
            "stock": {
                "display_name": "Stock Map",
                "short_desc": "Map of warehouse-id to quantity",
                "type": AbstractDataProviderType::get(AutoHashType),
                "required": True,
                "desc": "Hash whose keys are warehouse identifiers (e.g. `bl_123`) and values "
                    "are integer stock quantities.",
            },
        };
    }
    constructor() : HashDataType("BaseLinkerProductStockUpdateEntryDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
