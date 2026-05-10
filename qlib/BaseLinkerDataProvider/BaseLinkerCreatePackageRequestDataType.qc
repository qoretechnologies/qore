# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerCreatePackageRequestDataType.qc createPackage request type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Request type for the BaseLinker `createPackage` method
public class BaseLinkerCreatePackageRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "order_id": {
                "display_name": "Order ID",
                "short_desc": "Order to ship",
                "type": SoftIntType,
                "required": True,
                "ref_data": "orders",
            },
            "courier_code": {
                "display_name": "Courier Code",
                "short_desc": "Courier to use",
                "type": SoftStringType,
                "required": True,
                "desc": "Courier identifier code; see `list-couriers`.",
                "ref_data": "couriers",
            },
            "account_id": {
                "display_name": "Account ID",
                "short_desc": "Courier account to charge",
                "type": SoftIntOrNothingType,
                "desc": "Courier account identifier; see `list-courier-accounts`.",
                "ref_data": "courier_accounts",
                "depends_on": ("courier_code",),
            },
            "fields": {
                "display_name": "Courier Fields",
                "short_desc": "Courier-specific field values",
                "type": new ListDataType("fields", new BaseLinkerCourierFieldDataType(), True),
                "desc": "Courier-specific field values; see `list-courier-fields` for definitions. "
                    "Omit or pass an empty list (`[]`) if the selected courier has no form fields.",
            },
            "packages": {
                "display_name": "Packages",
                "short_desc": "Parcel dimensions / insurance",
                "type": new ListDataType("packages", new BaseLinkerPackageDimensionsDataType()),
                "required": True,
                "desc": "One entry per physical parcel being shipped.",
            },
        };
    }
    constructor() : HashDataType("BaseLinkerCreatePackageRequestDataType") {
        addQoreFields(Fields);
    }
}
} # namespace BaseLinkerDataProvider
