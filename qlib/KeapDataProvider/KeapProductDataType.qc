# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KeapDataProvider module definition

/*  KeapProductDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Keap product record type
public class KeapProductDataType inherits HashDataType {
    private {
        #! Field definitions for Keap products
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The product ID",
                "desc": "The unique identifier of the product",
            },
            "product_name": {
                "type": StringOrNothingType,
                "display_name": "Product Name",
                "short_desc": "The product name",
                "desc": "The name of the product",
                "example_value": "Premium Subscription",
            },
            "product_price": {
                "type": FloatOrNothingType,
                "display_name": "Product Price",
                "short_desc": "The product price",
                "desc": "The price of the product",
                "example_value": 29.99,
            },
            "product_desc": {
                "type": StringOrNothingType,
                "display_name": "Product Description",
                "short_desc": "The product description",
                "desc": "A description of the product",
            },
            "sku": {
                "type": StringOrNothingType,
                "display_name": "SKU",
                "short_desc": "The product SKU",
                "desc": "The stock keeping unit (SKU) code for the product",
                "example_value": "PREM-SUB-001",
            },
            "status": {
                "type": IntOrNothingType,
                "display_name": "Status",
                "short_desc": "The product status",
                "desc": "The status of the product",
                "allowed_values": (
                    <AllowedValueInfo>{"value": 0, "desc": "Inactive"},
                    <AllowedValueInfo>{"value": 1, "desc": "Active"},
                ),
            },
            "subscription_only": {
                "type": BoolOrNothingType,
                "display_name": "Subscription Only",
                "short_desc": "Whether the product is subscription-only",
                "desc": "Indicates whether the product is only available as a subscription "
                    "(`True` or `False`)",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace KeapDataProvider
