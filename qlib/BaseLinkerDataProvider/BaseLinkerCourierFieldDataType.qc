# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerCourierFieldDataType.qc courier field sub-type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! A single courier-specific field entry for createPackage
public class BaseLinkerCourierFieldDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Field ID",
                "short_desc": "Courier field identifier",
                "type": SoftStringType,
                "required": True,
                "desc": "Courier field identifier; see `list-courier-fields` for the field catalog.",
            },
            "value": {
                "display_name": "Field Value",
                "short_desc": "Value to assign to the field",
                "type": SoftStringType,
                "required": True,
            },
        };
    }
    constructor() : HashDataType("BaseLinkerCourierFieldDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
