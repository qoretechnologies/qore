# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GorgiasDataProvider module definition

/*  GorgiasMessageDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GorgiasDataProvider module
public namespace GorgiasDataProvider {

#! Gorgias message record type
public class GorgiasMessageDataType inherits HashDataType {
    public {
        #! Field definitions for Gorgias messages
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "Message ID",
                "short_desc": "The message identifier",
                "desc": "The unique identifier of the message",
            },
            "uri": {
                "type": StringOrNothingType,
                "display_name": "URI",
                "short_desc": "The message URI",
                "desc": "The URI path for the message resource",
            },
            "ticket_id": {
                "type": IntOrNothingType,
                "display_name": "Ticket ID",
                "short_desc": "Parent ticket ID",
                "desc": "The ID of the ticket this message belongs to",
            },
            "channel": {
                "type": StringOrNothingType,
                "display_name": "Channel",
                "short_desc": "Message channel",
                "desc": "The channel through which the message was sent "
                    "(e.g., `email`, `chat`, `api`, `internal-note`)",
            },
            "via": {
                "type": StringOrNothingType,
                "display_name": "Via",
                "short_desc": "Message source",
                "desc": "How the message was sent (e.g., `helpdesk`, `api`, `email`)",
            },
            "source": {
                "type": AutoHashOrNothingType,
                "display_name": "Source",
                "short_desc": "Message source details",
                "desc": "Detailed source information for the message",
            },
            "sender": {
                "type": new GorgiasSenderReceiverDataType(AutoHashOrNothingType),
                "display_name": "Sender",
                "short_desc": "Message sender",
                "desc": "The sender of the message (customer or agent)",
            },
            "receiver": {
                "type": new GorgiasSenderReceiverDataType(AutoHashOrNothingType),
                "display_name": "Receiver",
                "short_desc": "Message receiver",
                "desc": "The receiver of the message",
            },
            "from_agent": {
                "type": SoftBoolOrNothingType,
                "display_name": "From Agent",
                "short_desc": "Whether sent by agent",
                "desc": "Whether the message was sent by an agent (`True`) or a customer "
                    "(`False`)",
            },
            "subject": {
                "type": StringOrNothingType,
                "display_name": "Subject",
                "short_desc": "Message subject",
                "desc": "The subject line of the message",
            },
            "body_text": {
                "type": StringOrNothingType,
                "display_name": "Body Text",
                "short_desc": "Plain text body",
                "desc": "The plain text body of the message",
            },
            "body_html": {
                "type": StringOrNothingType,
                "display_name": "Body HTML",
                "short_desc": "HTML body",
                "desc": "The HTML body of the message",
            },
            "stripped_text": {
                "type": StringOrNothingType,
                "display_name": "Stripped Text",
                "short_desc": "Cleaned text body",
                "desc": "The text body with quoted text removed",
            },
            "stripped_html": {
                "type": StringOrNothingType,
                "display_name": "Stripped HTML",
                "short_desc": "Cleaned HTML body",
                "desc": "The HTML body with quoted text removed",
            },
            "public": {
                "type": SoftBoolOrNothingType,
                "display_name": "Public",
                "short_desc": "Whether publicly visible",
                "desc": "Whether the message is visible to the customer (`True`) or an "
                    "internal note (`False`)",
            },
            "attachments": {
                "type": new ListDataType("attachments",
                    new GorgiasAttachmentDataType(AutoHashOrNothingType), True),
                "display_name": "Attachments",
                "short_desc": "Message attachments",
                "desc": "List of file attachments on this message",
            },
            "actions": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Actions",
                "short_desc": "Message actions",
                "desc": "List of actions associated with this message",
            },
            "macros": {
                "type": new ListDataType("macros",
                    new GorgiasTicketMessageMacroDataType(AutoHashOrNothingType), True),
                "display_name": "Macros",
                "short_desc": "Applied macros",
                "desc": "List of macros applied to this message. Each item has an `id` "
                    "field identifying the macro",
            },
            "meta": {
                "type": AutoHashOrNothingType,
                "display_name": "Meta",
                "short_desc": "Message metadata",
                "desc": "Additional metadata for the message",
            },
            "created_datetime": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "The ISO 8601 timestamp when the message was created",
            },
            "sent_datetime": {
                "type": StringOrNothingType,
                "display_name": "Sent At",
                "short_desc": "Sent timestamp",
                "desc": "The ISO 8601 timestamp when the message was sent",
            },
            "failed_datetime": {
                "type": StringOrNothingType,
                "display_name": "Failed At",
                "short_desc": "Failed timestamp",
                "desc": "The ISO 8601 timestamp when the message delivery failed",
            },
            "opened_datetime": {
                "type": StringOrNothingType,
                "display_name": "Opened At",
                "short_desc": "Opened timestamp",
                "desc": "The ISO 8601 timestamp when the message was opened by the recipient",
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

} # namespace GorgiasDataProvider
