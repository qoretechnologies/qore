# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerSetOrderReturnFieldsRequestDataType.qc setOrderReturnFields request type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Request type for the BaseLinker `setOrderReturnFields` method
/** `products` cannot be edited via this method.
*/
public class BaseLinkerSetOrderReturnFieldsRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "order_return_id": {
                "display_name": "Order Return ID",
                "short_desc": "Return to update",
                "type": SoftIntType,
                "required": True,
                "ref_data": "order_returns",
            },
        } + BaseLinkerDataProviderBase::makeFieldsOptional(
            BaseLinkerAddOrderReturnRequestDataType::Fields - "products");
    }
    constructor() : HashDataType("BaseLinkerSetOrderReturnFieldsRequestDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
