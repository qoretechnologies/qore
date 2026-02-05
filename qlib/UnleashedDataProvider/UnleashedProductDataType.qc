# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore UnleashedDataProvider module definition

/*  UnleashedProductDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Product data type for Unleashed
public class UnleashedProductDataType inherits HashDataType {
    private {
        const Fields = {
            "Guid": {
                "type": SoftStringOrNothingType,
                "display_name": "Product GUID",
                "short_desc": "Unique product identifier",
                "desc": "The unique GUID identifier of the product",
            },
            "ProductCode": {
                "type": SoftStringOrNothingType,
                "display_name": "Product Code",
                "short_desc": "Product code",
                "desc": "The unique product code (required for create, cannot be modified after creation)",
            },
            "ProductDescription": {
                "type": SoftStringOrNothingType,
                "display_name": "Product Description",
                "short_desc": "Product name/description",
                "desc": "The product's description/name",
            },
            "Barcode": {
                "type": SoftStringOrNothingType,
                "display_name": "Barcode",
                "short_desc": "Product barcode",
                "desc": "The product's barcode",
            },
            "DefaultPurchasePrice": {
                "type": SoftFloatOrNothingType,
                "display_name": "Default Purchase Price",
                "short_desc": "Default purchase price",
                "desc": "The default purchase price for this product",
            },
            "DefaultSellPrice": {
                "type": SoftFloatOrNothingType,
                "display_name": "Default Sell Price",
                "short_desc": "Default sell price",
                "desc": "The default sell price for this product",
            },
            "MinimumSellPrice": {
                "type": SoftFloatOrNothingType,
                "display_name": "Minimum Sell Price",
                "short_desc": "Minimum sell price",
                "desc": "The minimum sell price for this product",
            },
            "MinStockAlertLevel": {
                "type": SoftFloatOrNothingType,
                "display_name": "Min Stock Alert Level",
                "short_desc": "Minimum stock alert",
                "desc": "Alert when stock falls below this level",
            },
            "MaxStockAlertLevel": {
                "type": SoftFloatOrNothingType,
                "display_name": "Max Stock Alert Level",
                "short_desc": "Maximum stock alert",
                "desc": "Alert when stock exceeds this level",
            },
            "ReOrderPoint": {
                "type": SoftFloatOrNothingType,
                "display_name": "Reorder Point",
                "short_desc": "Reorder point",
                "desc": "Stock level at which to reorder",
            },
            "PackSize": {
                "type": SoftFloatOrNothingType,
                "display_name": "Pack Size",
                "short_desc": "Pack size",
                "desc": "The pack size for this product",
            },
            "Weight": {
                "type": SoftFloatOrNothingType,
                "display_name": "Weight",
                "short_desc": "Product weight",
                "desc": "The product weight",
            },
            "Width": {
                "type": SoftFloatOrNothingType,
                "display_name": "Width",
                "short_desc": "Product width",
                "desc": "The product width",
            },
            "Height": {
                "type": SoftFloatOrNothingType,
                "display_name": "Height",
                "short_desc": "Product height",
                "desc": "The product height",
            },
            "Depth": {
                "type": SoftFloatOrNothingType,
                "display_name": "Depth",
                "short_desc": "Product depth",
                "desc": "The product depth",
            },
            "IsSerialized": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Serialized",
                "short_desc": "Serial number tracking",
                "desc": "Whether the product uses serial number tracking",
            },
            "IsBatchTracked": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Batch Tracked",
                "short_desc": "Batch tracking",
                "desc": "Whether the product uses batch tracking",
            },
            "IsSellable": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Sellable",
                "short_desc": "Can be sold",
                "desc": "Whether the product can be sold",
            },
            "IsPurchasable": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Purchasable",
                "short_desc": "Can be purchased",
                "desc": "Whether the product can be purchased",
            },
            "Obsolete": {
                "type": SoftBoolOrNothingType,
                "display_name": "Obsolete",
                "short_desc": "Is product obsolete",
                "desc": "Whether the product is marked as obsolete",
            },
            "ProductGroup": {
                "type": SoftAutoHashOrNothingType,
                "display_name": "Product Group",
                "short_desc": "Product group",
                "desc": "The product group",
            },
            "ProductSubGroup": {
                "type": SoftAutoHashOrNothingType,
                "display_name": "Product Sub Group",
                "short_desc": "Product sub group",
                "desc": "The product sub group",
            },
            "ProductBrand": {
                "type": SoftAutoHashOrNothingType,
                "display_name": "Product Brand",
                "short_desc": "Product brand",
                "desc": "The product brand",
            },
            "Supplier": {
                "type": SoftAutoHashOrNothingType,
                "display_name": "Supplier",
                "short_desc": "Default supplier",
                "desc": "The default supplier for this product",
            },
            "UnitOfMeasure": {
                "type": SoftAutoHashOrNothingType,
                "display_name": "Unit of Measure",
                "short_desc": "Unit of measure",
                "desc": "The base unit of measure for this product",
            },
            "CreatedOn": {
                "type": SoftDateOrNothingType,
                "display_name": "Created On",
                "short_desc": "Creation date",
                "desc": "When the product was created",
            },
            "LastModifiedOn": {
                "type": SoftDateOrNothingType,
                "display_name": "Last Modified On",
                "short_desc": "Last modification date",
                "desc": "When the product was last modified",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("UnleashedProductDataType") {
        addQoreFields(Fields);
    }
}

#! Product creation request data type
public class UnleashedCreateProductRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "ProductCode": {
                "type": SoftStringType,
                "display_name": "Product Code",
                "short_desc": "Unique product code",
                "desc": "The unique product code (required, cannot be modified after creation)",
                "required": True,
            },
            "ProductDescription": {
                "type": SoftStringType,
                "display_name": "Product Description",
                "short_desc": "Product name/description",
                "desc": "The product's description/name (required)",
                "required": True,
            },
            "DefaultPurchasePrice": {
                "type": SoftFloatOrNothingType,
                "display_name": "Default Purchase Price",
                "short_desc": "Default purchase price",
                "desc": "The default purchase price for this product",
            },
            "DefaultSellPrice": {
                "type": SoftFloatOrNothingType,
                "display_name": "Default Sell Price",
                "short_desc": "Default sell price",
                "desc": "The default sell price for this product",
            },
            "MinimumSellPrice": {
                "type": SoftFloatOrNothingType,
                "display_name": "Minimum Sell Price",
                "short_desc": "Minimum sell price",
                "desc": "The minimum sell price for this product",
            },
            "IsSerialized": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Serialized",
                "short_desc": "Serial number tracking",
                "desc": "Whether the product uses serial number tracking",
            },
            "IsBatchTracked": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Batch Tracked",
                "short_desc": "Batch tracking",
                "desc": "Whether the product uses batch tracking",
            },
            "IsSellable": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Sellable",
                "short_desc": "Can be sold",
                "desc": "Whether the product can be sold (defaults to true)",
            },
            "IsPurchasable": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Purchasable",
                "short_desc": "Can be purchased",
                "desc": "Whether the product can be purchased (defaults to true)",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("UnleashedCreateProductRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace UnleashedDataProvider
