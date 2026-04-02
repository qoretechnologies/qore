# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SegmentDataProvider module definition

/*  SegmentTrackingPlanDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SegmentDataProvider module
public namespace SegmentDataProvider {

#! Segment tracking plan record type
public class SegmentTrackingPlanDataType inherits HashDataType {
    public {
        #! Field definitions for Segment tracking plans
        const Fields = {
            "id": {
                "type": StringOrNothingType,
                "display_name": "ID",
                "short_desc": "Tracking plan ID",
                "desc": "The unique identifier for the tracking plan",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Tracking plan name",
                "desc": "The name of the tracking plan",
            },
            "slug": {
                "type": StringOrNothingType,
                "display_name": "Slug",
                "short_desc": "Tracking plan slug",
                "desc": "The URL-friendly slug of the tracking plan",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Description",
                "desc": "The description of the tracking plan",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "Tracking plan type",
                "desc": "The type of tracking plan: `LIVE`, `PROPERTY_LIBRARY`, `RULE_LIBRARY`, or `TEMPLATE`",
            },
            "updatedAt": {
                "type": DateOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "The timestamp of the last update",
            },
            "createdAt": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "The timestamp when the tracking plan was created",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SegmentDataProvider
