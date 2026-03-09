# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MailgunDataProvider module definition

/*  MailgunEventDataTypes.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the MailgunDataProvider module
public namespace MailgunDataProvider {

#! Base event data type with common fields
public class MailgunBaseEventDataType inherits HashDataType {
    public {
        #! Common field definitions for all Mailgun events
        const CommonFields = {
            "event": {
                "type": StringType,
                "display_name": "Event Type",
                "short_desc": "The type of event",
                "desc": "The type of event that occurred",
            },
            "timestamp": {
                "type": FloatOrNothingType,
                "display_name": "Timestamp",
                "short_desc": "Event timestamp",
                "desc": "Unix timestamp of when the event occurred",
            },
            "id": {
                "type": StringOrNothingType,
                "display_name": "Event ID",
                "short_desc": "Unique event identifier",
                "desc": "Unique identifier for this event",
            },
            "recipient": {
                "type": StringOrNothingType,
                "display_name": "Recipient",
                "short_desc": "Email recipient",
                "desc": "The recipient email address",
            },
            "tags": {
                "type": SoftListOrNothingType,
                "display_name": "Tags",
                "short_desc": "Message tags",
                "desc": "Tags associated with the message",
            },
            "message": {
                "type": AutoHashOrNothingType,
                "display_name": "Message",
                "short_desc": "Message details",
                "desc": "Message details including headers",
            },
            "campaigns": {
                "type": SoftListOrNothingType,
                "display_name": "Campaigns",
                "short_desc": "Associated campaigns",
                "desc": "Campaigns associated with the message",
            },
            "user-variables": {
                "type": AutoHashOrNothingType,
                "display_name": "User Variables",
                "short_desc": "Custom variables",
                "desc": "Custom variables attached to the message",
            },
        };
    }

    #! Creates the type with common fields
    constructor() {
        addQoreFields(CommonFields);
    }
}

#! Mailgun clicked event data type
public class MailgunClickedEventDataType inherits MailgunBaseEventDataType {
    public {
        #! Additional fields for click events
        const ClickFields = {
            "url": {
                "type": StringOrNothingType,
                "display_name": "URL",
                "short_desc": "Clicked URL",
                "desc": "The URL that was clicked",
            },
            "ip": {
                "type": StringOrNothingType,
                "display_name": "IP Address",
                "short_desc": "Clicker's IP",
                "desc": "The IP address of the clicker",
            },
            "client-info": {
                "type": AutoHashOrNothingType,
                "display_name": "Client Info",
                "short_desc": "Client information",
                "desc": "Information about the client (browser, OS, device)",
            },
            "geolocation": {
                "type": AutoHashOrNothingType,
                "display_name": "Geolocation",
                "short_desc": "Geographic location",
                "desc": "Geographic location of the clicker",
            },
        };
    }

    #! Creates the type
    constructor() : MailgunBaseEventDataType() {
        addQoreFields(ClickFields);
    }
}

#! Mailgun delivered event data type
public class MailgunDeliveredEventDataType inherits MailgunBaseEventDataType {
    public {
        #! Additional fields for delivery events
        const DeliveryFields = {
            "delivery-status": {
                "type": AutoHashOrNothingType,
                "display_name": "Delivery Status",
                "short_desc": "Delivery status details",
                "desc": "Details about the delivery status including SMTP response",
            },
            "envelope": {
                "type": AutoHashOrNothingType,
                "display_name": "Envelope",
                "short_desc": "Email envelope",
                "desc": "Email envelope information (sender, transport)",
            },
        };
    }

    #! Creates the type
    constructor() : MailgunBaseEventDataType() {
        addQoreFields(DeliveryFields);
    }
}

#! Mailgun opened event data type
public class MailgunOpenedEventDataType inherits MailgunBaseEventDataType {
    public {
        #! Additional fields for open events
        const OpenFields = {
            "ip": {
                "type": StringOrNothingType,
                "display_name": "IP Address",
                "short_desc": "Opener's IP",
                "desc": "The IP address of the opener",
            },
            "client-info": {
                "type": AutoHashOrNothingType,
                "display_name": "Client Info",
                "short_desc": "Client information",
                "desc": "Information about the client (browser, OS, device)",
            },
            "geolocation": {
                "type": AutoHashOrNothingType,
                "display_name": "Geolocation",
                "short_desc": "Geographic location",
                "desc": "Geographic location of the opener",
            },
        };
    }

    #! Creates the type
    constructor() : MailgunBaseEventDataType() {
        addQoreFields(OpenFields);
    }
}

#! Mailgun permanent failure event data type
public class MailgunPermanentFailEventDataType inherits MailgunBaseEventDataType {
    public {
        #! Additional fields for permanent failure events
        const FailureFields = {
            "severity": {
                "type": StringOrNothingType,
                "display_name": "Severity",
                "short_desc": "Failure severity",
                "desc": "The severity of the failure (permanent or temporary)",
                "example_value": "permanent",
            },
            "reason": {
                "type": StringOrNothingType,
                "display_name": "Reason",
                "short_desc": "Failure reason",
                "desc": "The reason for the failure",
            },
            "delivery-status": {
                "type": AutoHashOrNothingType,
                "display_name": "Delivery Status",
                "short_desc": "Delivery status details",
                "desc": "Details about the delivery failure including SMTP response",
            },
            "envelope": {
                "type": AutoHashOrNothingType,
                "display_name": "Envelope",
                "short_desc": "Email envelope",
                "desc": "Email envelope information",
            },
        };
    }

    #! Creates the type
    constructor() : MailgunBaseEventDataType() {
        addQoreFields(FailureFields);
    }
}

#! Mailgun temporary failure event data type
public class MailgunTemporaryFailEventDataType inherits MailgunPermanentFailEventDataType {
    #! Creates the type - uses same fields as permanent failure
    constructor() : MailgunPermanentFailEventDataType() {
    }
}

#! Mailgun complained (spam) event data type
public class MailgunComplainedEventDataType inherits MailgunBaseEventDataType {
    #! Creates the type
    constructor() : MailgunBaseEventDataType() {
        # Complained events use the base fields
    }
}

#! Mailgun unsubscribed event data type
public class MailgunUnsubscribedEventDataType inherits MailgunBaseEventDataType {
    public {
        #! Additional fields for unsubscribe events
        const UnsubscribeFields = {
            "ip": {
                "type": StringOrNothingType,
                "display_name": "IP Address",
                "short_desc": "User's IP",
                "desc": "The IP address of the user who unsubscribed",
            },
            "client-info": {
                "type": AutoHashOrNothingType,
                "display_name": "Client Info",
                "short_desc": "Client information",
                "desc": "Information about the client (browser, OS, device)",
            },
            "geolocation": {
                "type": AutoHashOrNothingType,
                "display_name": "Geolocation",
                "short_desc": "Geographic location",
                "desc": "Geographic location of the user",
            },
        };
    }

    #! Creates the type
    constructor() : MailgunBaseEventDataType() {
        addQoreFields(UnsubscribeFields);
    }
}

} # namespace MailgunDataProvider
