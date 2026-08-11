# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore PaddleDataProvider module definition

/*  PaddleRecordTables.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
#! The Paddle record-based tables
/** Five of Paddle's collections are record-shaped and are exposed as tables: \c customers, \c products,
    \c prices, \c transactions and \c reports.  Each is built from actions the manifest already exports.

    @section paddlerecord_paging Paddle Pages Forward Only

    A Paddle listing reports <tt>meta.pagination.has_more</tt> and a <tt>meta.pagination.next</tt> URL
    carrying the \c after argument that starts the next page.  \c has_more is authoritative and is what ends
    a walk: Paddle returns the \c next URL even on the last page, so a reader that only looked for one would
    never stop.  \c per_page is capped at 200 and is asked for in full.

    @section paddlerecord_nodelete Paddle Archives Rather Than Deletes

    None of these entities can be deleted: a customer, a product or a price is **archived** by setting its
    \c status, which is an update.  So the tables support read, create and update, and no delete action is
    registered.  \c transactions and \c reports cannot be updated either - a transaction's lifecycle is
    Paddle's, and a report is produced once and then downloaded.

    @section paddlerecord_pushdown What Paddle Filters

    Paddle's listings filter exactly on identity and status, and this module sends those: a customer by
    \c id, \c email or \c status, a price by its product, a transaction by its customer, subscription,
    invoice number or origin.  Everything else in a search is evaluated here as the records arrive.

    The timestamp filters are sent as narrowing hints rather than relied on.  Paddle's \c created_at and its
    siblings accept an operator suffix - <tt>created_at[GT]</tt> and so on - and a bare value is a form the
    documentation does not pin down, so the predicate is applied again to what comes back.  A search is
    therefore correct whichever way Paddle reads it.

    \c search is Paddle's own loose match over a customer's name and email and is not a predicate at all, so
    it stays an ordinary search option rather than being wired to an operator.
*/
public class PaddleRecordTables {
    public {
        #! The action group the record-based actions belong to
        const RecordGroup = "Record-Based";

        #! How every Paddle collection pages
        const Paging = <RestSchemaRecordPagingInfo>{
            "mode": RSRP_CURSOR,
            "page_option": "after",
            "size_option": "per_page",
            "max_size": 200,
            "next_path": ("meta", "pagination", "next"),
            "has_more_path": ("meta", "pagination", "has_more"),
            "next_cursor": \PaddleRecordTables::cursorFromLink(),
        };

        #! Filters every Paddle listing offers
        const CommonFilters = (
            <RestSchemaRecordFilterInfo>{
                "field": "id", "operator": DP_SEARCH_OP_EQ, "option": "id", "exact": True,
            },
            <RestSchemaRecordFilterInfo>{
                "field": "id", "operator": DP_SEARCH_OP_IN, "option": "id", "multi": True, "exact": True,
            },
            <RestSchemaRecordFilterInfo>{
                "field": "status", "operator": DP_SEARCH_OP_EQ, "option": "status", "exact": True,
            },
            <RestSchemaRecordFilterInfo>{
                "field": "status", "operator": DP_SEARCH_OP_IN, "option": "status", "multi": True,
                "exact": True,
            },
        );

        #! The customers table
        const CustomersTable = <RestSchemaRecordTableInfo>{
            "name": "customers",
            "display_name": "Customers",
            "short_desc": "Paddle customers as records",
            "desc": "Paddle customers as records; a customer is archived by setting its status rather than "
                "deleted",
            "list_action": "list_customers",
            "get_action": "get_customer",
            "create_action": "create_customer",
            "update_action": "update_customer",
            "id_field": "id",
            "id_option": "customer_id",
            "records_path": ("data",),
            "record_path": ("data",),
            "paging": Paging,
            "filters": CommonFilters + (
                <RestSchemaRecordFilterInfo>{
                    "field": "email", "operator": DP_SEARCH_OP_EQ, "option": "email", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "email", "operator": DP_SEARCH_OP_IN, "option": "email", "multi": True,
                    "exact": True,
                },
            ),
            "search_options": ("search", "order_by"),
        };

        #! The products table
        const ProductsTable = <RestSchemaRecordTableInfo>{
            "name": "products",
            "display_name": "Products",
            "short_desc": "Paddle products as records",
            "desc": "Paddle products as records; a product is archived by setting its status rather than "
                "deleted",
            "list_action": "list_products",
            "get_action": "get_product",
            "create_action": "create_product",
            "update_action": "update_product",
            "id_field": "id",
            "id_option": "product_id",
            "records_path": ("data",),
            "record_path": ("data",),
            "paging": Paging,
            "filters": CommonFilters + (
                <RestSchemaRecordFilterInfo>{
                    "field": "tax_category", "operator": DP_SEARCH_OP_EQ, "option": "tax_category",
                    "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "type", "operator": DP_SEARCH_OP_EQ, "option": "type", "exact": True,
                },
            ),
            "search_options": ("include", "order_by"),
        };

        #! The prices table
        const PricesTable = <RestSchemaRecordTableInfo>{
            "name": "prices",
            "display_name": "Prices",
            "short_desc": "Paddle prices as records",
            "desc": "Paddle prices as records; a price is archived by setting its status rather than deleted",
            "list_action": "list_prices",
            "get_action": "get_price",
            "create_action": "create_price",
            "update_action": "update_price",
            "id_field": "id",
            "id_option": "price_id",
            "records_path": ("data",),
            "record_path": ("data",),
            "paging": Paging,
            "filters": CommonFilters + (
                <RestSchemaRecordFilterInfo>{
                    "field": "product_id", "operator": DP_SEARCH_OP_EQ, "option": "product_id",
                    "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "product_id", "operator": DP_SEARCH_OP_IN, "option": "product_id",
                    "multi": True, "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "type", "operator": DP_SEARCH_OP_EQ, "option": "type", "exact": True,
                },
            ),
            "search_options": ("include", "recurring", "order_by"),
        };

        #! The transactions table
        /** A transaction's lifecycle belongs to Paddle: it is billed, paid and completed by the platform
            rather than edited, and the manifest exports no update for one.
        */
        const TransactionsTable = <RestSchemaRecordTableInfo>{
            "name": "transactions",
            "display_name": "Transactions",
            "short_desc": "Paddle transactions as records",
            "desc": "Paddle transactions as records; a transaction's lifecycle belongs to Paddle and cannot "
                "be edited through this table",
            "list_action": "list_transactions",
            "get_action": "get_transaction",
            "create_action": "create_transaction",
            "id_field": "id",
            "id_option": "transaction_id",
            "records_path": ("data",),
            "record_path": ("data",),
            "paging": Paging,
            "filters": CommonFilters + (
                <RestSchemaRecordFilterInfo>{
                    "field": "customer_id", "operator": DP_SEARCH_OP_EQ, "option": "customer_id",
                    "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "customer_id", "operator": DP_SEARCH_OP_IN, "option": "customer_id",
                    "multi": True, "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "subscription_id", "operator": DP_SEARCH_OP_EQ, "option": "subscription_id",
                    "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "invoice_number", "operator": DP_SEARCH_OP_EQ, "option": "invoice_number",
                    "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "origin", "operator": DP_SEARCH_OP_EQ, "option": "origin", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "collection_mode", "operator": DP_SEARCH_OP_EQ, "option": "collection_mode",
                    "exact": True,
                },
                # a bare value on one of Paddle's timestamp arguments is a form its documentation does not
                # pin down - the operator forms are "created_at[GT]" and so on - so it narrows the listing
                # and the predicate is applied again to what comes back
                <RestSchemaRecordFilterInfo>{
                    "field": "created_at", "operator": DP_SEARCH_OP_EQ, "option": "created_at",
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "updated_at", "operator": DP_SEARCH_OP_EQ, "option": "updated_at",
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "billed_at", "operator": DP_SEARCH_OP_EQ, "option": "billed_at",
                },
            ),
            "search_options": ("include", "order_by"),
        };

        #! The reports table's columns
        /** Paddle describes a report as **one of seven models**, one per report type, so its listing has no
            single element type to derive a record type from.  All seven declare the same eight properties
            and differ only in the values \c type takes and the shape of \c filters, so the columns are
            named here rather than left underived - a table with no columns would be worse than useless.
        */
        const ReportFields = (
            {"name": "id", "desc": "the report ID; not writable", "type": "*string"},
            {"name": "status", "desc": "the report state: `pending`, `ready`, `failed` or `expired`",
                "type": "*string"},
            {"name": "type", "desc": "what the report covers, ex: `transactions`", "type": "*string"},
            {"name": "rows", "desc": "how many rows the report holds", "type": "*int"},
            {"name": "filters", "desc": "the filters the report was requested with", "type": "*list"},
            {"name": "created_at", "desc": "when the report was requested; not writable", "type": "*string"},
            {"name": "updated_at", "desc": "when the report last changed; not writable", "type": "*string"},
            {"name": "expires_at", "desc": "when the report stops being downloadable", "type": "*string"},
        );

        #! The reports table
        /** A report is requested once and then downloaded; Paddle offers no update and no delete for one.
        */
        const ReportsTable = <RestSchemaRecordTableInfo>{
            "name": "reports",
            "display_name": "Reports",
            "short_desc": "Paddle reports as records",
            "desc": "Paddle reports as records; a report is requested once and then downloaded, and cannot "
                "be changed",
            "list_action": "list_reports",
            "get_action": "get_report",
            "create_action": "create_report",
            "id_field": "id",
            "id_option": "report_id",
            "records_path": ("data",),
            "record_path": ("data",),
            "paging": Paging,
            "record_type": RestSchemaTableDataProvider::makeRecordType(ReportFields),
            "read_only_fields": ("id", "status", "rows", "created_at", "updated_at", "expires_at"),
            "filters": (
                <RestSchemaRecordFilterInfo>{
                    "field": "status", "operator": DP_SEARCH_OP_EQ, "option": "status", "exact": True,
                },
                <RestSchemaRecordFilterInfo>{
                    "field": "status", "operator": DP_SEARCH_OP_IN, "option": "status", "multi": True,
                    "exact": True,
                },
            ),
            "search_options": ("order_by",),
        };

        #! Every table, in presentation order
        const Tables = (CustomersTable, ProductsTable, PricesTable, TransactionsTable, ReportsTable);

        #! The IDs the record-based actions are registered under
        /** The application this module replaces offered no record-based actions, so these are the
            conventional IDs.  No delete action is registered: nothing in this application can be deleted.
        */
        const RecordActionIds = <RestSchemaRecordActionIds>{
            "delete_records": NOTHING,
        };
    }

    #! Returns the cursor carried by a next-page link
    /** Paddle reports the next page as a URL repeating the original query with an \c after argument added,
        rather than as the cursor itself.
    */
    static *string cursorFromLink(auto value) {
        if (!value.val()) {
            return;
        }
        *list<string> m = (value.toString() =~ x/[?&]after=([^&]+)/);
        if (!m) {
            return;
        }
        return decode_url(m[0]);
    }
}
}
