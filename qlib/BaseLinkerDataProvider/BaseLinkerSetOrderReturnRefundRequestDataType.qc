# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerSetOrderReturnRefundRequestDataType.qc setOrderReturnRefund request type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Request type for the BaseLinker `setOrderReturnRefund` method
public class BaseLinkerSetOrderReturnRefundRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "return_id": {
                "display_name": "Return ID",
                "short_desc": "Return being refunded",
                "type": SoftIntType,
                "required": True,
                "ref_data": "order_returns",
            },
            "order_refund_done": {
                "display_name": "Refund Amount",
                "short_desc": "Total amount refunded (return currency)",
                "type": SoftFloatType,
                "required": True,
                "desc": "Cumulative refunded amount in the return's currency. Replaces the "
                    "current refund value (not added to it).",
                "example_value": 49.99,
            },
            "refund_date": {
                "display_name": "Refund Date",
                "short_desc": "Refund timestamp",
                "type": DateType,
                "required": True,
                "desc": "When the refund was issued. Sent to the BaseLinker "
                    "`setOrderReturnRefund` API as Unix seconds (the conversion is performed "
                    "inside `doRequestImpl()`); the field is exposed as a date so the UI "
                    "can render a date/time picker rather than asking the user for a "
                    "raw epoch number.",
            },
            "refund_comment": {
                "display_name": "Refund Comment",
                "short_desc": "Short comment for the refund",
                "type": SoftStringType,
                "required": True,
                "desc": "Short free-text comment attached to the refund (max 50 characters).",
                "example_value": "Refunded via Stripe",
            },
            "external_refund_id": {
                "display_name": "External Refund ID",
                "short_desc": "Identifier assigned by the refund provider",
                "type": SoftStringOrNothingType,
                "example_value": "re_3NqZxYJk2aBcD1e0F2gH3iJ4",
            },
        };
    }
    constructor() : HashDataType("BaseLinkerSetOrderReturnRefundRequestDataType") {
        addQoreFields(Fields);
    }
}
} # namespace BaseLinkerDataProvider
