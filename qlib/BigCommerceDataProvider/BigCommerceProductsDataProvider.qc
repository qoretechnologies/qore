# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceProductsDataProvider.qc BigCommerce products collection data provider

/*  BigCommerceProductsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BigCommerceDataProvider module
public namespace BigCommerceDataProvider {

#! BigCommerce products collection data provider
public class BigCommerceProductsDataProvider inherits BigCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "products",
            "desc": "BigCommerce products data provider for listing and searching products",
            "type": "BigCommerceProductsDataProvider",
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
            "product_id": <DataProviderOptionInfo>{
                "display_name": "Product ID",
                "short_desc": "Filter by product ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by product ID (comma-separated for multiple); maps to "
                    "BigCommerce `id:in` query parameter",
            },
            "name": <DataProviderOptionInfo>{
                "display_name": "Name",
                "short_desc": "Filter by exact product name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by exact product name; maps to BigCommerce `name` query "
                    "parameter",
            },
            "name_like": <DataProviderOptionInfo>{
                "display_name": "Name (Like)",
                "short_desc": "Filter by partial name match",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by partial product name match; maps to BigCommerce `name:like` "
                    "query parameter",
            },
            "sku": <DataProviderOptionInfo>{
                "display_name": "SKU",
                "short_desc": "Filter by SKU",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by product SKU; maps to BigCommerce `sku` query parameter",
            },
            "keyword": <DataProviderOptionInfo>{
                "display_name": "Keyword",
                "short_desc": "Search by keyword",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Search products by keyword; maps to BigCommerce `keyword` query "
                    "parameter",
            },
            "brand_id": <DataProviderOptionInfo>{
                "display_name": "Brand ID",
                "short_desc": "Filter by brand ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by brand ID; maps to BigCommerce `brand_id` query parameter",
            },
            "condition": <DataProviderOptionInfo>{
                "display_name": "Condition",
                "short_desc": "Filter by product condition",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by product condition",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "New", "desc": "New products"},
                    <AllowedValueInfo>{"value": "Used", "desc": "Used products"},
                    <AllowedValueInfo>{"value": "Refurbished", "desc": "Refurbished products"},
                ),
            },
            "categories": <DataProviderOptionInfo>{
                "display_name": "Categories",
                "short_desc": "Filter by category IDs",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by category IDs (comma-separated for multiple); maps to "
                    "BigCommerce `categories:in` query parameter",
            },
            "is_visible": <DataProviderOptionInfo>{
                "display_name": "Is Visible",
                "short_desc": "Filter by visibility",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "Filter by product visibility on the storefront; maps to BigCommerce "
                    "`is_visible` query parameter",
            },
            "date_modified_min": <DataProviderOptionInfo>{
                "display_name": "Date Modified (Min)",
                "short_desc": "Filter by minimum modification date",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Filter products modified on or after this date; maps to BigCommerce "
                    "`date_modified:min` query parameter",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results to return",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
            "page": <DataProviderOptionInfo>{
                "display_name": "Page",
                "short_desc": "Page number for pagination",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Page number for paginated results",
            },
            "sort": <DataProviderOptionInfo>{
                "display_name": "Sort",
                "short_desc": "Sort field for results",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Field to sort results by",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "id", "desc": "Sort by product ID"},
                    <AllowedValueInfo>{"value": "name", "desc": "Sort by product name"},
                    <AllowedValueInfo>{"value": "sku", "desc": "Sort by SKU"},
                    <AllowedValueInfo>{"value": "price", "desc": "Sort by price"},
                    <AllowedValueInfo>{"value": "date_modified",
                        "desc": "Sort by modification date"},
                    <AllowedValueInfo>{"value": "date_last_imported",
                        "desc": "Sort by last import date"},
                    <AllowedValueInfo>{"value": "inventory_level",
                        "desc": "Sort by inventory level"},
                    <AllowedValueInfo>{"value": "is_visible",
                        "desc": "Sort by visibility"},
                    <AllowedValueInfo>{"value": "name:asc",
                        "desc": "Sort by name ascending"},
                    <AllowedValueInfo>{"value": "name:desc",
                        "desc": "Sort by name descending"},
                ),
            },
            "direction": <DataProviderOptionInfo>{
                "display_name": "Direction",
                "short_desc": "Sort direction",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Sort direction for results",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "asc", "desc": "Ascending order"},
                    <AllowedValueInfo>{"value": "desc", "desc": "Descending order"},
                ),
            },
        };

        #! Child provider map
        const ChildMap = {
            "create": Class::forName("BigCommerceCreateProductDataProvider"),
            "update": Class::forName("BigCommerceUpdateProductDataProvider"),
            "delete": Class::forName("BigCommerceDeleteProductDataProvider"),
            "get": Class::forName("BigCommerceGetProductDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(BigCommerceRestClient::BigCommerceRestClient rest)
            : BigCommerceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "products";
    }

    #! Returns the provider description
    *string getDesc() {
        return "BigCommerce products data provider for listing and searching products";
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
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new BigCommerceProductDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Build query params mapping user-friendly names to BigCommerce API params
        hash<auto> params;

        if (search_options.product_id) {
            params."id:in" = search_options.product_id;
        }
        if (search_options.name) {
            params.name = search_options.name;
        }
        if (search_options.name_like) {
            params."name:like" = search_options.name_like;
        }
        if (search_options.sku) {
            params.sku = search_options.sku;
        }
        if (search_options.keyword) {
            params.keyword = search_options.keyword;
        }
        if (search_options.brand_id) {
            params.brand_id = search_options.brand_id;
        }
        if (search_options.condition) {
            params.condition = search_options.condition;
        }
        if (search_options.categories) {
            params."categories:in" = search_options.categories;
        }
        if (exists search_options.is_visible) {
            params.is_visible = search_options.is_visible;
        }
        if (search_options.date_modified_min) {
            params."date_modified:min" = formatDateTimeV3(search_options.date_modified_min);
        }
        if (search_options.sort) {
            params.sort = search_options.sort;
        }
        if (search_options.direction) {
            params.direction = search_options.direction;
        }
        if (search_options.page) {
            params.page = search_options.page;
        }

        list<hash<auto>> all_records = fetchAllPagesV3("catalog/products", params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BigCommerceDataProvider
