# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore XeroDataProvider module definition

/*  XeroRecordTables.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the XeroDataProvider module
public namespace XeroDataProvider {
#! The Xero record-based tables
/** Xero is the other application in this family whose API publishes a real filter expression language, and
    the accounting tables therefore declare
    @ref DataProvider::DataProviderInfo::supports_native_search "supports_native_search": a whole search
    expression is rendered into Xero's \c where clause and Xero does the searching.

    @section xerorecord_where The where Clause

    Xero's \c where takes an infix expression: <tt>Status=="AUTHORISED" AND Type=="ACCREC"</tt>.  Strings are
    double-quoted, booleans are bare keywords, a date is written as <tt>DateTime(y,m,d)</tt> and an
    identifier as <tt>Guid("...")</tt>, and the string predicates are method calls -
    <tt>Name.Contains("x")</tt>, <tt>Name.StartsWith("x")</tt>.
    @ref XeroRecordTables::Where "Where" declares all of that, and
    @ref RestSchemaActions::RestSchemaRecordFilter "RestSchemaRecordFilter" renders the expression tree into
    it.

    Xero rejects a \c where naming a field it does not index - and rejects one over a nested object outright
    on most endpoints - so each table declares the fields it will accept, and a predicate on anything else is
    evaluated here rather than sent and turned into an error.

    @section xerorecord_write A Write Takes a List of Documents

    Xero's accounting writes are \c POST operations taking a **list** of documents even when there is one of
    them, and the same operation creates and updates: a document carrying its identity is an update, one
    without is a create.  That is why these tables name the same action for both, and why the identity
    travels **inside** the document rather than in a path variable - there is no path to put it in.

    A response comes back the same way, as a list of the documents written, and the table unwraps the one
    element of it.

    @section xerorecord_nodelete Nothing Is Deleted

    Xero does not delete an accounting document: an invoice is voided, a contact is archived, and both are
    status changes and therefore updates.  No delete action is registered.

    @section xerorecord_settings The Settings Listings Do Not Page

    \c Items has no \c page or \c pageSize at all - Xero returns the whole collection in one response - so
    that table declares @ref RSRP_NONE and reads it once.  Declaring paging it does not have would send
    arguments Xero ignores and then loop forever waiting for a short page.

    @section xerorecord_projects Projects Are a Second Document

    Xero publishes its projects API as a separate description with its own action set, so the projects
    tables are built on that set rather than on the accounting one.  They page with \c page and \c pageSize
    like the accounting collections, but they have no filter expression language: a project is filtered by
    contact and state, and a task by charge type.

    @section xerorecord_readonly Three Collections Are Read Only

    \c accounts, \c tax-rates and \c purchase-orders list and are never written: this application exports no
    create, update or delete for any of them, and a table naming an action the manifest does not export
    could only ever fail.  They declare none, and the record-based create, update and delete actions refuse
    them by name rather than pretending.

    Two of the three take Xero's \c where clause and therefore search natively;
    \c purchase-orders does not - it narrows its listing with \c Status, \c DateFrom and \c DateTo as query
    arguments, which is a different claim and is declared as one.

    A tax rate has no identifier of its own: Xero addresses one by its \c TaxType, which is what a tax rate
    is, so that is the identity the table declares.
*/
public class XeroRecordTables {
    public {
        #! The action group the record-based actions belong to
        const RecordGroup = "Record-Based";

        #! Xero's "where" filter expression language
        const Where = <RestSchemaQuerySyntaxInfo>{
            "name": "Xero where",
            "binary_ops": {
                DP_SEARCH_OP_EQ: "==",
                DP_SEARCH_OP_NE: "!=",
                DP_SEARCH_OP_LT: "<",
                DP_SEARCH_OP_LE: "<=",
                DP_SEARCH_OP_GT: ">",
                DP_SEARCH_OP_GE: ">=",
            },
            "method_ops": {
                DP_OP_CONTAINS: "Contains",
                DP_OP_STARTS_WITH: "StartsWith",
                DP_OP_ENDS_WITH: "EndsWith",
            },
            "and_op": "AND",
            "or_op": "OR",
            "not_op": "NOT",
            "render_value": \XeroRecordTables::renderValue(),
        };

        #! How an accounting collection pages
        const AccountingPaging = <RestSchemaRecordPagingInfo>{
            "mode": RSRP_PAGE,
            "page_option": "page",
            "size_option": "pageSize",
            "max_size": 1000,
        };

        #! How a projects collection pages
        const ProjectsPaging = <RestSchemaRecordPagingInfo>{
            "mode": RSRP_PAGE,
            "page_option": "page",
            "size_option": "pageSize",
            "max_size": 500,
        };

        #! The invoices table
        const InvoicesTable = <RestSchemaRecordTableInfo>{
            "name": "invoices",
            "display_name": "Invoices",
            "short_desc": "Xero invoices as records",
            "desc": "Xero invoices as records; Xero evaluates a search over them itself",
            "list_action": "getInvoices",
            "create_action": "updateOrCreateInvoices",
            "update_action": "updateOrCreateInvoices",
            "id_field": "InvoiceID",
            "records_path": ("Invoices",),
            "write_record_path": ("Invoices",),
            "write_list_option": "Invoices",
            "paging": AccountingPaging,
            "search_options": ("order", "searchTerm", "includeArchived", "summaryOnly", "unitdp"),
            "query": <RestSchemaRecordQueryInfo>{
                "option": "where",
                "syntax": Where,
                "fields": {
                    "InvoiceID": True, "InvoiceNumber": True, "Type": True, "Status": True,
                    "Reference": True, "Date": True, "DueDate": True, "Total": True, "AmountDue": True,
                    "AmountPaid": True, "CurrencyCode": True, "UpdatedDateUTC": True,
                    "IsDiscounted": True, "HasAttachments": True, "SentToContact": True,
                },
            },
        };

        #! The contacts table
        const ContactsTable = <RestSchemaRecordTableInfo>{
            "name": "contacts",
            "display_name": "Contacts",
            "short_desc": "Xero contacts as records",
            "desc": "Xero contacts as records; Xero evaluates a search over them itself",
            "list_action": "getContacts",
            "create_action": "updateOrCreateContacts",
            "update_action": "updateOrCreateContacts",
            "id_field": "ContactID",
            "records_path": ("Contacts",),
            "write_record_path": ("Contacts",),
            "write_list_option": "Contacts",
            "paging": AccountingPaging,
            "search_options": ("order", "searchTerm", "includeArchived", "summaryOnly"),
            "query": <RestSchemaRecordQueryInfo>{
                "option": "where",
                "syntax": Where,
                "fields": {
                    "ContactID": True, "ContactNumber": True, "AccountNumber": True, "ContactStatus": True,
                    "Name": True, "FirstName": True, "LastName": True, "EmailAddress": True,
                    "CompanyNumber": True, "TaxNumber": True, "IsSupplier": True, "IsCustomer": True,
                    "UpdatedDateUTC": True,
                },
            },
        };

        #! The bank transactions table
        const BankTransactionsTable = <RestSchemaRecordTableInfo>{
            "name": "bank-transactions",
            "display_name": "Bank Transactions",
            "short_desc": "Xero bank transactions as records",
            "desc": "Xero bank transactions as records; Xero evaluates a search over them itself",
            "list_action": "getBankTransactions",
            "create_action": "updateOrCreateBankTransactions",
            "update_action": "updateOrCreateBankTransactions",
            "id_field": "BankTransactionID",
            "records_path": ("BankTransactions",),
            "write_record_path": ("BankTransactions",),
            "write_list_option": "BankTransactions",
            "paging": AccountingPaging,
            "search_options": ("order", "unitdp"),
            "query": <RestSchemaRecordQueryInfo>{
                "option": "where",
                "syntax": Where,
                "fields": {
                    "BankTransactionID": True, "Type": True, "Status": True, "Reference": True,
                    "Date": True, "Total": True, "CurrencyCode": True, "IsReconciled": True,
                    "UpdatedDateUTC": True,
                },
            },
        };

        #! The items table
        /** Xero's item listing takes no paging arguments at all: it answers with the whole collection.
        */
        const ItemsTable = <RestSchemaRecordTableInfo>{
            "name": "items",
            "display_name": "Items",
            "short_desc": "Xero inventory items as records",
            "desc": "Xero inventory items as records; Xero evaluates a search over them itself, and returns "
                "the whole collection in one response rather than paging it",
            "list_action": "getItems",
            "create_action": "updateOrCreateItems",
            "update_action": "updateOrCreateItems",
            "id_field": "ItemID",
            "records_path": ("Items",),
            "write_record_path": ("Items",),
            "write_list_option": "Items",
            "paging": <RestSchemaRecordPagingInfo>{"mode": RSRP_NONE},
            "search_options": ("order", "unitdp"),
            "query": <RestSchemaRecordQueryInfo>{
                "option": "where",
                "syntax": Where,
                "fields": {
                    "ItemID": True, "Code": True, "Name": True, "Description": True, "IsSold": True,
                    "IsPurchased": True, "IsTrackedAsInventory": True, "QuantityOnHand": True,
                    "UpdatedDateUTC": True,
                },
            },
        };

        #! The payments table
        /** Xero's payment create takes one payment rather than a list, and a payment is not updated: it is
            reversed by deleting it, which this application does not export.
        */
        const PaymentsTable = <RestSchemaRecordTableInfo>{
            "name": "payments",
            "display_name": "Payments",
            "short_desc": "Xero payments as records",
            "desc": "Xero payments as records; Xero evaluates a search over them itself.  A payment is not "
                "edited once made - it is reversed - so this table reads and creates only",
            "list_action": "getPayments",
            "create_action": "createPayment",
            "id_field": "PaymentID",
            "records_path": ("Payments",),
            "write_record_path": ("Payments",),
            "paging": AccountingPaging,
            "search_options": ("order",),
            "query": <RestSchemaRecordQueryInfo>{
                "option": "where",
                "syntax": Where,
                "fields": {
                    "PaymentID": True, "Status": True, "PaymentType": True, "Date": True, "Amount": True,
                    "Reference": True, "IsReconciled": True, "UpdatedDateUTC": True,
                },
            },
        };

        #! The invoice history table
        /** A history record is a note appended to one invoice.  It has no identity of its own and Xero
            returns the whole history in one response, so this table reads and appends and does nothing
            else - which is everything Xero's API offers for it.
        */
        const InvoiceHistoryTable = <RestSchemaRecordTableInfo>{
            "name": "invoice-history",
            "display_name": "Invoice History",
            "short_desc": "the history and notes on one invoice",
            "desc": "The history and notes on one invoice, as records; a record is appended and is never "
                "changed or removed",
            "list_action": "getInvoiceHistory",
            "create_action": "createInvoiceHistory",
            "scope_options": ("InvoiceID",),
            "records_path": ("HistoryRecords",),
            "write_record_path": ("HistoryRecords",),
            "write_list_option": "HistoryRecords",
            "paging": <RestSchemaRecordPagingInfo>{"mode": RSRP_NONE},
        };

        #! The accounts table
        /** Xero's account listing takes no paging arguments at all: it answers with the whole chart of
            accounts.

            Every field declared below was verified against the live API to be one Xero will accept in a
            \c where clause, including the booleans - Xero answers <tt>EnablePaymentsToAccount==false</tt>
            with exactly the accounts that have it unset.

            @see @ref xerorecord_readonly
        */
        const AccountsTable = <RestSchemaRecordTableInfo>{
            "name": "accounts",
            "display_name": "Accounts",
            "short_desc": "the accounts in the Xero chart of accounts",
            "desc": "The accounts in the Xero chart of accounts, as records; Xero evaluates a search over "
                "them itself, and returns the whole chart in one response rather than paging it.  Accounts "
                "are read only here",
            "list_action": "getAccounts",
            "id_field": "AccountID",
            "records_path": ("Accounts",),
            "paging": <RestSchemaRecordPagingInfo>{"mode": RSRP_NONE},
            "search_options": ("order",),
            "query": <RestSchemaRecordQueryInfo>{
                "option": "where",
                "syntax": Where,
                "fields": {
                    "AccountID": True, "Code": True, "Name": True, "Type": True, "Class": True,
                    "Status": True, "TaxType": True, "BankAccountType": True, "BankAccountNumber": True,
                    "CurrencyCode": True, "SystemAccount": True, "Description": True,
                    "EnablePaymentsToAccount": True, "ShowInExpenseClaims": True, "ReportingCode": True,
                    "AddToWatchlist": True, "HasAttachments": True, "UpdatedDateUTC": True,
                },
            },
        };

        #! The tax rates table
        /** Xero's tax rate listing takes no paging arguments either, and a tax rate has no identifier of
            its own: it **is** its \c TaxType, which is what every Xero document names to apply one.

            @see @ref xerorecord_readonly
        */
        const TaxRatesTable = <RestSchemaRecordTableInfo>{
            "name": "tax-rates",
            "display_name": "Tax Rates",
            "short_desc": "the tax rates the organisation can apply",
            "desc": "The tax rates the organisation can apply, as records; Xero evaluates a search over "
                "them itself, and returns them all in one response rather than paging them.  Tax rates are "
                "read only here, and a tax rate is named by its tax type",
            "list_action": "getTaxRates",
            "id_field": "TaxType",
            "records_path": ("TaxRates",),
            "paging": <RestSchemaRecordPagingInfo>{"mode": RSRP_NONE},
            "search_options": ("order",),
            "query": <RestSchemaRecordQueryInfo>{
                "option": "where",
                "syntax": Where,
                "fields": {
                    "TaxType": True, "Name": True, "Status": True, "ReportTaxType": True,
                    "CanApplyToAssets": True, "CanApplyToEquity": True, "CanApplyToExpenses": True,
                    "CanApplyToLiabilities": True, "CanApplyToRevenue": True, "DisplayTaxRate": True,
                    "EffectiveRate": True,
                },
            },
        };

        #! The purchase orders table
        /** The one accounting collection here with no \c where clause: Xero narrows it with \c Status,
            \c DateFrom and \c DateTo as query arguments instead, so this table declares \c filters and no
            filter expression language, and does not claim native search.

            \c Status is declared **inexact**: it narrows what has to be read and the predicate is applied
            again to what comes back, which is correct whether or not Xero matches a status exactly.

            The records are under \c PurchaseOrders.  Xero declares a \c Warnings array on the same
            envelope, so a scan for the first array-valued field finds the wrong one.

            @see @ref xerorecord_readonly
        */
        const PurchaseOrdersTable = <RestSchemaRecordTableInfo>{
            "name": "purchase-orders",
            "display_name": "Purchase Orders",
            "short_desc": "Xero purchase orders as records",
            "desc": "Xero purchase orders as records; a search narrows them by status and by date range, "
                "and the rest of a search is evaluated as the records arrive.  Purchase orders are read "
                "only here",
            "list_action": "getPurchaseOrders",
            "id_field": "PurchaseOrderID",
            "records_path": ("PurchaseOrders",),
            "paging": AccountingPaging,
            "search_options": ("Status", "DateFrom", "DateTo", "order"),
            "filters": (
                <RestSchemaRecordFilterInfo>{
                    "field": "Status", "operator": DP_SEARCH_OP_EQ, "option": "Status",
                },
            ),
        };

        #! Every accounting table, in presentation order
        const AccountingTables = (InvoicesTable, ContactsTable, BankTransactionsTable, ItemsTable,
            PaymentsTable, InvoiceHistoryTable, AccountsTable, TaxRatesTable, PurchaseOrdersTable);

        #! The projects table
        /** The projects API has no filter expression language: a project is filtered by contact and state.
        */
        const ProjectsTable = <RestSchemaRecordTableInfo>{
            "name": "projects",
            "display_name": "Projects",
            "short_desc": "Xero projects as records",
            "desc": "Xero projects as records",
            "list_action": "getProjects",
            "create_action": "createProject",
            "id_field": "projectId",
            "records_path": ("items",),
            "paging": ProjectsPaging,
            "filters": (
                <RestSchemaRecordFilterInfo>{
                    "field": "contactId", "operator": DP_SEARCH_OP_EQ, "option": "contactID", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "status", "operator": DP_SEARCH_OP_EQ, "option": "states", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "projectId", "operator": DP_SEARCH_OP_IN, "option": "projectIds",
                    "multi": True, "exact": True,
                },
            ),
        };

        #! The project tasks table
        const TasksTable = <RestSchemaRecordTableInfo>{
            "name": "project-tasks",
            "display_name": "Project Tasks",
            "short_desc": "the tasks on one Xero project",
            "desc": "The tasks on one Xero project, as records",
            "list_action": "getTasks",
            "create_action": "createTask",
            "id_field": "taskId",
            "scope_options": ("projectId",),
            "records_path": ("items",),
            "paging": ProjectsPaging,
            "filters": (
                <RestSchemaRecordFilterInfo>{
                    "field": "chargeType", "operator": DP_SEARCH_OP_EQ, "option": "chargeType",
                    "exact": True,
                },
            ),
        };

        #! Every projects table, in presentation order
        const ProjectsTables = (ProjectsTable, TasksTable);

        #! Every table, in presentation order
        const Tables = AccountingTables + ProjectsTables;

        #! The IDs the record-based actions are registered under
        /** Xero deletes nothing: a document is voided or archived, and both are updates.
        */
        const RecordActionIds = <RestSchemaRecordActionIds>{
            "delete_records": NOTHING,
        };
    }

    #! Renders one value as Xero's where clause spells it
    /** An identifier is wrapped in <tt>Guid("...")</tt> and a point in time in <tt>DateTime(y,m,d)</tt>,
        because Xero compares neither as a string.  A field whose name ends in \c "ID" carries an identifier,
        which is how Xero names every one of them.
    */
    static string renderValue(auto value, string field) {
        switch (value.typeCode()) {
            case NT_NOTHING:
            case NT_NULL:
                return "null";
            case NT_BOOLEAN:
                return value ? "true" : "false";
            case NT_DATE:
                return sprintf("DateTime(%d,%d,%d)", value.years(), value.months(), value.days());
            case NT_INT:
            case NT_FLOAT:
            case NT_NUMBER:
                return value.toString();
        }
        string str = value.toString();
        if (field.size() > 2 && field.substr(field.size() - 2) == "ID") {
            return sprintf("Guid(\"%s\")", str);
        }
        return "\"" + replace(replace(str, "\\", "\\\\"), "\"", "\\\"") + "\"";
    }
}
}
