# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Katana custom field entry sub-type
public class KatanaCustomFieldEntryDataType inherits HashDataType {
    public {
        const Fields = {
            "field_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Field Name",
                "short_desc": "Custom field name",
                "desc": "Name of the custom field",
            },
            "field_value": {
                "type": SoftStringOrNothingType,
                "display_name": "Field Value",
                "short_desc": "Custom field value",
                "desc": "Value of the custom field",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace KatanaDataProvider
