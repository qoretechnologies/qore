# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInvoiceDataProvider module definition

/*  ZohoInvoiceRecurringInvoicesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ZohoInvoiceDataProvider module
public namespace ZohoInvoiceDataProvider {

#! Zoho Invoice recurring invoices data provider - lists and searches recurring invoices
public class ZohoInvoiceRecurringInvoicesDataProvider inherits ZohoInvoiceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "recurring-invoices",
            "desc": "Zoho Invoice recurring invoices data provider",
            "type": "ZohoInvoiceRecurringInvoicesDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "organization_id": <DataProviderOptionInfo>{
                "display_name": "Organization",
                "short_desc": "Zoho Invoice organization",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The Zoho Invoice organization ID (handled automatically by REST client)",
            },
            "recurring_invoice_id": <DataProviderOptionInfo>{
                "display_name": "Recurring Invoice ID",
                "short_desc": "Get specific recurring invoice",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Get a specific recurring invoice by ID (for DPAT_FIND_SINGLE)",
            },
            "status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by status",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter recurring invoices by status (active, stopped, expired)",
            },
            "customer_id": <DataProviderOptionInfo>{
                "display_name": "Customer ID",
                "short_desc": "Filter by customer",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter recurring invoices by customer ID",
            },
            "recurrence_name": <DataProviderOptionInfo>{
                "display_name": "Recurrence Name",
                "short_desc": "Search by name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Search recurring invoices by recurrence name",
            },
            "page": <DataProviderOptionInfo>{
                "display_name": "Page",
                "short_desc": "Page number",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Page number for pagination",
            },
            "per_page": <DataProviderOptionInfo>{
                "display_name": "Per Page",
                "short_desc": "Results per page",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Number of results per page (max 200)",
            },
        };
    }

    #! Creates the object with a REST client
    constructor(ZohoInvoiceRestClient::ZohoInvoiceRestClient rest) : ZohoInvoiceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "recurring-invoices";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Zoho Invoice recurring invoices data provider for listing and searching recurring invoices";
    }

    #! Returns the record type
    private *AbstractDataProviderType getRecordTypeImpl() {
        return new ZohoInvoiceRecurringInvoiceDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If recurring_invoice_id is provided, fetch single recurring invoice (for DPAT_FIND_SINGLE)
        if (search_options.recurring_invoice_id) {
            hash<auto> response = doGet(sprintf("/recurringinvoices/%s", search_options.recurring_invoice_id));
            return new DefaultRecordIterator(new ListHashIterator((response.recurring_invoice,)), where_cond, search_options);
        }

        # Build query parameters for listing recurring invoices
        hash<auto> params;
        if (search_options.status) {
            params.status = search_options.status;
        }
        if (search_options.customer_id) {
            params.customer_id = search_options.customer_id;
        }
        if (search_options.recurrence_name) {
            params.recurrence_name = search_options.recurrence_name;
        }
        if (search_options.page) {
            params.page = search_options.page;
        }
        if (search_options.per_page) {
            params.per_page = search_options.per_page;
        }

        hash<auto> response = doGet("/recurringinvoices", params);
        return new DefaultRecordIterator(new ListHashIterator(response.recurring_invoices ?? ()), where_cond, search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ZohoInvoiceDataProvider
