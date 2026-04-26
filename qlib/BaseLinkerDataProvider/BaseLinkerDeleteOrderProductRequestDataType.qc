# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerDeleteOrderProductRequestDataType.qc deleteOrderProduct request type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Request type for the BaseLinker `deleteOrderProduct` method
public class BaseLinkerDeleteOrderProductRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "order_id": {
                "display_name": "Order ID",
                "short_desc": "Order containing the line item",
                "type": SoftIntType,
                "required": True,
                "ref_data": "orders",
            },
            "order_product_id": {
                "display_name": "Order Product ID",
                "short_desc": "Line-item identifier to delete",
                "type": SoftIntType,
                "required": True,
                "ref_data": "order_products",
                "depends_on": ("order_id",),
            },
        };
    }
    constructor() : HashDataType("BaseLinkerDeleteOrderProductRequestDataType") {
        addQoreFields(Fields);
    }
}
} # namespace BaseLinkerDataProvider
