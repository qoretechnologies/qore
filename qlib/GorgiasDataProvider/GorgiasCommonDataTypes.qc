# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GorgiasCommonDataTypes.qc shared data types for Gorgias data provider actions

/*  GorgiasCommonDataTypes.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Data type for customer references in ticket creation/updates
/** Used when associating a customer with a ticket. Provide either `id` to reference
    an existing customer, or `email` to match/create by email address.
*/
public class GorgiasCustomerRefDataType inherits HashDataType {
    public {
        #! Field definitions for a customer reference
        const Fields = {
            "id": {
                "type": IntOrNothingType,
                "display_name": "Customer ID",
                "short_desc": "Existing customer ID",
                "desc": "The ID of an existing Gorgias customer to associate with the ticket",
                "ref_data": "customers",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "Customer email address",
                "desc": "The email address of the customer. If no customer exists with this email, "
                    "a new customer will be created",
                "example_value": "customer@example.com",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Customer name",
                "desc": "The name of the customer (used when creating a new customer)",
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Data type for message sender/receiver references
/** Used when specifying the sender or receiver of a message. Provide `id` for a Gorgias
    user (agent) or `email` for a customer.
*/
public class GorgiasSenderReceiverDataType inherits HashDataType {
    public {
        #! Field definitions for a sender/receiver reference
        const Fields = {
            "id": {
                "type": IntOrNothingType,
                "display_name": "User ID",
                "short_desc": "Gorgias user ID",
                "desc": "The ID of a Gorgias user (agent). Use this for agent senders/receivers",
                "ref_data": "users",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "Email address",
                "desc": "The email address. Use this for customer senders/receivers",
                "example_value": "customer@example.com",
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Data type for customer communication channel entries
/** Used when specifying communication channels for a customer (email addresses,
    phone numbers, social media accounts, etc.).
*/
public class GorgiasCustomerChannelDataType inherits HashDataType {
    public {
        #! Field definitions for a customer channel
        const Fields = {
            "id": {
                "type": IntOrNothingType,
                "display_name": "Channel ID",
                "short_desc": "The channel identifier",
                "desc": "The unique identifier of the customer channel",
            },
            "address": {
                "type": StringType,
                "display_name": "Address",
                "short_desc": "Channel address",
                "desc": "The channel address (email address, phone number, social media handle, etc.)",
                "required": True,
                "example_value": "customer@example.com",
            },
            "type": {
                "type": StringType,
                "display_name": "Type",
                "short_desc": "Channel type",
                "desc": "The type of communication channel",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "email", "display_name": "Email"},
                    <AllowedValueInfo>{"value": "phone", "display_name": "Phone"},
                    <AllowedValueInfo>{"value": "facebook", "display_name": "Facebook"},
                    <AllowedValueInfo>{"value": "chat", "display_name": "Chat"},
                    <AllowedValueInfo>{"value": "instagram", "display_name": "Instagram"},
                    <AllowedValueInfo>{"value": "twitter", "display_name": "Twitter"},
                    <AllowedValueInfo>{"value": "whatsapp", "display_name": "WhatsApp"},
                ),
                "example_value": "email",
            },
            "preferred": {
                "type": SoftBoolOrNothingType,
                "display_name": "Preferred",
                "short_desc": "Whether this is the preferred channel",
                "desc": "Whether this is the customer's preferred communication channel",
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Data type for tag references in ticket creation/updates
/** Used when applying tags to a ticket.
*/
public class GorgiasTagRefDataType inherits HashDataType {
    public {
        #! Field definitions for a tag reference
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Tag Name",
                "short_desc": "The tag name",
                "desc": "The name of the tag to apply to the ticket",
                "required": True,
                "example_value": "shipping",
            },
            "decoration": {
                "type": new GorgiasTagDecorationDataType(AutoHashOrNothingType),
                "display_name": "Decoration",
                "short_desc": "Visual decoration",
                "desc": "Optional visual decoration settings for the tag, including color",
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Data type for user role
/** Contains the role name for a Gorgias user.
*/
public class GorgiasRoleDataType inherits HashDataType {
    public {
        #! Field definitions for a role
        const Fields = {
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Role name",
                "desc": "The name of the role",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "admin", "display_name": "Admin"},
                    <AllowedValueInfo>{"value": "agent", "display_name": "Agent"},
                    <AllowedValueInfo>{"value": "basic-agent", "display_name": "Basic Agent"},
                    <AllowedValueInfo>{"value": "lite-agent", "display_name": "Lite Agent"},
                    <AllowedValueInfo>{"value": "observer", "display_name": "Observer"},
                ),
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Data type for assignee (user or team) references
/** Used when assigning a user or team to a ticket. Provide `{"id": <user_id>}`.
*/
public class GorgiasAssigneeRefDataType inherits HashDataType {
    public {
        #! Field definitions for an assignee reference
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "Assignee ID",
                "desc": "The ID of the user or team to assign",
                "required": True,
                "ref_data": "users",
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Data type for message attachment objects
/** Used when specifying attachments on a message. Each attachment requires a `url`
    pointing to the file location.
*/
public class GorgiasAttachmentDataType inherits HashDataType {
    public {
        #! Field definitions for an attachment
        const Fields = {
            "url": {
                "type": StringType,
                "display_name": "URL",
                "short_desc": "Attachment URL",
                "desc": "The URL of the attachment file",
                "required": True,
                "example_value": "https://example.com/files/document.pdf",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "File name",
                "desc": "The display name of the attachment file",
                "example_value": "document.pdf",
            },
            "content_type": {
                "type": StringOrNothingType,
                "display_name": "Content Type",
                "short_desc": "MIME type",
                "desc": "The MIME type of the attachment (e.g., `application/pdf`, `image/png`)",
                "example_value": "application/pdf",
            },
            "size": {
                "type": IntOrNothingType,
                "display_name": "Size",
                "short_desc": "File size in bytes",
                "desc": "The size of the attachment file in bytes",
            },
            "public": {
                "type": SoftBoolOrNothingType,
                "display_name": "Public",
                "short_desc": "Whether publicly accessible",
                "desc": "Whether the attachment is publicly accessible. Default: `True`",
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Data type for satisfaction survey on a ticket
/** Contains CSAT survey data including score, customer comment, and timestamps.
*/
public class GorgiasSatisfactionSurveyDataType inherits HashDataType {
    public {
        #! Field definitions for a satisfaction survey
        const Fields = {
            "id": {
                "type": IntOrNothingType,
                "display_name": "Survey ID",
                "short_desc": "The survey identifier",
                "desc": "The unique identifier of the satisfaction survey",
            },
            "uri": {
                "type": StringOrNothingType,
                "display_name": "URI",
                "short_desc": "The survey URI",
                "desc": "The URI path for the survey resource",
            },
            "ticket_id": {
                "type": IntOrNothingType,
                "display_name": "Ticket ID",
                "short_desc": "Associated ticket ID",
                "desc": "The ID of the ticket the survey is associated with",
            },
            "customer_id": {
                "type": IntOrNothingType,
                "display_name": "Customer ID",
                "short_desc": "Customer who filled the survey",
                "desc": "The ID of the customer who filled the survey",
            },
            "score": {
                "type": IntOrNothingType,
                "display_name": "Score",
                "short_desc": "Satisfaction score",
                "desc": "The level of satisfaction. Scores range from `1` to `5`",
            },
            "body_text": {
                "type": StringOrNothingType,
                "display_name": "Comment",
                "short_desc": "Customer comment",
                "desc": "The comment sent by the customer (max 1000 characters)",
            },
            "meta": {
                "type": AutoHashOrNothingType,
                "display_name": "Meta",
                "short_desc": "Survey metadata",
                "desc": "Structured key-value metadata",
            },
            "created_datetime": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "The ISO 8601 timestamp when the survey was created",
            },
            "sent_datetime": {
                "type": StringOrNothingType,
                "display_name": "Sent At",
                "short_desc": "Sent timestamp",
                "desc": "The ISO 8601 timestamp when the survey was sent. "
                    "`NOTHING` if not yet sent",
            },
            "scored_datetime": {
                "type": StringOrNothingType,
                "display_name": "Scored At",
                "short_desc": "Scored timestamp",
                "desc": "The ISO 8601 timestamp when the survey was filled by the customer",
            },
            "should_send_datetime": {
                "type": StringOrNothingType,
                "display_name": "Should Send At",
                "short_desc": "Scheduled send time",
                "desc": "The ISO 8601 timestamp when the survey should be sent. "
                    "`NOTHING` if it won't be sent by Gorgias",
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

#! Data type for macro references on messages
/** Each item represents a macro that was applied to the message.
*/
public class GorgiasTicketMessageMacroDataType inherits HashDataType {
    public {
        #! Field definitions for a message macro reference
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "Macro ID",
                "short_desc": "The macro identifier",
                "desc": "The unique identifier of the macro that was applied",
                "required": True,
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

} # namespace GorgiasDataProvider
