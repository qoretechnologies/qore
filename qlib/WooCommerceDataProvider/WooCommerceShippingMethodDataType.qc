# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceShippingMethodDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce shipping method data type
public class WooCommerceShippingMethodDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "instance_id": {
                "display_name": "Instance ID",
                "short_desc": "Shipping method instance identifier",
                "type": IntOrNothingType,
                "desc": "The unique instance identifier for the shipping method",
            },
            "title": {
                "display_name": "Title",
                "short_desc": "Shipping method title",
                "type": StringOrNothingType,
                "desc": "The display title for the shipping method instance",
            },
            "order": {
                "display_name": "Order",
                "short_desc": "Shipping method sort order",
                "type": IntOrNothingType,
                "desc": "The sort order of the shipping method in the zone",
            },
            "enabled": {
                "display_name": "Enabled",
                "short_desc": "Whether the shipping method is enabled",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the shipping method is enabled",
            },
            "method_id": {
                "display_name": "Method ID",
                "short_desc": "Shipping method type identifier",
                "type": StringOrNothingType,
                "desc": "The shipping method type identifier (e.g., `flat_rate`, `free_shipping`)",
            },
            "method_title": {
                "display_name": "Method Title",
                "short_desc": "Shipping method type name",
                "type": StringOrNothingType,
                "desc": "The human-readable name for the shipping method type",
            },
            "method_description": {
                "display_name": "Method Description",
                "short_desc": "Shipping method description",
                "type": StringOrNothingType,
                "desc": "The description of the shipping method type",
            },
            "settings": {
                "display_name": "Settings",
                "short_desc": "Shipping method settings",
                "type": AutoHashOrNothingType,
                "desc": "The configuration settings for the shipping method instance",
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}

} # namespace WooCommerceDataProvider
