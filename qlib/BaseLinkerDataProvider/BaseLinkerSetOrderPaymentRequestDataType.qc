# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerSetOrderPaymentRequestDataType.qc setOrderPayment request type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Request type for the BaseLinker `setOrderPayment` method
public class BaseLinkerSetOrderPaymentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "order_id": {
                "display_name": "Order ID",
                "short_desc": "Order whose payment is being recorded",
                "type": SoftIntType,
                "required": True,
                "ref_data": "orders",
            },
            "payment_done": {
                "display_name": "Amount Paid",
                "short_desc": "Total amount paid (order currency)",
                "type": SoftFloatType,
                "required": True,
                "desc": "Cumulative amount paid so far in the order's currency.",
                "example_value": 99.99,
            },
            "payment_date": {
                "display_name": "Payment Date",
                "short_desc": "Payment timestamp",
                "type": DateType,
                "required": True,
                "desc": "When the payment was received. Sent to the BaseLinker "
                    "`setOrderPayment` API as Unix seconds (the conversion is performed "
                    "inside `doRequestImpl()`); the field is exposed as a date so the UI "
                    "can render a date/time picker rather than asking the user for a "
                    "raw epoch number.",
            },
            "payment_comment": {
                "display_name": "Payment Comment",
                "short_desc": "Free-text payment note",
                "type": SoftStringType,
                "required": True,
                "example_value": "Paid via Stripe",
            },
            "external_payment_id": {
                "display_name": "External Payment ID",
                "short_desc": "Identifier assigned by the payment gateway",
                "type": SoftStringOrNothingType,
                "example_value": "ch_3NqZxYJk2aBcD1e0F2gH3iJ4",
            },
        };
    }
    constructor() : HashDataType("BaseLinkerSetOrderPaymentRequestDataType") {
        addQoreFields(Fields);
    }
}
} # namespace BaseLinkerDataProvider
