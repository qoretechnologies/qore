# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BitlyUpdateQrCodeDataProvider.qc Bitly update QR code action data provider

/*  BitlyUpdateQrCodeDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a QR code
public class BitlyUpdateQrCodeRequestDataType inherits HashDataType {
    public {
        #! Field definitions for updating a QR code
        const Fields = {
            "qrcode_id": {
                "type": StringType,
                "display_name": "QR Code ID",
                "short_desc": "The QR code to update",
                "desc": "The ID of the QR code to update",
                "required": True,
                "ref_data": "qrcodes",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "New title",
                "desc": "The new title for the QR code",
                "preselected": True,
            },
            "archived": {
                "type": SoftBoolOrNothingType,
                "display_name": "Archived",
                "short_desc": "Archive status",
                "desc": "Set to `True` to archive the QR code or `False` to unarchive it",
            },
            "destination": {
                "type": new BitlyQrCodeDestinationDataType(AutoHashOrNothingType),
                "display_name": "Destination",
                "short_desc": "New target URL or bitlink",
                "desc": "The new destination for the QR code. Provide either `long_url` "
                    "or `bitlink_id`",
            },
            "render_customizations": {
                "type": new BitlyRenderCustomizationsDataType(AutoHashOrNothingType),
                "display_name": "Render Customizations",
                "short_desc": "QR code appearance settings",
                "desc": "Updated appearance customization settings for the QR code",
            },
            "tags": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Tags",
                "short_desc": "New tags",
                "desc": "A new list of tags for the QR code (replaces existing tags)",
            },
            "expiration_at": {
                "type": StringOrNothingType,
                "display_name": "Expiration",
                "short_desc": "When the QR code expires",
                "desc": "ISO 8601 timestamp when the QR code should expire",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Bitly update QR code action data provider
public class BitlyUpdateQrCodeDataProvider inherits BitlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing QR code",
            "type": "BitlyUpdateQrCodeDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(RestClientIo::RestClientIo rest) : BitlyDataProviderBase(rest) {
    }

    string getName() {
        return "update";
    }

    *string getDesc() {
        return "Update an existing QR code";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BitlyUpdateQrCodeRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BitlyQrCodeDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string qrcode_id = remove req.qrcode_id;
        return doPatch("qr-codes/" + qrcode_id, req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BitlyDataProvider
