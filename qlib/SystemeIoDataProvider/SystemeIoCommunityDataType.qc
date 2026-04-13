# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SystemeIoDataProvider module definition

/*  SystemeIoCommunityDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SystemeIoDataProvider module
public namespace SystemeIoDataProvider {

#! Systeme.io community record type
public class SystemeIoCommunityDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The community ID",
                "desc": "The unique identifier of the community",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Community name",
                "desc": "The name of the community",
            },
            "createdAt": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "The date and time when the community was created",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SystemeIoDataProvider
