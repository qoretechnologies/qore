# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnleashedDataProvider module definition

/*  UnleashedCustomersDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the UnleashedDataProvider module
public namespace UnleashedDataProvider {

#! Customers search data provider for Unleashed
public class UnleashedCustomersDataProvider inherits UnleashedDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "customers",
            "desc": "Search and list customers from Unleashed",
            "type": "UnleashedCustomersDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options for customers
        const SearchOptions = {
            "customerCode": <DataProviderOptionInfo>{
                "display_name": "Customer Code",
                "short_desc": "Filter by customer code",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter customers by their unique code",
            },
            "customerName": <DataProviderOptionInfo>{
                "display_name": "Customer Name",
                "short_desc": "Filter by customer name",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter customers by name (partial match from start)",
            },
            "contactEmail": <DataProviderOptionInfo>{
                "display_name": "Contact Email",
                "short_desc": "Filter by contact email",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter by contact email address",
            },
            "customerType": <DataProviderOptionInfo>{
                "display_name": "Customer Type",
                "short_desc": "Filter by customer type",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter by customer type name",
            },
            "modifiedSince": <DataProviderOptionInfo>{
                "display_name": "Modified Since",
                "short_desc": "Filter by modification date",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Return customers modified after this date/time",
            },
            "includeObsolete": <DataProviderOptionInfo>{
                "display_name": "Include Obsolete",
                "short_desc": "Include obsolete customers",
                "type": AbstractDataProviderType::get(BoolOrNothingType),
                "desc": "Whether to include obsolete customers (default: false)",
            },
            "pageSize": <DataProviderOptionInfo>{
                "display_name": "Page Size",
                "short_desc": "Results per page",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Number of results per page (max 200, default 200)",
            },
        };
    }

    private {
        #! Record type
        static UnleashedCustomerDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(UnleashedRestClient::UnleashedRestClient rest) : UnleashedDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(UnleashedRestClient::UnleashedRestConnection conn) : UnleashedDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : UnleashedDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "customers";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Search and list customers from Unleashed";
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the record type
    private *AbstractDataProviderType getRecordTypeImpl(*hash<auto> search_options) {
        return recordType;
    }

    #! Performs a search/query
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Build query parameters from search options
        hash<auto> params;

        if (search_options.customerCode) {
            params.customerCode = search_options.customerCode;
        }
        if (search_options.customerName) {
            params.customerName = search_options.customerName;
        }
        if (search_options.contactEmail) {
            params.contactEmail = search_options.contactEmail;
        }
        if (search_options.customerType) {
            params.customerType = search_options.customerType;
        }
        if (search_options.modifiedSince) {
            params.modifiedSince = formatDate(search_options.modifiedSince);
        }
        if (search_options.includeObsolete) {
            params.includeObsolete = search_options.includeObsolete ? "true" : "false";
        }
        if (search_options.pageSize) {
            params.pageSize = search_options.pageSize;
        } else {
            params.pageSize = DefaultPageSize;
        }

        # Call API
        auto response = doGet("/Customers", params);

        # Extract records from response
        list<hash<auto>> records;
        if (response.typeCode() == NT_HASH && response.Items.typeCode() == NT_LIST) {
            records = response.Items;
        }

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond,
            search_options, recordType.getFields());
    }
}

} # namespace UnleashedDataProvider
