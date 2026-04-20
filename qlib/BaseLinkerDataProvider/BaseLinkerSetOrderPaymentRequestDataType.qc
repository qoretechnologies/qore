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
            },
            "payment_date": {
                "display_name": "Payment Date",
                "short_desc": "Payment timestamp (Unix seconds)",
                "type": SoftIntOrNothingType,
                "desc": "Payment timestamp as Unix seconds. Defaults to the current time.",
            },
            "payment_comment": {
                "display_name": "Payment Comment",
                "short_desc": "Free-text payment note",
                "type": SoftStringOrNothingType,
            },
            "external_payment_id": {
                "display_name": "External Payment ID",
                "short_desc": "Identifier assigned by the payment gateway",
                "type": SoftStringOrNothingType,
            },
        };
    }
    constructor() : HashDataType("BaseLinkerSetOrderPaymentRequestDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
