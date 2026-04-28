# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerCourierPackageDataType.qc
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! BaseLinker courier package (shipment) record
public class BaseLinkerCourierPackageDataType inherits HashDataType {
    public {
        const Fields = {
            "package_id": {"display_name": "Package ID",
                "short_desc": "Internal package identifier", "type": SoftIntType},
            "order_id": {"display_name": "Order ID",
                "short_desc": "Order this package belongs to", "type": SoftIntOrNothingType},
            "courier_code": {"display_name": "Courier Code",
                "short_desc": "Courier identifier", "type": SoftStringOrNothingType},
            "account_id": {"display_name": "Courier Account ID",
                "short_desc": "Courier-account identifier", "type": SoftStringOrNothingType},
            "service": {"display_name": "Service",
                "short_desc": "Courier service identifier", "type": SoftStringOrNothingType},
            "package_number": {"display_name": "Package Number",
                "short_desc": "Carrier tracking number", "type": SoftStringOrNothingType},
            "status": {"display_name": "Status",
                "short_desc": "Package status label", "type": SoftStringOrNothingType},
            "tracking_url": {"display_name": "Tracking URL",
                "short_desc": "Tracking page URL", "type": SoftStringOrNothingType},
            "creation_date": {"display_name": "Creation Date",
                "short_desc": "Package creation timestamp (Unix seconds)",
                "type": SoftIntOrNothingType},
        };
    }
    constructor() : HashDataType("BaseLinkerCourierPackageDataType") {
        addQoreFields(Fields);
    }
}
} # namespace BaseLinkerDataProvider
