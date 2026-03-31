# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BitlyDataProvider module definition

/*  BitlyQrCodeDestinationDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BitlyDataProvider module
public namespace BitlyDataProvider {

#! QR code destination type — wraps the target URL or bitlink
public class BitlyQrCodeDestinationDataType inherits HashDataType {
    public {
        #! Field definitions for QR code destination
        const Fields = {
            "long_url": {
                "type": StringOrNothingType,
                "display_name": "Long URL",
                "short_desc": "Destination URL",
                "desc": "The destination URL for the QR code. Either `long_url` or "
                    "`bitlink_id` must be provided",
                "example_value": "https://example.com/my-page",
            },
            "bitlink_id": {
                "type": StringOrNothingType,
                "display_name": "Bitlink ID",
                "short_desc": "Existing bitlink to link",
                "desc": "The ID of an existing bitlink to create a QR code for. "
                    "Either `long_url` or `bitlink_id` must be provided",
                "ref_data": "bitlinks",
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

} # namespace BitlyDataProvider
