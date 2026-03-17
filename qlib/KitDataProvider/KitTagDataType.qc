# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KitDataProvider module definition

/*  KitTagDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Kit tag record type
public class KitTagDataType inherits HashDataType {
    public {
        #! Field definitions for Kit tags
        const Fields = {
            "id": {
                "type": SoftIntType,
                "display_name": "ID",
                "short_desc": "The tag ID",
                "desc": "The unique identifier of the tag",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The tag name",
                "desc": "The name of the tag",
            },
            "created_at": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the tag was created",
                "desc": "The timestamp when the tag was created",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace KitDataProvider
