# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceTagDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce product tag data type
public class WooCommerceTagDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Unique identifier for the tag",
                "type": IntOrNothingType,
                "desc": "The unique identifier for the product tag",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Tag name",
                "type": StringOrNothingType,
                "desc": "The tag name",
            },
            "slug": {
                "display_name": "Slug",
                "short_desc": "Tag slug",
                "type": StringOrNothingType,
                "desc": "The tag URL slug",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Tag description",
                "type": StringOrNothingType,
                "desc": "The tag description",
            },
            "count": {
                "display_name": "Count",
                "short_desc": "Number of products with this tag",
                "type": IntOrNothingType,
                "desc": "The number of products assigned to this tag",
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
