# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BitlyDataProvider module definition

/*  BitlyQrCodeDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Bitly QR code record type
public class BitlyQrCodeDataType inherits HashDataType {
    public {
        #! Field definitions for QR codes
        const Fields = {
            "qrcode_id": {
                "type": StringType,
                "display_name": "QR Code ID",
                "short_desc": "The QR code identifier",
                "desc": "The unique identifier of the QR code",
            },
            "group_guid": {
                "type": StringType,
                "display_name": "Group GUID",
                "short_desc": "Associated group",
                "desc": "The GUID of the group this QR code belongs to",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "The QR code title",
                "desc": "The title of the QR code",
            },
            "archived": {
                "type": BoolType,
                "display_name": "Archived",
                "short_desc": "Whether the QR code is archived",
                "desc": "Whether the QR code has been archived",
            },
            "destination": {
                "type": new BitlyQrCodeDestinationDataType(AutoHashOrNothingType),
                "display_name": "Destination",
                "short_desc": "Target URL or bitlink",
                "desc": "The destination URL or bitlink the QR code points to",
            },
            "render_customizations": {
                "type": new BitlyRenderCustomizationsDataType(AutoHashOrNothingType),
                "display_name": "Render Customizations",
                "short_desc": "QR code appearance settings",
                "desc": "The appearance customization settings of the QR code",
            },
            "tags": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Tags",
                "short_desc": "Associated tags",
                "desc": "A list of tags associated with the QR code",
            },
            "expiration_at": {
                "type": StringOrNothingType,
                "display_name": "Expiration",
                "short_desc": "When the QR code expires",
                "desc": "The ISO 8601 timestamp when the QR code expires",
            },
            "created": {
                "type": StringOrNothingType,
                "display_name": "Created",
                "short_desc": "When the QR code was created",
                "desc": "The ISO 8601 timestamp when the QR code was created",
            },
            "modified": {
                "type": StringOrNothingType,
                "display_name": "Modified",
                "short_desc": "When the QR code was last modified",
                "desc": "The ISO 8601 timestamp when the QR code was last modified",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace BitlyDataProvider
