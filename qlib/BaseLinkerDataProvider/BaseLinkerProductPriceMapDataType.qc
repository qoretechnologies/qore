# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerProductPriceMapDataType.qc price-group → price map sub-type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Dynamic-key map of price-group IDs to prices
/** BaseLinker inventory products carry per-price-group prices keyed by price-group
    identifier. The keys are dynamic (defined by the user in BaseLinker) so this
    wraps an auto-hash of `{"<price_group_id>": <price>}`.
*/
public class BaseLinkerProductPriceMapDataType inherits HashDataType {
    public {
        const Fields = {};
    }
    constructor() : HashDataType("BaseLinkerProductPriceMapDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
