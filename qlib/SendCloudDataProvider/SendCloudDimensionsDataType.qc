# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudDimensionsDataType.qc SendCloud dimensions data type

/*  SendCloudDimensionsDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Package dimensions type
public class SendCloudDimensionsDataType inherits HashDataType {
    public {
        const Fields = {
            "length": {
                "type": SoftStringType,
                "display_name": "Length",
                "short_desc": "Package length",
                "desc": "Length of the package (string-encoded decimal)",
                "required": True,
                "example_value": "20.0",
            },
            "width": {
                "type": SoftStringType,
                "display_name": "Width",
                "short_desc": "Package width",
                "desc": "Width of the package (string-encoded decimal)",
                "required": True,
                "example_value": "15.0",
            },
            "height": {
                "type": SoftStringType,
                "display_name": "Height",
                "short_desc": "Package height",
                "desc": "Height of the package (string-encoded decimal)",
                "required": True,
                "example_value": "10.0",
            },
            "unit": {
                "type": SoftStringType,
                "display_name": "Unit",
                "short_desc": "Distance unit",
                "desc": "Unit of measurement for length, width, and height",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "cm", "display_name": "Centimeters", "desc": "Centimeters"},
                    <AllowedValueInfo>{"value": "m", "display_name": "Meters", "desc": "Meters"},
                    <AllowedValueInfo>{"value": "mm", "display_name": "Millimeters", "desc": "Millimeters"},
                    <AllowedValueInfo>{"value": "in", "display_name": "Inches", "desc": "Inches"},
                ),
            },
        };
    }

    constructor() : HashDataType("SendCloudDimensionsDataType") {
        addQoreFields(Fields);
    }
}

} # namespace SendCloudDataProvider
