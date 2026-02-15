# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WaveDataProvider module definition

/*  WaveInvoiceEventDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the WaveDataProvider module
public namespace WaveDataProvider {

#! Event ID for new invoices
public const EVENT_INVOICE_CREATED = "invoice-created";

#! Wave invoice event data provider - watches for new invoices via webhooks
public class WaveInvoiceEventDataProvider inherits WaveDataProviderBase, DataProvider::DelayedObservable {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "WaveInvoiceEventDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_observable": True,
        };

        #! Constructor options
        const ConstructorOptions = WaveDataProviderBase::BaseConstructorOptions + {
            "businessId": <DataProviderOptionInfo>{
                "display_name": "Business",
                "short_desc": "Filter events to a specific business",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Only watch for events from this business (optional)",
            },
            "event_type": <DataProviderOptionInfo>{
                "display_name": "Event Type",
                "short_desc": "Which events to watch for",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The type of events to watch for",
                "default_value": EVENT_INVOICE_CREATED,
                "allowed_values": (
                    <AllowedValueInfo>{"value": EVENT_INVOICE_CREATED, "display_name": "Invoice Created",
                        "desc": "Watch for newly created invoices"},
                ),
            },
        };
    }

    private {
        #! Business ID filter
        *string businessId;

        #! Event type to watch for
        string event_type = EVENT_INVOICE_CREATED;

        #! Webhook info returned from DataProviderWebhook::createWebhook()
        *hash<auto> webhook_info;

        #! Wave webhook ID for cleanup
        *string wave_webhook_id;

        #! Invoice event data type
        static WaveInvoiceWebhookEventDataType invoiceEventDataType();
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : WaveDataProviderBase(options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.businessId) {
            businessId = copts.businessId;
        }
        if (copts.event_type) {
            event_type = copts.event_type;
        }
    }

    #! Creates the object with a REST client
    constructor(WaveRestClient::WaveRestClient rest, *hash<auto> options) : WaveDataProviderBase(rest) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.businessId) {
            businessId = copts.businessId;
        }
        if (copts.event_type) {
            event_type = copts.event_type;
        }
    }

    #! Stops observing and deletes the object
    destructor() {
        stopEventsIntern();
    }

    #! Returns the data provider name
    string getName() {
        return event_type;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Wave invoice created event data provider";
    }

    #! Called when all observers have been added to the object
    observersReady() {
        # Create local webhook endpoint
        webhook_info = DataProvider::DataProviderWebhook::createWebhook({
            "method": "POST",
            "callback": \handleWebhookEvent(),
        });

        log(LoggerLevel::INFO, "WaveInvoiceEventDataProvider: webhook endpoint created at %y", webhook_info.url);
        # NOTE: Wave webhook registration requires REST API access to https://api.waveapps.com/webhooks/
        # This would need to be configured manually or through a separate setup step
        # For now, the local webhook is ready to receive events if configured externally
    }

    #! Called to stop generating events
    stopEvents() {
        stopEventsIntern();
    }

    #! Returns a hash of all supported event types
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            event_type: <DataProviderMessageInfo>{
                "desc": "A new invoice has been created in Wave",
                "type": invoiceEventDataType,
            },
        };
    }

    #! Returns example data for the given observable event
    private auto getExampleEventDataImpl(string event_id) {
        # Try to get real data from the API first
        try {
            *string bid = businessId;
            if (!bid) {
                hash<auto> bizData = doGraphQL(ListBusinessesQuery);
                if (bizData.businesses.edges.size() > 0) {
                    bid = bizData.businesses.edges[0].node.id;
                }
            }
            if (bid) {
                hash<auto> data = doGraphQL(ListInvoicesQuery, {"businessId": bid, "pageSize": 1});
                if (data.business.invoices.edges.size() > 0) {
                    return {
                        "business_id": bid,
                        "event": WaveActionCreated,
                        "triggered_at": now_us().format("YYYY-MM-DDTHH:mm:ss.xxxZ"),
                        "data": flattenEdges(data.business.invoices.edges[0].node),
                    };
                }
            }
        } catch () {
            # Fall through to fake data
        }
        return getFakeExampleData();
    }

    #! Returns fake example data when real API data is unavailable
    private hash<auto> getFakeExampleData() {
        return {
            "business_id": "QnVzaW5lc3M6YWJjZGVm",
            "event": WaveActionCreated,
            "triggered_at": "2025-01-24T14:32:18.084Z",
            "data": generateExampleData(new WaveInvoiceDataType().getFields()),
        };
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Stops the webhook and cleans up
    private stopEventsIntern() {
        if (webhook_info) {
            DataProvider::DataProviderWebhook::deleteWebhook(webhook_info);
            remove webhook_info;
        }
    }

    #! Handle incoming webhook events from Wave
    private handleWebhookEvent(hash<auto> payload) {
        # Filter by business if specified
        if (businessId && payload.business_id && payload.business_id != businessId) {
            return;
        }

        # Check event type
        if (payload.event && payload.event != "invoice.created") {
            return;
        }

        log(LoggerLevel::DEBUG, "handleWebhookEvent: invoice.created for invoice %y",
            payload.data.id ?? "<unknown>");
        notifyObservers(event_type, payload);
    }
}

} # namespace WaveDataProvider
