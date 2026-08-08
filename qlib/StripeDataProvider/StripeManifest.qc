# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore StripeDataProvider module definition

/*  StripeManifest.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
#! The operations the Stripe application exports, and how they are presented
/** The manifest is deliberately explicit.  Stripe's schema declares 416 paths; this application exports a
    reviewed subset, and an endpoint appearing in a future schema does not become a public action until it is
    added here with a name, a description, a group and whatever presentation it needs.

    Action IDs are stable and independent of Stripe's \c operationId values: an upstream rename must be
    reported as drift rather than silently rename an action that existing workflows call.

    @section stripemanifest_overlay Notes on the Overlay

    The overlay was re-derived against the current schema rather than carried over from the TypeScript
    application it replaces, which had two defects worth recording so they are not reintroduced:

    - Stripe's timestamps are marked \c "format": \c "unix-time" and are counts of **seconds**; the previous
      overlay presented several of them as milliseconds, which is wrong by a factor of 1000.  Every one of
      them is listed here with @ref RestSchemaActions::RestSchemaActionCodecs::UnixSeconds "UnixSeconds", so
      a user supplies a date and the conversion happens once, in one place.
    - the previous invoice-ID selector called the payment-intents endpoint.  The only option in this
      application that takes an invoice ID is the \c invoice filter on
      @ref stripe_action_list_customer_balance_transactions "list-customer-balance-transactions", and it is
      populated from the invoice listing.
*/
public class StripeManifest {
    public {
        #! Action group: the account the credentials belong to
        const GroupAccount = "Account";

        #! Action group: Stripe Connect accounts managed by this platform
        const GroupConnect = "Connected Accounts";

        #! Action group: funds held by Stripe and the transactions behind them
        const GroupBalance = "Balance";

        #! Action group: charges
        const GroupCharges = "Charges";

        #! Action group: customers and the objects attached to them
        const GroupCustomers = "Customers";

        #! Action group: invoices
        const GroupInvoices = "Invoices";

        #! Action group: payment intents
        const GroupPaymentIntents = "Payment Intents";

        #! Action group: refunds
        const GroupRefunds = "Refunds";

        #! Reference data name: connected accounts
        const RefAccount = "stripe-account";

        #! Reference data name: customers
        const RefCustomer = "stripe-customer";

        #! Reference data name: charges
        const RefCharge = "stripe-charge";

        #! Reference data name: invoices
        const RefInvoice = "stripe-invoice";

        #! Reference data name: payment intents
        const RefPaymentIntent = "stripe-payment-intent";

        #! Reference data name: balance transactions
        const RefBalanceTransaction = "stripe-balance-transaction";

        #! The dropdowns the application offers
        /** Each is produced by executing a listing action from this same manifest, so a dropdown can never
            drift away from an endpoint the application actually exports.

            \c limit is set to Stripe's maximum page size, so that filling a dropdown is a single request
            rather than a walk through the whole collection.
        */
        const ReferenceData = {
            RefAccount: <RestSchemaReferenceDataInfo>{
                "name": RefAccount,
                "action": "list-connected-accounts",
                "options": {"limit": 100},
                "records_path": "data",
                "value_field": "id",
                "display_field": "email",
                "desc": "Stripe Connect accounts managed by this platform",
            },
            RefCustomer: <RestSchemaReferenceDataInfo>{
                "name": RefCustomer,
                "action": "list-customers",
                "options": {"limit": 100},
                "records_path": "data",
                "value_field": "id",
                "display_field": "email",
                "desc": "customers in this Stripe account",
            },
            RefCharge: <RestSchemaReferenceDataInfo>{
                "name": RefCharge,
                "action": "list-charges",
                "options": {"limit": 100},
                "records_path": "data",
                "value_field": "id",
                "display_field": "description",
                "desc": "recent charges",
            },
            RefInvoice: <RestSchemaReferenceDataInfo>{
                "name": RefInvoice,
                "action": "list-invoices",
                "options": {"limit": 100},
                "records_path": "data",
                "value_field": "id",
                "display_field": "number",
                "desc": "recent invoices",
            },
            RefPaymentIntent: <RestSchemaReferenceDataInfo>{
                "name": RefPaymentIntent,
                "action": "list-payment-intents",
                "options": {"limit": 100},
                "records_path": "data",
                "value_field": "id",
                "display_field": "description",
                "desc": "recent payment intents",
            },
            RefBalanceTransaction: <RestSchemaReferenceDataInfo>{
                "name": RefBalanceTransaction,
                "action": "list-balance-transactions",
                "options": {"limit": 100},
                "records_path": "data",
                "value_field": "id",
                "display_field": "description",
                "desc": "recent balance transactions",
            },
        };

        #! Overlay shared by the \c limit option of every listing action
        const LimitOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Limit",
            "short_desc": "How many records to return, from 1 to 100",
            "desc": "How many records to return in one request, between `1` and `100`. Stripe returns `10` "
                "when this is not set.\n\n"
                "The response reports `has_more`, and the ID of the last record returned can be passed as "
                "`starting_after` to retrieve the next page.",
            "preselected": True,
            "example_value": 25,
        };

