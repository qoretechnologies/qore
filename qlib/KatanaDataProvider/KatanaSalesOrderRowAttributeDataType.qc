# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Katana sales order row attribute sub-type ({key, value} per CreateSalesOrderRowAttributeDto)
public class KatanaSalesOrderRowAttributeDataType inherits HashDataType {
    public {
        const Fields = {
            "key": {
                "type": SoftStringType,
                "required": True,
                "display_name": "Key",
                "short_desc": "Attribute key",
                "desc": "Attribute key (e.g. `Color`)",
            },
            "value": {
                "type": SoftStringType,
                "required": True,
                "display_name": "Value",
                "short_desc": "Attribute value",
                "desc": "Attribute value (e.g. `Red`)",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace KatanaDataProvider
