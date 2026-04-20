# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerProductStockMapDataType.qc warehouse → stock map sub-type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Dynamic-key map of warehouse IDs to stock quantities
/** BaseLinker inventory products carry per-warehouse stock keyed by warehouse
    identifier (e.g. `bl_123`, `shop_456`). The keys are dynamic so this wraps
    an auto-hash of `{"<warehouse_id>": <qty>}`.
*/
public class BaseLinkerProductStockMapDataType inherits HashDataType {
    public {
        const Fields = {};
    }
    constructor() : HashDataType("BaseLinkerProductStockMapDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
