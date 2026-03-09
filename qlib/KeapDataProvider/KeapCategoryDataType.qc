# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KeapDataProvider module definition

/*  KeapCategoryDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Keap tag category data type
public class KeapCategoryDataType inherits HashDataType {
    public {
        #! Field definitions for tag categories
        const Fields = {
            "id": {
                "type": IntOrNothingType,
                "display_name": "Category ID",
                "short_desc": "The category ID",
                "desc": "The unique identifier of the tag category",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Category Name",
                "short_desc": "The category name",
                "desc": "The name of the tag category",
                "example_value": "Lead Status",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The category description",
                "desc": "A description of the tag category",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("KeapCategoryDataType") {
        addQoreFields(Fields);
    }

    #! Creates the type with a base type (for optional fields)
    constructor(Type base_type) : HashDataType(base_type, "KeapCategoryDataType") {
        addQoreFields(Fields);
    }
}

} # namespace KeapDataProvider
