# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerGetLabelRequestDataType.qc getLabel request type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Request type for the BaseLinker `getLabel` method
/** Exactly one of `package_id` or `package_number` must be supplied.
*/
public class BaseLinkerGetLabelRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "courier_code": {
                "display_name": "Courier Code",
                "short_desc": "Courier issuing the label",
                "type": SoftStringType,
                "required": True,
                "ref_data": "couriers",
            },
            "package_id": {
                "display_name": "Package ID",
                "short_desc": "BaseLinker package ID (alternative to package number)",
                "type": SoftIntOrNothingType,
                "desc": "Internal BaseLinker package ID. Supply either this or `package_number`.",
            },
            "package_number": {
                "display_name": "Package Number",
                "short_desc": "Carrier-assigned tracking number (alternative to package ID)",
                "type": SoftStringOrNothingType,
                "desc": "Carrier-assigned tracking/package number. Supply either this or "
                    "`package_id`.",
            },
        };
    }
    constructor() : HashDataType("BaseLinkerGetLabelRequestDataType") {
        addQoreFields(Fields);
    }
}
} # namespace BaseLinkerDataProvider
