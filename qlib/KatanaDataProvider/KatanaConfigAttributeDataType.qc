# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Katana variant config attribute sub-type
public class KatanaConfigAttributeDataType inherits HashDataType {
    public {
        const Fields = {
            "config_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Config Name",
                "short_desc": "Configuration attribute name",
                "desc": "Name of the configuration attribute (e.g. `Color`)",
            },
            "config_value": {
                "type": SoftStringOrNothingType,
                "display_name": "Config Value",
                "short_desc": "Configuration attribute value",
                "desc": "Value of the configuration attribute (e.g. `Red`)",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace KatanaDataProvider
