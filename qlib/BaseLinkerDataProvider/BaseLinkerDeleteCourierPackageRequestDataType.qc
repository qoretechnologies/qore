# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerDeleteCourierPackageRequestDataType.qc deleteCourierPackage request type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Request type for the BaseLinker `deleteCourierPackage` method
/** Exactly one of `package_id` or `package_number` must be supplied.
*/
public class BaseLinkerDeleteCourierPackageRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "courier_code": {
                "display_name": "Courier Code",
                "short_desc": "Courier that owns the package",
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
            "force_delete": {
                "display_name": "Force Delete",
                "short_desc": "Delete locally even if the carrier call fails",
                "type": SoftBoolOrNothingType,
                "desc": "When `True`, removes the package from BaseLinker even if the carrier "
                    "refuses the cancellation.",
            },
        };
    }
    constructor() : HashDataType("BaseLinkerDeleteCourierPackageRequestDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
