# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore XeroDataProvider module definition

/*  XeroManifest.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
#! The operations the Xero application exports, and how they are presented
/** There are two manifests here rather than one, because Xero publishes one description per product and
    each becomes its own action set; see @ref XeroDataProvider::XeroSchema "XeroSchema".

    @section xeromanifest_ids Action IDs

    The action IDs are those of the TypeScript application this module replaces, which took them from the
    \c operationId of a vendored Swagger 2.0 document.  Xero generates that document and the OpenAPI 3 one
    from the same source, so they agree and the IDs carry over unchanged - \c "getContacts",
    \c "updateOrCreateInvoices".  Each is recorded as \c operation_id as well, so an upstream rename
    surfaces as drift rather than silently renaming an action.

    @section xeromanifest_added What This Application Adds

    Five read-only listings are exported that the TypeScript application did not.  Every one of them backs
    something that application already offered without the endpoint behind it:

    |!Action|!Why
    |\c getAccounts|its account, account-code and bank-account dropdowns all listed accounts, which it did not export
    |\c getTaxRates|its tax type dropdown listed tax rates, which it did not export
    |\c getCreditNotes|its credit note dropdown and its "new credit note" trigger both read credit notes, which it did not export
    |\c getBankTransactions|its bank transaction dropdown and its "new bank transaction" trigger both read bank transactions, which it did not export
    |\c getPayments|its "new payment" trigger read payments, which it did not export

    @section xeromanifest_tenant The Tenant Is Not an Option

    Every Xero operation declares a required \c Xero-Tenant-Id header, and the description declares it on
    each operation because that is the only place OpenAPI can put it.  It is not a per-action choice: a
    connection is a connection to one organisation's books, and asking for it on every action would let one
    workflow write an invoice into the wrong company.  It is dropped from every action here and sent by
    @ref XeroRestClient::XeroRestClientBase "XeroRestClientBase" as a header on every request.

    \c Idempotency-Key is dropped for a different reason: it is a caller-generated deduplication token, not
    a property of the thing being created, and the application this module replaces did not offer it either.

    @section xeromanifest_envelopes Xero's Envelopes

    A Xero accounting collection is an object with one array in it named after the type - \c {Contacts: [...]},
    \c {Invoices: [...]} - and a create takes the same envelope, so creating one invoice means sending a
    one-element \c Invoices array.  That is Xero's contract, not a presentation choice, and the actions
    document it rather than hiding it.

    The projects API is different again: it returns \c {items, pagination}, in \c camelCase where accounting
    uses \c PascalCase.  Two products, two conventions, one application.

    @section xeromanifest_dates Dates in Query Arguments

    Xero declares its date-valued query arguments as plain strings and expects \c YYYY-MM-DD.  Those fields
    carry @ref XeroDataProvider::XeroCodecs::QueryDate "the query date codec", so the action takes a %Qore
    date and the wire gets the format Xero reads.
*/
public class XeroManifest {
    public {
        #! Action group: contacts
        const GroupContacts = "Contacts";

        #! Action group: invoices and quotes
        const GroupInvoices = "Invoices";

        #! Action group: payments, credit notes and bank transactions
        const GroupPayments = "Payments";

        #! Action group: items, accounts and tax rates
        const GroupSettings = "Items and Accounts";

        #! Action group: purchase orders
        const GroupPurchasing = "Purchasing";

        #! Action group: projects
        const GroupProjects = "Projects";

        #! Reference data name: contacts
        const RefContact = "xero-contact";

        #! Reference data name: invoices
        const RefInvoice = "xero-invoice";

        #! Reference data name: accounts, by ID
        const RefAccount = "xero-account";

        #! Reference data name: accounts, by code
        const RefAccountCode = "xero-account-code";

        #! Reference data name: bank accounts
        const RefBankAccount = "xero-bank-account";

        #! Reference data name: items, by ID
        const RefItem = "xero-item";

        #! Reference data name: items, by code
        const RefItemCode = "xero-item-code";

        #! Reference data name: tax types
        const RefTaxType = "xero-tax-type";

        #! Reference data name: credit notes
        const RefCreditNote = "xero-credit-note";

        #! Reference data name: bank transactions
        const RefBankTransaction = "xero-bank-transaction";

        #! Reference data name: projects
        const RefProject = "xero-project";

        #! Xero's maximum page size for a listing
        const MaxPageSize = XeroRestClient::XeroRestClientBase::MaxPageSize;

        #! The dropdowns the accounting document produces
        /** A Xero accounting collection is an object with one array in it, named after the type, so every
            one of these follows a records path.
        */
        const AccountingReferenceData = {
            RefContact: <RestSchemaReferenceDataInfo>{
                "name": RefContact,
                "action": "getContacts",
                "records_path": "Contacts",
                "value_field": "ContactID",
                "display_field": "Name",
                "desc": "the contacts in this organisation",
            },
            RefInvoice: <RestSchemaReferenceDataInfo>{
                "name": RefInvoice,
                "action": "getInvoices",
                "records_path": "Invoices",
                "value_field": "InvoiceID",
                "display_field": "InvoiceNumber",
                "desc": "the invoices in this organisation",
            },
            RefAccount: <RestSchemaReferenceDataInfo>{
                "name": RefAccount,
                "action": "getAccounts",
                "records_path": "Accounts",
                "value_field": "AccountID",
                "display_field": "Name",
                "desc": "the accounts in the chart of accounts",
            },
            RefAccountCode: <RestSchemaReferenceDataInfo>{
                "name": RefAccountCode,
                "action": "getAccounts",
                "records_path": "Accounts",
                # the same listing, read by code rather than by ID: a line item names one or the other, and
                # which one it names decides what Xero accepts
                "value_field": "Code",
                "display_field": "Name",
                "desc": "the accounts in the chart of accounts, by code",
            },
            RefBankAccount: <RestSchemaReferenceDataInfo>{
                "name": RefBankAccount,
                "action": "getAccounts",
                # a bank transaction can only be posted to a bank account, so the dropdown offers only those
                "options": {"where": "Type==\"BANK\""},
                "records_path": "Accounts",
                "value_field": "AccountID",
                "display_field": "Name",
                "desc": "the bank accounts in this organisation",
            },
            RefItem: <RestSchemaReferenceDataInfo>{
                "name": RefItem,
                "action": "getItems",
                "records_path": "Items",
                "value_field": "ItemID",
                "display_field": "Code",
                "desc": "the items in this organisation",
            },
            RefItemCode: <RestSchemaReferenceDataInfo>{
                "name": RefItemCode,
                "action": "getItems",
                "records_path": "Items",
                "value_field": "Code",
                "display_field": "Code",
                "desc": "the items in this organisation, by code",
            },
            RefTaxType: <RestSchemaReferenceDataInfo>{
                "name": RefTaxType,
                "action": "getTaxRates",
                "records_path": "TaxRates",
                "value_field": "TaxType",
                "display_field": "Name",
                "desc": "the tax rates in this organisation",
            },
            RefCreditNote: <RestSchemaReferenceDataInfo>{
                "name": RefCreditNote,
                "action": "getCreditNotes",
                "records_path": "CreditNotes",
                "value_field": "CreditNoteID",
                "display_field": "CreditNoteNumber",
                "desc": "the credit notes in this organisation",
            },
            RefBankTransaction: <RestSchemaReferenceDataInfo>{
                "name": RefBankTransaction,
                "action": "getBankTransactions",
                "records_path": "BankTransactions",
                "value_field": "BankTransactionID",
                "display_field": "Reference",
                "desc": "the bank transactions in this organisation",
            },
        };

        #! The dropdowns the projects document produces
        /** The contact dropdown is **not** here: contacts are declared in the accounting document, and the
            projects set reaches it as a peer.  Declaring a second one here would mean a second listing to
            keep in step with the first.
        */
        const ProjectsReferenceData = {
            RefProject: <RestSchemaReferenceDataInfo>{
                "name": RefProject,
                "action": "getProjects",
                "options": {"pageSize": 500},
                # the projects API is camelCase where accounting is PascalCase, envelope included
                "records_path": "items",
                "value_field": "projectId",
                "display_field": "name",
                "desc": "the projects in this organisation",
            },
        };

        #! Overlay dropping a field the connection owns
        const ConnectionOwnedOverlay = <RestSchemaFieldOverlayInfo>{
            "ignore": True,
        };

        #! Overlay dropping the response fields Xero declares in its request schemas
        /** Xero's request and response bodies share a schema, so a create carries \c pagination and
            \c Warnings - which are things the server says back, not things a caller sends.
        */
        const ResponseOnlyOverlay = <RestSchemaFieldOverlayInfo>{
            "ignore": True,
        };

        #! Overlay dropping the tenant header, as the accounting document spells it
        const AccountingCommon = {
            "xero-tenant-id": ConnectionOwnedOverlay,
        };

        #! Overlay dropping the tenant header and the idempotency key, for an accounting write
        /** Only a write declares \c Idempotency-Key, and naming an overlay key an operation does not
            declare is an error -- which is what keeps this split honest rather than tidy.
        */
        const AccountingWriteCommon = AccountingCommon + {
            "Idempotency-Key": ConnectionOwnedOverlay,
        };

        #! Overlay dropping the tenant header, as the projects document spells it
        /** The two documents disagree on the case of the header name, so the overlay cannot be shared.
        */
        const ProjectsCommon = {
            "Xero-Tenant-Id": ConnectionOwnedOverlay,
        };

        #! Overlay dropping the tenant header and the idempotency key, for a projects write
        const ProjectsWriteCommon = ProjectsCommon + {
            "Idempotency-Key": ConnectionOwnedOverlay,
        };

        #! Overlay for the contact an action works with
        const ContactOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Contact",
            "short_desc": "The contact this applies to",
            "ref_data": RefContact,
            "supports_custom_values": True,
            "preselected": True,
        };

