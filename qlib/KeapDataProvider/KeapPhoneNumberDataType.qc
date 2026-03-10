# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KeapDataProvider module definition

/*  KeapPhoneNumberDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {

#! Keap phone number type
public class KeapPhoneNumberDataType inherits HashDataType {
    private {
        const Fields = {
            "number": {
                "type": StringType,
                "display_name": "Number",
                "short_desc": "The phone number",
                "desc": "The phone number",
                "required": True,
                "example_value": "+1-555-123-4567",
            },
            "field": {
                "type": StringOrNothingType,
                "display_name": "Field",
                "short_desc": "The phone field slot",
                "desc": "The phone number field slot to use",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "PHONE1", "desc": "Primary phone"},
                    <AllowedValueInfo>{"value": "PHONE2", "desc": "Secondary phone"},
                    <AllowedValueInfo>{"value": "PHONE3", "desc": "Third phone"},
                    <AllowedValueInfo>{"value": "PHONE4", "desc": "Fourth phone"},
                    <AllowedValueInfo>{"value": "PHONE5", "desc": "Fifth phone"},
                ),
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "The phone number type",
                "desc": "The type of phone number (e.g., `Mobile`, `Home`, `Work`)",
            },
            "extension": {
                "type": StringOrNothingType,
                "display_name": "Extension",
                "short_desc": "Phone extension",
                "desc": "The phone number extension",
            },
        };
    }

    constructor() : HashDataType("KeapPhoneNumber") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type, "KeapPhoneNumber") {
        addQoreFields(Fields);
    }
}

} # namespace KeapDataProvider
