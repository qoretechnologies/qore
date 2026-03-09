# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquarespaceCreateProductDataProvider.qc Squarespace create product action data provider

/*  SquarespaceCreateProductDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Base request type for creating a product (uses PHYSICAL variant type as default)
public class SquarespaceCreateProductRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "type": StringType,
                "display_name": "Type",
                "short_desc": "The product type",
                "desc": "The product type: `PHYSICAL`, `SERVICE`, or `GIFT_CARD`. "
                    "Note: `DIGITAL` products cannot be created via the API.",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "PHYSICAL", "desc": "Physical product with shipping"},
                    <AllowedValueInfo>{"value": "SERVICE", "desc": "Service product"},
                    <AllowedValueInfo>{"value": "GIFT_CARD", "desc": "Gift card"},
                ),
            },
            "storePageId": {
                "type": StringType,
                "display_name": "Store Page ID",
                "short_desc": "The store page to add the product to",
                "desc": "The store page ID to add the product to",
                "required": True,
                "ref_data": "store_pages",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The product name",
                "desc": "The product name (max 200 characters)",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The product description (HTML)",
                "desc": "The product description in HTML format (max 102,400 characters)",
            },
            "urlSlug": {
                "type": StringOrNothingType,
                "display_name": "URL Slug",
                "short_desc": "The URL slug for the product",
                "desc": "The URL slug for the product (alphanumeric + hyphens, must be unique)",
            },
            "tags": {
                "type": SoftListOrNothingType,
                "display_name": "Tags",
                "short_desc": "List of tags for the product",
                "desc": "List of tags for the product (max 100 tags, each max 100 characters)",
            },
            "isVisible": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Visible",
                "short_desc": "Whether the product is visible on the store",
                "desc": "Whether the product is visible on the store; defaults to hidden if omitted",
            },
            "variantAttributes": {
                "type": SoftListOrNothingType,
                "display_name": "Variant Attributes",
                "short_desc": "List of variant attribute names",
                "desc": "List of variant attribute names (e.g., Size, Color); "
                    "PHYSICAL and SERVICE products only",
            },
            "variants": {
                "type": new ListDataType("PhysicalVariantList",
                    new SquarespacePhysicalVariantDataType(), True),
                "display_name": "Variants",
                "short_desc": "List of product variants",
                "desc": "List of product variants (1-100 items); fields depend on product type",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Squarespace create product action data provider
public class SquarespaceCreateProductDataProvider inherits SquarespaceDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new product in Squarespace",
            "type": "SquarespaceCreateProductDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(SquarespaceRestClient::SquarespaceRestClient rest) : SquarespaceDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Create a new product in Squarespace";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new SquarespaceCreateProductRequestDataType();
    }

    #! Returns a dynamic request type based on the product type
    /** When the user selects a product type, the variant fields are adjusted:
        - PHYSICAL: sku, pricing (with sale), stock, attributes, shippingMeasurements
        - SERVICE: sku, pricing (with sale), stock, attributes
        - GIFT_CARD: sku, pricing (basePrice only)
    */
    private *AbstractDataProviderType getRequestTypeWithDataImpl(auto req) {
        *string product_type;
        if (req.typeCode() == NT_HASH) {
            product_type = req.type;
        }
        if (!product_type) {
            return getRequestTypeImpl();
        }
        return buildRequestTypeForProductType(product_type);
    }

    #! Builds a request type with variant fields appropriate for the given product type
    private AbstractDataProviderType buildRequestTypeForProductType(string product_type) {
        # Get the base request type and clone its fields
        SquarespaceCreateProductRequestDataType base_type();
        hash<string, AbstractDataField> fields = base_type.getFields();

        # Replace the variants field with the type-specific variant list
        AbstractDataProviderType variant_type;
        switch (product_type) {
            case "GIFT_CARD":
                variant_type = new ListDataType("GiftCardVariantList",
                    new SquarespaceGiftCardVariantDataType(), True);
                break;
            case "SERVICE":
                variant_type = new ListDataType("ServiceVariantList",
                    new SquarespaceServiceVariantDataType(), True);
                break;
            default:
                # PHYSICAL is the default (most fields)
                variant_type = new ListDataType("PhysicalVariantList",
                    new SquarespacePhysicalVariantDataType(), True);
                break;
        }

        HashDataType rv("CreateProductRequest");
        foreach string key in (keys fields) {
            if (key == "variants") {
                rv.addField(new QoreDataField({
                    "name": "variants",
                    "display_name": "Variants",
                    "short_desc": "List of product variants",
                    "desc": "List of product variants (1-100 items)",
                    "type": variant_type,
                    "required": True,
                }));
            } else if (key == "variantAttributes") {
                # Only include variantAttributes for PHYSICAL and SERVICE
                if (product_type == "PHYSICAL" || product_type == "SERVICE") {
                    rv.addField(fields{key});
                }
            } else {
                rv.addField(fields{key});
            }
        }
        return rv;
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new SquarespaceProductDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("v2/commerce/products", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquarespaceDataProvider
