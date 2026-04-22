# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerSetOrderReturnFieldsRequestDataType.qc setOrderReturnFields request type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Request type for the BaseLinker `setOrderReturnFields` method
/** `setOrderReturnFields` updates mutable header fields only. The API does not
    accept:

    - `status_id` (use `set-order-return-status`)
    - `refunded` / refund account details (use `set-order-return-refund`)
    - `order_id`, `date_add`, `currency`, `custom_source_id`, `reference_number`,
      `products`

    Uses `return_id` (not `order_return_id`) per the API.
*/
public class BaseLinkerSetOrderReturnFieldsRequestDataType inherits HashDataType {
    public {
        const ExcludedAddReturnFields = (
            "order_id", "status_id", "custom_source_id", "reference_number",
            "date_add", "currency", "refunded",
            "refund_account_number", "refund_iban", "refund_swift",
            "products",
        );

        const Fields = {
            "return_id": {
                "display_name": "Return ID",
                "short_desc": "Return to update",
                "type": SoftIntType,
                "required": True,
                "ref_data": "order_returns",
            },
        } + BaseLinkerDataProviderBase::makeFieldsOptional(
            BaseLinkerAddOrderReturnRequestDataType::Fields - ExcludedAddReturnFields);
    }
    constructor() : HashDataType("BaseLinkerSetOrderReturnFieldsRequestDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
