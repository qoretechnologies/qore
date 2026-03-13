# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyReplyReceivedEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Instantly reply received event data provider - watches for reply events
public class InstantlyReplyReceivedEventDataProvider inherits InstantlyBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "InstantlyReplyReceivedEventDataProvider",
            "display_name": "Reply Received",
            "short_desc": "Watch for email reply events",
            "desc": "Triggers when a lead replies to a campaign email",
            "constructor_options": ConstructorOptions,
            "supports_observable": True,
        };

        #! Constructor options
        const ConstructorOptions = InstantlyDataProviderBase::BaseConstructorOptions + LocalOptions;
    }

    private {
        #! Event data type
        static InstantlyReplyReceivedEventDataType eventDataType();
    }

    #! Creates the object with a REST client
    constructor(InstantlyRestClient::InstantlyRestClient rest, *hash<auto> options)
            : InstantlyBaseEventDataProvider(rest, options) {
    }

    #! Returns the data provider name
    string getName() {
        return EVENT_REPLY_RECEIVED;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Instantly reply received event - triggers when a lead replies to an email";
    }

    #! Returns the event type
    string getEventType() {
        return EVENT_REPLY_RECEIVED;
    }

    #! Returns the Instantly webhook event type
    string getWebhookEventType() {
        return WEBHOOK_REPLY_RECEIVED;
    }

    #! Returns the event data type
    AbstractDataProviderType getEventDataType() {
        return eventDataType;
    }

    #! Returns example event data
    private hash<auto> getFakeExampleData() {
        return {
            "event_type": WEBHOOK_REPLY_RECEIVED,
            "timestamp": now_us().format("YYYY-MM-DDTHH:mm:ss.xxxZ"),
            "campaign_id": "00000000-0000-0000-0000-000000000000",
            "campaign_name": "Example Campaign",
            "lead_email": "lead@example.com",
            "email_account": "sender@example.com",
            "reply_body": "Thanks for reaching out!",
        };
    }

    #! Returns event-specific fields from the webhook payload
    private hash<auto> getEventSpecificFields(hash<auto> payload) {
        return {
            "lead_email": payload.lead_email,
            "email_account": payload.email_account,
            "reply_body": payload.reply_body ?? payload.body,
        };
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace InstantlyDataProvider
