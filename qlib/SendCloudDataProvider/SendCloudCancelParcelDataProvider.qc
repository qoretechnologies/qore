# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudCancelParcelDataProvider.qc SendCloud cancel-parcel action data provider

/*  SendCloudCancelParcelDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Cancel-parcel request type
public class SendCloudCancelParcelRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftIntType,
                "display_name": "Parcel ID",
                "short_desc": "Parcel to cancel",
                "desc": "Numeric ID of the parcel to cancel",
                "required": True,
                "ref_data": "parcels",
            },
        };
    }

    constructor() : HashDataType("SendCloudCancelParcelRequestDataType") {
        addQoreFields(Fields);
    }
}

#! Cancel-parcel action provider (POST /parcels/{id}/cancel)
public class SendCloudCancelParcelDataProvider inherits SendCloudDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "cancel-parcel",
            "desc": "Cancel a SendCloud parcel that has been announced to the carrier",
            "type": "SendCloudCancelParcelDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType RequestType = new SendCloudCancelParcelRequestDataType();
        static *AbstractDataProviderType ResponseType = new SendCloudCancelResponseDataType();
    }

    constructor(*RestClientIo::RestClientIo rest) : SendCloudDataProviderBase(rest) {
    }

    string getName() {
        return "cancel-parcel";
    }

    *string getDesc() {
        return "Cancel a SendCloud parcel that has been announced";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int id = req.id;
        return doPost(sprintf("parcels/%d/cancel", id), {});
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SendCloudDataProvider
