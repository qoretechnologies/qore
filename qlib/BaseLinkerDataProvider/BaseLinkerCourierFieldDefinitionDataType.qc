# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerCourierFieldDefinitionDataType.qc courier-field definition record
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Definition of a courier-specific field (from getCourierFields)
/** Describes a field that `createPackage` accepts for a given courier. Use
    this together with @ref BaseLinkerCourierFieldDataType to supply values.
*/
public class BaseLinkerCourierFieldDefinitionDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Field ID",
                "short_desc": "Field identifier",
                "type": SoftStringType,
            },
            "name": {
                "display_name": "Field Name",
                "short_desc": "Human-readable field label",
                "type": SoftStringOrNothingType,
            },
            "type": {
                "display_name": "Field Type",
                "short_desc": "Field data type",
                "type": SoftStringOrNothingType,
                "desc": "Data type of the field, e.g. `text`, `select`, `checkbox`.",
            },
            "options": {
                "display_name": "Field Options",
                "short_desc": "Allowed option values (for select fields)",
                "type": AbstractDataProviderType::get(AutoHashOrNothingType),
                "desc": "For `select` fields, a hash of allowed values keyed by option ID.",
            },
            "default": {
                "display_name": "Default Value",
                "short_desc": "Default value",
                "type": SoftStringOrNothingType,
            },
        };
    }
    constructor() : HashDataType("BaseLinkerCourierFieldDefinitionDataType") {
        addQoreFields(Fields);
    }
}
} # namespace BaseLinkerDataProvider
