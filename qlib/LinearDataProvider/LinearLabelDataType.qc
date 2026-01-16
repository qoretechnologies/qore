# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearLabelDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the LinearDataProvider module
public namespace LinearDataProvider {

#! Linear label record type
public class LinearLabelDataType inherits HashDataType {
    private {
        #! Field definitions for Linear labels
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The unique identifier of the label",
                "desc": "The unique identifier of the label",
                "example_value": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The label name",
                "desc": "The name of the label",
                "example_value": "bug",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The label description",
                "desc": "The description of the label",
                "example_value": "Issues related to bugs and defects",
            },
            "color": {
                "type": StringOrNothingType,
                "display_name": "Color",
                "short_desc": "The label color",
                "desc": "The color of the label in hex format",
                "example_value": "#FF5733",
            },
            "team": {
                "type": AutoHashOrNothingType,
                "display_name": "Team",
                "short_desc": "The team",
                "desc": "The team the label belongs to (null for workspace labels)",
            },
            "createdAt": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the label was created",
                "desc": "The date and time when the label was created",
            },
            "updatedAt": {
                "type": DateOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the label was last updated",
                "desc": "The date and time when the label was last updated",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace LinearDataProvider
