# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerProductPriceUpdateEntryDataType.qc
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! A single product entry in updateInventoryProductsPrices
public class BaseLinkerProductPriceUpdateEntryDataType inherits HashDataType {
    public {
        const Fields = {
            "product_id": {
                "display_name": "Product ID",
                "short_desc": "BaseLinker inventory product ID",
                "type": SoftStringType,
                "required": True,
            },
            "variant_id": {
                "display_name": "Variant ID",
                "short_desc": "Optional variant identifier",
                "type": SoftStringOrNothingType,
            },
            "prices": {
                "display_name": "Prices Map",
                "short_desc": "Map of price-group-id to price",
                "type": AbstractDataProviderType::get(AutoHashType),
                "required": True,
                "desc": "Hash whose keys are price group identifiers and values are gross prices "
                    "(floats) in the group's currency.",
            },
        };
    }
    constructor() : HashDataType("BaseLinkerProductPriceUpdateEntryDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
