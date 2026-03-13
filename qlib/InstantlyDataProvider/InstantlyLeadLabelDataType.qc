# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyLeadLabelDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Instantly lead label data type
public class InstantlyLeadLabelDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Label ID",
                "short_desc": "Label UUID",
                "type": StringOrNothingType,
                "desc": "The unique identifier (UUID) of the label",
            },
            "label": {
                "display_name": "Label",
                "short_desc": "Label name",
                "type": StringType,
                "desc": "The name of the label",
                "required": True,
            },
            "interest_status_label": {
                "display_name": "Interest Status Label",
                "short_desc": "Interest status",
                "type": StringOrNothingType,
                "desc": "The interest status associated with this label (e.g., `positive`, "
                    "`negative`, `neutral`)",
            },
            "interest_status": {
                "display_name": "Interest Status",
                "short_desc": "Interest status number",
                "type": IntOrNothingType,
                "desc": "The numeric interest status value",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Label description",
                "type": StringOrNothingType,
                "desc": "The description of the label",
            },
            "use_with_ai": {
                "display_name": "Use With AI",
                "short_desc": "Use with AI flag",
                "type": BoolOrNothingType,
                "desc": "Whether this label should be used with AI",
            },
            "timestamp_created": {
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "type": DateOrNothingType,
                "desc": "The date and time the label was created",
            },
            "created_by": {
                "display_name": "Created By",
                "short_desc": "Creator UUID",
                "type": StringOrNothingType,
                "desc": "The UUID of the user who created the label",
            },
            "organization_id": {
                "display_name": "Organization ID",
                "short_desc": "Organization UUID",
                "type": StringOrNothingType,
                "desc": "The organization ID this label belongs to",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace InstantlyDataProvider
