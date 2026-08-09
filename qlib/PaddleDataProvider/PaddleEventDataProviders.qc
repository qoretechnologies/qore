# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore PaddleDataProvider module definition

/*  PaddleEventDataProviders.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the PaddleDataProvider module
public namespace PaddleDataProvider {
#! Event ID: Paddle \c "customer.created"
public const EVENT_CUSTOMER_CREATED = "customer-created";

#! Event ID: Paddle \c "customer.updated"
public const EVENT_CUSTOMER_UPDATED = "customer-updated";

#! Event ID: Paddle \c "product.created"
public const EVENT_PRODUCT_CREATED = "product-created";

#! Event ID: Paddle \c "product.updated"
public const EVENT_PRODUCT_UPDATED = "product-updated";

#! Event ID: Paddle \c "price.created"
public const EVENT_PRICE_CREATED = "price-created";

#! Event ID: Paddle \c "price.updated"
public const EVENT_PRICE_UPDATED = "price-updated";

#! Event ID: Paddle \c "subscription.created"
public const EVENT_SUBSCRIPTION_CREATED = "subscription-created";

#! Event ID: Paddle \c "subscription.activated"
public const EVENT_SUBSCRIPTION_ACTIVATED = "subscription-activated";

#! Event ID: Paddle \c "subscription.updated"
public const EVENT_SUBSCRIPTION_UPDATED = "subscription-updated";

#! Event ID: Paddle \c "subscription.canceled"
public const EVENT_SUBSCRIPTION_CANCELED = "subscription-canceled";

#! Event ID: Paddle \c "subscription.past_due"
public const EVENT_SUBSCRIPTION_PAST_DUE = "subscription-past-due";

#! Event ID: Paddle \c "transaction.created"
public const EVENT_TRANSACTION_CREATED = "transaction-created";

#! Event ID: Paddle \c "transaction.completed"
public const EVENT_TRANSACTION_COMPLETED = "transaction-completed";

#! Event ID: Paddle \c "transaction.payment_failed"
public const EVENT_TRANSACTION_PAYMENT_FAILED = "transaction-payment-failed";

#! Event ID: Paddle \c "report.created"
public const EVENT_REPORT_CREATED = "report-created";

#! Event ID: Paddle \c "report.updated"
public const EVENT_REPORT_UPDATED = "report-updated";

#! Reports Paddle \c "customer.created" events
public class PaddleCustomerCreatedEventDataProvider inherits PaddleBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_CUSTOMER_CREATED,
            "type": "PaddleCustomerCreatedEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*PaddleRestClient::PaddleRestClientIo rest) : PaddleBaseEventDataProvider(rest) {
    }

    #! Returns the Paddle event type this provider watches
    string getPaddleEventType() {
        return "customer.created";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_CUSTOMER_CREATED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to a customer being created";
    }

    #! Returns the example entity carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "ctm_01h8b8b8b8b8b8b8b8b8b8b8b8", "status": "active", "name": "Sam Rivera", "email": "sam@example.com", "marketing_consent": False, "locale": "en", "created_at": "2026-08-09T12:00:00.000000Z", "updated_at": "2026-08-09T12:00:00.000000Z"};
    }
}

#! Reports Paddle \c "customer.updated" events
public class PaddleCustomerUpdatedEventDataProvider inherits PaddleBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_CUSTOMER_UPDATED,
            "type": "PaddleCustomerUpdatedEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*PaddleRestClient::PaddleRestClientIo rest) : PaddleBaseEventDataProvider(rest) {
    }

    #! Returns the Paddle event type this provider watches
    string getPaddleEventType() {
        return "customer.updated";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_CUSTOMER_UPDATED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to a customer's details changing";
    }

    #! Returns the example entity carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "ctm_01h8b8b8b8b8b8b8b8b8b8b8b8", "status": "active", "name": "Sam Rivera", "email": "sam.rivera@example.com", "marketing_consent": True, "locale": "en", "created_at": "2026-08-09T12:00:00.000000Z", "updated_at": "2026-08-09T13:30:00.000000Z"};
    }
}

#! Reports Paddle \c "product.created" events
public class PaddleProductCreatedEventDataProvider inherits PaddleBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_PRODUCT_CREATED,
            "type": "PaddleProductCreatedEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*PaddleRestClient::PaddleRestClientIo rest) : PaddleBaseEventDataProvider(rest) {
    }

    #! Returns the Paddle event type this provider watches
    string getPaddleEventType() {
        return "product.created";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_PRODUCT_CREATED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to a catalog product being created";
    }

    #! Returns the example entity carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "pro_01h8b8b8b8b8b8b8b8b8b8b8b8", "name": "Qorus Enterprise", "type": "standard", "tax_category": "saas", "status": "active", "description": "Enterprise plan", "created_at": "2026-08-09T12:00:00.000000Z", "updated_at": "2026-08-09T12:00:00.000000Z"};
    }
}

#! Reports Paddle \c "product.updated" events
public class PaddleProductUpdatedEventDataProvider inherits PaddleBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_PRODUCT_UPDATED,
            "type": "PaddleProductUpdatedEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*PaddleRestClient::PaddleRestClientIo rest) : PaddleBaseEventDataProvider(rest) {
    }

    #! Returns the Paddle event type this provider watches
    string getPaddleEventType() {
        return "product.updated";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_PRODUCT_UPDATED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to a catalog product changing";
    }

    #! Returns the example entity carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "pro_01h8b8b8b8b8b8b8b8b8b8b8b8", "name": "Qorus Enterprise", "type": "standard", "tax_category": "saas", "status": "archived", "description": "Enterprise plan", "created_at": "2026-08-09T12:00:00.000000Z", "updated_at": "2026-08-09T14:00:00.000000Z"};
    }
}

#! Reports Paddle \c "price.created" events
public class PaddlePriceCreatedEventDataProvider inherits PaddleBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_PRICE_CREATED,
            "type": "PaddlePriceCreatedEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*PaddleRestClient::PaddleRestClientIo rest) : PaddleBaseEventDataProvider(rest) {
    }

    #! Returns the Paddle event type this provider watches
    string getPaddleEventType() {
        return "price.created";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_PRICE_CREATED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to a catalog price being created";
    }

    #! Returns the example entity carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "pri_01h8b8b8b8b8b8b8b8b8b8b8b8", "product_id": "pro_01h8b8b8b8b8b8b8b8b8b8b8b8", "description": "Enterprise monthly", "type": "standard", "status": "active", "unit_price": {"amount": "9900", "currency_code": "USD"}, "billing_cycle": {"interval": "month", "frequency": 1}, "created_at": "2026-08-09T12:00:00.000000Z", "updated_at": "2026-08-09T12:00:00.000000Z"};
    }
}

#! Reports Paddle \c "price.updated" events
public class PaddlePriceUpdatedEventDataProvider inherits PaddleBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_PRICE_UPDATED,
            "type": "PaddlePriceUpdatedEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*PaddleRestClient::PaddleRestClientIo rest) : PaddleBaseEventDataProvider(rest) {
    }

    #! Returns the Paddle event type this provider watches
    string getPaddleEventType() {
        return "price.updated";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_PRICE_UPDATED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to a catalog price changing";
    }

    #! Returns the example entity carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "pri_01h8b8b8b8b8b8b8b8b8b8b8b8", "product_id": "pro_01h8b8b8b8b8b8b8b8b8b8b8b8", "description": "Enterprise monthly", "type": "standard", "status": "archived", "unit_price": {"amount": "10900", "currency_code": "USD"}, "billing_cycle": {"interval": "month", "frequency": 1}, "created_at": "2026-08-09T12:00:00.000000Z", "updated_at": "2026-08-09T15:00:00.000000Z"};
    }
}

#! Reports Paddle \c "subscription.created" events
public class PaddleSubscriptionCreatedEventDataProvider inherits PaddleBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_SUBSCRIPTION_CREATED,
            "type": "PaddleSubscriptionCreatedEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*PaddleRestClient::PaddleRestClientIo rest) : PaddleBaseEventDataProvider(rest) {
    }

    #! Returns the Paddle event type this provider watches
    string getPaddleEventType() {
        return "subscription.created";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_SUBSCRIPTION_CREATED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to a subscription being created";
    }

    #! Returns the example entity carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "sub_01h8b8b8b8b8b8b8b8b8b8b8b8", "status": "trialing", "customer_id": "ctm_01h8b8b8b8b8b8b8b8b8b8b8b8", "currency_code": "USD", "collection_mode": "automatic", "started_at": "2026-08-09T12:00:00.000000Z", "next_billed_at": "2026-08-23T12:00:00.000000Z", "created_at": "2026-08-09T12:00:00.000000Z", "updated_at": "2026-08-09T12:00:00.000000Z"};
    }
}

#! Reports Paddle \c "subscription.activated" events
public class PaddleSubscriptionActivatedEventDataProvider inherits PaddleBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_SUBSCRIPTION_ACTIVATED,
            "type": "PaddleSubscriptionActivatedEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*PaddleRestClient::PaddleRestClientIo rest) : PaddleBaseEventDataProvider(rest) {
    }

    #! Returns the Paddle event type this provider watches
    string getPaddleEventType() {
        return "subscription.activated";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_SUBSCRIPTION_ACTIVATED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to a subscription becoming active";
    }

    #! Returns the example entity carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "sub_01h8b8b8b8b8b8b8b8b8b8b8b8", "status": "active", "customer_id": "ctm_01h8b8b8b8b8b8b8b8b8b8b8b8", "currency_code": "USD", "collection_mode": "automatic", "started_at": "2026-08-09T12:00:00.000000Z", "next_billed_at": "2026-09-09T12:00:00.000000Z", "created_at": "2026-08-09T12:00:00.000000Z", "updated_at": "2026-08-23T12:00:00.000000Z"};
    }
}

#! Reports Paddle \c "subscription.updated" events
public class PaddleSubscriptionUpdatedEventDataProvider inherits PaddleBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_SUBSCRIPTION_UPDATED,
            "type": "PaddleSubscriptionUpdatedEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*PaddleRestClient::PaddleRestClientIo rest) : PaddleBaseEventDataProvider(rest) {
    }

    #! Returns the Paddle event type this provider watches
    string getPaddleEventType() {
        return "subscription.updated";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_SUBSCRIPTION_UPDATED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to a subscription changing";
    }

    #! Returns the example entity carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "sub_01h8b8b8b8b8b8b8b8b8b8b8b8", "status": "active", "customer_id": "ctm_01h8b8b8b8b8b8b8b8b8b8b8b8", "currency_code": "USD", "collection_mode": "automatic", "scheduled_change": {"action": "cancel", "effective_at": "2026-09-09T12:00:00.000000Z"}, "next_billed_at": "2026-09-09T12:00:00.000000Z", "created_at": "2026-08-09T12:00:00.000000Z", "updated_at": "2026-08-25T12:00:00.000000Z"};
    }
}

#! Reports Paddle \c "subscription.canceled" events
public class PaddleSubscriptionCanceledEventDataProvider inherits PaddleBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_SUBSCRIPTION_CANCELED,
            "type": "PaddleSubscriptionCanceledEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*PaddleRestClient::PaddleRestClientIo rest) : PaddleBaseEventDataProvider(rest) {
    }

    #! Returns the Paddle event type this provider watches
    string getPaddleEventType() {
        return "subscription.canceled";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_SUBSCRIPTION_CANCELED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to a subscription ending";
    }

    #! Returns the example entity carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "sub_01h8b8b8b8b8b8b8b8b8b8b8b8", "status": "canceled", "customer_id": "ctm_01h8b8b8b8b8b8b8b8b8b8b8b8", "currency_code": "USD", "collection_mode": "automatic", "canceled_at": "2026-09-09T12:00:00.000000Z", "created_at": "2026-08-09T12:00:00.000000Z", "updated_at": "2026-09-09T12:00:00.000000Z"};
    }
}

#! Reports Paddle \c "subscription.past_due" events
public class PaddleSubscriptionPastDueEventDataProvider inherits PaddleBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_SUBSCRIPTION_PAST_DUE,
            "type": "PaddleSubscriptionPastDueEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*PaddleRestClient::PaddleRestClientIo rest) : PaddleBaseEventDataProvider(rest) {
    }

    #! Returns the Paddle event type this provider watches
    string getPaddleEventType() {
        return "subscription.past_due";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_SUBSCRIPTION_PAST_DUE;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to a subscription's payment failing";
    }

    #! Returns the example entity carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "sub_01h8b8b8b8b8b8b8b8b8b8b8b8", "status": "past_due", "customer_id": "ctm_01h8b8b8b8b8b8b8b8b8b8b8b8", "currency_code": "USD", "collection_mode": "automatic", "next_billed_at": "2026-09-09T12:00:00.000000Z", "created_at": "2026-08-09T12:00:00.000000Z", "updated_at": "2026-09-09T12:05:00.000000Z"};
    }
}

#! Reports Paddle \c "transaction.created" events
public class PaddleTransactionCreatedEventDataProvider inherits PaddleBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_TRANSACTION_CREATED,
            "type": "PaddleTransactionCreatedEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*PaddleRestClient::PaddleRestClientIo rest) : PaddleBaseEventDataProvider(rest) {
    }

    #! Returns the Paddle event type this provider watches
    string getPaddleEventType() {
        return "transaction.created";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_TRANSACTION_CREATED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to a transaction being created";
    }

    #! Returns the example entity carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "txn_01h8b8b8b8b8b8b8b8b8b8b8b8", "status": "draft", "customer_id": "ctm_01h8b8b8b8b8b8b8b8b8b8b8b8", "currency_code": "USD", "origin": "subscription_recurring", "collection_mode": "automatic", "subscription_id": "sub_01h8b8b8b8b8b8b8b8b8b8b8b8", "created_at": "2026-09-09T12:00:00.000000Z", "updated_at": "2026-09-09T12:00:00.000000Z"};
    }
}

#! Reports Paddle \c "transaction.completed" events
public class PaddleTransactionCompletedEventDataProvider inherits PaddleBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_TRANSACTION_COMPLETED,
            "type": "PaddleTransactionCompletedEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*PaddleRestClient::PaddleRestClientIo rest) : PaddleBaseEventDataProvider(rest) {
    }

    #! Returns the Paddle event type this provider watches
    string getPaddleEventType() {
        return "transaction.completed";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_TRANSACTION_COMPLETED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to a transaction being paid in full";
    }

    #! Returns the example entity carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "txn_01h8b8b8b8b8b8b8b8b8b8b8b8", "status": "completed", "customer_id": "ctm_01h8b8b8b8b8b8b8b8b8b8b8b8", "currency_code": "USD", "origin": "web", "collection_mode": "automatic", "billed_at": "2026-09-09T12:00:00.000000Z", "details": {"totals": {"subtotal": "9900", "tax": "1980", "total": "11880", "currency_code": "USD"}}, "created_at": "2026-09-09T12:00:00.000000Z", "updated_at": "2026-09-09T12:01:00.000000Z"};
    }
}

#! Reports Paddle \c "transaction.payment_failed" events
public class PaddleTransactionPaymentFailedEventDataProvider inherits PaddleBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_TRANSACTION_PAYMENT_FAILED,
            "type": "PaddleTransactionPaymentFailedEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*PaddleRestClient::PaddleRestClientIo rest) : PaddleBaseEventDataProvider(rest) {
    }

    #! Returns the Paddle event type this provider watches
    string getPaddleEventType() {
        return "transaction.payment_failed";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_TRANSACTION_PAYMENT_FAILED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to a payment attempt failing";
    }

    #! Returns the example entity carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "txn_01h8b8b8b8b8b8b8b8b8b8b8b8", "status": "past_due", "customer_id": "ctm_01h8b8b8b8b8b8b8b8b8b8b8b8", "currency_code": "USD", "origin": "subscription_recurring", "collection_mode": "automatic", "payments": ({"status": "error", "error_code": "declined", "created_at": "2026-09-09T12:00:00.000000Z"},), "created_at": "2026-09-09T12:00:00.000000Z", "updated_at": "2026-09-09T12:00:05.000000Z"};
    }
}

#! Reports Paddle \c "report.created" events
public class PaddleReportCreatedEventDataProvider inherits PaddleBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_REPORT_CREATED,
            "type": "PaddleReportCreatedEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*PaddleRestClient::PaddleRestClientIo rest) : PaddleBaseEventDataProvider(rest) {
    }

    #! Returns the Paddle event type this provider watches
    string getPaddleEventType() {
        return "report.created";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_REPORT_CREATED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to a report being requested";
    }

    #! Returns the example entity carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "rep_01h8b8b8b8b8b8b8b8b8b8b8b8", "status": "pending", "type": "transactions", "rows": 0, "created_at": "2026-08-09T12:00:00.000000Z", "updated_at": "2026-08-09T12:00:00.000000Z"};
    }
}

#! Reports Paddle \c "report.updated" events
public class PaddleReportUpdatedEventDataProvider inherits PaddleBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_REPORT_UPDATED,
            "type": "PaddleReportUpdatedEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*PaddleRestClient::PaddleRestClientIo rest) : PaddleBaseEventDataProvider(rest) {
    }

    #! Returns the Paddle event type this provider watches
    string getPaddleEventType() {
        return "report.updated";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_REPORT_UPDATED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to a report becoming ready";
    }

    #! Returns the example entity carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "rep_01h8b8b8b8b8b8b8b8b8b8b8b8", "status": "ready", "type": "transactions", "rows": 1420, "expires_at": "2026-08-16T12:00:00.000000Z", "created_at": "2026-08-09T12:00:00.000000Z", "updated_at": "2026-08-09T12:03:00.000000Z"};
    }
}

#! The container for the Paddle webhook event providers
/** Event actions name a path under \c "/events", so this provider resolves that path and each event ID
    below it.
*/
public class PaddleEventsDataProvider inherits AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "type": "PaddleEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
        };
    }

    private {
        #! The REST client used for API calls
        *PaddleRestClient::PaddleRestClientIo rest;

        #! The event providers, keyed by event ID
        const ChildMap = {
            EVENT_CUSTOMER_CREATED: Class::forName("PaddleDataProvider::PaddleCustomerCreatedEventDataProvider"),
            EVENT_CUSTOMER_UPDATED: Class::forName("PaddleDataProvider::PaddleCustomerUpdatedEventDataProvider"),
            EVENT_PRODUCT_CREATED: Class::forName("PaddleDataProvider::PaddleProductCreatedEventDataProvider"),
            EVENT_PRODUCT_UPDATED: Class::forName("PaddleDataProvider::PaddleProductUpdatedEventDataProvider"),
            EVENT_PRICE_CREATED: Class::forName("PaddleDataProvider::PaddlePriceCreatedEventDataProvider"),
            EVENT_PRICE_UPDATED: Class::forName("PaddleDataProvider::PaddlePriceUpdatedEventDataProvider"),
            EVENT_SUBSCRIPTION_CREATED: Class::forName("PaddleDataProvider::PaddleSubscriptionCreatedEventDataProvider"),
            EVENT_SUBSCRIPTION_ACTIVATED: Class::forName("PaddleDataProvider::PaddleSubscriptionActivatedEventDataProvider"),
            EVENT_SUBSCRIPTION_UPDATED: Class::forName("PaddleDataProvider::PaddleSubscriptionUpdatedEventDataProvider"),
            EVENT_SUBSCRIPTION_CANCELED: Class::forName("PaddleDataProvider::PaddleSubscriptionCanceledEventDataProvider"),
            EVENT_SUBSCRIPTION_PAST_DUE: Class::forName("PaddleDataProvider::PaddleSubscriptionPastDueEventDataProvider"),
            EVENT_TRANSACTION_CREATED: Class::forName("PaddleDataProvider::PaddleTransactionCreatedEventDataProvider"),
            EVENT_TRANSACTION_COMPLETED: Class::forName("PaddleDataProvider::PaddleTransactionCompletedEventDataProvider"),
            EVENT_TRANSACTION_PAYMENT_FAILED: Class::forName("PaddleDataProvider::PaddleTransactionPaymentFailedEventDataProvider"),
            EVENT_REPORT_CREATED: Class::forName("PaddleDataProvider::PaddleReportCreatedEventDataProvider"),
            EVENT_REPORT_UPDATED: Class::forName("PaddleDataProvider::PaddleReportUpdatedEventDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(*PaddleRestClient::PaddleRestClientIo rest) {
        self.rest = rest;
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Paddle webhook events";
    }

    #! Returns the event IDs
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns the provider reporting the given event, or nothing if the event is unknown
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(rest);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Describes one webhook event the application reports
public hashdecl PaddleEventRegistrationInfo {
    #! The data provider event ID
    string event;

    #! The Paddle event type
    string event_type;

    #! The action display name
    string display_name;

    #! The action short description in plain text
    string short_desc;

    #! The action description with Markdown formatting
    string desc;
}

#! The webhook events the application reports, in registration order
public const PaddleEvents = (
    <PaddleEventRegistrationInfo>{
        "event": EVENT_CUSTOMER_CREATED,
        "event_type": "customer.created",
        "display_name": "Customer Created",
        "short_desc": "React to a customer being created",
        "desc": "Fires when a customer is created, however it happened -- through the API, a checkout, or the Paddle dashboard.\n\nThis replaces polling the customer listing: a poll ordered by creation time cannot see a customer that was created and removed between two runs, and costs a request per interval whether or not anything happened.",
    },
    <PaddleEventRegistrationInfo>{
        "event": EVENT_CUSTOMER_UPDATED,
        "event_type": "customer.updated",
        "display_name": "Customer Updated",
        "short_desc": "React to a customer's details changing",
        "desc": "Fires when a customer changes, including when one is archived -- `status` becomes `archived` rather than the customer being deleted.\n\nPaddle sends the whole customer, not a diff, so compare against what you hold if you need to know which field moved.",
    },
    <PaddleEventRegistrationInfo>{
        "event": EVENT_PRODUCT_CREATED,
        "event_type": "product.created",
        "display_name": "Product Created",
        "short_desc": "React to a catalog product being created",
        "desc": "Fires when a product is added to the catalog.\n\nA product carries no price of its own, so a newly created product cannot be sold until a price exists for it; watch `price.created` for that.\n\nThis replaces polling the product listing.",
    },
    <PaddleEventRegistrationInfo>{
        "event": EVENT_PRODUCT_UPDATED,
        "event_type": "product.updated",
        "display_name": "Product Updated",
        "short_desc": "React to a catalog product changing",
        "desc": "Fires when a product changes, including when one is archived -- `status` becomes `archived`, which withdraws it from sale without affecting existing subscriptions.\n\n**Archiving a product does not archive its prices**, so a price of an archived product can still be billed unless it is archived too.",
    },
    <PaddleEventRegistrationInfo>{
        "event": EVENT_PRICE_CREATED,
        "event_type": "price.created",
        "display_name": "Price Created",
        "short_desc": "React to a catalog price being created",
        "desc": "Fires when a price is created against a product.\n\n`unit_price.amount` is a string in the **smallest unit of its currency**, so `1000` is $10.00 in USD but 1000 yen in JPY. A `billing_cycle` makes it a subscription price; without one it is a one-off charge.",
    },
    <PaddleEventRegistrationInfo>{
        "event": EVENT_PRICE_UPDATED,
        "event_type": "price.updated",
        "display_name": "Price Updated",
        "short_desc": "React to a catalog price changing",
        "desc": "Fires when a price changes, including when one is archived.\n\n**A change to `unit_price` affects new purchases only**; subscriptions already billing this price keep the amount they were created with until they are explicitly updated. A price change is therefore not a signal that anyone's bill changed.",
    },
    <PaddleEventRegistrationInfo>{
        "event": EVENT_SUBSCRIPTION_CREATED,
        "event_type": "subscription.created",
        "display_name": "Subscription Created",
        "short_desc": "React to a subscription being created",
        "desc": "Fires when a subscription is created.\n\n**Created is not the same as paid.** A new subscription is often `trialing`, and one created for a transaction that has not been collected yet is not earning anything; check `status` before provisioning. `subscription.activated` is the signal that billing has actually begun.\n\nThis replaces polling the subscription listing.",
    },
    <PaddleEventRegistrationInfo>{
        "event": EVENT_SUBSCRIPTION_ACTIVATED,
        "event_type": "subscription.activated",
        "display_name": "Subscription Activated",
        "short_desc": "React to a subscription becoming active",
        "desc": "Fires when a subscription first becomes `active` -- a trial ended and was paid for, or a scheduled start arrived.\n\nThis is the signal to provision a paid plan: it means Paddle has collected, where `subscription.created` only means the record exists.",
    },
    <PaddleEventRegistrationInfo>{
        "event": EVENT_SUBSCRIPTION_UPDATED,
        "event_type": "subscription.updated",
        "display_name": "Subscription Updated",
        "short_desc": "React to a subscription changing",
        "desc": "Fires for any change to a subscription: a plan change, a quantity change, a renewal moving `next_billed_at`, or a pause or cancellation being **scheduled**.\n\n`scheduled_change` is the field that distinguishes those last cases -- a subscription can be `active` and still be booked to end. Paddle sends the whole subscription rather than a diff.",
    },
    <PaddleEventRegistrationInfo>{
        "event": EVENT_SUBSCRIPTION_CANCELED,
        "event_type": "subscription.canceled",
        "display_name": "Subscription Canceled",
        "short_desc": "React to a subscription ending",
        "desc": "Fires when a subscription actually ends and `status` becomes `canceled`.\n\n**This is the deprovisioning signal, not the cancellation request.** A cancellation booked for the end of the billing period appears first as a `scheduled_change` on `subscription.updated`, and the customer has paid up to that date; this event is when the date arrives.",
    },
    <PaddleEventRegistrationInfo>{
        "event": EVENT_SUBSCRIPTION_PAST_DUE,
        "event_type": "subscription.past_due",
        "display_name": "Subscription Past Due",
        "short_desc": "React to a subscription's payment failing",
        "desc": "Fires when a renewal could not be collected and the subscription becomes `past_due`.\n\nPaddle retries on its dunning schedule, so this is not yet the end: the subscription recovers to `active` if a retry succeeds, or is canceled when the retries run out. Treat it as a warning to prompt the customer, not as a reason to cut off access.",
    },
    <PaddleEventRegistrationInfo>{
        "event": EVENT_TRANSACTION_CREATED,
        "event_type": "transaction.created",
        "display_name": "Transaction Created",
        "short_desc": "React to a transaction being created",
        "desc": "Fires when a transaction is created, including the draft Paddle raises for an upcoming renewal.\n\n**A created transaction has not been paid**; most are `draft` or `ready` at this point. Use `transaction.completed` for money actually collected.\n\nThis replaces polling the transaction listing.",
    },
    <PaddleEventRegistrationInfo>{
        "event": EVENT_TRANSACTION_COMPLETED,
        "event_type": "transaction.completed",
        "display_name": "Transaction Completed",
        "short_desc": "React to a transaction being paid in full",
        "desc": "Fires when a transaction is `completed`: the money has been collected and the invoice is settled.\n\nThis is the event to fulfil an order on. `details.totals` breaks the amount down into subtotal, tax and the grand total, all as strings in the smallest unit of `currency_code`.",
    },
    <PaddleEventRegistrationInfo>{
        "event": EVENT_TRANSACTION_PAYMENT_FAILED,
        "event_type": "transaction.payment_failed",
        "display_name": "Transaction Payment Failed",
        "short_desc": "React to a payment attempt failing",
        "desc": "Fires when an attempt to collect a transaction fails.\n\n`payments` carries the attempt and the reason it failed. Paddle retries automatically for a subscription renewal, so a single failure is not final; the subscription moving to `past_due` is the point at which the customer needs to act.",
    },
    <PaddleEventRegistrationInfo>{
        "event": EVENT_REPORT_CREATED,
        "event_type": "report.created",
        "display_name": "Report Created",
        "short_desc": "React to a report being requested",
        "desc": "Fires when a report is requested. It arrives `pending`: Paddle generates the file in the background, so there is nothing to download yet.\n\nWait for `report.updated` with `status` of `ready` before asking for the file.\n\nThis replaces polling the report listing.",
    },
    <PaddleEventRegistrationInfo>{
        "event": EVENT_REPORT_UPDATED,
        "event_type": "report.updated",
        "display_name": "Report Updated",
        "short_desc": "React to a report becoming ready",
        "desc": "Fires when a report's status changes: `ready` when the file can be downloaded, `failed` when generation did not succeed, `expired` once Paddle has removed it.\n\nThis is the signal to call **Get Report File** -- and to do it promptly, because a report is kept only for a limited time.",
    },
);
}
