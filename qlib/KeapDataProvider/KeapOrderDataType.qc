# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KeapDataProvider module definition

/*  KeapOrderDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {

#! Keap order record type
public class KeapOrderDataType inherits HashDataType {
    private {
        #! Field definitions for Keap orders
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The order ID",
                "desc": "The unique identifier of the order",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "The order title",
                "desc": "The title of the order",
                "example_value": "Order #1001",
            },
            "contact": {
                "type": AutoHashOrNothingType,
                "display_name": "Contact",
                "short_desc": "The associated contact",
                "desc": "The contact associated with this order (hash with id, first_name, "
                    "last_name, email, etc.)",
            },
            "order_date": {
                "type": DateOrNothingType,
                "display_name": "Order Date",
                "short_desc": "When the order was placed",
                "desc": "The date when the order was placed",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The order status",
                "desc": "The current status of the order",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "PAID"},
                    <AllowedValueInfo>{"value": "PENDING"},
                    <AllowedValueInfo>{"value": "REFUNDED"},
                ),
            },
            "total": {
                "type": FloatOrNothingType,
                "display_name": "Total",
                "short_desc": "The order total",
                "desc": "The total amount of the order",
                "example_value": 99.99,
            },
            "order_items": {
                "type": AutoListOrNothingType,
                "display_name": "Order Items",
                "short_desc": "The items in the order",
                "desc": "A list of line items included in the order",
            },
            "shipping_information": {
                "type": AutoHashOrNothingType,
                "display_name": "Shipping Information",
                "short_desc": "Shipping details",
                "desc": "The shipping information for the order (hash with address, city, state, "
                    "zip, country, etc.)",
            },
            "creation_date": {
                "type": DateOrNothingType,
                "display_name": "Creation Date",
                "short_desc": "When the order was created",
                "desc": "The timestamp when the order was created",
            },
            "modification_date": {
                "type": DateOrNothingType,
                "display_name": "Modification Date",
                "short_desc": "When the order was last modified",
                "desc": "The timestamp when the order was last modified",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace KeapDataProvider
