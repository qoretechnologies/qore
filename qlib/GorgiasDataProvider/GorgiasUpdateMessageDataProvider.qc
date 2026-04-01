# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GorgiasUpdateMessageDataProvider.qc Gorgias update message action data provider

/*  GorgiasUpdateMessageDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a message
public class GorgiasUpdateMessageRequestDataType inherits HashDataType {
    public {
        #! Field definitions for updating a message
        const Fields = {
            "ticket_id": {
                "type": IntType,
                "display_name": "Ticket ID",
                "short_desc": "The ticket containing the message",
                "desc": "The ID of the ticket containing the message to update",
                "required": True,
                "ref_data": "tickets",
            },
            "message_id": {
                "type": IntType,
                "display_name": "Message ID",
                "short_desc": "The message to update",
                "desc": "The ID of the message to update",
                "required": True,
            },
            "body_text": {
                "type": StringOrNothingType,
                "display_name": "Body Text",
                "short_desc": "Plain text body",
                "desc": "The updated plain text body of the message",
                "preselected": True,
            },
            "body_html": {
                "type": StringOrNothingType,
                "display_name": "Body HTML",
                "short_desc": "HTML body",
                "desc": "The updated HTML body of the message",
                "preselected": True,
            },
            "channel": {
                "type": StringOrNothingType,
                "display_name": "Channel",
                "short_desc": "Message channel",
                "desc": "The channel for the message",
                "allowed_values": ChannelAllowedValues,
            },
            "via": {
                "type": StringOrNothingType,
                "display_name": "Via",
                "short_desc": "Message source",
                "desc": "How the message was created",
                "allowed_values": ViaAllowedValues,
            },
            "subject": {
                "type": StringOrNothingType,
                "display_name": "Subject",
                "short_desc": "Message subject",
                "desc": "The updated subject line of the message",
            },
            "from_agent": {
                "type": SoftBoolOrNothingType,
                "display_name": "From Agent",
                "short_desc": "Whether from agent",
                "desc": "Whether the message is from an agent (`True`) or customer (`False`)",
            },
            "sender": {
                "type": new GorgiasSenderReceiverDataType(AutoHashOrNothingType),
                "display_name": "Sender",
                "short_desc": "Message sender",
                "desc": "The sender of the message. Provide `id` for agents "
                    "or `email` for customers",
            },
            "receiver": {
                "type": new GorgiasSenderReceiverDataType(AutoHashOrNothingType),
                "display_name": "Receiver",
                "short_desc": "Message receiver",
                "desc": "The receiver of the message. Provide `id` for an agent "
                    "or `email` for a customer",
            },
            "external_id": {
                "type": StringOrNothingType,
                "display_name": "External ID",
                "short_desc": "External identifier",
                "desc": "An external identifier for the message",
            },
            "attachments": {
                "type": new ListDataType("attachments",
                    new GorgiasAttachmentDataType(AutoHashOrNothingType), True),
                "display_name": "Attachments",
                "short_desc": "File attachments",
                "desc": "Updated list of file attachments for the message",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Gorgias update message action data provider
public class GorgiasUpdateMessageDataProvider inherits GorgiasDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing message on a Gorgias ticket",
            "type": "GorgiasUpdateMessageDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(RestClientIo::RestClientIo rest) : GorgiasDataProviderBase(rest) {
    }

    string getName() {
        return "update";
    }

    *string getDesc() {
        return "Update an existing message on a Gorgias ticket";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new GorgiasUpdateMessageRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new GorgiasMessageDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int ticket_id = remove req.ticket_id;
        int message_id = remove req.message_id;
        return doPut("tickets/" + ticket_id + "/messages/" + message_id, req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace GorgiasDataProvider
