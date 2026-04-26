# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerDeleteOrdersRequestDataType.qc deleteOrders request type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Request type for deleting BaseLinker orders
public class BaseLinkerDeleteOrdersRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "order_ids": {
                "display_name": "Order IDs",
                "short_desc": "Orders to delete (max 1000 per call)",
                "desc": "Array of BaseLinker order IDs to delete (up to 1000 per call).",
                "type": new ListDataType("order_ids", SoftIntType),
                "required": True,
                "ref_data": "orders",
            },
        };
    }
    constructor() : HashDataType("BaseLinkerDeleteOrdersRequestDataType") {
        addQoreFields(Fields);
    }
}
} # namespace BaseLinkerDataProvider
