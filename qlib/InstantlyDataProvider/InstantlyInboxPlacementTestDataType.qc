# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyInboxPlacementTestDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the InstantlyDataProvider module
public namespace InstantlyDataProvider {

#! Instantly inbox placement test data type
public class InstantlyInboxPlacementTestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Test ID",
                "short_desc": "Test UUID",
                "type": StringOrNothingType,
                "desc": "The unique identifier (UUID) of the inbox placement test",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Test name",
                "type": StringType,
                "desc": "The name of the inbox placement test",
                "required": True,
            },
            "type": {
                "display_name": "Type",
                "short_desc": "Test type",
                "type": IntOrNothingType,
                "desc": "The test type: `1` = One-time, `2` = Automated",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 1, "display_name": "One-time"},
                    <AllowedValueInfo>{"value": 2, "display_name": "Automated"},
                ),
            },
            "sending_method": {
                "display_name": "Sending Method",
                "short_desc": "Sending method",
                "type": IntOrNothingType,
                "desc": "The sending method: `1` = From Instantly, `2` = From Outside",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 1, "display_name": "From Instantly"},
                    <AllowedValueInfo>{"value": 2, "display_name": "From Outside"},
                ),
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Test status",
                "type": IntOrNothingType,
                "desc": "The test status: `1` = Active, `2` = Paused, `3` = Completed",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 1, "display_name": "Active"},
                    <AllowedValueInfo>{"value": 2, "display_name": "Paused"},
                    <AllowedValueInfo>{"value": 3, "display_name": "Completed"},
                ),
            },
            "created_at": {
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "type": DateOrNothingType,
                "desc": "The date and time the test was created",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace InstantlyDataProvider
