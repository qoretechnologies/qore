# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationProductsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShipStationDataProvider module
public namespace ShipStationDataProvider {

#! ShipStation products collection data provider
public class ShipStationProductsDataProvider inherits ShipStationDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "products",
            "desc": "ShipStation products data provider - list and search products",
            "type": "ShipStationProductsDataProvider",
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
            "sku": <DataProviderOptionInfo>{
                "display_name": "SKU",
                "short_desc": "Filter by SKU",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter products by SKU (starts with search)",
            },
            "name": <DataProviderOptionInfo>{
                "display_name": "Name",
                "short_desc": "Filter by name",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter products by name (partial match)",
            },
            "productCategoryId": <DataProviderOptionInfo>{
                "display_name": "Category ID",
                "short_desc": "Filter by category",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter products by category ID",
            },
            "productTypeId": <DataProviderOptionInfo>{
                "display_name": "Product Type ID",
                "short_desc": "Filter by product type",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter products by product type ID",
            },
            "tagId": <DataProviderOptionInfo>{
                "display_name": "Tag",
                "short_desc": "Filter by tag",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter products by tag ID",
                "ref_data": "tags",
            },
            "includeInactive": <DataProviderOptionInfo>{
                "display_name": "Include Inactive",
                "short_desc": "Include inactive products",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "Include inactive products in results",
            },
            "sortBy": <DataProviderOptionInfo>{
                "display_name": "Sort By",
                "short_desc": "Field to sort by",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Sort results by this field",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "SKU", "desc": "Sort by SKU"},
                    <AllowedValueInfo>{"value": "ModifyDate", "desc": "Sort by modify date"},
                    <AllowedValueInfo>{"value": "CreateDate", "desc": "Sort by create date"},
                ),
            },
            "sortDir": <DataProviderOptionInfo>{
                "display_name": "Sort Direction",
                "short_desc": "Sort direction",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Sort direction (ASC or DESC)",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "ASC", "desc": "Ascending"},
                    <AllowedValueInfo>{"value": "DESC", "desc": "Descending"},
                ),
            },
            "page": <DataProviderOptionInfo>{
                "display_name": "Page",
                "short_desc": "Page number",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Page number for pagination (starting at 1)",
            },
            "pageSize": <DataProviderOptionInfo>{
                "display_name": "Page Size",
                "short_desc": "Results per page",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Number of results per page (max 500)",
            },
        };
    }

    private {
        #! Record type
        static ShipStationProductDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(ShipStationRestClient::ShipStationRestClient rest)
            : ShipStationDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ShipStationRestClient::ShipStationRestConnection conn) : ShipStationDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "products";
    }

    #! Returns the provider description
    *string getDesc() {
        return "ShipStation products data provider - list and search products";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for products
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> params;

        if (search_options.sku) {
            params.sku = search_options.sku;
        }
        if (search_options.name) {
            params.name = search_options.name;
        }
        if (search_options.productCategoryId) {
            params.productCategoryId = search_options.productCategoryId;
        }
        if (search_options.productTypeId) {
            params.productTypeId = search_options.productTypeId;
        }
        if (search_options.tagId) {
            params.tagId = search_options.tagId;
        }
        if (search_options.includeInactive) {
            params.includeInactive = search_options.includeInactive ? "true" : "false";
        }
        if (search_options.sortBy) {
            params.sortBy = search_options.sortBy;
        }
        if (search_options.sortDir) {
            params.sortDir = search_options.sortDir;
        }
        if (search_options.page) {
            params.page = search_options.page;
        }
        if (search_options.pageSize) {
            params.pageSize = search_options.pageSize;
        } else {
            params.pageSize = DefaultPageSize;
        }

        @debug(log(LoggerLevel::DEBUG, "searchRecordsImpl: params=%y", params));

        hash<auto> response = doGet("/products", params);
        list<hash<auto>> records = response.products ?? ();

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ShipStationDataProvider
