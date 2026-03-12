# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyLeadListDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Instantly lead list data type
public class InstantlyLeadListDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Lead List ID",
                "short_desc": "Lead list UUID",
                "type": StringOrNothingType,
                "desc": "The unique identifier (UUID) of the lead list",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "List name",
                "type": StringType,
                "desc": "The name of the lead list",
                "required": True,
            },
            "organization_id": {
                "display_name": "Organization ID",
                "short_desc": "Organization UUID",
                "type": StringOrNothingType,
                "desc": "The UUID of the organization this lead list belongs to",
            },
            "has_enrichment_task": {
                "display_name": "Has Enrichment Task",
                "short_desc": "Auto-enrichment enabled",
                "type": BoolOrNothingType,
                "desc": "Whether auto-enrichment is enabled for this lead list",
            },
            "created_at": {
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "type": DateOrNothingType,
                "desc": "The date and time the lead list was created",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace InstantlyDataProvider
