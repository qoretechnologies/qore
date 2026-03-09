# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInventoryDataProvider module definition

/*  ZohoInventoryInvoicesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ZohoInventoryDataProvider module
public namespace ZohoInventoryDataProvider {

#! Zoho Inventory invoices data provider
public class ZohoInventoryInvoicesDataProvider inherits ZohoInventoryDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "invoices",
            "desc": "Zoho Inventory invoices",
            "type": "ZohoInventoryInvoicesDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "constructor_options": BaseConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Search options
        const SearchOptions = {
            "organization_id": <DataProviderOptionInfo>{
                "display_name": "Organization ID",
                "short_desc": "Zoho Inventory organization ID",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The organization ID",
                "required": True,
            },
            "invoice_id": <DataProviderOptionInfo>{
                "display_name": "Invoice ID",
                "short_desc": "Filter by invoice ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The invoice ID for retrieving a single invoice",
            },
            "customer_id": <DataProviderOptionInfo>{
                "display_name": "Customer ID",
                "short_desc": "Filter by customer ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter invoices by customer ID",
            },
            "status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by status",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter invoices by status",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "draft",
                        "desc": "Draft invoices",
                    },
                    <AllowedValueInfo>{
                        "value": "sent",
                        "desc": "Sent invoices",
                    },
                    <AllowedValueInfo>{
                        "value": "overdue",
                        "desc": "Overdue invoices",
                    },
                    <AllowedValueInfo>{
                        "value": "paid",
                        "desc": "Paid invoices",
                    },
                    <AllowedValueInfo>{
                        "value": "void",
                        "desc": "Void invoices",
                    },
                    <AllowedValueInfo>{
                        "value": "unpaid",
                        "desc": "Unpaid invoices",
                    },
                    <AllowedValueInfo>{
                        "value": "partially_paid",
                        "desc": "Partially paid invoices",
                    },
                ),
            },
        };
    }

    #! Creates the object with a REST client
    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest) : ZohoInventoryDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "invoices";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new ZohoInventoryInvoiceDataType().getFields();
    }

    #! Searches for invoices
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        # Support DPAT_FIND_SINGLE: get a single invoice by ID
        if (search_options.invoice_id) {
            hash<auto> params;
            if (search_options.organization_id) {
                params.organization_id = search_options.organization_id;
            }
            hash<auto> response = doGet(sprintf("/invoices/%s", search_options.invoice_id), params);
            list<auto> records = response.invoice ? (response.invoice,) : ();
            return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options);
        }

        hash<auto> params;
        if (search_options.organization_id) {
            params.organization_id = search_options.organization_id;
        }
        if (search_options.customer_id) {
            params.customer_id = search_options.customer_id;
        }
        if (search_options.status) {
            params.status = search_options.status;
        }
        hash<auto> response = doGet("/invoices", params);
        return new DefaultRecordIterator(new ListHashIterator(response.invoices ?? ()), where_cond, search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ZohoInventoryDataProvider
