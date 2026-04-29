# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceShippingLineDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the WooCommerceDataProvider module
public namespace WooCommerceDataProvider {

#! WooCommerce order shipping-line data type
public class WooCommerceShippingLineDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Shipping line ID",
                "type": IntOrNothingType,
                "desc": "The shipping line ID (omit when creating)",
            },
            "method_id": {
                "display_name": "Method ID",
                "short_desc": "Shipping method ID",
                "type": StringOrNothingType,
                "desc": "The shipping method slug (e.g. `flat_rate`, `free_shipping`)",
                "example_value": "flat_rate",
            },
            "method_title": {
                "display_name": "Method Title",
                "short_desc": "Shipping method title",
                "type": StringOrNothingType,
                "desc": "The display title of the shipping method",
                "example_value": "Standard Shipping",
            },
            "total": {
                "display_name": "Total",
                "short_desc": "Shipping total",
                "type": StringOrNothingType,
                "desc": "The shipping total",
                "example_value": "10.00",
            },
            "total_tax": {
                "display_name": "Total Tax",
                "short_desc": "Total tax on shipping",
                "type": StringOrNothingType,
                "desc": "Total tax applied to the shipping line",
            },
            "meta_data": {
                "display_name": "Meta Data",
                "short_desc": "Shipping line meta data",
                "type": new ListDataType("ShippingLineMetaData", new WooCommerceMetaDataType(), True),
                "desc": "Per-line metadata key-value pairs",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }

    #! Creates the object with a specific base type
    constructor(Type base_type) : HashDataType(base_type, "WooCommerceShippingLine") {
        addQoreFields(Fields);
    }
}

} # namespace WooCommerceDataProvider
