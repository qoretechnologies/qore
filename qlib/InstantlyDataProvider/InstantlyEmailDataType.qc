# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyEmailDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Instantly email data type
public class InstantlyEmailDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Email ID",
                "short_desc": "Email UUID",
                "type": StringOrNothingType,
                "desc": "The unique identifier (UUID) of the email",
            },
            "subject": {
                "display_name": "Subject",
                "short_desc": "Subject line",
                "type": StringOrNothingType,
                "desc": "The subject line of the email",
            },
            "body": {
                "display_name": "Body",
                "short_desc": "Email body",
                "type": StringOrNothingType,
                "desc": "The body content of the email",
            },
            "from_address_email": {
                "display_name": "From Address",
                "short_desc": "Sender email",
                "type": StringOrNothingType,
                "desc": "The sender email address",
            },
            "to_address_email_list": {
                "display_name": "To Address List",
                "short_desc": "Recipients (comma-separated)",
                "type": StringOrNothingType,
                "desc": "The recipient email addresses (comma-separated)",
            },
            "ue_type": {
                "display_name": "Email Type",
                "short_desc": "Email type code",
                "type": IntOrNothingType,
                "desc": "The email type: `1` = Campaign, `2` = Received, `3` = Sent, `4` = Scheduled",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 1, "display_name": "Campaign"},
                    <AllowedValueInfo>{"value": 2, "display_name": "Received"},
                    <AllowedValueInfo>{"value": 3, "display_name": "Sent"},
                    <AllowedValueInfo>{"value": 4, "display_name": "Scheduled"},
                ),
            },
            "campaign_id": {
                "display_name": "Campaign ID",
                "short_desc": "Campaign UUID",
                "type": StringOrNothingType,
                "desc": "The UUID of the campaign associated with this email",
            },
            "lead_id": {
                "display_name": "Lead ID",
                "short_desc": "Lead UUID",
                "type": StringOrNothingType,
                "desc": "The UUID of the lead associated with this email",
            },
            "message_id": {
                "display_name": "Message ID",
                "short_desc": "Server message ID",
                "type": StringOrNothingType,
                "desc": "The server-assigned message ID",
            },
            "is_read": {
                "display_name": "Is Read",
                "short_desc": "Read status",
                "type": BoolOrNothingType,
                "desc": "Whether the email has been read",
            },
            "created_at": {
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "type": DateOrNothingType,
                "desc": "The date and time the email was created",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace InstantlyDataProvider
