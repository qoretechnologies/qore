# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerCourierAccountDataType.qc courier-account record
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! A courier account configured in BaseLinker (from getCourierAccounts)
public class BaseLinkerCourierAccountDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Account ID",
                "short_desc": "Courier account identifier",
                "type": SoftIntType,
            },
            "name": {
                "display_name": "Account Name",
                "short_desc": "Display name",
                "type": SoftStringOrNothingType,
            },
            "courier_code": {
                "display_name": "Courier Code",
                "short_desc": "Courier this account belongs to",
                "type": SoftStringOrNothingType,
            },
        };
    }
    constructor() : HashDataType("BaseLinkerCourierAccountDataType") {
        addQoreFields(Fields);
    }
}
} # namespace BaseLinkerDataProvider
