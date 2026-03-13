# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyEventDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the InstantlyDataProvider module
public namespace InstantlyDataProvider {

#! Event data type for reply received events
public class InstantlyReplyReceivedEventDataType inherits HashDataType {
    public {
        const Fields = {
            "event_type": {
                "type": StringType,
                "display_name": "Event Type",
                "short_desc": "The webhook event type",
                "desc": "The type of event: `reply_received`",
            },
            "timestamp": {
                "type": StringOrNothingType,
                "display_name": "Timestamp",
                "short_desc": "When the event occurred",
                "desc": "ISO 8601 timestamp of the event",
            },
            "campaign_id": {
                "type": StringOrNothingType,
                "display_name": "Campaign ID",
                "short_desc": "The campaign UUID",
                "desc": "The UUID of the campaign associated with this event",
            },
            "campaign_name": {
                "type": StringOrNothingType,
                "display_name": "Campaign Name",
                "short_desc": "The campaign name",
                "desc": "The name of the campaign associated with this event",
            },
            "lead_email": {
                "type": StringOrNothingType,
                "display_name": "Lead Email",
                "short_desc": "The lead's email address",
                "desc": "The email address of the lead who replied",
            },
            "email_account": {
                "type": StringOrNothingType,
                "display_name": "Email Account",
                "short_desc": "The sending email account",
                "desc": "The email account that sent the original email",
            },
            "reply_body": {
                "type": StringOrNothingType,
                "display_name": "Reply Body",
                "short_desc": "The reply content",
                "desc": "The body content of the reply",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Event data type for email bounced events
public class InstantlyEmailBouncedEventDataType inherits HashDataType {
    public {
        const Fields = {
            "event_type": {
                "type": StringType,
                "display_name": "Event Type",
                "short_desc": "The webhook event type",
                "desc": "The type of event: `email_bounced`",
            },
            "timestamp": {
                "type": StringOrNothingType,
                "display_name": "Timestamp",
                "short_desc": "When the event occurred",
                "desc": "ISO 8601 timestamp of the event",
            },
            "campaign_id": {
                "type": StringOrNothingType,
                "display_name": "Campaign ID",
                "short_desc": "The campaign UUID",
                "desc": "The UUID of the campaign associated with this event",
            },
            "campaign_name": {
                "type": StringOrNothingType,
                "display_name": "Campaign Name",
                "short_desc": "The campaign name",
                "desc": "The name of the campaign",
            },
            "lead_email": {
                "type": StringOrNothingType,
                "display_name": "Lead Email",
                "short_desc": "The bounced email address",
                "desc": "The email address that bounced",
            },
            "email_account": {
                "type": StringOrNothingType,
                "display_name": "Email Account",
                "short_desc": "The sending email account",
                "desc": "The email account that sent the email",
            },
            "bounce_type": {
                "type": StringOrNothingType,
                "display_name": "Bounce Type",
                "short_desc": "Hard or soft bounce",
                "desc": "The type of bounce (`hard` or `soft`)",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Hard Bounce",
                        "short_desc": "Permanent delivery failure",
                        "value": "hard",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Soft Bounce",
                        "short_desc": "Temporary delivery failure",
                        "value": "soft",
                    },
                ),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Event data type for lead interested events
public class InstantlyLeadInterestedEventDataType inherits HashDataType {
    public {
        const Fields = {
            "event_type": {
                "type": StringType,
                "display_name": "Event Type",
                "short_desc": "The webhook event type",
                "desc": "The type of event: `lead_interested`",
            },
            "timestamp": {
                "type": StringOrNothingType,
                "display_name": "Timestamp",
                "short_desc": "When the event occurred",
                "desc": "ISO 8601 timestamp of the event",
            },
            "campaign_id": {
                "type": StringOrNothingType,
                "display_name": "Campaign ID",
                "short_desc": "The campaign UUID",
                "desc": "The UUID of the campaign associated with this event",
            },
            "campaign_name": {
                "type": StringOrNothingType,
                "display_name": "Campaign Name",
                "short_desc": "The campaign name",
                "desc": "The name of the campaign",
            },
            "lead_email": {
                "type": StringOrNothingType,
                "display_name": "Lead Email",
                "short_desc": "The lead's email address",
                "desc": "The email address of the interested lead",
            },
            "lead_first_name": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The lead's first name",
                "desc": "The first name of the interested lead",
            },
            "lead_last_name": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "The lead's last name",
                "desc": "The last name of the interested lead",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Event data type for campaign completed events
public class InstantlyCampaignCompletedEventDataType inherits HashDataType {
    public {
        const Fields = {
            "event_type": {
                "type": StringType,
                "display_name": "Event Type",
                "short_desc": "The webhook event type",
                "desc": "The type of event: `campaign_completed`",
            },
            "timestamp": {
                "type": StringOrNothingType,
                "display_name": "Timestamp",
                "short_desc": "When the event occurred",
                "desc": "ISO 8601 timestamp of the event",
            },
            "campaign_id": {
                "type": StringOrNothingType,
                "display_name": "Campaign ID",
                "short_desc": "The campaign UUID",
                "desc": "The UUID of the completed campaign",
            },
            "campaign_name": {
                "type": StringOrNothingType,
                "display_name": "Campaign Name",
                "short_desc": "The campaign name",
                "desc": "The name of the completed campaign",
            },
            "total_leads": {
                "type": IntOrNothingType,
                "display_name": "Total Leads",
                "short_desc": "Total leads in campaign",
                "desc": "The total number of leads in the completed campaign",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace InstantlyDataProvider