        #! Overlay for the invoice an action works on
        const InvoiceOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Invoice",
            "short_desc": "The invoice this applies to",
            "ref_data": RefInvoice,
            "supports_custom_values": True,
            "preselected": True,
        };

        #! Overlay for the \c where filter every accounting listing accepts
        const WhereOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Filter",
            "short_desc": "A Xero filter expression",
            "desc": "A filter in Xero's own expression language, ex: `Status==\"AUTHORISED\"`, "
                "`Contact.ContactID=guid(\"...\")`, `Type==\"ACCPAY\"`.\n\n"
                "This is not SQL and not a query string: string values are double-quoted, a GUID is wrapped "
                "in `guid(...)`, and conditions are joined with `AND`.",
        };

        #! Overlay for the \c order argument every accounting listing accepts
        const OrderOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Order",
            "short_desc": "A field to sort by, ex: \"UpdatedDateUTC DESC\"",
            "example_value": "UpdatedDateUTC DESC",
        };

        #! Overlay for the \c page argument
        const PageOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Page",
            "short_desc": "Which page to return, counting from 1",
            "example_value": 1,
        };

        #! Overlay for the \c pageSize argument
        const PageSizeOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Page Size",
            "short_desc": "How many records to return in one page",
            "desc": "How many records to return in one page. Xero's default is 100 and its maximum is "
                + MaxPageSize + ".",
            "example_value": 100,
        };

        #! Overlay for the \c If-Modified-Since header every accounting listing accepts
        const ModifiedSinceOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Modified Since",
            "short_desc": "Return only records created or changed since this time",
            "desc": "Return only records created or changed since this time.\n\n"
                "This is what makes an incremental sweep possible: without it every call reads the whole "
                "collection.",
        };

        #! Overlay for the filter arguments an accounting listing accepts
        const FilterOverlay = {
            "where": WhereOverlay,
            "order": OrderOverlay,
        };

        #! Overlay for the paging arguments an accounting listing accepts
        const PagingOverlay = {
            "page": PageOverlay,
            "pageSize": PageSizeOverlay,
        };

        #! Overlay for the incremental-read header an accounting listing accepts
        const ModifiedOverlay = {
            "If-Modified-Since": ModifiedSinceOverlay,
        };

        #! Overlay for a full accounting listing: filter, paging and incremental read
        /** Not every listing takes all of these -- tax rates take no paging and no "If-Modified-Since",
            accounts and items no paging -- so this is composed per action rather than applied to all.
        */
        const ListingOverlay = AccountingCommon + FilterOverlay + PagingOverlay + ModifiedOverlay;

        #! The operations the application exports from the accounting document
        const AccountingManifest = (
            # --- Contacts ---
            <RestSchemaActionInfo>{
                "action": "getContacts",
                "method": "GET",
                "path": "/Contacts",
                "operation_id": "getContacts",
                "display_name": "Find Contacts",
                "short_desc": "List the contacts in the organisation",
                "desc": "Lists contacts - customers, suppliers and everyone else in the address book.\n\n"
                    "`where` filters with Xero's expression language: `IsCustomer==true`, "
                    "`Name.StartsWith(\"Acme\")`. `IDs` takes a **comma-separated** list of contact IDs "
                    "rather than a list.\n\n"
                    "The response is `{Contacts: [...]}`.",
                "groups": (GroupContacts,),
                "field_order": ("where", "order", "IDs", "searchTerm", "includeArchived", "summaryOnly",
                    "page", "pageSize", "If-Modified-Since"),
                "fields": AccountingCommon + FilterOverlay + PagingOverlay + ModifiedOverlay + {
                    "IDs": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Contact IDs",
                        "short_desc": "A comma-separated list of contact IDs",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "updateOrCreateContacts",
                "method": "POST",
                "path": "/Contacts",
                "operation_id": "updateOrCreateContacts",
                "display_name": "Create or Update Contacts",
                "short_desc": "Create contacts, or update them by ID",
                "desc": "Creates or updates contacts.\n\n"
                    "`Contacts` is an **array** even for one contact - that is Xero's envelope. A member "
                    "carrying `ContactID` updates that contact; one without creates a new one.\n\n"
                    "`summarizeErrors` set to `false` makes Xero accept the members it can and report the "
                    "rest, instead of rejecting the whole batch.",
                "groups": (GroupContacts,),
                "field_order": ("Contacts", "summarizeErrors"),
                "fields": AccountingWriteCommon + {
                    "pagination": ResponseOnlyOverlay,
                    "Warnings": ResponseOnlyOverlay,
                    "Contacts": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Contacts",
                        "short_desc": "The contacts to create or update; an array even for one",
                        "required": True,
                        "preselected": True,
                        "fields": {
                            "ContactID": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Contact",
                                "short_desc": "Set this to update an existing contact",
                                "ref_data": RefContact,
                                "supports_custom_values": True,
                            },
                        },
                    },
                },
            },

            # --- Invoices ---
            <RestSchemaActionInfo>{
                "action": "getInvoices",
                "method": "GET",
                "path": "/Invoices",
                "operation_id": "getInvoices",
                "display_name": "Find Invoices",
                "short_desc": "List invoices and bills",
                "desc": "Lists invoices.\n\n"
                    "**Bills are invoices here**: `Type==\"ACCPAY\"` selects what Xero's UI calls a bill "
                    "and `Type==\"ACCREC\"` a sales invoice, and without a filter both are returned.\n\n"
                    "`IDs`, `InvoiceNumbers`, `ContactIDs` and `Statuses` each take a **comma-separated** "
                    "list. The response is `{Invoices: [...]}`.",
                "groups": (GroupInvoices,),
                "field_order": ("where", "order", "IDs", "InvoiceNumbers", "ContactIDs", "Statuses",
                    "searchTerm", "createdByMyApp", "includeArchived", "summaryOnly", "unitdp", "page",
                    "pageSize", "If-Modified-Since"),
                "fields": AccountingCommon + FilterOverlay + PagingOverlay + ModifiedOverlay + {
                    "IDs": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Invoice IDs",
                        "short_desc": "A comma-separated list of invoice IDs",
                    },
                    "ContactIDs": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Contact IDs",
                        "short_desc": "A comma-separated list of contact IDs",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "updateOrCreateInvoices",
                "method": "POST",
                "path": "/Invoices",
                "operation_id": "updateOrCreateInvoices",
                "display_name": "Create or Update Invoices",
                "short_desc": "Create invoices or bills, or update them by ID",
                "desc": "Creates or updates invoices.\n\n"
                    "`Invoices` is an **array** even for one invoice. Each member needs `Type` - `ACCREC` "
                    "for a sales invoice, `ACCPAY` for a bill - a `Contact`, and `LineItems`.\n\n"
                    "A line item needs an amount and an account: give `AccountCode`, or an `ItemCode` whose "
                    "item already carries one. `Status` decides whether it is a draft: leave it unset for "
                    "`DRAFT`, or set `AUTHORISED` to approve it as it is created.",
                "groups": (GroupInvoices,),
                "field_order": ("Invoices", "summarizeErrors", "unitdp", "allowBackorders"),
                "fields": AccountingWriteCommon + {
                    "pagination": ResponseOnlyOverlay,
                    "Warnings": ResponseOnlyOverlay,
                    "Invoices": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Invoices",
                        "short_desc": "The invoices to create or update; an array even for one",
                        "required": True,
                        "preselected": True,
                        "fields": {
                            "InvoiceID": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Invoice",
                                "short_desc": "Set this to update an existing invoice",
                                "ref_data": RefInvoice,
                                "supports_custom_values": True,
                            },
                            "Contact": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Contact",
                                "preselected": True,
                                "fields": {
                                    "ContactID": ContactOverlay,
                                },
                            },
                            "LineItems": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Line Items",
                                "preselected": True,
                                "fields": {
                                    "AccountID": <RestSchemaFieldOverlayInfo>{
                                        "display_name": "Account",
                                        "ref_data": RefAccount,
                                        "supports_custom_values": True,
                                    },
                                    "AccountCode": <RestSchemaFieldOverlayInfo>{
                                        "display_name": "Account Code",
                                        "ref_data": RefAccountCode,
                                        "supports_custom_values": True,
                                    },
                                    "ItemCode": <RestSchemaFieldOverlayInfo>{
                                        "display_name": "Item Code",
                                        "ref_data": RefItemCode,
                                        "supports_custom_values": True,
                                    },
                                    "TaxType": <RestSchemaFieldOverlayInfo>{
                                        "display_name": "Tax Type",
                                        "ref_data": RefTaxType,
                                        "supports_custom_values": True,
                                    },
                                },
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "getInvoiceHistory",
                "method": "GET",
                "path": "/Invoices/{InvoiceID}/History",
                "operation_id": "getInvoiceHistory",
                "display_name": "Find Invoice History",
                "short_desc": "Read an invoice's history and notes",
                "desc": "Reads the history of an invoice: who changed it, when, and any notes recorded "
                    "against it.",
                "groups": (GroupInvoices,),
                "field_order": ("InvoiceID",),
                "fields": AccountingCommon + {
                    "InvoiceID": InvoiceOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "createInvoiceHistory",
                "method": "PUT",
                "path": "/Invoices/{InvoiceID}/History",
                "operation_id": "createInvoiceHistory",
                "display_name": "Add Note to Invoice",
                "short_desc": "Record a note against an invoice",
                "desc": "Adds a note to an invoice's history.\n\n"
                    "`HistoryRecords` is an **array** even for one note, and the note itself is its "
                    "`Details` property.",
                "groups": (GroupInvoices,),
                "field_order": ("InvoiceID", "HistoryRecords"),
                "fields": AccountingWriteCommon + {
                    "InvoiceID": InvoiceOverlay,
                    "HistoryRecords": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Notes",
                        "short_desc": "The notes to record; each one's text is its \"Details\"",
                        "required": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "emailInvoice",
                "method": "POST",
                "path": "/Invoices/{InvoiceID}/Email",
                "operation_id": "emailInvoice",
                "display_name": "Email Invoice",
                "short_desc": "Send an invoice to its contact by email",
                "desc": "Sends an invoice to the email address on its contact.\n\n"
                    "Xero decides the recipient and the template; there is nothing to fill in but the "
                    "invoice. The invoice has to be `AUTHORISED` - a draft cannot be sent.",
                "groups": (GroupInvoices,),
                "field_order": ("InvoiceID",),
                "fields": AccountingWriteCommon + {
                    "InvoiceID": InvoiceOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "updateOrCreateQuotes",
                "method": "POST",
                "path": "/Quotes",
                "operation_id": "updateOrCreateQuotes",
                "display_name": "Create or Update Quotes",
                "short_desc": "Create quotes, or update them by ID",
                "desc": "Creates or updates quotes.\n\n"
                    "`Quotes` is an **array** even for one quote, and each needs a `Contact`, a `Date` and "
                    "`LineItems`.",
                "groups": (GroupInvoices,),
                "field_order": ("Quotes", "summarizeErrors"),
                "fields": AccountingWriteCommon + {
                    "Quotes": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Quotes",
                        "short_desc": "The quotes to create or update; an array even for one",
                        "required": True,
                        "preselected": True,
                        "fields": {
                            "Contact": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Contact",
                                "preselected": True,
                                "fields": {
                                    "ContactID": ContactOverlay,
                                },
                            },
                            "LineItems": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Line Items",
                                "preselected": True,
                                "fields": {
                                    "AccountID": <RestSchemaFieldOverlayInfo>{
                                        "display_name": "Account",
                                        "ref_data": RefAccount,
                                        "supports_custom_values": True,
                                    },
                                    "ItemCode": <RestSchemaFieldOverlayInfo>{
                                        "display_name": "Item Code",
                                        "ref_data": RefItemCode,
                                        "supports_custom_values": True,
                                    },
                                    "TaxType": <RestSchemaFieldOverlayInfo>{
                                        "display_name": "Tax Type",
                                        "ref_data": RefTaxType,
                                        "supports_custom_values": True,
                                    },
                                },
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "updateOrCreateRepeatingInvoices",
                "method": "POST",
                "path": "/RepeatingInvoices",
                "operation_id": "updateOrCreateRepeatingInvoices",
                "display_name": "Create or Update Repeating Invoices",
                "short_desc": "Set up an invoice that repeats on a schedule",
                "desc": "Creates or updates repeating invoice templates.\n\n"
                    "`Schedule` is what makes it repeat - its period, unit and next scheduled date - and "
                    "`Status` decides whether Xero sends each occurrence (`AUTHORISED`) or leaves it as a "
                    "draft (`DRAFT`).",
                "groups": (GroupInvoices,),
                "field_order": ("RepeatingInvoices", "summarizeErrors"),
                "fields": AccountingWriteCommon + {
                    "RepeatingInvoices": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Repeating Invoices",
                        "short_desc": "The templates to create or update; an array even for one",
                        "required": True,
                        "preselected": True,
                        "fields": {
                            "Contact": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Contact",
                                "preselected": True,
                                "fields": {
                                    "ContactID": ContactOverlay,
                                },
                            },
                            "LineItems": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Line Items",
                                "preselected": True,
                                "fields": {
                                    "AccountID": <RestSchemaFieldOverlayInfo>{
                                        "display_name": "Account",
                                        "ref_data": RefAccount,
                                        "supports_custom_values": True,
                                    },
                                    "AccountCode": <RestSchemaFieldOverlayInfo>{
                                        "display_name": "Account Code",
                                        "ref_data": RefAccountCode,
                                        "supports_custom_values": True,
                                    },
                                    "ItemCode": <RestSchemaFieldOverlayInfo>{
                                        "display_name": "Item Code",
                                        "ref_data": RefItemCode,
                                        "supports_custom_values": True,
                                    },
                                    "TaxType": <RestSchemaFieldOverlayInfo>{
                                        "display_name": "Tax Type",
                                        "ref_data": RefTaxType,
                                        "supports_custom_values": True,
                                    },
                                },
                            },
                        },
                    },
                },
            },

            # --- Payments, credit notes and bank transactions ---
            <RestSchemaActionInfo>{
                "action": "getPayments",
                "method": "GET",
                "path": "/Payments",
                "operation_id": "getPayments",
                "display_name": "Find Payments",
                "short_desc": "List the payments in the organisation",
                "desc": "Lists payments made against invoices and credit notes.\n\n"
                    "This is exported so that the **New Payment** event has something to read: the "
                    "application this module replaces polled payments without exporting any way to list "
                    "one.\n\n"
                    "The response is `{Payments: [...]}`.",
                "groups": (GroupPayments,),
                "field_order": ("where", "order", "page", "pageSize", "If-Modified-Since"),
                "fields": AccountingCommon + FilterOverlay + PagingOverlay + ModifiedOverlay,
            },
            <RestSchemaActionInfo>{
                "action": "createPayment",
                "method": "POST",
                "path": "/Payments",
                "operation_id": "createPayment",
                "display_name": "Create Payment",
                "short_desc": "Record a payment against an invoice",
                "desc": "Records a payment.\n\n"
                    "Unlike most of the accounting API this takes **one** payment rather than an envelope "
                    "of them. It needs the `Invoice` it pays, the `Account` it is paid from or into, an "
                    "`Amount` and a `Date`.\n\n"
                    "The account has to be one that allows payments - a bank account, in practice - and "
                    "the invoice has to be `AUTHORISED`.",
                "groups": (GroupPayments,),
                "field_order": ("Invoice", "Account", "Amount", "Date", "Reference", "CreditNote",
                    "Status"),
                "fields": AccountingWriteCommon + {
                    "Warnings": ResponseOnlyOverlay,
                    "ValidationErrors": ResponseOnlyOverlay,
                    "HasAccount": ResponseOnlyOverlay,
                    "HasValidationErrors": ResponseOnlyOverlay,
                    "UpdatedDateUTC": ResponseOnlyOverlay,
                    "UpdatedDateUTCString": ResponseOnlyOverlay,
                    "StatusAttributeString": ResponseOnlyOverlay,
                    "Invoice": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Invoice",
                        "short_desc": "The invoice being paid",
                        "preselected": True,
                        "fields": {
                            "InvoiceID": InvoiceOverlay,
                        },
                    },
                    "Account": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account",
                        "short_desc": "The account the payment is made from or into",
                        "preselected": True,
                        "fields": {
                            "AccountID": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Account",
                                "ref_data": RefBankAccount,
                                "supports_custom_values": True,
                                "preselected": True,
                            },
                        },
                    },
                    "CreditNote": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Credit Note",
                        "short_desc": "The credit note being paid, instead of an invoice",
                        "fields": {
                            "CreditNoteID": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Credit Note",
                                "ref_data": RefCreditNote,
                                "supports_custom_values": True,
                            },
                        },
                    },
                    "Amount": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Amount",
                        "preselected": True,
                    },
                    "Date": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Date",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "getCreditNotes",
                "method": "GET",
                "path": "/CreditNotes",
                "operation_id": "getCreditNotes",
                "display_name": "Find Credit Notes",
                "short_desc": "List the credit notes in the organisation",
                "desc": "Lists credit notes.\n\n"
                    "This is exported so that the credit note dropdown and the **New Credit Note** event "
                    "have something to read; the application this module replaces offered both without "
                    "exporting the endpoint behind them.\n\n"
                    "The response is `{CreditNotes: [...]}`.",
                "groups": (GroupPayments,),
                "field_order": ("where", "order", "unitdp", "page", "pageSize", "If-Modified-Since"),
                "fields": AccountingCommon + FilterOverlay + PagingOverlay + ModifiedOverlay,
            },
            <RestSchemaActionInfo>{
                "action": "createCreditNoteAllocation",
                "method": "PUT",
                "path": "/CreditNotes/{CreditNoteID}/Allocations",
                "operation_id": "createCreditNoteAllocation",
                "display_name": "Allocate Credit Note",
                "short_desc": "Apply a credit note to an invoice",
                "desc": "Applies a credit note to one or more invoices.\n\n"
                    "`Allocations` is an **array** even for one allocation, and each names the `Invoice` it "
                    "applies to and the `Amount` applied. The credit note has to have enough remaining "
                    "credit, and the invoice has to be `AUTHORISED`.",
                "groups": (GroupPayments,),
                "field_order": ("CreditNoteID", "Allocations", "summarizeErrors"),
                "fields": AccountingWriteCommon + {
                    "CreditNoteID": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Credit Note",
                        "ref_data": RefCreditNote,
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "Allocations": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Allocations",
                        "short_desc": "The invoices to apply the credit to, and how much to each",
                        "required": True,
                        "preselected": True,
                        "fields": {
                            "Invoice": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Invoice",
                                "preselected": True,
                                "fields": {
                                    "InvoiceID": InvoiceOverlay,
                                },
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "getBankTransactions",
                "method": "GET",
                "path": "/BankTransactions",
                "operation_id": "getBankTransactions",
                "display_name": "Find Bank Transactions",
                "short_desc": "List the bank transactions in the organisation",
                "desc": "Lists bank transactions - spend and receive money entries against a bank "
                    "account.\n\n"
                    "This is exported so that the bank transaction dropdown and the **New Bank "
                    "Transaction** event have something to read.\n\n"
                    "The response is `{BankTransactions: [...]}`.",
                "groups": (GroupPayments,),
                "field_order": ("where", "order", "References", "unitdp", "page", "pageSize",
                    "If-Modified-Since"),
                "fields": AccountingCommon + FilterOverlay + PagingOverlay + ModifiedOverlay,
            },
            <RestSchemaActionInfo>{
                "action": "updateOrCreateBankTransactions",
                "method": "POST",
                "path": "/BankTransactions",
                "operation_id": "updateOrCreateBankTransactions",
                "display_name": "Create Bank Transaction",
                "short_desc": "Record money spent or received against a bank account",
                "desc": "Creates or updates bank transactions.\n\n"
                    "`BankTransactions` is an **array** even for one. Each needs a `Type` - `SPEND` or "
                    "`RECEIVE` - a `Contact`, a `BankAccount` and `LineItems`.\n\n"
                    "The bank account must be an account of type `BANK`, which is what its dropdown "
                    "offers; a line item needs `AccountCode` or `AccountID` to say which account it posts "
                    "to.",
                "groups": (GroupPayments,),
                "field_order": ("BankTransactions", "summarizeErrors", "unitdp"),
                "fields": AccountingWriteCommon + {
                    "pagination": ResponseOnlyOverlay,
                    "Warnings": ResponseOnlyOverlay,
                    "BankTransactions": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Bank Transactions",
                        "short_desc": "The transactions to create or update; an array even for one",
                        "required": True,
                        "preselected": True,
                        "fields": {
                            "Contact": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Contact",
                                "preselected": True,
                                "fields": {
                                    "ContactID": ContactOverlay,
                                },
                            },
                            "BankAccount": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Bank Account",
                                "preselected": True,
                                "fields": {
                                    "AccountID": <RestSchemaFieldOverlayInfo>{
                                        "display_name": "Bank Account",
                                        "ref_data": RefBankAccount,
                                        "supports_custom_values": True,
                                        "preselected": True,
                                    },
                                },
                            },
                            "LineItems": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Line Items",
                                "preselected": True,
                                "fields": {
                                    "AccountID": <RestSchemaFieldOverlayInfo>{
                                        "display_name": "Account",
                                        "ref_data": RefAccount,
                                        "supports_custom_values": True,
                                    },
                                    "AccountCode": <RestSchemaFieldOverlayInfo>{
                                        "display_name": "Account Code",
                                        "ref_data": RefAccountCode,
                                        "supports_custom_values": True,
                                    },
                                    "ItemCode": <RestSchemaFieldOverlayInfo>{
                                        "display_name": "Item Code",
                                        "ref_data": RefItemCode,
                                        "supports_custom_values": True,
                                    },
                                    "TaxType": <RestSchemaFieldOverlayInfo>{
                                        "display_name": "Tax Type",
                                        "ref_data": RefTaxType,
                                        "supports_custom_values": True,
                                    },
                                },
                            },
                        },
                    },
                },
            },

            # --- Items, accounts and tax rates ---
            <RestSchemaActionInfo>{
                "action": "getItems",
                "method": "GET",
                "path": "/Items",
                "operation_id": "getItems",
                "display_name": "Find Items",
                "short_desc": "List the items in the organisation",
                "desc": "Lists inventory and service items with their codes, prices and accounts.\n\n"
                    "The response is `{Items: [...]}`.",
                "groups": (GroupSettings,),
                "field_order": ("where", "order", "unitdp", "If-Modified-Since"),
                "fields": AccountingCommon + FilterOverlay + ModifiedOverlay,
            },
            <RestSchemaActionInfo>{
                "action": "updateOrCreateItems",
                "method": "POST",
                "path": "/Items",
                "operation_id": "updateOrCreateItems",
                "display_name": "Create or Update Items",
                "short_desc": "Create items, or update them by ID",
                "desc": "Creates or updates items.\n\n"
                    "`Items` is an **array** even for one item, and `Code` is what identifies an item to a "
                    "line item elsewhere.",
                "groups": (GroupSettings,),
                "field_order": ("Items", "summarizeErrors", "unitdp"),
                "fields": AccountingWriteCommon + {
                    "Items": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Items",
                        "short_desc": "The items to create or update; an array even for one",
                        "required": True,
                        "preselected": True,
                        "fields": {
                            "ItemID": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Item",
                                "short_desc": "Set this to update an existing item",
                                "ref_data": RefItem,
                                "supports_custom_values": True,
                            },
                            "Code": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Code",
                                "short_desc": "The item code, which is how line items name it",
                                "ref_data": RefItemCode,
                                "supports_custom_values": True,
                                "preselected": True,
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "getAccounts",
                "method": "GET",
                "path": "/Accounts",
                "operation_id": "getAccounts",
                "display_name": "Find Accounts",
                "short_desc": "List the chart of accounts",
                "desc": "Lists the chart of accounts with each account's code, name, type and tax type.\n\n"
                    "This is what backs the account, account-code and bank-account dropdowns; the "
                    "application this module replaces offered all three without exporting the endpoint "
                    "that fills them. `Type==\"BANK\"` narrows it to bank accounts.\n\n"
                    "The response is `{Accounts: [...]}`.",
                "groups": (GroupSettings,),
                "field_order": ("where", "order", "If-Modified-Since"),
                "fields": AccountingCommon + FilterOverlay + ModifiedOverlay,
            },
            <RestSchemaActionInfo>{
                "action": "getTaxRates",
                "method": "GET",
                "path": "/TaxRates",
                "operation_id": "getTaxRates",
                "display_name": "Find Tax Rates",
                "short_desc": "List the tax rates in the organisation",
                "desc": "Lists tax rates with the `TaxType` code a line item uses to name one.\n\n"
                    "This is what backs the tax type dropdown; the application this module replaces "
                    "offered it without exporting the endpoint that fills it.\n\n"
                    "The response is `{TaxRates: [...]}`.",
                "groups": (GroupSettings,),
                "field_order": ("where", "order"),
                "fields": AccountingCommon + FilterOverlay,
            },

            # --- Purchasing ---
            <RestSchemaActionInfo>{
                "action": "getPurchaseOrders",
                "method": "GET",
                "path": "/PurchaseOrders",
                "operation_id": "getPurchaseOrders",
                "display_name": "Find Purchase Orders",
                "short_desc": "List the purchase orders in the organisation",
                "desc": "Lists purchase orders.\n\n"
                    "`Status` selects one state, and `DateFrom` and `DateTo` bound the listing by order "
                    "date. Xero writes those two as calendar dates, so they take a date here and are sent "
                    "as `YYYY-MM-DD`.\n\n"
                    "The response is `{PurchaseOrders: [...]}`.",
                "groups": (GroupPurchasing,),
                "field_order": ("Status", "DateFrom", "DateTo", "order", "page", "pageSize",
                    "If-Modified-Since"),
                "fields": AccountingCommon + {"order": OrderOverlay} + PagingOverlay + ModifiedOverlay + {
                    "DateFrom": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Date From",
                        "short_desc": "Return only orders dated on or after this day",
                        "codec": XeroCodecs::QueryDate,
                    },
                    "DateTo": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Date To",
                        "short_desc": "Return only orders dated on or before this day",
                        "codec": XeroCodecs::QueryDate,
                    },
                },
            },
        );

        #! The operations the application exports from the projects document
        const ProjectsManifest = (
            <RestSchemaActionInfo>{
                "action": "getProjects",
                "method": "GET",
                "path": "/Projects",
                "operation_id": "getProjects",
                "display_name": "Find Projects",
                "short_desc": "List the projects in the organisation",
                "desc": "Lists projects.\n\n"
                    "`contactID` narrows the listing to one contact's projects and `states` to `INPROGRESS` "
                    "or `CLOSED`.\n\n"
                    "The projects API answers `{items, pagination}` in `camelCase`, where the accounting "
                    "API answers a `PascalCase` envelope named after the type.",
                "groups": (GroupProjects,),
                "field_order": ("contactID", "states", "projectIds", "page", "pageSize"),
                "fields": ProjectsCommon + {
                    "contactID": ContactOverlay + <RestSchemaFieldOverlayInfo>{
                        "short_desc": "Return only this contact's projects",
                        "preselected": False,
                    },
                    "projectIds": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Projects",
                        "short_desc": "Return only these projects",
                        "element_ref_data": RefProject,
                        "supports_custom_values": True,
                    },
                    "pageSize": PageSizeOverlay,
                    "page": PageOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "createProject",
                "method": "POST",
                "path": "/Projects",
                "operation_id": "createProject",
                "display_name": "Create Project",
                "short_desc": "Create a project for a contact",
                "desc": "Creates a project.\n\n"
                    "`contactId` and `name` are required; `estimateAmount` and `deadlineUtc` are the "
                    "budget and the due date.",
                "groups": (GroupProjects,),
                "field_order": ("contactId", "name", "estimateAmount", "deadlineUtc"),
                "fields": ProjectsWriteCommon + {
                    "contactId": ContactOverlay + <RestSchemaFieldOverlayInfo>{
                        "short_desc": "The contact the project is for",
                        "required": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "getProjectUsers",
                "method": "GET",
                "path": "/ProjectsUsers",
                "operation_id": "getProjectUsers",
                "display_name": "Find Project Users",
                "short_desc": "List the users who can be assigned to project tasks",
                "desc": "Lists the users of the projects API - the people time can be recorded against.",
                "groups": (GroupProjects,),
                "field_order": ("page", "pageSize"),
                "fields": ProjectsCommon + {
                    "pageSize": PageSizeOverlay,
                    "page": PageOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "getTasks",
                "method": "GET",
                "path": "/Projects/{projectId}/Tasks",
                "operation_id": "getTasks",
                "display_name": "Find Tasks",
                "short_desc": "List the tasks in a project",
                "desc": "Lists a project's tasks with their rates, charge types and estimates.",
                "groups": (GroupProjects,),
                "field_order": ("projectId", "chargeType", "taskIds", "page", "pageSize"),
                "fields": ProjectsCommon + {
                    "projectId": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Project",
                        "ref_data": RefProject,
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "pageSize": PageSizeOverlay,
                    "page": PageOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "createTask",
                "method": "POST",
                "path": "/Projects/{projectId}/Tasks",
                "operation_id": "createTask",
                "display_name": "Create Task",
                "short_desc": "Add a task to a project",
                "desc": "Creates a task in a project.\n\n"
                    "`chargeType` decides how it is billed - `TIME`, `FIXED` or `NON_CHARGEABLE` - and a "
                    "`TIME` task needs a `rate`.",
                "groups": (GroupProjects,),
                "field_order": ("projectId", "name", "chargeType", "rate", "estimateMinutes"),
                "fields": ProjectsWriteCommon + {
                    "projectId": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Project",
                        "ref_data": RefProject,
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "preselected": True,
                    },
                },
            },
        );
    }
}
}
