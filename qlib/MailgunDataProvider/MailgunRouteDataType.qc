# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MailgunDataProvider module definition

/*  MailgunRouteDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the MailgunDataProvider module
public namespace MailgunDataProvider {

#! Mailgun route record type
public class MailgunRouteDataType inherits HashDataType {
    private {
        #! Field definitions for Mailgun routes
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Route ID",
                "short_desc": "The unique route identifier",
                "desc": "The unique identifier of the route",
                "example_value": "5e1234567890abcdef123456",
            },
            "priority": {
                "type": IntOrNothingType,
                "display_name": "Priority",
                "short_desc": "Route priority",
                "desc": "The priority of the route (lower number = higher priority)",
                "example_value": 0,
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Route description",
                "desc": "A description of what this route does",
                "example_value": "Forward support emails",
            },
            "expression": {
                "type": StringOrNothingType,
                "display_name": "Expression",
                "short_desc": "Matching expression",
                "desc": "The expression used to match incoming messages",
                "example_value": "match_recipient('support@example.com')",
            },
            "actions": {
                "type": SoftListOrNothingType,
                "display_name": "Actions",
                "short_desc": "Route actions",
                "desc": "The actions to execute when the expression matches",
                "example_value": ("forward('http://example.com/webhook')", "stop()"),
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the route was created",
                "desc": "The date and time when the route was created",
                "example_value": "Tue, 01 Jan 2025 09:00:00 GMT",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace MailgunDataProvider
