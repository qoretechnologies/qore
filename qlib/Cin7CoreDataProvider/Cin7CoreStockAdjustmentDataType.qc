# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreStockAdjustmentDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Cin7Core stock adjustment data type
public class Cin7CoreStockAdjustmentDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "TaskID": {
                "type": SoftStringOrNothingType,
                "display_name": "Task ID",
                "short_desc": "Unique adjustment identifier",
                "desc": "The unique identifier (GUID) of the stock adjustment",
            },
            "AdjustmentNumber": {
                "type": SoftStringOrNothingType,
                "display_name": "Adjustment Number",
                "short_desc": "Adjustment number",
                "desc": "The stock adjustment number",
            },
            "Status": {
                "type": SoftStringOrNothingType,
                "display_name": "Status",
                "short_desc": "Adjustment status",
                "desc": "Status of the stock adjustment",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "DRAFT", "desc": "Draft adjustment"},
                    <AllowedValueInfo>{"value": "AUTHORISED", "desc": "Authorised adjustment"},
                    <AllowedValueInfo>{"value": "VOID", "desc": "Voided adjustment"},
                ),
            },
            "AdjustmentDate": {
                "type": SoftStringOrNothingType,
                "display_name": "Adjustment Date",
                "short_desc": "Date of adjustment",
                "desc": "The date the adjustment was made",
            },
            "Location": {
                "type": SoftStringOrNothingType,
                "display_name": "Location",
                "short_desc": "Warehouse location",
                "desc": "The warehouse location for the adjustment",
            },
            "Account": {
                "type": SoftStringOrNothingType,
                "display_name": "Account",
                "short_desc": "Adjustment account",
                "desc": "The account for the adjustment",
            },
            "Reason": {
                "type": SoftStringOrNothingType,
                "display_name": "Reason",
                "short_desc": "Adjustment reason",
                "desc": "The reason for the stock adjustment",
            },
            "Lines": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Lines",
                "short_desc": "Adjustment lines",
                "desc": "List of stock adjustment line items",
            },
            "LastModifiedOn": {
                "type": SoftStringOrNothingType,
                "display_name": "Last Modified",
                "short_desc": "Last modification date",
                "desc": "ISO 8601 timestamp when the adjustment was last modified",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreStockAdjustmentDataType") {
        addQoreFields(Fields);
    }
}

#! Cin7Core create stock adjustment request data type
public class Cin7CoreCreateStockAdjustmentRequestDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "AdjustmentDate": {
                "type": SoftStringOrNothingType,
                "display_name": "Adjustment Date",
                "short_desc": "Date of adjustment",
                "desc": "The date of the adjustment (defaults to today)",
            },
            "Location": {
                "type": SoftStringType,
                "display_name": "Location",
                "short_desc": "Warehouse location",
                "desc": "The warehouse location for the adjustment (required)",
                "required": True,
            },
            "Account": {
                "type": SoftStringOrNothingType,
                "display_name": "Account",
                "short_desc": "Adjustment account",
                "desc": "The account for the adjustment",
            },
            "Reason": {
                "type": SoftStringOrNothingType,
                "display_name": "Reason",
                "short_desc": "Adjustment reason",
                "desc": "The reason for the stock adjustment",
            },
            "Lines": {
                "type": SoftAutoListType,
                "display_name": "Lines",
                "short_desc": "Adjustment lines",
                "desc": "List of stock adjustment line items (required)",
                "required": True,
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreCreateStockAdjustmentRequestDataType") {
        addQoreFields(Fields);
    }
}

#! Cin7Core stock adjustment line data type
public class Cin7CoreStockAdjustmentLineDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "ProductID": {
                "type": SoftStringOrNothingType,
                "display_name": "Product ID",
                "short_desc": "Product identifier",
                "desc": "The product GUID",
            },
            "SKU": {
                "type": SoftStringOrNothingType,
                "display_name": "SKU",
                "short_desc": "Stock keeping unit",
                "desc": "The product SKU",
            },
            "Name": {
                "type": SoftStringOrNothingType,
                "display_name": "Name",
                "short_desc": "Product name",
                "desc": "The product name",
            },
            "Quantity": {
                "type": SoftFloatOrNothingType,
                "display_name": "Quantity",
                "short_desc": "Adjustment quantity",
                "desc": "The quantity to adjust (positive to add, negative to remove)",
            },
            "UnitCost": {
                "type": SoftFloatOrNothingType,
                "display_name": "Unit Cost",
                "short_desc": "Cost per unit",
                "desc": "The unit cost for the adjustment",
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
                "type": SoftStringOrNothingType,
                "display_name": "Expiry Date",
                "short_desc": "Expiration date",
                "desc": "The product's expiry date",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreStockAdjustmentLineDataType") {
        addQoreFields(Fields);
    }
}

} # namespace Cin7CoreDataProvider
