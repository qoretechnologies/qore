# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoBooksDataProvider module definition

/*  ZohoBooksItemDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Zoho Books item data type
public class ZohoBooksItemDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "item_id": {
                "display_name": "Item ID",
                "short_desc": "Unique identifier for the item",
                "desc": "The unique identifier for this item in Zoho Books",
                "type": SoftStringType,
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Item name",
                "desc": "The name of the item",
                "type": SoftStringType,
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Item description",
                "desc": "Description of the item",
                "type": SoftStringOrNothingType,
            },
            "sku": {
                "display_name": "SKU",
                "short_desc": "Stock keeping unit",
                "desc": "The SKU code of the item",
                "type": SoftStringOrNothingType,
            },
            "rate": {
                "display_name": "Rate",
                "short_desc": "Unit rate",
                "desc": "The unit rate/price of the item",
                "type": SoftFloatOrNothingType,
            },
            "unit": {
                "display_name": "Unit",
                "short_desc": "Unit of measurement",
                "desc": "The unit of measurement (e.g., pcs, kg, hrs)",
                "type": SoftStringOrNothingType,
            },
            "product_type": {
                "display_name": "Product Type",
                "short_desc": "Type of product",
                "desc": "The type of product",
                "type": SoftStringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "goods", "desc": "Physical goods"},
                    <AllowedValueInfo>{"value": "service", "desc": "Services"},
                ),
            },
            "is_taxable": {
                "display_name": "Is Taxable",
                "short_desc": "Taxable flag",
                "desc": "Whether the item is taxable",
                "type": SoftBoolOrNothingType,
            },
            "tax_id": {
                "display_name": "Tax ID",
                "short_desc": "Tax identifier",
                "desc": "The ID of the tax applied to this item",
                "type": SoftStringOrNothingType,
            },
            "tax_name": {
                "display_name": "Tax Name",
                "short_desc": "Tax name",
                "desc": "The name of the tax applied",
                "type": SoftStringOrNothingType,
            },
            "tax_percentage": {
                "display_name": "Tax Percentage",
                "short_desc": "Tax percentage",
                "desc": "The tax percentage applied to this item",
                "type": SoftFloatOrNothingType,
            },
            "purchase_rate": {
                "display_name": "Purchase Rate",
                "short_desc": "Purchase price",
                "desc": "The purchase price of the item",
                "type": SoftFloatOrNothingType,
            },
            "purchase_account_id": {
                "display_name": "Purchase Account ID",
                "short_desc": "Purchase account identifier",
                "desc": "The ID of the purchase account",
                "type": SoftStringOrNothingType,
            },
            "account_id": {
                "display_name": "Account ID",
                "short_desc": "Sales account identifier",
                "desc": "The ID of the sales account",
                "type": SoftStringOrNothingType,
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Item status",
                "desc": "Status of the item",
                "type": SoftStringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "active", "desc": "Active item"},
                    <AllowedValueInfo>{"value": "inactive", "desc": "Inactive item"},
                ),
            },
            "created_time": {
                "display_name": "Created Time",
                "short_desc": "When the item was created",
                "desc": "The timestamp when the item was created",
                "type": SoftStringOrNothingType,
            },
            "last_modified_time": {
                "display_name": "Last Modified Time",
                "short_desc": "When the item was last modified",
                "desc": "The timestamp when the item was last modified",
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
