# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationProductDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! ShipStation product data type
public class ShipStationProductDataType inherits HashDataType {
    #! Creates the object
    constructor() : HashDataType("ShipStationProduct", {
        "productId": {
            "display_name": "Product ID",
            "short_desc": "Unique identifier for the product",
            "type": IntOrNothingType,
            "desc": "The unique numeric identifier for the product",
        },
        "sku": {
            "display_name": "SKU",
            "short_desc": "Stock keeping unit",
            "type": SoftStringOrNothingType,
            "desc": "The product's stock keeping unit (SKU)",
        },
        "name": {
            "display_name": "Name",
            "short_desc": "Product name",
            "type": SoftStringOrNothingType,
            "desc": "The product name",
        },
        "price": {
            "display_name": "Price",
            "short_desc": "Product price",
            "type": SoftFloatOrNothingType,
            "desc": "The product price",
        },
        "defaultCost": {
            "display_name": "Default Cost",
            "short_desc": "Default cost of the product",
            "type": SoftFloatOrNothingType,
            "desc": "The default cost of the product",
        },
        "length": {
            "display_name": "Length",
            "short_desc": "Product length",
            "type": SoftFloatOrNothingType,
            "desc": "The product length in inches",
        },
        "width": {
            "display_name": "Width",
            "short_desc": "Product width",
            "type": SoftFloatOrNothingType,
            "desc": "The product width in inches",
        },
        "height": {
            "display_name": "Height",
            "short_desc": "Product height",
            "type": SoftFloatOrNothingType,
            "desc": "The product height in inches",
        },
        "weightOz": {
            "display_name": "Weight (oz)",
            "short_desc": "Product weight in ounces",
            "type": SoftFloatOrNothingType,
            "desc": "The product weight in ounces",
        },
        "internalNotes": {
            "display_name": "Internal Notes",
            "short_desc": "Internal notes about the product",
            "type": SoftStringOrNothingType,
            "desc": "Internal notes about the product",
        },
        "fulfillmentSku": {
            "display_name": "Fulfillment SKU",
            "short_desc": "SKU used by fulfillment center",
            "type": SoftStringOrNothingType,
            "desc": "The SKU used by the fulfillment center (if different from SKU)",
        },
        "createDate": {
            "display_name": "Create Date",
            "short_desc": "Date the product was created",
            "type": DateOrNothingType,
            "desc": "The date and time the product was created",
        },
        "modifyDate": {
            "display_name": "Modify Date",
            "short_desc": "Date the product was last modified",
            "type": DateOrNothingType,
            "desc": "The date and time the product was last modified",
        },
        "active": {
            "display_name": "Active",
            "short_desc": "Whether the product is active",
            "type": SoftBoolOrNothingType,
            "desc": "Whether the product is active",
        },
        "productCategory": {
            "display_name": "Product Category",
            "short_desc": "Category of the product",
            "type": AutoHashOrNothingType,
            "desc": "The product category information",
        },
        "productType": {
            "display_name": "Product Type",
            "short_desc": "Type of product",
            "type": SoftStringOrNothingType,
            "desc": "The product type",
        },
        "warehouseLocation": {
            "display_name": "Warehouse Location",
            "short_desc": "Location in warehouse",
            "type": SoftStringOrNothingType,
            "desc": "The location of the product in the warehouse",
        },
        "defaultCarrierCode": {
            "display_name": "Default Carrier Code",
            "short_desc": "Default carrier for shipping",
            "type": SoftStringOrNothingType,
            "desc": "The default carrier code for shipping this product",
        },
        "defaultServiceCode": {
            "display_name": "Default Service Code",
            "short_desc": "Default shipping service",
            "type": SoftStringOrNothingType,
            "desc": "The default service code for shipping this product",
        },
        "defaultPackageCode": {
            "display_name": "Default Package Code",
            "short_desc": "Default package type",
            "type": SoftStringOrNothingType,
            "desc": "The default package code for shipping this product",
        },
        "defaultIntlCarrierCode": {
            "display_name": "Default Int'l Carrier Code",
            "short_desc": "Default international carrier",
            "type": SoftStringOrNothingType,
            "desc": "The default carrier code for international shipments",
        },
        "defaultIntlServiceCode": {
            "display_name": "Default Int'l Service Code",
            "short_desc": "Default international service",
            "type": SoftStringOrNothingType,
            "desc": "The default service code for international shipments",
        },
        "defaultIntlPackageCode": {
            "display_name": "Default Int'l Package Code",
            "short_desc": "Default international package type",
            "type": SoftStringOrNothingType,
            "desc": "The default package code for international shipments",
        },
        "defaultConfirmation": {
            "display_name": "Default Confirmation",
            "short_desc": "Default confirmation type",
            "type": SoftStringOrNothingType,
            "desc": "The default confirmation type for shipments",
        },
        "defaultIntlConfirmation": {
            "display_name": "Default Int'l Confirmation",
            "short_desc": "Default international confirmation type",
            "type": SoftStringOrNothingType,
            "desc": "The default confirmation type for international shipments",
        },
        "customsDescription": {
            "display_name": "Customs Description",
            "short_desc": "Description for customs",
            "type": SoftStringOrNothingType,
            "desc": "The customs description for international shipments",
        },
        "customsValue": {
            "display_name": "Customs Value",
            "short_desc": "Value for customs",
            "type": SoftFloatOrNothingType,
            "desc": "The customs value for international shipments",
        },
        "customsTariffNo": {
            "display_name": "Customs Tariff Number",
            "short_desc": "HS tariff number",
            "type": SoftStringOrNothingType,
            "desc": "The harmonized system tariff number for customs",
        },
        "customsCountryCode": {
            "display_name": "Customs Country Code",
            "short_desc": "Country of origin for customs",
            "type": SoftStringOrNothingType,
            "desc": "The country of origin code for customs",
        },
        "noCustoms": {
            "display_name": "No Customs",
            "short_desc": "Whether product is exempt from customs",
            "type": SoftBoolOrNothingType,
            "desc": "Whether the product is exempt from customs",
        },
        "tags": {
            "display_name": "Tags",
            "short_desc": "Product tags",
            "type": SoftAutoListOrNothingType,
            "desc": "List of tags assigned to the product",
        },
    }) {
    }
}

} # namespace ShipStationDataProvider
