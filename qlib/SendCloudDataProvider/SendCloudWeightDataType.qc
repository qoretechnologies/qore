# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudWeightDataType.qc SendCloud weight data type

/*  SendCloudWeightDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SendCloudDataProvider module
public namespace SendCloudDataProvider {

#! Package weight type
public class SendCloudWeightDataType inherits HashDataType {
    public {
        const Fields = {
            "value": {
                "type": SoftStringType,
                "display_name": "Weight",
                "short_desc": "Numeric weight value",
                "desc": "The weight of the package as a string-encoded decimal number",
                "required": True,
                "example_value": "1.500",
            },
            "unit": {
                "type": SoftStringType,
                "display_name": "Unit",
                "short_desc": "Weight unit",
                "desc": "Unit of measurement for the weight value",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "kg", "display_name": "Kilograms", "desc": "Kilograms"},
                    <AllowedValueInfo>{"value": "g", "display_name": "Grams", "desc": "Grams"},
                    <AllowedValueInfo>{"value": "lb", "display_name": "Pounds", "desc": "Pounds"},
                    <AllowedValueInfo>{"value": "oz", "display_name": "Ounces", "desc": "Ounces"},
                ),
            },
        };
    }

    constructor() : HashDataType("SendCloudWeightDataType") {
        addQoreFields(Fields);
    }
}

} # namespace SendCloudDataProvider
