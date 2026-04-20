# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerProductTextFieldsDataType.qc multilang text-fields map sub-type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! Dynamic-key map of multilang product text fields
/** BaseLinker's inventory product text fields use compound keys of the form
    `<field_name>|<lang>|<source_id>` (where `|<lang>` and `|<source_id>` are
    optional). Values are strings, except `features` whose value is a hash of
    `{param_name: param_value}`.

    Example keys: `name`, `name|en`, `description|de|0`, `features`.
*/
public class BaseLinkerProductTextFieldsDataType inherits HashDataType {
    public {
        const Fields = {};
    }
    constructor() : HashDataType("BaseLinkerProductTextFieldsDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
