# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceBaseEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the WooCommerceDataProvider module
public namespace WooCommerceDataProvider {

#! Base class for WooCommerce built-in webhook event data providers
/** This abstract class provides common functionality for all WooCommerce webhook-based event providers.

    WooCommerce webhooks are registered via POST /webhooks and deliver events via HTTP POST.
    For built-in topics (e.g., order.created), the webhook payload contains the full resource JSON.
*/
public class WooCommerceBaseEventDataProvider inherits WooCommerceDataProviderBase, DataProvider::DelayedObservable {
    public {
        #! Constructor options for event data providers
        const ConstructorOptions = WooCommerceDataProviderBase::BaseConstructorOptions;
    }

    private {
        #! Webhook info returned from DataProviderWebhook::createWebhook()
        *hash<auto> webhook_info;

        #! WooCommerce webhook ID (returned when creating the webhook)
        *int webhook_id;

        #! Webhook secret for verifying deliveries
        string webhook_secret;
    }

    #! Creates the object with a REST client
    constructor(WooCommerceRestClient::WooCommerceRestClient rest) : WooCommerceDataProviderBase(rest) {
        webhook_secret = get_random_string(32);
    }

    #! Creates the object with a connection
    constructor(WooCommerceRestClient::WooCommerceRestConnection conn) : WooCommerceDataProviderBase(conn) {
        webhook_secret = get_random_string(32);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : WooCommerceDataProviderBase(options) {
        webhook_secret = get_random_string(32);
    }

    #! Stops observing and deletes the object
    destructor() {
        stopEventsIntern();
    }

    #! Returns the event type that this provider handles
    /** Must be overridden by subclasses */
    abstract string getEventType();

    #! Returns the WooCommerce webhook topic (e.g., "order.created")
    /** Must be overridden by subclasses */
    abstract string getWebhookTopic();

    #! Returns the event data type
    /** Must be overridden by subclasses */
    abstract AbstractDataProviderType getEventDataType();

    #! Returns the provider name
    string getName() {
        return getEventType();
    }

    #! Called when all observers have been added to the object
    observersReady() {
        # Create local webhook endpoint
        webhook_info = DataProvider::DataProviderWebhook::createWebhook({
            "method": "POST",
            "callback": \handleWebhookEvent(),
        });

        # Register webhook with WooCommerce
        try {
            hash<auto> response = doPost("webhooks", {
                "name": sprintf("Qorus %s webhook", getEventType()),
                "topic": getWebhookTopic(),
                "delivery_url": webhook_info.url,
                "secret": webhook_secret,
                "status": "active",
            });
            webhook_id = response.id;

            log(LoggerLevel::INFO, "WooCommerceBaseEventDataProvider: registered webhook %y (ID: %d) for %s events",
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
                "desc": sprintf("Fired when a %s event occurs in WooCommerce", event_type),
                "type": getEventDataType(),
            },
        };
    }

    #! Returns the API resource path for fetching example data (e.g., "orders", "products")
    /** Must be overridden by subclasses */
    abstract string getResourcePath();

    #! Returns example event data for UI preview
    private auto getExampleEventDataImpl(string event_id) {
        try {
            auto response = doGet(getResourcePath(), {"per_page": "1"});
            if (response.typeCode() == NT_LIST && response.size() > 0) {
                return response[0];
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getExampleEventDataImpl: error fetching example data: %s", ex.desc);
        }
        return getFakeExampleData();
    }

    #! Returns fake example data for when the API is not available
    /** Must be overridden by subclasses to return resource-specific fake data */
    abstract hash<auto> getFakeExampleData();

    #! Handle incoming webhook events from WooCommerce
    /** For built-in topics, the payload IS the full resource JSON */
    private handleWebhookEvent(hash<auto> payload) {
        string event_type = getEventType();
        log(LoggerLevel::DEBUG, "handleWebhookEvent: received %s event", event_type);
        notifyObservers(event_type, payload);
    }

    #! Stops the webhook and cleans up
    private stopEventsIntern() {
        # Delete webhook from WooCommerce first
        if (webhook_id) {
            try {
                doDelete(sprintf("webhooks/%d", webhook_id), {"force": "true"});
                log(LoggerLevel::INFO, "WooCommerceBaseEventDataProvider: deleted WooCommerce webhook %d",
                    webhook_id);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "WooCommerceBaseEventDataProvider: error deleting webhook %d: %s",
                    webhook_id, get_exception_string(ex));
            }
            remove webhook_id;
        }

        # Delete local webhook endpoint
        if (webhook_info) {
            DataProvider::DataProviderWebhook::deleteWebhook(webhook_info);
            remove webhook_info;
        }
    }
}

} # namespace WooCommerceDataProvider
