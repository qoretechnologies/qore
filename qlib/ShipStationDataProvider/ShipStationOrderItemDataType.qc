# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationOrderItemDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! ShipStation order item record type
/** Represents a line item within an order.
*/
public class ShipStationOrderItemDataType inherits HashDataType {
    private {
        #! Field definitions for ShipStation order items
        const Fields = {
            "orderItemId": {
                "type": IntOrNothingType,
                "display_name": "Order Item ID",
                "short_desc": "Unique item identifier",
                "desc": "The unique identifier for this order item (assigned by ShipStation)",
            },
            "lineItemKey": {
                "type": StringOrNothingType,
                "display_name": "Line Item Key",
                "short_desc": "External line item identifier",
                "desc": "An identifier for the line item in the source system",
            },
            "sku": {
                "type": StringOrNothingType,
                "display_name": "SKU",
                "short_desc": "Stock keeping unit",
                "desc": "The SKU (stock keeping unit) for this item",
                "example_value": "WIDGET-001",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "Item name",
                "desc": "The name or description of this item",
                "example_value": "Blue Widget",
            },
            "imageUrl": {
                "type": StringOrNothingType,
                "display_name": "Image URL",
                "short_desc": "Product image URL",
                "desc": "URL to an image of this item",
            },
            "weight": {
                "type": new ShipStationWeightDataType(),
                "display_name": "Weight",
                "short_desc": "Item weight",
                "desc": "The weight of a single unit of this item",
            },
            "quantity": {
                "type": IntType,
                "display_name": "Quantity",
                "short_desc": "Item quantity",
                "desc": "The quantity of this item ordered",
                "example_value": 2,
            },
            "unitPrice": {
                "type": FloatOrNothingType,
                "display_name": "Unit Price",
                "short_desc": "Price per unit",
                "desc": "The price per unit of this item",
                "example_value": 19.99,
            },
            "taxAmount": {
                "type": FloatOrNothingType,
                "display_name": "Tax Amount",
                "short_desc": "Tax for this item",
                "desc": "The tax amount for this item",
            },
            "shippingAmount": {
                "type": FloatOrNothingType,
                "display_name": "Shipping Amount",
                "short_desc": "Shipping for this item",
                "desc": "The shipping amount allocated to this item",
            },
            "warehouseLocation": {
                "type": StringOrNothingType,
                "display_name": "Warehouse Location",
                "short_desc": "Location in warehouse",
                "desc": "The location of this item in the warehouse",
            },
            "options": {
                "type": new ListDataType("ItemOptions", AbstractDataProviderTypeMap."hash<auto>", True),
                "display_name": "Options",
                "short_desc": "Item options",
                "desc": "Custom options for this item (e.g., size, color)",
            },
            "productId": {
                "type": IntOrNothingType,
                "display_name": "Product ID",
                "short_desc": "ShipStation product ID",
                "desc": "The ShipStation product ID for this item",
            },
            "fulfillmentSku": {
                "type": StringOrNothingType,
                "display_name": "Fulfillment SKU",
                "short_desc": "SKU for fulfillment",
                "desc": "The SKU used by the fulfillment provider",
            },
            "adjustment": {
                "type": BoolOrNothingType,
                "display_name": "Adjustment",
                "short_desc": "Is adjustment item",
                "desc": "Whether this is an adjustment item (e.g., discount)",
            },
            "upc": {
                "type": StringOrNothingType,
                "display_name": "UPC",
                "short_desc": "Universal Product Code",
                "desc": "The UPC barcode for this item",
            },
            "createDate": {
                "type": StringOrNothingType,
                "display_name": "Create Date",
                "short_desc": "When item was created",
                "desc": "The date this item was created in ShipStation",
            },
            "modifyDate": {
                "type": StringOrNothingType,
                "display_name": "Modify Date",
                "short_desc": "When item was modified",
                "desc": "The date this item was last modified",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ShipStationDataProvider
