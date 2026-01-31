# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoBooksDataProvider module definition

/*  ZohoBooksPurchaseOrderDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Zoho Books purchase order data type
public class ZohoBooksPurchaseOrderDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "purchaseorder_id": {
                "display_name": "Purchase Order ID",
                "short_desc": "Unique identifier for the purchase order",
                "desc": "The unique identifier for this purchase order in Zoho Books",
                "type": SoftStringType,
            },
            "purchaseorder_number": {
                "display_name": "Purchase Order Number",
                "short_desc": "Purchase order number",
                "desc": "The purchase order number",
                "type": SoftStringOrNothingType,
            },
            "vendor_id": {
                "display_name": "Vendor ID",
                "short_desc": "Vendor identifier",
                "desc": "The ID of the vendor for this purchase order",
                "type": SoftStringType,
            },
            "vendor_name": {
                "display_name": "Vendor Name",
                "short_desc": "Vendor name",
                "desc": "The name of the vendor",
                "type": SoftStringOrNothingType,
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Purchase order status",
                "desc": "Status of the purchase order (draft, open, closed, void, etc.)",
                "type": SoftStringOrNothingType,
            },
            "date": {
                "display_name": "Purchase Order Date",
                "short_desc": "Date of the purchase order",
                "desc": "The date of the purchase order (YYYY-MM-DD format)",
                "type": SoftStringOrNothingType,
            },
            "delivery_date": {
                "display_name": "Delivery Date",
                "short_desc": "Expected delivery date",
                "desc": "The expected date of delivery",
                "type": SoftStringOrNothingType,
            },
            "currency_id": {
                "display_name": "Currency ID",
                "short_desc": "Currency identifier",
                "desc": "The currency ID for the purchase order",
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
                "desc": "The subtotal amount before taxes",
                "type": SoftFloatOrNothingType,
            },
            "tax_total": {
                "display_name": "Tax Total",
                "short_desc": "Total tax amount",
                "desc": "The total tax amount on the purchase order",
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
                "short_desc": "Purchase order total",
                "desc": "The total amount of the purchase order",
                "type": SoftFloatOrNothingType,
            },
            "reference_number": {
                "display_name": "Reference Number",
                "short_desc": "Reference number",
                "desc": "An optional reference number",
                "type": SoftStringOrNothingType,
            },
            "line_items": {
                "display_name": "Line Items",
                "short_desc": "Purchase order line items",
                "desc": "The line items on the purchase order",
                "type": new SoftListDataType("LineItems", new ZohoBooksLineItemDataType(), True),
            },
            "notes": {
                "display_name": "Notes",
                "short_desc": "Purchase order notes",
                "desc": "Notes for the purchase order",
                "type": SoftStringOrNothingType,
            },
            "terms": {
                "display_name": "Terms & Conditions",
                "short_desc": "Terms and conditions",
                "desc": "Terms and conditions for the purchase order",
                "type": SoftStringOrNothingType,
            },
            "ship_via": {
                "display_name": "Ship Via",
                "short_desc": "Shipping method",
                "desc": "The shipping method for the purchase order",
                "type": SoftStringOrNothingType,
            },
            "created_time": {
                "display_name": "Created Time",
                "short_desc": "When the purchase order was created",
                "desc": "The timestamp when the purchase order was created",
                "type": SoftStringOrNothingType,
            },
            "last_modified_time": {
                "display_name": "Last Modified Time",
                "short_desc": "When the purchase order was last modified",
                "desc": "The timestamp when the purchase order was last modified",
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
