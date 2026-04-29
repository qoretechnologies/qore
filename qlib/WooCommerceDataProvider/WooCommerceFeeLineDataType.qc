# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceFeeLineDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce order fee-line data type
public class WooCommerceFeeLineDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Fee line ID",
                "type": IntOrNothingType,
                "desc": "The fee line ID (omit when creating)",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Fee name",
                "type": StringOrNothingType,
                "desc": "The fee name shown to customers",
                "example_value": "Handling Fee",
            },
            "tax_class": {
                "display_name": "Tax Class",
                "short_desc": "Tax class slug",
                "type": StringOrNothingType,
                "desc": "Tax class slug applied to the fee",
            },
            "tax_status": {
                "display_name": "Tax Status",
                "short_desc": "Tax status",
                "type": StringOrNothingType,
                "desc": "Tax status: `taxable` or `none`",
            },
            "total": {
                "display_name": "Total",
                "short_desc": "Fee total",
                "type": StringOrNothingType,
                "desc": "The fee total",
                "example_value": "5.00",
            },
            "total_tax": {
                "display_name": "Total Tax",
                "short_desc": "Total tax on fee",
                "type": StringOrNothingType,
                "desc": "Total tax applied to the fee",
            },
            "meta_data": {
                "display_name": "Meta Data",
                "short_desc": "Fee line meta data",
                "type": new ListDataType("FeeLineMetaData", new WooCommerceMetaDataType(), True),
                "desc": "Per-line metadata key-value pairs",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }

    #! Creates the object with a specific base type
    constructor(Type base_type) : HashDataType(base_type, "WooCommerceFeeLine") {
        addQoreFields(Fields);
    }
}

} # namespace WooCommerceDataProvider
