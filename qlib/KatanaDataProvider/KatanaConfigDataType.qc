# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Katana product/material config sub-type
public class KatanaConfigDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "type": SoftStringType,
                "display_name": "Name",
                "required": True,
                "short_desc": "Config attribute name",
                "desc": "Name of the configuration attribute (e.g. `Color`, `Size`)",
            },
            "values": {
                "type": new ListDataType("KatanaConfigValuesList", SoftStringType, True),
                "display_name": "Values",
                "short_desc": "Allowed values",
                "desc": "List of allowed values for this configuration attribute",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace KatanaDataProvider
