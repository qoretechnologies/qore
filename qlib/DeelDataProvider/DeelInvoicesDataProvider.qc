# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelInvoicesDataProvider.qc Deel invoices collection data provider

/*  DeelInvoicesDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DeelDataProvider module
public namespace DeelDataProvider {

#! Deel invoices collection data provider
public class DeelInvoicesDataProvider inherits DeelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "invoices",
            "desc": "Deel invoices data provider for listing and searching invoices",
            "type": "DeelInvoicesDataProvider",
            "supports_read": True,
            "has_record": True,
            "supports_native_search": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "invoice_id": <DataProviderOptionInfo>{
                "display_name": "Invoice ID",
                "short_desc": "Get a specific invoice by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by invoice ID for single record lookup",
            },
            "contract_id": <DataProviderOptionInfo>{
                "display_name": "Contract ID",
                "short_desc": "Filter by contract",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter invoices by associated contract ID",
            },
            "status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by invoice status",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter invoices by status (e.g., `pending`, `paid`, `cancelled`)",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "pending", "desc": "Pending invoices"},
                    <AllowedValueInfo>{"value": "paid", "desc": "Paid invoices"},
                    <AllowedValueInfo>{"value": "cancelled", "desc": "Cancelled invoices"},
                ),
            },
            "date_from": <DataProviderOptionInfo>{
                "display_name": "Date From",
                "short_desc": "Start of date range",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter invoices from this date (YYYY-MM-DD format)",
            },
            "date_to": <DataProviderOptionInfo>{
                "display_name": "Date To",
                "short_desc": "End of date range",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter invoices up to this date (YYYY-MM-DD format)",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
            "offset": <DataProviderOptionInfo>{
                "display_name": "Offset",
                "short_desc": "Results offset",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Number of results to skip (for manual pagination)",
            },
        };
    }

    #! Creates the object with a REST client
    constructor(DeelRestClient::DeelRestClient rest) : DeelDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "invoices";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Deel invoices data provider for listing and searching invoices";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new DeelInvoiceDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If invoice_id provided, get single record
        if (search_options.invoice_id) {
            hash<auto> result = doGet("invoices/" + encode_url(search_options.invoice_id, True));
            list<hash<auto>> data = result.data ? (result.data,) : ();
            return new DefaultRecordIterator(new ListHashIterator(data),
                where_cond, search_options);
        }

        # Build query params for listing
        hash<auto> params;
        if (search_options.contract_id) {
            params.contractId = search_options.contract_id;
        }
        if (search_options.status) {
            params.status = search_options.status;
        }
        if (search_options.date_from) {
            params.dateFrom = search_options.date_from;
        }
        if (search_options.date_to) {
            params.dateTo = search_options.date_to;
        }

        list<hash<auto>> all_records = fetchAllPages("invoices", "invoices", params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DeelDataProvider
