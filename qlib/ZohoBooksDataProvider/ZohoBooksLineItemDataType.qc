# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoBooksDataProvider module definition

/*  ZohoBooksLineItemDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ZohoBooksDataProvider module
public namespace ZohoBooksDataProvider {

#! Zoho Books line item data type for invoices, bills, estimates, etc.
public class ZohoBooksLineItemDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "line_item_id": {
                "display_name": "Line Item ID",
                "short_desc": "Unique identifier for the line item",
                "desc": "The unique identifier for this line item",
                "type": SoftStringOrNothingType,
            },
            "item_id": {
                "display_name": "Item ID",
                "short_desc": "Reference to an inventory item",
                "desc": "The ID of the item from the Items list (optional)",
                "type": SoftStringOrNothingType,
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Name of the line item",
                "desc": "The name or description of the line item",
                "type": StringType,
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Additional description",
                "desc": "Additional description for the line item",
                "type": SoftStringOrNothingType,
            },
            "quantity": {
                "display_name": "Quantity",
                "short_desc": "Quantity of items",
                "desc": "The quantity of items",
                "type": SoftFloatType,
                "default_value": 1.0,
            },
            "rate": {
                "display_name": "Rate",
                "short_desc": "Price per unit",
                "desc": "The price per unit for this item",
                "type": SoftFloatType,
            },
            "unit": {
                "display_name": "Unit",
                "short_desc": "Unit of measurement",
                "desc": "The unit of measurement (e.g., pcs, hrs, kg)",
                "type": SoftStringOrNothingType,
            },
            "item_total": {
                "display_name": "Item Total",
                "short_desc": "Total for this line item",
                "desc": "The total amount for this line item (quantity * rate)",
                "type": SoftFloatOrNothingType,
            },
            "discount": {
                "display_name": "Discount",
                "short_desc": "Discount amount or percentage",
                "desc": "Discount applied to this line item",
                "type": SoftFloatOrNothingType,
            },
            "discount_amount": {
                "display_name": "Discount Amount",
                "short_desc": "Discount amount",
                "desc": "The discount amount for this line item",
                "type": SoftFloatOrNothingType,
            },
            "tax_id": {
                "display_name": "Tax ID",
                "short_desc": "Tax rate ID",
                "desc": "The ID of the tax rate to apply",
                "type": SoftStringOrNothingType,
            },
            "tax_name": {
                "display_name": "Tax Name",
                "short_desc": "Name of the tax",
                "desc": "The name of the tax applied",
                "type": SoftStringOrNothingType,
            },
            "tax_percentage": {
                "display_name": "Tax Percentage",
                "short_desc": "Tax rate percentage",
                "desc": "The tax percentage applied",
                "type": SoftFloatOrNothingType,
            },
            "account_id": {
                "display_name": "Account ID",
                "short_desc": "Income/expense account",
                "desc": "The account ID for this line item",
                "type": SoftStringOrNothingType,
            },
            "project_id": {
                "display_name": "Project ID",
                "short_desc": "Associated project",
                "desc": "The project ID this line item is associated with",
                "type": SoftStringOrNothingType,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ZohoBooksDataProvider
