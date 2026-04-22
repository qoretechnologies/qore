# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerSuccessIdResponseDataType.qc
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Generic success response with a newly assigned identifier
/** BaseLinker's add*-style methods return `{"status": "SUCCESS", "<something>_id": <id>}`.
    This data type covers the common fields across those responses.
*/
public class BaseLinkerSuccessIdResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "status": {
                "display_name": "Status",
                "short_desc": "BaseLinker response status",
                "type": SoftStringType,
                "desc": "Always `SUCCESS` on successful calls.",
            },
            "order_id": {
                "display_name": "Order ID",
                "short_desc": "Newly assigned order ID",
                "type": SoftIntOrNothingType,
            },
            "return_id": {
                "display_name": "Return ID",
                "short_desc": "Newly assigned return ID",
                "type": SoftIntOrNothingType,
            },
            "product_id": {
                "display_name": "Product ID",
                "short_desc": "Newly assigned product ID",
                "type": SoftStringOrNothingType,
            },
            "package_id": {
                "display_name": "Package ID",
                "short_desc": "Newly assigned package ID",
                "type": SoftIntOrNothingType,
            },
            "package_number": {
                "display_name": "Package Number",
                "short_desc": "Carrier-assigned tracking/package number",
                "type": SoftStringOrNothingType,
            },
            "counter": {
                "display_name": "Counter",
                "short_desc": "Number of records updated (bulk endpoints)",
                "type": SoftIntOrNothingType,
            },
            "warnings": {
                "display_name": "Warnings",
                "short_desc": "Per-record warning messages",
                "type": AbstractDataProviderType::get(AutoHashOrNothingType),
            },
        };
    }
    constructor() : HashDataType("BaseLinkerSuccessIdResponseDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
