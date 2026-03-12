# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyLeadDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Instantly lead data type
public class InstantlyLeadDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Lead ID",
                "short_desc": "Lead UUID",
                "type": StringOrNothingType,
                "desc": "The unique identifier (UUID) of the lead",
            },
            "email": {
                "display_name": "Email",
                "short_desc": "Lead email address",
                "type": StringType,
                "desc": "The email address of the lead",
                "required": True,
            },
            "first_name": {
                "display_name": "First Name",
                "short_desc": "First name",
                "type": StringOrNothingType,
                "desc": "The first name of the lead",
            },
            "last_name": {
                "display_name": "Last Name",
                "short_desc": "Last name",
                "type": StringOrNothingType,
                "desc": "The last name of the lead",
            },
            "company_name": {
                "display_name": "Company Name",
                "short_desc": "Company name",
                "type": StringOrNothingType,
                "desc": "The company name of the lead",
            },
            "phone": {
                "display_name": "Phone",
                "short_desc": "Phone number",
                "type": StringOrNothingType,
                "desc": "The phone number of the lead",
            },
            "website": {
                "display_name": "Website",
                "short_desc": "Website URL",
                "type": StringOrNothingType,
                "desc": "The website URL of the lead",
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Lead status code",
                "type": IntOrNothingType,
                "desc": "The status of the lead: `1` = Active, `2` = Paused, `3` = Completed, "
                    "`-1` = Bounced, `-2` = Unsubscribed, `-3` = Skipped",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 1, "display_name": "Active"},
                    <AllowedValueInfo>{"value": 2, "display_name": "Paused"},
                    <AllowedValueInfo>{"value": 3, "display_name": "Completed"},
                    <AllowedValueInfo>{"value": -1, "display_name": "Bounced"},
                    <AllowedValueInfo>{"value": -2, "display_name": "Unsubscribed"},
                    <AllowedValueInfo>{"value": -3, "display_name": "Skipped"},
                ),
            },
            "campaign": {
                "display_name": "Campaign",
                "short_desc": "Associated campaign UUID",
                "type": StringOrNothingType,
                "desc": "The UUID of the campaign associated with this lead",
            },
            "list_id": {
                "display_name": "List ID",
                "short_desc": "Lead list UUID",
                "type": StringOrNothingType,
                "desc": "The UUID of the lead list this lead belongs to",
            },
            "email_open_count": {
                "display_name": "Email Open Count",
                "short_desc": "Open count",
                "type": IntOrNothingType,
                "desc": "The number of times emails to this lead have been opened (read-only)",
            },
            "email_reply_count": {
                "display_name": "Email Reply Count",
                "short_desc": "Reply count",
                "type": IntOrNothingType,
                "desc": "The number of email replies from this lead (read-only)",
            },
            "email_click_count": {
                "display_name": "Email Click Count",
                "short_desc": "Click count",
                "type": IntOrNothingType,
                "desc": "The number of email link clicks from this lead (read-only)",
            },
            "created_at": {
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "type": DateOrNothingType,
                "desc": "The date and time the lead was created",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace InstantlyDataProvider
