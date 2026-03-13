# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LemlistDataProvider module definition

/*  LemlistBaseEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the LemlistDataProvider module
public namespace LemlistDataProvider {

#! Base class for Lemlist event data providers (webhook-based triggers)
/** This abstract class provides common functionality for all Lemlist webhook-based event providers.

    Lemlist webhooks deliver events via HTTP POST to a registered URL.
    The webhook payload contains the event type and lead/campaign metadata.
*/
public class LemlistBaseEventDataProvider inherits LemlistDataProviderBase, DataProvider::DelayedObservable {
    public {
        #! Local options (event-specific options)
        const LocalOptions = {
            "campaignId": <DataProviderOptionInfo>{
                "display_name": "Campaign ID",
                "short_desc": "Filter events to a specific campaign",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Filter events to a specific Lemlist campaign. If not specified, watches all campaigns.",
            },
        };
    }

    private {
        #! Optional campaign ID to filter events
        *string campaignId;

        #! Webhook info returned from DataProviderWebhook::createWebhook()
        *hash<auto> webhook_info;

        #! Lemlist webhook ID (returned when subscribing)
        *string webhook_id;
    }

    #! Creates the object with a REST client
    constructor(LemlistRestClient::LemlistRestClient rest, *hash<auto> options)
            : LemlistDataProviderBase(rest) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", LocalOptions, options);
        if (copts.campaignId) {
            campaignId = copts.campaignId;
        }
    }

    #! Stops observing and deletes the object
    destructor() {
        stopEventsIntern();
    }

    #! Returns the event type that this provider handles
    /** Must be overridden by subclasses */
    abstract string getEventType();

    #! Returns the Lemlist webhook event type (e.g., emailsSent)
    /** Must be overridden by subclasses */
    abstract string getWebhookEventType();

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

        # Register webhook with Lemlist
        try {
            hash<auto> body = {
                "targetUrl": webhook_info.url,
                "type": getWebhookEventType(),
            };

            # Optionally filter by campaign
            if (campaignId) {
                body.campaignId = campaignId;
            }

            hash<auto> response = doPost("/hooks", body);

            # Lemlist returns the webhook ID
            webhook_id = response._id ?? response.id;
            if (!webhook_id) {
                log(LoggerLevel::WARN, "LemlistBaseEventDataProvider: webhook registered but no ID returned "
                    "(response keys: %y) — webhook cannot be cleaned up on stop", keys response);
            }

            log(LoggerLevel::INFO, "LemlistBaseEventDataProvider: registered webhook %y (ID: %s) for %s events",
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
        # Try real API data first
        try {
            hash<auto> params = {"type": getWebhookEventType(), "limit": 1};
            auto response = doGet("/activities", params);
            list<auto> items = response.typeCode() == NT_LIST
                ? response
                : (response.activities ?? response.items ?? ());
            if (items) {
                hash<auto> item = items[0];
                return {
                    "type": item.type ?? getWebhookEventType(),
                    "campaignId": item.campaignId,
                    "campaignName": item.campaignName,
                    "leadId": item.leadId,
                    "leadEmail": item.leadEmail ?? item.email,
                    "sequenceStepId": item.sequenceStepId,
                    "createdAt": item.createdAt,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "LemlistBaseEventDataProvider: failed to get live example data: %s; using fake data",
                get_exception_string(ex));
        }
        return getFakeExampleData();
    }

    #! Returns fake example data for the event type
    private hash<auto> getFakeExampleData() {
        return {
            "type": getWebhookEventType(),
            "campaignId": "cam_abc123",
            "campaignName": "My Outreach Campaign",
            "leadId": "lea_xyz789",
            "leadEmail": "prospect@example.com",
            "sequenceStepId": "seq_step_001",
            "createdAt": "2026-01-15T10:30:00.000Z",
        };
    }

    #! Stops the webhook and cleans up
    private stopEventsIntern() {
        # Delete webhook from Lemlist first
        if (webhook_id) {
            try {
                doDelete(sprintf("/hooks/%s", encode_url(webhook_id, True)));
                log(LoggerLevel::INFO, "LemlistBaseEventDataProvider: deleted Lemlist webhook %s", webhook_id);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "LemlistBaseEventDataProvider: error deleting Lemlist webhook: %s",
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

    #! Handle incoming webhook events from Lemlist
    private handleWebhookEvent(hash<auto> payload) {
        string event_type = getEventType();
        log(LoggerLevel::DEBUG, "handleWebhookEvent: received %s event", event_type);

        # Verify the event type matches
        string received_type = payload.type ?? "";
        if (!received_type || received_type != getWebhookEventType()) {
            log(LoggerLevel::DEBUG, "handleWebhookEvent: ignoring event type %s (expecting %s)",
                received_type, getWebhookEventType());
            return;
        }

        # Build normalized event data
        hash<auto> event_data = {
            "type": payload.type,
            "campaignId": payload.campaignId,
            "campaignName": payload.campaignName,
            "leadId": payload.leadId,
            "leadEmail": payload.leadEmail ?? payload.email,
            "sequenceStepId": payload.sequenceStepId,
            "createdAt": payload.createdAt ?? now_us().format("YYYY-MM-DDTHH:mm:ss.xxxZ"),
        };

        notifyObservers(event_type, event_data);
    }
}

} # namespace LemlistDataProvider
