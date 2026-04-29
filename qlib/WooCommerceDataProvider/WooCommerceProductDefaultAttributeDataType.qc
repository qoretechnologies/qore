# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceProductDefaultAttributeDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce default attribute data type for variable products
/** Each entry selects the default option for one of the product's attributes. */
public class WooCommerceProductDefaultAttributeDataType inherits HashDataType {
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
            "option": {
                "display_name": "Option",
                "short_desc": "Selected option value",
                "type": StringOrNothingType,
                "desc": "The selected default option value (term name) for this attribute",
                "example_value": "Red",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }

    #! Creates the object with a specific base type
    constructor(Type base_type) : HashDataType(base_type, "WooCommerceProductDefaultAttribute") {
        addQoreFields(Fields);
    }
}

} # namespace WooCommerceDataProvider
