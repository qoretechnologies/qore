# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerDeleteCourierPackageRequestDataType.qc deleteCourierPackage request type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Request type for the BaseLinker `deleteCourierPackage` method
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
                "short_desc": "Package to delete",
                "type": SoftStringType,
                "required": True,
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
