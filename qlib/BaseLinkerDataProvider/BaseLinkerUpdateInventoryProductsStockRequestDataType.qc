# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerUpdateInventoryProductsStockRequestDataType.qc updateInventoryProductsStock request type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Request type for the BaseLinker `updateInventoryProductsStock` method
/** BaseLinker accepts up to 1000 products per call.
*/
public class BaseLinkerUpdateInventoryProductsStockRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "inventory_id": {
                "display_name": "Inventory ID",
                "short_desc": "Target inventory",
                "type": SoftStringType,
                "required": True,
                "ref_data": "inventories",
            },
            "products": {
                "display_name": "Products",
                "short_desc": "Per-product stock updates (max 1000 products)",
                "type": AbstractDataProviderType::get(AutoHashType),
                "required": True,
                "desc": "Nested map of the shape "
                    "`{\"<product_id>\": {\"<variant_id>|bl_<warehouse_id>\": <qty>}}`. "
                    "For non-variant products use the product ID as the inner key prefix "
                    "(e.g. `0|bl_123`). Up to 1000 products per call.",
            },
        };
    }
    constructor() : HashDataType("BaseLinkerUpdateInventoryProductsStockRequestDataType") {
        addQoreFields(Fields);
    }
}
} # namespace BaseLinkerDataProvider
