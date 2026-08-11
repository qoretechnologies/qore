# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore StripeDataProvider module definition

/*  StripeRecordTables.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
#! The Stripe record-based tables
/** Eleven of Stripe's collections are record-shaped and are exposed as tables.  Each is built from actions
    the manifest already exports, so the request contracts and the record shapes are the pinned schema's.

    @section striperecord_paging Stripe Pages by the Last Record's ID

    A Stripe listing takes \c starting_after - the ID of the last record of the previous page - rather than a
    page number or an opaque cursor, and reports \c has_more beside the records.  \c limit is capped at 100
    and is asked for in full.  @ref RSRP_AFTER_ID is exactly this contract, and \c has_more is what ends a
    walk.

    @section striperecord_pushdown What Stripe Filters

    Stripe's listings filter on the relationships between its objects, and this module sends those: a charge
    or an invoice by customer, a refund by charge or payment intent, an invoice by status or subscription.
    Every one of them is an exact equality on an identifier, which is what makes it safe to drop the
    predicate once it has been sent.

    A record's \c created is **not** wired to an operator.  Stripe's \c created argument accepts either an
    exact second or a <tt>{gt, gte, lt, lte}</tt> object, and this module's timestamps are %Qore
    @ref date values converted by the \c UnixSeconds codec; comparing a point in time for equality to the
    second is not a search anybody means, and the range form is not one operator.  A date predicate is
    therefore evaluated here, and \c created stays available as an ordinary search option for a caller who
    wants Stripe's own semantics.

    @section striperecord_delete Only Two Things Can Be Deleted

    Stripe deletes a customer and an invoice; everything else is reversed, refunded, cancelled or expired,
    and those are creates and updates of other objects rather than deletes.  Each table declares what its own
    resource supports and nothing more.
*/
public class StripeRecordTables {
    public {
        #! The action group the record-based actions belong to
        const RecordGroup = "Record-Based";

        #! How every Stripe collection pages
        const Paging = <RestSchemaRecordPagingInfo>{
            "mode": RSRP_AFTER_ID,
            "page_option": "starting_after",
            "size_option": "limit",
            "max_size": 100,
            "has_more_path": ("has_more",),
        };

        #! The customers table
        const CustomersTable = <RestSchemaRecordTableInfo>{
            "name": "customers",
            "display_name": "Customers",
            "short_desc": "Stripe customers as records",
            "desc": "Stripe customers as records",
            "list_action": "list-customers",
            "get_action": "get-customer",
            "create_action": "create-customer",
            "update_action": "update-customer",
            "delete_action": "delete-customer",
            "id_field": "id",
            "id_option": "customer",
            "records_path": ("data",),
            "paging": Paging,
            "search_options": ("created", "test_clock"),
            "filters": (
                <RestSchemaRecordFilterInfo>{
                    "field": "email", "operator": DP_SEARCH_OP_EQ, "option": "email", "exact": True,
                },
            ),
        };

        #! The charges table
        const ChargesTable = <RestSchemaRecordTableInfo>{
            "name": "charges",
            "display_name": "Charges",
            "short_desc": "Stripe charges as records",
            "desc": "Stripe charges as records; a charge is refunded rather than deleted",
            "list_action": "list-charges",
            "get_action": "get-charge",
            "create_action": "create-charge",
            "update_action": "update-charge",
            "id_field": "id",
            "id_option": "charge",
            "records_path": ("data",),
            "paging": Paging,
            "search_options": ("created",),
            "filters": (
                <RestSchemaRecordFilterInfo>{
                    "field": "customer", "operator": DP_SEARCH_OP_EQ, "option": "customer", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "payment_intent", "operator": DP_SEARCH_OP_EQ, "option": "payment_intent",
                    "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "transfer_group", "operator": DP_SEARCH_OP_EQ, "option": "transfer_group",
                    "exact": True,
                },
            ),
        };

        #! The invoices table
        const InvoicesTable = <RestSchemaRecordTableInfo>{
            "name": "invoices",
            "display_name": "Invoices",
            "short_desc": "Stripe invoices as records",
            "desc": "Stripe invoices as records",
            "list_action": "list-invoices",
            "get_action": "get-invoice",
            "create_action": "create-invoice",
            "update_action": "update-invoice",
            "delete_action": "delete-invoice",
            "id_field": "id",
            "id_option": "invoice",
            "records_path": ("data",),
            "paging": Paging,
            # "subscription" is a listing argument, but the invoice this API version returns carries the
            # subscription elsewhere and not as a top-level column, so it stays an option rather than a
            # predicate on a field the record does not have
            "search_options": ("created", "due_date", "subscription"),
            "filters": (
                <RestSchemaRecordFilterInfo>{
                    "field": "customer", "operator": DP_SEARCH_OP_EQ, "option": "customer", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "status", "operator": DP_SEARCH_OP_EQ, "option": "status", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "collection_method", "operator": DP_SEARCH_OP_EQ, "option": "collection_method",
                    "exact": True,
                },
            ),
        };

        #! The payment intents table
        /** A payment intent is cancelled or confirmed rather than edited, and the manifest exports neither
            an update nor a delete for one.
        */
        const PaymentIntentsTable = <RestSchemaRecordTableInfo>{
            "name": "payment-intents",
            "display_name": "Payment Intents",
            "short_desc": "Stripe payment intents as records",
            "desc": "Stripe payment intents as records; an intent is confirmed or cancelled rather than "
                "edited",
            "list_action": "list-payment-intents",
            "create_action": "create-payment-intent",
            "id_field": "id",
            "records_path": ("data",),
            "paging": Paging,
            "search_options": ("created",),
            "filters": (
                <RestSchemaRecordFilterInfo>{
                    "field": "customer", "operator": DP_SEARCH_OP_EQ, "option": "customer", "exact": True,
                },
            ),
        };

        #! The refunds table
        const RefundsTable = <RestSchemaRecordTableInfo>{
            "name": "refunds",
            "display_name": "Refunds",
            "short_desc": "Stripe refunds as records",
            "desc": "Stripe refunds as records; a refund is final and is neither edited nor deleted",
            "list_action": "list-refunds",
            "create_action": "create-refund",
            "id_field": "id",
            "records_path": ("data",),
            "paging": Paging,
            "search_options": ("created",),
            "filters": (
                <RestSchemaRecordFilterInfo>{
                    "field": "charge", "operator": DP_SEARCH_OP_EQ, "option": "charge", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "payment_intent", "operator": DP_SEARCH_OP_EQ, "option": "payment_intent",
                    "exact": True,
                },
            ),
        };

        #! The connected accounts table
        const AccountsTable = <RestSchemaRecordTableInfo>{
            "name": "connected-accounts",
            "display_name": "Connected Accounts",
            "short_desc": "the connected accounts of a Stripe platform",
            "desc": "The connected accounts of a Stripe platform, as records",
            "list_action": "list-connected-accounts",
            "get_action": "get-connected-account",
            "update_action": "update-connected-account",
            "delete_action": "delete-connected-account",
            "id_field": "id",
            "id_option": "account",
            "records_path": ("data",),
            "paging": Paging,
            "search_options": ("created",),
        };

        #! The columns an external account carries whatever kind it is
        /** Stripe describes an external account as **either** a bank account or a card, so its listing has
            no single element type to derive a record type from.  These are the columns both declare; a table
            over a polymorphic collection can offer no more than that, and \c object says which kind a record
            is so that the rest can be read through the action surface.
        */
        const ExternalAccountFields = (
            {"name": "id", "desc": "the external account ID", "type": "*string"},
            {"name": "object", "desc": "the kind of account: `bank_account` or `card`", "type": "*string"},
            {"name": "account", "desc": "the connected account this belongs to", "type": "*string"},
            {"name": "customer", "desc": "the customer this belongs to, if any", "type": "*string"},
            {"name": "country", "desc": "the two-letter country code", "type": "*string"},
            {"name": "currency", "desc": "the three-letter currency code", "type": "*string"},
            {"name": "default_for_currency", "desc": "whether payouts in this currency use this account",
                "type": "*bool"},
            {"name": "fingerprint", "desc": "a value identifying the underlying account across objects",
                "type": "*string"},
            {"name": "last4", "desc": "the last four digits", "type": "*string"},
            {"name": "status", "desc": "the account state", "type": "*string"},
            {"name": "available_payout_methods", "desc": "the payout methods this account supports",
                "type": "*list"},
            {"name": "metadata", "desc": "the key-value pairs attached to the object", "type": "*hash"},
        );

        #! The columns a customer payment source carries whatever kind it is
        /** A source is a bank account, a card **or** a source object, and the three agree on very little.
            These are the columns all three declare.
        */
        const CustomerSourceFields = (
            {"name": "id", "desc": "the source ID", "type": "*string"},
            {"name": "object", "desc": "the kind of source: `bank_account`, `card` or `source`",
                "type": "*string"},
            {"name": "customer", "desc": "the customer this belongs to", "type": "*string"},
            {"name": "currency", "desc": "the three-letter currency code", "type": "*string"},
            {"name": "status", "desc": "the source state", "type": "*string"},
            {"name": "metadata", "desc": "the key-value pairs attached to the object", "type": "*hash"},
        );

        #! The external accounts table
        const ExternalAccountsTable = <RestSchemaRecordTableInfo>{
            "name": "external-accounts",
            "display_name": "External Accounts",
            "short_desc": "the bank accounts and cards of one connected account",
            "desc": "The bank accounts and cards a connected account is paid out to, as records",
            "list_action": "list-external-accounts",
            "create_action": "create-external-account",
            "id_field": "id",
            "scope_options": ("account",),
            "records_path": ("data",),
            "paging": Paging,
            "record_type": RestSchemaTableDataProvider::makeRecordType(ExternalAccountFields),
            "search_options": ("object",),
            "filters": (
                <RestSchemaRecordFilterInfo>{
                    "field": "object", "operator": DP_SEARCH_OP_EQ, "option": "object", "exact": True,
                },
            ),
        };

        #! The account people table
        const AccountPeopleTable = <RestSchemaRecordTableInfo>{
            "name": "account-people",
            "display_name": "Account People",
            "short_desc": "the people associated with one connected account",
            "desc": "The people associated with one connected account, as records",
            "list_action": "list-account-people",
            "create_action": "create-account-person",
            "id_field": "id",
            "scope_options": ("account",),
            "records_path": ("data",),
            "paging": Paging,
        };

        #! The customer balance transactions table
        const CustomerBalanceTransactionsTable = <RestSchemaRecordTableInfo>{
            "name": "customer-balance-transactions",
            "display_name": "Customer Balance Transactions",
            "short_desc": "the credit and debit entries on one customer's balance",
            "desc": "The credit and debit entries on one customer's balance, as records",
            "list_action": "list-customer-balance-transactions",
            "create_action": "create-customer-balance-transaction",
            "id_field": "id",
            "scope_options": ("customer",),
            "records_path": ("data",),
            "paging": Paging,
            "search_options": ("created",),
            "filters": (
                <RestSchemaRecordFilterInfo>{
                    "field": "invoice", "operator": DP_SEARCH_OP_EQ, "option": "invoice", "exact": True,
                },
            ),
        };

        #! The customer sources table
        const CustomerSourcesTable = <RestSchemaRecordTableInfo>{
            "name": "customer-sources",
            "display_name": "Customer Payment Sources",
            "short_desc": "the payment sources attached to one customer",
            "desc": "The payment sources attached to one customer, as records",
            "list_action": "list-customer-sources",
            "create_action": "create-customer-source",
            "id_field": "id",
            "scope_options": ("customer",),
            "records_path": ("data",),
            "paging": Paging,
            "record_type": RestSchemaTableDataProvider::makeRecordType(CustomerSourceFields),
            "search_options": ("object",),
            "filters": (
                <RestSchemaRecordFilterInfo>{
                    "field": "object", "operator": DP_SEARCH_OP_EQ, "option": "object", "exact": True,
                },
            ),
        };

        #! The customer subscriptions table
        const CustomerSubscriptionsTable = <RestSchemaRecordTableInfo>{
            "name": "customer-subscriptions",
            "display_name": "Customer Subscriptions",
            "short_desc": "the subscriptions of one customer",
            "desc": "The subscriptions of one customer, as records",
            "list_action": "list-customer-subscriptions",
            "create_action": "create-customer-subscription",
            "id_field": "id",
            "scope_options": ("customer",),
            "records_path": ("data",),
            "paging": Paging,
        };

        #! Every table, in presentation order
        const Tables = (CustomersTable, ChargesTable, InvoicesTable, PaymentIntentsTable, RefundsTable,
            AccountsTable, ExternalAccountsTable, AccountPeopleTable, CustomerBalanceTransactionsTable,
            CustomerSourcesTable, CustomerSubscriptionsTable);

        #! The IDs the record-based actions are registered under
        const RecordActionIds = <RestSchemaRecordActionIds>{};
    }
}
}
