# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CloseDataProvider module definition

/*  CloseSmsDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CloseDataProvider module
public namespace CloseDataProvider {

#! Close SMS activity record type
public class CloseSmsDataType inherits HashDataType {
    public {
        #! Field definitions for Close SMS
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "SMS ID",
                "short_desc": "The SMS ID",
                "desc": "The unique identifier of the SMS activity",
            },
            "lead_id": {
                "type": StringOrNothingType,
                "display_name": "Lead ID",
                "short_desc": "The lead ID",
                "desc": "The unique identifier of the lead associated with this SMS",
            },
            "user_id": {
                "type": StringOrNothingType,
                "display_name": "User ID",
                "short_desc": "The user ID",
                "desc": "The unique identifier of the user associated with this SMS",
            },
            "contact_id": {
                "type": StringOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The contact ID",
                "desc": "The unique identifier of the contact associated with this SMS",
            },
            "local_phone": {
                "type": StringOrNothingType,
                "display_name": "Local Phone",
                "short_desc": "Local phone number",
                "desc": "The local phone number used for the SMS",
            },
            "remote_phone": {
                "type": StringOrNothingType,
                "display_name": "Remote Phone",
                "short_desc": "Remote phone number",
                "desc": "The remote phone number for the SMS",
            },
            "direction": {
                "type": StringOrNothingType,
                "display_name": "Direction",
                "short_desc": "SMS direction",
                "desc": "The direction of the SMS (`inbound` or `outbound`)",
            },
            "text": {
                "type": StringOrNothingType,
                "display_name": "Text",
                "short_desc": "SMS message text",
                "desc": "SMS message text",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "SMS status",
                "desc": "The current status of the SMS",
            },
            "attachments": {
                "type": AutoListOrNothingType,
                "display_name": "Attachments",
                "short_desc": "SMS attachments",
                "desc": "A list of attachment objects associated with this SMS",
            },
            "date_created": {
                "type": StringOrNothingType,
                "display_name": "Date Created",
                "short_desc": "When the SMS was created",
                "desc": "The timestamp when the SMS was created (ISO 8601 format)",
            },
            "date_updated": {
                "type": StringOrNothingType,
                "display_name": "Date Updated",
                "short_desc": "When the SMS was last updated",
                "desc": "The timestamp when the SMS was last updated (ISO 8601 format)",
            },
            "organization_id": {
                "type": StringOrNothingType,
                "display_name": "Organization ID",
                "short_desc": "The organization ID",
                "desc": "The unique identifier of the organization this SMS belongs to",
            },
            "created_by": {
                "type": StringOrNothingType,
                "display_name": "Created By",
                "short_desc": "The user who created the SMS",
                "desc": "The unique identifier of the user who created this SMS",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace CloseDataProvider
