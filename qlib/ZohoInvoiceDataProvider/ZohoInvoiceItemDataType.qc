# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInvoiceDataProvider module definition

/*  ZohoInvoiceItemDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ZohoInvoiceDataProvider module
public namespace ZohoInvoiceDataProvider {

#! Zoho Invoice item data type
public class ZohoInvoiceItemDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "item_id": {
                "display_name": "Item ID",
                "short_desc": "Unique identifier for the item",
                "desc": "The unique identifier for this item",
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
            "rate": {
                "display_name": "Rate",
                "short_desc": "Item price",
                "desc": "The price per unit of the item",
                "type": SoftFloatOrNothingType,
            },
            "unit": {
                "display_name": "Unit",
                "short_desc": "Unit of measurement",
                "desc": "The unit of measurement (e.g., pcs, hrs, kg)",
                "type": SoftStringOrNothingType,
            },
            "sku": {
                "display_name": "SKU",
                "short_desc": "Stock keeping unit",
                "desc": "The stock keeping unit (SKU) code",
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
            "item_type": {
                "display_name": "Item Type",
                "short_desc": "Type of item",
                "desc": "The type of item (sales, purchases, sales_and_purchases)",
                "type": SoftStringOrNothingType,
            },
            "product_type": {
                "display_name": "Product Type",
                "short_desc": "Product type",
                "desc": "The product type (goods, service)",
                "type": SoftStringOrNothingType,
            },
            "tax_id": {
                "display_name": "Tax ID",
                "short_desc": "Default tax ID",
                "desc": "The default tax ID for the item",
                "type": SoftStringOrNothingType,
            },
            "tax_name": {
                "display_name": "Tax Name",
                "short_desc": "Tax name",
                "desc": "The name of the default tax",
                "type": SoftStringOrNothingType,
            },
            "tax_percentage": {
                "display_name": "Tax Percentage",
                "short_desc": "Tax rate percentage",
                "desc": "The default tax percentage",
                "type": SoftFloatOrNothingType,
            },
            "account_id": {
                "display_name": "Account ID",
                "short_desc": "Income account",
                "desc": "The income account ID for the item",
                "type": SoftStringOrNothingType,
            },
            "account_name": {
                "display_name": "Account Name",
                "short_desc": "Account name",
                "desc": "The name of the income account",
                "type": SoftStringOrNothingType,
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

#! Zoho Invoice create item request data type
public class ZohoInvoiceCreateItemRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating an item
        const Fields = {
            "name": {
                "display_name": "Name",
                "short_desc": "Item name",
                "desc": "The name of the item",
                "type": SoftStringType,
                "required": True,
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Item description",
                "desc": "Description of the item",
                "type": SoftStringOrNothingType,
            },
            "rate": {
                "display_name": "Rate",
                "short_desc": "Item price",
                "desc": "The price per unit of the item",
                "type": SoftFloatOrNothingType,
            },
            "unit": {
                "display_name": "Unit",
                "short_desc": "Unit of measurement",
                "desc": "The unit of measurement (e.g., pcs, hrs, kg)",
                "type": SoftStringOrNothingType,
            },
            "sku": {
                "display_name": "SKU",
                "short_desc": "Stock keeping unit",
                "desc": "The stock keeping unit (SKU) code",
                "type": SoftStringOrNothingType,
            },
            "item_type": {
                "display_name": "Item Type",
                "short_desc": "Type of item",
                "desc": "The type of item (sales, purchases, sales_and_purchases)",
                "type": SoftStringOrNothingType,
            },
            "product_type": {
                "display_name": "Product Type",
                "short_desc": "Product type",
                "desc": "The product type (goods, service)",
                "type": SoftStringOrNothingType,
            },
            "tax_id": {
                "display_name": "Tax ID",
                "short_desc": "Default tax ID",
                "desc": "The default tax ID for the item",
                "type": SoftStringOrNothingType,
            },
            "account_id": {
                "display_name": "Account ID",
                "short_desc": "Income account",
                "desc": "The income account ID for the item",
                "type": SoftStringOrNothingType,
            },
            "custom_fields": {
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "Custom field values for the item",
                "type": AutoHashOrNothingType,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ZohoInvoiceDataProvider
