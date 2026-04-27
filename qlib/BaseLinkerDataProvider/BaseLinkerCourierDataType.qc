# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerCourierDataType.qc
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! BaseLinker courier metadata
public class BaseLinkerCourierDataType inherits HashDataType {
    public {
        const Fields = {
            "code": {"display_name": "Code",
                "short_desc": "Courier identifier code", "type": SoftStringType},
            "name": {"display_name": "Name",
                "short_desc": "Courier display name", "type": SoftStringOrNothingType},
        };
    }
    constructor() : HashDataType("BaseLinkerCourierDataType") {
        addQoreFields(Fields);
    }
}
} # namespace BaseLinkerDataProvider
