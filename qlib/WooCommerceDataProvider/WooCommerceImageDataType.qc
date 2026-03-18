# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceImageDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce image data type
public class WooCommerceImageDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Image ID",
                "type": IntOrNothingType,
                "desc": "The image attachment ID",
            },
            "src": {
                "display_name": "Source URL",
                "short_desc": "Image source URL",
                "type": StringOrNothingType,
                "desc": "The URL of the image",
                "example_value": "https://example.com/wp-content/uploads/2026/01/product-image.jpg",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Image name",
                "type": StringOrNothingType,
                "desc": "The image name",
                "example_value": "product-image",
            },
            "alt": {
                "display_name": "Alt Text",
                "short_desc": "Image alt text",
                "type": StringOrNothingType,
                "desc": "The alternative text for the image",
                "example_value": "Premium T-Shirt front view",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }

    #! Creates the object with a specific base type (for optional fields)
    constructor(Type base_type) : HashDataType(base_type, "WooCommerceImage") {
        addQoreFields(Fields);
    }
}

} # namespace WooCommerceDataProvider
