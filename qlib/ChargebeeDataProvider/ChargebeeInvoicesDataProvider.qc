# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ChargebeeDataProvider module definition

/*  ChargebeeInvoicesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Chargebee invoices collection data provider
public class ChargebeeInvoicesDataProvider inherits ChargebeeDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "invoices",
            "desc": "Chargebee invoices data provider; list and search invoices",
            "type": "ChargebeeInvoicesDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Invoice ID",
                "short_desc": "Get a specific invoice by ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter by invoice ID for single record lookup",
            },
            "customer_id": <DataProviderOptionInfo>{
                "display_name": "Customer ID",
                "short_desc": "Filter by customer ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter invoices by customer ID (customer_id[is])",
            },
            "subscription_id": <DataProviderOptionInfo>{
                "display_name": "Subscription ID",
                "short_desc": "Filter by subscription ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter invoices by subscription ID (subscription_id[is])",
            },
            "status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by invoice status",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter invoices by status (status[is])",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "paid", "desc": "Paid invoice"},
                    <AllowedValueInfo>{"value": "posted", "desc": "Posted invoice"},
                    <AllowedValueInfo>{"value": "payment_due", "desc": "Payment due"},
                    <AllowedValueInfo>{"value": "not_paid", "desc": "Not paid"},
                    <AllowedValueInfo>{"value": "voided", "desc": "Voided invoice"},
                    <AllowedValueInfo>{"value": "pending", "desc": "Pending invoice"},
                ),
            },
            "recurring": <DataProviderOptionInfo>{
                "display_name": "Recurring",
                "short_desc": "Filter by recurring flag",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "Filter invoices by recurring flag (recurring[is])",
            },
            "date_from": <DataProviderOptionInfo>{
                "display_name": "Date From",
                "short_desc": "Filter by invoice date (after)",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter invoices with date at or after this Unix timestamp (date[after])",
            },
            "date_to": <DataProviderOptionInfo>{
                "display_name": "Date To",
                "short_desc": "Filter by invoice date (before)",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter invoices with date at or before this Unix timestamp (date[before])",
            },
            "updated_at_from": <DataProviderOptionInfo>{
                "display_name": "Updated At From",
                "short_desc": "Filter by update date (after)",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter invoices updated at or after this Unix timestamp (updated_at[after])",
            },
            "updated_at_to": <DataProviderOptionInfo>{
                "display_name": "Updated At To",
                "short_desc": "Filter by update date (before)",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter invoices updated at or before this Unix timestamp (updated_at[before])",
            },
            "sort_by": <DataProviderOptionInfo>{
                "display_name": "Sort By",
                "short_desc": "Sort field and direction",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Sort results by field and direction (e.g., sort_by[asc] or sort_by[desc])",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "date[asc]", "desc": "Sort by invoice date ascending"},
                    <AllowedValueInfo>{"value": "date[desc]", "desc": "Sort by invoice date descending"},
                    <AllowedValueInfo>{"value": "updated_at[asc]", "desc": "Sort by update date ascending"},
                    <AllowedValueInfo>{"value": "updated_at[desc]", "desc": "Sort by update date descending"},
                ),
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results per page",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Maximum number of results to return (1-100, default 100)",
            },
            "offset": <DataProviderOptionInfo>{
                "display_name": "Offset",
                "short_desc": "Pagination cursor",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Cursor string from previous response's next_offset for pagination",
            },
        };
    }

    private {
        #! Record type
        static ChargebeeInvoiceDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(ChargebeeRestClient::ChargebeeRestClient rest)
            : ChargebeeDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "invoices";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Chargebee invoices data provider; list and search invoices";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for invoices
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If id provided, get single record
        if (search_options.id) {
            hash<auto> response = doGet("/invoices/" + search_options.id);
            return new DefaultRecordIterator(new ListHashIterator((response.invoice,)),
                where_cond, search_options, recordType.getFields());
        }

        # Build query parameters from search options with Chargebee filter notation
        hash<auto> params;
        if (search_options.customer_id) {
            params."customer_id[is]" = search_options.customer_id;
        }
        if (search_options.subscription_id) {
            params."subscription_id[is]" = search_options.subscription_id;
        }
        if (search_options.status) {
            params."status[is]" = search_options.status;
        }
        if (exists search_options.recurring) {
            params."recurring[is]" = search_options.recurring ? "true" : "false";
        }
        if (search_options.date_from) {
            params."date[after]" = search_options.date_from;
        }
        if (search_options.date_to) {
            params."date[before]" = search_options.date_to;
        }
        if (search_options.updated_at_from) {
            params."updated_at[after]" = search_options.updated_at_from;
        }
        if (search_options.updated_at_to) {
            params."updated_at[before]" = search_options.updated_at_to;
        }
        if (search_options.sort_by) {
            params.sort_by = search_options.sort_by;
        }
        if (search_options.limit) {
            params.limit = search_options.limit;
        }
        if (search_options.offset) {
            params.offset = search_options.offset;
        }

        hash<auto> response = doGet("/invoices", params);
        list<hash<auto>> records = map $1.invoice, response.list, $1.invoice;
        if (!records) {
            records = ();
        }

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ChargebeeDataProvider
