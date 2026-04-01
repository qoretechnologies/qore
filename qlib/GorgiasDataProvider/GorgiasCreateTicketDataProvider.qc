# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GorgiasCreateTicketDataProvider.qc Gorgias create ticket action data provider

/*  GorgiasCreateTicketDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data type for message objects within ticket creation
public class GorgiasCreateTicketMessageDataType inherits HashDataType {
    public {
        #! Field definitions for a ticket message
        const Fields = {
            "channel": {
                "type": StringType,
                "display_name": "Channel",
                "short_desc": "Message channel",
                "desc": "The communication channel for the message",
                "required": True,
                "allowed_values": ChannelAllowedValues,
                "example_value": "api",
            },
            "from_agent": {
                "type": SoftBoolType,
                "display_name": "From Agent",
                "short_desc": "Whether sent by agent",
                "desc": "Whether the message is from an agent (`True`) or a customer (`False`). "
                    "Required field",
                "required": True,
                "example_value": True,
            },
            "via": {
                "type": StringType,
                "display_name": "Via",
                "short_desc": "Message source",
                "desc": "How the message was received or sent through Gorgias",
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
                "example_value": "I haven't received my order yet. Order number: 98765.",
            },
            "body_html": {
                "type": StringOrNothingType,
                "display_name": "Body HTML",
                "short_desc": "HTML body",
                "desc": "The HTML body of the message. Provide either `body_text` or "
                    "`body_html`",
            },
            "subject": {
                "type": StringOrNothingType,
                "display_name": "Subject",
                "short_desc": "Message subject",
                "desc": "The subject line of the message (for email channel)",
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
            "public": {
                "type": SoftBoolOrNothingType,
                "display_name": "Public",
                "short_desc": "Whether publicly visible",
                "desc": "Whether the message is visible to the customer. Default: `True`. "
                    "Set to `False` for internal notes",
            },
            "external_id": {
                "type": StringOrNothingType,
                "display_name": "External ID",
                "short_desc": "External identifier",
                "desc": "An external identifier for the message from an integrated system",
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Request type for creating a ticket
public class GorgiasCreateTicketRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating a ticket
        const Fields = {
            "subject": {
                "type": StringOrNothingType,
                "display_name": "Subject",
                "short_desc": "Ticket subject line",
                "desc": "The subject line of the ticket (max 998 characters)",
                "preselected": True,
                "example_value": "Order not received - #98765",
            },
            "channel": {
                "type": StringOrNothingType,
                "display_name": "Channel",
                "short_desc": "Communication channel",
                "desc": "The channel for the ticket (e.g., `email`, `chat`, `api`)",
                "allowed_values": ChannelAllowedValues,
                "preselected": True,
            },
            "via": {
                "type": StringType,
                "display_name": "Via",
                "short_desc": "Ticket source",
                "desc": "How the ticket was created. Required field",
                "required": True,
                "allowed_values": ViaAllowedValues,
                "example_value": "api",
            },
            "customer": {
                "type": new GorgiasCustomerRefDataType(AutoHashOrNothingType),
                "display_name": "Customer",
                "short_desc": "Customer data",
                "desc": "The customer for this ticket. Provide `id` to reference an existing "
                    "customer, or `email` to match or create by email address",
                "preselected": True,
            },
            "messages": {
                "type": new ListDataType("messages",
                    new GorgiasCreateTicketMessageDataType(AutoHashOrNothingType), True),
                "display_name": "Messages",
                "short_desc": "Initial messages",
                "desc": "List of initial messages for the ticket. Required field. "
                    "Each message must have `channel`, `from_agent`, and `via`. "
                    "Provide either `body_text` or `body_html` for message content. "
                    "Up to 500 messages per ticket",
                "required": True,
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Ticket status",
                "desc": "The initial status of the ticket",
                "allowed_values": StatusAllowedValues,
                "preselected": True,
            },
            "priority": {
                "type": StringOrNothingType,
                "display_name": "Priority",
                "short_desc": "Ticket priority",
                "desc": "The priority level of the ticket",
                "allowed_values": PriorityAllowedValues,
            },
            "assignee_user": {
                "type": new GorgiasAssigneeRefDataType(AutoHashOrNothingType),
                "display_name": "Assignee User",
                "short_desc": "Assigned agent",
                "desc": "The agent to assign to this ticket",
            },
            "assignee_team": {
                "type": new GorgiasAssigneeRefDataType(AutoHashOrNothingType),
                "display_name": "Assignee Team",
                "short_desc": "Assigned team",
                "desc": "The team to assign to this ticket",
            },
            "tags": {
                "type": new ListDataType("tags",
                    new GorgiasTagRefDataType(AutoHashOrNothingType), True),
                "display_name": "Tags",
                "short_desc": "Tags to apply",
                "desc": "List of tags to apply to the ticket. Each tag must have a `name` field",
            },
            "external_id": {
                "type": StringOrNothingType,
                "display_name": "External ID",
                "short_desc": "External identifier",
                "desc": "An external identifier for the ticket (max 255 characters)",
            },
            "language": {
                "type": StringOrNothingType,
                "display_name": "Language",
                "short_desc": "Ticket language",
                "desc": "The language of the ticket content",
            },
            "spam": {
                "type": SoftBoolOrNothingType,
                "display_name": "Spam",
                "short_desc": "Mark as spam",
                "desc": "Whether to mark the ticket as spam. Default: `False`",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Gorgias create ticket action data provider
public class GorgiasCreateTicketDataProvider inherits GorgiasDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new Gorgias support ticket",
            "type": "GorgiasCreateTicketDataProvider",
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
        return "Create a new Gorgias support ticket";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new GorgiasCreateTicketRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new GorgiasTicketDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("tickets", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace GorgiasDataProvider