        #! Overlay for a customer ID used as a filter or a reference
        const CustomerOverlay = <RestSchemaFieldOverlayInfo>{
            "ref_data": RefCustomer,
            "preselected": True,
        };

        #! Overlay for a customer ID that is a secondary reference rather than the action's subject
        const CustomerRefOverlay = <RestSchemaFieldOverlayInfo>{
            "ref_data": RefCustomer,
        };

        #! Overlay for a timestamp Stripe carries as a count of seconds since the epoch
        const TimestampOverlay = <RestSchemaFieldOverlayInfo>{
            "codec": RestSchemaActionCodecs::UnixSeconds,
        };

        #! Overlay for a timestamp shown in the initial form
        /** NOTE: declared in full rather than by adding to @ref TimestampOverlay, because adding a hash to
            a typed hash widens the result to \c hash<auto>, which no longer matches the overlay type
        */
        const PreselectedTimestampOverlay = <RestSchemaFieldOverlayInfo>{
            "codec": RestSchemaActionCodecs::UnixSeconds,
            "preselected": True,
        };

        #! The operations the application exports
        const Manifest = (
            # --- Account ---
            <RestSchemaActionInfo>{
                "action": "get-account",
                "method": "GET",
                "path": "/v1/account",
                "operation_id": "GetAccount",
                "display_name": "Get Account",
                "short_desc": "Retrieve the account the credentials belong to",
                "desc": "Retrieves the Stripe account that the connection's credentials authenticate as.\n\n"
                    "Use this to confirm which account an integration is working against — the `id`, the "
                    "country and default currency it settles in, and which `capabilities` are active. It is "
                    "also the cheapest way to check that credentials are valid before doing anything else.",
                "groups": (GroupAccount,),
            },
            <RestSchemaActionInfo>{
                "action": "get-balance",
                "method": "GET",
                "path": "/v1/balance",
                "operation_id": "GetBalance",
                "display_name": "Get Balance",
                "short_desc": "Retrieve the current account balance",
                "desc": "Retrieves the funds Stripe currently holds for the account, split into `available` "
                    "and `pending` amounts per currency.\n\n"
                    "`pending` is money from payments that have not yet settled; `available` is what the "
                    "next payout can draw on. Amounts are in the currency's smallest unit — cents for USD "
                    "and EUR, whole yen for JPY.",
                "groups": (GroupBalance,),
            },
            <RestSchemaActionInfo>{
                "action": "list-balance-transactions",
                "method": "GET",
                "path": "/v1/balance/history",
                "operation_id": "GetBalanceHistory",
                "display_name": "List Balance Transactions",
                "short_desc": "List the transactions that moved the balance",
                "desc": "Lists every transaction that contributed to the account balance: charges, refunds, "
                    "Stripe's fees, disputes, transfers and payouts.\n\n"
                    "This is the ledger to reconcile against a bank statement. Filter by `payout` to get "
                    "exactly the transactions that made up one deposit, which is what a reconciliation job "
                    "normally needs.",
                "groups": (GroupBalance,),
                "field_order": ("limit", "payout", "type", "currency"),
                "fields": {
                    "limit": LimitOverlay,
                    "payout": <RestSchemaFieldOverlayInfo>{
                        "short_desc": "Return only the transactions included in this payout",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-balance-transaction",
                "method": "GET",
                "path": "/v1/balance/history/{id}",
                "operation_id": "GetBalanceHistoryId",
                "display_name": "Get Balance Transaction",
                "short_desc": "Retrieve one balance transaction",
                "desc": "Retrieves a single balance transaction, showing the gross amount, the fee Stripe "
                    "took and the net amount that reached the balance.\n\n"
                    "The `source` field links back to whatever caused it — the charge, refund or dispute.",
                "groups": (GroupBalance,),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Balance Transaction",
                        "short_desc": "The balance transaction to retrieve",
                        "ref_data": RefBalanceTransaction,
                    },
                },
            },

            # --- Connected accounts ---
            <RestSchemaActionInfo>{
                "action": "list-connected-accounts",
                "method": "GET",
                "path": "/v1/accounts",
                "operation_id": "GetAccounts",
                "display_name": "List Connected Accounts",
                "short_desc": "List the connected accounts this platform manages",
                "desc": "Lists the [Stripe Connect](https://stripe.com/connect) accounts this platform "
                    "manages.\n\n"
                    "This is also what populates the connected-account selectors on the other actions in "
                    "this group, so a user picks an account rather than pasting an `acct_…` ID.",
                "groups": (GroupConnect,),
                "field_order": ("limit",),
                "fields": {
                    "limit": LimitOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-connected-account",
                "method": "GET",
                "path": "/v1/accounts/{account}",
                "operation_id": "GetAccountsAccount",
                "display_name": "Get Connected Account",
                "short_desc": "Retrieve one connected account",
                "desc": "Retrieves a connected account, including its `requirements` — the information "
                    "Stripe still needs before the account can accept payments or receive payouts.\n\n"
                    "Checking `requirements.currently_due` is how an onboarding flow knows what to ask the "
                    "account holder for next.",
                "groups": (GroupConnect,),
                "fields": {
                    "account": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connected Account",
                        "ref_data": RefAccount,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-connected-account",
                "method": "POST",
                "path": "/v1/accounts/{account}",
                "operation_id": "PostAccountsAccount",
                "display_name": "Update Connected Account",
                "short_desc": "Update a connected account",
                "desc": "Updates a connected account. Only the fields supplied are changed; everything else "
                    "is left as it is.\n\n"
                    "This is how an onboarding flow submits the details Stripe asked for in the account's "
                    "`requirements`, such as `business_profile`, `company` or `individual`.",
                "groups": (GroupConnect,),
                "field_order": ("account", "email", "business_profile", "metadata"),
                "fields": {
                    "account": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connected Account",
                        "ref_data": RefAccount,
                    },
                    "email": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-connected-account",
                "method": "DELETE",
                "path": "/v1/accounts/{account}",
                "operation_id": "DeleteAccountsAccount",
                "display_name": "Delete Connected Account",
                "short_desc": "Delete a connected account",
                "desc": "Deletes a connected account.\n\n"
                    "**This cannot be undone.** Stripe only allows it for accounts the platform itself "
                    "created and that have no balance left; an account holding funds must be paid out "
                    "first.",
                "groups": (GroupConnect,),
                "fields": {
                    "account": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connected Account",
                        "ref_data": RefAccount,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-account-link",
                "method": "POST",
                "path": "/v1/account_links",
                "operation_id": "PostAccountLinks",
                "display_name": "Create Account Link",
                "short_desc": "Create a single-use onboarding URL for a connected account",
                "desc": "Creates a single-use URL that sends a connected account holder into Stripe's "
                    "hosted onboarding, where they enter the identity and bank details Stripe requires.\n\n"
                    "The link **expires a few minutes after it is created** and can only be used once, so "
                    "create it at the moment the user is ready to be redirected rather than storing it. "
                    "`refresh_url` is where Stripe sends them if it has expired, and `return_url` is where "
                    "they land when they are done.",
                "groups": (GroupConnect,),
                "field_order": ("account", "type", "refresh_url", "return_url"),
                "fields": {
                    "account": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connected Account",
                        "ref_data": RefAccount,
                        "preselected": True,
                    },
                    "refresh_url": <RestSchemaFieldOverlayInfo>{
                        "type": AbstractDataProviderTypeMap."*uri",
                        "preselected": True,
                        "example_value": "https://example.com/stripe/refresh",
                    },
                    "return_url": <RestSchemaFieldOverlayInfo>{
                        "type": AbstractDataProviderTypeMap."*uri",
                        "preselected": True,
                        "example_value": "https://example.com/stripe/return",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-login-link",
                "method": "POST",
                "path": "/v1/accounts/{account}/login_links",
                "operation_id": "PostAccountsAccountLoginLinks",
                "display_name": "Create Login Link",
                "short_desc": "Create a single-use link into a connected account's Express dashboard",
                "desc": "Creates a single-use URL that logs a connected account holder into their Express "
                    "dashboard, where they can see their payouts and update their details.\n\n"
                    "Only works for Express accounts. The link is short-lived, so generate it when the user "
                    "clicks rather than in advance.",
                "groups": (GroupConnect,),
                "fields": {
                    "account": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connected Account",
                        "ref_data": RefAccount,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-external-accounts",
                "method": "GET",
                "path": "/v1/accounts/{account}/external_accounts",
                "operation_id": "GetAccountsAccountExternalAccounts",
                "display_name": "List External Accounts",
                "short_desc": "List a connected account's payout destinations",
                "desc": "Lists the bank accounts and debit cards a connected account is paid out to.\n\n"
                    "Set `object` to `bank_account` or `card` to see only one kind.",
                "groups": (GroupConnect,),
                "field_order": ("account", "object", "limit"),
                "fields": {
                    "account": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connected Account",
                        "ref_data": RefAccount,
                    },
                    "limit": LimitOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-external-account",
                "method": "POST",
                "path": "/v1/accounts/{account}/external_accounts",
                "operation_id": "PostAccountsAccountExternalAccounts",
                "display_name": "Create External Account",
                "short_desc": "Attach a payout destination to a connected account",
                "desc": "Attaches a bank account or debit card to a connected account as a payout "
                    "destination.\n\n"
                    "Supply a token created by Stripe's client libraries rather than raw bank or card "
                    "numbers, so the details never pass through your own systems. Set "
                    "`default_for_currency` to make it the destination for payouts in its currency.",
                "groups": (GroupConnect,),
                "field_order": ("account", "external_account", "default_for_currency", "metadata"),
                "fields": {
                    "account": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connected Account",
                        "ref_data": RefAccount,
                    },
                    "external_account": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-account-people",
                "method": "GET",
                "path": "/v1/accounts/{account}/people",
                "operation_id": "GetAccountsAccountPeople",
                "display_name": "List Account People",
                "short_desc": "List the people associated with a connected account",
                "desc": "Lists the people Stripe holds on a connected account's legal entity — its "
                    "directors, owners, executives and representative.\n\n"
                    "Regulations require these to be identified before an account can be paid out, so this "
                    "is what an onboarding flow reads to see who is still missing.",
                "groups": (GroupConnect,),
                "field_order": ("account", "relationship", "limit"),
                "fields": {
                    "account": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connected Account",
                        "ref_data": RefAccount,
                    },
                    "limit": LimitOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-account-person",
                "method": "POST",
                "path": "/v1/accounts/{account}/people",
                "operation_id": "PostAccountsAccountPeople",
                "display_name": "Create Account Person",
                "short_desc": "Add a person to a connected account",
                "desc": "Adds a person to a connected account's legal entity and records their "
                    "`relationship` to it — director, owner, executive or representative.\n\n"
                    "Which details Stripe requires depends on the country and the person's relationship; "
                    "the account's `requirements` says what is still outstanding.",
                "groups": (GroupConnect,),
                "field_order": ("account", "first_name", "last_name", "email", "relationship"),
                "fields": {
                    "account": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connected Account",
                        "ref_data": RefAccount,
                    },
                    "first_name": <RestSchemaFieldOverlayInfo>{"preselected": True},
                    "last_name": <RestSchemaFieldOverlayInfo>{"preselected": True},
                    "email": <RestSchemaFieldOverlayInfo>{"preselected": True},
                    "relationship": <RestSchemaFieldOverlayInfo>{"preselected": True},
                },
            },

            # --- Charges ---
            <RestSchemaActionInfo>{
                "action": "list-charges",
                "method": "GET",
                "path": "/v1/charges",
                "operation_id": "GetCharges",
                "display_name": "List Charges",
                "short_desc": "List charges, most recent first",
                "desc": "Lists charges, most recently created first.\n\n"
                    "Filter by `customer` to see everything one customer has paid, or by `payment_intent` "
                    "to find the charge a payment produced.",
                "groups": (GroupCharges,),
                "field_order": ("limit", "customer", "payment_intent"),
                "fields": {
                    "limit": LimitOverlay,
                    "customer": CustomerOverlay,
                    "payment_intent": <RestSchemaFieldOverlayInfo>{
                        "ref_data": RefPaymentIntent,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-charge",
                "method": "GET",
                "path": "/v1/charges/{charge}",
                "operation_id": "GetChargesCharge",
                "display_name": "Get Charge",
                "short_desc": "Retrieve one charge",
                "desc": "Retrieves a charge, including whether it succeeded, whether it has been refunded, "
                    "and the outcome Stripe's fraud checks reached.",
                "groups": (GroupCharges,),
                "fields": {
                    "charge": <RestSchemaFieldOverlayInfo>{
                        "ref_data": RefCharge,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-charge",
                "method": "POST",
                "path": "/v1/charges",
                "operation_id": "PostCharges",
                "display_name": "Create Charge",
                "short_desc": "Charge a card directly (legacy; prefer a payment intent)",
                "desc": "Charges a card directly.\n\n"
                    "**Stripe no longer recommends this for new integrations.** It cannot carry out the "
                    "authentication that European regulations require, so payments needing 3D Secure will "
                    "be declined. Use **Create Payment Intent** instead; this action remains for "
                    "integrations that already depend on it.",
                "groups": (GroupCharges,),
                "field_order": ("amount", "currency", "customer", "source", "description"),
                "fields": {
                    "amount": <RestSchemaFieldOverlayInfo>{
                        "short_desc": "The amount in the currency's smallest unit",
                        "preselected": True,
                        "example_value": 2000,
                    },
                    "currency": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "example_value": "usd",
                    },
                    "customer": CustomerOverlay,
                    "description": <RestSchemaFieldOverlayInfo>{"preselected": True},
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-charge",
                "method": "POST",
                "path": "/v1/charges/{charge}",
                "operation_id": "PostChargesCharge",
                "display_name": "Update Charge",
                "short_desc": "Update a charge's description, metadata or fraud details",
                "desc": "Updates a charge. Only the fields supplied are changed.\n\n"
                    "The amount and the card cannot be changed once a charge exists; what can be updated is "
                    "the bookkeeping around it — `description`, `metadata`, `receipt_email` and "
                    "`fraud_details`. Reporting a charge as fraudulent through `fraud_details` also feeds "
                    "Stripe's fraud detection.",
                "groups": (GroupCharges,),
                "field_order": ("charge", "description", "metadata"),
                "fields": {
                    "charge": <RestSchemaFieldOverlayInfo>{
                        "ref_data": RefCharge,
                    },
                    "description": <RestSchemaFieldOverlayInfo>{"preselected": True},
                    "customer": CustomerRefOverlay,
                },
            },

            # --- Customers ---
            <RestSchemaActionInfo>{
                "action": "list-customers",
                "method": "GET",
                "path": "/v1/customers",
                "operation_id": "GetCustomers",
                "display_name": "List Customers",
                "short_desc": "List customers, most recent first",
                "desc": "Lists customers, most recently created first.\n\n"
                    "`email` matches exactly and is the usual way to find whether somebody already exists "
                    "before creating a duplicate.",
                "groups": (GroupCustomers,),
                "field_order": ("limit", "email"),
                "fields": {
                    "limit": LimitOverlay,
                    "email": <RestSchemaFieldOverlayInfo>{
                        "type": AbstractDataProviderTypeMap."*email",
                        "short_desc": "Return only customers with exactly this email address",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-customer",
                "method": "GET",
                "path": "/v1/customers/{customer}",
                "operation_id": "GetCustomersCustomer",
                "display_name": "Get Customer",
                "short_desc": "Retrieve one customer",
                "desc": "Retrieves a customer.\n\n"
                    "A customer that has been deleted is still returned, carrying `deleted: true` and "
                    "little else, so check that before relying on the rest of the record.",
                "groups": (GroupCustomers,),
                "fields": {
                    "customer": <RestSchemaFieldOverlayInfo>{
                        "ref_data": RefCustomer,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-customer",
                "method": "POST",
                "path": "/v1/customers",
                "operation_id": "PostCustomers",
                "display_name": "Create Customer",
                "short_desc": "Create a customer",
                "desc": "Creates a customer, which is what payment methods, invoices and subscriptions are "
                    "attached to.\n\n"
                    "Stripe does not enforce that email addresses are unique, so an integration that "
                    "creates a customer per order will accumulate duplicates; look the customer up first, "
                    "or record your own identifier in `metadata` and search on that.",
                "groups": (GroupCustomers,),
                "field_order": ("email", "name", "description", "phone", "metadata"),
                "fields": {
                    "email": <RestSchemaFieldOverlayInfo>{
                        "type": AbstractDataProviderTypeMap."*email",
                        "preselected": True,
                        "example_value": "customer@example.com",
                    },
                    "name": <RestSchemaFieldOverlayInfo>{"preselected": True},
                    "description": <RestSchemaFieldOverlayInfo>{"preselected": True},
                    "metadata": <RestSchemaFieldOverlayInfo>{
                        "short_desc": "Your own key/value pairs, returned with the customer",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-customer",
                "method": "POST",
                "path": "/v1/customers/{customer}",
                "operation_id": "PostCustomersCustomer",
                "display_name": "Update Customer",
                "short_desc": "Update a customer",
                "desc": "Updates a customer. Only the fields supplied are changed.\n\n"
                    "`metadata` is replaced key by key: supplying a key overwrites it, and supplying it "
                    "with an empty value removes it.",
                "groups": (GroupCustomers,),
                "field_order": ("customer", "email", "name", "description", "metadata"),
                "fields": {
                    "customer": <RestSchemaFieldOverlayInfo>{
                        "ref_data": RefCustomer,
                    },
                    "email": <RestSchemaFieldOverlayInfo>{
                        "type": AbstractDataProviderTypeMap."*email",
                        "preselected": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{"preselected": True},
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-customer",
                "method": "DELETE",
                "path": "/v1/customers/{customer}",
                "operation_id": "DeleteCustomersCustomer",
                "display_name": "Delete Customer",
                "short_desc": "Delete a customer",
                "desc": "Deletes a customer and immediately cancels any subscription they have.\n\n"
                    "**This cannot be undone.** The customer's charges and invoices remain, for accounting, "
                    "but the customer record itself is gone.",
                "groups": (GroupCustomers,),
                "fields": {
                    "customer": <RestSchemaFieldOverlayInfo>{
                        "ref_data": RefCustomer,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-customer-balance-transactions",
                "method": "GET",
                "path": "/v1/customers/{customer}/balance_transactions",
                "operation_id": "GetCustomersCustomerBalanceTransactions",
                "display_name": "List Customer Balance Transactions",
                "short_desc": "List the entries in a customer's credit balance",
                "desc": "Lists the entries that changed a customer's credit balance — the running credit or "
                    "debit Stripe applies to their next invoice.\n\n"
                    "A negative balance is credit the customer holds; a positive balance is money they owe "
                    "that will be added to their next invoice.",
                "groups": (GroupCustomers,),
                "field_order": ("customer", "limit", "invoice"),
                "fields": {
                    "customer": <RestSchemaFieldOverlayInfo>{
                        "ref_data": RefCustomer,
                    },
                    "limit": LimitOverlay,
                    "invoice": <RestSchemaFieldOverlayInfo>{
                        "short_desc": "Return only the entries applied to this invoice",
                        "ref_data": RefInvoice,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-customer-balance-transaction",
                "method": "POST",
                "path": "/v1/customers/{customer}/balance_transactions",
                "operation_id": "PostCustomersCustomerBalanceTransactions",
                "display_name": "Create Customer Balance Transaction",
                "short_desc": "Credit or debit a customer's balance",
                "desc": "Credits or debits a customer's balance, which is applied to their next invoice.\n\n"
                    "A **negative** `amount` gives the customer credit; a **positive** amount charges them. "
                    "The entry is immutable once created, so correcting a mistake means posting an "
                    "offsetting entry rather than changing this one.",
                "groups": (GroupCustomers,),
                "field_order": ("customer", "amount", "currency", "description"),
                "fields": {
                    "customer": <RestSchemaFieldOverlayInfo>{
                        "ref_data": RefCustomer,
                    },
                    "amount": <RestSchemaFieldOverlayInfo>{
                        "short_desc": "Negative to credit the customer, positive to charge them",
                        "example_value": -500,
                    },
                    "description": <RestSchemaFieldOverlayInfo>{"preselected": True},
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-customer-sources",
                "method": "GET",
                "path": "/v1/customers/{customer}/sources",
                "operation_id": "GetCustomersCustomerSources",
                "display_name": "List Customer Sources",
                "short_desc": "List a customer's saved payment sources",
                "desc": "Lists the payment sources saved against a customer.\n\n"
                    "Sources are Stripe's older payment-method model. New integrations should use payment "
                    "methods and payment intents; this remains for integrations that already store "
                    "sources.",
                "groups": (GroupCustomers,),
                "field_order": ("customer", "object", "limit"),
                "fields": {
                    "customer": <RestSchemaFieldOverlayInfo>{
                        "ref_data": RefCustomer,
                    },
                    "limit": LimitOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-customer-source",
                "method": "POST",
                "path": "/v1/customers/{customer}/sources",
                "operation_id": "PostCustomersCustomerSources",
                "display_name": "Create Customer Source",
                "short_desc": "Attach a payment source to a customer",
                "desc": "Attaches a payment source to a customer so it can be charged later.\n\n"
                    "Supply a token created by Stripe's client libraries rather than raw card numbers, so "
                    "card data never reaches your own systems.",
                "groups": (GroupCustomers,),
                "field_order": ("customer", "source", "metadata"),
                "fields": {
                    "customer": <RestSchemaFieldOverlayInfo>{
                        "ref_data": RefCustomer,
                    },
                    "source": <RestSchemaFieldOverlayInfo>{"preselected": True},
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-customer-subscriptions",
                "method": "GET",
                "path": "/v1/customers/{customer}/subscriptions",
                "operation_id": "GetCustomersCustomerSubscriptions",
                "display_name": "List Customer Subscriptions",
                "short_desc": "List a customer's subscriptions",
                "desc": "Lists a customer's subscriptions, including those that have been cancelled but "
                    "have not yet reached the end of the period already paid for.",
                "groups": (GroupCustomers,),
                "field_order": ("customer", "limit"),
                "fields": {
                    "customer": <RestSchemaFieldOverlayInfo>{
                        "ref_data": RefCustomer,
                    },
                    "limit": LimitOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-customer-subscription",
                "method": "POST",
                "path": "/v1/customers/{customer}/subscriptions",
                "operation_id": "PostCustomersCustomerSubscriptions",
                "display_name": "Create Customer Subscription",
                "short_desc": "Subscribe a customer to one or more prices",
                "desc": "Subscribes a customer to one or more prices and starts billing them on a "
                    "recurring cycle.\n\n"
                    "The customer needs a default payment method, or the first invoice will not be paid. "
                    "`trial_end` starts the subscription with a free trial, and `billing_cycle_anchor` "
                    "aligns billing to a fixed date so several subscriptions bill together.",
                "groups": (GroupCustomers,),
                "field_order": ("customer", "items", "trial_end", "billing_cycle_anchor", "cancel_at"),
                "fields": {
                    "customer": <RestSchemaFieldOverlayInfo>{
                        "ref_data": RefCustomer,
                    },
                    "items": <RestSchemaFieldOverlayInfo>{
                        "short_desc": "The prices to subscribe the customer to, with quantities",
                        "preselected": True,
                    },
                    # Stripe carries these as counts of seconds since the epoch
                    "trial_end": TimestampOverlay,
                    "billing_cycle_anchor": TimestampOverlay,
                    "cancel_at": TimestampOverlay,
                    "backdate_start_date": TimestampOverlay,
                },
            },

            # --- Invoices ---
            <RestSchemaActionInfo>{
                "action": "list-invoices",
                "method": "GET",
                "path": "/v1/invoices",
                "operation_id": "GetInvoices",
                "display_name": "List Invoices",
                "short_desc": "List invoices, most recent first",
                "desc": "Lists invoices, most recently created first.\n\n"
                    "Filter by `status` to find work to do: `draft` invoices are still editable, `open` "
                    "ones are awaiting payment, and `uncollectible` ones have been written off.",
                "groups": (GroupInvoices,),
                "field_order": ("limit", "customer", "status"),
                "fields": {
                    "limit": LimitOverlay,
                    "customer": CustomerOverlay,
                    "status": <RestSchemaFieldOverlayInfo>{"preselected": True},
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-invoice",
                "method": "GET",
                "path": "/v1/invoices/{invoice}",
                "operation_id": "GetInvoicesInvoice",
                "display_name": "Get Invoice",
                "short_desc": "Retrieve one invoice",
                "desc": "Retrieves an invoice, including its line items, totals, status and the hosted page "
                    "and PDF a customer can be sent to.",
                "groups": (GroupInvoices,),
                "fields": {
                    "invoice": <RestSchemaFieldOverlayInfo>{
                        "ref_data": RefInvoice,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-invoice",
                "method": "POST",
                "path": "/v1/invoices",
                "operation_id": "PostInvoices",
                "display_name": "Create Invoice",
                "short_desc": "Create a draft invoice for a customer",
                "desc": "Creates an invoice for a customer.\n\n"
                    "A new invoice is a **draft**: it is not sent and cannot be paid until it is finalized, "
                    "and while it is a draft its line items can still be changed. "
                    "`automatically_finalizes_at` schedules that step. With `collection_method` set to "
                    "`send_invoice` a `due_date` is required.",
                "groups": (GroupInvoices,),
                "field_order": ("customer", "collection_method", "due_date", "description", "metadata"),
                "fields": {
                    "customer": CustomerOverlay,
                    "collection_method": <RestSchemaFieldOverlayInfo>{"preselected": True},
                    "description": <RestSchemaFieldOverlayInfo>{"preselected": True},
                    # Stripe carries these as counts of seconds since the epoch
                    "due_date": PreselectedTimestampOverlay,
                    "effective_at": TimestampOverlay,
                    "automatically_finalizes_at": TimestampOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-invoice",
                "method": "POST",
                "path": "/v1/invoices/{invoice}",
                "operation_id": "PostInvoicesInvoice",
                "display_name": "Update Invoice",
                "short_desc": "Update an invoice",
                "desc": "Updates an invoice. Only the fields supplied are changed.\n\n"
                    "Most fields can only be changed while the invoice is still a **draft**; once it has "
                    "been finalized Stripe accepts only a few, such as `metadata` and `description`.",
                "groups": (GroupInvoices,),
                "field_order": ("invoice", "description", "due_date", "metadata"),
                "fields": {
                    "invoice": <RestSchemaFieldOverlayInfo>{
                        "ref_data": RefInvoice,
                    },
                    "description": <RestSchemaFieldOverlayInfo>{"preselected": True},
                    "due_date": TimestampOverlay,
                    "effective_at": TimestampOverlay,
                    "automatically_finalizes_at": TimestampOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-invoice",
                "method": "DELETE",
                "path": "/v1/invoices/{invoice}",
                "operation_id": "DeleteInvoicesInvoice",
                "display_name": "Delete Invoice",
                "short_desc": "Delete a draft invoice",
                "desc": "Deletes a draft invoice.\n\n"
                    "**Only drafts can be deleted.** A finalized invoice is part of the account's records "
                    "and must be voided or marked uncollectible instead.",
                "groups": (GroupInvoices,),
                "fields": {
                    "invoice": <RestSchemaFieldOverlayInfo>{
                        "ref_data": RefInvoice,
                    },
                },
            },

            # --- Payment intents ---
            <RestSchemaActionInfo>{
                "action": "list-payment-intents",
                "method": "GET",
                "path": "/v1/payment_intents",
                "operation_id": "GetPaymentIntents",
                "display_name": "List Payment Intents",
                "short_desc": "List payment intents, most recent first",
                "desc": "Lists payment intents, most recently created first.\n\n"
                    "The `status` of each says where it is: `requires_payment_method` and "
                    "`requires_action` are waiting on the customer, `succeeded` means the money was taken.",
                "groups": (GroupPaymentIntents,),
                "field_order": ("limit", "customer"),
                "fields": {
                    "limit": LimitOverlay,
                    "customer": CustomerOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-payment-intent",
                "method": "POST",
                "path": "/v1/payment_intents",
                "operation_id": "PostPaymentIntents",
                "display_name": "Create Payment Intent",
                "short_desc": "Start a payment",
                "desc": "Starts a payment and tracks it through to completion.\n\n"
                    "This is the way to take a payment: a payment intent can carry out the customer "
                    "authentication that European regulations require, which a direct charge cannot. "
                    "`amount` is in the currency's smallest unit — `2000` is $20.00 — and the "
                    "`client_secret` on the result is what the client-side libraries use to finish the "
                    "payment with the customer.",
                "groups": (GroupPaymentIntents,),
                "field_order": ("amount", "currency", "customer", "description", "metadata"),
                "fields": {
                    "amount": <RestSchemaFieldOverlayInfo>{
                        "short_desc": "The amount in the currency's smallest unit",
                        "preselected": True,
                        "example_value": 2000,
                    },
                    "currency": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "example_value": "usd",
                    },
                    "customer": CustomerOverlay,
                    "description": <RestSchemaFieldOverlayInfo>{"preselected": True},
                },
            },

            # --- Refunds ---
            <RestSchemaActionInfo>{
                "action": "list-refunds",
                "method": "GET",
                "path": "/v1/refunds",
                "operation_id": "GetRefunds",
                "display_name": "List Refunds",
                "short_desc": "List refunds, most recent first",
                "desc": "Lists refunds, most recently created first.\n\n"
                    "Filter by `charge` or `payment_intent` to see what has already been refunded against "
                    "a payment before issuing another.",
                "groups": (GroupRefunds,),
                "field_order": ("limit", "charge", "payment_intent"),
                "fields": {
                    "limit": LimitOverlay,
                    "charge": <RestSchemaFieldOverlayInfo>{
                        "ref_data": RefCharge,
                        "preselected": True,
                    },
                    "payment_intent": <RestSchemaFieldOverlayInfo>{
                        "ref_data": RefPaymentIntent,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-refund",
                "method": "POST",
                "path": "/v1/refunds",
                "operation_id": "PostRefunds",
                "display_name": "Create Refund",
                "short_desc": "Refund a charge or payment intent",
                "desc": "Refunds a payment, in full or in part.\n\n"
                    "Name either the `charge` or the `payment_intent` to refund. Leaving `amount` unset "
                    "refunds the whole remaining balance; setting it refunds that much, and the action can "
                    "be repeated until the full amount has been returned. Stripe's own fee is not returned "
                    "on a refund.",
                "groups": (GroupRefunds,),
                "field_order": ("charge", "payment_intent", "amount", "reason", "metadata"),
                "fields": {
                    "charge": <RestSchemaFieldOverlayInfo>{
                        "ref_data": RefCharge,
                        "preselected": True,
                        "required_groups": ("payment",),
                    },
                    "payment_intent": <RestSchemaFieldOverlayInfo>{
                        "ref_data": RefPaymentIntent,
                        "preselected": True,
                        "required_groups": ("payment",),
                    },
                    "amount": <RestSchemaFieldOverlayInfo>{
                        "short_desc": "How much to refund; the whole remaining amount when unset",
                        "preselected": True,
                    },
                    "reason": <RestSchemaFieldOverlayInfo>{"preselected": True},
                    "customer": CustomerRefOverlay,
                },
            },
        );
    }
}
}
