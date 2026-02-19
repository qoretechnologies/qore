# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquarespaceProductsDataProvider.qc Squarespace products collection data provider

/*  SquarespaceProductsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquarespaceDataProvider module
public namespace SquarespaceDataProvider {

#! Squarespace products collection data provider
public class SquarespaceProductsDataProvider inherits SquarespaceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "products",
            "desc": "Squarespace products data provider for listing and searching products",
            "type": "SquarespaceProductsDataProvider",
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
                "short_desc": "Get specific product(s) by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Get specific product(s) by ID",
            },
            "type": <DataProviderOptionInfo>{
                "display_name": "Product Type",
                "short_desc": "Filter by product type (PHYSICAL, SERVICE, GIFT_CARD, DIGITAL)",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by product type (PHYSICAL, SERVICE, GIFT_CARD, DIGITAL)",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "PHYSICAL", "desc": "Physical product"},
                    <AllowedValueInfo>{"value": "SERVICE", "desc": "Service product"},
                    <AllowedValueInfo>{"value": "GIFT_CARD", "desc": "Gift card"},
                    <AllowedValueInfo>{"value": "DIGITAL", "desc": "Digital product"},
                ),
            },
            "modified_after": <DataProviderOptionInfo>{
                "display_name": "Modified After",
                "short_desc": "Products modified after this ISO 8601 datetime",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Products modified after this ISO 8601 datetime",
            },
            "modified_before": <DataProviderOptionInfo>{
                "display_name": "Modified Before",
                "short_desc": "Products modified before this ISO 8601 datetime",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Products modified before this ISO 8601 datetime",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };

        #! Child provider map
        const ChildMap = {
            "create": Class::forName("SquarespaceCreateProductDataProvider"),
            "update": Class::forName("SquarespaceUpdateProductDataProvider"),
            "delete": Class::forName("SquarespaceDeleteProductDataProvider"),
            "images": Class::forName("SquarespaceProductImagesDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(SquarespaceRestClient::SquarespaceRestClient rest) : SquarespaceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "products";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Squarespace products data provider for listing and searching products";
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
        return new SquarespaceProductDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If product_id provided, get single record
        if (search_options.product_id) {
            hash<auto> result = doGet("v2/commerce/products/" + search_options.product_id);
            # Squarespace wraps even single-product responses in {"products": [...]}
            list<hash<auto>> products = result.products ?? (result,);
            return new DefaultRecordIterator(new ListHashIterator(products),
                where_cond, search_options);
        }

        # Build query params
        hash<auto> params;
        if (search_options.type) {
            params.type = search_options.type;
        }
        if (search_options.modified_after) {
            params.modifiedAfter = search_options.modified_after;
        }
        if (search_options.modified_before) {
            params.modifiedBefore = search_options.modified_before;
        }

        list<hash<auto>> all_records = fetchAllPages("v2/commerce/products", "products", params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquarespaceDataProvider
