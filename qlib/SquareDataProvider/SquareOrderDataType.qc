# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SquareDataProvider module definition

/*  SquareOrderDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquareDataProvider module
public namespace SquareDataProvider {

#! Square order record type
public class SquareOrderDataType inherits HashDataType {
    private {
        #! Field definitions for Square orders
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The order ID",
                "desc": "The unique identifier of the order",
            },
            "location_id": {
                "type": StringOrNothingType,
                "display_name": "Location ID",
                "short_desc": "The location ID",
                "desc": "The ID of the location where the order was created",
            },
            "reference_id": {
                "type": StringOrNothingType,
                "display_name": "Reference ID",
                "short_desc": "External reference ID",
                "desc": "An optional reference ID from an external system",
            },
            "source": {
                "type": AutoHashOrNothingType,
                "display_name": "Source",
                "short_desc": "The order source",
                "desc": "The source of the order (hash with name field)",
            },
            "customer_id": {
                "type": StringOrNothingType,
                "display_name": "Customer ID",
                "short_desc": "The customer ID",
                "desc": "The ID of the customer associated with the order",
            },
            "line_items": {
                "type": AutoListOrNothingType,
                "display_name": "Line Items",
                "short_desc": "The order line items",
                "desc": "List of line items in the order, each with uid, name, quantity, "
                    "base_price_money, total_money, etc.",
            },
            "taxes": {
                "type": AutoListOrNothingType,
                "display_name": "Taxes",
                "short_desc": "Order-level taxes",
                "desc": "List of taxes applied to the order",
            },
            "discounts": {
                "type": AutoListOrNothingType,
                "display_name": "Discounts",
                "short_desc": "Order-level discounts",
                "desc": "List of discounts applied to the order",
            },
            "total_money": {
                "type": AutoHashOrNothingType,
                "display_name": "Total Money",
                "short_desc": "The total amount",
                "desc": "The total amount of the order (hash with amount in cents and currency)",
            },
            "total_tax_money": {
                "type": AutoHashOrNothingType,
                "display_name": "Total Tax",
                "short_desc": "The total tax amount",
                "desc": "The total tax amount of the order (hash with amount in cents and currency)",
            },
            "total_discount_money": {
                "type": AutoHashOrNothingType,
                "display_name": "Total Discount",
                "short_desc": "The total discount amount",
                "desc": "The total discount amount of the order (hash with amount in cents and currency)",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "The order state",
                "desc": "The current state of the order (OPEN, COMPLETED, CANCELED, DRAFT)",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the order was created",
                "desc": "The timestamp when the order was created (RFC 3339 format)",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the order was last updated",
                "desc": "The timestamp when the order was last updated (RFC 3339 format)",
            },
            "closed_at": {
                "type": StringOrNothingType,
                "display_name": "Closed At",
                "short_desc": "When the order was closed",
                "desc": "The timestamp when the order was closed (RFC 3339 format)",
            },
            "version": {
                "type": IntOrNothingType,
                "display_name": "Version",
                "short_desc": "The order version",
                "desc": "The current version of the order (for optimistic concurrency)",
            },
            "net_amounts": {
                "type": AutoHashOrNothingType,
                "display_name": "Net Amounts",
                "short_desc": "Net amount details",
                "desc": "Net amounts for the order (hash with total_money, tax_money, "
                    "discount_money, tip_money, service_charge_money)",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SquareDataProvider
