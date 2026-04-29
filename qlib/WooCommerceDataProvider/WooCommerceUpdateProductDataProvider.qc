# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceUpdateProductDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a product
public class WooCommerceUpdateProductRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Product ID",
                "type": IntType,
                "desc": "The unique identifier for the product to update",
                "required": True,
                "example_value": 456,
                "ref_data": "products",
            },
            "name": {
                "display_name": "Name",
                "type": StringOrNothingType,
                "desc": "The product name",
                "example_value": "Premium T-Shirt",
            },
            "slug": {
                "display_name": "Slug",
                "type": StringOrNothingType,
                "desc": "The product URL slug",
                "example_value": "premium-t-shirt",
            },
            "type": {
                "display_name": "Type",
                "type": StringOrNothingType,
                "desc": "Product type. Different types expose different fields:\n\n"
                    "- `simple`: standard product (the default)\n"
                    "- `variable`: parent of variations — uses `attributes` (with `variation: true`) and "
                        "`default_attributes`\n"
                    "- `external`: links to an external product page — requires `external_url`/`button_text`\n"
                    "- `grouped`: groups other products together — uses `grouped_products`",
                "allowed_values": ProductTypeAllowedValues,
            },
            "status": {
                "display_name": "Status",
                "type": StringOrNothingType,
                "desc": "Product status",
                "allowed_values": ProductStatusAllowedValues,
            },
            "catalog_visibility": {
                "display_name": "Catalog Visibility",
                "type": StringOrNothingType,
                "desc": "Catalog visibility: `visible`, `catalog`, `search`, `hidden`",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "visible", "desc": "Visible everywhere"},
                    <AllowedValueInfo>{"value": "catalog", "desc": "Catalog only"},
                    <AllowedValueInfo>{"value": "search", "desc": "Search only"},
                    <AllowedValueInfo>{"value": "hidden", "desc": "Hidden"},
                ),
            },
            "featured": {
                "display_name": "Featured",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the product is featured",
                "example_value": False,
            },
            "regular_price": {
                "display_name": "Regular Price",
                "type": StringOrNothingType,
                "desc": "The regular price of the product",
                "example_value": "29.99",
            },
            "sale_price": {
                "display_name": "Sale Price",
                "type": StringOrNothingType,
                "desc": "The product sale price",
                "example_value": "19.99",
            },
            "date_on_sale_from": {
                "display_name": "Date On Sale From",
                "type": DateOrNothingType,
                "desc": "Start date of the sale price",
            },
            "date_on_sale_to": {
                "display_name": "Date On Sale To",
                "type": DateOrNothingType,
                "desc": "End date of the sale price",
            },
            "description": {
                "display_name": "Description",
                "type": StringOrNothingType,
                "desc": "The full product description",
                "example_value": "A premium quality cotton t-shirt available in multiple sizes.",
            },
            "short_description": {
                "display_name": "Short Description",
                "type": StringOrNothingType,
                "desc": "The short product description",
                "example_value": "Premium cotton t-shirt",
            },
            "sku": {
                "display_name": "SKU",
                "type": StringOrNothingType,
                "desc": "The unique stock keeping unit identifier",
                "example_value": "TSHIRT-PREM-001",
            },
            "categories": {
                "display_name": "Categories",
                "type": new ListDataType("Categories", new WooCommerceCategoryDataType(), True),
                "desc": "List of category objects",
            },
            "tags": {
                "display_name": "Tags",
                "type": new ListDataType("Tags", new WooCommerceTagDataType(), True),
                "desc": "List of tag objects",
            },
            "images": {
                "display_name": "Images",
                "type": new ListDataType("Images", new WooCommerceImageDataType(), True),
                "desc": "List of image objects",
            },
            "manage_stock": {
                "display_name": "Manage Stock",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, stock management is enabled",
                "example_value": True,
            },
            "stock_quantity": {
                "display_name": "Stock Quantity",
                "type": IntOrNothingType,
                "desc": "The number of items in stock",
                "example_value": 100,
            },
            "stock_status": {
                "display_name": "Stock Status",
                "type": StringOrNothingType,
                "desc": "Stock status: `instock`, `outofstock`, `onbackorder`",
                "allowed_values": StockStatusAllowedValues,
            },
            "backorders": {
                "display_name": "Backorders",
                "type": StringOrNothingType,
                "desc": "Backorder behavior: `no`, `notify`, `yes`",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "no", "desc": "Do not allow backorders"},
                    <AllowedValueInfo>{"value": "notify", "desc": "Allow but notify customer"},
                    <AllowedValueInfo>{"value": "yes", "desc": "Allow backorders"},
                ),
            },
            "sold_individually": {
                "display_name": "Sold Individually",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, customers can only buy one of this item per order",
                "example_value": False,
            },
            "low_stock_amount": {
                "display_name": "Low Stock Amount",
                "type": IntOrNothingType,
                "desc": "The threshold below which the product is considered low stock",
            },
            "purchase_note": {
                "display_name": "Purchase Note",
                "type": StringOrNothingType,
                "desc": "Note shown to the customer after purchase",
            },
            "weight": {
                "display_name": "Weight",
                "type": StringOrNothingType,
                "desc": "The product weight",
                "example_value": "0.5",
            },
            "dimensions": {
                "display_name": "Dimensions",
                "type": new WooCommerceDimensionsDataType(AutoHashOrNothingType),
                "desc": "Product dimensions: `length`, `width`, `height`",
            },
            "virtual": {
                "display_name": "Virtual",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the product is virtual",
                "example_value": False,
            },
            "downloadable": {
                "display_name": "Downloadable",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the product is downloadable",
                "example_value": False,
            },
            "tax_status": {
                "display_name": "Tax Status",
                "type": StringOrNothingType,
                "desc": "Tax status: `taxable`, `shipping`, `none`",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "taxable", "desc": "Taxable"},
                    <AllowedValueInfo>{"value": "shipping", "desc": "Shipping only"},
                    <AllowedValueInfo>{"value": "none", "desc": "None"},
                ),
            },
            "tax_class": {
                "display_name": "Tax Class",
                "type": StringOrNothingType,
                "desc": "Tax class slug (e.g. `standard`, `reduced-rate`, `zero-rate`)",
            },
            "shipping_class": {
                "display_name": "Shipping Class",
                "type": StringOrNothingType,
                "desc": "Shipping class slug",
            },
            "reviews_allowed": {
                "display_name": "Reviews Allowed",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, customer reviews are allowed",
                "example_value": True,
            },
            "menu_order": {
                "display_name": "Menu Order",
                "type": IntOrNothingType,
                "desc": "Menu sort order for the product",
                "example_value": 0,
            },
            "upsell_ids": {
                "display_name": "Upsell Product IDs",
                "type": new ListDataType("UpsellIdList", SoftIntType, True),
                "desc": "List of upsell product IDs",
                "ref_data": "products",
            },
            "cross_sell_ids": {
                "display_name": "Cross-sell Product IDs",
                "type": new ListDataType("CrossSellIdList", SoftIntType, True),
                "desc": "List of cross-sell product IDs",
                "ref_data": "products",
            },
            "parent_id": {
                "display_name": "Parent Product ID",
                "type": IntOrNothingType,
                "desc": "Parent product ID (for grouped/variation products)",
                "ref_data": "products",
            },
            "external_url": {
                "display_name": "External URL",
                "type": StringOrNothingType,
                "desc": "External product URL — only used when `type` is `external`",
                "example_value": "https://example.com/product",
            },
            "button_text": {
                "display_name": "Button Text",
                "type": StringOrNothingType,
                "desc": "External product button text — only used when `type` is `external`",
                "example_value": "Buy on Example.com",
            },
            "grouped_products": {
                "display_name": "Grouped Products",
                "type": new ListDataType("GroupedProductIdList", SoftIntType, True),
                "desc": "List of grouped product IDs — only used when `type` is `grouped`",
                "ref_data": "products",
            },
            "attributes": {
                "display_name": "Attributes",
                "type": new ListDataType("ProductAttributes", new WooCommerceProductAttributeDataType(), True),
                "desc": "List of product attributes. For `variable` products, set `variation` to `True` on each "
                    "attribute used to build variations.",
            },
            "default_attributes": {
                "display_name": "Default Attributes",
                "type": new ListDataType("ProductDefaultAttributes",
                    new WooCommerceProductDefaultAttributeDataType(), True),
                "desc": "Default selected attribute options — only used when `type` is `variable`",
            },
            "downloads": {
                "display_name": "Downloads",
                "type": new ListDataType("ProductDownloads", new WooCommerceProductDownloadDataType(), True),
                "desc": "List of downloadable files — only used when `downloadable` is `True`",
            },
            "download_limit": {
                "display_name": "Download Limit",
                "type": IntOrNothingType,
                "desc": "Maximum number of downloads per purchase (`-1` for unlimited) — only used when "
                    "`downloadable` is `True`",
                "example_value": -1,
            },
            "download_expiry": {
                "display_name": "Download Expiry",
                "type": IntOrNothingType,
                "desc": "Number of days before download access expires (`-1` for never) — only used when "
                    "`downloadable` is `True`",
                "example_value": -1,
            },
            "meta_data": {
                "display_name": "Meta Data",
                "type": new ListDataType("MetaData", new WooCommerceMetaDataType(), True),
                "desc": "List of metadata key-value pairs",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for updating a WooCommerce product
public class WooCommerceUpdateProductDataProvider inherits WooCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Updates an existing WooCommerce product",
            "type": "WooCommerceUpdateProductDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static WooCommerceUpdateProductRequestDataType RequestType();

        #! Response type
        static WooCommerceProductDataType ResponseType();

        #! Field names that only apply to specific product types or to downloadable products
        const TypeSpecificFields = WooCommerceCreateProductDataProvider::TypeSpecificFields;
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
        return "update";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Updates an existing WooCommerce product";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int id = remove req.id;
        hash<auto> body = {} + req;
        if (body.date_on_sale_from) {
            body.date_on_sale_from = format_date("YYYY-MM-DDTHH:mm:SS", body.date_on_sale_from);
        }
        if (body.date_on_sale_to) {
            body.date_on_sale_to = format_date("YYYY-MM-DDTHH:mm:SS", body.date_on_sale_to);
        }
        return doPut(sprintf("products/%d", id), body);
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns a request type filtered for the selected product type
    private *AbstractDataProviderType getRequestTypeWithOptionsImpl(*hash<auto> options) {
        return WooCommerceUpdateProductDataProvider::buildRequestTypeForProductType(options.type);
    }

    #! Returns a request type filtered for the product type in the request data
    private *AbstractDataProviderType getRequestTypeWithDataImpl(auto req) {
        *string product_type;
        bool downloadable = False;
        if (req.typeCode() == NT_HASH) {
            product_type = req.type;
            downloadable = parse_boolean(req.downloadable);
        }
        return WooCommerceUpdateProductDataProvider::buildRequestTypeForProductType(product_type, downloadable);
    }

    #! Builds a request type with only the fields that apply to the given product type
    static AbstractDataProviderType buildRequestTypeForProductType(*string product_type, *bool downloadable) {
        hash<string, AbstractDataField> fields = RequestType.getFields();
        hash<string, bool> include_field = WooCommerceCreateProductDataProvider::getRelevantFields(
            product_type, downloadable);
        HashDataType rv("UpdateProductRequest");
        foreach string key in (keys fields) {
            if (include_field{key} ?? True) {
                rv.addField(fields{key});
            }
        }
        return rv;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace WooCommerceDataProvider
