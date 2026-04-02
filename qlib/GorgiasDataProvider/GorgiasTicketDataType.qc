# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GorgiasDataProvider module definition

/*  GorgiasTicketDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Gorgias ticket record type
public class GorgiasTicketDataType inherits HashDataType {
    public {
        #! Field definitions for Gorgias tickets
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "Ticket ID",
                "short_desc": "The ticket identifier",
                "desc": "The unique identifier of the ticket",
            },
            "uri": {
                "type": StringOrNothingType,
                "display_name": "URI",
                "short_desc": "The ticket URI",
                "desc": "The URI path for the ticket resource",
            },
            "external_id": {
                "type": StringOrNothingType,
                "display_name": "External ID",
                "short_desc": "External identifier",
                "desc": "An external identifier for the ticket from an integrated system",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Ticket status",
                "desc": "The current status of the ticket: `open` or `closed`",
            },
            "channel": {
                "type": StringOrNothingType,
                "display_name": "Channel",
                "short_desc": "Communication channel",
                "desc": "The channel through which the ticket was created "
                    "(e.g., `email`, `chat`, `api`, `internal-note`)",
            },
            "via": {
                "type": StringOrNothingType,
                "display_name": "Via",
                "short_desc": "Ticket source",
                "desc": "How the ticket was created (e.g., `helpdesk`, `api`, `email`)",
            },
            "from_agent": {
                "type": SoftBoolOrNothingType,
                "display_name": "From Agent",
                "short_desc": "Whether created by agent",
                "desc": "Whether the ticket was created by an agent (`True`) or a customer "
                    "(`False`)",
            },
            "spam": {
                "type": SoftBoolOrNothingType,
                "display_name": "Spam",
                "short_desc": "Whether marked as spam",
                "desc": "Whether the ticket has been marked as spam",
            },
            "subject": {
                "type": StringOrNothingType,
                "display_name": "Subject",
                "short_desc": "Ticket subject",
                "desc": "The subject line of the ticket (max 998 characters)",
            },
            "priority": {
                "type": StringOrNothingType,
                "display_name": "Priority",
                "short_desc": "Ticket priority",
                "desc": "The priority level of the ticket: `low`, `normal`, `high`, or `critical`",
            },
            "language": {
                "type": StringOrNothingType,
                "display_name": "Language",
                "short_desc": "Detected language",
                "desc": "The detected language of the ticket content",
            },
            "customer": {
                "type": new GorgiasCustomerRefDataType(AutoHashOrNothingType),
                "display_name": "Customer",
                "short_desc": "Associated customer",
                "desc": "The customer associated with this ticket",
            },
            "assignee_user": {
                "type": new GorgiasAssigneeRefDataType(AutoHashOrNothingType),
                "display_name": "Assignee User",
                "short_desc": "Assigned agent",
                "desc": "The agent assigned to this ticket",
            },
            "assignee_team": {
                "type": new GorgiasAssigneeRefDataType(AutoHashOrNothingType),
                "display_name": "Assignee Team",
                "short_desc": "Assigned team",
                "desc": "The team assigned to this ticket",
            },
            "tags": {
                "type": new ListDataType("tags",
                    new GorgiasTagRefDataType(AutoHashOrNothingType), True),
                "display_name": "Tags",
                "short_desc": "Ticket tags",
                "desc": "List of tags applied to this ticket",
            },
            "messages": {
                "type": new ListDataType("messages",
                    new GorgiasMessageDataType(AutoHashOrNothingType), True),
                "display_name": "Messages",
                "short_desc": "Ticket messages",
                "desc": "List of messages in this ticket",
            },
            "meta": {
                "type": AutoHashOrNothingType,
                "display_name": "Meta",
                "short_desc": "Ticket metadata",
                "desc": "Additional metadata for the ticket",
            },
            "custom_fields": {
                "type": AutoHashOrNothingType,
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "Custom field values associated with this ticket",
            },
            "is_unread": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Unread",
                "short_desc": "Whether unread",
                "desc": "Whether the ticket has unread messages",
            },
            "created_datetime": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "The ISO 8601 timestamp when the ticket was created",
            },
            "opened_datetime": {
                "type": StringOrNothingType,
                "display_name": "Opened At",
                "short_desc": "Opened timestamp",
                "desc": "The ISO 8601 timestamp when the ticket was opened",
            },
            "updated_datetime": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "The ISO 8601 timestamp when the ticket was last updated",
            },
            "closed_datetime": {
                "type": StringOrNothingType,
                "display_name": "Closed At",
                "short_desc": "Closed timestamp",
                "desc": "The ISO 8601 timestamp when the ticket was closed",
            },
            "last_message_datetime": {
                "type": StringOrNothingType,
                "display_name": "Last Message At",
                "short_desc": "Last message timestamp",
                "desc": "The ISO 8601 timestamp of the last message in the ticket",
            },
            "last_received_message_datetime": {
                "type": StringOrNothingType,
                "display_name": "Last Received Message At",
                "short_desc": "Last received message timestamp",
                "desc": "The ISO 8601 timestamp of the last message received from the customer",
            },
            "snooze_datetime": {
                "type": StringOrNothingType,
                "display_name": "Snooze Until",
                "short_desc": "Snooze datetime",
                "desc": "The ISO 8601 timestamp until which the ticket is snoozed",
            },
            "trashed_datetime": {
                "type": StringOrNothingType,
                "display_name": "Trashed At",
                "short_desc": "Trashed timestamp",
                "desc": "The ISO 8601 timestamp when the ticket was trashed",
            },
            "satisfaction_survey": {
                "type": new GorgiasSatisfactionSurveyDataType(AutoHashOrNothingType),
                "display_name": "Satisfaction Survey",
                "short_desc": "CSAT survey data",
                "desc": "Customer satisfaction survey data for this ticket, including "
                    "score, customer comment, and timestamps",
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

} # namespace GorgiasDataProvider
