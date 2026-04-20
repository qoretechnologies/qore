# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerDeleteOrdersRequestDataType.qc deleteOrders request type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Request type for deleting a single BaseLinker order
public class BaseLinkerDeleteOrdersRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "order_id": {
                "display_name": "Order ID",
                "short_desc": "Order to delete",
                "type": SoftIntType,
                "required": True,
                "ref_data": "orders",
            },
        };
    }
    constructor() : HashDataType("BaseLinkerDeleteOrdersRequestDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
