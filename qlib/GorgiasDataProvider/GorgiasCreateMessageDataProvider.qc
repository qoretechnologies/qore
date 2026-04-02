# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GorgiasCreateMessageDataProvider.qc Gorgias create message action data provider

/*  GorgiasCreateMessageDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a message
public class GorgiasCreateMessageRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "ticket_id": {
                "type": IntType,
                "display_name": "Ticket ID",
                "short_desc": "The ticket to add a message to",
                "desc": "The ID of the ticket to add the message to",
                "required": True,
                "ref_data": "tickets",
            },
            "channel": {
                "type": StringType,
                "display_name": "Channel",
                "short_desc": "Message channel",
                "desc": "The channel for the message",
                "required": True,
                "allowed_values": ChannelAllowedValues,
                "example_value": "internal-note",
            },
            "via": {
                "type": StringType,
                "display_name": "Via",
                "short_desc": "Message source",
                "desc": "How the message was created",
                "required": True,
                "allowed_values": ViaAllowedValues,
                "example_value": "api",
            },
            "body_text": {
                "type": StringOrNothingType,
                "display_name": "Body Text",
                "short_desc": "Plain text body",
                "desc": "The plain text body of the message. Provide either `body_text` or "
                    "`body_html`",
                "preselected": True,
                "example_value": "Thank you for contacting us. We are looking into your issue.",
            },
            "body_html": {
                "type": StringOrNothingType,
                "display_name": "Body HTML",
                "short_desc": "HTML body",
                "desc": "The HTML body of the message. Provide either `body_text` or "
                    "`body_html`",
                "preselected": True,
            },
            "subject": {
                "type": StringOrNothingType,
                "display_name": "Subject",
                "short_desc": "Message subject",
                "desc": "The subject line of the message (for email channel)",
            },
            "from_agent": {
                "type": SoftBoolOrNothingType,
                "display_name": "From Agent",
                "short_desc": "Whether from agent",
                "desc": "Whether the message is from an agent (`True`) or customer (`False`)",
                "preselected": True,
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
            "attachments": {
                "type": new ListDataType("attachments",
                    new GorgiasAttachmentDataType(AutoHashOrNothingType), True),
                "display_name": "Attachments",
                "short_desc": "File attachments",
                "desc": "List of file attachments for the message. Each attachment "
                    "requires a `url` pointing to the file location, and optionally "
                    "`name`, `content_type`, `size`, and `public` fields",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Gorgias create message action data provider
public class GorgiasCreateMessageDataProvider inherits GorgiasDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Add a message to a Gorgias ticket",
            "type": "GorgiasCreateMessageDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(RestClientIo::RestClientIo rest) : GorgiasDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Add a message to a Gorgias ticket";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new GorgiasCreateMessageRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new GorgiasMessageDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int ticket_id = remove req.ticket_id;
        return doPost("tickets/" + ticket_id + "/messages", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace GorgiasDataProvider
