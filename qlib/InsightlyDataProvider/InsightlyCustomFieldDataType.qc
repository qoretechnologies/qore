# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InsightlyDataProvider module definition

/*  InsightlyCustomFieldDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#! Contains all public definitions in the InsightlyDataProvider module
public namespace InsightlyDataProvider {

#! Insightly custom field data type
public class InsightlyCustomFieldDataType inherits HashDataType {
    public {
        #! Field definitions for Insightly custom fields
        const Fields = {
            "FIELD_NAME": {
                "type": SoftStringType,
                "display_name": "Field Name",
                "required": True,
                "short_desc": "The custom field name",
                "desc": "The name of the custom field (e.g., `MyCustomField__c`)",
            },
            "CUSTOM_FIELD_ID": {
                "type": SoftStringOrNothingType,
                "display_name": "Custom Field ID",
                "short_desc": "The custom field identifier",
                "desc": "The system-generated identifier for the custom field",
            },
            "FIELD_VALUE": {
                "type": AbstractDataProviderType::get(AutoType),
                "display_name": "Field Value",
                "short_desc": "The custom field value",
                "desc": "The value of the custom field. Can be a string, number, boolean, or "
                    "date depending on the field configuration",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("InsightlyCustomFieldDataType") {
        addQoreFields(Fields);
    }
}

} # namespace InsightlyDataProvider
