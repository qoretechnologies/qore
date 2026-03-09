# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreProductDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the Cin7CoreDataProvider module
public namespace Cin7CoreDataProvider {

#! Cin7Core product data type
public class Cin7CoreProductDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "ID": {
                "type": SoftStringOrNothingType,
                "display_name": "Product ID",
                "short_desc": "Unique product identifier",
                "desc": "The unique identifier (GUID) of the product in Cin7Core",
            },
            "SKU": {
                "type": SoftStringOrNothingType,
                "display_name": "SKU",
                "short_desc": "Stock keeping unit",
                "desc": "The product's stock keeping unit code",
            },
            "Name": {
                "type": SoftStringOrNothingType,
                "display_name": "Name",
                "short_desc": "Product name",
                "desc": "The product name",
            },
            "Category": {
                "type": SoftStringOrNothingType,
                "display_name": "Category",
                "short_desc": "Product category",
                "desc": "The product category",
            },
            "Brand": {
                "type": SoftStringOrNothingType,
                "display_name": "Brand",
                "short_desc": "Brand name",
                "desc": "The product brand",
            },
            "Type": {
                "type": SoftStringOrNothingType,
                "display_name": "Type",
                "short_desc": "Product type",
                "desc": "The product type",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Stock", "desc": "Stocked/inventory product"},
                    <AllowedValueInfo>{"value": "Non-Stock", "desc": "Non-stocked product"},
                    <AllowedValueInfo>{"value": "Service", "desc": "Service item"},
                ),
            },
            "CostingMethod": {
                "type": SoftStringOrNothingType,
                "display_name": "Costing Method",
                "short_desc": "Costing method",
                "desc": "The inventory costing method",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "FIFO", "desc": "First In, First Out"},
                    <AllowedValueInfo>{"value": "FEFO", "desc": "First Expired, First Out"},
                    <AllowedValueInfo>{"value": "Special - Loss", "desc": "Special batch - loss on disposal"},
                    <AllowedValueInfo>{"value": "Special - Margin", "desc": "Special batch - margin-based"},
                ),
            },
            "Status": {
                "type": SoftStringOrNothingType,
                "display_name": "Status",
                "short_desc": "Product status",
                "desc": "Active or deprecated status",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Active", "desc": "Active product"},
                    <AllowedValueInfo>{"value": "Deprecated", "desc": "Deprecated product"},
                ),
            },
            "Description": {
                "type": SoftStringOrNothingType,
                "display_name": "Description",
                "short_desc": "Product description",
                "desc": "Detailed product description",
            },
            "UOM": {
                "type": SoftStringOrNothingType,
                "display_name": "Unit of Measure",
                "short_desc": "UOM",
                "desc": "The unit of measure",
            },
            "DefaultLocation": {
                "type": SoftStringOrNothingType,
                "display_name": "Default Location",
                "short_desc": "Default warehouse",
                "desc": "The default warehouse location",
            },
            "Length": {
                "type": SoftFloatOrNothingType,
                "display_name": "Length",
                "short_desc": "Product length",
                "desc": "Product length dimension",
            },
            "Width": {
                "type": SoftFloatOrNothingType,
                "display_name": "Width",
                "short_desc": "Product width",
                "desc": "Product width dimension",
            },
            "Height": {
                "type": SoftFloatOrNothingType,
                "display_name": "Height",
                "short_desc": "Product height",
                "desc": "Product height dimension",
            },
            "Weight": {
                "type": SoftFloatOrNothingType,
                "display_name": "Weight",
                "short_desc": "Product weight",
                "desc": "Product weight",
            },
            "WeightUnits": {
                "type": SoftStringOrNothingType,
                "display_name": "Weight Units",
                "short_desc": "Weight unit",
                "desc": "Weight unit of measure",
            },
            "DimensionUnits": {
                "type": SoftStringOrNothingType,
                "display_name": "Dimension Units",
                "short_desc": "Dimension unit",
                "desc": "Dimension unit of measure",
            },
            "Barcode": {
                "type": SoftStringOrNothingType,
                "display_name": "Barcode",
                "short_desc": "Product barcode",
                "desc": "Product barcode",
            },
            "MinimumBeforeReorder": {
                "type": SoftFloatOrNothingType,
                "display_name": "Reorder Point",
                "short_desc": "Minimum before reorder",
                "desc": "Stock level that triggers reorder",
            },
            "ReorderQuantity": {
                "type": SoftFloatOrNothingType,
                "display_name": "Reorder Quantity",
                "short_desc": "Reorder qty",
                "desc": "Quantity to reorder",
            },
            "PriceTiers": {
                "type": new ListDataType("PriceTiers", new Cin7CorePriceTierDataType(), True),
                "display_name": "Price Tiers",
                "short_desc": "Pricing tiers",
                "desc": "List of price tier settings",
            },
            "SupplierInfo": {
                "type": new ListDataType("SupplierInfo", new Cin7CoreProductSupplierDataType(), True),
                "display_name": "Supplier Info",
                "short_desc": "Supplier details",
                "desc": "List of supplier information",
            },
            "Tags": {
                "type": SoftStringOrNothingType,
                "display_name": "Tags",
                "short_desc": "Product tags",
                "desc": "Comma-separated tags",
            },
            "LastModifiedOn": {
                "type": SoftDateOrNothingType,
                "display_name": "Last Modified",
                "short_desc": "Last modification date",
                "desc": "Timestamp when the product was last modified",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreProductDataType") {
        addQoreFields(Fields);
    }
}

