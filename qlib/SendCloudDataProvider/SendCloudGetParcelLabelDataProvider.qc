# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudGetParcelLabelDataProvider.qc SendCloud get-parcel-label action data provider

/*  SendCloudGetParcelLabelDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Get-parcel-label request type
public class SendCloudGetParcelLabelRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "parcel_id": {
                "type": SoftIntType,
                "display_name": "Parcel ID",
                "short_desc": "Parcel whose label URLs to retrieve",
                "desc": "Numeric ID of the parcel whose label URLs are being retrieved",
                "required": True,
                "ref_data": "parcels",
            },
        };
    }

    constructor() : HashDataType("SendCloudGetParcelLabelRequestDataType") {
        addQoreFields(Fields);
    }
}

#! Get-parcel-label action provider (GET /labels/{parcel_id})
public class SendCloudGetParcelLabelDataProvider inherits SendCloudDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-parcel-label",
            "desc": "Get the label URLs for a SendCloud parcel",
            "type": "SendCloudGetParcelLabelDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType RequestType = new SendCloudGetParcelLabelRequestDataType();
        static *AbstractDataProviderType ResponseType = new SendCloudGetParcelLabelResponseDataType();
    }

    constructor(*RestClientIo::RestClientIo rest) : SendCloudDataProviderBase(rest) {
    }

    string getName() {
        return "get-parcel-label";
    }

    *string getDesc() {
        return "Get the label URLs (A4 and A6 PDFs) for a SendCloud parcel";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int parcel_id = req.parcel_id;
        return doGet(sprintf("labels/%d", parcel_id));
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SendCloudDataProvider
