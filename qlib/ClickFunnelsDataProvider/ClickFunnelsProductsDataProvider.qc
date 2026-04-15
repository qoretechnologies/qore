# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClickFunnelsProductsDataProvider.qc ClickFunnels products collection and actions

/*  ClickFunnelsProductsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace ClickFunnelsDataProvider {

public class ClickFunnelsProductsDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "products",
            "desc": "ClickFunnels products data provider",
            "type": "ClickFunnelsProductsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = {
            "product_id": <DataProviderOptionInfo>{
                "display_name": "Product ID",
                "short_desc": "Get a specific product by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by product ID for single record lookup",
            },
            "after": <DataProviderOptionInfo>{
                "display_name": "After Cursor",
                "short_desc": "Pagination cursor",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The cursor value for fetching the next page of results",
            },
            "sort_order": <DataProviderOptionInfo>{
                "display_name": "Sort Order",
                "short_desc": "Sort order for results",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Sort order for results",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "asc", "display_name": "Ascending"},
                    <AllowedValueInfo>{"value": "desc", "display_name": "Descending"},
                ),
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };
        const ChildMap = {
            "create": Class::forName("ClickFunnelsCreateProductDataProvider"),
            "update": Class::forName("ClickFunnelsUpdateProductDataProvider"),
        };
    }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest) : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "products"; }
    *string getDesc() { return "ClickFunnels products data provider"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name}; if (cls) { return cls.newObject(rest); }
    }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) { return new ClickFunnelsProductDataType().getFields(); }
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        if (search_options.product_id) {
            auto result = doGet("products/" + search_options.product_id);
            return new DefaultRecordIterator(new ListHashIterator((result,)), where_cond, search_options);
        }
        hash<auto> params;
        if (search_options.sort_order) { params.sort_order = search_options.sort_order; }
        list<auto> records = fetchAllPages(getWorkspacePath("products"), params, search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Create Product ---
public class ClickFunnelsCreateProductRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": { "type": StringType, "display_name": "Name", "short_desc": "The product name", "desc": "The name of the product", "required": True, "example_value": "Online Course" },
            "current_path": { "type": StringOrNothingType, "display_name": "Current Path", "short_desc": "The product URL path", "desc": "The URL path for the product" },
            "visible_in_store": { "type": SoftBoolOrNothingType, "display_name": "Visible in Store", "short_desc": "Whether visible in store", "desc": "Whether the product is visible in the store" },
            "visible_in_customer_center": { "type": SoftBoolOrNothingType, "display_name": "Visible in Customer Center", "short_desc": "Whether visible in customer center", "desc": "Whether the product is visible in the customer center" },
            "seo_title": { "type": StringOrNothingType, "display_name": "SEO Title", "short_desc": "The SEO title", "desc": "The SEO title for the product" },
            "seo_description": { "type": StringOrNothingType, "display_name": "SEO Description", "short_desc": "The SEO description", "desc": "The SEO meta description" },
            "seo_image_url": { "type": StringOrNothingType, "display_name": "SEO Image URL", "short_desc": "The SEO image URL", "desc": "The SEO image URL for the product" },
            "commissionable": { "type": SoftBoolOrNothingType, "display_name": "Commissionable", "short_desc": "Whether commissionable", "desc": "Whether eligible for affiliate commissions" },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ClickFunnelsCreateProductDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{ "name": "create", "desc": "Create a new product in ClickFunnels", "type": "ClickFunnelsCreateProductDataProvider", "supports_request": True, "constructor_options": ConstructorOptions };
        const ConstructorOptions = BaseConstructorOptions;
        static *AbstractDataProviderType ResponseType = new ClickFunnelsProductDataType();
        static *AbstractDataProviderType RequestType = new ClickFunnelsCreateProductRequestDataType();
    }
    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest) : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "create"; }
    *string getDesc() { return "Create a new product in ClickFunnels"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return new ClickFunnelsCreateProductRequestDataType(); }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ClickFunnelsProductDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost(getWorkspacePath("products"), {"product": req});
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Update Product ---
public class ClickFunnelsUpdateProductRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "product_id": { "type": StringType, "display_name": "Product ID", "short_desc": "The product to update", "desc": "The unique identifier of the product to update", "required": True, "ref_data": "products" },
            "name": { "type": StringOrNothingType, "display_name": "Name", "short_desc": "The product name", "desc": "The new name for the product" },
            "current_path": { "type": StringOrNothingType, "display_name": "Current Path", "short_desc": "The product URL path", "desc": "The new URL path for the product" },
            "visible_in_store": { "type": SoftBoolOrNothingType, "display_name": "Visible in Store", "short_desc": "Whether visible in store", "desc": "Whether the product is visible in the store" },
            "visible_in_customer_center": { "type": SoftBoolOrNothingType, "display_name": "Visible in Customer Center", "short_desc": "Whether visible in customer center", "desc": "Whether the product is visible in the customer center" },
            "seo_title": { "type": StringOrNothingType, "display_name": "SEO Title", "short_desc": "The SEO title", "desc": "The new SEO title" },
            "seo_description": { "type": StringOrNothingType, "display_name": "SEO Description", "short_desc": "The SEO description", "desc": "The new SEO description" },
            "seo_image_url": { "type": StringOrNothingType, "display_name": "SEO Image URL", "short_desc": "The SEO image URL", "desc": "The new SEO image URL" },
            "commissionable": { "type": SoftBoolOrNothingType, "display_name": "Commissionable", "short_desc": "Whether commissionable", "desc": "Whether eligible for affiliate commissions" },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ClickFunnelsUpdateProductDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{ "name": "update", "desc": "Update a product in ClickFunnels", "type": "ClickFunnelsUpdateProductDataProvider", "supports_request": True, "constructor_options": ConstructorOptions };
        const ConstructorOptions = BaseConstructorOptions;
        static *AbstractDataProviderType ResponseType = new ClickFunnelsProductDataType();
        static *AbstractDataProviderType RequestType = new ClickFunnelsUpdateProductRequestDataType();
    }
    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest) : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "update"; }
    *string getDesc() { return "Update a product in ClickFunnels"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return new ClickFunnelsUpdateProductRequestDataType(); }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ClickFunnelsProductDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = remove req.product_id;
        return doPut("products/" + id, {"product": req});
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace ClickFunnelsDataProvider
