# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CustomerIoDataProvider module definition

/*  CustomerIoBaseEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Base class for Customer.io event data providers
/** This abstract class provides common functionality for all Customer.io webhook-based event providers.

    Customer.io reporting webhooks are registered via POST /reporting_webhooks (App API) and deliver
    events via HTTP POST.
*/
public class CustomerIoBaseEventDataProvider inherits CustomerIoDataProviderBase, DataProvider::DelayedObservable {
    private {
        #! Webhook info returned from DataProviderWebhook::createWebhook()
        *hash<auto> webhook_info;

        #! Customer.io reporting webhook ID
        *int webhook_id;
    }

    #! Creates the object with a REST client
    constructor(CustomerIoRestClient::CustomerIoRestClient rest)
            : CustomerIoDataProviderBase(rest) {
    }

    #! Stops observing and deletes the object
    destructor() {
        stopEventsIntern();
    }

    #! Returns the event type that this provider handles
    /** Must be overridden by subclasses */
    abstract string getEventType();

    #! Returns the Customer.io webhook metric name (e.g., "email_sent")
    /** Must be overridden by subclasses */
    abstract string getMetricName();

    #! Returns the event data type
    /** Must be overridden by subclasses */
    abstract AbstractDataProviderType getEventDataType();

    #! Called when all observers have been added to the object
    observersReady() {
        # Create local webhook endpoint
        webhook_info = DataProvider::DataProviderWebhook::createWebhook({
            "method": "POST",
            "callback": \handleWebhookEvent(),
        });

        # Register reporting webhook with Customer.io
        try {
            hash<auto> body = {
                "endpoint": webhook_info.url,
                "events": {
                    getMetricName(): True,
                },
            };

            hash<auto> response = doPost("/reporting_webhooks", body);

            # Customer.io returns the webhook ID
            webhook_id = response.id;

            log(LoggerLevel::INFO, "CustomerIoBaseEventDataProvider: registered webhook %y (ID: %d) for %s events",
                webhook_info.url, webhook_id, getEventType());
        } catch (hash<ExceptionInfo> ex) {
            # Clean up local webhook on failure
            if (webhook_info) {
                DataProvider::DataProviderWebhook::deleteWebhook(webhook_info);
                remove webhook_info;
            }
            rethrow;
        }
    }

    #! Called to stop generating events
    stopEvents() {
        stopEventsIntern();
    }

    #! Returns a hash of all supported event types
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        string event_type = getEventType();
        return {
            event_type: <DataProviderMessageInfo>{
                "desc": sprintf("A %s event has occurred", event_type),
                "type": getEventDataType(),
            },
        };
    }

    #! Returns example data for the given observable event
    private auto getExampleEventDataImpl(string event_id) {
        # Try to fetch real event data from the API
        try {
            auto response = doGet("/activities", {"type": getMetricName(), "limit": 1});
            auto activities = response.activities ?? response;
            if (activities.typeCode() == NT_LIST && elements activities > 0) {
                return activities[0];
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getExampleEventDataImpl: error fetching real data: %s", ex.desc);
        }
        return getFakeExampleData();
    }

    #! Returns fake example data with event-type-specific fields
    private hash<auto> getFakeExampleData() {
        string metric = getMetricName();
        hash<auto> rv = {
            "event_id": "evt_0123456789abcdef",
            "metric": metric,
            "object_type": getObjectType(),
            "timestamp": get_epoch_seconds(now()),
            "customer_id": "customer-123",
            "email_address": "customer@example.com",
            "campaign_id": 100,
            "action_id": 200,
            "delivery_id": "del_0123456789abcdef",
        };
        # Add event-type-specific fields
        switch (metric) {
            case "email_sent":
            case "email_delivered":
            case "email_opened":
                rv.subject = "Welcome to our service!";
                rv.recipient = "customer@example.com";
                break;
            case "email_clicked":
            case "sms_clicked":
            case "slack_clicked":
                rv.subject = "Check out our new features!";
                rv.recipient = "customer@example.com";
                rv.link_url = "https://example.com/features";
                rv.href = "https://example.com/features";
                break;
            case "email_bounced":
                rv.subject = "Welcome!";
                rv.recipient = "bounced@invalid.example.com";
                rv.bounce_type = "hard";
                rv.reason = "550 5.1.1 User unknown";
                break;
            case "email_dropped":
            case "email_spammed":
            case "email_unsubscribed":
                rv.subject = "Newsletter";
                rv.recipient = "customer@example.com";
                rv.reason = metric == "email_spammed" ? "Marked as spam by recipient" : "Unsubscribed";
                break;
            case "sms_sent":
            case "sms_delivered":
                rv.recipient = "+15551234567";
                break;
            case "sms_failed":
                rv.recipient = "+15551234567";
                rv.reason = "Invalid phone number";
                break;
            case "push_sent":
            case "push_delivered":
            case "push_opened":
                rv.subject = "New message for you!";
                break;
            case "push_failed":
                rv.subject = "New message for you!";
                rv.reason = "Device token expired";
                break;
            case "slack_sent":
                rv.subject = "Slack notification";
                break;
            case "slack_failed":
                rv.subject = "Slack notification";
                rv.reason = "Channel not found";
                break;
        }
        return rv;
    }

    #! Returns the object type based on the metric name
    private string getObjectType() {
        string metric = getMetricName();
        if (metric =~ /^email_/) return "email";
        if (metric =~ /^sms_/) return "sms";
        if (metric =~ /^push_/) return "push";
        if (metric =~ /^slack_/) return "slack";
        return "email";
    }

    #! Stops the webhook and cleans up
    private stopEventsIntern() {
        # Delete reporting webhook from Customer.io first
        if (webhook_id) {
            try {
                doDelete(sprintf("/reporting_webhooks/%d", webhook_id));
                log(LoggerLevel::INFO, "CustomerIoBaseEventDataProvider: deleted Customer.io webhook %d",
                    webhook_id);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "CustomerIoBaseEventDataProvider: error deleting webhook: %s",
                    get_exception_string(ex));
            }
            remove webhook_id;
        }

        # Delete local webhook endpoint
        if (webhook_info) {
            DataProvider::DataProviderWebhook::deleteWebhook(webhook_info);
            remove webhook_info;
        }
    }

    #! Handle incoming webhook events from Customer.io
    private handleWebhookEvent(hash<auto> payload) {
        string event_type = getEventType();
        log(LoggerLevel::DEBUG, "handleWebhookEvent: received %s event", event_type);

        # Verify metric matches
        string received_metric = payload.metric ?? "";
        if (received_metric && received_metric != getMetricName()) {
            log(LoggerLevel::DEBUG, "handleWebhookEvent: ignoring metric %s (expecting %s)",
                received_metric, getMetricName());
            return;
        }

        notifyObservers(event_type, payload);
    }
}

} # namespace CustomerIoDataProvider
