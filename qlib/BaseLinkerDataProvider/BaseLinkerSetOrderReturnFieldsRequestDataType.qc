# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerSetOrderReturnFieldsRequestDataType.qc setOrderReturnFields request type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Request type for the BaseLinker <tt>setOrderReturnFields</tt> method
/** <tt>setOrderReturnFields</tt> updates mutable header fields only. The API does not
    accept:

    - <tt>status_id</tt> (use <tt>set-order-return-status</tt>)
    - <tt>refunded</tt> / refund account details (use <tt>set-order-return-refund</tt>)
    - <tt>order_id</tt>, <tt>date_add</tt>, <tt>currency</tt>, <tt>custom_source_id</tt>, <tt>reference_number</tt>,
      <tt>products</tt>

    Uses <tt>return_id</tt> (not <tt>order_return_id</tt>) per the API.
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
    constructor() : HashDataType("BaseLinkerSetOrderReturnFieldsRequestDataType") {
        addQoreFields(Fields);
    }
}
} # namespace BaseLinkerDataProvider
