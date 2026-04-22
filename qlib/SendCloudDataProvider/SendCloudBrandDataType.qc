# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudBrandDataType.qc SendCloud brand data type

/*  SendCloudBrandDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SendCloudDataProvider module
public namespace SendCloudDataProvider {

#! SendCloud brand record
public class SendCloudBrandDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftIntOrNothingType,
                "display_name": "ID",
                "short_desc": "Brand ID",
                "desc": "Unique numeric identifier of the brand",
            },
            "name": {
                "type": SoftStringOrNothingType,
                "display_name": "Name",
                "short_desc": "Brand name",
                "desc": "Display name of the brand",
            },
            "url": {
                "type": SoftStringOrNothingType,
                "display_name": "URL",
                "short_desc": "Brand homepage URL",
                "desc": "URL to the brand's homepage (used in tracking emails)",
            },
            "sender_addresses": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Sender Addresses",
                "short_desc": "Associated sender addresses",
                "desc": "List of sender-address IDs (or address hashes) associated with this brand",
            },
            "logo": {
                "type": SoftStringOrNothingType,
                "display_name": "Logo",
                "short_desc": "Brand logo URL",
                "desc": "URL of the brand logo image",
            },
        };
    }

    constructor() : HashDataType("SendCloudBrandDataType") {
        addQoreFields(Fields);
    }
}

} # namespace SendCloudDataProvider
