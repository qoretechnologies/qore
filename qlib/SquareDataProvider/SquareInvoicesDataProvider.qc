# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareInvoicesDataProvider.qc Square invoices collection data provider

/*  SquareInvoicesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquareDataProvider module
public namespace SquareDataProvider {

#! Square invoices collection data provider
public class SquareInvoicesDataProvider inherits SquareDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "invoices",
            "desc": "Square invoices data provider for listing invoices",
            "type": "SquareInvoicesDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
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
            "location_id": <DataProviderOptionInfo>{
                "display_name": "Location ID",
                "short_desc": "Filter by location (required)",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter invoices by location ID (required by Square API, "
                    "auto-detected if not provided)",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };

        #! Child provider map
        const ChildMap = {
            "create": Class::forName("SquareCreateInvoiceDataProvider"),
            "send": Class::forName("SquareSendInvoiceDataProvider"),
            "cancel": Class::forName("SquareCancelInvoiceDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(SquareRestClient::SquareRestClient rest) : SquareDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "invoices";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Square invoices data provider for listing invoices";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    #! Returns the record type
    private *AbstractDataProviderType getRecordTypeImpl() {
        return new SquareInvoiceDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If invoice_id provided, get single record
        if (search_options.invoice_id) {
            hash<auto> result = doGet("invoices/" + search_options.invoice_id);
            return new DefaultRecordIterator(new ListHashIterator((result.invoice,)),
                where_cond, search_options);
        }

        # Get location_id (required by Square API)
        string location_id;
        if (search_options.location_id) {
            location_id = search_options.location_id;
        } else {
            location_id = getFirstLocationId();
        }

        hash<auto> params = {"location_id": location_id};

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

} # namespace SquareDataProvider
