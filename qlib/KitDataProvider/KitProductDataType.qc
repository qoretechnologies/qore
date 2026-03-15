# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KitProductDataType.qc Kit product data type for purchase line items

/*  KitProductDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KitDataProvider module
public namespace KitDataProvider {

#! Data type for a product line item in a Kit purchase
public class KitProductDataType inherits HashDataType {
    public {
        #! Field definitions for a purchase product
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Product Name",
                "short_desc": "The product name",
                "desc": "The name of the product",
                "required": True,
                "example_value": "Premium Course",
            },
            "pid": {
                "type": SoftIntType,
                "display_name": "Product ID",
                "short_desc": "Unique product identifier",
                "desc": "Your unique product identifier (must be unique within the purchase)",
                "required": True,
                "example_value": 1,
            },
            "lid": {
                "type": SoftIntType,
                "display_name": "Line Item ID",
                "short_desc": "Unique line item identifier",
                "desc": "Line item identifier (must be unique within the purchase)",
                "required": True,
                "example_value": 1,
            },
            "quantity": {
                "type": SoftIntType,
                "display_name": "Quantity",
                "short_desc": "Product quantity",
                "desc": "The quantity of the product purchased",
                "required": True,
                "example_value": 1,
            },
            "unit_price": {
                "type": FloatType,
                "display_name": "Unit Price",
                "short_desc": "Price per unit",
                "desc": "The price per unit of the product",
                "required": True,
                "example_value": 29.99,
            },
            "sku": {
                "type": StringOrNothingType,
                "display_name": "SKU",
                "short_desc": "Product SKU",
                "desc": "The stock keeping unit identifier for the product",
                "example_value": "COURSE-001",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace KitDataProvider
