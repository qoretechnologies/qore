# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoBooksDataProvider module definition

/*  ZohoBooksSalesOrderDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Zoho Books sales order data type
public class ZohoBooksSalesOrderDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "salesorder_id": {
                "display_name": "Sales Order ID",
                "short_desc": "Unique identifier for the sales order",
                "desc": "The unique identifier for this sales order in Zoho Books",
                "type": SoftStringType,
            },
            "salesorder_number": {
                "display_name": "Sales Order Number",
                "short_desc": "Sales order number",
                "desc": "The sales order number visible to customers",
                "type": SoftStringOrNothingType,
            },
            "customer_id": {
                "display_name": "Customer ID",
                "short_desc": "Customer identifier",
                "desc": "The ID of the customer for this sales order",
                "type": SoftStringType,
            },
            "customer_name": {
                "display_name": "Customer Name",
                "short_desc": "Customer name",
                "desc": "The name of the customer",
                "type": SoftStringOrNothingType,
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Sales order status",
                "desc": "Status of the sales order (draft, open, closed, void, etc.)",
                "type": SoftStringOrNothingType,
            },
            "date": {
                "display_name": "Sales Order Date",
                "short_desc": "Date of the sales order",
                "desc": "The date of the sales order (YYYY-MM-DD format)",
                "type": SoftStringOrNothingType,
            },
            "shipment_date": {
                "display_name": "Shipment Date",
                "short_desc": "Expected shipment date",
                "desc": "The expected date of shipment",
                "type": SoftStringOrNothingType,
            },
            "currency_id": {
                "display_name": "Currency ID",
                "short_desc": "Currency identifier",
                "desc": "The currency ID for the sales order",
                "type": SoftStringOrNothingType,
            },
            "currency_code": {
                "display_name": "Currency Code",
                "short_desc": "Currency code",
                "desc": "The currency code (e.g., USD, EUR)",
                "type": SoftStringOrNothingType,
            },
            "exchange_rate": {
                "display_name": "Exchange Rate",
                "short_desc": "Currency exchange rate",
                "desc": "The exchange rate for currency conversion",
                "type": SoftFloatOrNothingType,
            },
            "sub_total": {
                "display_name": "Subtotal",
                "short_desc": "Subtotal before tax",
                "desc": "The subtotal amount before taxes and discounts",
                "type": SoftFloatOrNothingType,
            },
            "tax_total": {
                "display_name": "Tax Total",
                "short_desc": "Total tax amount",
                "desc": "The total tax amount on the sales order",
                "type": SoftFloatOrNothingType,
            },
            "discount": {
                "display_name": "Discount",
                "short_desc": "Discount amount",
                "desc": "The discount amount or percentage",
                "type": SoftFloatOrNothingType,
            },
            "total": {
                "display_name": "Total",
                "short_desc": "Sales order total",
                "desc": "The total amount of the sales order",
                "type": SoftFloatOrNothingType,
            },
            "reference_number": {
                "display_name": "Reference Number",
                "short_desc": "Reference number",
                "desc": "An optional reference number (e.g., PO number)",
                "type": SoftStringOrNothingType,
            },
            "line_items": {
                "display_name": "Line Items",
                "short_desc": "Sales order line items",
                "desc": "The line items on the sales order",
                "type": new SoftListDataType("LineItems", new ZohoBooksLineItemDataType(), True),
            },
            "notes": {
                "display_name": "Notes",
                "short_desc": "Sales order notes",
                "desc": "Notes to display on the sales order",
                "type": SoftStringOrNothingType,
            },
            "terms": {
                "display_name": "Terms & Conditions",
                "short_desc": "Terms and conditions",
                "desc": "Terms and conditions to display on the sales order",
                "type": SoftStringOrNothingType,
            },
            "delivery_method": {
                "display_name": "Delivery Method",
                "short_desc": "Delivery method",
                "desc": "The delivery method for the sales order",
                "type": SoftStringOrNothingType,
            },
            "salesperson_name": {
                "display_name": "Salesperson",
                "short_desc": "Salesperson name",
                "desc": "The name of the salesperson",
                "type": SoftStringOrNothingType,
            },
            "created_time": {
                "display_name": "Created Time",
                "short_desc": "When the sales order was created",
                "desc": "The timestamp when the sales order was created",
                "type": SoftStringOrNothingType,
            },
            "last_modified_time": {
                "display_name": "Last Modified Time",
                "short_desc": "When the sales order was last modified",
                "desc": "The timestamp when the sales order was last modified",
                "type": SoftStringOrNothingType,
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}

} # namespace ZohoBooksDataProvider
