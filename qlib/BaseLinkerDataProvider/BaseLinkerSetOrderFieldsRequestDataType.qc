# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerSetOrderFieldsRequestDataType.qc setOrderFields request type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Request type for the BaseLinker `setOrderFields` method
/** `products` cannot be updated through `setOrderFields`; use
    `add-order-product`, `set-order-product-fields` and `delete-order-product`
    to manage line items.
*/
public class BaseLinkerSetOrderFieldsRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "order_id": {
                "display_name": "Order ID",
                "short_desc": "Order to update",
                "type": SoftIntType,
                "required": True,
                "desc": "Identifier of the order to update.",
                "ref_data": "orders",
            },
        } + BaseLinkerDataProviderBase::makeFieldsOptional(
            BaseLinkerAddOrderRequestDataType::Fields - "products");
    }
    constructor() : HashDataType("BaseLinkerSetOrderFieldsRequestDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
