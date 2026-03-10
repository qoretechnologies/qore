# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CloseDataProvider module definition

/*  CloseCallDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Close call activity record type
public class CloseCallDataType inherits HashDataType {
    public {
        #! Field definitions for Close calls
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Call ID",
                "short_desc": "The call ID",
                "desc": "The unique identifier of the call activity",
            },
            "lead_id": {
                "type": StringOrNothingType,
                "display_name": "Lead ID",
                "short_desc": "The lead ID",
                "desc": "The unique identifier of the lead associated with this call",
            },
            "user_id": {
                "type": StringOrNothingType,
                "display_name": "User ID",
                "short_desc": "The user ID",
                "desc": "The unique identifier of the user who made or received the call",
            },
            "contact_id": {
                "type": StringOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The contact ID",
                "desc": "The unique identifier of the contact associated with this call",
            },
            "direction": {
                "type": StringOrNothingType,
                "display_name": "Direction",
                "short_desc": "Call direction",
                "desc": "Call direction (`inbound` or `outbound`)",
            },
            "duration": {
                "type": IntOrNothingType,
                "display_name": "Duration",
                "short_desc": "Call duration in seconds",
                "desc": "Call duration in seconds",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone Number",
                "short_desc": "The phone number",
                "desc": "The phone number associated with this call",
            },
            "disposition": {
                "type": StringOrNothingType,
                "display_name": "Disposition",
                "short_desc": "Call disposition",
                "desc": "The disposition or outcome of the call",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Call status",
                "desc": "The current status of the call",
            },
            "note": {
                "type": StringOrNothingType,
                "display_name": "Note",
                "short_desc": "Call note",
                "desc": "A text note associated with this call",
            },
            "note_html": {
                "type": StringOrNothingType,
                "display_name": "Note HTML",
                "short_desc": "Call note in HTML",
                "desc": "The call note in HTML format",
            },
            "recording_url": {
                "type": StringOrNothingType,
                "display_name": "Recording URL",
                "short_desc": "Call recording URL",
                "desc": "The URL of the call recording, if available",
            },
            "voicemail_url": {
                "type": StringOrNothingType,
                "display_name": "Voicemail URL",
                "short_desc": "Voicemail URL",
                "desc": "The URL of the voicemail, if available",
            },
            "cost": {
                "type": FloatOrNothingType,
                "display_name": "Cost",
                "short_desc": "Call cost",
                "desc": "The cost of the call",
            },
            "call_method": {
                "type": StringOrNothingType,
                "display_name": "Call Method",
                "short_desc": "Call method",
                "desc": "The method used for the call (e.g., regular, power dialer)",
            },
            "date_created": {
                "type": StringOrNothingType,
                "display_name": "Date Created",
                "short_desc": "When the call was created",
                "desc": "The timestamp when the call was created (ISO 8601 format)",
            },
            "date_updated": {
                "type": StringOrNothingType,
                "display_name": "Date Updated",
                "short_desc": "When the call was last updated",
                "desc": "The timestamp when the call was last updated (ISO 8601 format)",
            },
            "organization_id": {
                "type": StringOrNothingType,
                "display_name": "Organization ID",
                "short_desc": "The organization ID",
                "desc": "The unique identifier of the organization this call belongs to",
            },
            "created_by": {
                "type": StringOrNothingType,
                "display_name": "Created By",
                "short_desc": "The user who created the call",
                "desc": "The unique identifier of the user who created this call",
            },
            "updated_by": {
                "type": StringOrNothingType,
                "display_name": "Updated By",
                "short_desc": "The user who last updated the call",
                "desc": "The unique identifier of the user who last updated this call",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace CloseDataProvider
