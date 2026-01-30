# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationBaseEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShipStationDataProvider module
public namespace ShipStationDataProvider {

#! Base class for ShipStation event data providers
/** This abstract class provides common functionality for all ShipStation webhook-based event providers.

    ShipStation webhooks are registered via POST /webhooks/subscribe and deliver events via HTTP POST.
    The webhook payload contains a resource_url that must be fetched to get the full data.
*/
public class ShipStationBaseEventDataProvider inherits ShipStationDataProviderBase, DataProvider::DelayedObservable {
    public {
        #! Local options (event-specific options)
        const LocalOptions = {
            "store_id": <DataProviderOptionInfo>{
                "display_name": "Store ID",
                "short_desc": "The store to watch for events",
                "type": AbstractDataProviderTypeMap."*int",
                "desc": "The ShipStation store ID to monitor for events. If not specified, watches all stores.",
                "ref_data": "stores",
            },
        };
    }

    private {
        #! Store ID to filter events (optional)
        *int store_id;

        #! Webhook info returned from DataProviderWebhook::createWebhook()
        *hash<auto> webhook_info;

        #! ShipStation webhook ID (returned when subscribing)
        *int webhook_id;
    }

    #! Creates the object with a REST client
    constructor(ShipStationRestClient::ShipStationRestClient rest, *hash<auto> options)
            : ShipStationDataProviderBase(rest) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", LocalOptions, options);
        if (copts.store_id) {
            store_id = copts.store_id.value ?? copts.store_id;
        }
    }

    #! Stops observing and deletes the object
    destructor() {
        stopEventsIntern();
    }

    #! Returns the event type that this provider handles
    /** Must be overridden by subclasses */
    abstract string getEventType();

    #! Returns the ShipStation webhook event type (e.g., ORDER_NOTIFY)
    /** Must be overridden by subclasses */
    abstract string getWebhookEventType();

    #! Returns the event data type
    /** Must be overridden by subclasses */
    abstract AbstractDataProviderType getEventDataType();

    #! Called when all observers have been added to the object
    observersReady() {
        # Get store_id from context if not provided
        if (!exists store_id) {
            *hash<auto> ctx = DataProvider::DataProviderDataContextHelper::getHash();
            if (ctx.store_id) {
                store_id = ctx.store_id;
            }
        }

        # Create local webhook endpoint
        webhook_info = DataProvider::DataProviderWebhook::createWebhook({
            "method": "POST",
            "callback": \handleWebhookEvent(),
        });

        # Register webhook with ShipStation
        try {
            hash<auto> body = {
                "target_url": webhook_info.url,
                "event": getWebhookEventType(),
                "friendly_name": sprintf("Qore %s webhook", getEventType()),
            };

            # Optionally filter by store
            if (store_id) {
                body.store_id = store_id;
            }

            hash<auto> response = doPost("/webhooks/subscribe", body);

            # ShipStation returns the webhook ID
            webhook_id = response.id;

            log(LoggerLevel::INFO, "ShipStationBaseEventDataProvider: registered webhook %y (ID: %d) for %s events",
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
        return getExampleEventData();
    }

    #! Returns example event data - to be overridden by subclasses
    private hash<auto> getExampleEventData() {
        return {
            "event": getEventType(),
            "resource_url": "https://ssapi.shipstation.com/orders?orderNumber=TEST-001",
            "resource_type": getWebhookEventType(),
        };
    }

    #! Stops the webhook and cleans up
    private stopEventsIntern() {
        # Delete webhook from ShipStation first
        if (webhook_id) {
            try {
                doDelete(sprintf("/webhooks/%d", webhook_id));
                log(LoggerLevel::INFO, "ShipStationBaseEventDataProvider: deleted ShipStation webhook %d",
                    webhook_id);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "ShipStationBaseEventDataProvider: error deleting ShipStation webhook: %s",
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

    #! Handle incoming webhook events from ShipStation
    private handleWebhookEvent(hash<auto> payload) {
        string event_type = getEventType();
        log(LoggerLevel::DEBUG, "handleWebhookEvent: received %s event", event_type);

        # ShipStation webhook payload structure:
        # {
        #   "resource_url": "https://ssapi.shipstation.com/...",
        #   "resource_type": "ORDER_NOTIFY|ITEM_ORDER_NOTIFY|SHIP_NOTIFY|ITEM_SHIP_NOTIFY"
        # }

        # Verify the event type matches
        string received_type = payload.resource_type ?? "";
        if (received_type && received_type != getWebhookEventType()) {
            log(LoggerLevel::DEBUG, "handleWebhookEvent: ignoring event type %s (expecting %s)",
                received_type, getWebhookEventType());
            return;
        }

        # Fetch full data from resource_url
        hash<auto> event_data;
        try {
            event_data = fetchResourceData(payload.resource_url);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "handleWebhookEvent: error fetching resource data: %s",
                get_exception_string(ex));
            return;
        }

        # Transform to standard format
        hash<auto> transformed = {
            "event": event_type,
            "resource_url": payload.resource_url,
            "resource_type": payload.resource_type,
        };

        # Add event-specific fields
        transformed += getEventSpecificFields(event_data);

        notifyObservers(event_type, transformed);
    }

    #! Fetches the full resource data from the resource_url
    private hash<auto> fetchResourceData(string resource_url) {
        # The resource_url is a full URL to the ShipStation API
        # We can use our REST client to fetch it since it has the auth credentials
        # But we need to extract the path from the URL

        # Parse the URL to get the path
        hash<auto> url_info = parse_url(resource_url);
        string path = url_info.path ?? "";
        if (url_info.query) {
            path += "?" + url_info.query;
        }

        log(LoggerLevel::DEBUG, "fetchResourceData: fetching %s", path);
        hash<auto> response = doGet(path);

        # ShipStation returns different structures depending on the endpoint
        # For orders, it might be a single order or a list
        if (response.orders && response.orders.typeCode() == NT_LIST && elements response.orders > 0) {
            return response.orders[0];
        } else if (response.shipments && response.shipments.typeCode() == NT_LIST && elements response.shipments > 0) {
            return response.shipments[0];
        }

        return response;
    }

    #! Returns event-specific fields from the fetched data
    /** Can be overridden by subclasses to extract additional fields */
    private hash<auto> getEventSpecificFields(hash<auto> event_data) {
        return {};
    }
}

} # namespace ShipStationDataProvider
