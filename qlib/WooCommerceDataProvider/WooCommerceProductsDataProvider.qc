# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceProductsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the WooCommerceDataProvider module
public namespace WooCommerceDataProvider {

#! WooCommerce products collection data provider
public class WooCommerceProductsDataProvider inherits WooCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "products",
            "desc": "WooCommerce products data provider; list and search products",
            "type": "WooCommerceProductsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
            "record_requires_search_options": True,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by product status",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter products by status",
                "allowed_values": ProductStatusAllowedValues,
            },
            "category": <DataProviderOptionInfo>{
                "display_name": "Category",
                "short_desc": "Filter by category ID",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter products by category ID",
                "ref_data": "categories",
            },
            "tag": <DataProviderOptionInfo>{
                "display_name": "Tag",
                "short_desc": "Filter by tag ID",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter products by tag ID",
                "ref_data": "tags",
            },
            "sku": <DataProviderOptionInfo>{
                "display_name": "SKU",
                "short_desc": "Filter by SKU",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter products by SKU",
            },
            "search": <DataProviderOptionInfo>{
                "display_name": "Search",
                "short_desc": "Search products",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Limit results to those matching a search string",
            },
            "min_price": <DataProviderOptionInfo>{
                "display_name": "Min Price",
                "short_desc": "Minimum price filter",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter products with a price greater than or equal to this value",
            },
            "max_price": <DataProviderOptionInfo>{
                "display_name": "Max Price",
                "short_desc": "Maximum price filter",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter products with a price less than or equal to this value",
            },
            "stock_status": <DataProviderOptionInfo>{
                "display_name": "Stock Status",
                "short_desc": "Filter by stock status",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter products by stock status",
                "allowed_values": StockStatusAllowedValues,
            },
            "type": <DataProviderOptionInfo>{
                "display_name": "Type",
                "short_desc": "Filter by product type",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter products by type",
                "allowed_values": ProductTypeAllowedValues,
            },
            "featured": <DataProviderOptionInfo>{
                "display_name": "Featured",
                "short_desc": "Filter by featured status",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "Filter featured products",
            },
            "per_page": <DataProviderOptionInfo>{
                "display_name": "Per Page",
                "short_desc": "Results per page",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Maximum number of items to return per page (max 100)",
            },
            "page": <DataProviderOptionInfo>{
                "display_name": "Page",
                "short_desc": "Page number",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Current page of the collection (starting at 1)",
            },
            "order": <DataProviderOptionInfo>{
                "display_name": "Order",
                "short_desc": "Sort order",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Sort ascending or descending",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "asc", "desc": "Ascending"},
                    <AllowedValueInfo>{"value": "desc", "desc": "Descending"},
                ),
            },
            "orderby": <DataProviderOptionInfo>{
                "display_name": "Order By",
                "short_desc": "Sort by field",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Sort collection by the given attribute",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "date", "desc": "Sort by date"},
                    <AllowedValueInfo>{"value": "id", "desc": "Sort by ID"},
                    <AllowedValueInfo>{"value": "title", "desc": "Sort by title"},
                    <AllowedValueInfo>{"value": "slug", "desc": "Sort by slug"},
                    <AllowedValueInfo>{"value": "price", "desc": "Sort by price"},
                    <AllowedValueInfo>{"value": "popularity", "desc": "Sort by popularity"},
                    <AllowedValueInfo>{"value": "rating", "desc": "Sort by rating"},
                ),
            },
            "after": <DataProviderOptionInfo>{
                "display_name": "After",
                "short_desc": "Filter by creation date (after)",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Limit response to products created after the given date",
            },
            "before": <DataProviderOptionInfo>{
                "display_name": "Before",
                "short_desc": "Filter by creation date (before)",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Limit response to products created before the given date",
            },
            "modified_after": <DataProviderOptionInfo>{
                "display_name": "Modified After",
                "short_desc": "Filter by modification date (after)",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Limit response to products modified after the given date",
            },
            "modified_before": <DataProviderOptionInfo>{
                "display_name": "Modified Before",
                "short_desc": "Filter by modification date (before)",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Limit response to products modified before the given date",
            },
            "dates_are_gmt": <DataProviderOptionInfo>{
                "display_name": "Dates Are GMT",
                "short_desc": "Interpret date filters as GMT",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "If `True`, the date filters are interpreted as GMT timestamps",
            },
            "on_sale": <DataProviderOptionInfo>{
                "display_name": "On Sale",
                "short_desc": "Filter on-sale products",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "If `True`, return only products currently on sale",
            },
            "slug": <DataProviderOptionInfo>{
                "display_name": "Slug",
                "short_desc": "Filter by product slug",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Limit results to a single product matching this slug",
            },
            "include": <DataProviderOptionInfo>{
                "display_name": "Include",
                "short_desc": "Include only specific product IDs",
                "type": new ListDataType("ProductIncludeIds", SoftIntType, True),
                "desc": "Limit results to the specified product IDs",
            },
            "exclude": <DataProviderOptionInfo>{
                "display_name": "Exclude",
                "short_desc": "Exclude specific product IDs",
                "type": new ListDataType("ProductExcludeIds", SoftIntType, True),
                "desc": "Exclude the specified product IDs from the result set",
            },
            "parent": <DataProviderOptionInfo>{
                "display_name": "Parent",
                "short_desc": "Include only products with these parent IDs",
                "type": new ListDataType("ProductParentIds", SoftIntType, True),
                "desc": "Limit results to products whose `parent_id` is in this list",
            },
            "parent_exclude": <DataProviderOptionInfo>{
                "display_name": "Parent Exclude",
                "short_desc": "Exclude products with these parent IDs",
                "type": new ListDataType("ProductParentExcludeIds", SoftIntType, True),
                "desc": "Exclude products whose `parent_id` is in this list",
            },
        };
    }

    private {
        #! Record type
        static WooCommerceProductDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(WooCommerceRestClient::WooCommerceRestClient rest)
            : WooCommerceDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(WooCommerceRestClient::WooCommerceRestConnection conn) : WooCommerceDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : WooCommerceDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "products";
    }

    #! Returns the provider description
    *string getDesc() {
        return "WooCommerce products data provider; list and search products";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for products
    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> params = {} + search_options;
        if (!params.per_page) {
            params.per_page = DefaultPageSize;
        }
        foreach string key in (("after", "before", "modified_after", "modified_before")) {
            if (params{key}) {
                params{key} = format_date("YYYY-MM-DDTHH:mm:SS", params{key});
            }
        }
        auto result = doGet("products", params);
        return new DefaultRecordIterator<hash<auto>>(new ListHashIterator(result.typeCode() == NT_LIST ? result : ()),
            where_cond, search_options, recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace WooCommerceDataProvider
