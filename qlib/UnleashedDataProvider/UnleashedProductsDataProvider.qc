# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnleashedDataProvider module definition

/*  UnleashedProductsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Products search data provider for Unleashed
public class UnleashedProductsDataProvider inherits UnleashedDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "products",
            "desc": "Search and list products from Unleashed",
            "type": "UnleashedProductsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options for products
        const SearchOptions = {
            "productCode": <DataProviderOptionInfo>{
                "display_name": "Product Code",
                "short_desc": "Filter by product code",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter products by code (partial match from start)",
                "preselected": True,
            },
            "productDescription": <DataProviderOptionInfo>{
                "display_name": "Product Description",
                "short_desc": "Filter by product description",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter products by description (partial match from start)",
                "preselected": True,
            },
            "productGroup": <DataProviderOptionInfo>{
                "display_name": "Product Group",
                "short_desc": "Filter by product group",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter by product group name",
            },
            "modifiedSince": <DataProviderOptionInfo>{
                "display_name": "Modified Since",
                "short_desc": "Filter by modification date",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Return products modified after this date/time",
            },
            "includeObsolete": <DataProviderOptionInfo>{
                "display_name": "Include Obsolete",
                "short_desc": "Include obsolete products",
                "type": AbstractDataProviderType::get(BoolOrNothingType),
                "desc": "Whether to include obsolete products (default: false)",
            },
            "excludeAssembled": <DataProviderOptionInfo>{
                "display_name": "Exclude Assembled",
                "short_desc": "Exclude assembled products",
                "type": AbstractDataProviderType::get(BoolOrNothingType),
                "desc": "Whether to exclude assembled products",
            },
            "excludeComponents": <DataProviderOptionInfo>{
                "display_name": "Exclude Components",
                "short_desc": "Exclude component products",
                "type": AbstractDataProviderType::get(BoolOrNothingType),
                "desc": "Whether to exclude component products",
            },
            "pageSize": <DataProviderOptionInfo>{
                "display_name": "Page Size",
                "short_desc": "Results per page",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Number of results per page (max 200, default 200)",
            },
        };
    }

    public {
        #! Record type
        static UnleashedProductDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(UnleashedRestClient::UnleashedRestClient rest) : UnleashedDataProviderBase(rest) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : UnleashedDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "products";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Search and list products from Unleashed";
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
        hash<auto> params;

        if (search_options.productCode) {
            params.productCode = search_options.productCode;
        }
        if (search_options.productDescription) {
            params.productDescription = search_options.productDescription;
        }
        if (search_options.productGroup) {
            params.productGroup = search_options.productGroup;
        }
        if (search_options.modifiedSince) {
            params.modifiedSince = formatDate(search_options.modifiedSince);
        }
        if (search_options.includeObsolete) {
            params.includeObsolete = search_options.includeObsolete ? "true" : "false";
        }
        if (search_options.excludeAssembled) {
            params.excludeAssembled = search_options.excludeAssembled ? "true" : "false";
        }
        if (search_options.excludeComponents) {
            params.excludeComponents = search_options.excludeComponents ? "true" : "false";
        }
        if (search_options.pageSize) {
            params.pageSize = search_options.pageSize;
        } else {
            params.pageSize = DefaultPageSize;
        }

        auto response = doGet("/Products", params);

        list<hash<auto>> records;
        if (response.typeCode() == NT_HASH && response.Items.typeCode() == NT_LIST) {
            records = response.Items;
        }

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond,
            search_options, recordType.getFields());
    }
}

} # namespace UnleashedDataProvider
