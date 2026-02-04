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

    #! Tries to fetch a real order from the API for example data
    /** @param include_items if True, the order will include item details
        @return the most recent order, or NOTHING if none found or on error
    */
    private *hash<auto> tryFetchRealOrder(bool include_items = False) {
        try {
            hash<auto> params = {
                "pageSize": 1,
                "sortBy": "ModifyDate",
                "sortDir": "DESC",
            };
            # If filtering by store, include it in the query
            if (store_id) {
                params.storeId = store_id;
            }

            hash<auto> response = doGet("/orders", params);
            if (response.orders && response.orders.typeCode() == NT_LIST && elements response.orders > 0) {
                return response.orders[0];
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "tryFetchRealOrder: error fetching order: %s: %s", ex.err, ex.desc);
        }
        return NOTHING;
    }

    #! Tries to fetch a real shipment from the API for example data
    /** @param include_items if True, the shipment will include item details
        @return the most recent shipment, or NOTHING if none found or on error
    */
    private *hash<auto> tryFetchRealShipment(bool include_items = False) {
        try {
            hash<auto> params = {
                "pageSize": 1,
                "sortBy": "CreateDate",
                "sortDir": "DESC",
            };
            # If filtering by store, include it in the query
            if (store_id) {
                params.storeId = store_id;
            }
            # Include shipment items if requested
            if (include_items) {
                params.includeShipmentItems = "true";
            }

            hash<auto> response = doGet("/shipments", params);
            if (response.shipments && response.shipments.typeCode() == NT_LIST && elements response.shipments > 0) {
                return response.shipments[0];
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "tryFetchRealShipment: error fetching shipment: %s: %s", ex.err, ex.desc);
        }
        return NOTHING;
    }

    #! Maps an order from the API to order event data format
    /** @param order the order data from the API
        @param event_type the event type (e.g., EVENT_ORDER_NOTIFY)
        @param webhook_type the webhook type (e.g., WEBHOOK_ORDER_NOTIFY)
        @return the mapped event data
    */
    private hash<auto> mapOrderToEventData(hash<auto> order, string event_type, string webhook_type) {
        return {
            "event": event_type,
            "resource_url": sprintf("https://ssapi.shipstation.com/orders?orderNumber=%s",
                encode_url(order.orderNumber ?? "", True)),
            "resource_type": webhook_type,
            "orderId": order.orderId,
            "orderNumber": order.orderNumber,
            "orderKey": order.orderKey,
            "orderDate": order.orderDate,
            "orderStatus": order.orderStatus,
            "customerEmail": order.customerEmail,
            "shipTo": order.shipTo,
            "billTo": order.billTo,
            "orderTotal": order.orderTotal,
            "storeId": order.advancedOptions.storeId ?? order.storeId,
        };
    }

    #! Maps a shipment from the API to ship event data format
    /** @param shipment the shipment data from the API
        @param event_type the event type (e.g., EVENT_SHIP_NOTIFY)
        @param webhook_type the webhook type (e.g., WEBHOOK_SHIP_NOTIFY)
        @return the mapped event data
    */
    private hash<auto> mapShipmentToEventData(hash<auto> shipment, string event_type, string webhook_type) {
        return {
            "event": event_type,
            "resource_url": sprintf("https://ssapi.shipstation.com/shipments?shipmentId=%d",
                shipment.shipmentId ?? 0),
            "resource_type": webhook_type,
            "shipmentId": shipment.shipmentId,
            "orderId": shipment.orderId,
            "orderNumber": shipment.orderNumber,
            "trackingNumber": shipment.trackingNumber,
            "carrierCode": shipment.carrierCode,
            "serviceCode": shipment.serviceCode,
            "shipDate": shipment.shipDate,
            "createDate": shipment.createDate,
            "shipmentCost": shipment.shipmentCost,
            "insuranceCost": shipment.insuranceCost,
            "voided": shipment.voided,
            "shipTo": shipment.shipTo,
            "weight": shipment.weight,
            "dimensions": shipment.dimensions,
        };
    }

    #! Returns fake example data for order events - to be overridden by subclasses
    private hash<auto> getFakeOrderExampleData() {
        return {
            "event": getEventType(),
            "resource_url": "https://ssapi.shipstation.com/orders?orderNumber=TEST-001",
            "resource_type": getWebhookEventType(),
            "orderId": 12345,
            "orderNumber": "TEST-001",
            "orderKey": "unique-key-001",
            "orderDate": "2026-01-15T10:30:00.000Z",
            "orderStatus": "awaiting_shipment",
            "customerEmail": "customer@example.com",
            "shipTo": {
                "name": "John Doe",
                "street1": "123 Main St",
                "city": "Austin",
                "state": "TX",
                "postalCode": "78701",
                "country": "US",
            },
            "orderTotal": 99.99,
            "storeId": 12345,
        };
    }

    #! Returns fake example data for shipment events - to be overridden by subclasses
    private hash<auto> getFakeShipmentExampleData() {
        return {
            "event": getEventType(),
            "resource_url": "https://ssapi.shipstation.com/shipments?shipmentId=12345",
            "resource_type": getWebhookEventType(),
            "shipmentId": 12345,
            "orderId": 54321,
            "orderNumber": "TEST-001",
            "trackingNumber": "1Z999AA10123456784",
            "carrierCode": "ups",
            "serviceCode": "ups_ground",
            "shipDate": "2026-01-15",
            "createDate": "2026-01-15T10:30:00.000Z",
            "shipmentCost": 12.50,
            "insuranceCost": 0.00,
            "voided": False,
            "shipTo": {
                "name": "John Doe",
                "street1": "123 Main St",
                "city": "Austin",
                "state": "TX",
                "postalCode": "78701",
                "country": "US",
            },
            "weight": {
                "value": 2.5,
                "units": "pounds",
            },
            "dimensions": {
                "length": 10,
                "width": 8,
                "height": 4,
                "units": "inches",
            },
        };
    }
}

} # namespace ShipStationDataProvider
