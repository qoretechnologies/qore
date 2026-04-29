# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceProductAttributeDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce product attribute data type
/** Used on the product object to declare attributes (with multiple `options`),
    not to be confused with the variation attribute selector. */
public class WooCommerceProductAttributeDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Attribute ID",
                "type": IntOrNothingType,
                "desc": "The attribute ID (taxonomy attribute) — use either `id` or `name`",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Attribute name",
                "type": StringOrNothingType,
                "desc": "The attribute name (use either `id` or `name`)",
                "example_value": "Color",
            },
            "position": {
                "display_name": "Position",
                "short_desc": "Display position",
                "type": IntOrNothingType,
                "desc": "Display order of the attribute",
                "example_value": 0,
            },
            "visible": {
                "display_name": "Visible",
                "short_desc": "Visible to customers",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the attribute is visible on the product page",
                "example_value": True,
            },
            "variation": {
                "display_name": "Variation",
                "short_desc": "Used for variations",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the attribute is used to create variations",
                "example_value": True,
            },
            "options": {
                "display_name": "Options",
                "short_desc": "Possible values",
                "type": new ListDataType("ProductAttributeOptions", SoftStringType, True),
                "desc": "List of available option values for this attribute",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }

    #! Creates the object with a specific base type
    constructor(Type base_type) : HashDataType(base_type, "WooCommerceProductAttribute") {
        addQoreFields(Fields);
    }
}

} # namespace WooCommerceDataProvider
