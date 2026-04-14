# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyProductDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/
public namespace InsightlyDataProvider {

public class InsightlyProductDataType inherits HashDataType {
    public {
        const Fields = {
            "PRODUCT_ID": {
                "type": SoftIntOrNothingType, "display_name": "Product ID",
                "short_desc": "The unique product identifier",
                "desc": "The unique identifier of the product",
            },
            "PRODUCT_NAME": {
                "type": SoftStringOrNothingType, "display_name": "Product Name",
                "short_desc": "The product name", "desc": "The name of the product",
            },
            "PRODUCT_CODE": {
                "type": SoftStringOrNothingType, "display_name": "Product Code",
                "short_desc": "Product code", "desc": "The product code or SKU identifier",
            },
            "PRODUCT_SKU": {
                "type": SoftStringOrNothingType, "display_name": "Product SKU",
                "short_desc": "Stock keeping unit",
                "desc": "The stock keeping unit identifier",
            },
            "DESCRIPTION": {
                "type": SoftStringOrNothingType, "display_name": "Description",
                "short_desc": "Product description",
                "desc": "A description of the product",
            },
            "PRODUCT_FAMILY": {
                "type": SoftStringOrNothingType, "display_name": "Product Family",
                "short_desc": "Product family name",
                "desc": "The product family this product belongs to",
            },
            "PRODUCT_IMAGE_URL": {
                "type": SoftStringOrNothingType, "display_name": "Product Image URL",
                "short_desc": "Product image URL",
                "desc": "The URL of the product image",
            },
            "CURRENCY": {
                "type": SoftStringOrNothingType, "display_name": "Currency",
                "short_desc": "Default currency code",
                "desc": "The ISO 4217 currency code for the product",
            },
            "DEFAULT_PRICE": {
                "type": SoftFloatOrNothingType, "display_name": "Default Price",
                "short_desc": "The default price",
                "desc": "The default price of the product",
            },
            "PRODUCT_PRICEBOOKS": {
                "type": new ListDataType("InsightlyProdRespPricebookList",
                    new InsightlyProductPricebookDataType(), True),
                "display_name": "Product Pricebooks",
                "short_desc": "Pricebook entries",
                "desc": "A list of pricebook entries for the product",
            },
            "CUSTOMFIELDS": {
                "type": new ListDataType("InsightlyProdRespCustomFieldList",
                    new InsightlyCustomFieldDataType(), True),
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "A list of custom field values for the product",
            },
            "DATE_CREATED_UTC": {
                "type": SoftDateOrNothingType, "display_name": "Date Created",
                "short_desc": "Creation timestamp (UTC)",
                "desc": "The UTC timestamp when the product was created",
            },
            "DATE_UPDATED_UTC": {
                "type": SoftDateOrNothingType, "display_name": "Date Updated",
                "short_desc": "Last update timestamp (UTC)",
                "desc": "The UTC timestamp when the product was last updated",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class InsightlyCreateProductRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "PRODUCT_NAME": {
                "type": SoftStringType, "display_name": "Product Name",
                "required": True, "short_desc": "The product name",
                "desc": "The name of the product (required)",
                "example_value": "Enterprise License",
            },
            "PRODUCT_CODE": {
                "type": SoftStringOrNothingType, "display_name": "Product Code",
                "short_desc": "Product code",
                "desc": "The product code or SKU identifier",
                "preselected": True,
            },
            "PRODUCT_SKU": {
                "type": SoftStringOrNothingType, "display_name": "Product SKU",
                "short_desc": "Stock keeping unit",
                "desc": "The stock keeping unit identifier",
            },
            "DESCRIPTION": {
                "type": SoftStringOrNothingType, "display_name": "Description",
                "short_desc": "Product description",
                "desc": "A description of the product",
                "preselected": True,
            },
            "PRODUCT_FAMILY": {
                "type": SoftStringOrNothingType, "display_name": "Product Family",
                "short_desc": "Product family name",
                "desc": "The product family this product belongs to",
            },
            "CURRENCY": {
                "type": SoftStringOrNothingType, "display_name": "Currency",
                "short_desc": "Default currency code",
                "desc": "The ISO 4217 currency code (e.g., `USD`)",
            },
            "DEFAULT_PRICE": {
                "type": SoftFloatOrNothingType, "display_name": "Default Price",
                "short_desc": "The default price",
                "desc": "The default price of the product",
                "preselected": True,
            },
            "PRODUCT_PRICEBOOKS": {
                "type": new ListDataType("InsightlyProductPricebookList",
                    new InsightlyProductPricebookDataType(), True),
                "display_name": "Product Pricebooks",
                "short_desc": "Pricebook entries",
                "desc": "A list of pricebook entries for the product",
            },
            "CUSTOMFIELDS": {
                "type": new ListDataType("InsightlyProdCustomFieldList",
                    new InsightlyCustomFieldDataType(), True),
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "A list of custom field values",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class InsightlyUpdateProductRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "PRODUCT_ID": {
                "type": SoftIntType, "display_name": "Product ID",
                "required": True, "short_desc": "The product ID to update",
                "desc": "The unique identifier of the product to update",
                "ref_data": "products",
            },
        } + InsightlyDataProviderBase::makeFieldsOptional(
            InsightlyCreateProductRequestDataType::Fields);
    }
    constructor() { addQoreFields(Fields); }
}

public class InsightlyDeleteProductRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "PRODUCT_ID": {
                "type": SoftIntType, "display_name": "Product ID",
                "required": True, "short_desc": "The product ID to delete",
                "desc": "The unique identifier of the product to delete",
                "ref_data": "products",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

} # namespace InsightlyDataProvider
