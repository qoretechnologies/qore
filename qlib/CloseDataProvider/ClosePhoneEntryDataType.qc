# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClosePhoneEntryDataType.qc Close phone entry data type

/*  ClosePhoneEntryDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CloseDataProvider module
public namespace CloseDataProvider {

#! Close phone entry data type for contact phone numbers
public class ClosePhoneEntryDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "The phone type",
                "desc": "The type of phone number (e.g., `mobile`, `office`, `home`, `fax`)",
                "example_value": "mobile",
            },
            "phone": {
                "type": StringType,
                "display_name": "Phone",
                "short_desc": "The phone number",
                "desc": "The phone number value",
                "example_value": "+1-555-123-4567",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace CloseDataProvider
