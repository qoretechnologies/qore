# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CustomerIoDataProvider module definition

/*  CustomerIoEventDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CustomerIoDataProvider module
public namespace CustomerIoDataProvider {

#! Customer.io webhook event data type
/** Represents a webhook event in Customer.io.
*/
public class CustomerIoWebhookEventDataType inherits HashDataType {
    public {
        #! Field definitions for Customer.io webhook events
        const Fields = {
            "event_id": {
                "type": StringOrNothingType,
                "display_name": "Event ID",
                "short_desc": "Unique event identifier",
                "desc": "The unique identifier for this event",
            },
            "metric": {
                "type": StringOrNothingType,
                "display_name": "Metric",
                "short_desc": "Event metric type",
                "desc": "The type of event (e.g., `email_sent`, `email_opened`)",
            },
            "object_type": {
                "type": StringOrNothingType,
                "display_name": "Object Type",
                "short_desc": "Type of object",
                "desc": "The type of object (e.g., `email`, `sms`, `push`)",
            },
            "timestamp": {
                "type": IntOrNothingType,
                "display_name": "Timestamp",
                "short_desc": "Event timestamp",
                "desc": "Unix timestamp when the event occurred",
            },
            "subject": {
                "type": StringOrNothingType,
                "display_name": "Subject",
                "short_desc": "Message subject",
                "desc": "The subject line of the message",
            },
            "recipient": {
                "type": StringOrNothingType,
                "display_name": "Recipient",
                "short_desc": "Message recipient",
                "desc": "The recipient of the message",
            },
            "customer_id": {
                "type": StringOrNothingType,
                "display_name": "Customer ID",
                "short_desc": "Customer identifier",
                "desc": "The ID of the customer associated with this event",
            },
            "email_address": {
                "type": StringOrNothingType,
                "display_name": "Email Address",
                "short_desc": "Email address",
                "desc": "The email address associated with this event",
            },
            "campaign_id": {
                "type": IntOrNothingType,
                "display_name": "Campaign ID",
                "short_desc": "Campaign identifier",
                "desc": "The campaign that triggered this event",
            },
            "newsletter_id": {
                "type": IntOrNothingType,
                "display_name": "Newsletter ID",
                "short_desc": "Newsletter identifier",
                "desc": "The newsletter that triggered this event",
            },
            "broadcast_id": {
                "type": IntOrNothingType,
                "display_name": "Broadcast ID",
                "short_desc": "Broadcast identifier",
                "desc": "The broadcast that triggered this event",
            },
            "action_id": {
                "type": IntOrNothingType,
                "display_name": "Action ID",
                "short_desc": "Action identifier",
                "desc": "The action (message) that triggered this event",
            },
            "delivery_id": {
                "type": StringOrNothingType,
                "display_name": "Delivery ID",
                "short_desc": "Delivery identifier",
                "desc": "The unique delivery identifier for this message",
            },
            "link_url": {
                "type": StringOrNothingType,
                "display_name": "Link URL",
                "short_desc": "Clicked link URL",
                "desc": "The URL that was clicked (for click events)",
            },
            "href": {
                "type": StringOrNothingType,
                "display_name": "Link Href",
                "short_desc": "Link href",
                "desc": "The original href of the clicked link",
            },
            "bounce_type": {
                "type": StringOrNothingType,
                "display_name": "Bounce Type",
                "short_desc": "Type of bounce",
                "desc": "The type of bounce (for bounce events, e.g., `hard`, `soft`)",
            },
            "reason": {
                "type": StringOrNothingType,
                "display_name": "Reason",
                "short_desc": "Event reason",
                "desc": "The reason for the event (e.g., bounce reason, failure reason)",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace CustomerIoDataProvider
