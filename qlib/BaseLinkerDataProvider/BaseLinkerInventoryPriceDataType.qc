# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerInventoryPriceDataType.qc
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! A single inventory-prices row (getInventoryProductsPrices result entry)
public class BaseLinkerInventoryPriceDataType inherits HashDataType {
    public {
        const Fields = {
            "product_id": {"display_name": "Product ID",
                "short_desc": "Inventory product identifier", "type": SoftStringType},
            "variant_id": {"display_name": "Variant ID",
                "short_desc": "Variant identifier", "type": SoftStringOrNothingType},
            "prices": {
                "display_name": "Prices",
                "short_desc": "Map of price-group-id to gross price",
                "type": AbstractDataProviderType::get(AutoHashOrNothingType),
            },
        };
    }
    constructor() : HashDataType("BaseLinkerInventoryPriceDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
