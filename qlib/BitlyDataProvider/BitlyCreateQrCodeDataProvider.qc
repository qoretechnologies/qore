# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BitlyCreateQrCodeDataProvider.qc Bitly create QR code action data provider

/*  BitlyCreateQrCodeDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a QR code
public class BitlyCreateQrCodeRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating a QR code
        const Fields = {
            "title": {
                "type": StringType,
                "display_name": "Title",
                "short_desc": "QR code title",
                "desc": "The title of the QR code",
                "required": True,
                "example_value": "My QR Code",
            },
            "group_guid": {
                "type": StringType,
                "display_name": "Group GUID",
                "short_desc": "Group to create the QR code in",
                "desc": "The GUID of the group to create the QR code in",
                "required": True,
                "ref_data": "groups",
            },
            "destination": {
                "type": new BitlyQrCodeDestinationDataType(),
                "display_name": "Destination",
                "short_desc": "Target URL or bitlink",
                "desc": "The destination for the QR code. Provide either `long_url` "
                    "or `bitlink_id`",
                "required": True,
            },
            "render_customizations": {
                "type": new BitlyRenderCustomizationsDataType(AutoHashOrNothingType),
                "display_name": "Render Customizations",
                "short_desc": "QR code appearance settings",
                "desc": "Appearance customization settings for the QR code including "
                    "colors and dot pattern",
            },
            "tags": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Tags",
                "short_desc": "Tags for the QR code",
                "desc": "A list of tags to associate with the QR code for organization",
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

#! Bitly create QR code action data provider
public class BitlyCreateQrCodeDataProvider inherits BitlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new QR code",
            "type": "BitlyCreateQrCodeDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(RestClientIo::RestClientIo rest) : BitlyDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Create a new QR code";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BitlyCreateQrCodeRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BitlyQrCodeDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("qr-codes", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BitlyDataProvider
