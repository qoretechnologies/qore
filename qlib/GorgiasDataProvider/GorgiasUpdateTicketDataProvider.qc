# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GorgiasUpdateTicketDataProvider.qc Gorgias update ticket action data provider

/*  GorgiasUpdateTicketDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a ticket
public class GorgiasUpdateTicketRequestDataType inherits HashDataType {
    public {
        #! Field definitions for updating a ticket
        const Fields = {
            "ticket_id": {
                "type": IntType,
                "display_name": "Ticket ID",
                "short_desc": "The ticket to update",
                "desc": "The ID of the ticket to update",
                "required": True,
                "ref_data": "tickets",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Ticket status",
                "desc": "The new status of the ticket",
                "allowed_values": StatusAllowedValues,
                "preselected": True,
            },
            "priority": {
                "type": StringOrNothingType,
                "display_name": "Priority",
                "short_desc": "Ticket priority",
                "desc": "The new priority level",
                "allowed_values": PriorityAllowedValues,
                "preselected": True,
            },
            "subject": {
                "type": StringOrNothingType,
                "display_name": "Subject",
                "short_desc": "Ticket subject",
                "desc": "The new subject line (max 998 characters)",
                "preselected": True,
            },
            "assignee_user": {
                "type": new GorgiasAssigneeRefDataType(AutoHashOrNothingType),
                "display_name": "Assignee User",
                "short_desc": "Assigned agent",
                "desc": "The agent to assign, or `NOTHING` to unassign",
            },
            "assignee_team": {
                "type": new GorgiasAssigneeRefDataType(AutoHashOrNothingType),
                "display_name": "Assignee Team",
                "short_desc": "Assigned team",
                "desc": "The team to assign, or `NOTHING` to unassign",
            },
            "tags": {
                "type": new ListDataType("tags",
                    new GorgiasTagRefDataType(AutoHashOrNothingType), True),
                "display_name": "Tags",
                "short_desc": "Tags to set",
                "desc": "List of tags to set on the ticket. Replaces existing tags. "
                    "Each tag must have a `name` field",
            },
            "customer": {
                "type": new GorgiasCustomerRefDataType(AutoHashOrNothingType),
                "display_name": "Customer",
                "short_desc": "Customer data",
                "desc": "Update the customer associated with this ticket. Provide `id` to "
                    "reference an existing customer, or `email` to match by email address",
            },
            "external_id": {
                "type": StringOrNothingType,
                "display_name": "External ID",
                "short_desc": "External identifier",
                "desc": "An external identifier for the ticket (max 255 characters)",
            },
            "spam": {
                "type": SoftBoolOrNothingType,
                "display_name": "Spam",
                "short_desc": "Mark as spam",
                "desc": "Whether to mark the ticket as spam",
            },
            "snooze_datetime": {
                "type": StringOrNothingType,
                "display_name": "Snooze Until",
                "short_desc": "Snooze datetime",
                "desc": "ISO 8601 timestamp until which to snooze the ticket",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Gorgias update ticket action data provider
public class GorgiasUpdateTicketDataProvider inherits GorgiasDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing Gorgias support ticket",
            "type": "GorgiasUpdateTicketDataProvider",
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
        return "Update an existing Gorgias support ticket";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new GorgiasUpdateTicketRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new GorgiasTicketDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int ticket_id = remove req.ticket_id;
        return doPut("tickets/" + ticket_id, req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace GorgiasDataProvider
