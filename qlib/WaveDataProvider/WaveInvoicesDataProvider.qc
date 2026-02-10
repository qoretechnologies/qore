# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WaveDataProvider module definition

/*  WaveInvoicesDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the WaveDataProvider module
public namespace WaveDataProvider {

#! Wave invoices collection data provider
public class WaveInvoicesDataProvider inherits WaveDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "invoices",
            "desc": "Wave invoices data provider",
            "type": "WaveInvoicesDataProvider",
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
            "businessId": <DataProviderOptionInfo>{
                "display_name": "Business",
                "short_desc": "The business to list invoices for",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The ID of the business to list invoices for (required)",
            },
            "id": <DataProviderOptionInfo>{
                "display_name": "Invoice ID",
                "short_desc": "Get a specific invoice by ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "The ID of a specific invoice to retrieve",
            },
            "status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by invoice status",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter invoices by status",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Draft",
                        "short_desc": "Draft invoices",
                        "desc": "Invoices in draft status",
                        "value": "DRAFT",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Saved",
                        "short_desc": "Saved invoices",
                        "desc": "Invoices that have been saved",
                        "value": "SAVED",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Sent",
                        "short_desc": "Sent invoices",
                        "desc": "Invoices that have been sent",
                        "value": "SENT",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Overdue",
                        "short_desc": "Overdue invoices",
                        "desc": "Invoices that are past due",
                        "value": "OVERDUE",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Paid",
                        "short_desc": "Paid invoices",
                        "desc": "Invoices that have been fully paid",
                        "value": "PAID",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Partial",
                        "short_desc": "Partially paid invoices",
                        "desc": "Invoices that have been partially paid",
                        "value": "PARTIAL",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Unpaid",
                        "short_desc": "Unpaid invoices",
                        "desc": "Invoices that have not been paid",
                        "value": "UNPAID",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Viewed",
                        "short_desc": "Viewed invoices",
                        "desc": "Invoices that have been viewed by the customer",
                        "value": "VIEWED",
                    },
                ),
            },
            "invoiceNumber": <DataProviderOptionInfo>{
                "display_name": "Invoice Number",
                "short_desc": "Filter by invoice number",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter invoices by invoice number",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };
    }

    private {
        #! Record type
        static WaveInvoiceDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(WaveRestClient::WaveRestClient rest) : WaveDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(WaveRestClient::WaveRestConnection conn) : WaveDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "invoices";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Wave invoices collection data provider";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for invoices
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Get a single invoice by ID
        if (search_options.id) {
            *hash<auto> invoice;
            try {
                hash<auto> data = doGraphQL(GetInvoiceQuery, {
                    "businessId": search_options.businessId,
                    "invoiceId": search_options.id,
                });
                invoice = data.business.invoice;
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err == "GRAPHQL-ERROR") {
                    invoice = NOTHING;
                } else {
                    rethrow;
                }
            }
            if (!invoice) {
                return new DefaultRecordIterator(new ListHashIterator(()),
                    where_cond, search_options, recordType.getFields());
            }
            return new DefaultRecordIterator(new ListHashIterator((flattenEdges(invoice),)),
                where_cond, search_options, recordType.getFields());
        }

        # Filter by invoice number client-side (Wave has no server-side invoice number filter)
        if (search_options.invoiceNumber) {
            list<hash<auto>> results = ();
            hash<auto> vars = {"businessId": search_options.businessId, "page": 1, "pageSize": 100};
            while (True) {
                hash<auto> response = rest.graphql(ListInvoicesQuery, vars);
                if (response.body.errors) {
                    throw "GRAPHQL-ERROR", response.body.errors[0].message, response.body.errors;
                }
                *hash<auto> invoices = response.body.data.business.invoices;
                if (!invoices || !invoices.edges || !invoices.edges.size()) {
                    break;
                }
                foreach hash<auto> edge in (invoices.edges) {
                    hash<auto> invoice = flattenEdges(edge.node);
                    if (invoice.invoiceNumber == search_options.invoiceNumber) {
                        # Also apply status filter if set
                        if (!search_options.status || invoice.status == search_options.status) {
                            results += invoice;
                        }
                    }
                }
                if (!invoices.pageInfo || vars.page >= invoices.pageInfo.totalPages) {
                    break;
                }
                vars.page++;
            }
            return new DefaultRecordIterator(new ListHashIterator(results),
                where_cond, search_options, recordType.getFields());
        }

        # Filter by status client-side if specified
        if (search_options.status) {
            list<hash<auto>> results = ();
            hash<auto> vars = {"businessId": search_options.businessId, "page": 1, "pageSize": 100};
            while (True) {
                hash<auto> response = rest.graphql(ListInvoicesQuery, vars);
                if (response.body.errors) {
                    throw "GRAPHQL-ERROR", response.body.errors[0].message, response.body.errors;
                }
                *hash<auto> invoices = response.body.data.business.invoices;
                if (!invoices || !invoices.edges || !invoices.edges.size()) {
                    break;
                }
                foreach hash<auto> edge in (invoices.edges) {
                    hash<auto> invoice = flattenEdges(edge.node);
                    if (invoice.status == search_options.status) {
                        results += invoice;
                    }
                }
                if (!invoices.pageInfo || vars.page >= invoices.pageInfo.totalPages) {
                    break;
                }
                vars.page++;
            }
            return new DefaultRecordIterator(new ListHashIterator(results),
                where_cond, search_options, recordType.getFields());
        }

        # Default: paginated list of all invoices
        hash<auto> vars = {"businessId": search_options.businessId};
        int pageSize = search_options.limit ?? DefaultPageSize;

        return new WaveRecordIterator(rest, ListInvoicesQuery, "business", "invoices",
            vars, pageSize, recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace WaveDataProvider
