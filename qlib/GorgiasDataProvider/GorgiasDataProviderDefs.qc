# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GorgiasDataProviderDefs.qc defines constants and definitions for the GorgiasDataProvider module

/*  GorgiasDataProviderDefs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
    #! Application name constant
    public const AppName = "Gorgias";

    #! Default page size for list operations
    public const DefaultPageSize = 30;

    #! Default polling interval in seconds
    public const DefaultPollInterval = 60;

    #! Minimum polling interval in seconds
    public const MinPollInterval = 30;

    #! Maximum number of retries for rate-limited or transient error requests
    public const MaxRetries = 3;

    #! Set of retriable socket errors
    public const RetrySet = {
        "SOCKET-CONNECT-ERROR": True,
        "SOCKET-SSL-ERROR": True,
        "SOCKET-CLOSED": True,
        "SOCKET-TIMEOUT": True,
    };

    # --- Shared allowed values constants ---

    #! Allowed values for ticket status fields
    public const StatusAllowedValues = (
        <AllowedValueInfo>{"value": "open", "display_name": "Open"},
        <AllowedValueInfo>{"value": "closed", "display_name": "Closed"},
    );

    #! Allowed values for ticket priority fields
    public const PriorityAllowedValues = (
        <AllowedValueInfo>{"value": "low", "display_name": "Low"},
        <AllowedValueInfo>{"value": "normal", "display_name": "Normal"},
        <AllowedValueInfo>{"value": "high", "display_name": "High"},
        <AllowedValueInfo>{"value": "critical", "display_name": "Critical"},
    );

    #! Allowed values for communication channel fields
    public const ChannelAllowedValues = (
        <AllowedValueInfo>{"value": "email", "display_name": "Email"},
        <AllowedValueInfo>{"value": "chat", "display_name": "Chat"},
        <AllowedValueInfo>{"value": "api", "display_name": "API"},
        <AllowedValueInfo>{"value": "internal-note", "display_name": "Internal Note"},
        <AllowedValueInfo>{"value": "phone", "display_name": "Phone"},
        <AllowedValueInfo>{"value": "sms", "display_name": "SMS"},
        <AllowedValueInfo>{"value": "facebook", "display_name": "Facebook"},
        <AllowedValueInfo>{"value": "instagram", "display_name": "Instagram"},
        <AllowedValueInfo>{"value": "twitter", "display_name": "Twitter"},
        <AllowedValueInfo>{"value": "whatsapp", "display_name": "WhatsApp"},
        <AllowedValueInfo>{"value": "contact_form", "display_name": "Contact Form"},
        <AllowedValueInfo>{"value": "help-center", "display_name": "Help Center"},
    );

    #! Allowed values for ticket/message routing (via) fields
    public const ViaAllowedValues = (
        <AllowedValueInfo>{"value": "helpdesk", "display_name": "Helpdesk"},
        <AllowedValueInfo>{"value": "api", "display_name": "API"},
        <AllowedValueInfo>{"value": "email", "display_name": "Email"},
        <AllowedValueInfo>{"value": "chat", "display_name": "Chat"},
        <AllowedValueInfo>{"value": "phone", "display_name": "Phone"},
        <AllowedValueInfo>{"value": "sms", "display_name": "SMS"},
        <AllowedValueInfo>{"value": "facebook", "display_name": "Facebook"},
        <AllowedValueInfo>{"value": "instagram", "display_name": "Instagram"},
        <AllowedValueInfo>{"value": "twitter", "display_name": "Twitter"},
        <AllowedValueInfo>{"value": "whatsapp", "display_name": "WhatsApp"},
        <AllowedValueInfo>{"value": "contact_form", "display_name": "Contact Form"},
        <AllowedValueInfo>{"value": "help-center", "display_name": "Help Center"},
    );

    # --- Event name constants (used in triggers) ---

    #! Event name for new ticket
    public const EVENT_NEW_TICKET = "new-ticket";

    #! Event name for ticket update
    public const EVENT_TICKET_UPDATE = "ticket-update";

    #! Gorgias logo in SVG format (loaded from file)
    public const GorgiasLogo = File::readTextFile(get_script_dir() + "/gorgias-logo.svg");
}
