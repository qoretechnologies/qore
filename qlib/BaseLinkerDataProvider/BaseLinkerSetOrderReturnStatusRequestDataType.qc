# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerSetOrderReturnStatusRequestDataType.qc setOrderReturnStatus request type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Request type for the BaseLinker `setOrderReturnStatus` method
public class BaseLinkerSetOrderReturnStatusRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "order_return_id": {
                "display_name": "Order Return ID",
                "short_desc": "Return to transition",
                "type": SoftIntType,
                "required": True,
                "ref_data": "order_returns",
            },
            "status_id": {
                "display_name": "Return Status ID",
                "short_desc": "Target return status",
                "type": SoftIntType,
                "required": True,
                "desc": "Target return status identifier; see `list-order-return-statuses`.",
                "ref_data": "order_return_statuses",
            },
        };
    }
    constructor() : HashDataType("BaseLinkerSetOrderReturnStatusRequestDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
