# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceOrderNoteDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce order note data type
public class WooCommerceOrderNoteDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Unique identifier for the order note",
                "type": IntOrNothingType,
                "desc": "The unique identifier for the order note",
            },
            "author": {
                "display_name": "Author",
                "short_desc": "Note author",
                "type": StringOrNothingType,
                "desc": "The author of the order note",
            },
            "date_created": {
                "display_name": "Date Created",
                "short_desc": "Date the note was created",
                "type": DateOrNothingType,
                "desc": "The date and time the order note was created",
            },
            "note": {
                "display_name": "Note",
                "short_desc": "Note content",
                "type": StringOrNothingType,
                "desc": "The content of the order note",
            },
            "customer_note": {
                "display_name": "Customer Note",
                "short_desc": "Whether the note is visible to the customer",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the note is visible to the customer",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace WooCommerceDataProvider
