# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyBaseEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Base class for Instantly event data providers
/** This abstract class provides common functionality for all Instantly webhook-based event providers.

    Instantly webhooks are registered via POST /webhooks and deliver events via HTTP POST.
    The webhook payload contains the event data directly.
*/
public class InstantlyBaseEventDataProvider inherits InstantlyDataProviderBase, DataProvider::DelayedObservable {
    public {
        #! Local options (event-specific options)
        const LocalOptions = {
            "campaign_id": <DataProviderOptionInfo>{
                "display_name": "Campaign ID",
                "short_desc": "The campaign to watch for events",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The Instantly campaign ID to monitor for events. If not specified, watches all campaigns.",
            },
        };
    }

    private {
        #! Campaign ID to filter events (optional)
        *string campaign_id;

        #! Webhook info returned from DataProviderWebhook::createWebhook()
        *hash<auto> webhook_info;

        #! Instantly webhook ID (returned when creating webhook)
        *string webhook_id;
    }

    #! Creates the object with a REST client
    constructor(InstantlyRestClient::InstantlyRestClient rest, *hash<auto> options)
            : InstantlyDataProviderBase(rest) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", LocalOptions, options);
        if (copts.campaign_id) {
            campaign_id = copts.campaign_id;
        }
    }

    #! Stops observing and deletes the object
    destructor() {
        stopEventsIntern();
    }

    #! Returns the event type that this provider handles
    abstract string getEventType();

    #! Returns the Instantly webhook event type (e.g., reply_received)
    abstract string getWebhookEventType();

    #! Returns the event data type
    abstract AbstractDataProviderType getEventDataType();

    #! Called when all observers have been added to the object
    observersReady() {
        # Get campaign_id from context if not provided
        if (!exists campaign_id) {
            *hash<auto> ctx = DataProvider::DataProviderDataContextHelper::getHash();
            if (ctx.campaign_id) {
                campaign_id = ctx.campaign_id;
            }
        }

        # Create local webhook endpoint
        webhook_info = DataProvider::DataProviderWebhook::createWebhook({
            "method": "POST",
            "callback": \handleWebhookEvent(),
        });

        # Register webhook with Instantly
        try {
            hash<auto> body = {
                "webhook_url": webhook_info.url,
                "event_type": getWebhookEventType(),
            };

            # Optionally filter by campaign
            if (campaign_id) {
                body.campaign_id = campaign_id;
            }

            hash<auto> response = doPost("/webhooks", body);
            webhook_id = response.id;

            log(LoggerLevel::INFO, "InstantlyBaseEventDataProvider: registered webhook %y (ID: %s) for %s events",
                webhook_info.url, webhook_id, getEventType());
        } catch (hash<ExceptionInfo> ex) {
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
        try {
            return getExampleEventDataFromApi();
        } catch () {
            # Fall back to fake example data if API call fails
        }
        return getFakeExampleData();
    }

    #! Tries to get real example data from the API
    private hash<auto> getExampleEventDataFromApi() {
        # Try to get a real lead to use for example data
        hash<auto> body = {"limit": 1};
        hash<auto> response = rest.post("/leads/list", body);
        if (response.body.typeCode() == NT_HASH && response.body.items.size() > 0) {
            hash<auto> lead = response.body.items[0];
            hash<auto> example = getFakeExampleData();
            if (lead.email) {
                example.lead_email = lead.email;
            }
            if (lead.campaign_id) {
                example.campaign_id = lead.campaign_id;
            }
            if (lead.campaign_name) {
                example.campaign_name = lead.campaign_name;
            }
            return example;
        }
        throw "NO-DATA", "no leads available for example data";
    }

    #! Returns example event data - to be overridden by subclasses
    private hash<auto> getFakeExampleData() {
        return {
            "event_type": getWebhookEventType(),
            "timestamp": now_us().format("YYYY-MM-DDTHH:mm:ss.xxxZ"),
            "campaign_id": "00000000-0000-0000-0000-000000000000",
            "campaign_name": "Example Campaign",
        };
    }

    #! Stops the webhook and cleans up
    private stopEventsIntern() {
        if (webhook_id) {
            try {
                doDelete(sprintf("/webhooks/%s", webhook_id));
                log(LoggerLevel::INFO, "InstantlyBaseEventDataProvider: deleted Instantly webhook %s",
                    webhook_id);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "InstantlyBaseEventDataProvider: error deleting Instantly webhook: %s",
                    get_exception_string(ex));
            }
            remove webhook_id;
        }

        if (webhook_info) {
            DataProvider::DataProviderWebhook::deleteWebhook(webhook_info);
            remove webhook_info;
        }
    }

    #! Handle incoming webhook events from Instantly
    /** @note Instantly's webhook API does not currently support payload signing or signature verification.
        Webhook endpoints should be protected by keeping the URL secret and/or using network-level controls.
    */
    private handleWebhookEvent(hash<auto> payload) {
        string event_type = getEventType();
        log(LoggerLevel::DEBUG, "handleWebhookEvent: received %s event", event_type);

        # Verify the event type matches
        string received_type = payload.event_type ?? "";
        if (received_type && received_type != getWebhookEventType()) {
            log(LoggerLevel::DEBUG, "handleWebhookEvent: ignoring event type %s (expecting %s)",
                received_type, getWebhookEventType());
            return;
        }

        # Transform to standard format
        hash<auto> transformed = {
            "event_type": getWebhookEventType(),
            "timestamp": payload.timestamp ?? now_us().format("YYYY-MM-DDTHH:mm:ss.xxxZ"),
            "campaign_id": payload.campaign_id,
            "campaign_name": payload.campaign_name,
        };

        # Add event-specific fields from payload
        transformed += getEventSpecificFields(payload);

        notifyObservers(event_type, transformed);
    }

    #! Returns event-specific fields from the webhook payload
    /** Can be overridden by subclasses to extract additional fields */
    private hash<auto> getEventSpecificFields(hash<auto> payload) {
        return {};
    }
}

} # namespace InstantlyDataProvider
