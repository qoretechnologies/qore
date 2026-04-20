# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerUpdateInventoryProductsPricesRequestDataType.qc updateInventoryProductsPrices request type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Request type for the BaseLinker `updateInventoryProductsPrices` method
/** BaseLinker accepts up to 1000 products per call.
*/
public class BaseLinkerUpdateInventoryProductsPricesRequestDataType inherits HashDataType {
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
                "short_desc": "Per-product price updates (max 1000 products)",
                "type": AbstractDataProviderType::get(AutoHashType),
                "required": True,
                "desc": "Nested map of the shape "
                    "`{\"<product_id>\": {\"<price_group_id>\": <price>}}`. "
                    "Up to 1000 products per call.",
            },
        };
    }
    constructor() : HashDataType("BaseLinkerUpdateInventoryProductsPricesRequestDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
