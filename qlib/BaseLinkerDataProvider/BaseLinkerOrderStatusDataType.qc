# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerOrderStatusDataType.qc order-status record
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! An order status (from getOrderStatusList)
public class BaseLinkerOrderStatusDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Status ID",
                "short_desc": "Status identifier",
                "type": SoftIntType,
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Internal status name",
                "type": SoftStringOrNothingType,
            },
            "name_for_customer": {
                "display_name": "Customer-Facing Name",
                "short_desc": "Status name shown to customers",
                "type": SoftStringOrNothingType,
            },
            "color": {
                "display_name": "Color",
                "short_desc": "Status color (hex)",
                "type": SoftStringOrNothingType,
            },
        };
    }
    constructor() : HashDataType("BaseLinkerOrderStatusDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
