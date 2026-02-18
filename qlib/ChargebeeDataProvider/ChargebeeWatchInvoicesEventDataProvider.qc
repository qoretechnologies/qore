# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ChargebeeDataProvider module definition

/*  ChargebeeWatchInvoicesEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ChargebeeDataProvider module
public namespace ChargebeeDataProvider {

#! Chargebee watch invoices event data provider - watches for new invoice events
public class ChargebeeWatchInvoicesEventDataProvider inherits ChargebeeDataProviderBase,
        DataProvider::DelayedObservable {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "ChargebeeWatchInvoicesEventDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_observable": True,
        };

        #! Constructor options
        const ConstructorOptions = ChargebeeDataProviderBase::BaseConstructorOptions;
    }

    private {
        #! Webhook info returned from DataProviderWebhook::createWebhook()
        *hash<auto> webhook_info;

        #! Event data type
        static ChargebeeEventDataType eventDataType();

        #! The Chargebee event type to filter for
        const ChargebeeEventType = "invoice_generated";
    }

    #! Creates the object with a REST client
    constructor(ChargebeeRestClient::ChargebeeRestClient rest) : ChargebeeDataProviderBase(rest) {
    }

    #! Stops observing and deletes the object
    destructor() {
        stopEventsIntern();
    }

    #! Returns the data provider name
    string getName() {
        return EVENT_WATCH_INVOICES;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Chargebee watch invoices event data provider - triggers when a new invoice is created";
    }

    #! Called when all observers have been added to the object
    observersReady() {
        webhook_info = DataProvider::DataProviderWebhook::createWebhook({
            "method": "POST",
            "callback": \handleWebhookEvent(),
        });
        log(LoggerLevel::INFO, "ChargebeeWatchInvoicesEventDataProvider: webhook endpoint created at %y",
            webhook_info.url);
    }

    #! Called to stop generating events
    stopEvents() {
        stopEventsIntern();
    }

    #! Returns a hash of all supported event types
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_WATCH_INVOICES: <DataProviderMessageInfo>{
                "desc": "A new invoice has been created in Chargebee",
                "type": eventDataType,
            },
        };
    }

    #! Returns example data for the given observable event
    private auto getExampleEventDataImpl(string event_id) {
        try {
            hash<auto> response = doGet("events", {
                "event_type[is]": ChargebeeEventType,
                "limit": "1",
                "sort_by[desc]": "occurred_at",
            });
            if (response.list && response.list.size() > 0) {
                return response.list[0].event;
            }
        } catch () {
            # Fall through to fake data
        }
        return {
            "id": "ev_1234567890",
            "occurred_at": 1706115600,
            "source": "system",
            "event_type": ChargebeeEventType,
            "content": {
                "invoice": {
                    "id": "inv_001",
                    "customer_id": "cust_001",
                    "status": "paid",
                    "total": 9900,
                    "currency_code": "USD",
                },
            },
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

    #! Handle incoming webhook events from Chargebee
    private handleWebhookEvent(hash<auto> payload) {
        *string received_type = payload.event_type;
        if (received_type && received_type != ChargebeeEventType) {
            log(LoggerLevel::DEBUG, "handleWebhookEvent: ignoring event type %y (expecting %y)",
                received_type, ChargebeeEventType);
            return;
        }

        log(LoggerLevel::DEBUG, "handleWebhookEvent: received %s event %y", ChargebeeEventType, payload.id);
        notifyObservers(EVENT_WATCH_INVOICES, payload);
    }
}

} # namespace ChargebeeDataProvider
