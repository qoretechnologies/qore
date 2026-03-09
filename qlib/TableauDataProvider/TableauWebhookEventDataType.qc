# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TableauDataProvider module definition

/*  TableauWebhookEventDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the TableauDataProvider module
public namespace TableauDataProvider {

#! Tableau webhook event record type
public class TableauWebhookEventDataType inherits HashDataType {
    public {
        #! Field definitions for Tableau webhook events
        const Fields = {
            "resource": {
                "type": StringOrNothingType,
                "display_name": "Resource",
                "short_desc": "The resource type",
                "desc": "The resource type (WORKBOOK, DATASOURCE, etc.)",
            },
            "event_type": {
                "type": StringOrNothingType,
                "display_name": "Event Type",
                "short_desc": "The webhook event type",
                "desc": "The webhook event type string",
            },
            "resource_name": {
                "type": StringOrNothingType,
                "display_name": "Resource Name",
                "short_desc": "The name of the affected resource",
                "desc": "The name of the affected resource",
            },
            "resource_luid": {
                "type": StringOrNothingType,
                "display_name": "Resource LUID",
                "short_desc": "The LUID of the affected resource",
                "desc": "The LUID of the affected resource",
            },
            "site_luid": {
                "type": StringOrNothingType,
                "display_name": "Site LUID",
                "short_desc": "The LUID of the site",
                "desc": "The LUID of the site",
            },
            "created_at": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the event occurred",
                "desc": "When the event occurred",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace TableauDataProvider
