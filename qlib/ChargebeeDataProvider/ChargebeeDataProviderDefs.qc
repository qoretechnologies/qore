# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ChargebeeDataProvider module definition

/*  ChargebeeDataProviderDefs.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Chargebee app name constant
public const AppName = "Chargebee";

#! Default page size for pagination
public const DefaultPageSize = 100;

#! Maximum page size for Chargebee API
public const MaxPageSize = 100;

#! Event type for watching customer changes
public const EVENT_WATCH_CUSTOMERS = "watch-customers";

#! Event type for watching all events
public const EVENT_WATCH_EVENTS = "watch-events";

#! Event type for watching invoice changes
public const EVENT_WATCH_INVOICES = "watch-invoices";

#! Event type for watching subscription changes
public const EVENT_WATCH_SUBSCRIPTIONS = "watch-subscriptions";

#! All supported event types
public const AllEventTypes = (
    EVENT_WATCH_CUSTOMERS,
    EVENT_WATCH_EVENTS,
    EVENT_WATCH_INVOICES,
    EVENT_WATCH_SUBSCRIPTIONS,
);

#! Customer status: active
public const CUSTOMER_STATUS_ACTIVE = "active";

#! Subscription status: future
public const SUBSCRIPTION_STATUS_FUTURE = "future";

#! Subscription status: in trial
public const SUBSCRIPTION_STATUS_IN_TRIAL = "in_trial";

#! Subscription status: active
public const SUBSCRIPTION_STATUS_ACTIVE = "active";

#! Subscription status: non-renewing
public const SUBSCRIPTION_STATUS_NON_RENEWING = "non_renewing";

#! Subscription status: paused
public const SUBSCRIPTION_STATUS_PAUSED = "paused";

#! Subscription status: cancelled
public const SUBSCRIPTION_STATUS_CANCELLED = "cancelled";

#! Subscription status: transferred
public const SUBSCRIPTION_STATUS_TRANSFERRED = "transferred";

#! All supported subscription statuses
public const AllSubscriptionStatuses = (
    SUBSCRIPTION_STATUS_FUTURE,
    SUBSCRIPTION_STATUS_IN_TRIAL,
    SUBSCRIPTION_STATUS_ACTIVE,
    SUBSCRIPTION_STATUS_NON_RENEWING,
    SUBSCRIPTION_STATUS_PAUSED,
    SUBSCRIPTION_STATUS_CANCELLED,
    SUBSCRIPTION_STATUS_TRANSFERRED,
);

#! Invoice status: paid
public const INVOICE_STATUS_PAID = "paid";

#! Invoice status: posted
public const INVOICE_STATUS_POSTED = "posted";

#! Invoice status: payment due
public const INVOICE_STATUS_PAYMENT_DUE = "payment_due";

#! Invoice status: not paid
public const INVOICE_STATUS_NOT_PAID = "not_paid";

#! Invoice status: voided
public const INVOICE_STATUS_VOIDED = "voided";

#! Invoice status: pending
public const INVOICE_STATUS_PENDING = "pending";

#! All supported invoice statuses
public const AllInvoiceStatuses = (
    INVOICE_STATUS_PAID,
    INVOICE_STATUS_POSTED,
    INVOICE_STATUS_PAYMENT_DUE,
    INVOICE_STATUS_NOT_PAID,
    INVOICE_STATUS_VOIDED,
    INVOICE_STATUS_PENDING,
);

#! Chargebee logo in SVG format (loaded from file)
public const ChargebeeLogo = File::readTextFile(get_script_dir() + "/chargebee-logo.svg");

} # namespace ChargebeeDataProvider
