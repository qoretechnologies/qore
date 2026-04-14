# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InsightlyDataProvider module definition

/*  InsightlyEventLinkDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Insightly event link data type
public class InsightlyEventLinkDataType inherits HashDataType {
    public {
        #! Field definitions for Insightly event links
        const Fields = {
            "EVENT_LINK_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Event Link ID",
                "short_desc": "The unique event link identifier",
                "desc": "The unique identifier of the event link",
            },
            "OBJECT_NAME": {
                "type": SoftStringOrNothingType,
                "display_name": "Object Name",
                "short_desc": "The linked object type",
                "desc": "The type name of the linked object",
            },
            "OBJECT_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Object ID",
                "short_desc": "The linked object ID",
                "desc": "The unique identifier of the linked object",
            },
            "ROLE": {
                "type": SoftStringOrNothingType,
                "display_name": "Role",
                "short_desc": "The link role",
                "desc": "The role describing the relationship",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("InsightlyEventLinkDataType") {
        addQoreFields(Fields);
    }
}

} # namespace InsightlyDataProvider
