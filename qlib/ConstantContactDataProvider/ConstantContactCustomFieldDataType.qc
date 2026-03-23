# -*- mode: qore; indent-tabs-mode: nil -*-

/*  ConstantContactCustomFieldDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace ConstantContactDataProvider {

#! Constant Contact custom field data type
public class ConstantContactCustomFieldDataType inherits HashDataType {
    public {
        const Fields = {
            "custom_field_id": {
                "type": StringType,
                "display_name": "Custom Field ID",
                "short_desc": "The unique custom field identifier",
                "desc": "The unique identifier for the custom field (UUID format)",
            },
            "label": {
                "type": StringOrNothingType,
                "display_name": "Label",
                "short_desc": "The display label",
                "desc": "The display label for the custom field",
                "example_value": "Favorite Color",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The system name",
                "desc": "The system name for the custom field",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "The field type",
                "desc": "The data type of the custom field",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "string", "display_name": "String",
                        "desc": "A text string field"},
                    <AllowedValueInfo>{"value": "date", "display_name": "Date",
                        "desc": "A date field"},
                ),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant Contact create custom field request data type
public class ConstantContactCreateCustomFieldRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "label": {
                "type": StringType,
                "display_name": "Label",
                "short_desc": "The display label",
                "desc": "The display label for the custom field",
                "example_value": "Favorite Color",
            },
            "type": {
                "type": StringType,
                "display_name": "Type",
                "short_desc": "The field type",
                "desc": "The data type of the custom field",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "string",
                        "display_name": "String",
                        "desc": "A text string field",
                    },
                    <AllowedValueInfo>{
                        "value": "date",
                        "display_name": "Date",
                        "desc": "A date field",
                    },
                ),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant Contact update custom field request data type
public class ConstantContactUpdateCustomFieldRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "custom_field_id": {
                "type": StringType,
                "display_name": "Custom Field ID",
                "short_desc": "The custom field ID to update",
                "desc": "The unique identifier of the custom field to update",
            },
            "label": {
                "type": StringType,
                "display_name": "Label",
                "short_desc": "The new display label",
                "desc": "The new display label for the custom field",
            },
            "type": {
                "type": StringType,
                "display_name": "Type",
                "short_desc": "The custom field type",
                "desc": "The type of the custom field (e.g., `string`, `date`)",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "string",
                        "desc": "String type custom field",
                    },
                    <AllowedValueInfo>{
                        "value": "date",
                        "desc": "Date type custom field",
                    },
                ),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ConstantContactDataProvider
