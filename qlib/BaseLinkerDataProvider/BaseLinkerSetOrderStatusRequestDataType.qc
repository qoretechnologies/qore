# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerSetOrderStatusRequestDataType.qc setOrderStatus request type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Request type for the BaseLinker `setOrderStatus` method
public class BaseLinkerSetOrderStatusRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "order_id": {
                "display_name": "Order ID",
                "short_desc": "Order to transition",
                "type": SoftIntType,
                "required": True,
                "ref_data": "orders",
            },
            "status_id": {
                "display_name": "Status ID",
                "short_desc": "Target order status",
                "type": SoftIntType,
                "required": True,
                "desc": "New order status identifier; see `list-order-statuses`.",
                "ref_data": "order_statuses",
            },
        };
    }
    constructor() : HashDataType("BaseLinkerSetOrderStatusRequestDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
