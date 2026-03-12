# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CloseDataProvider module definition

/*  CloseEmailDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Close email activity record type
public class CloseEmailDataType inherits HashDataType {
    public {
        #! Field definitions for Close emails
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Email ID",
                "short_desc": "The email ID",
                "desc": "The unique identifier of the email activity",
            },
            "lead_id": {
                "type": StringOrNothingType,
                "display_name": "Lead ID",
                "short_desc": "The lead ID",
                "desc": "The unique identifier of the lead associated with this email",
            },
            "user_id": {
                "type": StringOrNothingType,
                "display_name": "User ID",
                "short_desc": "The user ID",
                "desc": "The unique identifier of the user associated with this email",
            },
            "contact_id": {
                "type": StringOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The contact ID",
                "desc": "The unique identifier of the contact associated with this email",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Email status",
                "desc": "Email status: `inbox`, `draft`, `scheduled`, `outbox`, `sent`, or `error`",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Inbox",
                        "short_desc": "Received email",
                        "desc": "Email received in the inbox",
                        "value": "inbox",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Draft",
                        "short_desc": "Draft email",
                        "desc": "Email saved as a draft",
                        "value": "draft",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Scheduled",
                        "short_desc": "Scheduled email",
                        "desc": "Email scheduled for future sending",
                        "value": "scheduled",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Outbox",
                        "short_desc": "Queued for sending",
                        "desc": "Email queued for sending",
                        "value": "outbox",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Sent",
                        "short_desc": "Sent email",
                        "desc": "Email that has been sent",
                        "value": "sent",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Error",
                        "short_desc": "Failed email",
                        "desc": "Email that failed to send",
                        "value": "error",
                    },
                ),
            },
            "direction": {
                "type": StringOrNothingType,
                "display_name": "Direction",
                "short_desc": "Email direction",
                "desc": "The direction of the email: `incoming` or `outgoing`",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Incoming",
                        "short_desc": "Received email",
                        "desc": "Email received from an external sender",
                        "value": "incoming",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Outgoing",
                        "short_desc": "Sent email",
                        "desc": "Email sent to an external recipient",
                        "value": "outgoing",
                    },
                ),
            },
            "subject": {
                "type": StringOrNothingType,
                "display_name": "Subject",
                "short_desc": "Email subject",
                "desc": "The subject line of the email",
            },
            "body_text": {
                "type": StringOrNothingType,
                "display_name": "Body Text",
                "short_desc": "Email body as plain text",
                "desc": "The plain text body of the email",
            },
            "body_html": {
                "type": StringOrNothingType,
                "display_name": "Body HTML",
                "short_desc": "Email body as HTML",
                "desc": "The HTML body of the email",
            },
            "sender": {
                "type": StringOrNothingType,
                "display_name": "Sender",
                "short_desc": "Email sender",
                "desc": "The sender email address",
            },
            "to": {
                "type": AutoListOrNothingType,
                "display_name": "To",
                "short_desc": "To recipients",
                "desc": "A list of recipient email addresses",
            },
            "cc": {
                "type": AutoListOrNothingType,
                "display_name": "CC",
                "short_desc": "CC recipients",
                "desc": "A list of CC recipient email addresses",
            },
            "bcc": {
                "type": AutoListOrNothingType,
                "display_name": "BCC",
                "short_desc": "BCC recipients",
                "desc": "A list of BCC recipient email addresses",
            },
            "attachments": {
                "type": AutoListOrNothingType,
                "display_name": "Attachments",
                "short_desc": "Email attachments",
                "desc": "A list of attachment objects associated with this email",
            },
            "template_id": {
                "type": StringOrNothingType,
                "display_name": "Template ID",
                "short_desc": "The email template ID",
                "desc": "The unique identifier of the email template used, if any",
            },
            "date_created": {
                "type": StringOrNothingType,
                "display_name": "Date Created",
                "short_desc": "When the email was created",
                "desc": "The timestamp when the email was created (ISO 8601 format)",
            },
            "date_updated": {
                "type": StringOrNothingType,
                "display_name": "Date Updated",
                "short_desc": "When the email was last updated",
                "desc": "The timestamp when the email was last updated (ISO 8601 format)",
            },
            "organization_id": {
                "type": StringOrNothingType,
                "display_name": "Organization ID",
                "short_desc": "The organization ID",
                "desc": "The unique identifier of the organization this email belongs to",
            },
            "created_by": {
                "type": StringOrNothingType,
                "display_name": "Created By",
                "short_desc": "The user who created the email",
                "desc": "The unique identifier of the user who created this email",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace CloseDataProvider