#! Cin7Core product availability data type
public class Cin7CoreProductAvailabilityDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "ID": {
                "type": SoftStringOrNothingType,
                "display_name": "Product ID",
                "short_desc": "Product identifier",
                "desc": "The unique identifier of the product",
            },
            "SKU": {
                "type": SoftStringOrNothingType,
                "display_name": "SKU",
                "short_desc": "Stock keeping unit",
                "desc": "The product's SKU code",
            },
            "Name": {
                "type": SoftStringOrNothingType,
                "display_name": "Name",
                "short_desc": "Product name",
                "desc": "The product name",
            },
            "Location": {
                "type": SoftStringOrNothingType,
                "display_name": "Location",
                "short_desc": "Warehouse location",
                "desc": "The warehouse location",
            },
            "Bin": {
                "type": SoftStringOrNothingType,
                "display_name": "Bin",
                "short_desc": "Bin location",
                "desc": "The bin location within the warehouse",
            },
            "Batch": {
                "type": SoftStringOrNothingType,
                "display_name": "Batch",
                "short_desc": "Batch number",
                "desc": "The batch number",
            },
            "ExpiryDate": {
                "type": SoftDateOrNothingType,
                "display_name": "Expiry Date",
                "short_desc": "Expiration date",
                "desc": "The product's expiry date",
            },
            "OnHand": {
                "type": SoftFloatOrNothingType,
                "display_name": "On Hand",
                "short_desc": "Quantity on hand",
                "desc": "Current quantity in stock",
            },
            "Allocated": {
                "type": SoftFloatOrNothingType,
                "display_name": "Allocated",
                "short_desc": "Allocated quantity",
                "desc": "Quantity allocated to orders",
            },
            "Available": {
                "type": SoftFloatOrNothingType,
                "display_name": "Available",
                "short_desc": "Available quantity",
                "desc": "Quantity available for sale (OnHand - Allocated)",
            },
            "OnOrder": {
                "type": SoftFloatOrNothingType,
                "display_name": "On Order",
                "short_desc": "Quantity on order",
                "desc": "Quantity on purchase orders",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreProductAvailabilityDataType") {
        addQoreFields(Fields);
    }
}

#! Cin7Core price tier data type
public class Cin7CorePriceTierDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "PriceTierName": {
                "type": SoftStringOrNothingType,
                "display_name": "Price Tier Name",
                "short_desc": "Tier name",
                "desc": "The name of the price tier",
            },
            "Price": {
                "type": SoftFloatOrNothingType,
                "display_name": "Price",
                "short_desc": "Tier price",
                "desc": "The price for this tier",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CorePriceTierDataType") {
        addQoreFields(Fields);
    }
}

#! Cin7Core product supplier data type
public class Cin7CoreProductSupplierDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "SupplierID": {
                "type": SoftStringOrNothingType,
                "display_name": "Supplier ID",
                "short_desc": "Supplier identifier",
                "desc": "The unique identifier of the supplier",
            },
            "SupplierInventoryCode": {
                "type": SoftStringOrNothingType,
                "display_name": "Supplier Inventory Code",
                "short_desc": "Supplier SKU",
                "desc": "The supplier's inventory code for this product",
            },
            "SupplierProductName": {
                "type": SoftStringOrNothingType,
                "display_name": "Supplier Product Name",
                "short_desc": "Supplier product name",
                "desc": "The product name as used by the supplier",
            },
            "Cost": {
                "type": SoftFloatOrNothingType,
                "display_name": "Cost",
                "short_desc": "Unit cost",
                "desc": "The unit cost from this supplier",
            },
            "FixedCost": {
                "type": SoftFloatOrNothingType,
                "display_name": "Fixed Cost",
                "short_desc": "Fixed cost",
                "desc": "The fixed cost from this supplier",
            },
            "Currency": {
                "type": SoftStringOrNothingType,
                "display_name": "Currency",
                "short_desc": "Currency code",
                "desc": "The currency for pricing from this supplier",
            },
            "DropShip": {
                "type": SoftBoolOrNothingType,
                "display_name": "Drop Ship",
                "short_desc": "Drop ship flag",
                "desc": "Whether this supplier supports drop shipping",
            },
            "URL": {
                "type": SoftStringOrNothingType,
                "display_name": "URL",
                "short_desc": "Supplier URL",
                "desc": "The URL for this product at the supplier",
            },
            "LastSupplied": {
                "type": SoftDateOrNothingType,
                "display_name": "Last Supplied",
                "short_desc": "Last supplied date",
                "desc": "The date this product was last supplied",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreProductSupplierDataType") {
        addQoreFields(Fields);
    }
}

} # namespace Cin7CoreDataProvider
