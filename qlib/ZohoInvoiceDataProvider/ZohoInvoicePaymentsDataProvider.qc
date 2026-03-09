# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInvoiceDataProvider module definition

/*  ZohoInvoicePaymentsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Zoho Invoice payments data provider - lists and searches customer payments
public class ZohoInvoicePaymentsDataProvider inherits ZohoInvoiceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "payments",
            "desc": "Zoho Invoice payments data provider",
            "type": "ZohoInvoicePaymentsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
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
            "payment_id": <DataProviderOptionInfo>{
                "display_name": "Payment ID",
                "short_desc": "Get specific payment",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Get a specific payment by ID (for DPAT_FIND_SINGLE)",
            },
            "customer_id": <DataProviderOptionInfo>{
                "display_name": "Customer ID",
                "short_desc": "Filter by customer",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter payments by customer ID",
            },
            "date_start": <DataProviderOptionInfo>{
                "display_name": "Date Start",
                "short_desc": "Filter by start date",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter payments on or after this date (`YYYY-MM-DD`)",
            },
            "date_end": <DataProviderOptionInfo>{
                "display_name": "Date End",
                "short_desc": "Filter by end date",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter payments on or before this date (`YYYY-MM-DD`)",
            },
            "payment_mode": <DataProviderOptionInfo>{
                "display_name": "Payment Mode",
                "short_desc": "Filter by payment method",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter payments by payment mode (cash, check, credit_card, etc.)",
            },
            "reference_number": <DataProviderOptionInfo>{
                "display_name": "Reference Number",
                "short_desc": "Search by reference",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Search payments by reference number",
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
        return "payments";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Zoho Invoice payments data provider for listing and searching customer payments";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new ZohoInvoicePaymentDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If payment_id is provided, fetch single payment (for DPAT_FIND_SINGLE)
        if (search_options.payment_id) {
            hash<auto> response = doGet(sprintf("/customerpayments/%s", search_options.payment_id));
            return new DefaultRecordIterator(new ListHashIterator((response.customerpayment ?? response.payment,)), where_cond, search_options);
        }

        # Build query parameters for listing payments
        hash<auto> params;
        if (search_options.customer_id) {
            params.customer_id = search_options.customer_id;
        }
        if (search_options.date_start) {
            params.date_start = search_options.date_start;
        }
        if (search_options.date_end) {
            params.date_end = search_options.date_end;
        }
        if (search_options.payment_mode) {
            params.payment_mode = search_options.payment_mode;
        }
        if (search_options.reference_number) {
            params.reference_number = search_options.reference_number;
        }
        if (search_options.page) {
            params.page = search_options.page;
        }
        if (search_options.per_page) {
            params.per_page = search_options.per_page;
        }

        hash<auto> response = doGet("/customerpayments", params);
        return new DefaultRecordIterator(new ListHashIterator(response.customerpayments ?? response.payments ?? ()), where_cond, search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ZohoInvoiceDataProvider
