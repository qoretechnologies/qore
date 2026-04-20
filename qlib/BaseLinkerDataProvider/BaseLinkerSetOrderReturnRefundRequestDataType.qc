# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerSetOrderReturnRefundRequestDataType.qc setOrderReturnRefund request type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Request type for the BaseLinker `setOrderReturnRefund` method
public class BaseLinkerSetOrderReturnRefundRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "order_return_id": {
                "display_name": "Order Return ID",
                "short_desc": "Return being refunded",
                "type": SoftIntType,
                "required": True,
                "ref_data": "order_returns",
            },
            "refund_done": {
                "display_name": "Refund Amount",
                "short_desc": "Total amount refunded (return currency)",
                "type": SoftFloatType,
                "required": True,
                "desc": "Cumulative refunded amount in the return's currency.",
            },
            "external_refund_id": {
                "display_name": "External Refund ID",
                "short_desc": "Identifier assigned by the refund provider",
                "type": SoftStringOrNothingType,
            },
        };
    }
    constructor() : HashDataType("BaseLinkerSetOrderReturnRefundRequestDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
