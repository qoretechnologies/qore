# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerAddOrderProductRequestDataType.qc addOrderProduct request type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Request type for the BaseLinker `addOrderProduct` method
/** Combines the target `order_id` with the shared
    @ref BaseLinkerOrderProductDataType line-item fields.
*/
public class BaseLinkerAddOrderProductRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "order_id": {
                "display_name": "Order ID",
                "short_desc": "Order to append the product to",
                "type": SoftIntType,
                "required": True,
                "ref_data": "orders",
            },
        } + BaseLinkerOrderProductDataType::Fields;
    }
    constructor() : HashDataType("BaseLinkerAddOrderProductRequestDataType") {
        addQoreFields(Fields);
    }
}
} # namespace BaseLinkerDataProvider
