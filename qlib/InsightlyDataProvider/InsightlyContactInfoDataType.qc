# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InsightlyDataProvider module definition

/*  InsightlyContactInfoDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Insightly contact info data type (phone, email, website, social)
public class InsightlyContactInfoDataType inherits HashDataType {
    public {
        #! Field definitions for Insightly contact info entries
        const Fields = {
            "TYPE": {
                "type": SoftStringOrNothingType,
                "display_name": "Type",
                "short_desc": "Contact info type",
                "desc": "The type of contact information",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Phone", "display_name": "Phone",
                        "desc": "Phone number"},
                    <AllowedValueInfo>{"value": "Email", "display_name": "Email",
                        "desc": "Email address"},
                    <AllowedValueInfo>{"value": "Website", "display_name": "Website",
                        "desc": "Website URL"},
                    <AllowedValueInfo>{"value": "Social", "display_name": "Social",
                        "desc": "Social media profile"},
                ),
            },
            "SUBTYPE": {
                "type": SoftStringOrNothingType,
                "display_name": "Subtype",
                "short_desc": "Contact info subtype",
                "desc": "The subtype of the contact information (e.g., `Work`, `Home`, `Mobile`)",
            },
            "LABEL": {
                "type": SoftStringOrNothingType,
                "display_name": "Label",
                "short_desc": "Label for this entry",
                "desc": "A custom label for this contact information entry",
            },
            "DETAIL": {
                "type": SoftStringOrNothingType,
                "display_name": "Detail",
                "short_desc": "The contact detail value",
                "desc": "The actual value (phone number, email address, URL, etc.)",
                "example_value": "+1-555-0100",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("InsightlyContactInfoDataType") {
        addQoreFields(Fields);
    }
}

} # namespace InsightlyDataProvider
