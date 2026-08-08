# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore StripeDataProvider module definition

/*  StripeEventDataProviders.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the StripeDataProvider module
public namespace StripeDataProvider {
#! Event ID: a charge succeeding
public const EVENT_CHARGE_SUCCEEDED = "charge-succeeded";

#! Event ID: a charge being refunded
public const EVENT_CHARGE_REFUNDED = "charge-refunded";

#! Event ID: a customer disputing a charge
public const EVENT_CHARGE_DISPUTE_CREATED = "charge-dispute-created";

#! Event ID: a customer completing Stripe Checkout
public const EVENT_CHECKOUT_SESSION_COMPLETED = "checkout-session-completed";

#! Event ID: a customer being created
public const EVENT_CUSTOMER_CREATED = "customer-created";

#! Event ID: an invoice being created
public const EVENT_INVOICE_CREATED = "invoice-created";

#! Event ID: an invoice payment failing
public const EVENT_INVOICE_PAYMENT_FAILED = "invoice-payment-failed";

#! Event ID: a payment succeeding
public const EVENT_PAYMENT_INTENT_SUCCEEDED = "payment-intent-succeeded";

#! Event ID: a payment failing
public const EVENT_PAYMENT_INTENT_FAILED = "payment-intent-payment-failed";

#! Event ID: a payment link being created
public const EVENT_PAYMENT_LINK_CREATED = "payment-link-created";

#! Event ID: a subscription being created
public const EVENT_SUBSCRIPTION_CREATED = "customer-subscription-created";

#! Event ID: a subscription changing
public const EVENT_SUBSCRIPTION_UPDATED = "customer-subscription-updated";

#! Event ID: a subscription ending
public const EVENT_SUBSCRIPTION_CANCELED = "customer-subscription-deleted";

#! Reports Stripe \c "charge.succeeded" events
public class StripeChargeSucceededEventDataProvider inherits StripeBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_CHARGE_SUCCEEDED,
            "type": "StripeChargeSucceededEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*StripeRestClient::StripeRestClientIo rest) : StripeBaseEventDataProvider(rest) {
    }

    #! Returns the Stripe event type this provider watches
    string getStripeEventType() {
        return "charge.succeeded";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_CHARGE_SUCCEEDED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to a charge succeeding";
    }

    #! Returns the example object carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "ch_00000000000000", "object": "charge", "amount": 2000, "currency": "usd", "paid": True, "status": "succeeded", "customer": "cus_00000000000000", "payment_intent": "pi_00000000000000", "created": 1709294400};
    }
}

#! Reports Stripe \c "charge.refunded" events
public class StripeChargeRefundedEventDataProvider inherits StripeBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_CHARGE_REFUNDED,
            "type": "StripeChargeRefundedEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*StripeRestClient::StripeRestClientIo rest) : StripeBaseEventDataProvider(rest) {
    }

    #! Returns the Stripe event type this provider watches
    string getStripeEventType() {
        return "charge.refunded";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_CHARGE_REFUNDED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to a charge being refunded";
    }

    #! Returns the example object carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "ch_00000000000000", "object": "charge", "amount": 2000, "amount_refunded": 2000, "currency": "usd", "refunded": True, "status": "succeeded", "created": 1709294400};
    }
}

#! Reports Stripe \c "charge.dispute.created" events
public class StripeChargeDisputeCreatedEventDataProvider inherits StripeBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_CHARGE_DISPUTE_CREATED,
            "type": "StripeChargeDisputeCreatedEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*StripeRestClient::StripeRestClientIo rest) : StripeBaseEventDataProvider(rest) {
    }

    #! Returns the Stripe event type this provider watches
    string getStripeEventType() {
        return "charge.dispute.created";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_CHARGE_DISPUTE_CREATED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to a customer disputing a charge";
    }

    #! Returns the example object carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "dp_00000000000000", "object": "dispute", "amount": 2000, "currency": "usd", "charge": "ch_00000000000000", "reason": "fraudulent", "status": "warning_needs_response", "created": 1709294400};
    }
}

#! Reports Stripe \c "checkout.session.completed" events
public class StripeCheckoutSessionCompletedEventDataProvider inherits StripeBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_CHECKOUT_SESSION_COMPLETED,
            "type": "StripeCheckoutSessionCompletedEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*StripeRestClient::StripeRestClientIo rest) : StripeBaseEventDataProvider(rest) {
    }

    #! Returns the Stripe event type this provider watches
    string getStripeEventType() {
        return "checkout.session.completed";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_CHECKOUT_SESSION_COMPLETED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to a customer completing Stripe Checkout";
    }

    #! Returns the example object carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "cs_00000000000000", "object": "checkout.session", "amount_total": 2000, "currency": "usd", "customer": "cus_00000000000000", "payment_status": "paid", "status": "complete", "mode": "payment"};
    }
}

#! Reports Stripe \c "customer.created" events
public class StripeCustomerCreatedEventDataProvider inherits StripeBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_CUSTOMER_CREATED,
            "type": "StripeCustomerCreatedEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*StripeRestClient::StripeRestClientIo rest) : StripeBaseEventDataProvider(rest) {
    }

    #! Returns the Stripe event type this provider watches
    string getStripeEventType() {
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

    #! Returns the example object carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "cus_00000000000000", "object": "customer", "email": "customer@example.com", "name": "Example Customer", "created": 1709294400, "livemode": False};
    }
}

#! Reports Stripe \c "invoice.created" events
public class StripeInvoiceCreatedEventDataProvider inherits StripeBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_INVOICE_CREATED,
            "type": "StripeInvoiceCreatedEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*StripeRestClient::StripeRestClientIo rest) : StripeBaseEventDataProvider(rest) {
    }

    #! Returns the Stripe event type this provider watches
    string getStripeEventType() {
        return "invoice.created";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_INVOICE_CREATED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to an invoice being created";
    }

    #! Returns the example object carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "in_00000000000000", "object": "invoice", "customer": "cus_00000000000000", "status": "draft", "total": 2000, "currency": "usd", "created": 1709294400};
    }
}

#! Reports Stripe \c "invoice.payment_failed" events
public class StripeInvoicePaymentFailedEventDataProvider inherits StripeBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_INVOICE_PAYMENT_FAILED,
            "type": "StripeInvoicePaymentFailedEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*StripeRestClient::StripeRestClientIo rest) : StripeBaseEventDataProvider(rest) {
    }

    #! Returns the Stripe event type this provider watches
    string getStripeEventType() {
        return "invoice.payment_failed";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_INVOICE_PAYMENT_FAILED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to an invoice payment failing";
    }

    #! Returns the example object carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "in_00000000000000", "object": "invoice", "customer": "cus_00000000000000", "status": "open", "attempt_count": 1, "total": 2000, "currency": "usd", "next_payment_attempt": 1709380800};
    }
}

#! Reports Stripe \c "payment_intent.succeeded" events
public class StripePaymentIntentSucceededEventDataProvider inherits StripeBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_PAYMENT_INTENT_SUCCEEDED,
            "type": "StripePaymentIntentSucceededEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*StripeRestClient::StripeRestClientIo rest) : StripeBaseEventDataProvider(rest) {
    }

    #! Returns the Stripe event type this provider watches
    string getStripeEventType() {
        return "payment_intent.succeeded";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_PAYMENT_INTENT_SUCCEEDED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to a payment succeeding";
    }

    #! Returns the example object carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "pi_00000000000000", "object": "payment_intent", "amount": 2000, "amount_received": 2000, "currency": "usd", "status": "succeeded", "customer": "cus_00000000000000", "created": 1709294400};
    }
}

#! Reports Stripe \c "payment_intent.payment_failed" events
public class StripePaymentIntentFailedEventDataProvider inherits StripeBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_PAYMENT_INTENT_FAILED,
            "type": "StripePaymentIntentFailedEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*StripeRestClient::StripeRestClientIo rest) : StripeBaseEventDataProvider(rest) {
    }

    #! Returns the Stripe event type this provider watches
    string getStripeEventType() {
        return "payment_intent.payment_failed";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_PAYMENT_INTENT_FAILED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to a payment failing";
    }

    #! Returns the example object carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "pi_00000000000000", "object": "payment_intent", "amount": 2000, "currency": "usd", "status": "requires_payment_method", "last_payment_error": {"code": "card_declined", "message": "Your card was declined."}};
    }
}

#! Reports Stripe \c "payment_link.created" events
public class StripePaymentLinkCreatedEventDataProvider inherits StripeBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_PAYMENT_LINK_CREATED,
            "type": "StripePaymentLinkCreatedEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*StripeRestClient::StripeRestClientIo rest) : StripeBaseEventDataProvider(rest) {
    }

    #! Returns the Stripe event type this provider watches
    string getStripeEventType() {
        return "payment_link.created";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_PAYMENT_LINK_CREATED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to a payment link being created";
    }

    #! Returns the example object carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "plink_00000000000000", "object": "payment_link", "active": True, "url": "https://buy.stripe.com/test_00000000000000", "livemode": False};
    }
}

#! Reports Stripe \c "customer.subscription.created" events
public class StripeSubscriptionCreatedEventDataProvider inherits StripeBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_SUBSCRIPTION_CREATED,
            "type": "StripeSubscriptionCreatedEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*StripeRestClient::StripeRestClientIo rest) : StripeBaseEventDataProvider(rest) {
    }

    #! Returns the Stripe event type this provider watches
    string getStripeEventType() {
        return "customer.subscription.created";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_SUBSCRIPTION_CREATED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to a subscription being created";
    }

    #! Returns the example object carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "sub_00000000000000", "object": "subscription", "customer": "cus_00000000000000", "status": "active", "current_period_start": 1709294400, "current_period_end": 1711886400, "cancel_at_period_end": False};
    }
}

#! Reports Stripe \c "customer.subscription.updated" events
public class StripeSubscriptionUpdatedEventDataProvider inherits StripeBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_SUBSCRIPTION_UPDATED,
            "type": "StripeSubscriptionUpdatedEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*StripeRestClient::StripeRestClientIo rest) : StripeBaseEventDataProvider(rest) {
    }

    #! Returns the Stripe event type this provider watches
    string getStripeEventType() {
        return "customer.subscription.updated";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_SUBSCRIPTION_UPDATED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to a subscription changing";
    }

    #! Returns the example object carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "sub_00000000000000", "object": "subscription", "customer": "cus_00000000000000", "status": "past_due", "current_period_end": 1711886400, "cancel_at_period_end": False};
    }
}

#! Reports Stripe \c "customer.subscription.deleted" events
public class StripeSubscriptionCanceledEventDataProvider inherits StripeBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_SUBSCRIPTION_CANCELED,
            "type": "StripeSubscriptionCanceledEventDataProvider",
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    #! Creates the object with a REST client
    constructor(*StripeRestClient::StripeRestClientIo rest) : StripeBaseEventDataProvider(rest) {
    }

    #! Returns the Stripe event type this provider watches
    string getStripeEventType() {
        return "customer.subscription.deleted";
    }

    #! Returns the data provider event ID
    string getEventId() {
        return EVENT_SUBSCRIPTION_CANCELED;
    }

    #! Returns a description of what the event means
    string getEventDesc() {
        return "React to a subscription ending";
    }

    #! Returns the example object carried by this event type
    hash<auto> getExampleObject() {
        return {"id": "sub_00000000000000", "object": "subscription", "customer": "cus_00000000000000", "status": "canceled", "canceled_at": 1709294400, "ended_at": 1709294400};
    }
}

#! Events container data provider
/** Event actions name a data provider path under \c "/events", so the application's root resolves that
    path to this provider, and this provider resolves each event ID to the provider that reports it.
*/
public class StripeEventsDataProvider inherits AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "type": "StripeEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
        };
    }

    private {
        #! The REST client used for API calls
        *StripeRestClient::StripeRestClientIo rest;

        #! The event providers, keyed by event ID
        const ChildMap = {
            EVENT_CHARGE_SUCCEEDED: Class::forName("StripeDataProvider::StripeChargeSucceededEventDataProvider"),
            EVENT_CHARGE_REFUNDED: Class::forName("StripeDataProvider::StripeChargeRefundedEventDataProvider"),
            EVENT_CHARGE_DISPUTE_CREATED: Class::forName("StripeDataProvider::StripeChargeDisputeCreatedEventDataProvider"),
            EVENT_CHECKOUT_SESSION_COMPLETED: Class::forName("StripeDataProvider::StripeCheckoutSessionCompletedEventDataProvider"),
            EVENT_CUSTOMER_CREATED: Class::forName("StripeDataProvider::StripeCustomerCreatedEventDataProvider"),
            EVENT_INVOICE_CREATED: Class::forName("StripeDataProvider::StripeInvoiceCreatedEventDataProvider"),
            EVENT_INVOICE_PAYMENT_FAILED: Class::forName("StripeDataProvider::StripeInvoicePaymentFailedEventDataProvider"),
            EVENT_PAYMENT_INTENT_SUCCEEDED: Class::forName("StripeDataProvider::StripePaymentIntentSucceededEventDataProvider"),
            EVENT_PAYMENT_INTENT_FAILED: Class::forName("StripeDataProvider::StripePaymentIntentFailedEventDataProvider"),
            EVENT_PAYMENT_LINK_CREATED: Class::forName("StripeDataProvider::StripePaymentLinkCreatedEventDataProvider"),
            EVENT_SUBSCRIPTION_CREATED: Class::forName("StripeDataProvider::StripeSubscriptionCreatedEventDataProvider"),
            EVENT_SUBSCRIPTION_UPDATED: Class::forName("StripeDataProvider::StripeSubscriptionUpdatedEventDataProvider"),
            EVENT_SUBSCRIPTION_CANCELED: Class::forName("StripeDataProvider::StripeSubscriptionCanceledEventDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(*StripeRestClient::StripeRestClientIo rest) {
        self.rest = rest;
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Stripe webhook events";
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
}
