# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceTagsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce tags collection data provider
public class WooCommerceTagsDataProvider inherits WooCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "tags",
            "desc": "WooCommerce product tags data provider; list and search tags",
            "type": "WooCommerceTagsDataProvider",
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
            "search": <DataProviderOptionInfo>{
                "display_name": "Search",
                "short_desc": "Search tags",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Limit results to those matching a search string",
            },
            "hide_empty": <DataProviderOptionInfo>{
                "display_name": "Hide Empty",
                "short_desc": "Hide empty tags",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "If `True`, hide tags with no products",
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
                    <AllowedValueInfo>{"value": "id", "desc": "Sort by ID"},
                    <AllowedValueInfo>{"value": "include", "desc": "Sort by include"},
                    <AllowedValueInfo>{"value": "name", "desc": "Sort by name"},
                    <AllowedValueInfo>{"value": "slug", "desc": "Sort by slug"},
                    <AllowedValueInfo>{"value": "term_group", "desc": "Sort by term group"},
                    <AllowedValueInfo>{"value": "description", "desc": "Sort by description"},
                    <AllowedValueInfo>{"value": "count", "desc": "Sort by count"},
                ),
            },
        };
    }

    private {
        #! Record type
        static WooCommerceTagDataType recordType();
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
        return "tags";
    }

    #! Returns the provider description
    *string getDesc() {
        return "WooCommerce product tags data provider; list and search tags";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for tags
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> params = {} + search_options;
        if (!params.per_page) {
            params.per_page = DefaultPageSize;
        }
        auto result = doGet("products/tags", params);
        return new DefaultRecordIterator(new ListHashIterator(result.typeCode() == NT_LIST ? result : ()),
            where_cond, search_options, recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace WooCommerceDataProvider
